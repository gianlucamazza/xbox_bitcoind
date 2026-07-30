// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "log.h"
#include "node_host.h"

namespace xbb {

namespace {
std::wstring DatadirW() {
    return LocalStatePath() + L"\\bitcoin";
}
} // namespace

NodeStatus NodeStatusSnapshot() {
    NodeStatus s;
    s.running = false;
    auto w = DatadirW();
    int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    s.datadir.assign(static_cast<size_t>(need > 0 ? need - 1 : 0), '\0');
    if (need > 1) {
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.datadir.data(), need, nullptr, nullptr);
    }
    s.message =
        "Scaffold only — Bitcoin Core not linked yet.\n"
        "Pin v31.1 desktop baseline is green on CI.\n"
        "Planned: -datadir=" +
        s.datadir + " with config/bitcoin.conf.console defaults.";
    return s;
}

bool NodeStart() {
    Logf("[node] NodeStart stub — Core not linked");
    return false;
}

void NodeStop() {
    Logf("[node] NodeStop stub");
}

} // namespace xbb
