#!/usr/bin/env bash
# deploy.sh — Xbox Device Portal helpers for xbox_bitcoind
#
# Sub-commands:
#   deploy.sh <package.msix>                             Upload and install .msix (+ companion .cer)
#   deploy.sh probe                                      OS info + package summary (same as probe-console.sh)
#   deploy.sh os-info                                    Raw /api/os/info
#   deploy.sh packages                                   List installed packages (JSON)
#   deploy.sh disk-usage                                 /api/devices/file/usage
#   deploy.sh install-cert <cert.cer>                    Install trust certificate
#   deploy.sh pfn [hint]                                 Print installed package full name
#   deploy.sh get-log [pfn]                              Print LocalState/bitcoind.log
#   deploy.sh list-localstate [pfn]                      List LocalState files
#   deploy.sh fetch-file <pfn> <name> <local-out> [subdir]
#   deploy.sh upload-file <local> <pfn> [remote-dir] [remote-name]
#   deploy.sh mkdir-localstate <pfn> <relpath>
#   deploy.sh start-app [pfn]
#   deploy.sh stop-app [pfn]                             Soft stop (suspend → flush → optional DELETE)
#   deploy.sh package-list                               List installed xbox_bitcoind package full names
#   deploy.sh package-gc [--keep N] [--yes]              Uninstall older revisions (keep newest N)
#   deploy.sh status                                     Node/IBD snapshot (see node-status.sh)
#   deploy.sh health                                     Ops hygiene one-shot (see health-check.sh)
#   deploy.sh soft-stop-test                             Persistence self-check
#   deploy.sh diagnose-startup [pfn]
#
# Env:
#   XBB_SOFT_STOP_MAX_WAIT     Seconds after suspend before DELETE if still active (default 180)
#   XBB_SOFT_STOP_MIN_GRACE    Min seconds before accepting !IsRunning alone (default 8)
#   XBB_SOFT_STOP_REQUIRE_EXIT If 1, require process fully gone (not just !IsRunning); default 0
#
# Required: source scripts/env.sh credentials (XBOX_IP, XBOX_USER, XBOX_PASS)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/env.sh"

BASE_URL="https://${XBOX_IP}:${XBOX_PORT}"
CURL_CFG="$(xbox_curl_config)"
trap 'rm -f "${CURL_CFG}"' EXIT
CURL_AUTH=(--basic -K "${CURL_CFG}" -k -sS)
APP_ID="${XBOX_BITCOIND_APP_ID}"
APP_ENTRY="${XBOX_BITCOIND_APP_ENTRY}"
LOG_NAME="${XBOX_BITCOIND_LOG}"

# Xbox WDP requires X-CSRF-Token on POST/DELETE.
CSRF_TOKEN=$(curl "${CURL_AUTH[@]}" "${BASE_URL}/" -o /dev/null -D - 2>/dev/null |
	sed -n 's/.*[Cc][Ss][Rr][Ff]-[Tt]oken=\([^;[:space:]]*\).*/\1/p' |
	tr -d '\r' | head -n 1)

if [[ -z "${CSRF_TOKEN}" ]]; then
	echo "Warning: failed to extract CSRF token. POST requests may fail." >&2
fi

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
if len(matches) > 1:
    others = ", ".join(p.get("PackageFullName", "") for p in matches[1:])
    sys.stderr.write(
        "Warning: %d packages registered; using highest version %s (also: %s)\n"
        % (len(matches), matches[0].get("PackageFullName", ""), others))
print(matches[0].get("PackageFullName", ""))
'
}

require_pfn() {
	local pfn="${1:-}"
	if [[ -z "${pfn}" ]]; then
		pfn="$(get_pfn)"
	fi
	if [[ -z "${pfn}" ]]; then
		echo "Error: ${APP_ID} package not found on Xbox (not installed yet?)." >&2
		exit 1
	fi
	printf '%s\n' "${pfn}"
}

aumid_for_pfn() {
	local pfn="$1"
	local pfamily
	# shellcheck disable=SC2001
	pfamily=$(echo "${pfn}" | sed 's/_[0-9][0-9.]*_[^_]*__/_/')
	printf '%s!%s' "${pfamily}" "${APP_ENTRY}" | base64 -w0
}

