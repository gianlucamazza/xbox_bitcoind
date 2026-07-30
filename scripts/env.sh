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

export XBOX_IP
export XBOX_USER
export XBOX_PASS
export XBOX_PORT="${XBOX_PORT:-11443}"
export XBOX_ENV_SOURCE="${_xbox_env_found}"

# Package identity for this project (WDP helpers).
# AppX Identity Name: only [-.A-Za-z0-9] (no underscore)
export XBOX_BITCOIND_APP_ID="${XBOX_BITCOIND_APP_ID:-GianlucaMazza.xboxbitcoind}"
export XBOX_BITCOIND_APP_ENTRY="${XBOX_BITCOIND_APP_ENTRY:-xbox_bitcoind}"
export XBOX_BITCOIND_LOG="${XBOX_BITCOIND_LOG:-bitcoind.log}"

unset _f _xbox_env_candidates _xbox_env_found
