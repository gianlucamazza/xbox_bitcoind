// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace xbb {

// Future integration point for Bitcoin Core bitcoind.
// Scaffold only: does not link Core yet.
struct NodeStatus {
    bool running = false;
    std::string message = "bitcoind not linked (scaffold)";
    std::string datadir;
};

// Returns planned datadir under LocalState\bitcoin and status message.
NodeStatus NodeStatusSnapshot();

// Placeholder start/stop — no-op until Core is linked into the package.
bool NodeStart();
void NodeStop();

} // namespace xbb
