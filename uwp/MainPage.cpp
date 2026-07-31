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
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Xaml::Shapes;

namespace xbb {
namespace {

Color C(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return ColorHelper::FromArgb(a, r, g, b);
}

// High-contrast dark theme (WCAG-friendly on TV).
const Color kBg = C(12, 14, 18);
const Color kCard = C(26, 30, 38);
const Color kCardBorder = C(48, 54, 66);
const Color kOrange = C(247, 147, 26);
const Color kOrangeDim = C(160, 90, 18);
const Color kCyan = C(56, 189, 248); // headers bar
const Color kWhite = C(245, 247, 250);
const Color kMuted = C(148, 156, 170);
const Color kGreen = C(52, 168, 83);
const Color kYellow = C(234, 179, 8);
const Color kRed = C(220, 68, 68);
const Color kGray = C(100, 106, 118);
const Color kPurple = C(130, 100, 180);
const Color kLogFg = C(186, 192, 204);
const Color kSparkFill = C(247, 147, 26, 40);

constexpr size_t kHistMax = 90; // ~3 min at 2s refresh

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

std::wstring PackageVersionLabel() {
    try {
        auto v = winrt::Windows::ApplicationModel::Package::Current().Id().Version();
        std::wostringstream os;
        os << L"v" << v.Major << L"." << v.Minor << L"." << v.Build << L"." << v.Revision;
        return os.str();
    } catch (...) {
        return L"";
    }
}

std::wstring NowClockLocal() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wostringstream os;
    os << std::setfill(L'0') << std::setw(2) << st.wHour << L":" << std::setw(2) << st.wMinute
       << L":" << std::setw(2) << st.wSecond;
    return os.str();
}

Border MakeMetricCard(hstring const& label, TextBlock& value_out) {
    auto border = Border{};
    border.Background(SolidColorBrush{kCard});
    border.BorderBrush(SolidColorBrush{kCardBorder});
    border.BorderThickness(ThicknessHelper::FromUniformLength(1));
    border.CornerRadius(CornerRadiusHelper::FromUniformRadius(12));
    border.Padding(ThicknessHelper::FromLengths(18, 14, 18, 14));
    border.HorizontalAlignment(HorizontalAlignment::Stretch);
    border.VerticalAlignment(VerticalAlignment::Stretch);
    border.MinHeight(104);

    // Left accent strip (visual anchor)
    auto root = Grid{};
    auto strip_col = ColumnDefinition{};
    strip_col.Width(GridLengthHelper::FromPixels(4));
    root.ColumnDefinitions().Append(strip_col);
    auto body_col = ColumnDefinition{};
    body_col.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    root.ColumnDefinitions().Append(body_col);

    auto strip = Border{};
    strip.Background(SolidColorBrush{kCardBorder});
    strip.CornerRadius(CornerRadiusHelper::FromRadii(2, 0, 0, 2));
    strip.Margin(ThicknessHelper::FromLengths(-14, -10, 10, -10));
    strip.Width(4);
    strip.HorizontalAlignment(HorizontalAlignment::Left);
    Grid::SetColumn(strip, 0);
    root.Children().Append(strip);

    auto stack = StackPanel{};
    stack.Spacing(8);
    auto lab = TextBlock{};
    lab.Text(label);
    lab.FontSize(12);
    lab.CharacterSpacing(80);
    lab.Foreground(SolidColorBrush{kMuted});

    value_out = TextBlock{};
    value_out.Text(L"—");
    value_out.FontSize(28);
    value_out.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    value_out.Foreground(SolidColorBrush{kWhite});
    value_out.TextTrimming(TextTrimming::CharacterEllipsis);

    stack.Children().Append(lab);
    stack.Children().Append(value_out);
    Grid::SetColumn(stack, 1);
    root.Children().Append(stack);
    border.Child(root);
    return border;
}

Button MakeActionButton(hstring const& label) {
    auto btn = Button{};
    btn.Content(box_value(label));
    btn.MinHeight(58);
    btn.MinWidth(168);
    btn.Padding(ThicknessHelper::FromLengths(26, 12, 26, 12));
    btn.FontSize(18);
    btn.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    btn.Margin(ThicknessHelper::FromLengths(0, 0, 14, 0));
    btn.Background(SolidColorBrush{kCard});
    btn.Foreground(SolidColorBrush{kWhite});
    btn.BorderBrush(SolidColorBrush{kCardBorder});
    btn.BorderThickness(ThicknessHelper::FromUniformLength(1));
    btn.UseSystemFocusVisuals(true);
    btn.IsTabStop(true);
    return btn;
}

TextBlock MakeSectionLabel(hstring const& text) {
    auto t = TextBlock{};
    t.Text(text);
    t.FontSize(11);
    t.Foreground(SolidColorBrush{kMuted});
    t.Margin(ThicknessHelper::FromLengths(2, 0, 0, 8));
    t.CharacterSpacing(100);
    t.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    return t;
}

ProgressBar MakeBar(Color fg) {
    auto bar = ProgressBar{};
    bar.Minimum(0);
    bar.Maximum(1);
    bar.Value(0);
    bar.Height(10);
    bar.Foreground(SolidColorBrush{fg});
    bar.Background(SolidColorBrush{kCard});
    bar.CornerRadius(CornerRadiusHelper::FromUniformRadius(5));
    return bar;
}

} // namespace

