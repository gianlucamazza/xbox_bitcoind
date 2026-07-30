# UWP patches for Bitcoin Core (pin v31.1)

Applied by `scripts/apply-uwp-patches.sh` / `.ps1` after fetch.

| Patch | Purpose |
|-------|---------|
| `0001-lockedpool-no-virtuallock.patch` | No VirtualLock/Unlock on AppContainer |
| `0002-runcommand-noop-uwp.patch` | No `_wsystem` under UWP |
| `0003-bitcoind-embed-entry.patch` | `BitcoindMain` when `BITCOIND_EMBED` |
| `0004-vcpkg-drop-libevent-override.patch` | Use current vcpkg libevent (UWP fix) instead of 2.1.12#7 |

Do not commit a patched `third_party/bitcoin` tree — only these patch files.
| `0005-uwp-win32-api-shims.patch` | CreateFile2, no CSIDL, no exec/CreateProcess on UWP |
