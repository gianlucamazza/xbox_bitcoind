// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace xbb {

struct NodeStatus {
    bool running = false;
    bool available = false; // true when XBB_WITH_CORE
    bool rpc_ready = false;
    std::string message;
    std::string datadir;
    std::string chain;
    std::string subversion; // bitcoind user-agent
    std::string warnings;
    int last_exit = 0;
    int blocks = 0;
    int headers = 0;
    int connections = 0;
    int mempool_tx = 0;
    double verification_progress = 0.0; // 0..1
    bool initial_block_download = false;
    bool pruned = false;
    bool network_active = true;
    int64_t size_on_disk = 0;
    int64_t prune_target_size = 0;
    int64_t mempool_bytes = 0;
    int64_t uptime_sec = 0;
    int64_t mediantime = 0; // tip median time (unix s); 0 if unknown
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
