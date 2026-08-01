// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"
#include "log.h"

#include <winrt/Windows.Storage.h>

namespace xbb {
namespace {

std::mutex g_mu;
FILE* g_fp = nullptr;

} // namespace

std::wstring LocalStatePath() {
    // Magic static: first caller wins the init race (node thread, probes, UI all call this).
    static const std::wstring local = []() -> std::wstring {
        try {
            return std::wstring(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path().c_str());
        } catch (...) {
            return L".";
        }
    }();
    return local;
}

std::wstring LocalStateFile(const wchar_t* name) {
    return LocalStatePath() + L"\\" + name;
}

void LogInit() {
    std::lock_guard lock(g_mu);
    if (g_fp) {
        return;
    }
    try {
        auto path = LocalStateFile(L"bitcoind.log");
        g_fp = _wfopen(path.c_str(), L"a");
    } catch (...) {
    }
}

void LogWrite(const char* msg) {
    SYSTEMTIME st = {};
    GetSystemTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "%02u:%02u:%02u.%03u ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    OutputDebugStringA(ts);
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
    std::lock_guard lock(g_mu);
    if (g_fp) {
        fputs(ts, g_fp);
        fputs(msg, g_fp);
        fputc('\n', g_fp);
        fflush(g_fp);
    }
}

void Logf(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LogWrite(buf);
}

} // namespace xbb
