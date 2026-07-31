#!/usr/bin/env bash
# v1-close-check.sh — evaluate whether v1 "IBD complete + stability" can be closed.
#
# Exit codes:
#   0  all gates pass (safe to tick plan-core-uwp checklist)
#   1  incomplete (prints which gates failed)
#   2  cannot assess (portal / no samples)
#
# Usage:
#   ./scripts/v1-close-check.sh
#   ./scripts/v1-close-check.sh --json
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="${XBB_STATE_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/xbox_bitcoind}"
LOG_FILE="${XBB_IBD_LOG:-${STATE_DIR}/ibd.jsonl}"
JSON=0
[[ "${1:-}" == "--json" ]] && JSON=1

if ! line="$("${SCRIPT_DIR}/node-status.sh" --json 2>/dev/null)"; then
	echo "FAIL: cannot reach console / node-status" >&2
	exit 2
fi

python3 - "$line" "$LOG_FILE" "$JSON" <<'PY'
import json, sys, os
from datetime import datetime, timezone, timedelta

sample = json.loads(sys.argv[1])
log_path = sys.argv[2]
as_json = sys.argv[3] == "1"

gates = []

def gate(name, ok, detail):
    gates.append({"name": name, "ok": bool(ok), "detail": detail})

# --- live sample ---
running = bool(sample.get("running"))
height = sample.get("tip_height")
progress = sample.get("tip_progress")
ws = sample.get("working_set")
errs = sample.get("recent_errors") or []

gate("process_running", running, f"running={running}")
gate("has_tip_height", isinstance(height, int) and height > 0, f"tip_height={height}")
gate("no_recent_log_errors", len(errs) == 0, f"errors={len(errs)}")

# Heuristic: Core reports progress≈1.0 when near tip; during IBD often << 1
# Accept progress >= 0.999 as "synced enough" OR progress missing but height very high.
near_tip = isinstance(progress, (int, float)) and progress >= 0.999
gate("sync_near_tip", near_tip, f"tip_progress={progress} (need >= 0.999)")

# --- history: >= 24h of samples after near-tip preferred; if not near tip, fail early ---
rows = []
if os.path.isfile(log_path):
    for ln in open(log_path, encoding="utf-8", errors="replace"):
        ln = ln.strip()
        if not ln:
            continue
        try:
            rows.append(json.loads(ln))
        except Exception:
            pass

stable_24h = False
stable_detail = "need near_tip first"
if near_tip and rows:
    # Samples with progress>=0.999 or height within 200 of current tip
    def parse_ts(r):
        try:
            return datetime.fromisoformat(r["ts"].replace("Z", "+00:00"))
        except Exception:
            return None
    now = parse_ts(sample) or datetime.now(timezone.utc)
    window = []
    for r in rows:
        t = parse_ts(r)
        if t is None:
            continue
        if now - t <= timedelta(hours=24):
            window.append(r)
    # Also include live sample conceptually
    if len(window) < 2:
        stable_detail = f"only {len(window)} samples in last 24h (timer should produce ~24)"
    else:
        heights = [r.get("tip_height") for r in window if isinstance(r.get("tip_height"), int)]
        run_ok = all(r.get("running") for r in window)
        if not heights:
            stable_detail = "no heights in window"
        else:
            hmin, hmax = min(heights), max(heights)
            # At tip, height should advance slowly (~6/h); allow drift but not stall at low height
            advanced_or_stable = (hmax - hmin) >= 0  # always
            # Require all running and height near current (within 500 blocks)
            near = all(abs(h - height) <= 500 for h in heights) if height else False
            stable_24h = run_ok and near and len(window) >= 12
            stable_detail = (
                f"samples_24h={len(window)} running_all={run_ok} "
                f"height_range=[{hmin},{hmax}] near_current={near} (need >=12 samples)"
            )
elif near_tip:
    stable_detail = f"no history file {log_path}"

gate("stable_24h_at_tip", stable_24h, stable_detail)

# Soft-stop retest is manual/ops — report as advisory if near tip
gate(
    "soft_stop_retest_advisory",
    True,
    "when near_tip: run ./scripts/soft-stop-test.sh and record in docs/persistence.md",
)

passed = all(g["ok"] for g in gates if g["name"] != "soft_stop_retest_advisory")
# soft_stop is advisory always ok; core gates decide exit

core_ok = all(
    g["ok"] for g in gates if g["name"] in (
        "process_running", "has_tip_height", "no_recent_log_errors",
        "sync_near_tip", "stable_24h_at_tip",
    )
)

out = {
    "ok": core_ok,
    "sample": {
        "tip_height": height,
        "tip_progress": progress,
        "running": running,
        "working_set": ws,
    },
    "gates": gates,
    "next": (
        "Tick plan-core-uwp.md IBD checkbox; tag release if desired."
        if core_ok
        else "Leave node running; keep hourly timer; re-run this script later."
    ),
}

if as_json:
    print(json.dumps(out, indent=2))
else:
    print("=== v1 close check ===")
    print(f"live: height={height} progress={progress} running={running}")
    for g in gates:
        mark = "PASS" if g["ok"] else "FAIL"
        if g["name"] == "soft_stop_retest_advisory":
            mark = "NOTE"
        print(f"  [{mark}] {g['name']}: {g['detail']}")
    print()
    if core_ok:
        print("RESULT: v1 ops closure criteria MET")
        print(out["next"])
        sys.exit(0)
    else:
        print("RESULT: v1 ops closure NOT yet met (expected during IBD)")
        print(out["next"])
        sys.exit(1)
PY
