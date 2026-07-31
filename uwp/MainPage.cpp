// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "MainPage.h"
#include "log.h"
#include "node_host.h"
#include "probes.h"
#include "rpc_client.h"

#include <winrt/Windows.System.Threading.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;

namespace xbb {
namespace {

Color C(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return ColorHelper::FromArgb(a, r, g, b);
}

const Color kBg = C(14, 17, 22);
const Color kCard = C(24, 28, 36);
const Color kOrange = C(247, 147, 26);
const Color kWhite = C(255, 255, 255);
const Color kMuted = C(160, 168, 180);
const Color kGreen = C(46, 160, 67);
const Color kYellow = C(218, 165, 32);
const Color kRed = C(200, 60, 60);
const Color kGray = C(90, 96, 108);
const Color kPurple = C(88, 60, 120);

std::wstring Utf8ToWide(std::string const& text) {
    if (text.empty()) {
        return {};
    }
    int need = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(need > 0 ? need - 1 : 0), L'\0');
    if (need > 1) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, w.data(), need);
    }
    return w;
}

std::wstring FormatInt(int v) {
    std::wstring s = std::to_wstring(v);
    // thousands separators
    int insert = 0;
    for (int i = static_cast<int>(s.size()) - 1; i > 0; --i) {
        ++insert;
        if (insert % 3 == 0) {
            s.insert(static_cast<size_t>(i), 1, L',');
        }
    }
    return s;
}

std::wstring FormatPct(double p) {
    std::wostringstream os;
    // More digits while IBD; coarser when synced.
    const int prec = (p < 0.999) ? 3 : 1;
    os << std::fixed << std::setprecision(prec) << (p * 100.0) << L"%";
    return os.str();
}

std::wstring FormatBytes(int64_t n) {
    if (n <= 0) {
        return L"—";
    }
    const wchar_t* units[] = {L"B", L"KiB", L"MiB", L"GiB", L"TiB"};
    double v = static_cast<double>(n);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    std::wostringstream os;
    os << std::fixed << std::setprecision(u == 0 ? 0 : 1) << v << L" " << units[u];
    return os.str();
}

std::wstring FormatUptime(int64_t sec) {
    if (sec <= 0) {
        return L"—";
    }
    const int64_t d = sec / 86400;
    const int64_t h = (sec % 86400) / 3600;
    const int64_t m = (sec % 3600) / 60;
    std::wostringstream os;
    if (d > 0) {
        os << d << L"d " << h << L"h";
    } else if (h > 0) {
        os << h << L"h " << m << L"m";
    } else {
        os << m << L"m";
    }
    return os.str();
}

Border MakeCard(hstring const& label, TextBlock& value_out) {
    auto border = Border{};
    border.Background(SolidColorBrush{kCard});
    border.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    border.Padding(ThicknessHelper::FromUniformLength(16));
    border.Margin(ThicknessHelper::FromLengths(0, 0, 12, 0));
    border.MinWidth(160);
    border.MinHeight(96);

    auto stack = StackPanel{};
    stack.Spacing(6);

    auto lab = TextBlock{};
    lab.Text(label);
    lab.FontSize(14);
    lab.Foreground(SolidColorBrush{kMuted});

    value_out = TextBlock{};
    value_out.Text(L"—");
    value_out.FontSize(28);
    value_out.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    value_out.Foreground(SolidColorBrush{kWhite});

    stack.Children().Append(lab);
    stack.Children().Append(value_out);
    border.Child(stack);
    return border;
}

Button MakeButton(hstring const& label) {
    auto btn = Button{};
    btn.Content(box_value(label));
    btn.MinHeight(52);
    btn.MinWidth(140);
    btn.Padding(ThicknessHelper::FromLengths(20, 10, 20, 10));
    btn.FontSize(18);
    btn.Margin(ThicknessHelper::FromLengths(0, 0, 12, 0));
    btn.Background(SolidColorBrush{kCard});
    btn.Foreground(SolidColorBrush{kWhite});
    return btn;
}

} // namespace

