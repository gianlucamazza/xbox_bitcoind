#!/usr/bin/env bash
# health-check.sh — single ops-hygiene signal for xbox_bitcoind (host → console).
#
# Exit codes:
#   0  healthy (node running, portal OK, no hard failures)
#   1  degraded (running but stuck tip, stale samples, timer off, …)
#   2  critical (portal down, package missing, process stopped, …)
#
# Usage:
#   ./scripts/health-check.sh
#   ./scripts/health-check.sh --json
#   ./scripts/health-check.sh --strict   # treat degraded as exit 2
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/env.sh"

STATE_DIR="${XBB_STATE_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/xbox_bitcoind}"
LOG_FILE="${XBB_IBD_LOG:-${STATE_DIR}/ibd.jsonl}"
JSON=0
STRICT=0

for arg in "$@"; do
	case "$arg" in
	--json) JSON=1 ;;
	--strict) STRICT=1 ;;
	-h | --help)
		sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
		exit 0
		;;
	*)
		echo "Unknown arg: $arg" >&2
		exit 2
		;;
	esac
done

# Collect checks as JSON lines then summarize in Python.
TMP="$(mktemp)"
trap 'rm -f "${TMP}"' EXIT

add() {
	# add level name detail  (level: ok|warn|crit)
	python3 -c 'import json,sys; print(json.dumps({"level":sys.argv[1],"name":sys.argv[2],"detail":sys.argv[3]}))' \
		"$1" "$2" "$3" >>"${TMP}"
}

# --- portal ---
if ! curl --basic -u "${XBOX_USER}:${XBOX_PASS}" -k -sS --connect-timeout 5 --max-time 15 \
	"https://${XBOX_IP}:${XBOX_PORT}/api/os/info" >/dev/null 2>&1; then
	add crit portal "unreachable ${XBOX_IP}:${XBOX_PORT}"
else
	add ok portal "ok ${XBOX_IP}:${XBOX_PORT}"
fi

# --- live node-status ---
set +e
STATUS_JSON="$("${SCRIPT_DIR}/node-status.sh" --json 2>/dev/null)"
NS_RC=$?
set -e

if [[ ${NS_RC} -ne 0 || -z "${STATUS_JSON}" ]]; then
	add crit node_status "node-status failed rc=${NS_RC}"
	STATUS_JSON='{}'
else
	add ok node_status "ok"
fi

# --- timer ---
TIMER_STATE="unknown"
if command -v systemctl >/dev/null 2>&1; then
	if systemctl --user is-enabled xbox-bitcoind-ibd-sample.timer >/dev/null 2>&1; then
		if systemctl --user is-active xbox-bitcoind-ibd-sample.timer >/dev/null 2>&1; then
			TIMER_STATE="enabled+active"
			add ok ibd_timer "${TIMER_STATE}"
		else
			TIMER_STATE="enabled+inactive"
			add warn ibd_timer "${TIMER_STATE}"
		fi
	else
		TIMER_STATE="disabled"
		add warn ibd_timer "not enabled (./scripts/install-ibd-timer.sh)"
	fi
else
	add warn ibd_timer "systemctl unavailable"
fi

# --- samples age / stuck ---
python3 - "${TMP}" "${STATUS_JSON}" "${LOG_FILE}" "${TIMER_STATE}" "${JSON}" "${STRICT}" <<'PY'
import json, sys, os
from datetime import datetime, timezone, timedelta
from pathlib import Path

checks_path, status_raw, log_path, timer_state, as_json, strict = sys.argv[1:7]
as_json = as_json == "1"
strict = strict == "1"

checks = []
for line in Path(checks_path).read_text(encoding="utf-8").splitlines():
    line = line.strip()
    if line:
        checks.append(json.loads(line))

try:
    st = json.loads(status_raw) if status_raw.strip() else {}
except Exception:
    st = {}

def add(level, name, detail):
    checks.append({"level": level, "name": name, "detail": detail})

if st.get("ok") is False and st.get("error"):
    add("crit", "node", st.get("error"))
else:
    running = bool(st.get("running"))
    pfn = st.get("pfn") or ""
    height = st.get("tip_height")
    progress = st.get("tip_progress")
    errs = st.get("recent_errors") or []

    if not pfn:
        add("crit", "package", "not installed")
    else:
        add("ok", "package", pfn.split("_")[1] if "_" in pfn else pfn)

    if running:
        add("ok", "process", f"pid={st.get('pid')}")
    else:
        add("crit", "process", "not running (Home suspend? start-app / re-open title)")

    if isinstance(height, int) and height >= 0:
        add("ok", "tip_height", str(height))
    elif running:
        add("warn", "tip_height", "missing (headers-only or early load)")

    if isinstance(progress, (int, float)):
        add("ok", "tip_progress", f"{progress:.6f}")

    if errs:
        add("warn", "log_errors", f"{len(errs)} recent")
    else:
        add("ok", "log_errors", "none")

# Sample freshness + stuck within last hours
log = Path(log_path)
if log.is_file():
    rows = []
    for ln in log.read_text(encoding="utf-8", errors="replace").splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            rows.append(json.loads(ln))
        except Exception:
            pass
    if rows:
        last = rows[-1]
        try:
            ts = datetime.fromisoformat(str(last["ts"]).replace("Z", "+00:00"))
            age = datetime.now(timezone.utc) - ts
            age_h = age.total_seconds() / 3600.0
            if age_h <= 2.0:
                add("ok", "sample_age", f"{age_h:.1f}h")
            elif age_h <= 6.0:
                add("warn", "sample_age", f"{age_h:.1f}h (timer lag?)")
            else:
                add("warn", "sample_age", f"{age_h:.1f}h stale")
        except Exception:
            add("warn", "sample_age", "unparseable last ts")

        # Stuck: last 6 samples same height while claiming running
        tail = rows[-6:]
        if len(tail) >= 6:
            heights = [r.get("tip_height") for r in tail]
            running_all = all(r.get("running") for r in tail)
            if running_all and heights[0] is not None and len(set(heights)) == 1:
                add("warn", "stuck_tip", f"height={heights[0]} for {len(tail)} samples")
            else:
                add("ok", "stuck_tip", "no")
    else:
        add("warn", "samples", "log empty")
else:
    add("warn", "samples", f"no {log_path}")

# Score
levels = [c["level"] for c in checks]
if "crit" in levels:
    code = 2
    overall = "critical"
elif "warn" in levels:
    code = 2 if strict else 1
    overall = "degraded"
else:
    code = 0
    overall = "healthy"

if as_json:
    print(json.dumps({"ok": code == 0, "overall": overall, "exit": code, "checks": checks}, indent=2))
else:
    print(f"=== xbox_bitcoind health: {overall} ===")
    for c in checks:
        mark = {"ok": "OK  ", "warn": "WARN", "crit": "CRIT"}[c["level"]]
        print(f"  [{mark}] {c['name']}: {c['detail']}")
    print(f"exit={code}  (0=healthy 1=degraded 2=critical)")
    if overall != "healthy":
        print("ops: leave app focused for IBD; re-open title after Home; soft-stop only")
        print("     ./scripts/node-status.sh · ./scripts/ibd-report.sh · ./scripts/deploy.sh start-app")

sys.exit(code)
PY
