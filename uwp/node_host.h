// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <string>

namespace xbb {

struct NodeStatus {
    bool running = false;
    bool available = false; // true when BITCOIND_EMBED linked
    std::string message;
    std::string datadir;
    int last_exit = 0;
};

// Snapshot for UI (thread-safe enough for status reads).
NodeStatus NodeStatusSnapshot();

// Start bitcoind on a background thread (no-op / false if not linked).
bool NodeStart();
void NodeStop();

// true if compiled with XBB_WITH_CORE
bool NodeCoreLinked();

} // namespace xbb
