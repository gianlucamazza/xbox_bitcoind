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
| **Locked pages / secrets** | `VirtualAlloc` + `VirtualLock` (`support/lockedpool.cpp`); `SecureZeroMemory` | **RISK** — `VirtualLock` may fail under AppContainer (non-fatal in Core); alloc should work | Treat lock failure as OK (Core already does); probe alloc |
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

## Open probes (need console Hello-UWP)

- [ ] Outbound TCP to public host from UWP  
- [ ] Multi-GB multi-file write under LocalState  
- [ ] `VirtualAlloc` + optional `VirtualLock`  
- [ ] LevelDB open/write under LocalState  
- [ ] RAM ceiling with Game package during synthetic load  

## Status

| Item | State |
|------|--------|
| Static matrix | **done** |
| Console probes | pending (Hello-UWP) |
| Go for engineering | **GO** with listed risks |
