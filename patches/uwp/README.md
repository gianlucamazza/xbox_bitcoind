# UWP patches for Bitcoin Core (pin v31.1)

Applied by `scripts/apply-uwp-patches.sh` / `.ps1` after fetch.

These adapt AppContainer API surface only — **not** compiler workarounds.
Bitcoin Core v31.1 requires **Visual Studio 2026 (18.3+)**; build Core UWP
with that toolchain (same as the MSVC desktop baseline).

| Patch | Purpose |
|-------|---------|
| `0001-lockedpool-no-virtuallock.patch` | No VirtualLock/Unlock on AppContainer |
| `0002-runcommand-noop-uwp.patch` | No `_wsystem` under UWP |
| `0003-bitcoind-embed-entry.patch` | `BitcoindMain` when `BITCOIND_EMBED` |
| `0004-vcpkg-drop-libevent-override.patch` | Use current vcpkg libevent (UWP fix) instead of 2.1.12#7 |
| `0005-uwp-win32-api-shims.patch` | CreateFile2, windows.h for file APIs, no CSIDL/exec/GetModuleFileName |
| `0006-uwp-subprocess-stub.patch` | CreateProcess/CreatePipe stubs after `windows.h` |
| `0007-uwp-netif-no-gateway-route.patch` | No GetBestRoute2 / MIB_IPFORWARD_ROW2 under UWP |
| `0008-uwp-bitcoin-embed-static-lib.patch` | WindowsStore: `bitcoin_embed` static lib instead of bitcoind.exe |
| `0009-uwp-leveldb-durable-nommap.patch` | LevelDB: no mmap, mmap fallback, WRITE_THROUGH on UWP |
| `0010-uwp-faster-db-write-interval.patch` | Flush chainstate/block index every 30–60s on UWP (not 50–70min) |

Do not commit a patched `third_party/bitcoin` tree — only these patch files.
