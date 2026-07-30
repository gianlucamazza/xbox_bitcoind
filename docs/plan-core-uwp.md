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

1. `0001-lockedpool-no-virtuallock.patch` — no `VirtualLock`/`Unlock`/`GetProcessWorkingSetSize` on UWP  
2. `0002-runcommand-noop-uwp.patch` — `runCommand` no-op under `WINAPI_FAMILY_APP`  
3. `0003-bitcoind-embed-entry.patch` — `BitcoindMain` when `BITCOIND_EMBED`  
4. Apply via `scripts/apply-uwp-patches.sh` after fetch

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

- [ ] Core static libs build for UWP on GHA  
- [ ] MSIX starts node thread without crash  
- [ ] regtest mines/generates or accepts blocks  
- [ ] mainnet pruned headers progress (IBD long-running, Game class)  
- [ ] logs in LocalState  

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
