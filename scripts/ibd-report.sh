#!/usr/bin/env bash
# ibd-report.sh — summarize IBD JSONL history (and optional errors/milestones).
set -euo pipefail

STATE_DIR="${XBB_STATE_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/xbox_bitcoind}"
LOG_FILE="${XBB_IBD_LOG:-${STATE_DIR}/ibd.jsonl}"
ERR_FILE="${XBB_IBD_ERROR_LOG:-${STATE_DIR}/ibd-errors.jsonl}"
MILE_FILE="${STATE_DIR}/milestones.log"

if [[ ! -f "${LOG_FILE}" ]]; then
	echo "No samples yet: ${LOG_FILE}"
	echo "Run: ./scripts/ibd-sample.sh   or enable the user timer (./scripts/install-ibd-timer.sh)"
	exit 0
fi

python3 - "${LOG_FILE}" "${ERR_FILE}" "${MILE_FILE}" <<'PY'
import json, sys
from pathlib import Path
from datetime import datetime

log_path, err_path, mile_path = map(Path, sys.argv[1:4])
rows = []
for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
    line = line.strip()
    if not line:
        continue
    try:
        rows.append(json.loads(line))
    except Exception:
        continue

print(f"=== IBD report ({log_path}) ===")
print(f"samples: {len(rows)}")
if not rows:
    sys.exit(0)

def h(r):
    return r.get("tip_height")

first, last = rows[0], rows[-1]
print(f"first:   ts={first.get('ts')} height={h(first)} progress={first.get('tip_progress')} running={first.get('running')}")
print(f"last:    ts={last.get('ts')} height={h(last)} progress={last.get('tip_progress')} running={last.get('running')} ws={last.get('working_set')}")

# Rate over full window if possible
try:
    t0 = datetime.fromisoformat(first["ts"].replace("Z", "+00:00"))
    t1 = datetime.fromisoformat(last["ts"].replace("Z", "+00:00"))
    hours = max((t1 - t0).total_seconds() / 3600.0, 1e-6)
    dh = (h(last) or 0) - (h(first) or 0)
    print(f"delta:   +{dh} blocks over {hours:.1f} h  (~{dh/hours:.0f} blocks/h)")
except Exception:
    pass

# Last 24-ish samples (if hourly ≈ 1 day)
window = rows[-24:] if len(rows) >= 2 else rows
hs = [h(r) for r in window if h(r) is not None]
if len(hs) >= 2:
    print(f"recent:  last {len(window)} samples height {hs[0]} → {hs[-1]} (Δ{hs[-1]-hs[0]})")

if mile_path.is_file():
    print("--- milestones ---")
    print(mile_path.read_text(encoding="utf-8").rstrip() or "(none)")

if err_path.is_file():
    errs = [ln for ln in err_path.read_text(encoding="utf-8", errors="replace").splitlines() if ln.strip()]
    print(f"--- errors ({len(errs)}) ---")
    for ln in errs[-5:]:
        try:
            e = json.loads(ln)
            print(f"  {e.get('ts')} {e.get('error')}")
        except Exception:
            print(f"  {ln[:120]}")
else:
    print("--- errors: none ---")
PY
