#!/usr/bin/env bash
# ibd-sample.sh — append one JSONL node-status sample for long-run IBD history.
#
# Default log: ~/.local/state/xbox_bitcoind/ibd.jsonl
# Errors:      ~/.local/state/xbox_bitcoind/ibd-errors.jsonl
# Milestones:  ~/.local/state/xbox_bitcoind/milestones.log
#
# Override: XBB_IBD_LOG, XBB_IBD_ERROR_LOG, XBB_STATE_DIR
#
# Usage:
#   ./scripts/ibd-sample.sh           # human summary
#   ./scripts/ibd-sample.sh -q        # quiet (systemd timer)
#   ./scripts/ibd-report.sh           # summarize history
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="${XBB_STATE_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/xbox_bitcoind}"
LOG_FILE="${XBB_IBD_LOG:-${STATE_DIR}/ibd.jsonl}"
ERR_FILE="${XBB_IBD_ERROR_LOG:-${STATE_DIR}/ibd-errors.jsonl}"
MILE_FILE="${STATE_DIR}/milestones.log"
QUIET=0

for arg in "$@"; do
	case "$arg" in
	-q | --quiet) QUIET=1 ;;
	-h | --help)
		sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
		exit 0
		;;
	esac
done

mkdir -p "${STATE_DIR}"

ts_utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }

append_error() {
	local msg="$1"
	python3 -c "import json,sys; print(json.dumps({'ok':False,'ts':sys.argv[1],'error':sys.argv[2]}))" \
		"$(ts_utc)" "${msg}" >>"${ERR_FILE}"
	if [[ "${QUIET}" -eq 0 ]]; then
		echo "ibd-sample error: ${msg}" >&2
	fi
}

# Soft-fail for unattended timers: never leave unit in failed state for portal blips.
ERR_TMP="$(mktemp "${STATE_DIR}/ibd-sample.err.XXXXXX")"
set +e
line="$("${SCRIPT_DIR}/node-status.sh" --json 2>"${ERR_TMP}")"
rc=$?
set -e

if [[ ${rc} -ne 0 || -z "${line}" ]]; then
	err="$(head -c 400 "${ERR_TMP}" 2>/dev/null || true)"
	append_error "node-status failed rc=${rc} ${err}"
	rm -f "${ERR_TMP}"
	exit 0
fi
rm -f "${ERR_TMP}"

if ! printf '%s' "${line}" | python3 -c "import json,sys; json.load(sys.stdin)" 2>/dev/null; then
	append_error "invalid json from node-status"
	exit 0
fi

printf '%s\n' "${line}" >>"${LOG_FILE}"

# Milestone markers (once each) for soft-stop reminders.
python3 - "${line}" "${MILE_FILE}" <<'PY'
import json, os, sys
from datetime import datetime, timezone

sample = json.loads(sys.argv[1])
mile_path = sys.argv[2]
h = sample.get("tip_height")
if not isinstance(h, int):
    sys.exit(0)
thresholds = [500_000, 700_000, 800_000, 850_000, 900_000]
done = set()
if os.path.isfile(mile_path):
    with open(mile_path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("height>="):
                try:
                    done.add(int(line.split("=", 1)[1].split()[0]))
                except Exception:
                    pass
ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
with open(mile_path, "a", encoding="utf-8") as f:
    for t in thresholds:
        if h >= t and t not in done:
            f.write(f"height>={t} reached_at={ts} tip={h}  # consider: ./scripts/soft-stop-test.sh\n")
            f.flush()
PY

# Stuck detection: same tip_height in last 6 samples while claiming running.
python3 - "${LOG_FILE}" "${ERR_FILE}" <<'PY' 2>/dev/null || true
import json, sys
from pathlib import Path
log, err = Path(sys.argv[1]), Path(sys.argv[2])
lines = log.read_text(encoding="utf-8", errors="replace").strip().splitlines()
if len(lines) < 6:
    sys.exit(0)
rows = []
for line in lines[-6:]:
    try:
        rows.append(json.loads(line))
    except Exception:
        sys.exit(0)
heights = [r.get("tip_height") for r in rows]
running = all(r.get("running") for r in rows)
if running and heights[0] is not None and len(set(heights)) == 1:
    # 6 identical tips ≈ 6h if hourly — flag once per height
    flag = err.parent / f".stuck-{heights[0]}"
    if not flag.exists():
        msg = {
            "ok": False,
            "ts": rows[-1].get("ts"),
            "error": "stuck_tip",
            "tip_height": heights[0],
            "samples": 6,
            "hint": "node running but tip unchanged for 6 samples; check peers/debug.log",
        }
        err.open("a", encoding="utf-8").write(json.dumps(msg) + "\n")
        flag.write_text("1\n", encoding="utf-8")
PY

if [[ "${QUIET}" -eq 1 ]]; then
	exit 0
fi

echo "appended -> ${LOG_FILE}"
python3 -c "import json,sys; d=json.loads(sys.argv[1]); print('height={h} progress={p} ws={w} running={r}'.format(h=d.get('tip_height'), p=d.get('tip_progress'), w=d.get('working_set'), r=d.get('running')))" "${line}"
