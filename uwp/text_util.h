// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

// UTF-8 ⇄ UTF-16 conversion helpers (Win32). Single home for the conversions
// that used to be duplicated across node_host.cpp / MainPage.cpp / probes.cpp.

#include <string>

#include <windows.h>

namespace xbb {

inline std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u(static_cast<size_t>(need > 0 ? need - 1 : 0), '\0');
    if (need > 1) {
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, u.data(), need, nullptr, nullptr);
    }
    return u;
}

inline std::wstring Utf8ToWide(const std::string& text) {
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

} // namespace xbb
