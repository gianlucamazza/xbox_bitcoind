// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace xbb {

struct BlockchainInfo {
    std::string chain;
    int blocks = 0;
    int headers = 0;
    double verification_progress = 0.0;
    bool initial_block_download = false;
    bool pruned = false;
    int64_t size_on_disk = 0;       // bytes
    int64_t prune_target_size = 0;  // bytes (0 if unknown / not set)
    std::string warnings;
};

struct NetworkInfo {
    int connections = 0;
    bool network_active = true;
    std::string subversion; // e.g. /Satoshi:31.1.0/
};

struct MempoolInfo {
    int size = 0;        // tx count
    int64_t bytes = 0;   // memory usage of txs
    int64_t usage = 0;   // total mempool usage
};

// JSON-RPC over loopback using cookie auth from <datadir>/.cookie.
//
// Limits: response fields are extracted with a minimal flat-object scanner
// (not a full JSON parser). Suitable for Core getblockchaininfo / getnetworkinfo /
// getmempoolinfo / uptime scalar fields. Nested objects, arrays, and complex
// escape sequences in warnings are best-effort only — do not rely on this for
// arbitrary RPC methods without hardening the extractors.
std::optional<std::string> RpcCall(const std::string& datadir_utf8, const std::string& method,
                                   const std::string& params_json = "[]");

std::optional<BlockchainInfo> RpcGetBlockchainInfo(const std::string& datadir_utf8);
std::optional<NetworkInfo> RpcGetNetworkInfo(const std::string& datadir_utf8);
std::optional<MempoolInfo> RpcGetMempoolInfo(const std::string& datadir_utf8);
// Seconds since bitcoind start; nullopt if RPC not ready.
std::optional<int64_t> RpcUptime(const std::string& datadir_utf8);
bool RpcStop(const std::string& datadir_utf8);

// Last N lines of debug.log (UTF-8). Empty if missing/unreadable.
std::string ReadDebugLogTail(const std::string& datadir_utf8, size_t max_lines = 40);

} // namespace xbb
