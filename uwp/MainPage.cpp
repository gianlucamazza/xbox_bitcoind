// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "MainPage.h"
#include "log.h"
#include "node_host.h"
#include "probes.h"
#include "rpc_client.h"
#include "text_util.h"
#include "xbb_version.generated.h"

#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.System.Threading.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
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
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::Graphics::Display;

namespace xbb {
namespace {

Color C(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return ColorHelper::FromArgb(a, r, g, b);
}

const Color kBg = C(12, 14, 18);
const Color kCard = C(26, 30, 38);
const Color kCardBorder = C(48, 54, 66);
const Color kOrange = C(247, 147, 26);
const Color kOrangeDim = C(160, 90, 18);
const Color kCyan = C(56, 189, 248);
const Color kWhite = C(245, 247, 250);
const Color kMuted = C(148, 156, 170);
const Color kGreen = C(52, 168, 83);
const Color kYellow = C(234, 179, 8);
const Color kRed = C(220, 68, 68);
const Color kGray = C(100, 106, 118);
const Color kPurple = C(130, 100, 180);
const Color kLogFg = C(186, 192, 204);
const Color kSparkFill = C(247, 147, 26, 40);

constexpr size_t kHistMax = 90;
// Matches StartUiTimer interval (used for session ETA).
constexpr double kRefreshIntervalSec = 2.0;


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
    // TV-friendly: one decimal while syncing (14.0%), whole percent near tip (100%).
    std::wostringstream os;
    const double pct = std::clamp(p, 0.0, 1.0) * 100.0;
    const int prec = (p < 0.999) ? 1 : 0;
    os << std::fixed << std::setprecision(prec) << pct << L"%";
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

// Age of chain tip from mediantime (consensus-operational, not soft-fork signaling).
std::wstring FormatTipAge(int64_t mediantime_unix) {
    if (mediantime_unix <= 0) {
        return {};
    }
    const auto now = static_cast<int64_t>(std::time(nullptr));
    int64_t age = now - mediantime_unix;
    if (age < 0) {
        age = 0;
    }
    if (age < 90) {
        return L"tip now";
    }
    if (age < 3600) {
        return L"tip " + std::to_wstring(age / 60) + L"m";
    }
    if (age < 86400) {
        return L"tip " + std::to_wstring(age / 3600) + L"h";
    }
    return L"tip " + std::to_wstring(age / 86400) + L"d";
}

int64_t TipAgeSec(int64_t mediantime_unix) {
    if (mediantime_unix <= 0) {
        return -1;
    }
    const auto now = static_cast<int64_t>(std::time(nullptr));
    int64_t age = now - mediantime_unix;
    return age < 0 ? 0 : age;
}

// MSIX package identity (app), e.g. "0.1.0.65" — not Bitcoin Core.
std::wstring PackageVersionDigits() {
    try {
        auto v = winrt::Windows::ApplicationModel::Package::Current().Id().Version();
        std::wostringstream os;
        os << v.Major << L"." << v.Minor << L"." << v.Build << L"." << v.Revision;
        return os.str();
    } catch (...) {
        return L"";
    }
}

// "Bitcoin Core v31.1 · app 0.1.0.65" — Core from pin header; app from package.
std::wstring HeaderVersionSubtitle() {
    std::wostringstream os;
    os << L"Bitcoin Core " << XBB_CORE_TAG_W;
    const auto pkg = PackageVersionDigits();
    if (!pkg.empty()) {
        os << L" · app " << pkg;
    } else {
        os << L" · Dev Mode";
    }
    return os.str();
}

std::wstring NowClockLocal() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wostringstream os;
    os << std::setfill(L'0') << std::setw(2) << st.wHour << L":" << std::setw(2) << st.wMinute
       << L":" << std::setw(2) << st.wSecond;
    return os.str();
}

ProgressBar MakeBar(Color fg) {
    auto bar = ProgressBar{};
    bar.Minimum(0);
    bar.Maximum(1);
    bar.Value(0);
    bar.Height(8);
    bar.Foreground(SolidColorBrush{fg});
    bar.Background(SolidColorBrush{kCard});
    bar.CornerRadius(CornerRadiusHelper::FromUniformRadius(4));
    return bar;
}

} // namespace

// --- Self-discovery (WinRT bounds) + pure planner in ui_layout.h -------------------

void GetUsableSize(double page_w, double page_h, double& out_w, double& out_h, double& out_inset) {
    double w = page_w > 1 ? page_w : 1920;
    double h = page_h > 1 ? page_h : 1080;
    double raw_scale = 1.0;
    try {
        auto bounds = ApplicationView::GetForCurrentView().VisibleBounds();
        if (bounds.Width > 32 && bounds.Height > 32) {
            w = bounds.Width;
            h = bounds.Height;
        }
    } catch (...) {
        // Fallback: Page size (pre-activation / test).
    }
    try {
        // Xbox/UWP often reports 960×540 DIPs at 200% scale for a 1080p panel.
        raw_scale = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
    } catch (...) {
        raw_scale = 1.0;
    }
    ComputeUsableSize(w, h, raw_scale, out_w, out_h, out_inset);
}

UiLayout DiscoverLayout(double width, double height) {
    double uw = 0, uh = 0, inset = 0;
    GetUsableSize(width, height, uw, uh, inset);
    return MakeLayout(uw, uh, width > 1 ? width : uw, height > 1 ? height : uh, inset);
}

