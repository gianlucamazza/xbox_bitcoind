// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "App.h"
#include "MainPage.h"
#include "log.h"
#include "node_host.h"

#include <thread>

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;

namespace winrt::xbox_bitcoind::implementation {

App::App() {
    xbb::LogInit();
    xbb::Logf("[app] App ctor");
    // Clean bitcoind shutdown on suspend/terminate so LevelDB/block index flush to disk.
    m_suspending_token = this->Suspending({this, &App::OnSuspending});
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

void App::OnSuspending(IInspectable const&, SuspendingEventArgs const& e) {
    xbb::Logf("[app] OnSuspending — stopping node for durable flush");
    auto deferral = e.SuspendingOperation().GetDeferral();
    // Run off UI thread; Complete when NodeStop finishes (RPC stop + join).
    std::thread([deferral]() {
        try {
            xbb::NodeStop();
        } catch (...) {
            xbb::Logf("[app] OnSuspending NodeStop threw");
        }
        deferral.Complete();
        xbb::Logf("[app] OnSuspending complete");
    }).detach();
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
