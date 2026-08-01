// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"
#include "log.h"
#include "node_host.h"
#include "probes.h"

#include <chrono>
#include <fstream>
#include <optional>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

namespace xbb {
namespace {

ProbeResult ProbeLocalStateWrite() {
    ProbeResult r{"localstate_write", false, {}};
    // Probe files are always deleted afterwards — leaving 16 MiB behind on every
    // launch is pointless flash wear on a console that already does IBD writes.
    constexpr size_t kChunk = 4 * 1024 * 1024;
    constexpr int kFiles = 4;
    auto dir = LocalStatePath() + L"\\probe";
    auto cleanup = [&]() {
        for (int i = 0; i < kFiles; ++i) {
            wchar_t name[64];
            swprintf(name, 64, L"\\probe\\chunk_%02d.bin", i);
            DeleteFileW((LocalStatePath() + name).c_str());
        }
        RemoveDirectoryW(dir.c_str());
    };
    try {
        CreateDirectoryW(dir.c_str(), nullptr);
        // Several 4 MiB files (well under 2 GB single-file limit)
        std::vector<char> buf(kChunk, static_cast<char>(0xAB));
        size_t total = 0;
        for (int i = 0; i < kFiles; ++i) {
            wchar_t name[64];
            swprintf(name, 64, L"\\probe\\chunk_%02d.bin", i);
            auto path = LocalStatePath() + name;
            FILE* f = _wfopen(path.c_str(), L"wb");
            if (!f) {
                r.detail = "fopen failed";
                cleanup();
                return r;
            }
            if (fwrite(buf.data(), 1, buf.size(), f) != buf.size()) {
                fclose(f);
                r.detail = "fwrite short";
                cleanup();
                return r;
            }
            fclose(f);
            total += buf.size();
        }
        cleanup();
        r.ok = true;
        r.detail = "wrote " + std::to_string(total / (1024 * 1024)) + " MiB across " +
                   std::to_string(kFiles) + " files under LocalState\\probe (cleaned up)";
    } catch (const std::exception& ex) {
        cleanup();
        r.detail = ex.what();
    } catch (...) {
        cleanup();
        r.detail = "unknown exception";
    }
    return r;
}

ProbeResult ProbeVirtualAlloc() {
    ProbeResult r{"virtual_alloc", false, {}};
    // VirtualLock/VirtualUnlock are desktop-partition APIs and are not available
    // under WINAPI_PARTITION_APP (UWP). Probe committed memory only.
    constexpr SIZE_T kSize = 64 * 1024 * 1024; // 64 MiB
    void* p = VirtualAlloc(nullptr, kSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) {
        r.detail = "VirtualAlloc failed err=" + std::to_string(GetLastError());
        return r;
    }
    // Touch pages
    volatile char* c = static_cast<volatile char*>(p);
    for (SIZE_T i = 0; i < kSize; i += 4096) {
        c[i] = 1;
    }
    VirtualFree(p, 0, MEM_RELEASE);
    r.ok = true;
    r.detail = "VirtualAlloc+touch 64MiB OK (VirtualLock not in UWP partition — skipped)";
    return r;
}

ProbeResult ProbeOutboundTcp() {
    ProbeResult r{"outbound_tcp", false, {}};
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        r.detail = "WSAStartup failed";
        return r;
    }

    // DNS + TCP connect to a public HTTP host (port 80). No full HTTP needed.
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* res = nullptr;
    int gai = getaddrinfo("one.one.one.one", "80", &hints, &res);
    if (gai != 0 || !res) {
        r.detail = "getaddrinfo failed: " + std::to_string(gai);
        WSACleanup();
        return r;
    }

    SOCKET s = INVALID_SOCKET;
    std::string how;
    for (auto* p = res; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) {
            continue;
        }
        // short timeout
        DWORD timeout_ms = 8000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
        if (connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) {
            how = (p->ai_family == AF_INET6) ? "IPv6" : "IPv4";
            break;
        }
        closesocket(s);
        s = INVALID_SOCKET;
    }
    freeaddrinfo(res);

    if (s == INVALID_SOCKET) {
        r.detail = "connect failed WSA=" + std::to_string(WSAGetLastError());
        WSACleanup();
        return r;
    }

