#!/usr/bin/env bash
# node-status.sh — one-shot IBD / process snapshot from Device Portal.
#
# Usage:
#   ./scripts/node-status.sh              # print human summary
#   ./scripts/node-status.sh --json       # machine-readable
#   ./scripts/node-status.sh --loop 3600  # sample every N seconds (Ctrl-C to stop)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/env.sh"

BASE_URL="https://${XBOX_IP}:${XBOX_PORT}"
CURL_CFG="$(xbox_curl_config)"
trap 'rm -f "${CURL_CFG}"' EXIT
CURL_AUTH=(--basic -K "${CURL_CFG}" -k -sS --connect-timeout 8 --max-time 120)
APP_ID="${XBOX_BITCOIND_APP_ID}"

JSON=0
LOOP=0
INTERVAL=3600

while [[ $# -gt 0 ]]; do
	case "$1" in
	--json)
		JSON=1
		shift
		;;
	--loop)
		LOOP=1
		INTERVAL="${2:-3600}"
		shift 2
		;;
	-h | --help)
		sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
		exit 0
		;;
	*)
		echo "Unknown arg: $1" >&2
		exit 1
		;;
	esac
done

get_pfn() {
	curl "${CURL_AUTH[@]}" "${BASE_URL}/api/app/packagemanager/packages" |
		APP_ID="${APP_ID}" python3 -c '
import json, os, re, sys
app_id = os.environ["APP_ID"]
data = json.load(sys.stdin)
matches = [p for p in data.get("InstalledPackages", [])
           if app_id in p.get("PackageRelativeId", "")
           or app_id in p.get("PackageFullName", "")]
if not matches:
    sys.exit(0)
def version_key(package):
    name = package.get("PackageFullName", "")
    m = re.search(r"_(\d+(?:\.\d+)*)_", name)
    return tuple(int(x) for x in m.group(1).split(".")) if m else ()
matches.sort(key=version_key, reverse=True)
print(matches[0].get("PackageFullName", ""))
'
}

