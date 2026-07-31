// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include "pch.h"

#include "node_host.h"

#include <array>
#include <deque>
#include <memory>
#include <string>

namespace xbb {

// Programmatic XAML dashboard (no WinRT runtimeclass — avoids MarkupCompilePass2).
// 10-foot UI for Xbox Series S with modern status visualization.
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
    winrt::Windows::UI::Xaml::UIElement BuildHeader();
    winrt::Windows::UI::Xaml::UIElement BuildMetricGrid(
        std::array<winrt::Windows::UI::Xaml::Controls::TextBlock*, 4> values,
        std::array<wchar_t const*, 4> labels);
    winrt::Windows::UI::Xaml::UIElement BuildProgressSection();
    winrt::Windows::UI::Xaml::UIElement BuildActions();
    winrt::Windows::UI::Xaml::UIElement BuildLogPanel();

    void WireButtons();
    void WireGamepadFocus();
    void StartUiTimer();
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

    winrt::Windows::UI::Xaml::Controls::TextBlock m_title{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_subtitle{nullptr};
    winrt::Windows::UI::Xaml::Controls::Border m_pill{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_pill_text{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_network_label{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_updated{nullptr};

    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_height{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_headers{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_progress{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_peers{nullptr};

    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_behind{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_disk{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_mempool{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_uptime{nullptr};

    // Dual bars: header catch-up vs block verification
    winrt::Windows::UI::Xaml::Controls::ProgressBar m_bar_headers{nullptr};
    winrt::Windows::UI::Xaml::Controls::ProgressBar m_bar_verify{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_progress_label{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_meta{nullptr};

    // Sparkline of recent verification progress (session memory)
    winrt::Windows::UI::Xaml::Controls::Canvas m_spark_canvas{nullptr};
    winrt::Windows::UI::Xaml::Shapes::Polyline m_spark_line{nullptr};
    winrt::Windows::UI::Xaml::Shapes::Polyline m_spark_fill{nullptr};

    winrt::Windows::UI::Xaml::Controls::Button m_btn_start{nullptr};
    winrt::Windows::UI::Xaml::Controls::Button m_btn_stop{nullptr};
    winrt::Windows::UI::Xaml::Controls::Button m_btn_refresh{nullptr};

    winrt::Windows::UI::Xaml::Controls::TextBlock m_log{nullptr};
    winrt::Windows::UI::Xaml::Controls::ScrollViewer m_log_scroll{nullptr};

    winrt::Windows::UI::Xaml::DispatcherTimer m_timer{nullptr};
    std::string m_probe_note;
    std::wstring m_last_log;
    std::deque<double> m_hist_progress; // 0..1, last N samples
    bool m_refreshing = false;
    bool m_stopping = false;
};

} // namespace xbb
