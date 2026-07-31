#!/usr/bin/env bash
# install-ibd-timer.sh — install/enable user systemd timer for hourly IBD samples.
#
# Best practice: unit templates live in-repo; generated units go to
# ~/.config/systemd/user/ (never /etc without explicit ops approval).
#
# Usage:
#   ./scripts/install-ibd-timer.sh           # install + enable + start
#   ./scripts/install-ibd-timer.sh --status
#   ./scripts/install-ibd-timer.sh --uninstall
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
SERVICE_NAME="xbox-bitcoind-ibd-sample.service"
TIMER_NAME="xbox-bitcoind-ibd-sample.timer"
TEMPLATE="${ROOT}/contrib/systemd/user/${SERVICE_NAME}.in"
TIMER_SRC="${ROOT}/contrib/systemd/user/${TIMER_NAME}"

cmd="${1:-install}"

status() {
	systemctl --user status "${TIMER_NAME}" --no-pager 2>/dev/null || true
	systemctl --user list-timers "${TIMER_NAME}" --no-pager 2>/dev/null || true
	echo "log:    ${XDG_STATE_HOME:-$HOME/.local/state}/xbox_bitcoind/ibd.jsonl"
	echo "report: ${ROOT}/scripts/ibd-report.sh"
}

uninstall() {
	systemctl --user disable --now "${TIMER_NAME}" 2>/dev/null || true
	rm -f "${UNIT_DIR}/${SERVICE_NAME}" "${UNIT_DIR}/${TIMER_NAME}"
	systemctl --user daemon-reload
	echo "Removed ${TIMER_NAME} / ${SERVICE_NAME}"
}

install() {
	if ! command -v systemctl >/dev/null; then
		echo "systemctl not found" >&2
		exit 1
	fi
	if [[ ! -f "${TEMPLATE}" || ! -f "${TIMER_SRC}" ]]; then
		echo "Missing unit templates under contrib/systemd/user/" >&2
		exit 1
	fi
	mkdir -p "${UNIT_DIR}"
	# Substitute absolute repo path (portable across clones if re-run).
	sed "s|@REPO@|${ROOT}|g" "${TEMPLATE}" >"${UNIT_DIR}/${SERVICE_NAME}"
	# Timer docs path: keep generic; Exec is in the service.
	cp -f "${TIMER_SRC}" "${UNIT_DIR}/${TIMER_NAME}"
	# Fix Documentation= if present with hard-coded path in timer (optional)
	sed -i "s|file:%h/Workspace/tooling/xbox_bitcoind/docs/ops.md|file:${ROOT}/docs/ops.md|g" \
		"${UNIT_DIR}/${TIMER_NAME}" 2>/dev/null || true

	systemctl --user daemon-reload
	systemctl --user enable --now "${TIMER_NAME}"
	# Fire once immediately so history starts without waiting for the hour.
	systemctl --user start "${SERVICE_NAME}" || true
	echo "Enabled ${TIMER_NAME}"
	status
}

case "${cmd}" in
install | --install | "") install ;;
--status | status) status ;;
--uninstall | uninstall) uninstall ;;
-h | --help)
	sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
	;;
*)
	echo "Unknown command: ${cmd}" >&2
	exit 1
	;;
esac
