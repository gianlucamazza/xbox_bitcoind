#!/usr/bin/env bash
# ibd-report.sh — summarize IBD JSONL history (rate, ETA, errors, milestones).
set -euo pipefail

STATE_DIR="${XBB_STATE_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/xbox_bitcoind}"
LOG_FILE="${XBB_IBD_LOG:-${STATE_DIR}/ibd.jsonl}"
ERR_FILE="${XBB_IBD_ERROR_LOG:-${STATE_DIR}/ibd-errors.jsonl}"
MILE_FILE="${STATE_DIR}/milestones.log"
# Rough mainnet tip for ETA when progress not near 1 (updated occasionally; not consensus).
ASSUME_TIP="${XBB_ASSUME_TIP_HEIGHT:-920000}"

if [[ ! -f "${LOG_FILE}" ]]; then
	echo "No samples yet: ${LOG_FILE}"
	echo "Run: ./scripts/ibd-sample.sh   or enable the user timer (./scripts/install-ibd-timer.sh)"
	exit 0
fi

python3 - "${LOG_FILE}" "${ERR_FILE}" "${MILE_FILE}" "${ASSUME_TIP}" <<'PY'
import json, sys, math
from pathlib import Path
from datetime import datetime, timezone

log_path, err_path, mile_path = map(Path, sys.argv[1:4])
assume_tip = int(sys.argv[4])

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

def prog(r):
    p = r.get("tip_progress")
    return float(p) if isinstance(p, (int, float)) else None

def parse_ts(r):
    try:
        return datetime.fromisoformat(str(r["ts"]).replace("Z", "+00:00"))
    except Exception:
        return None

def fmt_eta(hours):
    if hours is None or not math.isfinite(hours) or hours < 0:
        return "n/a"
    if hours < 1.0 / 60:
        return "<1m"
    if hours < 1:
        return f"~{max(1, int(hours * 60))}m"
    if hours < 48:
        return f"~{hours:.1f}h"
    return f"~{hours / 24.0:.1f}d"

# Contiguous segment ending at last sample: reset if height drops hard (datadir wipe / reinstall).
valid = [r for r in rows if h(r) is not None and parse_ts(r)]
segment = []
for r in valid:
    if segment and h(r) is not None and h(segment[-1]) is not None:
        prev, cur = h(segment[-1]), h(r)
        # Wipe / restore: large absolute drop or collapse to near-genesis while previously high.
        if cur + 50_000 < prev or (prev >= 50_000 and cur < 5_000):
            print(
                f"note:    height discontinuity {prev} → {cur} at {r.get('ts')} "
                f"(datadir wipe/reinstall?) — rate/ETA use segment after reset"
            )
            segment = []
    segment.append(r)

if not segment:
    segment = valid[-24:] if valid else rows

first, last = segment[0], segment[-1]
print(
    f"first:   ts={first.get('ts')} height={h(first)} progress={first.get('tip_progress')} "
    f"running={first.get('running')}  (segment n={len(segment)})"
)
print(
    f"last:    ts={last.get('ts')} height={h(last)} progress={last.get('tip_progress')} "
    f"running={last.get('running')} ws={last.get('working_set')}"
)

# Segment rate
try:
    t0 = parse_ts(first)
    t1 = parse_ts(last)
    if t0 and t1 and h(last) is not None and h(first) is not None:
        hours = max((t1 - t0).total_seconds() / 3600.0, 1e-6)
        dh = h(last) - h(first)
        rate = dh / hours
        print(f"delta:   +{dh} blocks over {hours:.1f} h  (~{rate:.0f} blocks/h)")
except Exception:
    pass

# Recent window within segment (last ≤24)
window = segment[-24:] if len(segment) >= 2 else segment
if len(window) >= 2:
    hs = [h(r) for r in window]
    print(f"recent:  last {len(window)} samples height {hs[0]} → {hs[-1]} (Δ{hs[-1] - hs[0]})")
    t0 = parse_ts(window[0])
    t1 = parse_ts(window[-1])
    hours = max((t1 - t0).total_seconds() / 3600.0, 1e-6)
    rate = (hs[-1] - hs[0]) / hours
    print(f"rate:    ~{rate:.0f} blocks/h (recent window {hours:.1f} h)")

    tip_h = hs[-1]
    remain_blocks = max(0, assume_tip - tip_h)
    eta_h = (remain_blocks / rate) if rate > 1 else None
    p = prog(window[-1])
    p0 = prog(window[0])
    eta_p = None
    if p is not None and p0 is not None and p < 0.999 and p > p0:
        dprog = (p - p0) / hours
        if dprog > 1e-9:
            eta_p = (1.0 - p) / dprog
    print(f"eta:     height→~{assume_tip}: {fmt_eta(eta_h)}  |  progress: {fmt_eta(eta_p)}")
    print(f"         (height ETA uses XBB_ASSUME_TIP_HEIGHT={assume_tip}; rough only)")
elif len(window) == 1:
    print("recent:  only one valid sample in segment — need more history for rate/ETA")

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
