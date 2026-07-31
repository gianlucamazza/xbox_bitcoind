#!/usr/bin/env bash
# test-ui-layout.sh — compile+run pure layout unit checks (no WinRT / no Xbox).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HDR="${ROOT}/uwp/ui_layout.h"
SRC="$(mktemp "${TMPDIR:-/tmp}/xbb-ui-layout-test.XXXXXX.cpp")"
BIN="$(mktemp "${TMPDIR:-/tmp}/xbb-ui-layout-test.XXXXXX")"
trap 'rm -f "${SRC}" "${BIN}"' EXIT

if [[ ! -f "${HDR}" ]]; then
	echo "missing ${HDR}" >&2
	exit 1
fi

cat >"${SRC}" <<'CPP'
#include "ui_layout.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace xbb;

static void expect(bool ok, const char* msg) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

int main() {
    // 960×540 @ raw 2 → effective ~1080p Standard, 4 cols, secondary+spark if tall enough
    {
        auto L = DiscoverLayoutFromViewport(960, 540, 2.0);
        expect(L.density == UiDensity::Standard, "960x540@2 density Standard");
        expect(L.primary_columns == 4, "960x540@2 four columns");
        expect(L.usable_h > 900, "960x540@2 usable height scale-corrected");
        auto plan = PlanSections(L.usable_h, L);
        expect(plan.show_secondary, "960x540@2 show secondary");
    }
    // Tiny phone-like → Compact, 2 cols
    {
        auto L = DiscoverLayoutFromViewport(640, 360, 1.0);
        expect(L.density == UiDensity::Compact, "640x360 Compact");
        expect(L.primary_columns == 2, "640x360 two columns");
        auto plan = PlanSections(L.usable_h, L);
        expect(!plan.show_spark, "compact tight: no spark");
    }
    // Comfort 1440p-ish
    {
        auto L = DiscoverLayoutFromViewport(2560, 1440, 1.0);
        expect(L.density == UiDensity::Comfort, "2560x1440 Comfort");
        expect(L.primary_columns == 4, "comfort four columns");
    }
    // Scale heuristic when raw=1 but DIP is half-1080
    {
        double scale = EffectiveViewScale(960, 540, 1.0);
        expect(std::fabs(scale - 2.0) < 1e-9, "heuristic scale 2.0 for 960x540");
    }
    // ETA helper
    {
        double h = EstimateEtaHours(0.20, 0.10, 10, 2.0); // 10 samples @ 2s, +0.10 progress
        expect(h > 0, "eta positive while syncing");
        expect(EstimateEtaHours(0.9995, 0.99, 10, 2.0) == 0.0, "near tip eta 0");
        expect(EstimateEtaHours(0.1, 0.1, 10, 2.0) < 0, "flat progress no eta");
    }
    std::puts("ui_layout tests OK");
    return 0;
}
CPP

g++ -std=c++20 -O0 -Wall -Wextra -Werror -I"${ROOT}/uwp" -o "${BIN}" "${SRC}"
"${BIN}"
