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
#include <cmath>
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

TextBlock MakeTinyLabel(hstring const& text) {
    auto t = TextBlock{};
    t.Text(text);
    t.FontSize(10);
    t.Foreground(SolidColorBrush{kMuted});
    t.Margin(ThicknessHelper::FromLengths(2, 0, 0, 4));
    t.CharacterSpacing(80);
    t.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    return t;
}

} // namespace

// --- Self-discovery: map viewport → layout tokens ---------------------------------

UiLayout DiscoverLayout(double width, double height) {
    UiLayout L;
    L.viewport_w = width > 1 ? width : 1920;
    L.viewport_h = height > 1 ? height : 1080;

    // Height drives TV fit; width handles rare portrait / split (unlikely on Xbox).
    // Compact: tight safe-area or small window. Standard: 1080p TV. Comfort: tall.
    if (L.viewport_h < 860 || L.viewport_w < 1100) {
        L.density = UiDensity::Compact;
    } else if (L.viewport_h >= 1200 && L.viewport_w >= 1600) {
        L.density = UiDensity::Comfort;
    } else {
        L.density = UiDensity::Standard;
    }

    if (L.viewport_w < 1280) {
        L.metric_columns = 2;
    } else {
        L.metric_columns = 4;
    }

    switch (L.density) {
    case UiDensity::Compact:
        L.pad_x = 20;
        L.pad_y = 12;
        L.title_fs = 20;
        L.subtitle_fs = 11;
        L.value_fs = 18;
        L.label_fs = 10;
        L.log_fs = 11;
        L.meta_fs = 11;
        L.btn_fs = 15;
        L.pill_fs = 12;
        L.card_min_h = 56;
        L.card_pad_y = 6;
        L.card_pad_x = 10;
        L.card_gap = 6;
        L.spark_h = 28;
        L.spark_card_h = 44;
        L.bar_h_headers = 5;
        L.bar_h_verify = 7;
        L.log_min_h = 96;
        L.btn_min_h = 40;
        L.btn_min_w = 120;
        L.header_margin_b = 6;
        L.section_gap = 4;
        L.show_sparkline = L.viewport_h >= 780;
        L.show_section_labels = false;
        L.show_subtitle = false;
        L.meta_wrap = false;
        break;
    case UiDensity::Comfort:
        L.pad_x = 40;
        L.pad_y = 28;
        L.title_fs = 30;
        L.subtitle_fs = 14;
        L.value_fs = 26;
        L.label_fs = 12;
        L.log_fs = 13;
        L.meta_fs = 13;
        L.btn_fs = 18;
        L.pill_fs = 15;
        L.card_min_h = 88;
        L.card_pad_y = 12;
        L.card_pad_x = 16;
        L.card_gap = 12;
        L.spark_h = 48;
        L.spark_card_h = 76;
        L.bar_h_headers = 8;
        L.bar_h_verify = 12;
        L.log_min_h = 200;
        L.btn_min_h = 52;
        L.btn_min_w = 160;
        L.header_margin_b = 14;
        L.section_gap = 10;
        L.show_sparkline = true;
        L.show_section_labels = true;
        L.show_subtitle = true;
        L.meta_wrap = true;
        break;
    case UiDensity::Standard:
    default:
        // Tuned so 1920×1080 with ~5–8% overscan still shows header→log without page scroll.
        L.pad_x = 28;
        L.pad_y = 16;
        L.title_fs = 24;
        L.subtitle_fs = 12;
        L.value_fs = 20;
        L.label_fs = 10;
        L.log_fs = 12;
        L.meta_fs = 12;
        L.btn_fs = 16;
        L.pill_fs = 13;
        L.card_min_h = 64;
        L.card_pad_y = 7;
        L.card_pad_x = 12;
        L.card_gap = 8;
        L.spark_h = 32;
        L.spark_card_h = 52;
        L.bar_h_headers = 6;
        L.bar_h_verify = 9;
        L.log_min_h = 112;
        L.btn_min_h = 44;
        L.btn_min_w = 136;
        L.header_margin_b = 8;
        L.section_gap = 6;
        L.show_sparkline = true;
        L.show_section_labels = false;
        L.show_subtitle = true;
        L.meta_wrap = false;
        break;
    }
    return L;
}

// --- Controller -------------------------------------------------------------------

MainPageController::MainPageController() = default;

