#!/usr/bin/env bash
# check-doc-versions.sh — enforce the version SSOT across docs (ci-linux lint).
#
# SSOT:
#   console package rev → docs/tracking.md snapshot row "Console package"
#   latest release      → first "## [X.Y.Z]" section in CHANGELOG.md
#
# Every other doc that quotes the live console rev or the latest release must
# agree with those two values (historical mentions live in dated log sections
# and are not checked). This is what stopped working when ~14 hand-maintained
# copies drifted — see the v0.1.4 review.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

CONSOLE_REV="$(sed -nE 's/^\| *Console package *\| *\*\*([0-9.]+)\*\*.*/\1/p' docs/tracking.md | head -1)"
LATEST_REL="$(sed -nE 's/^## \[([0-9]+\.[0-9]+\.[0-9]+)\].*/\1/p' CHANGELOG.md | head -1)"

if [[ -z "${CONSOLE_REV}" ]]; then
	echo "SSOT missing: no 'Console package | **rev**' row in docs/tracking.md" >&2
	exit 1
fi
if [[ -z "${LATEST_REL}" ]]; then
	echo "SSOT missing: no '## [X.Y.Z]' section in CHANGELOG.md" >&2
	exit 1
fi

fail=0
require() {
	local file="$1" needle="$2" what="$3"
	if ! grep -Fq "${needle}" "${file}"; then
		echo "MISMATCH: ${file} does not mention ${what} '${needle}'" >&2
		echo "  SSOT: console rev ← docs/tracking.md · latest release ← CHANGELOG.md" >&2
		fail=1
	fi
}

# Live console rev (SSOT: tracking.md)
require README.md "${CONSOLE_REV}" "console package rev"
require docs/README.md "${CONSOLE_REV}" "console package rev"
require docs/console.md "${CONSOLE_REV}" "console package rev"
require docs/plan-core-uwp.md "${CONSOLE_REV}" "console package rev"

# Latest release (SSOT: CHANGELOG.md)
require README.md "releases/tag/v${LATEST_REL}" "latest release link"
require CHANGELOG.md "[${LATEST_REL}]: https://github.com/gianlucamazza/xbox_bitcoind/releases/tag/v${LATEST_REL}" "release link ref"

if [[ "${fail}" -ne 0 ]]; then
	exit 1
fi
echo "doc-versions OK: console ${CONSOLE_REV} · latest release v${LATEST_REL}"
