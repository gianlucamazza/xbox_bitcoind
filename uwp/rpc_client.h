// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

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
};

struct NetworkInfo {
    int connections = 0;
};

// JSON-RPC over loopback using cookie auth from <datadir>/.cookie
std::optional<std::string> RpcCall(const std::string& datadir_utf8, const std::string& method,
                                   const std::string& params_json = "[]");

std::optional<BlockchainInfo> RpcGetBlockchainInfo(const std::string& datadir_utf8);
std::optional<NetworkInfo> RpcGetNetworkInfo(const std::string& datadir_utf8);
bool RpcStop(const std::string& datadir_utf8);

// Last N lines of debug.log (UTF-8). Empty if missing/unreadable.
std::string ReadDebugLogTail(const std::string& datadir_utf8, size_t max_lines = 40);

} // namespace xbb
