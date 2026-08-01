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
    // Xbox Home / title switch: OS suspends UWP (Game class does not prevent this).
    // Soft-stop on suspend for LevelDB durability; restart on resume if we had been running.
    m_suspending_token = this->Suspending({this, &App::OnSuspending});
    m_resuming_token = this->Resuming({this, &App::OnResuming});
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
    // Prefer clean stop over freezing mid-write (LevelDB). Game OS still suspends the process
    // after this — IBD does not continue while the title is not in focus. The OS deferral
    // budget is much shorter than a worst-case mid-IBD flush; if it runs out the process is
    // suspended anyway and the host DELETE + relaunch path recovers.
    const bool was_running = xbb::NodeStatusSnapshot().running;
    m_restart_node_on_resume = was_running && xbb::NodeCoreLinked();
    xbb::Logf("[app] OnSuspending — soft-stop node (was_running=%d restart_on_resume=%d)",
              was_running ? 1 : 0, m_restart_node_on_resume ? 1 : 0);
    auto deferral = e.SuspendingOperation().GetDeferral();
    auto stopped = std::make_shared<std::promise<void>>();
    m_pending_stop = stopped->get_future().share();
    std::thread([deferral, stopped]() {
        try {
            xbb::NodeStop();
        } catch (...) {
            xbb::Logf("[app] OnSuspending NodeStop threw");
        }
        stopped->set_value();
        deferral.Complete();
        xbb::Logf("[app] OnSuspending complete");
    }).detach();
}

void App::OnResuming(IInspectable const&, IInspectable const&) {
    xbb::Logf("[app] OnResuming restart_node=%d", m_restart_node_on_resume ? 1 : 0);
    if (!m_restart_node_on_resume || !xbb::NodeCoreLinked()) {
        return;
    }
    m_restart_node_on_resume = false;
    // Resume on a worker thread so UI can paint. Wait out any in-flight suspend stop first,
    // otherwise the restart would target the instance being stopped (fast Home in/out).
    auto pending_stop = m_pending_stop;
    std::thread([pending_stop]() {
        try {
            if (pending_stop.valid()) {
                pending_stop.wait();
            }
            if (xbb::NodeStart()) {
                xbb::Logf("[app] OnResuming: NodeStart OK (continue IBD)");
            } else {
                xbb::Logf("[app] OnResuming: NodeStart failed");
            }
        } catch (...) {
            xbb::Logf("[app] OnResuming: NodeStart threw");
        }
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
