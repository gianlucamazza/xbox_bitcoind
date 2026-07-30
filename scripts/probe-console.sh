#!/usr/bin/env bash
# probe-console.sh — verify Device Portal reachability and print console baseline.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${SCRIPT_DIR}/env.sh"

BASE_URL="https://${XBOX_IP}:${XBOX_PORT}"
CURL_AUTH=(--basic -u "${XBOX_USER}:${XBOX_PASS}" -k -sS --connect-timeout 5 --max-time 20)

echo "=== xbox_bitcoind console probe ==="
echo "env:     ${XBOX_ENV_SOURCE}"
echo "portal:  ${BASE_URL}"
echo "user:    ${XBOX_USER}"
echo

echo "--- os/info ---"
OS_JSON="$(curl "${CURL_AUTH[@]}" -w "\nHTTP %{http_code}\n" "${BASE_URL}/api/os/info")"
echo "${OS_JSON}"
echo

HTTP_CODE="$(echo "${OS_JSON}" | tail -n1 | awk '{print $2}')"
if [[ "${HTTP_CODE}" != "200" ]]; then
	echo "FAIL: Device Portal os/info returned HTTP ${HTTP_CODE}" >&2
	exit 1
fi

echo "--- disk / file usage (if available) ---"
USAGE="$(curl "${CURL_AUTH[@]}" -w "\nHTTP %{http_code}\n" "${BASE_URL}/api/devices/file/usage" 2>/dev/null || true)"
echo "${USAGE}"
echo

echo "--- packages (xllama / bitcoind) ---"
curl "${CURL_AUTH[@]}" "${BASE_URL}/api/app/packagemanager/packages" | python3 -c '
import json, sys
data = json.load(sys.stdin)
pkgs = data.get("InstalledPackages") or []
print(f"installed_packages={len(pkgs)}")
keys = ("xllama", "bitcoind", "bitcoin")
for p in pkgs:
    name = p.get("PackageFullName") or p.get("Name") or ""
    low = name.lower()
    if any(k in low for k in keys):
        print(f"  {name}")
'
echo

echo "OK: console reachable."
echo "Note: after installing xbox_bitcoind MSIX, set App type to Game in Dev Home."