print_log() {
	local pfn
	pfn="$(require_pfn "${1:-}")"
	curl "${CURL_AUTH[@]}" \
		"${BASE_URL}/api/filesystem/apps/file?knownfolderid=LocalAppData&packagefullname=${pfn}&path=\\LocalState&filename=${LOG_NAME}" ||
		true
}

list_localstate() {
	local pfn
	pfn="$(require_pfn "${1:-}")"
	curl "${CURL_AUTH[@]}" \
		"${BASE_URL}/api/filesystem/apps/files?knownfolderid=LocalAppData&packagefullname=${pfn}&path=\\LocalState" ||
		true
}

fetch_file() {
	local pfn="$1" name="$2" out="$3" subdir="${4:-}"
	local path='\LocalState'
	if [[ -n "${subdir}" ]]; then
		path="\\LocalState\\${subdir}"
	fi
	curl "${CURL_AUTH[@]}" --fail -o "${out}" \
		"${BASE_URL}/api/filesystem/apps/file?knownfolderid=LocalAppData&packagefullname=${pfn}&path=${path}&filename=${name}"
	echo "Fetched ${name} -> ${out}"
}

mkdir_localstate() {
	local pfn="$1"
	local relpath="$2"
	local parent_param="%5CLocalState"
	local accumulated=""
	local part
	while IFS= read -r part || [[ -n "${part}" ]]; do
		[[ -z "${part}" ]] && continue
		echo "Creating remote dir LocalState\\${accumulated:+${accumulated}\\}${part} ..."
		RESP=$(curl "${CURL_AUTH[@]}" \
			-H "X-CSRF-Token:${CSRF_TOKEN}" \
			-X POST \
			-d "" \
			"${BASE_URL}/api/filesystem/apps/folder?knownfolderid=LocalAppData&packagefullname=${pfn}&path=${parent_param}&newfoldername=${part}" 2>/dev/null || echo "")
		if [[ -n "${RESP}" ]]; then
			echo "  mkdir response: ${RESP}"
		fi
		parent_param="${parent_param}%5C${part}"
		accumulated="${accumulated:+${accumulated}\\}${part}"
	done < <(printf '%s' "${relpath}" | tr '\134' '\n')
}

print_process_status() {
	curl "${CURL_AUTH[@]}" "${BASE_URL}/api/resourcemanager/processes" |
		APP_ID="${APP_ID}" python3 -c '
import json, os, sys
app_id = os.environ["APP_ID"].lower()
data = json.load(sys.stdin)
matches = [
    p for p in data.get("Processes", [])
    if app_id in (p.get("PackageFullName") or "").lower()
    or "xbox_bitcoind" in (p.get("ImageName") or "").lower()
]
if not matches:
    print("process: not running")
else:
    for p in matches:
        print(
            "process: pid={pid} image={image} running={running} ws={ws} pkg={pkg}".format(
                pid=p.get("ProcessId", ""),
                image=p.get("ImageName", ""),
                running=p.get("IsRunning", ""),
                ws=p.get("WorkingSetSize", ""),
                pkg=p.get("PackageFullName", ""),
            )
        )
'
}

start_app() {
	local pfn
	pfn="$(require_pfn "${1:-}")"
	local aumid
	aumid="$(aumid_for_pfn "${pfn}")"
	local resp http
	resp=$(curl "${CURL_AUTH[@]}" \
		-H "X-CSRF-Token:${CSRF_TOKEN}" \
		-H "Content-Length: 0" \
		-X POST \
		-d "" \
		-w "\n%{http_code}" \
		"${BASE_URL}/api/taskmanager/app?appid=${aumid}" 2>/dev/null || true)
	http=$(printf '%s\n' "${resp}" | tail -n1)
	body=$(printf '%s\n' "${resp}" | sed '$d')
	if [[ "${http}" != "200" && "${http}" != "204" ]]; then
		echo "Error: start-app failed HTTP ${http}: ${body}" >&2
		echo "AUMID (base64)=${aumid}" >&2
		exit 1
	fi
	echo "Started ${pfn}."
}

# True if any xbox_bitcoind process row exists (running or residual suspended shell).
process_listed() {
	curl "${CURL_AUTH[@]}" "${BASE_URL}/api/resourcemanager/processes" 2>/dev/null |
		APP_ID="${APP_ID}" python3 -c '
import json, os, sys
app = (os.environ.get("APP_ID") or "").lower()
d = json.load(sys.stdin)
for p in d.get("Processes", []):
    img = (p.get("ImageName") or "").lower()
    pkg = (p.get("PackageFullName") or "").lower()
    if "xbox_bitcoind" in img or (app and app in pkg):
        sys.exit(0)
sys.exit(1)
'
}