MainPageController::MainPageController() = default;

void MainPageController::Init() {
    BuildUI();
    WireButtons();
    ApplyStatus(NodeStatusSnapshot(), {}, "Running probes…");
    StartUiTimer();
}

void MainPageController::BuildUI() {
    m_root = Page{};
    auto root_grid = Grid{};
    root_grid.Background(SolidColorBrush{kBg});
    root_grid.Padding(ThicknessHelper::FromUniformLength(28));

    // rows: header | metrics1 | metrics2 | progress | actions | log
    root_grid.RowDefinitions().Append(RowDefinition{});
    root_grid.RowDefinitions().Append(RowDefinition{});
    root_grid.RowDefinitions().Append(RowDefinition{});
    root_grid.RowDefinitions().Append(RowDefinition{});
    root_grid.RowDefinitions().Append(RowDefinition{});
    auto log_row = RowDefinition{};
    log_row.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    root_grid.RowDefinitions().Append(log_row);

    // --- Header ---
    auto header = Grid{};
    auto col_title = ColumnDefinition{};
    col_title.Width(GridLengthHelper::Auto());
    header.ColumnDefinitions().Append(col_title);
    auto mid = ColumnDefinition{};
    mid.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    header.ColumnDefinitions().Append(mid);
    auto col_net = ColumnDefinition{};
    col_net.Width(GridLengthHelper::Auto());
    header.ColumnDefinitions().Append(col_net);
    header.Margin(ThicknessHelper::FromLengths(0, 0, 0, 16));

    m_title = TextBlock{};
    m_title.Text(L"xbox_bitcoind");
    m_title.FontSize(34);
    m_title.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    m_title.Foreground(SolidColorBrush{kOrange});
    m_title.VerticalAlignment(VerticalAlignment::Center);
    header.Children().Append(m_title);

    m_pill = Border{};
    m_pill.CornerRadius(CornerRadiusHelper::FromUniformRadius(16));
    m_pill.Padding(ThicknessHelper::FromLengths(16, 8, 16, 8));
    m_pill.VerticalAlignment(VerticalAlignment::Center);
    m_pill.HorizontalAlignment(HorizontalAlignment::Center);
    m_pill_text = TextBlock{};
    m_pill_text.FontSize(16);
    m_pill_text.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    m_pill_text.Foreground(SolidColorBrush{kWhite});
    m_pill.Child(m_pill_text);
    Grid::SetColumn(m_pill, 1);
    header.Children().Append(m_pill);

    m_network_label = TextBlock{};
    m_network_label.Text(L"mainnet · prune");
    m_network_label.FontSize(16);
    m_network_label.Foreground(SolidColorBrush{kMuted});
    m_network_label.VerticalAlignment(VerticalAlignment::Center);
    m_network_label.HorizontalAlignment(HorizontalAlignment::Right);
    Grid::SetColumn(m_network_label, 2);
    header.Children().Append(m_network_label);

    Grid::SetRow(header, 0);
    root_grid.Children().Append(header);

    // --- Metric cards row 1 (chain) ---
    auto metrics = StackPanel{};
    metrics.Orientation(Orientation::Horizontal);
    metrics.Margin(ThicknessHelper::FromLengths(0, 0, 0, 10));
    metrics.Children().Append(MakeCard(L"Height", m_val_height));
    metrics.Children().Append(MakeCard(L"Headers", m_val_headers));
    metrics.Children().Append(MakeCard(L"Progress", m_val_progress));
    auto peers_card = MakeCard(L"Peers", m_val_peers);
    peers_card.Margin(ThicknessHelper::FromUniformLength(0));
    metrics.Children().Append(peers_card);
    Grid::SetRow(metrics, 1);
    root_grid.Children().Append(metrics);

    // --- Metric cards row 2 (node health — standard bitcoind view) ---
    auto metrics2 = StackPanel{};
    metrics2.Orientation(Orientation::Horizontal);
    metrics2.Margin(ThicknessHelper::FromLengths(0, 0, 0, 12));
    metrics2.Children().Append(MakeCard(L"Behind", m_val_behind));
    metrics2.Children().Append(MakeCard(L"Disk", m_val_disk));
    metrics2.Children().Append(MakeCard(L"Mempool", m_val_mempool));
    auto up_card = MakeCard(L"Uptime", m_val_uptime);
    up_card.Margin(ThicknessHelper::FromUniformLength(0));
    metrics2.Children().Append(up_card);
    Grid::SetRow(metrics2, 2);
    root_grid.Children().Append(metrics2);

    // --- Progress + meta ---
    auto prog_panel = StackPanel{};
    prog_panel.Spacing(8);
    prog_panel.Margin(ThicknessHelper::FromLengths(0, 0, 0, 16));
    m_progress_bar = ProgressBar{};
    m_progress_bar.Minimum(0);
    m_progress_bar.Maximum(1);
    m_progress_bar.Value(0);
    m_progress_bar.Height(12);
    m_progress_bar.Foreground(SolidColorBrush{kOrange});
    m_progress_bar.Background(SolidColorBrush{kCard});
    m_meta = TextBlock{};
    m_meta.Text(L"Datadir —");
    m_meta.FontSize(14);
    m_meta.Foreground(SolidColorBrush{kMuted});
    m_meta.TextWrapping(TextWrapping::Wrap);
    prog_panel.Children().Append(m_progress_bar);
    prog_panel.Children().Append(m_meta);
    Grid::SetRow(prog_panel, 3);
    root_grid.Children().Append(prog_panel);

    // --- Actions ---
    auto actions = StackPanel{};
    actions.Orientation(Orientation::Horizontal);
    actions.Margin(ThicknessHelper::FromLengths(0, 0, 0, 16));
    m_btn_start = MakeButton(L"Start");
    m_btn_stop = MakeButton(L"Stop soft");
    m_btn_refresh = MakeButton(L"Refresh");
    actions.Children().Append(m_btn_start);
    actions.Children().Append(m_btn_stop);
    actions.Children().Append(m_btn_refresh);
    Grid::SetRow(actions, 4);
    root_grid.Children().Append(actions);

    // --- Log ---
    auto log_border = Border{};
    log_border.Background(SolidColorBrush{kCard});
    log_border.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    log_border.Padding(ThicknessHelper::FromUniformLength(12));

    auto log_stack = StackPanel{};
    auto log_title = TextBlock{};
    log_title.Text(L"debug.log");
    log_title.FontSize(14);
    log_title.Foreground(SolidColorBrush{kMuted});
    log_title.Margin(ThicknessHelper::FromLengths(0, 0, 0, 8));

    m_log_scroll = ScrollViewer{};
    m_log_scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    m_log_scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    m_log = TextBlock{};
    m_log.TextWrapping(TextWrapping::Wrap);
    m_log.FontSize(13);
    m_log.FontFamily(FontFamily{L"Consolas"});
    m_log.Foreground(SolidColorBrush{C(200, 205, 215)});
    m_log.Text(L"…");
    m_log_scroll.Content(m_log);
    // Fill remaining space via parent Grid star row + border stretch
    log_stack.Children().Append(log_title);
    log_stack.Children().Append(m_log_scroll);
    log_border.Child(log_stack);
    Grid::SetRow(log_border, 5);
    root_grid.Children().Append(log_border);

    m_root.Content(root_grid);

    SetPill(L"INIT", kGray);
}

