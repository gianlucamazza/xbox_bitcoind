#!/usr/bin/env bash
# apply-console-conf.sh — soft-stop, push config/bitcoin.conf.console to the
# console datadir, start the app again.
#
# bitcoin.conf is only auto-created when missing; re-deploy of the MSIX does
# not overwrite an existing LocalState conf. Use this after tuning defaults.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "${ROOT}/scripts/env.sh"

CONF_SRC="${ROOT}/config/bitcoin.conf.console"
DEPLOY="${ROOT}/scripts/deploy.sh"

if [[ ! -f "${CONF_SRC}" ]]; then
	echo "missing ${CONF_SRC}" >&2
	exit 1
fi

if ! "${ROOT}/scripts/probe-console.sh" >/dev/null 2>&1; then
	echo "console not reachable (probe-console failed). Check xbox-env / network." >&2
	exit 1
fi

PFN="$("${DEPLOY}" pfn)"
echo "PFN: ${PFN}"
echo "Source conf:"
grep -E '^(prune|dbcache|maxconnections|maxmempool|blocksonly|listen)=' "${CONF_SRC}" || true

echo ""
echo "Soft-stopping app (flush chainstate) ..."
"${DEPLOY}" stop-app "${PFN}" || true
sleep 2

printf 'Uploading bitcoin.conf → LocalState\\bitcoin\\ ...\n'
"${DEPLOY}" upload-file "${CONF_SRC}" "${PFN}" "bitcoin" "bitcoin.conf"

echo "Starting app ..."
"${DEPLOY}" start-app "${PFN}"

echo ""
echo "Done. Wait ~30–60s for bitcoind, then:"
echo "  ./scripts/node-status.sh"
echo "  ./scripts/deploy.sh fetch-file \"${PFN}\" bitcoin.conf /tmp/bitcoin.conf bitcoin"
echo ""
echo "Note: blocksonly=1 is on for IBD. Before Lightning / full mempool at tip,"
echo "edit conf (comment blocksonly) and re-run this script."