# True only if process is actively running (IsRunning true). Residual UWP shells after a
# clean OnSuspending often stay listed with IsRunning=false — that is NOT "running".
process_actively_running() {
	curl "${CURL_AUTH[@]}" "${BASE_URL}/api/resourcemanager/processes" 2>/dev/null |
		APP_ID="${APP_ID}" python3 -c '
import json, os, sys
app = (os.environ.get("APP_ID") or "").lower()
d = json.load(sys.stdin)
for p in d.get("Processes", []):
    img = (p.get("ImageName") or "").lower()
    pkg = (p.get("PackageFullName") or "").lower()
    if "xbox_bitcoind" not in img and not (app and app in pkg):
        continue
    # Missing IsRunning → assume active (older portals).
    ir = p.get("IsRunning", True)
    if ir in (True, "true", "True", 1, "1"):
        sys.exit(0)
sys.exit(1)
'
}

# Backward-compatible name used by package-gc / diagnose: "is the app up?"
process_running() {
	process_actively_running
}

# App log markers written after RPC stop + join (LocalState\bitcoind.log).
soft_stop_log_clean() {
	local pfn="${1:-}"
	local tmp
	tmp="$(mktemp)"
	# Best-effort; portal/file may lag mid-IBD.
	if ! fetch_file_quiet "${pfn}" "${LOG_NAME}" "${tmp}" 2>/dev/null; then
		rm -f "${tmp}"
		return 1
	fi
	if grep -E -q 'OnSuspending complete|node thread joined|BitcoindMain exited rc=0' "${tmp}" 2>/dev/null; then
		# Prefer recent tail so an old successful stop does not mask a failed one.
		if tail -n 80 "${tmp}" | grep -E -q 'OnSuspending complete|node thread joined|BitcoindMain exited rc=0'; then
			rm -f "${tmp}"
			return 0
		fi
	fi
	rm -f "${tmp}"
	return 1
}

# fetch_file without noisy stdout (used by soft-stop polling).
fetch_file_quiet() {
	local pfn="$1"
	local name="$2"
	local out="$3"
	local subdir="${4:-}"
	local path='\LocalState'
	if [[ -n "${subdir}" ]]; then
		path="\\LocalState\\${subdir}"
	fi
	local code
	code=$(curl "${CURL_AUTH[@]}" \
		-o "${out}" \
		-w "%{http_code}" \
		"${BASE_URL}/api/filesystem/apps/file?knownfolderid=LocalAppData&packagefullname=${pfn}&path=${path}&filename=${name}" 2>/dev/null || echo "000")
	[[ "${code}" == "200" ]]
}

suspend_package() {
	local pkg_b64="$1"
	curl "${CURL_AUTH[@]}" \
		-H "X-CSRF-Token:${CSRF_TOKEN}" \
		-H "Content-Length: 0" \
		-X POST \
		-d "" \
		"${BASE_URL}/api/taskmanager/app/state?package=${pkg_b64}&state=suspend" >/dev/null 2>&1 || true
}