void MainPageController::WireButtons() {
    auto self = shared_from_this();
    m_btn_start.Click([self](IInspectable const&, RoutedEventArgs const&) { self->OnStartClick(); });
    m_btn_stop.Click([self](IInspectable const&, RoutedEventArgs const&) { self->OnStopClick(); });
    m_btn_refresh.Click([self](IInspectable const&, RoutedEventArgs const&) { self->OnRefreshClick(); });
}

void MainPageController::StartUiTimer() {
    m_timer = DispatcherTimer{};
    m_timer.Interval(std::chrono::milliseconds(2000));
    auto self = shared_from_this();
    m_timer.Tick([self](IInspectable const&, IInspectable const&) { self->RefreshAsync(); });
    m_timer.Start();
}

void MainPageController::SetPill(std::wstring const& text, Color bg) {
    if (!m_pill_text || !m_pill) {
        return;
    }
    m_pill_text.Text(text);
    m_pill.Background(SolidColorBrush{bg});
}

void MainPageController::SetMetric(TextBlock const& value, std::wstring const& text) {
    if (value) {
        value.Text(text);
    }
}

void MainPageController::ApplyStatus(NodeStatus const& st, std::string const& log_tail,
                                     std::string const& probe_note) {
    auto clear_metrics = [&]() {
        SetMetric(m_val_height, L"—");
        SetMetric(m_val_headers, L"—");
        SetMetric(m_val_progress, L"—");
        SetMetric(m_val_peers, L"—");
        SetMetric(m_val_behind, L"—");
        SetMetric(m_val_disk, L"—");
        SetMetric(m_val_mempool, L"—");
        SetMetric(m_val_uptime, L"—");
        m_progress_bar.Value(0);
    };

    if (!st.available) {
        SetPill(L"NO CORE", kPurple);
        clear_metrics();
        m_meta.Text(Utf8ToWide("Scaffold build — Core not linked. " + st.datadir));
        m_network_label.Text(L"scaffold");
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(false);
        m_btn_refresh.IsEnabled(true);
        if (!probe_note.empty()) {
            m_log.Text(Utf8ToWide(probe_note));
        }
        return;
    }

    if (m_stopping) {
        SetPill(L"STOPPING", kYellow);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(false);
    } else if (!st.running) {
        if (st.last_exit != 0) {
            SetPill(L"ERROR", kRed);
        } else {
            SetPill(L"STOPPED", kGray);
        }
        m_btn_start.IsEnabled(true);
        m_btn_stop.IsEnabled(false);
    } else if (!st.rpc_ready) {
        SetPill(L"STARTING", kYellow);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
    } else if (!st.network_active) {
        SetPill(L"NET OFF", kRed);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
    } else if (st.initial_block_download || st.verification_progress < 0.999) {
        SetPill(L"SYNCING", kOrange);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
    } else {
        SetPill(L"SYNCED", kGreen);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
    }

    if (st.rpc_ready) {
        const int behind = (st.headers > st.blocks) ? (st.headers - st.blocks) : 0;
        SetMetric(m_val_height, FormatInt(st.blocks));
        SetMetric(m_val_headers, FormatInt(st.headers));
        SetMetric(m_val_progress, FormatPct(st.verification_progress));
        SetMetric(m_val_peers, FormatInt(st.connections));
        SetMetric(m_val_behind, behind > 0 ? FormatInt(behind) : L"0");
        SetMetric(m_val_disk, FormatBytes(st.size_on_disk));
        if (st.mempool_tx > 0 || st.mempool_bytes > 0) {
            SetMetric(m_val_mempool, FormatInt(st.mempool_tx) + L" tx");
        } else {
            SetMetric(m_val_mempool, L"0 tx");
        }
        SetMetric(m_val_uptime, FormatUptime(st.uptime_sec));
        m_progress_bar.Value(std::clamp(st.verification_progress, 0.0, 1.0));

        std::wstring net = Utf8ToWide(st.chain.empty() ? "main" : st.chain);
        if (st.pruned) {
            net += L" · prune";
        }
        if (st.initial_block_download) {
            net += L" · IBD";
        }
        m_network_label.Text(net);
    } else if (st.running) {
        SetMetric(m_val_height, L"…");
        SetMetric(m_val_headers, L"…");
        SetMetric(m_val_progress, L"…");
        SetMetric(m_val_peers, L"…");
        SetMetric(m_val_behind, L"…");
        SetMetric(m_val_disk, L"…");
        SetMetric(m_val_mempool, L"…");
        SetMetric(m_val_uptime, L"…");
        m_progress_bar.Value(0);
    } else {
        clear_metrics();
    }

    std::ostringstream meta;
    meta << st.datadir;
    if (!st.subversion.empty()) {
        meta << "  ·  " << st.subversion;
    }
    if (!st.message.empty()) {
        meta << "  ·  " << st.message;
    }
    if (st.pruned && st.prune_target_size > 0) {
        meta << "  ·  prune target ~" << (st.prune_target_size / (1024 * 1024)) << " MiB";
    }
    if (!st.warnings.empty()) {
        meta << "  ·  warn: " << st.warnings;
    }
    if (st.last_exit != 0 && !st.running) {
        meta << "  ·  last exit " << st.last_exit;
    }
    if (!probe_note.empty()) {
        meta << "  ·  " << probe_note;
    }
    m_meta.Text(Utf8ToWide(meta.str()));

    if (!log_tail.empty()) {
        m_log.Text(Utf8ToWide(log_tail));
        m_log_scroll.ChangeView(nullptr, m_log_scroll.ScrollableHeight(), nullptr);
    }
}

