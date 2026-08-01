#!/usr/bin/env bash
# fetch-bitcoin-core.sh — clone or update third_party/bitcoin to the pinned commit.
#
# Uses COMMIT (not TAG) as the checkout target so annotated release tags do not
# emit git's "refs/tags/… is not a commit!" noise on shallow clones.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIN_FILE="${ROOT}/config/bitcoin-core.pin"
DEST="${ROOT}/third_party/bitcoin"

# shellcheck disable=SC1090
source <(grep -E '^(TAG|COMMIT|REPO_URL)=' "${PIN_FILE}")

: "${TAG:?}" "${COMMIT:?}" "${REPO_URL:?}"

mkdir -p "${ROOT}/third_party"

fetch_commit() {
	local dir="$1"
	git -C "${dir}" fetch --depth 1 origin "${COMMIT}"
	git -C "${dir}" checkout --detach FETCH_HEAD
}

if [[ ! -d "${DEST}/.git" ]]; then
	echo "Cloning ${REPO_URL} @ ${COMMIT} (${TAG}) → ${DEST}"
	rm -rf "${DEST}"
	mkdir -p "${DEST}"
	git -C "${DEST}" init
	git -C "${DEST}" remote add origin "${REPO_URL}"
	fetch_commit "${DEST}"
else
	echo "Updating existing clone at ${DEST}"
	# A previously patched tree makes checkout fail ("local changes would be
	# overwritten"): reset tracked files and drop untracked ones (incl. the patch
	# marker), but keep build dirs — CI caches live under them.
	git -C "${DEST}" reset --hard
	git -C "${DEST}" clean -fdx -e build-uwp -e build-linux-smoke
	fetch_commit "${DEST}"
fi

HEAD="$(git -C "${DEST}" rev-parse HEAD)"
if [[ "${HEAD}" != "${COMMIT}" ]]; then
	echo "ERROR: checkout is ${HEAD}, expected ${COMMIT} (${TAG})" >&2
	exit 1
fi

echo "OK: Bitcoin Core ${TAG} @ ${HEAD}"
# describe may fail on pure shallow commit checkouts; never fail the fetch for it
git -C "${DEST}" describe --tags --always 2>/dev/null || true
echo "Tree: ${DEST}"
