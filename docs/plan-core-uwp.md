# Plan: Bitcoin Core → UWP full node (complete)

## Goal

Run a **pruned validating `bitcoind`** inside the Hello-UWP package on Series S
Dev Mode, using pin **v31.1**, with status UI + logs.

## Architecture

```
xbox_bitcoind.exe (UWP AppContainer, Game class)
├── UI (MainPage) — status / probe report / node state
├── probes — AppContainer capability checks
└── node_host → BitcoindMain(argc, argv)
        │
        ├── -datadir=<LocalState>\bitcoin
        ├── -conf=<LocalState>\bitcoin\bitcoin.conf
        ├── prune=550 listen=0 server=1 dbcache=256
        └── links: bitcoin_node + util + common + deps (UWP)
```

**In-process only** (no `CreateProcess`). Shutdown via `Interrupt`/`Shutdown` on app exit.

## Work packages

| # | Package | Deliverable |
|---|---------|-------------|
| 1 | **Patches** | lockedpool, runCommand, optional fs; `BitcoindMain` embed entry |
| 2 | **Core UWP build** | `build-core-uwp.ps1` → static libs via CMake + `x64-uwp` vcpkg |
| 3 | **Embed + UI** | `node_host` starts/stops node thread; UI shows running/errors |
| 4 | **Package** | `build-uwp.ps1 -WithCore` links libs into MSIX |
| 5 | **CI** | `build-uwp.yml` builds core+app; artifact MSIX |
| 6 | **Console** | deploy, Game class, regtest then mainnet pruned |

## Patch set (`patches/uwp/`)

1. `0001` — no `VirtualLock`/`Unlock`/`GetProcessWorkingSetSize` on UWP  
2. `0002` — `runCommand` no-op under `WINAPI_FAMILY_APP`  
3. `0003` — `BitcoindMain` when `BITCOIND_EMBED`  
4. `0004` — drop pinned libevent so vcpkg UWP port can build  
5. `0005` — CreateFile2 / no CSIDL / no exec / no GetModuleFileName  
6. `0006` — subprocess CreateProcess/CreatePipe stubs **after** `windows.h`  
7. `0007` — netif: no IP Helper gateway route APIs on UWP  
8. Apply via `scripts/apply-uwp-patches.sh` after fetch

## Build (Windows CI)

```
fetch pin → apply patches → vcpkg x64-uwp (boost multi_index/signals2, libevent)
  → cmake WindowsStore / x64-uwp → build static node stack
  → MSBuild UWP app WITH_CORE=1 → sign MSIX
```

## Config defaults

`config/bitcoin.conf.console` + host-forced:

```
-datadir=<LocalState>\bitcoin
-printtoconsole=0
-debuglogfile=debug.log
```

## Success criteria

- [x] Plan + patches + embed + CI wiring  
- [x] Hello-UWP MSIX on Series S (probes 4/4)  
- [x] vcpkg UWP deps path (libevent UWP fix via drop override)  
- [ ] Core static libs **full** compile for UWP on GHA (in progress — remaining desktop APIs patched incrementally)  
- [ ] MSIX with Core linked starts node thread without crash  
- [ ] regtest / mainnet pruned progress  
- [ ] logs in LocalState  

### Implementation status (2026-07-31)

| Layer | Status |
|-------|--------|
| Scaffold UI + probes | **Done**, deployed on console |
| Desktop Core pin v31.1 | **Done**, MSVC 137/137 |
| UWP patches 0001–0007 | **Done** in `patches/uwp/` |
| `build-core-uwp.ps1` / `-WithCore` | **Done** (target `bitcoin_node`, external signer off) |
| CI `uwp-core` | **Iterating** (BOOL/subprocess fix + netif stub; next: remaining APIs) |
| Full node on Xbox | **Not yet** — blocked until Core static stack links + MSIX WithCore |

## Risks (known)

| Risk | Mitigation |
|------|------------|
| Boost/libevent UWP port gaps | x64-uwp triplet; fall back to vendored minimal deps |
| LevelDB mmap / weakly_canonical | datadir only under LocalState; mmap limit 0 if needed |
| RAM / dbcache | start 128–256 MiB; Game package |
| Long CI | cache vcpkg + core build dir |
| API surface still missing | iterate patches from link/runtime errors |

## Out of scope v1

Wallet, mining UI, Store, Lightning, listen=1, archival node.