void MainPageController::RefreshAsync() {
    if (m_refreshing) {
        return;
    }
    m_refreshing = true;
    auto self = shared_from_this();
    auto dispatcher = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher();

    winrt::Windows::System::Threading::ThreadPool::RunAsync([self, dispatcher](auto&&) {
        NodeStatus st = NodeStatusLive();
        std::string log;
        if (st.available) {
            log = ReadDebugLogTail(st.datadir, 40);
        }
        auto probe = self->m_probe_note;
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                            [self, st, log, probe]() {
                                self->ApplyStatus(st, log, probe);
                                self->m_refreshing = false;
                            });
    });
}

void MainPageController::OnStartClick() {
    Logf("[ui] Start clicked");
    if (!NodeCoreLinked()) {
        return;
    }
    if (NodeStart()) {
        ApplyStatus(NodeStatusSnapshot(), {}, m_probe_note);
        RefreshAsync();
    }
}

void MainPageController::OnStopClick() {
    Logf("[ui] Stop soft clicked");
    auto self = shared_from_this();
    auto dispatcher = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher();
    m_stopping = true;
    m_btn_stop.IsEnabled(false);
    m_btn_start.IsEnabled(false);
    SetPill(L"STOPPING", kYellow);
    winrt::Windows::System::Threading::ThreadPool::RunAsync([self, dispatcher](auto&&) {
        NodeStop();
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [self]() {
            self->m_stopping = false;
            self->ApplyStatus(NodeStatusSnapshot(), {}, self->m_probe_note);
            self->RefreshAsync();
        });
    });
}

void MainPageController::OnRefreshClick() {
    RefreshAsync();
}

void MainPageController::StartProbesAsync() {
    auto self = shared_from_this();
    auto dispatcher = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher();

    winrt::Windows::System::Threading::ThreadPool::RunAsync([self, dispatcher](auto&&) {
        auto results = RunProbes();
        auto report = FormatProbeReport(results);
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                            [self, report]() {
                                self->m_probe_note = "probes OK";
                                self->ApplyStatus(NodeStatusSnapshot(), report, self->m_probe_note);
                                if (NodeCoreLinked()) {
                                    Logf("[ui] auto-start after probes");
                                    NodeStart();
                                    self->ApplyStatus(NodeStatusSnapshot(), report, self->m_probe_note);
                                }
                                self->RefreshAsync();
                            });
    });
}

} // namespace xbb
