// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "App.h"
#include "MainPage.h"
#include "log.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;

namespace winrt::xbox_bitcoind::implementation {

App::App() {
    xbb::LogInit();
    xbb::Logf("[app] App ctor");
}

void App::OnLaunched(LaunchActivatedEventArgs const&) {
    xbb::Logf("[app] OnLaunched");
    Window window = Window::Current();
    if (!window.Content()) {
        m_controller = std::make_shared<::xbb::MainPageController>();
        m_controller->Init();
        window.Content(m_controller->Root());
    }
    window.Activate();
    if (m_controller) {
        m_controller->StartProbesAsync();
    }
}

} // namespace winrt::xbox_bitcoind::implementation

// UWP entry: Appx EntryPoint="xbox_bitcoind.App" → factory creates App.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        winrt::Windows::UI::Xaml::Application::Start(
            [](auto&&) { winrt::make<::winrt::xbox_bitcoind::implementation::App>(); });
    } catch (winrt::hresult_error const& e) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[app] wWinMain hresult: 0x%08X\n",
                 static_cast<unsigned>(e.code().value));
        OutputDebugStringA(buf);
        xbb::Logf("%s", buf);
    } catch (std::exception const& e) {
        xbb::Logf("[app] wWinMain exception: %s", e.what());
    } catch (...) {
        xbb::Logf("[app] wWinMain: unknown exception");
    }
    return 0;
}
