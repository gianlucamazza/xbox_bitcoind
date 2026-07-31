// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <string>

namespace xbb {

struct NodeStatus {
    bool running = false;
    bool available = false; // true when XBB_WITH_CORE
    bool rpc_ready = false;
    std::string message;
    std::string datadir;
    std::string chain;
    int last_exit = 0;
    int blocks = 0;
    int headers = 0;
    int connections = 0;
    double verification_progress = 0.0; // 0..1
    bool initial_block_download = false;
    bool pruned = false;
};

// Snapshot for UI (mutex-protected message + atomics).
NodeStatus NodeStatusSnapshot();

// Enrich snapshot with live RPC + optional debug.log path (blocking; call off UI thread).
NodeStatus NodeStatusLive();

// Start bitcoind on a background thread (no-op / false if not linked).
bool NodeStart();
// Stop via RPC "stop", then join the node thread (best-effort).
void NodeStop();

// true if compiled with XBB_WITH_CORE
bool NodeCoreLinked();

std::string NodeDatadirUtf8();

} // namespace xbb
