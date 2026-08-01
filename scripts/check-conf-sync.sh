#!/usr/bin/env bash
# check-conf-sync.sh — assert the embedded bitcoin.conf fallback in the UWP host
# (uwp/node_host.cpp — single home since the probes reuse SeedDatadirConf) matches the
# canonical config/bitcoin.conf.console. Used by ci-linux lint.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONF="${ROOT}/config/bitcoin.conf.console"

conf_keys() {
	grep -E '^[a-z]+=' "${CONF}" | sort
}

# Extract the key=value lines from the C string literal assigned to `content`
# (concatenated "..." fragments with embedded \n).
cpp_keys() {
	local file="$1"
	python3 - "$file" <<'PY'
import re
import sys

src = open(sys.argv[1], encoding="utf-8").read()
m = re.search(r"const char\* content =\s*(.*?);", src, re.S)
if not m:
    sys.exit(f"{sys.argv[1]}: embedded conf literal not found")
literal = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))
for line in literal.split("\\n"):
    if re.match(r"^[a-z]+=", line):
        print(line)
PY
}

CPP="uwp/node_host.cpp"
if ! diff <(conf_keys) <(cpp_keys "${ROOT}/${CPP}" | sort) >/dev/null; then
	echo "MISMATCH: ${CPP} embedded conf fallback != config/bitcoin.conf.console" >&2
	diff <(conf_keys) <(cpp_keys "${ROOT}/${CPP}" | sort) >&2 || true
	echo "Keep the embedded fallback in sync with config/bitcoin.conf.console" >&2
	exit 1
fi
echo "conf-sync OK: embedded fallback matches config/bitcoin.conf.console"
