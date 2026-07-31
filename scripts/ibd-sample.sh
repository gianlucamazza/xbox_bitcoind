#!/usr/bin/env bash
# ibd-sample.sh — append one JSONL node-status sample for long-run IBD history.
#
# Default log: ~/.local/state/xbox_bitcoind/ibd.jsonl
# Override: XBB_IBD_LOG=/path/to/file.jsonl
#
# Usage:
#   ./scripts/ibd-sample.sh
#   # cron / systemd timer every hour:
#   #   .../xbox_bitcoind/scripts/ibd-sample.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/xbox_bitcoind"
LOG_FILE="${XBB_IBD_LOG:-${STATE_DIR}/ibd.jsonl}"

mkdir -p "$(dirname "${LOG_FILE}")"

# shellcheck source=node-status.sh
# node-status sources env itself
line="$("${SCRIPT_DIR}/node-status.sh" --json)"
printf '%s\n' "${line}" >>"${LOG_FILE}"

if [[ "${1:-}" == "-q" || "${1:-}" == "--quiet" ]]; then
	exit 0
fi

echo "appended -> ${LOG_FILE}"
python3 -c "import json,sys; d=json.loads(sys.argv[1]); print('height={h} progress={p} ws={w} running={r}'.format(h=d.get('tip_height'), p=d.get('tip_progress'), w=d.get('working_set'), r=d.get('running')))" "${line}"
