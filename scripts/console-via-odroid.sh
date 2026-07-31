#!/usr/bin/env bash
# console-via-odroid.sh — reach Xbox Device Portal when the laptop is off the
# home LAN, using Odroid on Tailscale (ssh Host odroid-ts) as a jump host.
#
# Why not `ssh -L`?
#   Odroid sshd has AllowTcpForwarding no (hardening). We bridge with:
#     laptop:11443  --socat-->  ssh odroid-ts 'nc XBOX:11443'  --> Device Portal
#
# Usage:
#   source scripts/console-via-odroid.sh     # start bridge + set overrides for this shell
#   ./scripts/probe-console.sh
#   ./scripts/node-status.sh
#   ./scripts/apply-console-conf.sh
#
#   scripts/console-via-odroid.sh status|stop
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Defaults match home lab; override before source if needed.
XBOX_LAN_IP="${XBOX_LAN_IP:-192.168.1.44}"
XBOX_PORT_LOCAL="${XBOX_PORT_LOCAL:-11443}"
ODROID_SSH="${ODROID_SSH:-odroid-ts}"
LISTEN_IP="${XBOX_BRIDGE_LISTEN:-127.0.0.1}"
PID_FILE="${XBOX_BRIDGE_PID:-${XDG_RUNTIME_DIR:-/tmp}/xbox-bitcoind-odroid-bridge.pid}"
LOG_FILE="${XBOX_BRIDGE_LOG:-/tmp/xbox-bitcoind-odroid-bridge.log}"

cmd="${1:-start}"

bridge_running() {
	if [[ -f "${PID_FILE}" ]]; then
		local pid
		pid="$(cat "${PID_FILE}" 2>/dev/null || true)"
		if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
			return 0
		fi
	fi
	return 1
}

stop_bridge() {
	if [[ -f "${PID_FILE}" ]]; then
		local pid
		pid="$(cat "${PID_FILE}" 2>/dev/null || true)"
		if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
			kill "${pid}" 2>/dev/null || true
			sleep 0.2
			kill -9 "${pid}" 2>/dev/null || true
			echo "stopped bridge pid=${pid}"
		fi
		rm -f "${PID_FILE}"
	else
		echo "no pid file (${PID_FILE})"
	fi
}

status_bridge() {
	if bridge_running; then
		echo "bridge: running pid=$(cat "${PID_FILE}") listen=${LISTEN_IP}:${XBOX_PORT_LOCAL} → ${ODROID_SSH} → ${XBOX_LAN_IP}:11443"
		ss -ltn 2>/dev/null | grep -E ":${XBOX_PORT_LOCAL}\\b" || true
		return 0
	fi
	echo "bridge: not running"
	return 1
}

start_bridge() {
	if ! command -v socat >/dev/null 2>&1; then
		echo "socat required (pacman -S socat / apt install socat)" >&2
		return 1
	fi
	if ! command -v ssh >/dev/null 2>&1; then
		echo "ssh required" >&2
		return 1
	fi

	if bridge_running; then
		echo "bridge already running pid=$(cat "${PID_FILE}")"
	else
		# Free listen port if a stale process holds it (ignore errors).
		if ss -ltn 2>/dev/null | grep -qE "${LISTEN_IP}:${XBOX_PORT_LOCAL}\\b|: ${XBOX_PORT_LOCAL}\\b"; then
			echo "warning: something already listens on ${LISTEN_IP}:${XBOX_PORT_LOCAL}" >&2
		fi

		# Probe odroid + console from odroid (fail fast).
		if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "${ODROID_SSH}" \
			"nc -z -w 3 ${XBOX_LAN_IP} 11443" >/dev/null 2>&1; then
			echo "cannot reach ${XBOX_LAN_IP}:11443 via ${ODROID_SSH}" >&2
			echo "  check: ssh ${ODROID_SSH} && console on LAN / Device Portal enabled" >&2
			return 1
		fi

		# shellcheck disable=SC2086
		nohup socat \
			TCP-LISTEN:"${XBOX_PORT_LOCAL}",bind="${LISTEN_IP}",reuseaddr,fork \
			EXEC:"ssh -o BatchMode=yes -o ConnectTimeout=15 ${ODROID_SSH} nc ${XBOX_LAN_IP} 11443" \
			>"${LOG_FILE}" 2>&1 &
		echo $! >"${PID_FILE}"
		sleep 0.4
		if ! bridge_running; then
			echo "bridge failed to stay up; see ${LOG_FILE}" >&2
			return 1
		fi
		echo "bridge started pid=$(cat "${PID_FILE}") log=${LOG_FILE}"
	fi

	# Smoke HTTPS (401 = portal up, auth required).
	local code
	code="$(curl -sk --connect-timeout 8 -o /dev/null -w '%{http_code}' \
		"https://${LISTEN_IP}:${XBOX_PORT_LOCAL}/" || true)"
	if [[ "${code}" != "401" && "${code}" != "200" ]]; then
		echo "warning: portal via bridge returned HTTP ${code:-none} (expected 401)" >&2
	else
		echo "portal OK via bridge (HTTP ${code})"
	fi

	export XBOX_IP_OVERRIDE="${LISTEN_IP}"
	export XBOX_PORT_OVERRIDE="${XBOX_PORT_LOCAL}"
	export XBOX_LAN_IP
	export ODROID_SSH
	echo "exported XBOX_IP_OVERRIDE=${XBOX_IP_OVERRIDE} XBOX_PORT_OVERRIDE=${XBOX_PORT_OVERRIDE}"
	echo "next: source ${ROOT}/scripts/env.sh   # or run probe/deploy (they source env)"
}

case "${cmd}" in
start)
	start_bridge
	;;
stop)
	stop_bridge
	;;
status)
	status_bridge
	;;
*)
	echo "Usage: $0 {start|stop|status}" >&2
	echo "   or: source $0   # same as start + exports for current shell" >&2
	exit 1
	;;
esac

# When sourced, do not exit the parent shell on success.
if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
	return 0
fi