stop_app() {
	local pfn
	pfn="$(require_pfn "${1:-}")"
	# Device Portal expects base64-encoded package full name.
	local pkg_b64
	pkg_b64=$(printf '%s' "${pfn}" | base64 -w0)
	# Soft-stop: suspend → OnSuspending → RPC stop → LevelDB flush.
	# Success = node durable stop, NOT necessarily UWP process row gone.
	# Mid-IBD chainstate can take minutes; default wait 180s (override XBB_SOFT_STOP_MAX_WAIT).
	local max_wait="${XBB_SOFT_STOP_MAX_WAIT:-180}"
	local min_grace="${XBB_SOFT_STOP_MIN_GRACE:-8}"
	local require_exit="${XBB_SOFT_STOP_REQUIRE_EXIT:-0}"
	if ! [[ "${max_wait}" =~ ^[0-9]+$ ]] || ((max_wait < 30)); then
		max_wait=180
	fi
	if ! [[ "${min_grace}" =~ ^[0-9]+$ ]] || ((min_grace < 0)); then
		min_grace=8
	fi
	if ((min_grace > max_wait)); then
		min_grace=$max_wait
	fi
	echo "Soft-stop ${pfn} (suspend → wait up to ${max_wait}s; grace ${min_grace}s; require_exit=${require_exit})…"
	suspend_package "${pkg_b64}"
	local waited=0
	# Re-post suspend periodically: Xbox may drop the first suspend before OnSuspending runs.
	local re_suspend_every=45
	local log_check_every=10
	local log_clean=0
	local active=1
	local listed=1
	while ((waited < max_wait)); do
		if process_actively_running; then
			active=1
		else
			active=0
		fi
		if process_listed; then
			listed=1
		else
			listed=0
		fi
		if ((waited > 0 && waited % log_check_every == 0)); then
			if soft_stop_log_clean "${pfn}"; then
				log_clean=1
			fi
		fi

		# Strong: process fully gone.
		if ((listed == 0)); then
			echo "Clean soft stop after ${waited}s (process gone)."
			echo "Stopped ${pfn}."
			return 0
		fi
		# Strong: durable stop markers + not actively running (ignore old log alone).
		if ((log_clean == 1 && active == 0)); then
			echo "Clean soft stop after ${waited}s (log markers + not actively running)."
			echo "Stopped ${pfn}."
			return 0
		fi
		# Weak: !IsRunning after grace — residual suspended shell is OK (do not DELETE).
		# Grace avoids racing OS IsRunning=false before OnSuspending finishes flush.
		if ((active == 0 && require_exit == 0 && waited >= min_grace)); then
			echo "Clean soft stop after ${waited}s (not actively running; residual shell OK, no DELETE)."
			echo "Stopped ${pfn}."
			return 0
		fi

		if ((waited > 0 && waited % re_suspend_every == 0)); then
			if ((active == 1)); then
				echo "Still active at ${waited}s — re-posting suspend…"
				suspend_package "${pkg_b64}"
			else
				echo "Shell residual at ${waited}s (IsRunning=false) — waiting for grace/log…"
			fi
		fi
		waited=$((waited + 1))
		sleep 1
	done

	# Final checks before DELETE — never treat stale log markers as success while active.
	if soft_stop_log_clean "${pfn}"; then
		log_clean=1
	fi
	if ! process_actively_running; then
		if ((log_clean == 1)); then
			echo "Clean soft stop after ${max_wait}s (log markers + not active at deadline; no DELETE)."
		else
			echo "Clean soft stop after ${max_wait}s (not actively running at deadline; no DELETE)."
		fi
		echo "Stopped ${pfn}."
		return 0
	fi

	echo "Warning: still actively running after ${max_wait}s suspend; sending taskmanager DELETE (may skip final flush)." >&2
	echo "  Tip: raise wait with XBB_SOFT_STOP_MAX_WAIT=300 (or 600) for deep IBD." >&2
	curl "${CURL_AUTH[@]}" \
		-H "X-CSRF-Token:${CSRF_TOKEN}" \
		-H "Content-Length: 0" \
		-X DELETE \
		-d "" \
		"${BASE_URL}/api/taskmanager/app?package=${pkg_b64}" >/dev/null 2>&1 || true
	# Give DELETE a moment; report residual process if any.
	sleep 2
	if process_actively_running; then
		echo "Warning: process still actively running after DELETE." >&2
	elif process_listed; then
		echo "Process not active after DELETE (shell may remain listed)."
	else
		echo "Process gone after DELETE fallback."
	fi
	echo "Stopped ${pfn}."
}

list_bitcoind_packages() {
	curl "${CURL_AUTH[@]}" "${BASE_URL}/api/app/packagemanager/packages" |
		APP_ID="${APP_ID}" python3 -c '
import json, os, re, sys
app_id = os.environ["APP_ID"]
data = json.load(sys.stdin)
matches = [p for p in data.get("InstalledPackages", [])
           if app_id in p.get("PackageRelativeId", "")
           or app_id in p.get("PackageFullName", "")]

def version_key(package):
    name = package.get("PackageFullName", "")
    m = re.search(r"_(\d+(?:\.\d+)*)_", name)
    return tuple(int(x) for x in m.group(1).split(".")) if m else ()

matches.sort(key=version_key, reverse=True)
for p in matches:
    print(p.get("PackageFullName", ""))
'
}

