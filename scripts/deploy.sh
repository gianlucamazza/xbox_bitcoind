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
#   deploy.sh stop-app [pfn]
#   deploy.sh diagnose-startup [pfn]
#
# Required: source scripts/env.sh credentials (XBOX_IP, XBOX_USER, XBOX_PASS)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/env.sh"

BASE_URL="https://${XBOX_IP}:${XBOX_PORT}"
CURL_AUTH=(--basic -u "${XBOX_USER}:${XBOX_PASS}" -k -sS)
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

stop_app() {
	local pfn
	pfn="$(require_pfn "${1:-}")"
	# Device Portal expects base64-encoded package full name.
	local pkg_b64
	pkg_b64=$(printf '%s' "${pfn}" | base64 -w0)
	curl "${CURL_AUTH[@]}" \
		-H "X-CSRF-Token:${CSRF_TOKEN}" \
		-H "Content-Length: 0" \
		-X DELETE \
		-d "" \
		"${BASE_URL}/api/taskmanager/app?package=${pkg_b64}" >/dev/null 2>&1 || true
	echo "Stopped ${pfn}."
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
