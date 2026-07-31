// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include "pch.h"

#include "node_host.h"

#include <memory>
#include <string>

namespace xbb {

// Programmatic XAML dashboard (no WinRT runtimeclass — avoids MarkupCompilePass2).
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
    void WireButtons();
    void StartUiTimer();
    void RefreshAsync();
    void ApplyStatus(NodeStatus const& st, std::string const& log_tail, std::string const& probe_note);
    void SetPill(std::wstring const& text, winrt::Windows::UI::Color bg);
    void SetMetric(winrt::Windows::UI::Xaml::Controls::TextBlock const& value, std::wstring const& text);
    void OnStartClick();
    void OnStopClick();
    void OnRefreshClick();

    winrt::Windows::UI::Xaml::Controls::Page m_root{nullptr};

    winrt::Windows::UI::Xaml::Controls::TextBlock m_title{nullptr};
    winrt::Windows::UI::Xaml::Controls::Border m_pill{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_pill_text{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_network_label{nullptr};

    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_height{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_headers{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_progress{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_val_peers{nullptr};

    winrt::Windows::UI::Xaml::Controls::ProgressBar m_progress_bar{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_meta{nullptr};

    winrt::Windows::UI::Xaml::Controls::Button m_btn_start{nullptr};
    winrt::Windows::UI::Xaml::Controls::Button m_btn_stop{nullptr};
    winrt::Windows::UI::Xaml::Controls::Button m_btn_refresh{nullptr};

    winrt::Windows::UI::Xaml::Controls::TextBlock m_log{nullptr};
    winrt::Windows::UI::Xaml::Controls::ScrollViewer m_log_scroll{nullptr};

    winrt::Windows::UI::Xaml::DispatcherTimer m_timer{nullptr};
    std::string m_probe_note;
    bool m_refreshing = false;
};

} // namespace xbb