std::wstring FormatEtaHours(double hours) {
    if (!(hours >= 0.0) || !std::isfinite(hours)) {
        return {};
    }
    if (hours <= 0.0) {
        return L"~now";
    }
    if (hours < 1.0 / 60.0) {
        return L"<1m";
    }
    if (hours < 1.0) {
        return L"~" + std::to_wstring((std::max)(1, static_cast<int>(hours * 60.0 + 0.5))) + L"m";
    }
    if (hours < 48.0) {
        std::wostringstream os;
        os << L"~" << std::fixed << std::setprecision(1) << hours << L"h";
        return os.str();
    }
    std::wostringstream os;
    os << L"~" << std::fixed << std::setprecision(1) << (hours / 24.0) << L"d";
    return os.str();
}

// --- Controller -------------------------------------------------------------------

MainPageController::MainPageController() = default;

void MainPageController::Init() {
    BuildUI();
    WireButtons();
    WireGamepadFocus();
    // Prefer live bounds immediately (may still be 960×540@2x on Series S).
    double w = 1920;
    double h = 1080;
    try {
        auto b = ApplicationView::GetForCurrentView().VisibleBounds();
        if (b.Width > 32 && b.Height > 32) {
            w = b.Width;
            h = b.Height;
        }
    } catch (...) {
    }
    OnRootSizeChanged(w, h);
    ApplyStatus(NodeStatusSnapshot(), {}, "Running probes…");
    StartUiTimer();
}

Border MainPageController::MakeMetricCard(wchar_t const* label, TextBlock& value_out,
                                          TextBlock& label_out) {
    auto border = Border{};
    border.Background(SolidColorBrush{kCard});
    border.BorderBrush(SolidColorBrush{kCardBorder});
    border.BorderThickness(ThicknessHelper::FromUniformLength(1));
    border.CornerRadius(CornerRadiusHelper::FromUniformRadius(10));
    border.HorizontalAlignment(HorizontalAlignment::Stretch);
    border.VerticalAlignment(VerticalAlignment::Stretch);

    auto root = Grid{};
    auto strip_col = ColumnDefinition{};
    strip_col.Width(GridLengthHelper::FromPixels(3));
    root.ColumnDefinitions().Append(strip_col);
    auto body_col = ColumnDefinition{};
    body_col.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    root.ColumnDefinitions().Append(body_col);

    auto strip = Border{};
    strip.Background(SolidColorBrush{kCardBorder});
    strip.Width(3);
    Grid::SetColumn(strip, 0);
    root.Children().Append(strip);

    auto stack = StackPanel{};
    stack.Spacing(2);
    // Slightly more centered than top-left: optical middle of the card body.
    stack.VerticalAlignment(VerticalAlignment::Center);
    stack.HorizontalAlignment(HorizontalAlignment::Stretch);
    stack.Padding(ThicknessHelper::FromLengths(4, 0, 2, 0));
    label_out = TextBlock{};
    auto lab = label_out;
    lab.Text(label);
    lab.CharacterSpacing(40);
    lab.Foreground(SolidColorBrush{kMuted});
    lab.TextAlignment(TextAlignment::Center);
    lab.HorizontalAlignment(HorizontalAlignment::Stretch);
    value_out = TextBlock{};
    value_out.Text(L"—");
    value_out.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    value_out.Foreground(SolidColorBrush{kWhite});
    value_out.TextTrimming(TextTrimming::CharacterEllipsis);
    value_out.TextAlignment(TextAlignment::Center);
    value_out.HorizontalAlignment(HorizontalAlignment::Stretch);
    stack.Children().Append(lab);
    stack.Children().Append(value_out);
    Grid::SetColumn(stack, 1);
    root.Children().Append(stack);
    border.Child(root);
    return border;
}

FrameworkElement MainPageController::BuildHeader() {
    auto header = Grid{};
    header.ColumnDefinitions().Append(ColumnDefinition{});
    auto mid = ColumnDefinition{};
    mid.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    header.ColumnDefinitions().Append(mid);
    header.ColumnDefinitions().Append(ColumnDefinition{});

    m_title_col = StackPanel{};
    m_title_col.Spacing(1);
    m_title = TextBlock{};
    m_title.Text(L"₿  xbox_bitcoind");
    m_title.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    m_title.Foreground(SolidColorBrush{kOrange});
    m_subtitle = TextBlock{};
    m_subtitle.Text(HeaderVersionSubtitle());
    m_subtitle.Foreground(SolidColorBrush{kMuted});
    m_title_col.Children().Append(m_title);
    m_title_col.Children().Append(m_subtitle);
    m_title_col.VerticalAlignment(VerticalAlignment::Center);
    header.Children().Append(m_title_col);

    auto center = StackPanel{};
    center.Spacing(2);
    center.HorizontalAlignment(HorizontalAlignment::Center);
    center.VerticalAlignment(VerticalAlignment::Center);
    m_pill = Border{};
    m_pill.CornerRadius(CornerRadiusHelper::FromUniformRadius(18));
    m_pill.HorizontalAlignment(HorizontalAlignment::Center);
    m_pill_text = TextBlock{};
    m_pill_text.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    m_pill_text.Foreground(SolidColorBrush{kWhite});
    m_pill_text.HorizontalAlignment(HorizontalAlignment::Center);
    m_pill.Child(m_pill_text);
    m_updated = TextBlock{};
    m_updated.Text(L"updated —");
    m_updated.Foreground(SolidColorBrush{kMuted});
    m_updated.HorizontalAlignment(HorizontalAlignment::Center);
    center.Children().Append(m_pill);
    center.Children().Append(m_updated);
    Grid::SetColumn(center, 1);
    header.Children().Append(center);

    m_network_label = TextBlock{};
    m_network_label.Text(L"main · prune");
    m_network_label.Foreground(SolidColorBrush{kMuted});
    m_network_label.VerticalAlignment(VerticalAlignment::Center);
    m_network_label.HorizontalAlignment(HorizontalAlignment::Right);
    Grid::SetColumn(m_network_label, 2);
    header.Children().Append(m_network_label);
    return header;
}