MainPageController::MainPageController() = default;

void MainPageController::Init() {
    BuildUI();
    WireButtons();
    WireGamepadFocus();
    ApplyStatus(NodeStatusSnapshot(), {}, "Running probes…");
    StartUiTimer();
}

FrameworkElement MainPageController::BuildHeader() {
    auto header = Grid{};
    header.ColumnDefinitions().Append(ColumnDefinition{});
    auto mid = ColumnDefinition{};
    mid.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    header.ColumnDefinitions().Append(mid);
    header.ColumnDefinitions().Append(ColumnDefinition{});
    header.Margin(ThicknessHelper::FromLengths(0, 0, 0, 18));

    auto title_col = StackPanel{};
    title_col.Spacing(2);
    m_title = TextBlock{};
    m_title.Text(L"₿  xbox_bitcoind");
    m_title.FontSize(30);
    m_title.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    m_title.Foreground(SolidColorBrush{kOrange});
    m_subtitle = TextBlock{};
    auto ver = PackageVersionLabel();
    m_subtitle.Text(ver.empty() ? L"Bitcoin Core · Dev Mode" : (L"Bitcoin Core · " + ver));
    m_subtitle.FontSize(13);
    m_subtitle.Foreground(SolidColorBrush{kMuted});
    title_col.Children().Append(m_title);
    title_col.Children().Append(m_subtitle);
    title_col.VerticalAlignment(VerticalAlignment::Center);
    header.Children().Append(title_col);

    auto center = StackPanel{};
    center.Spacing(6);
    center.HorizontalAlignment(HorizontalAlignment::Center);
    center.VerticalAlignment(VerticalAlignment::Center);
    m_pill = Border{};
    m_pill.CornerRadius(CornerRadiusHelper::FromUniformRadius(20));
    m_pill.Padding(ThicknessHelper::FromLengths(20, 10, 20, 10));
    m_pill.MinWidth(128);
    m_pill.HorizontalAlignment(HorizontalAlignment::Center);
    m_pill_text = TextBlock{};
    m_pill_text.FontSize(15);
    m_pill_text.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    m_pill_text.Foreground(SolidColorBrush{kWhite});
    m_pill_text.HorizontalAlignment(HorizontalAlignment::Center);
    m_pill.Child(m_pill_text);
    m_updated = TextBlock{};
    m_updated.Text(L"updated —");
    m_updated.FontSize(11);
    m_updated.Foreground(SolidColorBrush{kMuted});
    m_updated.HorizontalAlignment(HorizontalAlignment::Center);
    center.Children().Append(m_pill);
    center.Children().Append(m_updated);
    Grid::SetColumn(center, 1);
    header.Children().Append(center);

    m_network_label = TextBlock{};
    m_network_label.Text(L"main · prune");
    m_network_label.FontSize(15);
    m_network_label.Foreground(SolidColorBrush{kMuted});
    m_network_label.VerticalAlignment(VerticalAlignment::Center);
    m_network_label.HorizontalAlignment(HorizontalAlignment::Right);
    Grid::SetColumn(m_network_label, 2);
    header.Children().Append(m_network_label);

    return header;
}

