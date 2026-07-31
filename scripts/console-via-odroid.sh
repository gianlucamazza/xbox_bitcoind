#!/usr/bin/env bash
# console-via-odroid.sh — reach Xbox Device Portal off home LAN via Odroid
# (Tailscale host odroid-ts).
#
# Preferred: native SSH local forward (Odroid Match User gmazza):
#   ssh -N -L 127.0.0.1:11443:192.168.1.44:11443 odroid-ts
# Fallback: socat + ssh 'nc' if TCP forwarding is denied.
#
# Odroid (2026-07-31):
#   global AllowTcpForwarding no  (hardening.conf)
#   Match User gmazza → AllowTcpForwarding local
#                      PermitOpen 192.168.1.44:11443
#   drop-in: /etc/ssh/sshd_config.d/zz-gmazza-local-forward.conf
#
# Usage:
#   source scripts/console-via-odroid.sh
#   ./scripts/probe-console.sh
#   scripts/console-via-odroid.sh status|stop
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

XBOX_LAN_IP="${XBOX_LAN_IP:-192.168.1.44}"
XBOX_PORT_LOCAL="${XBOX_PORT_LOCAL:-11443}"
ODROID_SSH="${ODROID_SSH:-odroid-ts}"
LISTEN_IP="${XBOX_BRIDGE_LISTEN:-127.0.0.1}"
PID_FILE="${XBOX_BRIDGE_PID:-${XDG_RUNTIME_DIR:-/tmp}/xbox-bitcoind-odroid-bridge.pid}"
LOG_FILE="${XBOX_BRIDGE_LOG:-/tmp/xbox-bitcoind-odroid-bridge.log}"
MODE_FILE="${XBOX_BRIDGE_MODE:-${XDG_RUNTIME_DIR:-/tmp}/xbox-bitcoind-odroid-bridge.mode}"

cmd="${1:-start}"

listener_pid() {
	# Best-effort: ss -p may need privileges; fall back empty.
	ss -ltnp 2>/dev/null | sed -n "s/.*:${XBOX_PORT_LOCAL} .*pid=\\([0-9]\\+\\).*/\\1/p" | head -1
}

bridge_running() {
	if [[ -f "${PID_FILE}" ]]; then
		local pid
		pid="$(cat "${PID_FILE}" 2>/dev/null || true)"
		if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
			return 0
		fi
	fi
	# Listener present even if pid file stale.
	if ss -ltn 2>/dev/null | grep -qE ":${XBOX_PORT_LOCAL}\\b"; then
		return 0
	fi
	return 1
}

stop_bridge() {
	local pid=""
	if [[ -f "${PID_FILE}" ]]; then
		pid="$(cat "${PID_FILE}" 2>/dev/null || true)"
	fi
	if [[ -z "${pid}" ]]; then
		pid="$(listener_pid)"
	fi
	if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
		kill "${pid}" 2>/dev/null || true
		sleep 0.2
		kill -9 "${pid}" 2>/dev/null || true
		echo "stopped tunnel pid=${pid} mode=$(cat "${MODE_FILE}" 2>/dev/null || echo unknown)"
	else
		echo "no running tunnel found"
	fi
	rm -f "${PID_FILE}" "${MODE_FILE}"
}

status_bridge() {
	if bridge_running; then
		echo "tunnel: running pid=$(cat "${PID_FILE}" 2>/dev/null || listener_pid || echo '?') mode=$(cat "${MODE_FILE}" 2>/dev/null || echo unknown)"
		echo "  listen ${LISTEN_IP}:${XBOX_PORT_LOCAL} → ${ODROID_SSH} → ${XBOX_LAN_IP}:11443"
		ss -ltn 2>/dev/null | grep -E ":${XBOX_PORT_LOCAL}\\b" || true
		return 0
	fi
	echo "tunnel: not running"
	return 1
}