void MainPageController::LayoutMetricRow(Grid const& grid, std::vector<Border> const& cards, int columns) {
    if (!grid || cards.empty()) {
        return;
    }
    columns = (std::max)(2, (std::min)(4, columns));
    const int n = static_cast<int>(cards.size());
    const int rows = (n + columns - 1) / columns;

    grid.ColumnDefinitions().Clear();
    grid.RowDefinitions().Clear();
    for (int c = 0; c < columns; ++c) {
        auto col = ColumnDefinition{};
        col.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        grid.ColumnDefinitions().Append(col);
    }
    for (int r = 0; r < rows; ++r) {
        auto row = RowDefinition{};
        // Explicit min height so rows never collapse to 0 under budget pressure.
        row.MinHeight(m_layout.card_min_h);
        row.Height(GridLengthHelper::FromPixels(m_layout.card_min_h));
        grid.RowDefinitions().Append(row);
    }
    grid.ColumnSpacing(m_layout.card_gap);
    grid.RowSpacing(m_layout.card_gap);

    for (int i = 0; i < n; ++i) {
        Grid::SetRow(cards[static_cast<size_t>(i)], i / columns);
        Grid::SetColumn(cards[static_cast<size_t>(i)], i % columns);
    }
}

FrameworkElement MainPageController::BuildPrimaryMetrics() {
    m_primary_grid = Grid{};
    m_primary_cards.clear();
    m_primary_labels.clear();
    m_primary_values.clear();

    auto add = [&](wchar_t const* lab, TextBlock& val) {
        TextBlock label{nullptr};
        auto card = MakeMetricCard(lab, val, label);
        m_primary_cards.push_back(card);
        m_primary_values.push_back(val);
        m_primary_labels.push_back(label);
        m_primary_grid.Children().Append(card);
    };
    // P1 IBD-critical order
    add(L"HEIGHT", m_val_height);
    add(L"PROGRESS", m_val_progress);
    add(L"PEERS", m_val_peers);
    add(L"BEHIND", m_val_behind);

    LayoutMetricRow(m_primary_grid, m_primary_cards, 4);
    return m_primary_grid;
}

FrameworkElement MainPageController::BuildSecondaryMetrics() {
    m_secondary_grid = Grid{};
    m_secondary_cards.clear();
    m_secondary_labels.clear();
    m_secondary_values.clear();

    auto add = [&](wchar_t const* lab, TextBlock& val) {
        TextBlock label{nullptr};
        auto card = MakeMetricCard(lab, val, label);
        m_secondary_cards.push_back(card);
        m_secondary_values.push_back(val);
        m_secondary_labels.push_back(label);
        m_secondary_grid.Children().Append(card);
    };
    add(L"HEADERS", m_val_headers);
    add(L"DISK", m_val_disk);
    add(L"MEMPOOL", m_val_mempool);
    add(L"UPTIME", m_val_uptime);

    LayoutMetricRow(m_secondary_grid, m_secondary_cards, 4);
    return m_secondary_grid;
}

