#!/usr/bin/env bash
# Apply Xbox UWP patches to third_party/bitcoin (must match pin).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/third_party/bitcoin"
PATCH_DIR="${ROOT}/patches/uwp"

if [[ ! -d "${SRC}/.git" && ! -f "${SRC}/CMakeLists.txt" ]]; then
	echo "Core tree missing; run scripts/fetch-bitcoin-core.sh first" >&2
	exit 1
fi

# Idempotent marker
MARKER="${SRC}/.xbb-uwp-patches-applied"
if [[ -f "${MARKER}" ]]; then
	echo "UWP patches already applied ($(cat "${MARKER}"))"
	exit 0
fi

shopt -s nullglob
patches=("${PATCH_DIR}"/*.patch)
if [[ ${#patches[@]} -eq 0 ]]; then
	echo "No patches in ${PATCH_DIR}" >&2
	exit 1
fi

for p in "${patches[@]}"; do
	echo "Applying $(basename "$p") ..."
	# git apply prefers repo root; patch -p1 also works
	if git -C "${SRC}" apply --check "${p}" 2>/dev/null; then
		git -C "${SRC}" apply "${p}"
	elif patch -d "${SRC}" -p1 --dry-run <"${p}" >/dev/null 2>&1; then
		patch -d "${SRC}" -p1 <"${p}"
	else
		echo "Failed to apply ${p}" >&2
		exit 1
	fi
done

echo "uwp-$(date -Iseconds)" >"${MARKER}"
echo "OK: applied ${#patches[@]} UWP patches"
