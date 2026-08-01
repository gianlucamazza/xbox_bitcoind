// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

// Minimal extractors for flat JSON-RPC result objects (not a full parser).
// Pure C++ (no WinRT) so it is host-testable: scripts/test-rpc-client.sh.
// Limits: nested objects are skipped only by JsonResultObject's brace scan;
// string escapes are unescaped naively (\" \\ pass through, \n stays "n").

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xbb {

// Position of the value for "key": — first non-space after the colon.
inline std::optional<size_t> JsonValuePos(const std::string& json, const char* key) {
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
    if (pos >= json.size()) {
        return std::nullopt;
    }
    return pos;
}

// Parse a JSON string starting at json[pos] == '"'; advances pos past the closing quote.
inline std::optional<std::string> ParseJsonStringAt(const std::string& json, size_t& pos) {
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
    if (pos >= json.size()) {
        return std::nullopt; // unterminated
    }
    ++pos; // closing quote
    return out;
}

inline std::optional<std::string> JsonStringField(const std::string& json, const char* key) {
    auto pos = JsonValuePos(json, key);
    if (!pos) {
        return std::nullopt;
    }
    size_t p = *pos;
    return ParseJsonStringAt(json, p);
}

inline std::optional<int64_t> JsonIntField(const std::string& json, const char* key) {
    auto vp = JsonValuePos(json, key);
    if (!vp) {
        return std::nullopt;
    }
    const size_t pos = *vp;
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

inline std::optional<double> JsonDoubleField(const std::string& json, const char* key) {
    auto vp = JsonValuePos(json, key);
    if (!vp) {
        return std::nullopt;
    }
    const size_t pos = *vp;
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

inline std::optional<bool> JsonBoolField(const std::string& json, const char* key) {
    auto vp = JsonValuePos(json, key);
    if (!vp) {
        return std::nullopt;
    }
    const size_t pos = *vp;
    if (json.compare(pos, 4, "true") == 0) {
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        return false;
    }
    return std::nullopt;
}

// "warnings" comes as a plain string on older Core / -deprecatedrpc=warnings and as an
// array of strings on v31.x getblockchaininfo/getnetworkinfo. Accept both; join arrays.
inline std::optional<std::string> JsonWarningsField(const std::string& json,
                                                    const char* key = "warnings") {
    auto vp = JsonValuePos(json, key);
    if (!vp) {
        return std::nullopt;
    }
    size_t pos = *vp;
    if (json[pos] == '"') {
        return ParseJsonStringAt(json, pos);
    }
    if (json[pos] != '[') {
        return std::nullopt;
    }
    ++pos;
    std::vector<std::string> items;
    bool closed = false;
    while (pos < json.size()) {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
            ++pos;
        }
        if (pos >= json.size()) {
            break;
        }
        if (json[pos] == ']') {
            closed = true;
            break;
        }
        auto item = ParseJsonStringAt(json, pos);
        if (!item) {
            return std::nullopt;
        }
        items.push_back(std::move(*item));
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
            ++pos;
        }
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
        }
    }
    if (!closed) {
        return std::nullopt; // unterminated array
    }
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) {
            out += " · ";
        }
        out += items[i];
    }
    return out;
}

// Extract "result" value substring: object (string-aware brace matching), null, or scalar.
inline std::optional<std::string> JsonResultObject(const std::string& body) {
    auto vp = JsonValuePos(body, "result");
    if (!vp) {
        return std::nullopt;
    }
    size_t pos = *vp;
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
    bool in_string = false;
    const size_t start = pos;
    for (; pos < body.size(); ++pos) {
        const char c = body[pos];
        if (in_string) {
            if (c == '\\') {
                ++pos; // skip escaped char
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                return body.substr(start, pos - start + 1);
            }
        }
    }
    return std::nullopt;
}

// Scalar int64 result ("uptime", "getconnectioncount"): trims and parses.
inline std::optional<int64_t> JsonScalarInt64(const std::string& result) {
    size_t pos = 0;
    while (pos < result.size() && std::isspace(static_cast<unsigned char>(result[pos]))) {
        ++pos;
    }
    try {
        return std::stoll(result.substr(pos));
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace xbb
