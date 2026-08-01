// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "log.h"
#include "node_host.h"
#include "rpc_client.h"
#include "text_util.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#ifdef XBB_WITH_CORE
#include "bitcoind_embed.h"
#endif

namespace xbb {
namespace {

std::mutex g_mu;
// Serializes NodeStart/NodeStop and guards g_thread: start and stop can be issued
// concurrently (UI click, suspend worker, resume worker). Held for the whole stop wait,
// so a start racing a stop blocks until the stop settles — callers must be off the UI thread.
std::mutex g_lifecycle_mu;
std::atomic<bool> g_running{false};
std::atomic<int> g_exit{0};
std::thread g_thread;
#ifdef XBB_WITH_CORE
std::string g_message = "stopped";
#else
std::string g_message = "bitcoind not linked (scaffold)";
#endif

std::wstring DatadirW() {
    return LocalStatePath() + L"\\bitcoin";
}

} // namespace

std::string SeedDatadirConf() {
    auto base = DatadirW();
    CreateDirectoryW(base.c_str(), nullptr);
    auto conf = base + L"\\bitcoin.conf";
    // Never overwrite operator conf (IBD knobs / apply-console-conf). Seed only if missing.
    if (GetFileAttributesW(conf.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return "conf kept: " + WideToUtf8(base);
    }
    std::wstring packaged;
    try {
        auto ip = winrt::Windows::ApplicationModel::Package::Current().InstalledLocation().Path();
        packaged = std::wstring(ip.c_str()) + L"\\bitcoin.conf.console";
    } catch (...) {
    }
    if (!packaged.empty() && GetFileAttributesW(packaged.c_str()) != INVALID_FILE_ATTRIBUTES) {
        packaged.clear();
    }
    if (!packaged.empty() && CopyFileW(packaged.c_str(), conf.c_str(), TRUE)) {
        return "datadir seeded from package: " + WideToUtf8(base);
    }
    // Last-resort only: package should ship config/bitcoin.conf.console as
    // bitcoin.conf.console (vcxproj). Keep key knobs aligned with that file
    // (enforced by scripts/check-conf-sync.sh in CI).
    // Prefer: ./scripts/apply-console-conf.sh for operator updates.
    const char* content =
        "prune=550\nserver=1\nlisten=0\ndbcache=512\nmaxconnections=16\n"
        "maxmempool=50\nblocksonly=1\n"
        "printtoconsole=0\nupnp=0\nnatpmp=0\nrpcallowip=127.0.0.1\nrpcbind=127.0.0.1\n";
    FILE* f = _wfopen(conf.c_str(), L"wb");
    if (!f) {
        return "error: cannot write bitcoin.conf under " + WideToUtf8(base);
    }
    const size_t len = strlen(content);
    const bool ok = fwrite(content, 1, len, f) == len;
    fclose(f);
    if (!ok) {
        return "error: short write seeding bitcoin.conf";
    }
    Logf("[node] bitcoin.conf missing package seed; wrote embedded fallback");
    return "datadir seeded (embedded fallback): " + WideToUtf8(base);
}

namespace {

void EnsureDatadirLayout() {
    SeedDatadirConf();
}

#ifdef XBB_WITH_CORE
void NodeThreadMain() {
    EnsureDatadirLayout();
    std::string datadir = WideToUtf8(DatadirW());
    std::string conf = datadir + "\\bitcoin.conf";

    std::string a0 = "bitcoind";
    std::string a1 = "-datadir=" + datadir;
    std::string a2 = "-conf=" + conf;
    std::string a3 = "-server=1";
    std::string a4 = "-listen=0";
    std::string a5 = "-printtoconsole=0";

    std::vector<char*> argv;
    argv.push_back(a0.data());
    argv.push_back(a1.data());
    argv.push_back(a2.data());
    argv.push_back(a3.data());
    argv.push_back(a4.data());
    argv.push_back(a5.data());

    {
        std::lock_guard lock(g_mu);
        g_message = "bitcoind starting (datadir=" + datadir + ")";
    }
    Logf("[node] BitcoindMain starting datadir=%s", datadir.c_str());
    g_running = true;
    int rc = 1;
    try {
        rc = BitcoindMain(static_cast<int>(argv.size()), argv.data());
    } catch (const std::exception& e) {
        Logf("[node] BitcoindMain exception: %s", e.what());
        rc = 99;
    } catch (...) {
        Logf("[node] BitcoindMain unknown exception");
        rc = 98;
    }
    g_exit = rc;
    g_running = false;
    {
        std::lock_guard lock(g_mu);
        g_message = "bitcoind exited rc=" + std::to_string(rc);
    }
    Logf("[node] BitcoindMain exited rc=%d", rc);
}
#endif

} // namespace

bool NodeCoreLinked() {
#ifdef XBB_WITH_CORE
    return true;
#else
    return false;
#endif
}

std::string NodeDatadirUtf8() {
    return WideToUtf8(DatadirW());
}

NodeStatus NodeStatusSnapshot() {
    NodeStatus s;
    s.available = NodeCoreLinked();
    s.running = g_running.load();
    s.last_exit = g_exit.load();
    s.datadir = NodeDatadirUtf8();
    {
        std::lock_guard lock(g_mu);
        s.message = g_message;
    }
    if (!s.available) {
        s.message =
            "Scaffold / Core not linked.\n"
            "Build with -WithCore after scripts/build-core-uwp.ps1 succeeds.\n"
            "Planned datadir: " +
            s.datadir;
    }
    return s;
}

NodeStatus NodeStatusLive() {
    NodeStatus s = NodeStatusSnapshot();
    if (!s.available) {
        return s;
    }
    if (!s.running) {
        return s;
    }
    RpcError err = RpcError::None;
    if (auto chain = RpcGetBlockchainInfo(s.datadir, &err)) {
        s.rpc_ready = true;
        s.connections = -1; // unknown until getnetworkinfo answers
        s.chain = chain->chain;
        s.blocks = chain->blocks;
        s.headers = chain->headers;
        s.verification_progress = chain->verification_progress;
        s.initial_block_download = chain->initial_block_download;
        s.pruned = chain->pruned;
        s.size_on_disk = chain->size_on_disk;
        s.prune_target_size = chain->prune_target_size;
        s.mediantime = chain->mediantime;
        s.warnings = chain->warnings;
        const int behind = (s.headers > s.blocks) ? (s.headers - s.blocks) : 0;
        if (s.initial_block_download || s.verification_progress < 0.999) {
            s.message = "IBD " + s.chain;
            if (behind > 0) {
                s.message += " · " + std::to_string(behind) + " headers ahead";
            }
        } else {
            s.message = "synced " + s.chain;
        }
    } else {
        s.rpc_ready = false;
        switch (err) {
        case RpcError::AuthFailed:
            s.message = "RPC auth failed (stale cookie?)";
            break;
        case RpcError::WarmingUp:
            s.message = "starting (RPC warming up)";
            break;
        default:
            s.message = "starting (RPC not ready)";
            break;
        }
    }
    if (auto net = RpcGetNetworkInfo(s.datadir)) {
        s.connections = net->connections;
        s.network_active = net->network_active;
        s.subversion = net->subversion;
    }
    if (auto mp = RpcGetMempoolInfo(s.datadir)) {
        s.mempool_tx = mp->size;
        s.mempool_bytes = mp->usage > 0 ? mp->usage : mp->bytes;
    }
    if (auto up = RpcUptime(s.datadir)) {
        s.uptime_sec = *up;
    }
    return s;
}

bool NodeStart() {
#ifdef XBB_WITH_CORE
    std::lock_guard lifecycle(g_lifecycle_mu);
    if (g_running) {
        return true;
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
    EnsureDatadirLayout();
    g_exit = 0;
    {
        std::lock_guard lock(g_mu);
        g_message = "starting…";
    }
    g_thread = std::thread(NodeThreadMain);
    return true;
#else
    Logf("[node] NodeStart: Core not linked (XBB_WITH_CORE undefined)");
    return false;
#endif
}

void NodeStop() {
#ifdef XBB_WITH_CORE
    std::lock_guard lifecycle(g_lifecycle_mu);
    Logf("[node] NodeStop: requesting RPC stop");
    const std::string datadir = NodeDatadirUtf8();
    if (g_running.load()) {
        if (RpcStop(datadir)) {
            Logf("[node] RPC stop sent");
        } else {
            Logf("[node] RPC stop failed (node may not be ready)");
        }
    }
    if (g_thread.joinable()) {
        // Mid-IBD LevelDB flush can exceed 45s; wait up to ~150s before detach.
        constexpr int kMaxHalfSec = 300; // 300 * 500ms = 150s
        for (int i = 0; i < kMaxHalfSec && g_running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        if (!g_running.load()) {
            g_thread.join();
            Logf("[node] node thread joined");
        } else {
            Logf("[node] node still running after %ds wait; detaching (host may DELETE process)",
                 kMaxHalfSec / 2);
            g_thread.detach();
        }
    }
#else
    Logf("[node] NodeStop stub");
#endif
}

} // namespace xbb