    const char* req = "HEAD / HTTP/1.0\r\nHost: one.one.one.one\r\nConnection: close\r\n\r\n";
    send(s, req, static_cast<int>(strlen(req)), 0);
    char buf[256];
    int n = recv(s, buf, sizeof(buf) - 1, 0);
    closesocket(s);
    WSACleanup();

    if (n <= 0) {
        r.detail = "connected (" + how + ") but no response";
        // Still count as partial success for P2P-like outbound
        r.ok = true;
        return r;
    }
    buf[n] = 0;
    r.ok = true;
    r.detail = "TCP " + how + " to one.one.one.one:80 OK, recv " + std::to_string(n) + " bytes";
    return r;
}

ProbeResult ProbeDatadirLayout() {
    ProbeResult r{"datadir_layout", false, {}};
    try {
        // Single implementation shared with NodeStart (node_host).
        r.detail = SeedDatadirConf();
        r.ok = r.detail.rfind("error", 0) != 0;
    } catch (...) {
        r.detail = "exception";
    }
    return r;
}

// Cached results from a previous launch: heavy probes (16 MiB write, outbound
// TCP to a third-party host) should not rerun on every start. Valid only if the
// file parses and every probe was OK — any FAIL means re-probe next launch.
std::optional<std::vector<ProbeResult>> ReadCachedProbeResults() {
    std::ifstream in(LocalStateFile(L"probe-results.txt"));
    if (!in) {
        return std::nullopt;
    }
    std::vector<ProbeResult> out;
    std::string line;
    while (std::getline(in, line)) {
        const auto t1 = line.find('\t');
        const auto t2 = line.find('\t', t1 + 1);
        if (t1 == std::string::npos || t2 == std::string::npos) {
            return std::nullopt;
        }
        ProbeResult p;
        p.name = line.substr(0, t1);
        p.ok = line.substr(t1 + 1, t2 - t1 - 1) == "OK";
        p.detail = line.substr(t2 + 1);
        if (!p.ok) {
            return std::nullopt;
        }
        out.push_back(std::move(p));
    }
    if (out.size() < 4) {
        return std::nullopt;
    }
    return out;
}

} // namespace

std::vector<ProbeResult> RunProbes() {
    LogInit();
    if (auto cached = ReadCachedProbeResults()) {
        // All probes passed on a previous launch — skip the heavy ones (flash
        // wear + third-party connect). Datadir seeding is cheap and idempotent:
        // always refresh it, so a wiped datadir is re-seeded at next start.
        Logf("[probe] using cached probe results (all OK previously)");
        for (auto& p : *cached) {
            if (p.name == "datadir_layout") {
                p = ProbeDatadirLayout();
            }
        }
        return *cached;
    }
    Logf("[probe] starting AppContainer probes");
    std::vector<ProbeResult> out;
    out.push_back(ProbeLocalStateWrite());
    out.push_back(ProbeVirtualAlloc());
    out.push_back(ProbeOutboundTcp());
    out.push_back(ProbeDatadirLayout());
    for (const auto& p : out) {
        Logf("[probe] %s: %s — %s", p.name.c_str(), p.ok ? "OK" : "FAIL", p.detail.c_str());
    }
    // Persist machine-readable summary
    try {
        auto path = LocalStateFile(L"probe-results.txt");
        FILE* f = _wfopen(path.c_str(), L"w");
        if (f) {
            for (const auto& p : out) {
                fprintf(f, "%s\t%s\t%s\n", p.name.c_str(), p.ok ? "OK" : "FAIL", p.detail.c_str());
            }
            fclose(f);
        }
    } catch (...) {
    }
    return out;
}

std::string FormatProbeReport(const std::vector<ProbeResult>& results) {
    std::ostringstream oss;
    int ok = 0;
    for (const auto& p : results) {
        if (p.ok) {
            ++ok;
        }
        oss << (p.ok ? "[OK]   " : "[FAIL] ") << p.name << "\n  " << p.detail << "\n\n";
    }
    oss << "Summary: " << ok << "/" << results.size() << " probes passed.\n";
    oss << "After install: Dev Home → package → App type → Game.\n";
    oss << "Log: LocalState\\bitcoind.log\n";
    return oss.str();
}

} // namespace xbb
