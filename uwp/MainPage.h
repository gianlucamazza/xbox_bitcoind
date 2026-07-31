// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include "pch.h"

#include "node_host.h"

#include <array>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace xbb {

// Density from usable viewport height (after safe inset).
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

// Pure helpers (testable without XAML).
UiLayout DiscoverLayout(double width, double height);
LayoutPlan PlanSections(double usable_h, UiLayout const& L);
// Usable DIPs: VisibleBounds minus title-safe inset (~5%).
void GetUsableSize(double page_w, double page_h, double& out_w, double& out_h, double& out_inset);

class MainPageController : public std::enable_shared_from_this<MainPageController> {
  public:
    MainPageController();
    void Init();
    void StartProbesAsync();

    winrt::Windows::UI::Xaml::Controls::Page Root() const {
        return m_root;
    }

  private:
    void BuildUI();
    winrt::Windows::UI::Xaml::FrameworkElement BuildHeader();
    winrt::Windows::UI::Xaml::FrameworkElement BuildPrimaryMetrics();
    winrt::Windows::UI::Xaml::FrameworkElement BuildSecondaryMetrics();
    winrt::Windows::UI::Xaml::FrameworkElement BuildProgressSection();
    winrt::Windows::UI::Xaml::FrameworkElement BuildActions();
    winrt::Windows::UI::Xaml::FrameworkElement BuildLogPanel();

    winrt::Windows::UI::Xaml::Controls::Border MakeMetricCard(wchar_t const* label,
                                                             winrt::Windows::UI::Xaml::Controls::TextBlock& value_out);

    void WireButtons();
    void WireGamepadFocus();
    void StartUiTimer();
    void OnRootSizeChanged(double width, double height);
    void ApplyLayout(UiLayout const& L, LayoutPlan const& plan);
    void LayoutMetricRow(winrt::Windows::UI::Xaml::Controls::Grid const& grid,
                         std::vector<winrt::Windows::UI::Xaml::Controls::Border> const& cards, int columns);
    void RefreshAsync();
    void ApplyStatus(NodeStatus const& st, std::string const& log_tail, std::string const& probe_note);
    void SetPill(std::wstring const& text, winrt::Windows::UI::Color bg);
    void SetMetric(winrt::Windows::UI::Xaml::Controls::TextBlock const& value, std::wstring const& text,
                   winrt::Windows::UI::Color color);
    void StylePrimaryButton(winrt::Windows::UI::Xaml::Controls::Button const& btn, bool primary);
    void PushHistory(double verification, int blocks);
    void RedrawSparkline();
    void OnStartClick();
    void OnStopClick();
    void OnRefreshClick();

    winrt::Windows::UI::Xaml::Controls::Page m_root{nullptr};
    winrt::Windows::UI::Xaml::Controls::Grid m_root_grid{nullptr};

    winrt::Windows::UI::Xaml::Controls::TextBlock m_title{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_subtitle{nullptr};
    winrt::Windows::UI::Xaml::Controls::Border m_pill{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_pill_text{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_network_label{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_updated{nullptr};
    winrt::Windows::UI::Xaml::Controls::StackPanel m_title_col{nullptr};

    // P1 primary KPIs (always visible when possible)
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_height{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_progress{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_peers{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_behind{nullptr};
    winrt::Windows::UI::Xaml::Controls::Grid m_primary_grid{nullptr};
    std::vector<winrt::Windows::UI::Xaml::Controls::Border> m_primary_cards;
    std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock> m_primary_labels;
    std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock> m_primary_values;

    // P3 secondary KPIs (budget-gated)
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_headers{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_disk{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_mempool{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_uptime{nullptr};
    winrt::Windows::UI::Xaml::Controls::Grid m_secondary_grid{nullptr};
    std::vector<winrt::Windows::UI::Xaml::Controls::Border> m_secondary_cards;
    std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock> m_secondary_labels;
    std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock> m_secondary_values;
    winrt::Windows::UI::Xaml::Controls::StackPanel m_metrics_block{nullptr};

    winrt::Windows::UI::Xaml::Controls::ProgressBar m_bar_headers{nullptr};
    winrt::Windows::UI::Xaml::Controls::ProgressBar m_bar_verify{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_progress_label{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_meta{nullptr};
    winrt::Windows::UI::Xaml::Controls::Border m_spark_border{nullptr};
    winrt::Windows::UI::Xaml::Controls::Canvas m_spark_canvas{nullptr};
    winrt::Windows::UI::Xaml::Shapes::Polyline m_spark_line{nullptr};
    winrt::Windows::UI::Xaml::Shapes::Polyline m_spark_fill{nullptr};
    winrt::Windows::UI::Xaml::Controls::StackPanel m_progress_panel{nullptr};

    winrt::Windows::UI::Xaml::Controls::Button m_btn_start{nullptr};
    winrt::Windows::UI::Xaml::Controls::Button m_btn_stop{nullptr};
    winrt::Windows::UI::Xaml::Controls::Button m_btn_refresh{nullptr};
    winrt::Windows::UI::Xaml::Controls::StackPanel m_actions{nullptr};

    winrt::Windows::UI::Xaml::Controls::Border m_log_border{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_log{nullptr};
    winrt::Windows::UI::Xaml::Controls::ScrollViewer m_log_scroll{nullptr};

    winrt::Windows::UI::Xaml::DispatcherTimer m_timer{nullptr};
    UiLayout m_layout{};
    LayoutPlan m_plan{};
    std::string m_probe_note;
    std::wstring m_last_log;
    std::deque<double> m_hist_progress;
    // Cached secondary values for meta fold when secondary grid hidden
    int m_cache_headers = 0;
    int64_t m_cache_disk = 0;
    int m_cache_mempool = 0;
    int64_t m_cache_uptime = 0;
    bool m_refreshing = false;
    bool m_stopping = false;
};

} // namespace xbb
