# Spike: desktop Bitcoin Core baseline

**Pin:** v31.1 / `9be056a8a72b624dae9623b2f7bded92c2a21c91`  
**Source:** `third_party/bitcoin` via `config/bitcoin-core.pin`

## Linux smoke (Arch host)

| Field | Value |
|-------|--------|
| Date | 2026-07-30 |
| Host | Arch Linux, GCC 16.1.1, cmake 4.4, boost 1.91, libevent 2.1.13 |
| Flags | GUI=OFF wallet=OFF ZMQ=OFF IPC=OFF TESTS=OFF (daemon+cli only) |
| Script | manual cmake (same flags as `build-linux-smoke.sh`; tests skipped for wall time) |
| Result | **PASS** (~26 min wall) |
| `bitcoind -version` | `Bitcoin Core daemon version v31.1 bitcoind` |
| Binary | `third_party/bitcoin/build-linux-smoke/bin/bitcoind` |
| ctest | not run (TESTS=OFF) |
| Notes | Harmless `-Wstringop-overflow` noise from system Boost multi_index + GCC 16; link OK |

## Linux CI (`ci-linux`)

| Field | Value |
|-------|--------|
| Date | 2026-07-30 |
| Run | https://github.com/gianlucamazza/xbox_bitcoind/actions/runs/30582397368 |
| Result | **PASS** (lint + smoke, ~4 min after fix commit) |
| Artifact | `bitcoind-linux-x64` |

## MSVC baseline (GitHub Actions)

| Field | Value |
|-------|--------|
| Date | 2026-07-30 |
| Host | GHA `windows-2025-vs2026` (image 20260714.173.1) |
| VS / vcpkg | VS 2026 + `VCPKG_INSTALLATION_ROOT` |
| Script | `.\scripts\build-msvc-baseline.ps1` (tests on for `main`) |
| Flags | GUI=OFF wallet=OFF ZMQ=OFF IPC=OFF tests=**ON** |
| Result | **PASS** (~27 min wall cold) |
| `bitcoind -version` | `Bitcoin Core daemon version v31.1 bitcoind` |
| ctest | **137/137 passed** (258 s) |
| Binary | Actions artifact `bitcoind-msvc-x64` (`bin/Release/bitcoind.exe`) |
| Run | https://github.com/gianlucamazza/xbox_bitcoind/actions/runs/30582397400 |
| Regtest smoke | not run |
| Notes | Pin gate closed via CI; no local Windows VM required |

## Go / no-go for UWP port of this pin

- [x] Pin recorded and fetch script idempotent
- [x] Same pin builds on Linux (local + CI)
- [x] MSVC Release `bitcoind` builds (CI)
- [x] Unit tests acceptable (**137/137** on MSVC)
- [x] Console defaults in `config/bitcoin.conf.console` still sensible
- [x] GitHub Actions green on `main` (`ci-linux` + `ci-msvc-baseline`)

**Decision:** pin **v31.1** **GO** for UWP port work. Next: API matrix + Hello-UWP.