export_overrides() {
	export XBOX_IP_OVERRIDE="${LISTEN_IP}"
	export XBOX_PORT_OVERRIDE="${XBOX_PORT_LOCAL}"
	export XBOX_LAN_IP
	export ODROID_SSH
	echo "exported XBOX_IP_OVERRIDE=${XBOX_IP_OVERRIDE} XBOX_PORT_OVERRIDE=${XBOX_PORT_OVERRIDE}"
}

smoke_portal() {
	local code
	code="$(curl -sk --connect-timeout 8 -o /dev/null -w '%{http_code}' \
		"https://${LISTEN_IP}:${XBOX_PORT_LOCAL}/" || true)"
	if [[ "${code}" != "401" && "${code}" != "200" ]]; then
		echo "warning: portal via tunnel returned HTTP ${code:-none} (expected 401)" >&2
		return 1
	fi
	echo "portal OK via tunnel (HTTP ${code})"
	return 0
}

start_ssh_L() {
	# Background local forward; ExitOnForwardFailure aborts if sshd refuses.
	ssh -f -N \
		-o ExitOnForwardFailure=yes \
		-o BatchMode=yes \
		-o ServerAliveInterval=30 \
		-o ConnectTimeout=15 \
		-L "${LISTEN_IP}:${XBOX_PORT_LOCAL}:${XBOX_LAN_IP}:11443" \
		"${ODROID_SSH}"
	sleep 0.4
	local pid
	pid="$(listener_pid)"
	if [[ -z "${pid}" ]]; then
		# ssh -f may leave a process without ss -p visibility; still check port.
		if ! ss -ltn 2>/dev/null | grep -qE ":${XBOX_PORT_LOCAL}\\b"; then
			return 1
		fi
		pid="ssh-L"
	fi
	echo "${pid}" >"${PID_FILE}"
	echo "ssh-L" >"${MODE_FILE}"
	echo "tunnel started mode=ssh-L pid=${pid}"
}

start_socat() {
	if ! command -v socat >/dev/null 2>&1; then
		echo "socat required for fallback (pacman -S socat)" >&2
		return 1
	fi
	nohup socat \
		TCP-LISTEN:"${XBOX_PORT_LOCAL}",bind="${LISTEN_IP}",reuseaddr,fork \
		EXEC:"ssh -o BatchMode=yes -o ConnectTimeout=15 ${ODROID_SSH} nc ${XBOX_LAN_IP} 11443" \
		>"${LOG_FILE}" 2>&1 &
	echo $! >"${PID_FILE}"
	echo "socat" >"${MODE_FILE}"
	sleep 0.4
	if ! kill -0 "$(cat "${PID_FILE}")" 2>/dev/null; then
		echo "socat bridge failed; see ${LOG_FILE}" >&2
		return 1
	fi
	echo "tunnel started mode=socat pid=$(cat "${PID_FILE}") log=${LOG_FILE}"
}

start_bridge() {
	if ! command -v ssh >/dev/null 2>&1; then
		echo "ssh required" >&2
		return 1
	fi

	if bridge_running && [[ -f "${PID_FILE}" ]] && kill -0 "$(cat "${PID_FILE}")" 2>/dev/null; then
		echo "tunnel already running pid=$(cat "${PID_FILE}") mode=$(cat "${MODE_FILE}" 2>/dev/null || echo unknown)"
	else
		if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "${ODROID_SSH}" \
			"nc -z -w 3 ${XBOX_LAN_IP} 11443" >/dev/null 2>&1; then
			echo "cannot reach ${XBOX_LAN_IP}:11443 via ${ODROID_SSH}" >&2
			echo "  check: ssh ${ODROID_SSH}; console on LAN; Device Portal enabled" >&2
			return 1
		fi

		if start_ssh_L 2>/tmp/xbb-ssh-L.err; then
			:
		else
			echo "ssh -L failed; trying socat fallback..." >&2
			if [[ -s /tmp/xbb-ssh-L.err ]]; then
				sed 's/^/  /' /tmp/xbb-ssh-L.err >&2 || true
			fi
			start_socat
		fi
	fi

	smoke_portal || true
	export_overrides
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

if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
	return 0
fi
