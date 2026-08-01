#!/usr/bin/env bash
# Resolve Xbox Device Portal credentials for xbox_bitcoind.
#
# Priority:
#   1. XBOX_ENV_FILE (explicit path)
#   2. ~/.config/xbox_bitcoind/xbox-env
#   3. ~/.config/xllama/xbox-env  (shared Series S console — preferred default)
#
# Usage (from other scripts):
#   # shellcheck source=env.sh
#   source "$(dirname "$0")/env.sh"
#
# Or from a shell:
#   source scripts/env.sh

_xbox_bitcoind_env_loaded=1

# Optional overrides (set before source / before running tools):
#   XBOX_IP_OVERRIDE / XBOX_PORT_OVERRIDE  — e.g. 127.0.0.1 after ssh -L
#   XBOX_ENV_FILE                          — explicit credentials path
_xbox_ip_override="${XBOX_IP_OVERRIDE:-}"
_xbox_port_override="${XBOX_PORT_OVERRIDE:-}"

_xbox_env_candidates=()
if [[ -n "${XBOX_ENV_FILE:-}" ]]; then
	_xbox_env_candidates+=("${XBOX_ENV_FILE}")
fi
_xbox_env_candidates+=(
	"${HOME}/.config/xbox_bitcoind/xbox-env"
	"${HOME}/.config/xllama/xbox-env"
)

_xbox_env_found=""
for _f in "${_xbox_env_candidates[@]}"; do
	if [[ -f "$_f" ]]; then
		# shellcheck disable=SC1090
		source "$_f"
		_xbox_env_found="$_f"
		break
	fi
done

if [[ -z "$_xbox_env_found" ]]; then
	echo "xbox_bitcoind: no xbox-env found." >&2
	echo "  Copy config/xbox-env.example to ~/.config/xbox_bitcoind/xbox-env" >&2
	echo "  or reuse xllama: ensure ~/.config/xllama/xbox-env exists." >&2
	# Sourced → return; executed as a script → exit
	if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
		return 1
	fi
	exit 1
fi

: "${XBOX_IP:?XBOX_IP not set in ${_xbox_env_found}}"
: "${XBOX_USER:?XBOX_USER not set in ${_xbox_env_found}}"
: "${XBOX_PASS:?XBOX_PASS not set in ${_xbox_env_found}}"

# Tunnel / jump-host overrides win over the file (LAN IP may be unreachable).
if [[ -n "${_xbox_ip_override}" ]]; then
	XBOX_IP="${_xbox_ip_override}"
fi
if [[ -n "${_xbox_port_override}" ]]; then
	XBOX_PORT="${_xbox_port_override}"
fi

export XBOX_IP
export XBOX_USER
export XBOX_PASS
export XBOX_PORT="${XBOX_PORT:-11443}"
export XBOX_ENV_SOURCE="${_xbox_env_found}"
unset _xbox_ip_override _xbox_port_override

# Curl config file carrying the Device Portal credential, so the password never
# lands in argv (/proc/*/cmdline is world-readable for the whole request).
# Prints the file path; the caller owns cleanup: trap 'rm -f "${CURL_CFG}"' EXIT.
xbox_curl_config() {
	local _cfg _u _p
	_cfg="$(mktemp)"
	chmod 600 "${_cfg}"
	# curl config quoting: backslashes and double quotes must be escaped.
	_u="${XBOX_USER//\\/\\\\}"
	_u="${_u//\"/\\\"}"
	_p="${XBOX_PASS//\\/\\\\}"
	_p="${_p//\"/\\\"}"
	printf 'user = "%s:%s"\n' "${_u}" "${_p}" >"${_cfg}"
	# Optional TLS pin for the self-signed Device Portal cert. Scripts still pass
	# -k (chain is untrusted by design) but curl honors the pin alongside it, so
	# a LAN MITM can no longer swap the endpoint. Get the value with:
	#   openssl s_client -connect $XBOX_IP:$XBOX_PORT </dev/null 2>/dev/null \
	#     | openssl x509 -pubkey -noout | openssl pkey -pubin -outform DER \
	#     | openssl dgst -sha256 -binary | base64
	if [[ -n "${XBOX_PORTAL_PUBKEY:-}" ]]; then
		printf 'pinnedpubkey = "sha256//%s"\n' "${XBOX_PORTAL_PUBKEY#sha256//}" >>"${_cfg}"
	fi
	printf '%s\n' "${_cfg}"
}

# Package identity for this project (WDP helpers).
# AppX Identity Name: only [-.A-Za-z0-9] (no underscore)
export XBOX_BITCOIND_APP_ID="${XBOX_BITCOIND_APP_ID:-GianlucaMazza.xboxbitcoind}"
# Application@Id in AppxManifest (AppX pattern forbids underscores)
export XBOX_BITCOIND_APP_ENTRY="${XBOX_BITCOIND_APP_ENTRY:-App}"
export XBOX_BITCOIND_LOG="${XBOX_BITCOIND_LOG:-bitcoind.log}"

unset _f _xbox_env_candidates _xbox_env_found