FrameworkElement MainPageController::BuildMetricGrid(std::array<TextBlock*, 4> values,
                                                     std::array<wchar_t const*, 4> labels) {
    auto grid = Grid{};
    grid.ColumnSpacing(12);
    for (int i = 0; i < 4; ++i) {
        auto col = ColumnDefinition{};
        col.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        grid.ColumnDefinitions().Append(col);
    }
    for (int i = 0; i < 4; ++i) {
        auto card = MakeMetricCard(hstring{labels[static_cast<size_t>(i)]}, *values[static_cast<size_t>(i)]);
        Grid::SetColumn(card, i);
        grid.Children().Append(card);
    }
    return grid;
}

FrameworkElement MainPageController::BuildProgressSection() {
    auto panel = StackPanel{};
    panel.Spacing(10);
    panel.Margin(ThicknessHelper::FromLengths(0, 2, 0, 0));

    auto top = Grid{};
    auto star = ColumnDefinition{};
    star.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    top.ColumnDefinitions().Append(star);
    top.ColumnDefinitions().Append(ColumnDefinition{});
    top.Children().Append(MakeSectionLabel(L"SYNC VISUALIZATION"));
    m_progress_label = TextBlock{};
    m_progress_label.Text(L"—");
    m_progress_label.FontSize(14);
    m_progress_label.Foreground(SolidColorBrush{kOrange});
    m_progress_label.HorizontalAlignment(HorizontalAlignment::Right);
    m_progress_label.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    Grid::SetColumn(m_progress_label, 1);
    top.Children().Append(m_progress_label);

    // Legend + dual bars
    auto legend = StackPanel{};
    legend.Orientation(Orientation::Horizontal);
    legend.Spacing(16);
    auto leg_h = TextBlock{};
    leg_h.Text(L"● Headers");
    leg_h.FontSize(11);
    leg_h.Foreground(SolidColorBrush{kCyan});
    auto leg_v = TextBlock{};
    leg_v.Text(L"● Blocks verified");
    leg_v.FontSize(11);
    leg_v.Foreground(SolidColorBrush{kOrange});
    legend.Children().Append(leg_h);
    legend.Children().Append(leg_v);

    m_bar_headers = MakeBar(kCyan);
    m_bar_verify = MakeBar(kOrange);
    m_bar_headers.Height(8);
    m_bar_verify.Height(12);

    // Sparkline card
    auto spark_border = Border{};
    spark_border.Background(SolidColorBrush{kCard});
    spark_border.BorderBrush(SolidColorBrush{kCardBorder});
    spark_border.BorderThickness(ThicknessHelper::FromUniformLength(1));
    spark_border.CornerRadius(CornerRadiusHelper::FromUniformRadius(10));
    spark_border.Padding(ThicknessHelper::FromLengths(12, 8, 12, 8));
    spark_border.Height(72);

    auto spark_stack = StackPanel{};
    auto spark_lab = TextBlock{};
    spark_lab.Text(L"Verification trend (session)");
    spark_lab.FontSize(11);
    spark_lab.Foreground(SolidColorBrush{kMuted});
    spark_lab.Margin(ThicknessHelper::FromLengths(0, 0, 0, 4));

    m_spark_canvas = Canvas{};
    m_spark_canvas.Height(44);
    m_spark_canvas.HorizontalAlignment(HorizontalAlignment::Stretch);
    // Width filled on SizeChanged
    m_spark_fill = Polyline{};
    m_spark_fill.StrokeThickness(0);
    m_spark_fill.Fill(SolidColorBrush{kSparkFill});
    m_spark_line = Polyline{};
    m_spark_line.Stroke(SolidColorBrush{kOrange});
    m_spark_line.StrokeThickness(2.0);
    m_spark_line.StrokeLineJoin(PenLineJoin::Round);
    m_spark_canvas.Children().Append(m_spark_fill);
    m_spark_canvas.Children().Append(m_spark_line);
    m_spark_canvas.SizeChanged([this](IInspectable const&, SizeChangedEventArgs const&) {
        RedrawSparkline();
    });

    spark_stack.Children().Append(spark_lab);
    spark_stack.Children().Append(m_spark_canvas);
    spark_border.Child(spark_stack);

    m_meta = TextBlock{};
    m_meta.Text(L"Datadir —");
    m_meta.FontSize(13);
    m_meta.Foreground(SolidColorBrush{kMuted});
    m_meta.TextWrapping(TextWrapping::WrapWholeWords);

    panel.Children().Append(top);
    panel.Children().Append(legend);
    panel.Children().Append(m_bar_headers);
    panel.Children().Append(m_bar_verify);
    panel.Children().Append(spark_border);
    panel.Children().Append(m_meta);
    return panel;
}

