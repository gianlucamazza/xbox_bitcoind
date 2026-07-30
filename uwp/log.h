// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

// Appends to LocalState\bitcoind.log and OutputDebugString.
namespace xbb {

void LogInit();
void LogWrite(const char* msg);
void Logf(const char* fmt, ...);
std::wstring LocalStatePath();
std::wstring LocalStateFile(const wchar_t* name);

} // namespace xbb
