#!/usr/bin/env bash
# cut-release.sh — create an annotated version tag and push it to origin.
#
# Pushing tag v* triggers .github/workflows/release.yml which:
#   builds WithCore MSIX → publishes GitHub Release with .msix + .cer
#
# Usage:
#   ./scripts/cut-release.sh 0.2.0
#   ./scripts/cut-release.sh 0.2.0 --dry-run
#   ./scripts/cut-release.sh 0.2.0 --force     # skip the CHANGELOG section gate
#   ./scripts/cut-release.sh 0.2.0-rc.1
#
# Prerequisites: clean main (or current branch), synced with origin.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

DRY=0
FORCE=0
VERSION=""
for arg in "$@"; do
	case "$arg" in
	--dry-run) DRY=1 ;;
	--force) FORCE=1 ;;
	-h | --help)
		sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
		exit 0
		;;
	*)
		if [[ -z "${VERSION}" ]]; then
			VERSION="$arg"
		else
			echo "Unexpected arg: $arg" >&2
			exit 1
		fi
		;;
	esac
done

if [[ -z "${VERSION}" ]]; then
	echo "Usage: $0 <version> [--dry-run]" >&2
	echo "  example: $0 0.2.0" >&2
	exit 1
fi

# Accept 0.2.0 or v0.2.0
VERSION="${VERSION#v}"
if [[ ! "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.-]+)?$ ]]; then
	echo "Version must look like 0.2.0 or 0.2.0-rc.1 (got: ${VERSION})" >&2
	exit 1
fi

TAG="v${VERSION}"

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
	echo "Not a git repo" >&2
	exit 1
fi

if [[ -n "$(git status --porcelain)" ]]; then
	echo "Working tree not clean — commit or stash first." >&2
	git status -sb >&2
	exit 1
fi

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [[ "${BRANCH}" != "main" ]]; then
	echo "Warning: not on main (on ${BRANCH}). Tag will point at this commit." >&2
fi

git fetch origin --tags --quiet 2>/dev/null || true

if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
	echo "Tag already exists locally: ${TAG}" >&2
	exit 1
fi

if git ls-remote --exit-code --tags origin "refs/tags/${TAG}" >/dev/null 2>&1; then
	echo "Tag already exists on origin: ${TAG}" >&2
	exit 1
fi

HEAD="$(git rev-parse --short HEAD)"
MSG="xbox_bitcoind ${TAG}

Bitcoin Core pin: $(grep -E '^TAG=' config/bitcoin-core.pin | cut -d= -f2)
Commit: $(git rev-parse HEAD)
"

echo "Will create annotated tag:"
echo "  tag:    ${TAG}"
echo "  head:   ${HEAD} (${BRANCH})"
echo "  remote: origin"
echo "  next:   push tag → release.yml builds MSIX + publishes GitHub Release"
echo

if [[ "${DRY}" -eq 1 ]]; then
	echo "Dry run — no tag created."
	exit 0
fi

# CHANGELOG gate: the release notes embed the '## [X.Y.Z]' section — a tag
# without it ships an undocumented release. Blocking; override with --force.
if [[ -f CHANGELOG.md ]] && ! grep -q "## \[${VERSION}\]" CHANGELOG.md; then
	if [[ "${FORCE}" -eq 1 ]]; then
		echo "Warning: CHANGELOG.md has no '## [${VERSION}]' section (--force given, continuing)." >&2
	else
		echo "Error: CHANGELOG.md has no '## [${VERSION}]' section." >&2
		echo "  Move the Unreleased notes under '## [${VERSION}]' first, or pass --force." >&2
		exit 1
	fi
fi

git tag -a "${TAG}" -m "${MSG}"
echo "Created ${TAG}"

git push origin "${TAG}"
echo "Pushed ${TAG} to origin."
REPO_SLUG="$(git remote get-url origin | sed -E 's#.*github.com[:/]##' | sed 's/\.git$//')"
echo
echo "Watch: https://github.com/${REPO_SLUG}/actions/workflows/release.yml"
echo "When green: https://github.com/${REPO_SLUG}/releases/tag/${TAG}"
echo "After publish: move Unreleased notes under ## [${VERSION}] in CHANGELOG.md on main."
