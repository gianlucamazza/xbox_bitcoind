#!/usr/bin/env bash
# fetch-bitcoin-core.sh — clone or update third_party/bitcoin to the pinned tag/commit.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIN_FILE="${ROOT}/config/bitcoin-core.pin"
DEST="${ROOT}/third_party/bitcoin"

# shellcheck disable=SC1090
source <(grep -E '^(TAG|COMMIT|REPO_URL)=' "${PIN_FILE}")

: "${TAG:?}" "${COMMIT:?}" "${REPO_URL:?}"

mkdir -p "${ROOT}/third_party"

if [[ ! -d "${DEST}/.git" ]]; then
	echo "Cloning ${REPO_URL} (${TAG}) → ${DEST}"
	git clone --branch "${TAG}" --depth 1 "${REPO_URL}" "${DEST}"
else
	echo "Updating existing clone at ${DEST}"
	git -C "${DEST}" fetch --depth 1 origin "refs/tags/${TAG}:refs/tags/${TAG}" 2>/dev/null \
		|| git -C "${DEST}" fetch --depth 1 origin tag "${TAG}"
	git -C "${DEST}" checkout --detach "${COMMIT}" 2>/dev/null \
		|| git -C "${DEST}" checkout --detach "tags/${TAG}"
fi

HEAD="$(git -C "${DEST}" rev-parse HEAD)"
if [[ "${HEAD}" != "${COMMIT}" ]]; then
	# Depth-1 tag checkout should already be the peeled commit; try explicit fetch of the commit.
	echo "HEAD ${HEAD} != pin ${COMMIT}; fetching commit..."
	git -C "${DEST}" fetch --depth 1 origin "${COMMIT}" 2>/dev/null || true
	git -C "${DEST}" checkout --detach "${COMMIT}"
	HEAD="$(git -C "${DEST}" rev-parse HEAD)"
fi

if [[ "${HEAD}" != "${COMMIT}" ]]; then
	echo "ERROR: checkout is ${HEAD}, expected ${COMMIT} (${TAG})" >&2
	exit 1
fi

echo "OK: Bitcoin Core ${TAG} @ ${HEAD}"
git -C "${DEST}" describe --tags --always
echo "Tree: ${DEST}"
