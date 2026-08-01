// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include "pch.h"

#include "node_host.h"
#include "ui_layout.h"

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace xbb {

// Usable DIPs: VisibleBounds minus title-safe inset (~5%). WinRT-backed.
void GetUsableSize(double page_w, double page_h, double& out_w, double& out_h, double& out_inset);
// Layout tokens from page size (uses GetUsableSize).
UiLayout DiscoverLayout(double width, double height);
// PlanSections is pure — defined in ui_layout.h.

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
                                                             winrt::Windows::UI::Xaml::Controls::TextBlock& value_out,
                                                             winrt::Windows::UI::Xaml::Controls::TextBlock& label_out);

    void WireButtons();
    void WireGamepadFocus();
    void StartUiTimer();
    void OnRootSizeChanged(double width, double height);
    void ApplyLayout(UiLayout const& L, LayoutPlan const& plan);
    void LayoutMetricRow(winrt::Windows::UI::Xaml::Controls::Grid const& grid,
                         std::vector<winrt::Windows::UI::Xaml::Controls::Border> const& cards, int columns);
    void RefreshAsync();
    std::string ProbeNote() const;
    void SetProbeNote(std::string note);
    void ApplyStatus(NodeStatus const& st, std::string const& log_tail, std::string const& probe_note);
    void SetPill(std::wstring const& text, winrt::Windows::UI::Color bg);
    void SetMetric(winrt::Windows::UI::Xaml::Controls::TextBlock const& value, std::wstring const& text,
                   winrt::Windows::UI::Color color);
    void StylePrimaryButton(winrt::Windows::UI::Xaml::Controls::Button const& btn, bool primary);
    void PushHistory(double verification);
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
    // Written on ThreadPool (StartProbesAsync), read on UI + worker threads — use accessors.
    mutable std::mutex m_probe_mu;
    std::string m_probe_note;
    std::wstring m_last_log;
    std::deque<double> m_hist_progress;
    // Cached secondary values for meta fold when secondary grid hidden
    int m_cache_headers = 0;
    int64_t m_cache_disk = 0;
    int m_cache_mempool = 0;
    int64_t m_cache_uptime = 0;
    // Set on UI thread, cleared on UI or worker thread (RefreshAsync failure path).
    std::atomic<bool> m_refreshing{false};
    bool m_stopping = false;
    // Last running state seen by ApplyStatus (UI thread) — resets ETA history on transitions.
    bool m_last_running = false;
    std::chrono::steady_clock::time_point m_stop_started{};
};

} // namespace xbb