FrameworkElement MainPageController::BuildProgressSection() {
    m_progress_panel = StackPanel{};
    m_progress_panel.Spacing(4);

    auto top = Grid{};
    auto star = ColumnDefinition{};
    star.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    top.ColumnDefinitions().Append(star);
    top.ColumnDefinitions().Append(ColumnDefinition{});
    auto legend = StackPanel{};
    legend.Orientation(Orientation::Horizontal);
    legend.Spacing(12);
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
    top.Children().Append(legend);

    m_progress_label = TextBlock{};
    m_progress_label.Text(L"—");
    m_progress_label.Foreground(SolidColorBrush{kOrange});
    m_progress_label.HorizontalAlignment(HorizontalAlignment::Right);
    m_progress_label.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    m_progress_label.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(m_progress_label, 1);
    top.Children().Append(m_progress_label);

    m_bar_headers = MakeBar(kCyan);
    m_bar_verify = MakeBar(kOrange);

    m_spark_border = Border{};
    m_spark_border.Background(SolidColorBrush{kCard});
    m_spark_border.BorderBrush(SolidColorBrush{kCardBorder});
    m_spark_border.BorderThickness(ThicknessHelper::FromUniformLength(1));
    m_spark_border.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    m_spark_border.HorizontalAlignment(HorizontalAlignment::Stretch);

    auto spark_stack = StackPanel{};
    auto spark_lab = TextBlock{};
    spark_lab.Text(L"Verification trend");
    spark_lab.FontSize(10);
    spark_lab.Foreground(SolidColorBrush{kMuted});
    m_spark_canvas = Canvas{};
    m_spark_fill = Polyline{};
    m_spark_fill.StrokeThickness(0);
    m_spark_fill.Fill(SolidColorBrush{kSparkFill});
    m_spark_line = Polyline{};
    m_spark_line.Stroke(SolidColorBrush{kOrange});
    m_spark_line.StrokeThickness(2.0);
    m_spark_line.StrokeLineJoin(PenLineJoin::Round);
    m_spark_canvas.Children().Append(m_spark_fill);
    m_spark_canvas.Children().Append(m_spark_line);
    spark_stack.Children().Append(spark_lab);
    spark_stack.Children().Append(m_spark_canvas);
    m_spark_border.Child(spark_stack);
    m_spark_border.SizeChanged([this](IInspectable const& sender, SizeChangedEventArgs const&) {
        if (!m_spark_canvas) {
            return;
        }
        auto border = sender.as<Border>();
        const double pad = m_layout.card_pad_x * 2.0;
        m_spark_canvas.Width((std::max)(8.0, border.ActualWidth() - pad));
        RedrawSparkline();
    });

    m_meta = TextBlock{};
    m_meta.Text(L"—");
    m_meta.Foreground(SolidColorBrush{kMuted});
    m_meta.TextTrimming(TextTrimming::CharacterEllipsis);
    m_meta.MaxLines(1);

    m_progress_panel.Children().Append(top);
    m_progress_panel.Children().Append(m_bar_headers);
    m_progress_panel.Children().Append(m_bar_verify);
    m_progress_panel.Children().Append(m_spark_border);
    m_progress_panel.Children().Append(m_meta);
    return m_progress_panel;
}

FrameworkElement MainPageController::BuildActions() {
    m_actions = StackPanel{};
    m_actions.Orientation(Orientation::Horizontal);
    m_btn_start = Button{};
    m_btn_stop = Button{};
    m_btn_refresh = Button{};
    m_btn_start.Content(box_value(L"Start"));
    m_btn_stop.Content(box_value(L"Stop soft"));
    m_btn_refresh.Content(box_value(L"Refresh"));
    for (auto* b : {&m_btn_start, &m_btn_stop, &m_btn_refresh}) {
        (*b).UseSystemFocusVisuals(true);
        (*b).IsTabStop(true);
        (*b).BorderThickness(ThicknessHelper::FromUniformLength(1));
        (*b).Margin(ThicknessHelper::FromLengths(0, 0, 10, 0));
        (*b).FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    }
    StylePrimaryButton(m_btn_start, true);
    StylePrimaryButton(m_btn_stop, false);
    StylePrimaryButton(m_btn_refresh, false);
    m_actions.Children().Append(m_btn_start);
    m_actions.Children().Append(m_btn_stop);
    m_actions.Children().Append(m_btn_refresh);
    return m_actions;
}

FrameworkElement MainPageController::BuildLogPanel() {
    m_log_border = Border{};
    m_log_border.Background(SolidColorBrush{kCard});
    m_log_border.BorderBrush(SolidColorBrush{kCardBorder});
    m_log_border.BorderThickness(ThicknessHelper::FromUniformLength(1));
    m_log_border.CornerRadius(CornerRadiusHelper::FromUniformRadius(10));
    m_log_border.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_log_border.VerticalAlignment(VerticalAlignment::Stretch);

    auto inner = Grid{};
    inner.RowDefinitions().Append(RowDefinition{});
    auto star = RowDefinition{};
    star.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    inner.RowDefinitions().Append(star);

    auto log_title = TextBlock{};
    log_title.Text(L"DEBUG.LOG");
    log_title.FontSize(10);
    log_title.CharacterSpacing(40);
    log_title.Foreground(SolidColorBrush{kMuted});
    log_title.Margin(ThicknessHelper::FromLengths(0, 0, 0, 4));
    Grid::SetRow(log_title, 0);
    inner.Children().Append(log_title);

    m_log_scroll = ScrollViewer{};
    m_log_scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    m_log_scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    m_log_scroll.VerticalAlignment(VerticalAlignment::Stretch);
    m_log_scroll.IsTabStop(true);
    m_log = TextBlock{};
    m_log.TextWrapping(TextWrapping::Wrap);
    m_log.FontFamily(FontFamily{L"Consolas"});
    m_log.Foreground(SolidColorBrush{kLogFg});
    m_log.Text(L"…");
    m_log_scroll.Content(m_log);
    Grid::SetRow(m_log_scroll, 1);
    inner.Children().Append(m_log_scroll);
    m_log_border.Child(inner);
    return m_log_border;
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
    m_root.Background(SolidColorBrush{kBg});

    // Shell rows (each metric band is its own Auto row so secondary cannot be
    // painted over by Sync):
    //   0 Header | 1 Primary | 2 Secondary | 3 Sync | 4 Actions | 5 Log*
    m_root_grid = Grid{};
    m_root_grid.Background(SolidColorBrush{kBg});
    for (int i = 0; i < 5; ++i) {
        m_root_grid.RowDefinitions().Append(RowDefinition{});
    }
    auto log_row = RowDefinition{};
    log_row.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    m_root_grid.RowDefinitions().Append(log_row);

    auto header = BuildHeader();
    Grid::SetRow(header, 0);
    m_root_grid.Children().Append(header);

    auto primary = BuildPrimaryMetrics();
    primary.Margin(ThicknessHelper::FromLengths(0, 0, 0, 6));
    Grid::SetRow(primary, 1);
    m_root_grid.Children().Append(primary);

    auto secondary = BuildSecondaryMetrics();
    secondary.Margin(ThicknessHelper::FromLengths(0, 0, 0, 6));
    Grid::SetRow(secondary, 2);
    m_root_grid.Children().Append(secondary);

    auto prog = BuildProgressSection();
    Grid::SetRow(prog, 3);
    m_root_grid.Children().Append(prog);

    auto actions = BuildActions();
    Grid::SetRow(actions, 4);
    m_root_grid.Children().Append(actions);

    auto log = BuildLogPanel();
    Grid::SetRow(log, 5);
    m_root_grid.Children().Append(log);

    m_root.Content(m_root_grid);
    m_root.SizeChanged([this](IInspectable const&, SizeChangedEventArgs const& e) {
        OnRootSizeChanged(e.NewSize().Width, e.NewSize().Height);
    });
    SetPill(L"INIT", kGray);
}