uninstall_package() {
	local pfn="$1"
	# WDP: DELETE package by full name (URL-encoded).
	local enc
	enc=$(python3 -c "import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1], safe=''))" "${pfn}")
	local http body
	body="$(mktemp)"
	http=$(curl "${CURL_AUTH[@]}" \
		-H "X-CSRF-Token:${CSRF_TOKEN}" \
		-H "Content-Length: 0" \
		-X DELETE \
		-d "" \
		-o "${body}" \
		-w "%{http_code}" \
		"${BASE_URL}/api/app/packagemanager/package?package=${enc}" 2>/dev/null || echo "000")
	if [[ "${http}" != "200" && "${http}" != "204" ]]; then
		echo "Error: uninstall ${pfn} HTTP ${http}: $(cat "${body}" 2>/dev/null || true)" >&2
		rm -f "${body}"
		return 1
	fi
	rm -f "${body}"
	echo "Uninstalled ${pfn} (HTTP ${http})."
}

package_gc() {
	local keep=1
	local yes=0
	while [[ $# -gt 0 ]]; do
		case "$1" in
		--keep)
			keep="${2:-1}"
			shift 2
			;;
		--yes | -y)
			yes=1
			shift
			;;
		*)
			echo "Usage: $0 package-gc [--keep N] [--yes]" >&2
			return 1
			;;
		esac
	done
	if ! [[ "${keep}" =~ ^[0-9]+$ ]] || ((keep < 1)); then
		echo "Error: --keep must be integer >= 1" >&2
		return 1
	fi
	mapfile -t pkgs < <(list_bitcoind_packages)
	if [[ ${#pkgs[@]} -eq 0 ]]; then
		echo "No ${APP_ID} packages installed."
		return 0
	fi
	echo "Installed (${#pkgs[@]}), newest first:"
	local i=0
	for p in "${pkgs[@]}"; do
		if ((i < keep)); then
			echo "  KEEP  ${p}"
		else
			echo "  DROP  ${p}"
		fi
		i=$((i + 1))
	done
	if ((${#pkgs[@]} <= keep)); then
		echo "Nothing to remove (keep=${keep})."
		return 0
	fi
	if ((yes != 1)); then
		echo "Re-run with --yes to uninstall DROP entries."
		return 0
	fi
	# Never uninstall while that package process is active — stop newest first.
	if process_running; then
		echo "Stopping running app before GC…"
		stop_app "${pkgs[0]}"
	fi
	i=0
	for p in "${pkgs[@]}"; do
		if ((i >= keep)); then
			uninstall_package "${p}" || true
		fi
		i=$((i + 1))
	done
	echo "Remaining:"
	list_bitcoind_packages || true
}

usage() {
	cat >&2 <<EOF
Usage:
  $0 <path/to/xbox_bitcoind.msix>                         deploy package
  $0 probe                                                console probe
  $0 os-info | packages | disk-usage
  $0 install-cert <cert.cer>
  $0 pfn | get-log [pfn] | list-localstate [pfn]
  $0 fetch-file <pfn> <name> <local-out> [subdir]
  $0 upload-file <local> <pfn> [remote-dir] [remote-name]
  $0 mkdir-localstate <pfn> <relpath>
  $0 start-app [pfn] | stop-app [pfn] | diagnose-startup [pfn]
  $0 package-list | package-gc [--keep N] [--yes]
  $0 status | health | soft-stop-test

Env: XBB_SOFT_STOP_MAX_WAIT (default 180), XBB_SOFT_STOP_MIN_GRACE (8),
     XBB_SOFT_STOP_REQUIRE_EXIT (0) — soft-stop poll / DELETE policy.
EOF
}

# --- subcommands ---

cmd="${1:-}"

if [[ -z "${cmd}" ]]; then
	usage
	exit 1
fi

if [[ "${cmd}" == "probe" ]]; then
	exec "${SCRIPT_DIR}/probe-console.sh"
fi

if [[ "${cmd}" == "os-info" ]]; then
	curl "${CURL_AUTH[@]}" "${BASE_URL}/api/os/info"
	echo
	exit 0
fi

if [[ "${cmd}" == "packages" ]]; then
	curl "${CURL_AUTH[@]}" "${BASE_URL}/api/app/packagemanager/packages"
	echo
	exit 0
fi

if [[ "${cmd}" == "disk-usage" ]]; then
	curl "${CURL_AUTH[@]}" "${BASE_URL}/api/devices/file/usage"
	echo
	exit 0
fi

if [[ "${cmd}" == "pfn" ]]; then
	require_pfn "${2:-}"
	exit 0
fi

if [[ "${cmd}" == "get-log" ]]; then
	print_log "${2:-}"
	exit 0
fi

if [[ "${cmd}" == "list-localstate" ]]; then
	list_localstate "${2:-}"
	exit 0
fi

if [[ "${cmd}" == "fetch-file" ]]; then
	PFN="$(require_pfn "${2:-}")"
	NAME="${3:-}"
	OUT="${4:-}"
	SUBDIR="${5:-}"
	if [[ -z "${NAME}" || -z "${OUT}" ]]; then
		echo "Usage: $0 fetch-file <pfn> <name> <local-out> [subdir]" >&2
		exit 1
	fi
	fetch_file "${PFN}" "${NAME}" "${OUT}" "${SUBDIR}"
	exit 0
fi

if [[ "${cmd}" == "start-app" ]]; then
	start_app "${2:-}"
	exit 0
fi

if [[ "${cmd}" == "stop-app" ]]; then
	stop_app "${2:-}"
	exit 0
fi

if [[ "${cmd}" == "package-list" ]]; then
	list_bitcoind_packages
	exit 0
fi

if [[ "${cmd}" == "package-gc" ]]; then
	shift
	package_gc "$@"
	exit 0
fi

if [[ "${cmd}" == "status" ]]; then
	exec "${SCRIPT_DIR}/node-status.sh" "${@:2}"
fi

if [[ "${cmd}" == "health" ]]; then
	exec "${SCRIPT_DIR}/health-check.sh" "${@:2}"
fi

if [[ "${cmd}" == "soft-stop-test" ]]; then
	exec "${SCRIPT_DIR}/soft-stop-test.sh" "${@:2}"
fi

if [[ "${cmd}" == "diagnose-startup" ]]; then
	PFN="$(require_pfn "${2:-}")"
	echo "PFN: ${PFN}"
	echo "--- starting app ---"
	start_app "${PFN}"
	sleep 5
	echo "--- process ---"
	print_process_status
	echo "--- ${LOG_NAME} ---"
	print_log "${PFN}"
	echo ""
	echo "--- LocalState ---"
	list_localstate "${PFN}"
	echo ""
	exit 0
fi

if [[ "${cmd}" == "mkdir-localstate" ]]; then
	PFN="${2:-}"
	RELPATH="${3:-}"
	if [[ -z "${PFN}" || -z "${RELPATH}" ]]; then
		echo "Usage: $0 mkdir-localstate <package-full-name> <relpath>" >&2
		exit 1
	fi
	mkdir_localstate "${PFN}" "${RELPATH}"
	exit 0
fi

if [[ "${cmd}" == "install-cert" ]]; then
	CER="${2:-}"
	if [[ -z "${CER}" || ! -f "${CER}" ]]; then
		echo "Usage: $0 install-cert <path/to/cert.cer>" >&2
		exit 1
	fi
	echo "Installing certificate $(basename "${CER}") on Xbox at ${XBOX_IP} ..."
	RESP=$(curl "${CURL_AUTH[@]}" \
		-H "X-CSRF-Token:${CSRF_TOKEN}" \
		-X POST \
		-F "file=@${CER};type=application/octet-stream" \
		"${BASE_URL}/api/app/packagemanager/certificate?package=$(basename "${CER}")")
	echo "Response: ${RESP}"
	echo "Certificate installed."
	exit 0
fi

if [[ "${cmd}" == "upload-file" ]]; then
	LOCAL_PATH="${2:-}"
	PFN="${3:-}"
	REMOTE_DIR="${4:-}"
	REMOTE_NAME="${5:-}"

	if [[ -z "${LOCAL_PATH}" || -z "${PFN}" ]]; then
		echo "Usage: $0 upload-file <local-path> <package-full-name> [remote-dir] [remote-name]" >&2
		exit 1
	fi
	if [[ ! -f "${LOCAL_PATH}" ]]; then
		echo "Error: file not found: ${LOCAL_PATH}" >&2
		exit 1
	fi

	if [[ -n "${REMOTE_DIR}" ]]; then
		mkdir_localstate "${PFN}" "${REMOTE_DIR}"
	fi

	if [[ -n "${REMOTE_DIR}" ]]; then
		PATH_PARAM="%5CLocalState%5C${REMOTE_DIR//\\/%5C}"
	else
		PATH_PARAM="%5CLocalState"
	fi

	REMOTE_NAME="${REMOTE_NAME:-$(basename "${LOCAL_PATH}")}"
	printf 'Uploading %s → LocalState\\%s\\%s ...\n' "$(basename "${LOCAL_PATH}")" "${REMOTE_DIR}" "${REMOTE_NAME}"
	RESP=$(curl "${CURL_AUTH[@]}" \
		-H "X-CSRF-Token:${CSRF_TOKEN}" \
		-X POST \
		-F "file=@${LOCAL_PATH};type=application/octet-stream;filename=${REMOTE_NAME}" \
		"${BASE_URL}/api/filesystem/apps/file?knownfolderid=LocalAppData&packagefullname=${PFN}&path=${PATH_PARAM}" 2>/dev/null || echo "")
	if [[ -n "${RESP}" ]]; then
		echo "${RESP}"
		if ! echo "${RESP}" | python3 -c "import json,sys; d=json.load(sys.stdin); sys.exit(0 if d.get('Success',True) else 1)" 2>/dev/null; then
			echo "  ERROR: WDP upload failed" >&2
			exit 1
		fi
	fi
	echo "Done."
	exit 0
fi

# Default: deploy an .msix/.appx
APPX="${cmd}"
if [[ ! -f "${APPX}" ]]; then
	# Unknown subcommand or missing file
	if [[ "${APPX}" == *".msix" || "${APPX}" == *".appx" ]]; then
		echo "Error: file not found: ${APPX}" >&2
	else
		echo "Error: unknown subcommand or missing file: ${APPX}" >&2
		usage
	fi
	exit 1
fi

APPX_NAME=$(basename "${APPX}")
APPX_DIR=$(dirname "${APPX}")

# Auto-install companion .cer next to the package if present
for CER_PATH in \
	"${APPX_DIR}/${APPX_NAME%.msix}.cer" \
	"${APPX_DIR}/xbox_bitcoind-dev.cer" \
	"${APPX_DIR}/../xbox_bitcoind-dev.cer" \
	"${SCRIPT_DIR}/../uwp/xbox_bitcoind-dev.cer"; do
	if [[ -f "${CER_PATH}" ]]; then
		echo "Found companion certificate: $(readlink -f "${CER_PATH}")"
		"$0" install-cert "$(readlink -f "${CER_PATH}")" || true
		echo ""
		break
	fi
done

DEPS=()
DEPS_DIR="${APPX_DIR}/Dependencies/x64"
if [[ -d "${DEPS_DIR}" ]]; then
	while IFS= read -r -d '' dep; do
		DEPS+=("-F" "file=@${dep};type=application/octet-stream")
		echo "  + dependency: $(basename "${dep}")"
	done < <(find "${DEPS_DIR}" -name "*.appx" -print0)
fi

echo "Deploying ${APPX_NAME} to Xbox at ${XBOX_IP} ..."
RESP=$(curl "${CURL_AUTH[@]}" \
	-H "X-CSRF-Token:${CSRF_TOKEN}" \
	-X POST \
	-F "file=@${APPX};type=application/octet-stream" \
	"${DEPS[@]}" \
	"${BASE_URL}/api/app/packagemanager/package?package=${APPX_NAME}")
echo "Response: ${RESP}"

# Poll install state briefly
for _ in $(seq 1 30); do
	STATE=$(curl "${CURL_AUTH[@]}" "${BASE_URL}/api/app/packagemanager/state" 2>/dev/null || echo "")
	if echo "${STATE}" | python3 -c 'import json,sys
try:
 d=json.load(sys.stdin)
except Exception:
 sys.exit(1)
# WDP returns IsRunning / state fields depending on OS build
running = d.get("IsRunning", d.get("IsBusy", False))
sys.exit(0 if not running else 1)' 2>/dev/null; then
		break
	fi
	sleep 1
done

if PFN="$(get_pfn)" && [[ -n "${PFN}" ]]; then
	echo "Installed: ${PFN}"
	echo "Reminder: Dev Home → package → View details → App type → Game"
else
	echo "Warning: package not listed yet; check Device Portal UI." >&2
fi
