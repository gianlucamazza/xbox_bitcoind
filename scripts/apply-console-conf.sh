#!/usr/bin/env bash
# apply-console-conf.sh — soft-stop, push a conf profile to LocalState\bitcoin\bitcoin.conf,
# start the app again.
#
# Profiles (config/):
#   console | ibd  → bitcoin.conf.console  (IBD defaults, blocksonly=1)
#   tip            → bitcoin.conf.tip      (post-tip / pre-LN; no blocksonly)
#
# bitcoin.conf is only auto-created when missing on first start; MSIX reinstall does
# not overwrite an existing LocalState conf. Use this after tuning defaults.
#
# Usage:
#   ./scripts/apply-console-conf.sh
#   ./scripts/apply-console-conf.sh --profile tip
#   ./scripts/apply-console-conf.sh --profile tip --dry-run
#   ./scripts/apply-console-conf.sh --profile tip --force   # allow tip conf while progress < 0.99
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "${ROOT}/scripts/env.sh"

PROFILE="console"
DRY=0
FORCE=0

while [[ $# -gt 0 ]]; do
	case "$1" in
	--profile | -p)
		PROFILE="${2:?}"
		shift 2
		;;
	--dry-run)
		DRY=1
		shift
		;;
	--force)
		FORCE=1
		shift
		;;
	-h | --help)
		sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
		exit 0
		;;
	*)
		echo "Unknown arg: $1" >&2
		exit 1
		;;
	esac
done

case "${PROFILE}" in
console | ibd)
	CONF_SRC="${ROOT}/config/bitcoin.conf.console"
	PROFILE_LABEL="console/ibd"
	;;
tip)
	CONF_SRC="${ROOT}/config/bitcoin.conf.tip"
	PROFILE_LABEL="tip/pre-lightning"
	;;
*)
	echo "Unknown profile: ${PROFILE} (use console|ibd|tip)" >&2
	exit 1
	;;
esac

DEPLOY="${ROOT}/scripts/deploy.sh"

if [[ ! -f "${CONF_SRC}" ]]; then
	echo "missing ${CONF_SRC}" >&2
	exit 1
fi

echo "Profile: ${PROFILE_LABEL}"
echo "Source:  ${CONF_SRC}"
echo "Keys:"
grep -E '^(# )?(prune|dbcache|maxconnections|maxmempool|blocksonly|listen|server|rpcbind|rpcallowip)=' \
	"${CONF_SRC}" || true
if ! grep -qE '^blocksonly=' "${CONF_SRC}"; then
	echo "  (blocksonly not set — mempool/tx relay enabled)"
fi

if [[ "${DRY}" -eq 1 ]]; then
	echo ""
	echo "Dry run — console not modified."
	exit 0
fi

if ! "${ROOT}/scripts/probe-console.sh" >/dev/null 2>&1; then
	echo "console not reachable (probe-console failed). Check xbox-env / network." >&2
	exit 1
fi

# Guard: tip profile during IBD requires --force
if [[ "${PROFILE}" == "tip" && "${FORCE}" -eq 0 ]]; then
	set +e
	prog="$("${ROOT}/scripts/node-status.sh" --json 2>/dev/null | python3 -c \
		'import json,sys
try:
 d=json.load(sys.stdin); p=d.get("tip_progress")
 print(p if isinstance(p,(int,float)) else "")
except Exception:
 print("")' 2>/dev/null)"
	set -e
	if [[ -n "${prog}" ]]; then
		# shellcheck disable=SC2086
		need_force="$(python3 -c "import sys; p=float(sys.argv[1]); print(0 if p>=0.99 else 1)" "${prog}" 2>/dev/null || echo 1)"
		if [[ "${need_force}" == "1" ]]; then
			echo "" >&2
			echo "Refusing tip profile while tip_progress=${prog} (< 0.99)." >&2
			echo "  IBD should finish first. To override: $0 --profile tip --force" >&2
			exit 1
		fi
	fi
fi

PFN="$("${DEPLOY}" pfn)"
if [[ -z "${PFN}" ]]; then
	echo "No package installed; cannot upload conf." >&2
	exit 1
fi
echo ""
echo "PFN: ${PFN}"

echo "Soft-stopping app (flush chainstate) ..."
XBB_SOFT_STOP_MAX_WAIT="${XBB_SOFT_STOP_MAX_WAIT:-180}" "${DEPLOY}" stop-app "${PFN}" || true
sleep 2

printf 'Uploading %s → LocalState\\bitcoin\\bitcoin.conf ...\n' "$(basename "${CONF_SRC}")"
"${DEPLOY}" upload-file "${CONF_SRC}" "${PFN}" "bitcoin" "bitcoin.conf"

echo "Starting app ..."
"${DEPLOY}" start-app "${PFN}"

echo ""
echo "Done. Wait ~30–60s for bitcoind, then:"
echo "  ./scripts/health-check.sh"
echo "  ./scripts/node-status.sh"
echo "  ./scripts/deploy.sh fetch-file \"${PFN}\" bitcoin.conf /tmp/bitcoin.conf bitcoin"
echo ""
case "${PROFILE}" in
console | ibd)
	echo "Profile is IBD-oriented (blocksonly=1). At tip: --profile tip"
	;;
tip)
	echo "Profile is tip/pre-LN (mempool on). See docs/pre-lightning.md"
	;;
esac
