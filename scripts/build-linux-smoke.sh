#!/usr/bin/env bash
# build-linux-smoke.sh — host Linux smoke build of the *same pin* as MSVC baseline.
# Not a substitute for MSVC; catches CMake/source breakage early on Arch.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIN_FILE="${ROOT}/config/bitcoin-core.pin"
SRC="${ROOT}/third_party/bitcoin"
BUILD="${BUILD_DIR:-${SRC}/build-linux-smoke}"

# shellcheck disable=SC1090
source <(grep -E '^(TAG|COMMIT|BUILD_GUI|ENABLE_WALLET|WITH_ZMQ|ENABLE_IPC|BUILD_TESTS)=' "${PIN_FILE}")

: "${TAG:?}" "${COMMIT:?}"

if [[ ! -f "${SRC}/CMakeLists.txt" ]]; then
	echo "Source missing; running fetch..."
	"${ROOT}/scripts/fetch-bitcoin-core.sh"
fi

HEAD="$(git -C "${SRC}" rev-parse HEAD)"
if [[ "${HEAD}" != "${COMMIT}" ]]; then
	echo "ERROR: ${SRC} is ${HEAD}, pin is ${COMMIT} (${TAG})" >&2
	exit 1
fi

BUILD_GUI="${BUILD_GUI:-OFF}"
ENABLE_WALLET="${ENABLE_WALLET:-OFF}"
WITH_ZMQ="${WITH_ZMQ:-OFF}"
ENABLE_IPC="${ENABLE_IPC:-OFF}"
# Pin defaults BUILD_TESTS=ON; CI and fast smokes override via BUILD_TESTS=OFF or CI_SKIP_TESTS=1
if [[ "${CI_SKIP_TESTS:-}" == "1" || "${CI_SKIP_TESTS:-}" == "true" ]]; then
	BUILD_TESTS=OFF
else
	BUILD_TESTS="${BUILD_TESTS:-ON}"
fi
# Lean daemon-focused targets for smoke (still builds bitcoind + bitcoin-cli)
BUILD_CLI="${BUILD_CLI:-ON}"
BUILD_TX="${BUILD_TX:-OFF}"
BUILD_UTIL="${BUILD_UTIL:-OFF}"
BUILD_BITCOIN_BIN="${BUILD_BITCOIN_BIN:-OFF}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

echo "=== Linux smoke (pin ${TAG} @ ${HEAD}) ==="
echo "Build dir: ${BUILD}"
echo "Flags: type=${CMAKE_BUILD_TYPE} GUI=${BUILD_GUI} WALLET=${ENABLE_WALLET} ZMQ=${WITH_ZMQ} IPC=${ENABLE_IPC} TESTS=${BUILD_TESTS}"

# Prefer Ninja + ccache when available (much faster incremental / CI warm runs).
CMAKE_GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
	CMAKE_GENERATOR_ARGS=(-G Ninja)
	echo "Generator: Ninja"
else
	echo "Generator: default (install ninja for faster builds)"
fi

CMAKE_CCACHE_ARGS=()
if command -v ccache >/dev/null 2>&1; then
	export CCACHE_DIR="${CCACHE_DIR:-${ROOT}/.ccache-linux-smoke}"
	mkdir -p "${CCACHE_DIR}"
	CMAKE_CCACHE_ARGS=(
		-DCMAKE_C_COMPILER_LAUNCHER=ccache
		-DCMAKE_CXX_COMPILER_LAUNCHER=ccache
	)
	echo "ccache: ${CCACHE_DIR}"
	ccache -z >/dev/null 2>&1 || true
fi

cmake -B "${BUILD}" -S "${SRC}" \
	"${CMAKE_GENERATOR_ARGS[@]}" \
	"${CMAKE_CCACHE_ARGS[@]}" \
	-DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
	-DBUILD_GUI="${BUILD_GUI}" \
	-DENABLE_WALLET="${ENABLE_WALLET}" \
	-DWITH_ZMQ="${WITH_ZMQ}" \
	-DENABLE_IPC="${ENABLE_IPC}" \
	-DBUILD_TESTS="${BUILD_TESTS}" \
	-DBUILD_CLI="${BUILD_CLI}" \
	-DBUILD_TX="${BUILD_TX}" \
	-DBUILD_UTIL="${BUILD_UTIL}" \
	-DBUILD_BITCOIN_BIN="${BUILD_BITCOIN_BIN}"

cmake --build "${BUILD}" -j"$(nproc)"

if [[ "${BUILD_TESTS}" == "ON" ]]; then
	ctest --test-dir "${BUILD}" --output-on-failure -j"$(nproc)"
fi

if command -v ccache >/dev/null 2>&1; then
	echo "=== ccache stats ==="
	ccache -s || true
fi

BITCOIND="$(find "${BUILD}" -type f -name bitcoind | head -n1)"
if [[ -z "${BITCOIND}" ]]; then
	echo "ERROR: bitcoind not found under ${BUILD}" >&2
	exit 1
fi

echo ""
echo "OK: ${BITCOIND}"
"${BITCOIND}" -version

# Emit paths for GitHub Actions artifact steps
if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
	{
		echo "bitcoind=${BITCOIND}"
		echo "build_dir=${BUILD}"
	} >>"${GITHUB_OUTPUT}"
fi

echo ""
echo "Optional regtest smoke (not run automatically):"
echo "  DATADIR=\$(mktemp -d) ${BITCOIND} -regtest -datadir=\$DATADIR -server=1 -listen=0"
