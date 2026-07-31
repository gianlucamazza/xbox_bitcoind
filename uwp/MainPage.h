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

// Density discovered from actual page size (self-discovery). Series S is usually
// Standard (1080p content after overscan); Comfort is for taller viewports.
enum class UiDensity { Compact, Standard, Comfort };

// Token set produced by DiscoverLayout — all visual scale comes from here.
struct UiLayout {
    UiDensity density = UiDensity::Standard;
    double viewport_w = 1920;
    double viewport_h = 1080;
    double pad_x = 28;
    double pad_y = 18;
    double title_fs = 24;
    double subtitle_fs = 12;
    double value_fs = 20;
    double label_fs = 11;
    double log_fs = 12;
    double meta_fs = 12;
    double btn_fs = 16;
    double pill_fs = 14;
    double card_min_h = 68;
    double card_pad_y = 8;
    double card_pad_x = 12;
    double card_gap = 8;
    double spark_h = 36;
    double spark_card_h = 56;
    double bar_h_headers = 6;
    double bar_h_verify = 8;
    double log_min_h = 120;
    double btn_min_h = 44;
    double btn_min_w = 132;
    double header_margin_b = 8;
    double section_gap = 6;
    int metric_columns = 4; // 4 → two rows of four; 2 → four rows of two
    bool show_sparkline = true;
    bool show_section_labels = false;
    bool show_subtitle = true;
    bool meta_wrap = false;
};

// Discover layout from live viewport (DIPs). Pure function — easy to unit-test later.
UiLayout DiscoverLayout(double width, double height);

// Programmatic XAML dashboard (no WinRT runtimeclass — avoids MarkupCompilePass2).
// 10-foot UI for Xbox Series S with self-discovery + responsive tokens.
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
    winrt::Windows::UI::Xaml::FrameworkElement BuildMetricsBlock();
    winrt::Windows::UI::Xaml::FrameworkElement BuildProgressSection();
    winrt::Windows::UI::Xaml::FrameworkElement BuildActions();
    winrt::Windows::UI::Xaml::FrameworkElement BuildLogPanel();

    void WireButtons();
    void WireGamepadFocus();
    void StartUiTimer();
    void OnRootSizeChanged(double width, double height);
    void ApplyLayout(UiLayout const& L);
    void RelayoutMetricGrid(int columns);
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

    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_height{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_headers{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_progress{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_peers{nullptr};

    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_behind{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_disk{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_mempool{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_uptime{nullptr};

    winrt::Windows::UI::Xaml::Controls::Grid m_metrics_grid{nullptr};
    std::vector<winrt::Windows::UI::Xaml::Controls::Border> m_metric_cards;
    std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock> m_metric_labels;
    std::vector<winrt::Windows::UI::Xaml::Controls::TextBlock> m_metric_values;
    winrt::Windows::UI::Xaml::Controls::TextBlock m_chain_label{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_node_label{nullptr};
    winrt::Windows::UI::Xaml::Controls::StackPanel m_metrics_host{nullptr};

    winrt::Windows::UI::Xaml::Controls::ProgressBar m_bar_headers{nullptr};
    winrt::Windows::UI::Xaml::Controls::ProgressBar m_bar_verify{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_progress_label{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_meta{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_sync_section_label{nullptr};
    winrt::Windows::UI::Xaml::Controls::Border m_spark_border{nullptr};
    winrt::Windows::UI::Xaml::Controls::Canvas m_spark_canvas{nullptr};
    winrt::Windows::UI::Xaml::Shapes::Polyline m_spark_line{nullptr};
    winrt::Windows::UI::Xaml::Shapes::Polyline m_spark_fill{nullptr};

    winrt::Windows::UI::Xaml::Controls::Button m_btn_start{nullptr};
    winrt::Windows::UI::Xaml::Controls::Button m_btn_stop{nullptr};
    winrt::Windows::UI::Xaml::Controls::Button m_btn_refresh{nullptr};
    winrt::Windows::UI::Xaml::Controls::StackPanel m_actions{nullptr};

    winrt::Windows::UI::Xaml::Controls::Border m_log_border{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_log{nullptr};
    winrt::Windows::UI::Xaml::Controls::ScrollViewer m_log_scroll{nullptr};

    winrt::Windows::UI::Xaml::DispatcherTimer m_timer{nullptr};
    UiLayout m_layout{};
    std::string m_probe_note;
    std::wstring m_last_log;
    std::deque<double> m_hist_progress; // 0..1, last N samples
    bool m_refreshing = false;
    bool m_stopping = false;
};

} // namespace xbb
