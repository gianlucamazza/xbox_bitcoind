// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

namespace xbb {

struct ProbeResult {
    std::string name;
    bool ok = false;
    std::string detail;
};

// Run AppContainer probes for the bitcoind port (filesystem, sockets, alloc).
// Safe to call from a background thread; results are also written to the log.
std::vector<ProbeResult> RunProbes();

std::string FormatProbeReport(const std::vector<ProbeResult>& results);

} // namespace xbb