void MainPageController::OnRootSizeChanged(double width, double height) {
    if (width < 32 || height < 32) {
        return;
    }
    auto L = DiscoverLayout(width, height);
    auto plan = PlanSections(L.usable_h, L);

    const bool same =
        std::fabs(L.usable_w - m_layout.usable_w) < 2.0 && std::fabs(L.usable_h - m_layout.usable_h) < 2.0 &&
        L.density == m_layout.density && plan.show_secondary == m_plan.show_secondary &&
        plan.show_spark == m_plan.show_spark && L.primary_columns == m_layout.primary_columns;
    if (same && m_layout.usable_w > 1) {
        return;
    }

    ApplyLayout(L, plan);
    Logf("[ui] layout view=%.0fx%.0f usable_eff=%.0fx%.0f density=%d cols=%d secondary=%d spark=%d log_min=%.0f",
         L.viewport_w, L.viewport_h, L.usable_w, L.usable_h, static_cast<int>(L.density),
         L.primary_columns, plan.show_secondary ? 1 : 0, plan.show_spark ? 1 : 0, plan.log_min_h);
}

void MainPageController::ApplyLayout(UiLayout const& L, LayoutPlan const& plan) {
    m_layout = L;
    m_plan = plan;
    if (!m_root_grid) {
        return;
    }

    m_root_grid.Padding(ThicknessHelper::FromLengths(L.pad_x, L.pad_y, L.pad_x, L.pad_y));

    if (m_title) {
        m_title.FontSize(L.title_fs);
    }
    if (m_subtitle) {
        m_subtitle.FontSize(L.subtitle_fs);
        m_subtitle.Visibility(plan.show_subtitle ? Visibility::Visible : Visibility::Collapsed);
    }
    if (m_network_label) {
        m_network_label.FontSize(L.subtitle_fs + 1);
    }
    if (m_updated) {
        m_updated.FontSize((std::max)(10.0, L.subtitle_fs - 1));
    }
    if (m_pill) {
        m_pill.Padding(ThicknessHelper::FromLengths(14, 6, 14, 6));
        m_pill.MinWidth(L.btn_min_w * 0.8);
    }
    if (m_pill_text) {
        m_pill_text.FontSize(L.pill_fs);
    }

    auto style_cards = [&](std::vector<Border>& cards, std::vector<TextBlock>& labels,
                           std::vector<TextBlock>& values) {
        for (size_t i = 0; i < cards.size(); ++i) {
            cards[i].MinHeight(L.card_min_h);
            cards[i].Height(L.card_min_h);
            // Symmetric vertical pad keeps label+value optically centered in the card.
            const double pad_y = (std::max)(4.0, L.card_pad_y);
            cards[i].Padding(
                ThicknessHelper::FromLengths(L.card_pad_x * 0.75, pad_y, L.card_pad_x * 0.75, pad_y));
            if (i < labels.size()) {
                labels[i].FontSize(L.label_fs);
                labels[i].TextAlignment(TextAlignment::Center);
            }
            if (i < values.size()) {
                values[i].FontSize(L.value_fs);
                values[i].TextAlignment(TextAlignment::Center);
            }
        }
    };
    style_cards(m_primary_cards, m_primary_labels, m_primary_values);
    style_cards(m_secondary_cards, m_secondary_labels, m_secondary_values);

    LayoutMetricRow(m_primary_grid, m_primary_cards, L.primary_columns);
    LayoutMetricRow(m_secondary_grid, m_secondary_cards, L.primary_columns);

    // Shell row 2 is dedicated to secondary — collapse the whole row via Visibility
    // so Auto height becomes 0 when budget hides secondary (no paint-over).
    if (m_secondary_grid) {
        if (plan.show_secondary) {
            m_secondary_grid.Visibility(Visibility::Visible);
            const double sh = (L.primary_columns >= 4) ? L.card_min_h : (L.card_min_h * 2 + L.card_gap);
            m_secondary_grid.MinHeight(sh);
            m_secondary_grid.Height(sh);
        } else {
            m_secondary_grid.Visibility(Visibility::Collapsed);
            m_secondary_grid.ClearValue(FrameworkElement::HeightProperty());
            m_secondary_grid.MinHeight(0);
        }
    }
    if (m_primary_grid) {
        const double ph = (L.primary_columns >= 4) ? L.card_min_h : (L.card_min_h * 2 + L.card_gap);
        m_primary_grid.MinHeight(ph);
        m_primary_grid.Height(ph);
    }

    if (m_progress_label) {
        m_progress_label.FontSize(L.label_fs + 2);
    }
    if (m_bar_headers) {
        m_bar_headers.Height(L.bar_h_headers);
    }
    if (m_bar_verify) {
        m_bar_verify.Height(L.bar_h_verify);
    }
    if (m_spark_border) {
        m_spark_border.Visibility(plan.show_spark ? Visibility::Visible : Visibility::Collapsed);
        m_spark_border.Height(L.spark_card_h);
        m_spark_border.Padding(
            ThicknessHelper::FromLengths(L.card_pad_x, L.card_pad_y * 0.5, L.card_pad_x, L.card_pad_y * 0.5));
    }
    if (m_spark_canvas) {
        m_spark_canvas.Height(L.spark_h);
    }
    if (m_meta) {
        m_meta.FontSize(L.meta_fs);
        m_meta.TextWrapping(plan.meta_wrap ? TextWrapping::WrapWholeWords : TextWrapping::NoWrap);
        m_meta.MaxLines(plan.meta_wrap ? 2 : 1);
    }
    if (m_progress_panel) {
        m_progress_panel.Margin(ThicknessHelper::FromLengths(0, 0, 0, L.section_gap));
    }

    if (m_actions) {
        m_actions.Margin(ThicknessHelper::FromLengths(0, 2, 0, L.section_gap));
    }
    for (auto* b : {&m_btn_start, &m_btn_stop, &m_btn_refresh}) {
        if (!*b) {
            continue;
        }
        (*b).MinHeight(L.btn_min_h);
        (*b).Height(L.btn_min_h);
        (*b).MinWidth(L.btn_min_w);
        (*b).FontSize(L.btn_fs);
        (*b).Padding(ThicknessHelper::FromLengths(L.card_pad_x * 1.2, 4, L.card_pad_x * 1.2, 4));
    }

    if (m_log_border) {
        m_log_border.MinHeight(plan.log_min_h);
        m_log_border.Padding(ThicknessHelper::FromUniformLength((std::max)(8.0, L.card_pad_x * 0.7)));
    }
    if (m_log) {
        m_log.FontSize(L.log_fs);
    }
    // Log is last shell row (index 5 after Header/Primary/Secondary/Sync/Actions).
    if (m_root_grid && m_root_grid.RowDefinitions().Size() >= 6) {
        m_root_grid.RowDefinitions().GetAt(5).MinHeight(plan.log_min_h);
    }

    RedrawSparkline();
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
    m_timer.Interval(std::chrono::milliseconds(static_cast<int>(kRefreshIntervalSec * 1000.0)));
    auto self = shared_from_this();
    m_timer.Tick([self](IInspectable const&, IInspectable const&) {
        // Keep STOPPING pill elapsed even if RefreshAsync is busy.
        if (self->m_stopping) {
            const auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::steady_clock::now() - self->m_stop_started)
                                 .count();
            self->SetPill(L"STOPPING " + std::to_wstring(sec) + L"s", kYellow);
        }
        self->RefreshAsync();
    });
    m_timer.Start();
    RefreshAsync();
}

