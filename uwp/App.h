// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include "App.g.h"
#include "pch.h"

#include <memory>

namespace xbb {
class MainPageController;
}

namespace winrt::xbox_bitcoind::implementation {

struct App : AppT<App> {
    App();
    void OnLaunched(winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);
    void OnSuspending(winrt::Windows::Foundation::IInspectable const&,
                      winrt::Windows::ApplicationModel::SuspendingEventArgs const&);

  private:
    std::shared_ptr<::xbb::MainPageController> m_controller;
    winrt::event_token m_suspending_token{};
};

} // namespace winrt::xbox_bitcoind::implementation

namespace winrt::xbox_bitcoind::factory_implementation {

struct App : AppT<App, implementation::App> {};

} // namespace winrt::xbox_bitcoind::factory_implementation
