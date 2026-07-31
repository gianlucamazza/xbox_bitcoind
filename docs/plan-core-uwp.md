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
        └── links: bitcoin_embed → bitcoin_node + util + common + deps (UWP)
```

**In-process only** (no `CreateProcess`). Shutdown via `Interrupt`/`Shutdown` on app exit.

## Toolchain (non-negotiable)

| Piece | Requirement | Why |
|-------|-------------|-----|
| Bitcoin Core pin | **v31.1** | product pin |
| MSVC for Core | **VS 2026 18.3+** | Core docs + working consteval C++20 |
| CI `uwp-core` | `windows-2025-vs2026` | same as `ci-msvc-baseline` |
| CI `uwp-scaffold` | `windows-2022` | UWP v143 workload preinstalled |
| UWP app toolset | **v145** on VS2026 hosts | match Core objects |

Building Core for UWP on VS2022 produces **C7595** (`consteval` immediate functions). That is an unsupported toolchain, not an AppContainer issue — do **not** paper over it with language hacks in Core headers.

## Work packages

| # | Package | Deliverable |
|---|---------|-------------|
| 1 | **Patches** | AppContainer API surface only (`patches/uwp/0001`–`0008`) |
| 2 | **Core UWP build** | `build-core-uwp.ps1` → `bitcoin_embed` + stack via CMake + `x64-uwp` |
| 3 | **Embed + UI** | `node_host` starts/stops node thread; UI shows running/errors |
| 4 | **Package** | `build-uwp.ps1 -WithCore` links libs into MSIX |
| 5 | **CI** | `uwp-core` on VS2026; artifact MSIX |
| 6 | **Console** | deploy, Game class, regtest then mainnet pruned |

## Patch set (`patches/uwp/`)

AppContainer adaptations only (see `patches/uwp/README.md`).

## Build (Windows CI)

```
fetch pin → apply patches 0001–0008
  → cmake WindowsStore + x64-uwp (VS 18 2026)
  → build bitcoin_embed + static deps
  → write xbb-core-libs.props
  → MSBuild UWP app (v145) XbbWithCore → sign MSIX
```

## Config defaults

`config/bitcoin.conf.console` + host-forced:

```
-datadir=<LocalState>\bitcoin
-printtoconsole=0
-debuglogfile=debug.log
```

## Success criteria

- [x] Plan + AppContainer patches + embed + CI wiring  
- [x] Hello-UWP MSIX on Series S (probes 4/4)  
- [x] vcpkg UWP deps path (libevent via drop override)  
- [x] Toolchain aligned with Core requirement (VS2026 for `uwp-core`)  
- [x] Core static libs full compile for UWP on GHA (VS2026)  
- [x] MSIX WithCore links `BitcoindMain` (exe contains symbol; CI green)  
- [x] Deploy WithCore package to Series S (`0.1.0.34`)  
- [ ] Console: App type **Game** + confirm node thread / `debug.log`  
- [ ] regtest / mainnet pruned progress  

### Implementation status

| Layer | Status |
|-------|--------|
| Scaffold UI + probes | **Done**, deployed on console |
| Desktop Core pin v31.1 | **Done**, MSVC 137/137 on VS2026 |
| UWP patches 0001–0008 | **Done** (API surface; no language hacks) |
| `build-core-uwp` / `-WithCore` | **Done** (`bitcoin_embed`, props-based link) |
| CI `uwp-core` | **Green** on `windows-2025-vs2026` ([run 30594425693](https://github.com/gianlucamazza/xbox_bitcoind/actions/runs/30594425693)) |
| Package on Series S | **Installed** `GianlucaMazza.xboxbitcoind_0.1.0.34_*` (probes OK; set App type Game) |
| Full node sync | **Next** — Game class + BitcoindMain runtime validation |

## Risks (known)

| Risk | Mitigation |
|------|------------|
| VS2026 image missing UWP VC | CI step installs Universal + UWP.VC components |
| Boost/libevent UWP | x64-uwp + current libevent (0004) |
| LevelDB / LocalState | datadir only under LocalState |
| RAM / dbcache | start 128–256 MiB; Game package |
| Long CI | cache vcpkg + core build dir (keyed by VS2026) |

## Out of scope v1

Wallet, mining UI, Store, Lightning, listen=1, archival node.