void MainPageController::SetPill(std::wstring const& text, Color bg) {
    if (!m_pill || !m_pill_text) {
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

void MainPageController::PushHistory(double verification) {
    m_hist_progress.push_back(std::clamp(verification, 0.0, 1.0));
    while (m_hist_progress.size() > kHistMax) {
        m_hist_progress.pop_front();
    }
    RedrawSparkline();
}

void MainPageController::RedrawSparkline() {
    if (!m_spark_canvas || !m_spark_line || !m_spark_fill) {
        return;
    }
    if (!m_plan.show_spark) {
        m_spark_line.Points().Clear();
        m_spark_fill.Points().Clear();
        return;
    }
    const double w = m_spark_canvas.ActualWidth() > 1 ? m_spark_canvas.ActualWidth() : m_spark_canvas.Width();
    const double h = m_spark_canvas.ActualHeight() > 1 ? m_spark_canvas.ActualHeight() : m_layout.spark_h;
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
        SetMetric(m_val_progress, L"—", kMuted);
        SetMetric(m_val_peers, L"—", kMuted);
        SetMetric(m_val_behind, L"—", kMuted);
        SetMetric(m_val_headers, L"—", kMuted);
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
        m_meta.Text(Utf8ToWide("Scaffold — Core not linked"));
        m_network_label.Text(L"scaffold");
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(false);
        m_btn_refresh.IsEnabled(true);
        StylePrimaryButton(m_btn_start, false);
        if (!probe_note.empty() && m_log) {
            auto w = Utf8ToWide(probe_note);
            if (w != m_last_log) {
                m_log.Text(w);
                m_last_log = w;
            }
        }
        return;
    }

    // Session sparkline/ETA are per-run: samples across a stop/start (or suspend/resume)
    // would poison the slope, so drop history on running transitions.
    if (st.running != m_last_running) {
        m_hist_progress.clear();
        RedrawSparkline();
    }
    m_last_running = st.running;

    if (m_stopping) {
        const auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::steady_clock::now() - m_stop_started)
                             .count();
        SetPill(L"STOPPING " + std::to_wstring(sec) + L"s", kYellow);
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
    } else if (st.rpc_ready && (st.initial_block_download || st.verification_progress < 0.999)) {
        // Operational consensus: headers race vs block validation.
        const int behind0 = (st.headers > st.blocks) ? (st.headers - st.blocks) : 0;
        const bool headers_phase = st.blocks < 1000 && behind0 > 5000;
        SetPill(headers_phase ? L"HEADERS" : L"SYNCING", kOrange);
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
        StylePrimaryButton(m_btn_start, false);
    } else {
        // rpc_ready is guaranteed here (the !rpc_ready branch above returned the
        // chain to STARTING) — near tip: flag stale chain tip (no new blocks for long).
        const int64_t age = TipAgeSec(st.mediantime);
        if (age > 45 * 60 && st.connections > 0) {
            SetPill(L"STALE", kYellow);
        } else if (age > 45 * 60) {
            SetPill(L"STALE", kOrange);
        } else {
            SetPill(L"SYNCED", kGreen);
        }
        m_btn_start.IsEnabled(false);
        m_btn_stop.IsEnabled(true);
        StylePrimaryButton(m_btn_start, false);
    }

    if (st.rpc_ready) {
        const int behind = (st.headers > st.blocks) ? (st.headers - st.blocks) : 0;
        const bool syncing = st.initial_block_download || st.verification_progress < 0.999;
        const Color prog_c = syncing ? kOrange : kGreen;
        const Color peers_c = (st.connections == 0 && st.running) ? kRed
                               : (st.connections < 3 ? kYellow : kGreen);
        const Color behind_c = behind > 1000 ? kOrange : (behind > 0 ? kYellow : kGreen);

        SetMetric(m_val_height, FormatInt(st.blocks), kWhite);
        SetMetric(m_val_progress, FormatPct(st.verification_progress), prog_c);
        if (st.connections < 0) {
            // getnetworkinfo failed this tick — unknown, not zero.
            SetMetric(m_val_peers, L"—", kMuted);
        } else {
            SetMetric(m_val_peers, FormatInt(st.connections), peers_c);
        }
        SetMetric(m_val_behind, behind > 0 ? FormatInt(behind) : L"0", behind_c);

        m_cache_headers = st.headers;
        m_cache_disk = st.size_on_disk;
        m_cache_mempool = st.mempool_tx;
        m_cache_uptime = st.uptime_sec;

        SetMetric(m_val_headers, FormatInt(st.headers), kCyan);
        SetMetric(m_val_disk, FormatBytes(st.size_on_disk), kWhite);
        SetMetric(m_val_mempool, FormatInt(st.mempool_tx) + L" tx",
                  st.mempool_tx > 0 ? kWhite : kMuted);
        SetMetric(m_val_uptime, FormatUptime(st.uptime_sec), kWhite);

        double header_ratio = 1.0;
        if (st.headers > 0) {
            header_ratio =
                std::clamp(static_cast<double>(st.blocks) / static_cast<double>(st.headers), 0.0, 1.0);
        }
        const double verify = std::clamp(st.verification_progress, 0.0, 1.0);
        if (m_bar_headers) {
            m_bar_headers.Value(header_ratio);
        }
        if (m_bar_verify) {
            m_bar_verify.Value(verify);
            m_bar_verify.Foreground(SolidColorBrush{prog_c});
        }
        PushHistory(verify);

        std::wstring eta;
        if (syncing && m_hist_progress.size() >= 5) {
            const double hours = EstimateEtaHours(m_hist_progress.back(), m_hist_progress.front(),
                                                  m_hist_progress.size(), kRefreshIntervalSec);
            eta = FormatEtaHours(hours);
        }
        const std::wstring tip_age = FormatTipAge(st.mediantime);

        if (m_progress_label) {
            std::wostringstream os;
            os << FormatPct(verify);
            if (behind > 0) {
                os << L"  ·  " << FormatInt(behind) << L" behind";
            }
            if (!eta.empty()) {
                os << L"  ·  ETA " << eta;
            }
            if (!tip_age.empty() && !syncing) {
                os << L"  ·  " << tip_age;
            } else if (!tip_age.empty() && st.blocks > 0) {
                os << L"  ·  " << tip_age;
            }
            m_progress_label.Text(os.str());
            m_progress_label.Foreground(SolidColorBrush{prog_c});
        }

        std::wstring net = Utf8ToWide(st.chain.empty() ? "main" : st.chain);
        if (st.pruned) {
            net += L" · prune";
        }
        if (st.initial_block_download) {
            net += L" · IBD";
        } else if (!tip_age.empty()) {
            net += L" · ";
            net += tip_age;
        }
        m_network_label.Text(net);
    } else if (st.running) {
        SetMetric(m_val_height, L"…", kMuted);
        SetMetric(m_val_progress, L"…", kMuted);
        SetMetric(m_val_peers, L"…", kMuted);
        SetMetric(m_val_behind, L"…", kMuted);
        SetMetric(m_val_headers, L"…", kMuted);
        SetMetric(m_val_disk, L"…", kMuted);
        SetMetric(m_val_mempool, L"…", kMuted);
        SetMetric(m_val_uptime, L"…", kMuted);
    } else {
        clear_metrics();
    }

    // Meta: short status; fold secondary KPIs when grid hidden (no silent data loss).
    {
        std::wostringstream wmeta;
        if (!st.subversion.empty()) {
            wmeta << Utf8ToWide(st.subversion);
        } else if (!st.message.empty()) {
            wmeta << Utf8ToWide(st.message);
        } else {
            wmeta << L"bitcoind";
        }
        if (st.pruned && st.prune_target_size > 0) {
            wmeta << L"  ·  prune ≤" << (st.prune_target_size / (1024 * 1024)) << L" MiB";
        }
        if (!m_plan.show_secondary && st.rpc_ready) {
            wmeta << L"  ·  hdr " << FormatInt(m_cache_headers);
            wmeta << L"  ·  " << FormatBytes(m_cache_disk);
            wmeta << L"  ·  mem " << m_cache_mempool << L" tx";
            wmeta << L"  ·  up " << FormatUptime(m_cache_uptime);
        } else if (st.mempool_bytes > 0) {
            wmeta << L"  ·  mempool " << (st.mempool_bytes / 1024) << L" KiB";
        }
        if (!st.warnings.empty()) {
            wmeta << L"  ·  ⚠ " << Utf8ToWide(st.warnings);
        }
        if (st.last_exit != 0 && !st.running) {
            wmeta << L"  ·  exit " << st.last_exit;
        }
        if (!probe_note.empty() && m_layout.density != UiDensity::Compact) {
            wmeta << L"  ·  " << Utf8ToWide(probe_note);
        }
        m_meta.Text(wmeta.str());
    }

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

std::string MainPageController::ProbeNote() const {
    std::lock_guard lock(m_probe_mu);
    return m_probe_note;
}

void MainPageController::SetProbeNote(std::string note) {
    std::lock_guard lock(m_probe_mu);
    m_probe_note = std::move(note);
}

void MainPageController::RefreshAsync() {
    if (m_refreshing.exchange(true)) {
        return;
    }
    auto self = shared_from_this();
    auto dispatcher = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher();

    winrt::Windows::System::Threading::ThreadPool::RunAsync([self, dispatcher](auto&&) {
        // Any throw before the dispatcher callback would leave m_refreshing stuck true
        // and freeze the dashboard for good — re-arm from the worker on failure.
        try {
            NodeStatus st = NodeStatusLive();
            std::string log;
            if (st.available) {
                log = ReadDebugLogTail(st.datadir, 40);
            }
            auto probe = self->ProbeNote();
            dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                                [self, st, log, probe]() {
                                    self->ApplyStatus(st, log, probe);
                                    self->m_refreshing = false;
                                });
        } catch (...) {
            Logf("[ui] RefreshAsync worker failed; re-arming");
            self->m_refreshing = false;
        }
    });
}