FrameworkElement MainPageController::BuildActions() {
    auto actions = StackPanel{};
    actions.Orientation(Orientation::Horizontal);
    actions.Margin(ThicknessHelper::FromLengths(0, 10, 0, 0));
    m_btn_start = MakeActionButton(L"Start");
    m_btn_stop = MakeActionButton(L"Stop soft");
    m_btn_refresh = MakeActionButton(L"Refresh");
    StylePrimaryButton(m_btn_start, true);
    StylePrimaryButton(m_btn_stop, false);
    StylePrimaryButton(m_btn_refresh, false);
    actions.Children().Append(m_btn_start);
    actions.Children().Append(m_btn_stop);
    actions.Children().Append(m_btn_refresh);
    return actions;
}

FrameworkElement MainPageController::BuildLogPanel() {
    auto border = Border{};
    border.Background(SolidColorBrush{kCard});
    border.BorderBrush(SolidColorBrush{kCardBorder});
    border.BorderThickness(ThicknessHelper::FromUniformLength(1));
    border.CornerRadius(CornerRadiusHelper::FromUniformRadius(12));
    border.Padding(ThicknessHelper::FromUniformLength(14));
    border.HorizontalAlignment(HorizontalAlignment::Stretch);
    border.VerticalAlignment(VerticalAlignment::Stretch);

    auto inner = Grid{};
    inner.RowDefinitions().Append(RowDefinition{});
    auto star = RowDefinition{};
    star.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    inner.RowDefinitions().Append(star);

    auto log_title = TextBlock{};
    log_title.Text(L"DEBUG.LOG");
    log_title.FontSize(11);
    log_title.CharacterSpacing(80);
    log_title.Foreground(SolidColorBrush{kMuted});
    log_title.Margin(ThicknessHelper::FromLengths(0, 0, 0, 8));
    Grid::SetRow(log_title, 0);
    inner.Children().Append(log_title);

    m_log_scroll = ScrollViewer{};
    m_log_scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    m_log_scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    m_log_scroll.VerticalAlignment(VerticalAlignment::Stretch);
    m_log_scroll.IsTabStop(true);
    m_log = TextBlock{};
    m_log.TextWrapping(TextWrapping::Wrap);
    m_log.FontSize(12);
    m_log.FontFamily(FontFamily{L"Consolas"});
    m_log.Foreground(SolidColorBrush{kLogFg});
    m_log.Text(L"…");
    m_log_scroll.Content(m_log);
    Grid::SetRow(m_log_scroll, 1);
    inner.Children().Append(m_log_scroll);

    border.Child(inner);
    return border;
}

void MainPageController::StylePrimaryButton(Button const& btn, bool primary) {
    if (primary) {
        btn.Background(SolidColorBrush{kOrange});
        btn.Foreground(SolidColorBrush{kBg});
        btn.BorderBrush(SolidColorBrush{kOrangeDim});
    } else {
        btn.Background(SolidColorBrush{kCard});
        btn.Foreground(SolidColorBrush{kWhite});
        btn.BorderBrush(SolidColorBrush{kCardBorder});
    }
}

