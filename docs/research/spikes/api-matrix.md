# Spike: Bitcoin Core Win32 API surface vs Xbox UWP

**Pin:** v31.1 @ `9be056a8…`  
**Date:** 2026-07-30  
**Method:** static scan of `third_party/bitcoin/src` + xllama UWP constraints (same Series S)

Legend: **OK** usable / **RISK** needs probe or shim / **BLOCK** known AppContainer gap / **N/A** disabled in v1.

## Matrix

| Area | Core usage (v31.1) | UWP / AppContainer | Plan |
|------|--------------------|--------------------|------|
| **TCP client (outbound P2P)** | Winsock via `util/sock.cpp`, `net.cpp` (`connect`, `select`, `ioctlsocket` FIONBIO) | **OK** with `internetClient` (+ `privateNetworkClientServer`) | Start `listen=0`; probe outbound in Hello-UWP |
| **TCP listen / accept** | `Accept`, bind listeners in `net.cpp` | **RISK** — may work with capabilities; NAT still blocks inbound | Keep `listen=0` for v1 |
| **DNS / seeds** | libevent + system resolver | **OK** (same as xllama downloads) | Default seeds; no special work |
| **HTTP RPC server** | libevent `evhttp` in `httpserver.cpp` | **RISK** — loopback only; capability + bind `127.0.0.1` | Bind local; UI uses in-process or named pipe later |
| **Filesystem (datadir)** | `fs::` + `GetSpecialFolderPath(CSIDL_APPDATA/LOCAL_APPDATA)` default paths (`common/args.cpp`) | **RISK** — CSIDL desktop paths wrong in AppContainer; must force `-datadir` under LocalState | Host sets absolute LocalState path; ignore default AppData |
| **std::filesystem walks** | Widespread | **RISK** — `weakly_canonical` / root walk issues known on Xbox (xllama §8) | Avoid path canonicalization across Q:\; use lexical paths under LocalState |
| **LevelDB mmap** | Windows env uses limited mmap (`leveldb/util/env_windows.cc`, `MaxMmaps`) | **RISK** — may fall back; xllama saw no POSIX mmap | Prefer buffered I/O; measure; set mmap limit 0 if needed |
| **Locked pages / secrets** | `VirtualAlloc` + `VirtualLock` (`support/lockedpool.cpp`); `SecureZeroMemory` | **RISK** — `VirtualLock` **not in UWP partition** (won't link); need AppContainer-safe locked pool or no-lock path | Patch Core lockedpool for UWP; probe `VirtualAlloc` only (scaffold) |
| **Entropy** | Windows crypto APIs via util random | **OK** expected | Smoke only |
| **Threads** | std::thread / OS threads | **OK** — Series S ~6–7 usable cores | Cap via config if needed |
| **Process / spawn** | `CreateProcessW` in `util/subprocess.h`; `_wsystem` in `common/system.cpp` | **BLOCK / N/A** for daemon path — external signer / runCommand | Disable external signer; no `-alertnotify`/`-blocknotify` shells in v1 |
| **GetModuleFileName** | `util/exec.cpp`, crash paths | **RISK** — returns package path; usually OK | Use for logging only |
| **GUI / Qt** | optional | **N/A** | `BUILD_GUI=OFF` |
| **Wallet / SQLite** | optional | **N/A** v1 | `ENABLE_WALLET=OFF` until node works |
| **ZMQ / UPnP / IPC** | optional | **N/A** | already off in pin |
| **2 GB single-file** | `blk*.dat` max 128 MB | **OK** | Avoid huge bootstrap monofiles |
| **Long-running process** | daemon loop | **RISK** — suspension if not Game / focused | Mark package **Game**; keep foreground during IBD |

## Priority order for port work

1. **Datadir injection** — never rely on CSIDL AppData; always LocalState (and optional USB later).  
2. **Outbound sockets** — Hello-UWP TCP client + then link Core net stack.  
3. **VirtualAlloc / LevelDB I/O** — small write/read probe under LocalState.  
4. **RPC or status surface** — file log + simple UI first; HTTP RPC optional.  
5. **Strip / guard** subprocess and notify hooks.  

## Host integration sketch

```
UWP App (Game)
  └── set -datadir=<LocalState>\bitcoin
  └── set -conf=<packaged or LocalState bitcoin.conf>
  └── bitcoind main() in-process OR as linked library
  └── no CreateProcess children
```

## Console probes (Hello-UWP on Series S)

**Date:** 2026-07-30  
**Package:** `GianlucaMazza.xboxbitcoind_0.1.0.5_x64__m0e4707sws2jw`  
**MSIX:** CI run https://github.com/gianlucamazza/xbox_bitcoind/actions/runs/30585344358  

| Probe | Result |
|-------|--------|
| `localstate_write` | **OK** — 16 MiB / 4 files under LocalState\probe |
| `virtual_alloc` | **OK** — 64 MiB VirtualAlloc+touch |
| `outbound_tcp` | **OK** — TCP IPv4 to one.one.one.one:80, 222 bytes |
| `datadir_layout` | **OK** — LocalState\bitcoin created |

Still open: multi-GB scale, LevelDB, Game-class RAM ceiling, full IBD.

## Status

| Item | State |
|------|--------|
| Static matrix | **done** |
| Console probes | **4/4 PASS** (scaffold) |
| Go for engineering | **GO** — link Core next |
