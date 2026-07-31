// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "rpc_client.h"

#include "log.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <cctype>
#include <fstream>
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

std::optional<std::string> ReadCookie(const std::string& datadir) {
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

std::string Base64(const std::string& s) {
    auto buf = CryptographicBuffer::ConvertStringToBinary(
        winrt::to_hstring(s), BinaryStringEncoding::Utf8);
    return winrt::to_string(CryptographicBuffer::EncodeToBase64String(buf));
}

// Minimal extractors for flat JSON result objects (not a full parser).
std::optional<std::string> JsonStringField(const std::string& json, const char* key) {
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] != '"') {
        return std::nullopt;
    }
    ++pos;
    std::string out;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            out.push_back(json[pos + 1]);
            pos += 2;
            continue;
        }
        out.push_back(json[pos++]);
    }
    return out;
}

std::optional<int64_t> JsonIntField(const std::string& json, const char* key) {
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    size_t end = pos;
    if (end < json.size() && (json[end] == '-' || json[end] == '+')) {
        ++end;
    }
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
        ++end;
    }
    if (end == pos) {
        return std::nullopt;
    }
    try {
        return std::stoll(json.substr(pos, end - pos));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> JsonDoubleField(const std::string& json, const char* key) {
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    size_t end = pos;
    while (end < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '.' ||
            json[end] == 'e' || json[end] == 'E' || json[end] == '+' || json[end] == '-')) {
        ++end;
    }
    if (end == pos) {
        return std::nullopt;
    }
    try {
        return std::stod(json.substr(pos, end - pos));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> JsonBoolField(const std::string& json, const char* key) {
    const std::string pat = std::string("\"") + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (json.compare(pos, 4, "true") == 0) {
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        return false;
    }
    return std::nullopt;
}

// Extract "result" object substring (from first { after "result" to matching }).
std::optional<std::string> JsonResultObject(const std::string& body) {
    auto pos = body.find("\"result\"");
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = body.find(':', pos);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) {
        ++pos;
    }
    if (pos >= body.size()) {
        return std::nullopt;
    }
    if (body[pos] == 'n' && body.compare(pos, 4, "null") == 0) {
        return std::string("null");
    }
    if (body[pos] != '{') {
        // Scalar result — return rest until , or }
        size_t end = pos;
        while (end < body.size() && body[end] != ',' && body[end] != '}') {
            ++end;
        }
        return body.substr(pos, end - pos);
    }
    int depth = 0;
    size_t start = pos;
    for (; pos < body.size(); ++pos) {
        if (body[pos] == '{') {
            ++depth;
        } else if (body[pos] == '}') {
            --depth;
            if (depth == 0) {
                return body.substr(start, pos - start + 1);
            }
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> RpcCall(const std::string& datadir_utf8, const std::string& method,
                                   const std::string& params_json) {
    auto cookie = ReadCookie(datadir_utf8);
    if (!cookie) {
        return std::nullopt;
    }

    try {
        HttpClient client;
        auto auth = Base64(*cookie);
        client.DefaultRequestHeaders().Authorization(
            HttpCredentialsHeaderValue(L"Basic", winrt::to_hstring(auth)));

        std::ostringstream body;
        body << "{\"jsonrpc\":\"1.0\",\"id\":\"xbb\",\"method\":\"" << method << "\",\"params\":"
             << params_json << "}";

        HttpStringContent content(winrt::to_hstring(body.str()), UnicodeEncoding::Utf8,
                                  L"application/json");
        Uri uri(L"http://127.0.0.1:8332/");
        HttpResponseMessage resp = client.PostAsync(uri, content).get();
        if (resp.StatusCode() != HttpStatusCode::Ok) {
            return std::nullopt;
        }
        auto hstr = resp.Content().ReadAsStringAsync().get();
        return winrt::to_string(hstr);
    } catch (winrt::hresult_error const& e) {
        Logf("[rpc] call %s failed: 0x%08X", method.c_str(),
             static_cast<unsigned>(e.code().value));
        return std::nullopt;
    } catch (...) {
        Logf("[rpc] call %s failed: unknown", method.c_str());
        return std::nullopt;
    }
}

std::optional<BlockchainInfo> RpcGetBlockchainInfo(const std::string& datadir_utf8) {
    auto body = RpcCall(datadir_utf8, "getblockchaininfo");
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
    if (auto w = JsonStringField(*result, "warnings")) {
        info.warnings = *w;
    }
    return info;
}

std::optional<NetworkInfo> RpcGetNetworkInfo(const std::string& datadir_utf8) {
    NetworkInfo n;
    bool got = false;
    if (auto body = RpcCall(datadir_utf8, "getconnectioncount")) {
        auto result = JsonResultObject(*body);
        if (result) {
            try {
                std::string r = *result;
                while (!r.empty() && std::isspace(static_cast<unsigned char>(r.front()))) {
                    r.erase(r.begin());
                }
                n.connections = static_cast<int>(std::stoll(r));
                got = true;
            } catch (...) {
            }
        }
    }
    if (auto body = RpcCall(datadir_utf8, "getnetworkinfo")) {
        if (auto result = JsonResultObject(*body)) {
            if (auto c = JsonIntField(*result, "connections")) {
                n.connections = static_cast<int>(*c);
                got = true;
            }
            if (auto na = JsonBoolField(*result, "networkactive")) {
                n.network_active = *na;
            }
            if (auto sv = JsonStringField(*result, "subversion")) {
                n.subversion = *sv;
            }
            got = true;
        }
    }
    if (!got) {
        return std::nullopt;
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
    try {
        std::string r = *result;
        while (!r.empty() && std::isspace(static_cast<unsigned char>(r.front()))) {
            r.erase(r.begin());
        }
        return std::stoll(r);
    } catch (...) {
        return std::nullopt;
    }
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
    const auto size = static_cast<size_t>(in.tellg());
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
