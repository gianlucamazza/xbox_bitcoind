// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "MainPage.h"
#include "log.h"
#include "node_host.h"
#include "probes.h"

#include <winrt/Windows.System.Threading.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;

namespace xbb {

MainPageController::MainPageController() = default;

void MainPageController::Init() {
    BuildUI();
    auto st = NodeStatusSnapshot();
    std::string head = NodeCoreLinked() ? "xbox_bitcoind (Core linked)\n\n" : "xbox_bitcoind scaffold\n\n";
    SetStatus(head + st.message + "\n\nRunning probes…");
}

void MainPageController::BuildUI() {
    m_root = Page{};
    auto grid = Grid{};
    grid.Background(SolidColorBrush{Colors::Black()});
    // dark bg #0E1116
    grid.Background(SolidColorBrush{ColorHelper::FromArgb(255, 14, 17, 22)});

    auto panel = StackPanel{};
    panel.Margin(ThicknessHelper::FromUniformLength(32));
    panel.Spacing(12);

    m_title = TextBlock{};
    m_title.Text(L"xbox_bitcoind");
    m_title.FontSize(32);
    m_title.Foreground(SolidColorBrush{ColorHelper::FromArgb(255, 247, 147, 26)});
    m_title.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());

    m_scroll = ScrollViewer{};
    m_scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    m_scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);

    m_body = TextBlock{};
    m_body.TextWrapping(TextWrapping::Wrap);
    m_body.FontSize(16);
    m_body.FontFamily(FontFamily{L"Consolas"});
    m_body.Foreground(SolidColorBrush{Colors::White()});
    m_scroll.Content(m_body);

    panel.Children().Append(m_title);
    panel.Children().Append(m_scroll);
    m_root.Content(panel);
}

void MainPageController::SetStatus(std::string const& text) {
    m_body_text = text;
    if (!m_body) {
        return;
    }
    int need = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(need > 0 ? need - 1 : 0), L'\0');
    if (need > 1) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, w.data(), need);
    }
    m_body.Text(w);
}

void MainPageController::AppendLine(std::string const& line) {
    if (!m_body_text.empty() && m_body_text.back() != '\n') {
        m_body_text.push_back('\n');
    }
    m_body_text += line;
    SetStatus(m_body_text);
}

void MainPageController::StartProbesAsync() {
    auto self = shared_from_this();
    auto dispatcher = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher();

    winrt::Windows::System::Threading::ThreadPool::RunAsync(
        [self, dispatcher](auto&&) {
            auto results = RunProbes();
            auto report = FormatProbeReport(results);
            dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                                [self, report]() {
                                    auto st = NodeStatusSnapshot();
                                    std::string head =
                                        NodeCoreLinked() ? "xbox_bitcoind (Core linked)\n\n" : "xbox_bitcoind scaffold\n\n";
                                    self->SetStatus(head + st.message + "\n\n=== Probes ===\n\n" + report);
                                    // Auto-start node after probes when Core is linked
                                    if (NodeCoreLinked()) {
                                        self->AppendLine("\nStarting bitcoind…");
                                        if (NodeStart()) {
                                            self->AppendLine("Node thread started (see LocalState\\bitcoin\\debug.log).");
                                        } else {
                                            self->AppendLine("NodeStart failed.");
                                        }
                                    }
                                });
        });
}

} // namespace xbb
