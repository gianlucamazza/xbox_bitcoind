// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "log.h"
#include "node_host.h"
#include "rpc_client.h"

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
std::atomic<bool> g_running{false};
std::atomic<int> g_exit{0};
std::thread g_thread;
std::string g_message = "bitcoind not linked (scaffold)";

std::wstring DatadirW() {
    return LocalStatePath() + L"\\bitcoin";
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u(static_cast<size_t>(need > 0 ? need - 1 : 0), '\0');
    if (need > 1) {
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, u.data(), need, nullptr, nullptr);
    }
    return u;
}

void EnsureDatadirLayout() {
    auto base = DatadirW();
    CreateDirectoryW(base.c_str(), nullptr);
    auto conf = base + L"\\bitcoin.conf";
    if (GetFileAttributesW(conf.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring packaged;
        try {
            auto ip = winrt::Windows::ApplicationModel::Package::Current().InstalledLocation().Path();
            packaged = std::wstring(ip.c_str()) + L"\\bitcoin.conf.console";
        } catch (...) {
        }
        if (!packaged.empty() && GetFileAttributesW(packaged.c_str()) != INVALID_FILE_ATTRIBUTES) {
            CopyFileW(packaged.c_str(), conf.c_str(), TRUE);
        } else {
            const char* content =
                "prune=550\nserver=1\nlisten=0\ndbcache=256\nmaxconnections=8\n"
                "printtoconsole=0\nupnp=0\nnatpmp=0\nrpcallowip=127.0.0.1\nrpcbind=127.0.0.1\n";
            FILE* f = _wfopen(conf.c_str(), L"wb");
            if (f) {
                fwrite(content, 1, strlen(content), f);
                fclose(f);
            }
        }
    }
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
    if (auto chain = RpcGetBlockchainInfo(s.datadir)) {
        s.rpc_ready = true;
        s.chain = chain->chain;
        s.blocks = chain->blocks;
        s.headers = chain->headers;
        s.verification_progress = chain->verification_progress;
        s.initial_block_download = chain->initial_block_download;
        s.pruned = chain->pruned;
        if (s.initial_block_download || s.verification_progress < 0.999) {
            s.message = "syncing " + s.chain;
        } else {
            s.message = "synced " + s.chain;
        }
    } else {
        s.rpc_ready = false;
        s.message = "starting (RPC not ready)";
    }
    if (auto net = RpcGetNetworkInfo(s.datadir)) {
        s.connections = net->connections;
    }
    return s;
}

bool NodeStart() {
#ifdef XBB_WITH_CORE
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
        // Wait up to ~45s for clean shutdown
        for (int i = 0; i < 90 && g_running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        if (!g_running.load()) {
            g_thread.join();
            Logf("[node] node thread joined");
        } else {
            Logf("[node] node still running after wait; detaching");
            g_thread.detach();
        }
    }
#else
    Logf("[node] NodeStop stub");
#endif
}

} // namespace xbb