void MainPageController::Init() {
    BuildUI();
    WireButtons();
    WireGamepadFocus();
    // Seed Standard tokens before first measure.
    ApplyLayout(DiscoverLayout(1920, 1080));
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

    m_title_col = StackPanel{};
    m_title_col.Spacing(1);
    m_title = TextBlock{};
    m_title.Text(L"₿  xbox_bitcoind");
    m_title.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    m_title.Foreground(SolidColorBrush{kOrange});
    m_subtitle = TextBlock{};
    auto ver = PackageVersionLabel();
    m_subtitle.Text(ver.empty() ? L"Bitcoin Core · Dev Mode" : (L"Bitcoin Core · " + ver));
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

FrameworkElement MainPageController::BuildMetricsBlock() {
    m_metrics_host = StackPanel{};
    m_chain_label = MakeTinyLabel(L"CHAIN");
    m_node_label = MakeTinyLabel(L"NODE");
    m_metrics_host.Children().Append(m_chain_label);

    m_metrics_grid = Grid{};
    // 8 cards: height, headers, progress, peers, behind, disk, mempool, uptime
    struct Slot {
        TextBlock* value;
        wchar_t const* label;
    };
    // values filled after TextBlocks created
    m_metric_cards.clear();
    m_metric_labels.clear();
    m_metric_values.clear();

    auto add_card = [&](wchar_t const* label, TextBlock& value_out) {
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
        strip.HorizontalAlignment(HorizontalAlignment::Left);
        Grid::SetColumn(strip, 0);
        root.Children().Append(strip);

        auto stack = StackPanel{};
        stack.Spacing(4);
        auto lab = TextBlock{};
        lab.Text(label);
        lab.CharacterSpacing(60);
        lab.Foreground(SolidColorBrush{kMuted});
        value_out = TextBlock{};
        value_out.Text(L"—");
        value_out.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        value_out.Foreground(SolidColorBrush{kWhite});
        value_out.TextTrimming(TextTrimming::CharacterEllipsis);
        stack.Children().Append(lab);
        stack.Children().Append(value_out);
        Grid::SetColumn(stack, 1);
        root.Children().Append(stack);
        border.Child(root);

        m_metric_labels.push_back(lab);
        m_metric_values.push_back(value_out);
        m_metric_cards.push_back(border);
        m_metrics_grid.Children().Append(border);
    };

    add_card(L"HEIGHT", m_val_height);
    add_card(L"HEADERS", m_val_headers);
    add_card(L"PROGRESS", m_val_progress);
    add_card(L"PEERS", m_val_peers);
    add_card(L"BEHIND", m_val_behind);
    add_card(L"DISK", m_val_disk);
    add_card(L"MEMPOOL", m_val_mempool);
    add_card(L"UPTIME", m_val_uptime);

    m_metrics_host.Children().Append(m_metrics_grid);
    // NODE label is conceptual (second row); we toggle visibility with CHAIN for density.
    m_metrics_host.Children().Append(m_node_label);
    m_node_label.Visibility(Visibility::Collapsed);

    RelayoutMetricGrid(4);
    return m_metrics_host;
}

void MainPageController::RelayoutMetricGrid(int columns) {
    if (!m_metrics_grid || m_metric_cards.empty()) {
        return;
    }
    columns = (std::max)(2, (std::min)(4, columns));
    const int n = static_cast<int>(m_metric_cards.size());
    const int rows = (n + columns - 1) / columns;

    m_metrics_grid.ColumnDefinitions().Clear();
    m_metrics_grid.RowDefinitions().Clear();
    for (int c = 0; c < columns; ++c) {
        auto col = ColumnDefinition{};
        col.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        m_metrics_grid.ColumnDefinitions().Append(col);
    }
    for (int r = 0; r < rows; ++r) {
        m_metrics_grid.RowDefinitions().Append(RowDefinition{});
    }
    m_metrics_grid.ColumnSpacing(m_layout.card_gap);
    m_metrics_grid.RowSpacing(m_layout.card_gap);

    for (int i = 0; i < n; ++i) {
        const int r = i / columns;
        const int c = i % columns;
        Grid::SetRow(m_metric_cards[static_cast<size_t>(i)], r);
        Grid::SetColumn(m_metric_cards[static_cast<size_t>(i)], c);
    }
}

FrameworkElement MainPageController::BuildProgressSection() {
    auto panel = StackPanel{};

    auto top = Grid{};
    auto star = ColumnDefinition{};
    star.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    top.ColumnDefinitions().Append(star);
    top.ColumnDefinitions().Append(ColumnDefinition{});
    m_sync_section_label = MakeTinyLabel(L"SYNC");
    top.Children().Append(m_sync_section_label);
    m_progress_label = TextBlock{};
    m_progress_label.Text(L"—");
    m_progress_label.Foreground(SolidColorBrush{kOrange});
    m_progress_label.HorizontalAlignment(HorizontalAlignment::Right);
    m_progress_label.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
    m_progress_label.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(m_progress_label, 1);
    top.Children().Append(m_progress_label);

    auto legend = StackPanel{};
    legend.Orientation(Orientation::Horizontal);
    legend.Spacing(14);
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
        const double inner = (std::max)(8.0, border.ActualWidth() - pad);
        m_spark_canvas.Width(inner);
        RedrawSparkline();
    });

    m_meta = TextBlock{};
    m_meta.Text(L"Datadir —");
    m_meta.Foreground(SolidColorBrush{kMuted});
    m_meta.TextTrimming(TextTrimming::CharacterEllipsis);

    panel.Children().Append(top);
    panel.Children().Append(legend);
    panel.Children().Append(m_bar_headers);
    panel.Children().Append(m_bar_verify);
    panel.Children().Append(m_spark_border);
    panel.Children().Append(m_meta);
    panel.Spacing(4);
    return panel;
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
    log_title.CharacterSpacing(60);
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

    m_root_grid = Grid{};
    m_root_grid.Background(SolidColorBrush{kBg});

    // Rows: header, metrics, progress, actions, log*
    for (int i = 0; i < 4; ++i) {
        m_root_grid.RowDefinitions().Append(RowDefinition{});
    }
    auto log_row = RowDefinition{};
    log_row.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    m_root_grid.RowDefinitions().Append(log_row);

    auto header = BuildHeader();
    Grid::SetRow(header, 0);
    m_root_grid.Children().Append(header);

    auto metrics = BuildMetricsBlock();
    Grid::SetRow(metrics, 1);
    m_root_grid.Children().Append(metrics);

    auto prog = BuildProgressSection();
    Grid::SetRow(prog, 2);
    m_root_grid.Children().Append(prog);

    auto actions = BuildActions();
    Grid::SetRow(actions, 3);
    m_root_grid.Children().Append(actions);

    auto log = BuildLogPanel();
    Grid::SetRow(log, 4);
    m_root_grid.Children().Append(log);

    m_root.Content(m_root_grid);

    // Self-discovery: recompute tokens whenever the page is measured.
    m_root.SizeChanged([this](IInspectable const&, SizeChangedEventArgs const& e) {
        OnRootSizeChanged(e.NewSize().Width, e.NewSize().Height);
    });

    SetPill(L"INIT", kGray);
}

