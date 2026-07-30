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

## MSVC baseline (Windows — required)

| Field | Value |
|-------|--------|
| Date | |
| Host | e.g. Windows 11 VM (xllama UWP VM OK if NativeDesktop workload present) |
| VS version | VS 2026 18.3+ expected (Core preset `vs2026`) |
| vcpkg | VS-bundled / standalone path |
| Script | `.\scripts\build-msvc-baseline.ps1` |
| Flags | GUI=OFF wallet=OFF ZMQ=OFF IPC=OFF tests=ON |
| Result | **pending** — GHA `ci-msvc-baseline` on `windows-2025-vs2026` |
| `bitcoind -version` | |
| ctest summary | on `main` push (skipped on PR) |
| Binary path | Actions artifact `bitcoind-msvc-x64` |
| Regtest smoke | not run |
| Notes | See `docs/build-msvc-baseline.md` and `docs/ci.md` |

## Go / no-go for UWP port of this pin

- [x] Pin recorded and fetch script idempotent (`./scripts/fetch-bitcoin-core.sh`)
- [x] Same pin builds on Linux (daemon + CI workflow)
- [ ] MSVC Release `bitcoind` builds (CI workflow added; await green)
- [ ] Unit tests acceptable (or known failures listed)
- [x] Console defaults in `config/bitcoin.conf.console` still sensible for this version
- [x] GitHub Actions wired (`ci-linux`, `ci-msvc-baseline`)

**Decision:** pin **v31.1** accepted for port work; **MSVC gate** closes when `ci-msvc-baseline` is green on `main`.
