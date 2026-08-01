#!/usr/bin/env bash
# test-rpc-client.sh — compile+run pure JSON extractor unit checks (no WinRT / no Xbox).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HDR="${ROOT}/uwp/json_extract.h"
SRC="$(mktemp "${TMPDIR:-/tmp}/xbb-json-extract-test.XXXXXX.cpp")"
BIN="$(mktemp "${TMPDIR:-/tmp}/xbb-json-extract-test.XXXXXX")"
trap 'rm -f "${SRC}" "${BIN}"' EXIT

if [[ ! -f "${HDR}" ]]; then
	echo "missing ${HDR}" >&2
	exit 1
fi

cat >"${SRC}" <<'CPP'
#include "json_extract.h"
#include <cstdio>
#include <cstdlib>

using namespace xbb;

static void expect(bool ok, const char* msg) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

int main() {
    // Fields — realistic getblockchaininfo excerpt
    const std::string chain =
        R"({"result":{"chain":"main","blocks":368000,"headers":907000,)"
        R"("verificationprogress":5.7e-02,"initialblockdownload":true,"pruned":true,)"
        R"("size_on_disk":712345678,"mediantime":1425000000,"warnings":[]},"error":null,"id":"xbb"})";
    {
        auto r = JsonResultObject(chain);
        expect(r.has_value(), "result object extracted");
        expect(JsonStringField(*r, "chain") == std::string("main"), "chain string");
        expect(JsonIntField(*r, "blocks") == 368000, "blocks int");
        expect(JsonIntField(*r, "headers") == 907000, "headers int");
        auto vp = JsonDoubleField(*r, "verificationprogress");
        expect(vp && *vp > 0.056 && *vp < 0.058, "verificationprogress exponent double");
        expect(JsonBoolField(*r, "initialblockdownload") == true, "ibd bool");
        expect(JsonIntField(*r, "size_on_disk") == 712345678, "size_on_disk int64");
        expect(JsonWarningsField(*r) == std::string(""), "empty warnings array -> empty string");
        expect(!JsonIntField(*r, "missing").has_value(), "missing key -> nullopt");
    }
    // warnings: array form (Core v31.x) and legacy string form
    {
        const std::string arr = R"({"warnings":["w1","w2 spaced"]})";
        expect(JsonWarningsField(arr) == std::string("w1 · w2 spaced"), "warnings array joined");
        const std::string one = R"({"warnings":["only"]})";
        expect(JsonWarningsField(one) == std::string("only"), "warnings single item");
        const std::string legacy = R"({"warnings":"old style"})";
        expect(JsonWarningsField(legacy) == std::string("old style"), "warnings legacy string");
        const std::string ws = R"({"warnings": [ "a" , "b" ]})";
        expect(JsonWarningsField(ws) == std::string("a · b"), "warnings array with spaces");
        const std::string bad = R"({"warnings":[)";
        expect(!JsonWarningsField(bad).has_value(), "unterminated array -> nullopt");
    }
    // Braces inside string values must not break the result-object scan
    {
        const std::string tricky =
            R"({"result":{"subversion":"/Satoshi:31.1.0/{}}","connections":8},"error":null})";
        auto r = JsonResultObject(tricky);
        expect(r.has_value(), "braces in string: object extracted");
        expect(JsonIntField(*r, "connections") == 8, "braces in string: trailing field kept");
        expect(JsonStringField(*r, "subversion") == std::string("/Satoshi:31.1.0/{}}"),
               "braces in string: value intact");
    }
    // Escaped quote inside a string
    {
        const std::string esc = R"({"result":{"msg":"say \"hi\"","n":1},"error":null})";
        auto r = JsonResultObject(esc);
        expect(r.has_value(), "escaped quote: object extracted");
        expect(JsonStringField(*r, "msg") == std::string("say \"hi\""), "escaped quote unescaped");
        expect(JsonIntField(*r, "n") == 1, "escaped quote: sibling field");
    }
    // Scalar and null results
    {
        expect(JsonResultObject(R"({"result":12345,"error":null})") == std::string("12345"),
               "scalar result");
        expect(JsonResultObject(R"({"result":null,"error":null})") == std::string("null"),
               "null result");
        auto scalar = JsonScalarInt64("  42");
        expect(scalar == 42, "scalar int64 trims");
        expect(!JsonScalarInt64("x").has_value(), "scalar int64 garbage -> nullopt");
    }
    // Negative int
    {
        expect(JsonIntField(R"({"timeoffset":-3})", "timeoffset") == -3, "negative int");
    }
    std::puts("json_extract tests OK");
    return 0;
}
CPP

g++ -std=c++20 -O0 -Wall -Wextra -Werror -I"${ROOT}/uwp" -o "${BIN}" "${SRC}"
"${BIN}"
