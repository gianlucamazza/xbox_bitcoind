// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
//
// Pure layout helpers (no WinRT). Safe to unit-test on Linux hosts.
#pragma once

#include <algorithm>
#include <cmath>

namespace xbb {

// Density from usable viewport height (after safe inset), in effective DIPs.
enum class UiDensity { Compact, Standard, Comfort };

// Scale tokens (fonts, pads, min heights).
struct UiLayout {
    UiDensity density = UiDensity::Standard;
    double viewport_w = 1920;
    double viewport_h = 1080;
    double usable_w = 1920;
    double usable_h = 1080;
    double pad_x = 28;
    double pad_y = 16;
    double safe_inset = 0; // title-safe pad applied on each side
    double title_fs = 24;
    double subtitle_fs = 12;
    double value_fs = 20;
    double label_fs = 10;
    double log_fs = 12;
    double meta_fs = 12;
    double btn_fs = 16;
    double pill_fs = 13;
    double card_min_h = 64;
    double card_pad_y = 7;
    double card_pad_x = 12;
    double card_gap = 8;
    double spark_h = 32;
    double spark_card_h = 52;
    double bar_h_headers = 6;
    double bar_h_verify = 9;
    double log_min_h = 112;
    double btn_min_h = 44;
    double btn_min_w = 136;
    double section_gap = 6;
    int primary_columns = 4; // 4 or 2
};

// Progressive disclosure plan from height budget (never silent-clip KPIs).
struct LayoutPlan {
    bool show_secondary = true; // Headers, Disk, Mempool, Uptime
    bool show_spark = true;
    bool show_subtitle = true;
    bool show_section_labels = false;
    bool meta_wrap = false;
    double log_min_h = 112;
};

// Title-safe: ~5% each edge (Xbox TV overscan guidance).
inline constexpr double kUiSafeFraction = 0.05;

// Heuristic scale when RawPixelsPerViewPixel is missing/low (960×540@2× etc.).
inline double EffectiveViewScale(double width, double height, double raw_scale) {
    double scale = raw_scale;
    if (!(scale >= 0.5) || !std::isfinite(scale)) {
        scale = 1.0;
    }
    if (scale < 1.25) {
        const double ar = (height > 1.0) ? (width / height) : 0.0;
        if (ar > 1.5 && ar < 1.9) {
            if (height >= 500.0 && height <= 560.0) {
                scale = 2.0; // 960×540 → 1080p
            } else if (height >= 700.0 && height <= 740.0) {
                scale = 1.5; // 1280×720 → 1080p
            }
        }
    }
    return scale;
}

// Usable size in *effective* DIPs (scale-corrected) after title-safe inset.
inline void ComputeUsableSize(double page_w, double page_h, double raw_scale, double& out_w,
                              double& out_h, double& out_inset) {
    const double w = page_w > 1 ? page_w : 1920;
    const double h = page_h > 1 ? page_h : 1080;
    const double scale = EffectiveViewScale(w, h, raw_scale);
    const double eff_w = w * scale;
    const double eff_h = h * scale;
    const double inset = (std::max)(16.0, kUiSafeFraction * (std::min)(w, h));
    out_inset = inset;
    out_w = (std::max)(320.0, eff_w - 2.0 * inset * scale);
    out_h = (std::max)(240.0, eff_h - 2.0 * inset * scale);
}

// Density tokens from already-computed usable size (effective DIPs).
inline UiLayout MakeLayout(double usable_w, double usable_h, double viewport_w, double viewport_h,
                           double inset) {
    UiLayout L;
    L.usable_w = usable_w;
    L.usable_h = usable_h;
    L.viewport_w = viewport_w > 1 ? viewport_w : usable_w;
    L.viewport_h = viewport_h > 1 ? viewport_h : usable_h;
    L.safe_inset = inset;
    L.pad_x = inset + 8;
    L.pad_y = inset * 0.65 + 6;

    if (L.usable_h < 820 || L.usable_w < 1000) {
        L.density = UiDensity::Compact;
    } else if (L.usable_h >= 1100 && L.usable_w >= 1500) {
        L.density = UiDensity::Comfort;
    } else {
        L.density = UiDensity::Standard;
    }

    L.primary_columns = (L.usable_w < 1100) ? 2 : 4;

    switch (L.density) {
    case UiDensity::Compact:
        L.title_fs = 20;
        L.subtitle_fs = 11;
        L.value_fs = 18;
        L.label_fs = 10;
        L.log_fs = 11;
        L.meta_fs = 11;
        L.btn_fs = 15;
        L.pill_fs = 12;
        L.card_min_h = 54;
        L.card_pad_y = 5;
        L.card_pad_x = 10;
        L.card_gap = 6;
        L.spark_h = 26;
        L.spark_card_h = 42;
        L.bar_h_headers = 5;
        L.bar_h_verify = 7;
        L.log_min_h = 88;
        L.btn_min_h = 40;
        L.btn_min_w = 120;
        L.section_gap = 4;
        break;
    case UiDensity::Comfort:
        L.title_fs = 28;
        L.subtitle_fs = 13;
        L.value_fs = 24;
        L.label_fs = 11;
        L.log_fs = 13;
        L.meta_fs = 13;
        L.btn_fs = 17;
        L.pill_fs = 14;
        L.card_min_h = 76;
        L.card_pad_y = 10;
        L.card_pad_x = 14;
        L.card_gap = 10;
        L.spark_h = 40;
        L.spark_card_h = 64;
        L.bar_h_headers = 7;
        L.bar_h_verify = 10;
        L.log_min_h = 160;
        L.btn_min_h = 48;
        L.btn_min_w = 148;
        L.section_gap = 8;
        break;
    case UiDensity::Standard:
    default:
        L.title_fs = 22;
        L.subtitle_fs = 12;
        L.value_fs = 20;
        L.label_fs = 10;
        L.log_fs = 12;
        L.meta_fs = 12;
        L.btn_fs = 16;
        L.pill_fs = 13;
        L.card_min_h = 60;
        L.card_pad_y = 6;
        L.card_pad_x = 12;
        L.card_gap = 8;
        L.spark_h = 30;
        L.spark_card_h = 48;
        L.bar_h_headers = 6;
        L.bar_h_verify = 8;
        L.log_min_h = 100;
        L.btn_min_h = 44;
        L.btn_min_w = 132;
        L.section_gap = 6;
        break;
    }
    return L;
}

// Convenience: page size + raw scale → full layout tokens.
inline UiLayout DiscoverLayoutFromViewport(double width, double height, double raw_scale) {
    double uw = 0, uh = 0, inset = 0;
    ComputeUsableSize(width, height, raw_scale, uw, uh, inset);
    return MakeLayout(uw, uh, width > 1 ? width : uw, height > 1 ? height : uh, inset);
}

inline LayoutPlan PlanSections(double usable_h, UiLayout const& L) {
    LayoutPlan plan;
    plan.show_subtitle = (L.density != UiDensity::Compact);
    plan.show_section_labels = (L.density == UiDensity::Comfort);
    plan.meta_wrap = (L.density == UiDensity::Comfort);
    plan.log_min_h = L.log_min_h;

    const double header_h = L.title_fs + (plan.show_subtitle ? L.subtitle_fs + 8 : 4) + 12;
    const int prow = (L.primary_columns >= 4) ? 1 : 2;
    const double primary_h = prow * L.card_min_h + (prow - 1) * L.card_gap + L.section_gap;
    const double secondary_h = L.card_min_h + L.section_gap;
    const double sync_bars = 18 + L.bar_h_headers + L.bar_h_verify + 16 + L.meta_fs + 12;
    const double spark_h = L.spark_card_h + 6;
    const double actions_h = L.btn_min_h + L.section_gap * 2;
    const double log_floor = L.log_min_h;

    const double base = header_h + primary_h + sync_bars + actions_h + log_floor;

    plan.show_secondary = true;
    plan.show_spark = true;

    if (base + secondary_h + spark_h > usable_h) {
        plan.show_spark = false;
    }
    if (base + secondary_h + (plan.show_spark ? spark_h : 0) > usable_h) {
        plan.show_secondary = false;
    }
    double used = header_h + primary_h + sync_bars + actions_h +
                  (plan.show_secondary ? secondary_h : 0) + (plan.show_spark ? spark_h : 0);
    if (used + plan.log_min_h > usable_h) {
        plan.log_min_h = (std::max)(72.0, usable_h - used);
    }
    if (L.density == UiDensity::Compact && usable_h < 900) {
        plan.show_secondary = (base + secondary_h + 80 <= usable_h);
        plan.show_spark = false;
    }
    return plan;
}

// Session ETA from progress samples (interval_sec between samples). Hours remaining or <0 if unknown.
inline double EstimateEtaHours(double verify_now, double verify_old, size_t samples, double interval_sec) {
    if (samples < 2 || interval_sec <= 0 || !std::isfinite(verify_now) || !std::isfinite(verify_old)) {
        return -1.0;
    }
    if (verify_now >= 0.999) {
        return 0.0;
    }
    const double dp = verify_now - verify_old;
    const double hours = (static_cast<double>(samples - 1) * interval_sec) / 3600.0;
    if (dp <= 1e-9 || hours <= 1e-9) {
        return -1.0;
    }
    return (1.0 - verify_now) / (dp / hours);
}

} // namespace xbb