void MainPageController::BuildUI() {
    m_root = Page{};
    m_root.XYFocusKeyboardNavigation(XYFocusKeyboardNavigationMode::Enabled);

    auto root_grid = Grid{};
    root_grid.Background(SolidColorBrush{kBg});
    // Safe margins for 10-foot overscan
    root_grid.Padding(ThicknessHelper::FromLengths(40, 32, 40, 32));

    for (int i = 0; i < 5; ++i) {
        root_grid.RowDefinitions().Append(RowDefinition{});
    }
    auto log_row = RowDefinition{};
    log_row.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    root_grid.RowDefinitions().Append(log_row);

    auto header = BuildHeader();
    Grid::SetRow(header, 0);
    root_grid.Children().Append(header);

    auto chain_block = StackPanel{};
    chain_block.Margin(ThicknessHelper::FromLengths(0, 0, 0, 12));
    chain_block.Children().Append(MakeSectionLabel(L"CHAIN"));
    chain_block.Children().Append(BuildMetricGrid(
        {&m_val_height, &m_val_headers, &m_val_progress, &m_val_peers},
        {L"HEIGHT", L"HEADERS", L"PROGRESS", L"PEERS"}));
    Grid::SetRow(chain_block, 1);
    root_grid.Children().Append(chain_block);

    auto health_block = StackPanel{};
    health_block.Margin(ThicknessHelper::FromLengths(0, 0, 0, 14));
    health_block.Children().Append(MakeSectionLabel(L"NODE"));
    health_block.Children().Append(BuildMetricGrid(
        {&m_val_behind, &m_val_disk, &m_val_mempool, &m_val_uptime},
        {L"BEHIND", L"DISK", L"MEMPOOL", L"UPTIME"}));
    Grid::SetRow(health_block, 2);
    root_grid.Children().Append(health_block);

    auto prog = BuildProgressSection();
    Grid::SetRow(prog, 3);
    root_grid.Children().Append(prog);

    auto actions = BuildActions();
    Grid::SetRow(actions, 4);
    root_grid.Children().Append(actions);

    auto log = BuildLogPanel();
    Grid::SetRow(log, 5);
    root_grid.Children().Append(log);

    m_root.Content(root_grid);
    SetPill(L"INIT", kGray);
}

void MainPageController::WireButtons() {
    auto self = shared_from_this();
    m_btn_start.Click([self](IInspectable const&, RoutedEventArgs const&) { self->OnStartClick(); });
    m_btn_stop.Click([self](IInspectable const&, RoutedEventArgs const&) { self->OnStopClick(); });
    m_btn_refresh.Click([self](IInspectable const&, RoutedEventArgs const&) { self->OnRefreshClick(); });
}