void MainPageController::OnRootSizeChanged(double width, double height) {
    if (width < 32 || height < 32) {
        return;
    }
    // Ignore tiny noise remeasures
    if (std::fabs(width - m_layout.viewport_w) < 1.0 && std::fabs(height - m_layout.viewport_h) < 1.0 &&
        m_layout.viewport_w > 1) {
        return;
    }
    auto next = DiscoverLayout(width, height);
    const bool density_changed = next.density != m_layout.density;
    const bool cols_changed = next.metric_columns != m_layout.metric_columns;
    ApplyLayout(next);
    if (cols_changed || density_changed) {
        RelayoutMetricGrid(m_layout.metric_columns);
    }
    Logf("[ui] layout density=%d %.0fx%.0f cols=%d spark=%d", static_cast<int>(m_layout.density),
         m_layout.viewport_w, m_layout.viewport_h, m_layout.metric_columns,
         m_layout.show_sparkline ? 1 : 0);
}

void MainPageController::ApplyLayout(UiLayout const& L) {
    m_layout = L;
    if (!m_root_grid) {
        return;
    }

    m_root_grid.Padding(ThicknessHelper::FromLengths(L.pad_x, L.pad_y, L.pad_x, L.pad_y));

    if (m_title) {
        m_title.FontSize(L.title_fs);
    }
    if (m_subtitle) {
        m_subtitle.FontSize(L.subtitle_fs);
        m_subtitle.Visibility(L.show_subtitle ? Visibility::Visible : Visibility::Collapsed);
    }
    if (m_network_label) {
        m_network_label.FontSize(L.subtitle_fs + 1);
    }
    if (m_updated) {
        m_updated.FontSize((std::max)(10.0, L.subtitle_fs - 1));
    }
    if (m_pill) {
        m_pill.Padding(ThicknessHelper::FromLengths(L.pad_x * 0.55, L.pad_y * 0.45, L.pad_x * 0.55,
                                                    L.pad_y * 0.45));
        m_pill.MinWidth(L.btn_min_w * 0.85);
    }
    if (m_pill_text) {
        m_pill_text.FontSize(L.pill_fs);
    }
    if (m_title_col) {
        // header bottom margin via host: first child row spacing approximated on metrics host
    }

    if (m_chain_label) {
        m_chain_label.Visibility(L.show_section_labels ? Visibility::Visible : Visibility::Collapsed);
        m_chain_label.FontSize(L.label_fs);
    }
    if (m_node_label) {
        m_node_label.Visibility(Visibility::Collapsed);
    }
    if (m_sync_section_label) {
        m_sync_section_label.Visibility(L.show_section_labels ? Visibility::Visible : Visibility::Collapsed);
        m_sync_section_label.FontSize(L.label_fs);
    }

    if (m_metrics_host) {
        m_metrics_host.Margin(ThicknessHelper::FromLengths(0, 0, 0, L.section_gap));
        m_metrics_host.Spacing(L.section_gap * 0.5);
    }
    if (m_metrics_grid) {
        m_metrics_grid.ColumnSpacing(L.card_gap);
        m_metrics_grid.RowSpacing(L.card_gap);
    }

    for (size_t i = 0; i < m_metric_cards.size(); ++i) {
        auto& card = m_metric_cards[i];
        card.MinHeight(L.card_min_h);
        card.Padding(ThicknessHelper::FromLengths(L.card_pad_x, L.card_pad_y, L.card_pad_x, L.card_pad_y));
        if (i < m_metric_labels.size()) {
            m_metric_labels[i].FontSize(L.label_fs);
        }
        if (i < m_metric_values.size()) {
            m_metric_values[i].FontSize(L.value_fs);
        }
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
        m_spark_border.Visibility(L.show_sparkline ? Visibility::Visible : Visibility::Collapsed);
        m_spark_border.Height(L.spark_card_h);
        m_spark_border.Padding(
            ThicknessHelper::FromLengths(L.card_pad_x, L.card_pad_y * 0.6, L.card_pad_x, L.card_pad_y * 0.6));
    }
    if (m_spark_canvas) {
        m_spark_canvas.Height(L.spark_h);
    }
    if (m_meta) {
        m_meta.FontSize(L.meta_fs);
        m_meta.TextWrapping(L.meta_wrap ? TextWrapping::WrapWholeWords : TextWrapping::NoWrap);
        m_meta.MaxLines(L.meta_wrap ? 3 : 1);
    }

    if (m_actions) {
        m_actions.Margin(ThicknessHelper::FromLengths(0, L.section_gap * 0.5, 0, L.section_gap));
    }
    for (auto* b : {&m_btn_start, &m_btn_stop, &m_btn_refresh}) {
        if (!*b) {
            continue;
        }
        (*b).MinHeight(L.btn_min_h);
        (*b).MinWidth(L.btn_min_w);
        (*b).FontSize(L.btn_fs);
        (*b).Padding(ThicknessHelper::FromLengths(L.card_pad_x * 1.4, L.card_pad_y, L.card_pad_x * 1.4,
                                                  L.card_pad_y));
    }

    if (m_log_border) {
        m_log_border.MinHeight(L.log_min_h);
        m_log_border.Padding(ThicknessHelper::FromUniformLength((std::max)(8.0, L.card_pad_x * 0.75)));
    }
    if (m_log) {
        m_log.FontSize(L.log_fs);
    }
    // Ensure log star row never collapses below token min.
    if (m_root_grid && m_root_grid.RowDefinitions().Size() >= 5) {
        m_root_grid.RowDefinitions().GetAt(4).MinHeight(L.log_min_h);
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
    m_timer.Interval(std::chrono::milliseconds(2000));
    auto self = shared_from_this();
    m_timer.Tick([self](IInspectable const&, IInspectable const&) { self->RefreshAsync(); });
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

void MainPageController::PushHistory(double verification, int /*blocks*/) {
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
    if (m_layout.show_sparkline == false) {
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

        double header_ratio = 1.0;
        if (st.headers > 0) {
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

    // Meta: short by default so Standard density fits on TV without vertical overflow.
    std::ostringstream meta;
    if (!st.subversion.empty()) {
        meta << st.subversion;
    } else {
        meta << "bitcoind";
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
    if (m_layout.meta_wrap && !st.datadir.empty()) {
        meta << "  ·  " << st.datadir;
    }
    if (!probe_note.empty() && m_layout.density != UiDensity::Compact) {
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
        self->m_probe_note = all_ok ? "probes OK" : FormatProbeReport(results);
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [self]() {
            self->ApplyStatus(NodeStatusSnapshot(), {}, self->m_probe_note);
            self->RefreshAsync();
        });
    });
}

} // namespace xbb
