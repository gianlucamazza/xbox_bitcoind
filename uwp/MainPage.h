// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include "pch.h"

#include <memory>
#include <string>

namespace xbb {

// Programmatic XAML UI (not a WinRT runtimeclass — avoids MarkupCompilePass2).
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
    void SetStatus(std::string const& text);
    void AppendLine(std::string const& line);

    winrt::Windows::UI::Xaml::Controls::Page m_root{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_title{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_body{nullptr};
    winrt::Windows::UI::Xaml::Controls::ScrollViewer m_scroll{nullptr};
    std::string m_body_text;
};

} // namespace xbb
