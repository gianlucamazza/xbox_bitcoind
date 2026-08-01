// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "rpc_client.h"

#include "json_extract.h"
#include "log.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <cctype>
#include <fstream>
#include <mutex>
#include <sstream>

namespace xbb {
namespace {

using winrt::Windows::Foundation::Uri;
using winrt::Windows::Security::Cryptography::BinaryStringEncoding;
using winrt::Windows::Security::Cryptography::CryptographicBuffer;
using winrt::Windows::Storage::Streams::UnicodeEncoding;
using winrt::Windows::Web::Http::HttpClient;
using winrt::Windows::Web::Http::HttpResponseMessage;
using winrt::Windows::Web::Http::HttpStatusCode;
using winrt::Windows::Web::Http::HttpStringContent;
using winrt::Windows::Web::Http::Headers::HttpCredentialsHeaderValue;

std::optional<std::string> ReadCookieFile(const std::string& datadir) {
    const std::string path = datadir + "\\.cookie";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::string line;
    if (!std::getline(in, line)) {
        return std::nullopt;
    }
    // Format: __cookie__:<password>
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    if (line.find(':') == std::string::npos) {
        return std::nullopt;
    }
    return line;
}

// Cookie cache: bitcoind regenerates .cookie per run, so re-reading it on every RPC
// (5 calls / 2s tick) is wasted I/O. Invalidated on HTTP 401.
std::mutex g_cookie_mu;
std::string g_cookie_cached;

std::optional<std::string> CachedCookie(const std::string& datadir) {
    std::lock_guard lock(g_cookie_mu);
    if (!g_cookie_cached.empty()) {
        return g_cookie_cached;
    }
    if (auto c = ReadCookieFile(datadir)) {
        g_cookie_cached = *c;
        return g_cookie_cached;
    }
    return std::nullopt;
}

void InvalidateCookie() {
    std::lock_guard lock(g_cookie_mu);
    g_cookie_cached.clear();
}

std::string Base64(const std::string& s) {
    auto buf = CryptographicBuffer::ConvertStringToBinary(
        winrt::to_hstring(s), BinaryStringEncoding::Utf8);
    return winrt::to_string(CryptographicBuffer::EncodeToBase64String(buf));
}

// JSON field extractors live in json_extract.h (host-testable).

} // namespace

std::optional<std::string> RpcCall(const std::string& datadir_utf8, const std::string& method,
                                   const std::string& params_json, RpcError* err) {
    auto set_err = [err](RpcError e) {
        if (err) {
            *err = e;
        }
    };
    set_err(RpcError::None);

    std::ostringstream body;
    body << "{\"jsonrpc\":\"1.0\",\"id\":\"xbb\",\"method\":\"" << method << "\",\"params\":"
         << params_json << "}";

    // One retry on 401: the cached cookie may belong to a previous bitcoind run.
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto cookie = CachedCookie(datadir_utf8);
        if (!cookie) {
            set_err(RpcError::NoCookie);
            return std::nullopt;
        }
        try {
            HttpClient client;
            auto auth = Base64(*cookie);
            client.DefaultRequestHeaders().Authorization(
                HttpCredentialsHeaderValue(L"Basic", winrt::to_hstring(auth)));

            HttpStringContent content(winrt::to_hstring(body.str()), UnicodeEncoding::Utf8,
                                      L"application/json");
            Uri uri(L"http://127.0.0.1:8332/");
            HttpResponseMessage resp = client.PostAsync(uri, content).get();
            const auto status = resp.StatusCode();
            if (status == HttpStatusCode::Unauthorized) {
                InvalidateCookie();
                if (attempt == 0) {
                    continue;
                }
                set_err(RpcError::AuthFailed);
                return std::nullopt;
            }
            if (status == HttpStatusCode::ServiceUnavailable) {
                set_err(RpcError::WarmingUp);
                return std::nullopt;
            }
            if (status != HttpStatusCode::Ok) {
                set_err(RpcError::HttpError);
                return std::nullopt;
            }
            auto hstr = resp.Content().ReadAsStringAsync().get();
            return winrt::to_string(hstr);
        } catch (winrt::hresult_error const& e) {
            Logf("[rpc] call %s failed: 0x%08X", method.c_str(),
                 static_cast<unsigned>(e.code().value));
            set_err(RpcError::Network);
            return std::nullopt;
        } catch (...) {
            Logf("[rpc] call %s failed: unknown", method.c_str());
            set_err(RpcError::Network);
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<BlockchainInfo> RpcGetBlockchainInfo(const std::string& datadir_utf8,
                                                   RpcError* err) {
    auto body = RpcCall(datadir_utf8, "getblockchaininfo", "[]", err);
    if (!body) {
        return std::nullopt;
    }
    auto result = JsonResultObject(*body);
    if (!result || *result == "null") {
        return std::nullopt;
    }
    BlockchainInfo info;
    if (auto c = JsonStringField(*result, "chain")) {
        info.chain = *c;
    }
    if (auto b = JsonIntField(*result, "blocks")) {
        info.blocks = static_cast<int>(*b);
    }
    if (auto h = JsonIntField(*result, "headers")) {
        info.headers = static_cast<int>(*h);
    }
    if (auto p = JsonDoubleField(*result, "verificationprogress")) {
        info.verification_progress = *p;
    }
    if (auto ibd = JsonBoolField(*result, "initialblockdownload")) {
        info.initial_block_download = *ibd;
    }
    if (auto pr = JsonBoolField(*result, "pruned")) {
        info.pruned = *pr;
    }
    if (auto sz = JsonIntField(*result, "size_on_disk")) {
        info.size_on_disk = *sz;
    }
    if (auto pts = JsonIntField(*result, "prune_target_size")) {
        info.prune_target_size = *pts;
    }
    if (auto mt = JsonIntField(*result, "mediantime")) {
        info.mediantime = *mt;
    }
    if (auto w = JsonWarningsField(*result)) {
        info.warnings = *w;
    }
    return info;
}

std::optional<NetworkInfo> RpcGetNetworkInfo(const std::string& datadir_utf8) {
    // getnetworkinfo carries connections too — no separate getconnectioncount call.
    auto body = RpcCall(datadir_utf8, "getnetworkinfo");
    if (!body) {
        return std::nullopt;
    }
    auto result = JsonResultObject(*body);
    if (!result || *result == "null") {
        return std::nullopt;
    }
    NetworkInfo n;
    if (auto c = JsonIntField(*result, "connections")) {
        n.connections = static_cast<int>(*c);
    }
    if (auto na = JsonBoolField(*result, "networkactive")) {
        n.network_active = *na;
    }
    if (auto sv = JsonStringField(*result, "subversion")) {
        n.subversion = *sv;
    }
    return n;
}

std::optional<MempoolInfo> RpcGetMempoolInfo(const std::string& datadir_utf8) {
    auto body = RpcCall(datadir_utf8, "getmempoolinfo");
    if (!body) {
        return std::nullopt;
    }
    auto result = JsonResultObject(*body);
    if (!result || *result == "null") {
        return std::nullopt;
    }
    MempoolInfo m;
    if (auto s = JsonIntField(*result, "size")) {
        m.size = static_cast<int>(*s);
    }
    if (auto b = JsonIntField(*result, "bytes")) {
        m.bytes = *b;
    }
    if (auto u = JsonIntField(*result, "usage")) {
        m.usage = *u;
    }
    return m;
}

std::optional<int64_t> RpcUptime(const std::string& datadir_utf8) {
    auto body = RpcCall(datadir_utf8, "uptime");
    if (!body) {
        return std::nullopt;
    }
    auto result = JsonResultObject(*body);
    if (!result) {
        return std::nullopt;
    }
    return JsonScalarInt64(*result);
}

bool RpcStop(const std::string& datadir_utf8) {
    auto body = RpcCall(datadir_utf8, "stop");
    return body.has_value();
}

std::string ReadDebugLogTail(const std::string& datadir_utf8, size_t max_lines) {
    const std::string path = datadir_utf8 + "\\debug.log";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    in.seekg(0, std::ios::end);
    const auto end_pos = in.tellg();
    if (end_pos <= 0) {
        return {}; // empty file or tellg failure
    }
    const auto size = static_cast<size_t>(end_pos);
    const size_t cap = 256 * 1024;
    const size_t read_sz = size > cap ? cap : size;
    in.seekg(static_cast<std::streamoff>(size - read_sz), std::ios::beg);
    std::string buf(read_sz, '\0');
    in.read(buf.data(), static_cast<std::streamsize>(read_sz));
    buf.resize(static_cast<size_t>(in.gcount()));

    std::vector<std::string> lines;
    std::istringstream ss(buf);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    if (lines.size() > max_lines) {
        lines.erase(lines.begin(), lines.end() - static_cast<std::ptrdiff_t>(max_lines));
    }
    std::ostringstream out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) {
            out << '\n';
        }
        out << lines[i];
    }
    return out.str();
}

} // namespace xbb
