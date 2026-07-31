#!/usr/bin/env bash
# soft-stop-test.sh — verify chain tip survives suspend → stop → restart.
#
# Usage:
#   ./scripts/soft-stop-test.sh
#   ./scripts/soft-stop-test.sh --wait-load 180
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/env.sh"

WAIT_LOAD=180
while [[ $# -gt 0 ]]; do
	case "$1" in
	--wait-load)
		WAIT_LOAD="${2:?}"
		shift 2
		;;
	-h | --help)
		sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
		exit 0
		;;
	*)
		echo "Unknown arg: $1" >&2
		exit 1
		;;
	esac
done

STATE_DIR="${XBB_STATE_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/xbox_bitcoind}"
mkdir -p "${STATE_DIR}"
PRE_LOG="${STATE_DIR}/soft-stop-pre.txt"
POST_LOG="${STATE_DIR}/soft-stop-post.txt"

echo "=== soft-stop-test ==="
echo "1) Pre-stop status"
"${SCRIPT_DIR}/node-status.sh" | tee "${PRE_LOG}"

PRE_TIP="$(
	"${SCRIPT_DIR}/node-status.sh" --json | python3 -c 'import json,sys; d=json.load(sys.stdin); print(d.get("tip_height") or "")'
)"
if [[ -z "${PRE_TIP}" ]]; then
	echo "Error: could not parse pre-stop tip height from debug.log" >&2
	exit 1
fi
echo "pre_tip_height=${PRE_TIP}"

echo
echo "2) Soft stop (suspend + flush)"
"${SCRIPT_DIR}/deploy.sh" stop-app

echo
echo "3) Start app"
"${SCRIPT_DIR}/deploy.sh" start-app

echo
echo "4) Wait up to ${WAIT_LOAD}s for Loaded best chain / nBestHeight"
PFN="$("${SCRIPT_DIR}/deploy.sh" pfn | tail -1)"
LOADED=""
for ((i = 1; i <= WAIT_LOAD; i++)); do
	TMP="$(mktemp "${STATE_DIR}/soft-stop-debug.XXXXXX")"
	if "${SCRIPT_DIR}/deploy.sh" fetch-file "${PFN}" debug.log "${TMP}" bitcoin >/dev/null 2>&1; then
		LOADED="$(
			python3 - "${TMP}" <<'PY'
import re, sys
path = sys.argv[1]
with open(path, "rb") as f:
    f.seek(0, 2)
    size = f.tell()
    f.seek(max(0, size - 512 * 1024))
    text = f.read().decode("utf-8", errors="replace")
# Prefer the *last* Loaded best chain / nBestHeight in the file (post-restart)
loaded = nbest = None
for line in text.splitlines():
    if "Loaded best chain:" in line and "height=" in line:
        m = re.search(r"height=(\d+)", line)
        if m:
            loaded = int(m.group(1))
    if "nBestHeight =" in line:
        m = re.search(r"nBestHeight = (\d+)", line)
        if m:
            nbest = int(m.group(1))
if loaded is not None:
    print(loaded)
elif nbest is not None:
    print(nbest)
PY
		)"
		if [[ -n "${LOADED}" && "${LOADED}" -gt 0 ]]; then
			# Ensure this is a post-restart load: process running + recent Done loading
			if rg -q "init message: Done loading" "${TMP}" 2>/dev/null; then
				# Check last Done loading is after last Shutdown if both present
				if python3 - "${TMP}" "${PRE_TIP}" <<'PY'
import re, sys
path, pre = sys.argv[1], int(sys.argv[2])
with open(path, "rb") as f:
    f.seek(0, 2)
    size = f.tell()
    f.seek(max(0, size - 1024 * 1024))
    text = f.read().decode("utf-8", errors="replace")
lines = text.splitlines()
last_load = last_done = last_shutdown = -1
loaded_h = None
for i, line in enumerate(lines):
    if "Shutdown: done" in line or "Shutdown: In progress" in line:
        last_shutdown = i
    if "Loaded best chain:" in line and "height=" in line:
        last_load = i
        m = re.search(r"height=(\d+)", line)
        if m:
            loaded_h = int(m.group(1))
    if "init message: Done loading" in line:
        last_done = i
# Success if we saw load after a shutdown, or load height near pre tip
if loaded_h is None:
    sys.exit(1)
if last_shutdown >= 0 and last_load > last_shutdown:
    sys.exit(0 if loaded_h >= max(1, pre - 5000) else 1)
# No shutdown marker in tail: accept if loaded close to pre tip
sys.exit(0 if loaded_h >= max(1, pre - 5000) else 1)
PY
				then
					rm -f "${TMP}"
					break
				fi
			fi
		fi
	fi
	rm -f "${TMP}"
	sleep 1
	if ((i % 15 == 0)); then
		echo "  … still waiting (${i}s), last_loaded=${LOADED:-none}"
	fi
done

echo
echo "5) Post-restart status"
"${SCRIPT_DIR}/node-status.sh" | tee "${POST_LOG}"
POST_TIP="$(
	"${SCRIPT_DIR}/node-status.sh" --json | python3 -c 'import json,sys; d=json.load(sys.stdin); print(d.get("loaded_height") or d.get("nBestHeight") or d.get("tip_height") or "")'
)"

echo
echo "=== verdict ==="
echo "pre_tip_height=${PRE_TIP}"
echo "post_loaded_or_tip=${POST_TIP}"
if [[ -z "${POST_TIP}" ]]; then
	echo "FAIL: no post-restart height"
	exit 1
fi
python3 - "${PRE_TIP}" "${POST_TIP}" <<'PY'
import sys
pre, post = int(sys.argv[1]), int(sys.argv[2])
# Allow small rewind only if crash mid-flush; success if within 5k of pre (UWP write interval)
delta = post - pre
print(f"delta_post_minus_pre={delta}")
if post >= pre - 2000:
    print("PASS: chain state conserved across soft stop")
    sys.exit(0)
print("FAIL: post height far below pre-stop tip (lost unflushed progress?)")
sys.exit(1)
PY