sample_once() {
	local pfn tmp_log
	pfn="$(get_pfn)"
	if [[ -z "${pfn}" ]]; then
		if [[ "${JSON}" -eq 1 ]]; then
			echo '{"ok":false,"error":"package_not_installed"}'
		else
			echo "package: not installed (${APP_ID})"
		fi
		return 1
	fi

	tmp_log="$(mktemp)"
	# Process metrics
	local proc_json
	proc_json="$(curl "${CURL_AUTH[@]}" "${BASE_URL}/api/resourcemanager/processes" 2>/dev/null || echo '{}')"

	# Best-effort: fetch debug.log (can be large — last portion via full file; portal has no range)
	# Prefer small: we only parse tail after download; timeout max-time on curl above.
	if ! curl "${CURL_AUTH[@]}" --fail -o "${tmp_log}" \
		"${BASE_URL}/api/filesystem/apps/file?knownfolderid=LocalAppData&packagefullname=${pfn}&path=%5CLocalState%5Cbitcoin&filename=debug.log" 2>/dev/null; then
		: >"${tmp_log}"
	fi

	# Datadir listings (portal is non-recursive — sum top-level + blocks + chainstate)
	local files_json blocks_json chain_json
	files_json="$(curl "${CURL_AUTH[@]}" \
		"${BASE_URL}/api/filesystem/apps/files?knownfolderid=LocalAppData&packagefullname=${pfn}&path=%5CLocalState%5Cbitcoin" 2>/dev/null || echo '{}')"
	blocks_json="$(curl "${CURL_AUTH[@]}" \
		"${BASE_URL}/api/filesystem/apps/files?knownfolderid=LocalAppData&packagefullname=${pfn}&path=%5CLocalState%5Cbitcoin%5Cblocks" 2>/dev/null || echo '{}')"
	chain_json="$(curl "${CURL_AUTH[@]}" \
		"${BASE_URL}/api/filesystem/apps/files?knownfolderid=LocalAppData&packagefullname=${pfn}&path=%5CLocalState%5Cbitcoin%5Cchainstate" 2>/dev/null || echo '{}')"

	python3 - "$pfn" "${tmp_log}" "${proc_json}" "${files_json}" "${blocks_json}" "${chain_json}" "${JSON}" <<'PY'
import json, os, re, sys
from datetime import datetime, timezone

pfn, log_path, proc_raw, files_raw, blocks_raw, chain_raw, as_json = sys.argv[1:8]
as_json = as_json == "1"

def parse_proc(raw):
    try:
        data = json.loads(raw)
    except Exception:
        return None
    for p in data.get("Processes") or []:
        img = (p.get("ImageName") or "").lower()
        pkg = (p.get("PackageFullName") or "").lower()
        if "xbox_bitcoind" in img or "xboxbitcoind" in pkg:
            return p
    return None

def parse_log(path):
    tip_h = tip_prog = tip_cache = tip_time = None
    nbest = loaded = tree = None
    errors = []
    try:
        # Read only last ~2 MiB to keep memory bounded on huge logs
        with open(path, "rb") as f:
            f.seek(0, 2)
            size = f.tell()
            f.seek(max(0, size - 2 * 1024 * 1024))
            text = f.read().decode("utf-8", errors="replace")
    except Exception:
        text = ""
    for line in text.splitlines():
        if "UpdateTip:" in line and "height=" in line:
            m = re.search(
                r"(\d{4}-\d{2}-\d{2}T[\d:]+Z).*height=(\d+).*progress=([0-9.]+).*cache=([0-9.]+MiB)",
                line,
            )
            if m:
                tip_time, tip_h, tip_prog, tip_cache = m.group(1), int(m.group(2)), float(m.group(3)), m.group(4)
        if "nBestHeight =" in line:
            m = re.search(r"nBestHeight = (\d+)", line)
            if m:
                nbest = int(m.group(1))
        if "Loaded best chain:" in line and "height=" in line:
            m = re.search(r"height=(\d+)", line)
            if m:
                loaded = int(m.group(1))
        if "block tree size =" in line:
            m = re.search(r"block tree size = (\d+)", line)
            if m:
                tree = int(m.group(1))
        if re.search(r"\bERROR:", line) or "Corruption" in line or "Out of memory" in line:
            errors.append(line[-200:])
    return {
        "tip_height": tip_h,
        "tip_progress": tip_prog,
        "tip_cache": tip_cache,
        "tip_time": tip_time,
        "nBestHeight": nbest,
        "loaded_height": loaded,
        "block_tree_size": tree,
        "recent_errors": errors[-5:],
        "log_bytes": os.path.getsize(path) if os.path.isfile(path) else 0,
    }

def parse_files(raw):
    try:
        data = json.loads(raw)
    except Exception:
        return {"items": 0, "listed_bytes": 0}
    items = data.get("Items") or []
    total = 0
    for it in items:
        # Type 16 = directory (no size); Type 32 = file
        total += int(it.get("FileSize") or 0)
    return {"items": len(items), "listed_bytes": total}

proc = parse_proc(proc_raw)
logi = parse_log(log_path)
root = parse_files(files_raw)
blocks = parse_files(blocks_raw)
chain = parse_files(chain_raw)
approx = root["listed_bytes"] + blocks["listed_bytes"] + chain["listed_bytes"]
now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

ws = (proc or {}).get("WorkingSetSize")
pws = (proc or {}).get("PrivateWorkingSet")
running = bool(proc and proc.get("IsRunning"))

out = {
    "ok": True,
    "ts": now,
    "pfn": pfn,
    "running": running,
    "pid": (proc or {}).get("ProcessId"),
    "working_set": ws,
    "private_working_set": pws,
    "cpu": (proc or {}).get("CPUUsage"),
    **logi,
    "bitcoin_dir_approx_bytes": approx,
    "blocks_bytes": blocks["listed_bytes"],
    "chainstate_bytes": chain["listed_bytes"],
    "bitcoin_root_items": root["items"],
}

if as_json:
    print(json.dumps(out, separators=(",", ":")))
else:
    def hb(n):
        if n is None:
            return "n/a"
        n = float(n)
        u = ["B", "KiB", "MiB", "GiB"]
        i = 0
        while n >= 1024 and i < len(u) - 1:
            n /= 1024
            i += 1
        return f"{n:.1f} {u[i]}" if i else f"{int(n)} {u[i]}"

    print(f"=== xbox_bitcoind node status @ {now} ===")
    print(f"package:  {pfn}")
    print(f"process:  {'running' if running else 'stopped'}" + (f" pid={out['pid']}" if out.get("pid") else ""))
    print(f"memory:   WS={hb(ws)}  private={hb(pws)}  cpu={out.get('cpu')}")
    print(f"tip:      height={out.get('tip_height')}  progress={out.get('tip_progress')}  cache={out.get('tip_cache')}  @ {out.get('tip_time')}")
    print(f"startup:  nBestHeight={out.get('nBestHeight')}  loaded={out.get('loaded_height')}  block_tree={out.get('block_tree_size')}")
    print(
        f"datadir:  ≈{hb(out.get('bitcoin_dir_approx_bytes'))} "
        f"(blocks≈{hb(out.get('blocks_bytes'))} chainstate≈{hb(out.get('chainstate_bytes'))}; portal non-recursive)"
    )
    print(f"debug.log ≈{hb(out.get('log_bytes'))}")
    if out.get("recent_errors"):
        print("errors:")
        for e in out["recent_errors"]:
            print(f"  ! {e}")
    else:
        print("errors:   none in log tail")
    print()
    print("ops: leave app open during IBD; stop only via ./scripts/deploy.sh stop-app (soft stop)")
PY
	rm -f "${tmp_log}"
}

if [[ "${LOOP}" -eq 1 ]]; then
	while true; do
		sample_once || true
		sleep "${INTERVAL}"
	done
else
	sample_once
fi