void MainPageController::WireGamepadFocus() {
    m_btn_start.XYFocusRight(m_btn_stop);
    m_btn_stop.XYFocusLeft(m_btn_start);
    m_btn_stop.XYFocusRight(m_btn_refresh);
    m_btn_refresh.XYFocusLeft(m_btn_stop);
    m_btn_start.XYFocusDown(m_log_scroll);
    m_btn_stop.XYFocusDown(m_log_scroll);
    m_btn_refresh.XYFocusDown(m_log_scroll);
    m_log_scroll.XYFocusUp(m_btn_start);
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

void MainPageController::SetMetric(TextBlock const& value, std::wstring const& text, Color color) {
    if (!value) {
        return;
    }
    if (value.Text() != text) {
        value.Text(text);
    }
    value.Foreground(SolidColorBrush{color});
}

void MainPageController::PushHistory(double verification, int /*blocks*/) {
    const double v = std::clamp(verification, 0.0, 1.0);
    m_hist_progress.push_back(v);
    while (m_hist_progress.size() > kHistMax) {
        m_hist_progress.pop_front();
    }
    RedrawSparkline();
}

void MainPageController::RedrawSparkline() {
    if (!m_spark_canvas || !m_spark_line || !m_spark_fill) {
        return;
    }
    const double w = m_spark_canvas.ActualWidth();
    const double h = m_spark_canvas.ActualHeight();
    if (w < 8 || h < 8 || m_hist_progress.size() < 2) {
        m_spark_line.Points().Clear();
        m_spark_fill.Points().Clear();
        return;
    }

    double lo = 1.0;
    double hi = 0.0;
    for (double p : m_hist_progress) {
        lo = (std::min)(lo, p);
        hi = (std::max)(hi, p);
    }
    // Expand flat ranges so the line is visible
    if (hi - lo < 1e-6) {
        lo = (std::max)(0.0, lo - 0.01);
        hi = (std::min)(1.0, hi + 0.01);
    }
    const double span = hi - lo;

    auto pts = m_spark_line.Points();
    auto fill = m_spark_fill.Points();
    pts.Clear();
    fill.Clear();

    const size_t n = m_hist_progress.size();
    fill.Append(Point(0.f, static_cast<float>(h)));
    for (size_t i = 0; i < n; ++i) {
        const double x = (n == 1) ? 0.0 : (static_cast<double>(i) / static_cast<double>(n - 1)) * w;
        const double norm = (m_hist_progress[i] - lo) / span;
        const double y = h - norm * (h - 2.0) - 1.0;
        Point pt(static_cast<float>(x), static_cast<float>(y));
        pts.Append(pt);
        fill.Append(pt);
    }
    fill.Append(Point(static_cast<float>(w), static_cast<float>(h)));
}

void MainPageController::ApplyStatus(NodeStatus const& st, std::string const& log_tail,
                                     std::string const& probe_note) {
    auto clear_metrics = [&]() {
        SetMetric(m_val_height, L"—", kMuted);
        SetMetric(m_val_headers, L"—", kMuted);
        SetMetric(m_val_progress, L"—", kMuted);
        SetMetric(m_val_peers, L"—", kMuted);
        SetMetric(m_val_behind, L"—", kMuted);
        SetMetric(m_val_disk, L"—", kMuted);
        SetMetric(m_val_mempool, L"—", kMuted);
        SetMetric(m_val_uptime, L"—", kMuted);
        if (m_bar_headers) {
            m_bar_headers.Value(0);
        }
        if (m_bar_verify) {
            m_bar_verify.Value(0);
        }
        if (m_progress_label) {
            m_progress_label.Text(L"—");
        }
    };

    if (m_updated) {
        m_updated.Text(L"updated " + NowClockLocal());
    }

    if (!st.available) {
        SetPill(L"NO CORE", kPurple);
        clear_metrics();
        m_meta.Text(Utf8ToWide("Scaffold build — Core not linked. " + st.datadir));
        m_network_label.Text(L"scaffold");
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(false);
        m_btn_refresh.IsEnabled(true);
        StylePrimaryButton(m_btn_start, false);
        if (!probe_note.empty()) {
            auto w = Utf8ToWide(probe_note);
            if (w != m_last_log) {
                m_log.Text(w);
                m_last_log = w;
            }
        }
        return;
    }

    if (m_stopping) {
        SetPill(L"STOPPING", kYellow);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(false);
        StylePrimaryButton(m_btn_start, false);
    } else if (!st.running) {
        SetPill(st.last_exit != 0 ? L"ERROR" : L"STOPPED", st.last_exit != 0 ? kRed : kGray);
        m_btn_start.IsEnabled(true);
        m_btn_stop.IsEnabled(false);
        StylePrimaryButton(m_btn_start, true);
    } else if (!st.rpc_ready) {
        SetPill(L"STARTING", kYellow);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
        StylePrimaryButton(m_btn_start, false);
    } else if (!st.network_active) {
        SetPill(L"NET OFF", kRed);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
        StylePrimaryButton(m_btn_start, false);
    } else if (st.initial_block_download || st.verification_progress < 0.999) {
        SetPill(L"SYNCING", kOrange);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
        StylePrimaryButton(m_btn_start, false);
    } else {
        SetPill(L"SYNCED", kGreen);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
        StylePrimaryButton(m_btn_start, false);
    }

    if (st.rpc_ready) {
        const int behind = (st.headers > st.blocks) ? (st.headers - st.blocks) : 0;
        const bool syncing = st.initial_block_download || st.verification_progress < 0.999;
        const Color prog_c = syncing ? kOrange : kGreen;
        const Color peers_c = (st.connections <= 0 && st.running) ? kRed
                               : (st.connections < 3 ? kYellow : kGreen);
        const Color behind_c = behind > 1000 ? kOrange : (behind > 0 ? kYellow : kGreen);

        SetMetric(m_val_height, FormatInt(st.blocks), kWhite);
        SetMetric(m_val_headers, FormatInt(st.headers), kCyan);
        SetMetric(m_val_progress, FormatPct(st.verification_progress), prog_c);
        SetMetric(m_val_peers, FormatInt(st.connections), peers_c);
        SetMetric(m_val_behind, behind > 0 ? FormatInt(behind) : L"0", behind_c);
        SetMetric(m_val_disk, FormatBytes(st.size_on_disk), kWhite);
        SetMetric(m_val_mempool, FormatInt(st.mempool_tx) + L" tx",
                  st.mempool_tx > 0 ? kWhite : kMuted);
        SetMetric(m_val_uptime, FormatUptime(st.uptime_sec), kWhite);

        // Dual progress: header catch-up vs verification
        double header_ratio = 1.0;
        if (st.headers > 0) {
            // Approx: how close blocks are to known headers (0..1)
            header_ratio = std::clamp(static_cast<double>(st.blocks) / static_cast<double>(st.headers),
                                      0.0, 1.0);
        }
        const double verify = std::clamp(st.verification_progress, 0.0, 1.0);
        if (m_bar_headers) {
            m_bar_headers.Value(header_ratio);
        }
        if (m_bar_verify) {
            m_bar_verify.Value(verify);
            m_bar_verify.Foreground(SolidColorBrush{prog_c});
        }
        if (m_progress_label) {
            std::wostringstream os;
            os << FormatPct(verify);
            if (behind > 0) {
                os << L"  ·  " << FormatInt(behind) << L" behind";
            }
            m_progress_label.Text(os.str());
            m_progress_label.Foreground(SolidColorBrush{prog_c});
        }

        PushHistory(verify, st.blocks);

        std::wstring net = Utf8ToWide(st.chain.empty() ? "main" : st.chain);
        if (st.pruned) {
            net += L" · prune";
        }
        if (st.initial_block_download) {
            net += L" · IBD";
        }
        if (!st.network_active) {
            net += L" · net off";
        }
        m_network_label.Text(net);
    } else if (st.running) {
        SetMetric(m_val_height, L"…", kMuted);
        SetMetric(m_val_headers, L"…", kMuted);
        SetMetric(m_val_progress, L"…", kMuted);
        SetMetric(m_val_peers, L"…", kMuted);
        SetMetric(m_val_behind, L"…", kMuted);
        SetMetric(m_val_disk, L"…", kMuted);
        SetMetric(m_val_mempool, L"…", kMuted);
        SetMetric(m_val_uptime, L"…", kMuted);
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
        meta << "  ·  prune ≤" << (st.prune_target_size / (1024 * 1024)) << " MiB";
    }
    if (st.mempool_bytes > 0) {
        meta << "  ·  mempool " << (st.mempool_bytes / 1024) << " KiB";
    }
    if (!st.warnings.empty()) {
        meta << "  ·  ⚠ " << st.warnings;
    }
    if (st.last_exit != 0 && !st.running) {
        meta << "  ·  last exit " << st.last_exit;
    }
    if (!probe_note.empty()) {
        meta << "  ·  " << probe_note;
    }
    m_meta.Text(Utf8ToWide(meta.str()));

    if (!log_tail.empty()) {
        auto w = Utf8ToWide(log_tail);
        if (w != m_last_log) {
            m_log.Text(w);
            m_last_log = std::move(w);
            m_log_scroll.UpdateLayout();
            m_log_scroll.ChangeView(nullptr, m_log_scroll.ScrollableHeight(), nullptr);
        }
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
            log = ReadDebugLogTail(st.datadir, 48);
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
    if (!NodeCoreLinked() || m_stopping) {
        return;
    }
    if (NodeStart()) {
        ApplyStatus(NodeStatusSnapshot(), {}, m_probe_note);
        RefreshAsync();
    }
}

void MainPageController::OnStopClick() {
    Logf("[ui] Stop soft clicked");
    if (m_stopping) {
        return;
    }
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
                                try {
                                    if (self->m_btn_start && self->m_btn_start.IsEnabled()) {
                                        self->m_btn_start.Focus(FocusState::Programmatic);
                                    } else if (self->m_btn_stop && self->m_btn_stop.IsEnabled()) {
                                        self->m_btn_stop.Focus(FocusState::Programmatic);
                                    }
                                } catch (...) {
                                }
                            });
    });
}

} // namespace xbb