void MainPageController::OnStartClick() {
    Logf("[ui] Start clicked");
    if (!NodeCoreLinked() || m_stopping) {
        return;
    }
    // NodeStart may join a previous node thread and touch the datadir — keep it off the UI thread.
    m_btn_start.IsEnabled(false);
    auto self = shared_from_this();
    auto dispatcher = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher();
    winrt::Windows::System::Threading::ThreadPool::RunAsync([self, dispatcher](auto&&) {
        const bool ok = NodeStart();
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [self, ok]() {
            if (ok) {
                self->ApplyStatus(NodeStatusSnapshot(), {}, self->ProbeNote());
                self->RefreshAsync();
            } else {
                self->m_btn_start.IsEnabled(true);
            }
        });
    });
}

void MainPageController::OnStopClick() {
    Logf("[ui] Stop soft clicked");
    if (m_stopping) {
        return;
    }
    auto self = shared_from_this();
    auto dispatcher = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher();
    m_stopping = true;
    m_stop_started = std::chrono::steady_clock::now();
    m_btn_stop.IsEnabled(false);
    m_btn_start.IsEnabled(false);
    SetPill(L"STOPPING 0s", kYellow);
    winrt::Windows::System::Threading::ThreadPool::RunAsync([self, dispatcher](auto&&) {
        NodeStop();
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [self]() {
            self->m_stopping = false;
            self->ApplyStatus(NodeStatusSnapshot(), {}, self->ProbeNote());
            self->RefreshAsync();
        });
    });
}

void MainPageController::OnRefreshClick() {
    Logf("[ui] Refresh clicked");
    RefreshAsync();
}

void MainPageController::StartProbesAsync() {
    auto self = shared_from_this();
    auto dispatcher = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher();
    winrt::Windows::System::Threading::ThreadPool::RunAsync([self, dispatcher](auto&&) {
        auto results = RunProbes();
        bool all_ok = true;
        for (auto const& r : results) {
            if (!r.ok) {
                all_ok = false;
                break;
            }
        }
        self->SetProbeNote(all_ok ? "probes OK" : FormatProbeReport(results));
        // Auto-start pruned node after probes (WithCore builds) — here on the worker,
        // NodeStart blocks and must not run on the UI thread.
        if (NodeCoreLinked() && !NodeStatusSnapshot().running) {
            Logf("[ui] auto-start after probes");
            NodeStart();
        }
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [self]() {
            self->ApplyStatus(NodeStatusSnapshot(), {}, self->ProbeNote());
            self->RefreshAsync();
        });
    });
}

} // namespace xbb
