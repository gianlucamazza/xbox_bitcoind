# Plan: Bitcoin Core → UWP full node

Implementation plan and live checklist. High-level status: root
[README](../README.md). Index: [docs/README.md](README.md).

## Goal

Run a **pruned validating `bitcoind`** inside the UWP package on Series S
Dev Mode, pin **v31.1**, with status UI + logs + durable chain state.

## Architecture

```
xbox_bitcoind.exe (UWP AppContainer, Game class)
├── MainPage — status dashboard (RPC metrics, Start/Stop, log tail)
├── probes — AppContainer capability checks
├── rpc_client — loopback JSON-RPC + cookie
└── node_host → BitcoindMain(argc, argv)   [XBB_WITH_CORE]
        │
        ├── -datadir=<LocalState>\bitcoin
        ├── -conf=<LocalState>\bitcoin\bitcoin.conf
        ├── prune=550 listen=0 server=1 dbcache=256
        └── links: bitcoin_embed → bitcoin_node + util + common + deps (x64-uwp)
```

**In-process only** (no `CreateProcess`). Clean shutdown:

- UI Stop / app suspend → RPC `stop` → join node thread → LevelDB flush
- `deploy.sh stop-app` posts **suspend** first, then taskmanager DELETE

## Toolchain (non-negotiable)

| Piece | Requirement | Why |
|-------|-------------|-----|
| Bitcoin Core pin | **v31.1** | product pin |
| MSVC for Core | **VS 2026 18.3+** | Core docs + working consteval C++20 |
| CI `uwp-core` | `windows-2025-vs2026` | same as `ci-msvc-baseline` |
| CI `uwp-scaffold` | `windows-2022` | UWP v143 workload preinstalled |
| UWP app toolset | **v145** on VS2026 hosts | match Core objects |

Building Core for UWP on VS2022 produces **C7595** (`consteval`). That is an
unsupported toolchain — do **not** paper over it with language hacks in Core headers.

## Work packages

| # | Package | Deliverable | Status |
|---|---------|-------------|--------|
| 1 | **Patches** | AppContainer + durability (`patches/uwp/0001`–`0010`) | **done** |
| 2 | **Core UWP build** | `build-core-uwp.ps1` → `bitcoin_embed` + stack | **done** |
| 3 | **Embed + UI** | `node_host` + dashboard RPC | **done** |
| 4 | **Package** | `build-uwp.ps1 -WithCore` MSIX (+ `event.dll`) | **done** |
| 5 | **CI** | `uwp-core` on VS2026; artifact MSIX | **green** |
| 6 | **Console** | deploy, Game class, mainnet pruned IBD | **running** |
| 7 | **Persistence** | soft stop + LevelDB durability | **verified** |

## Patch set (`patches/uwp/`)

AppContainer + UWP durability only — see [patches/uwp/README.md](../patches/uwp/README.md).

| Range | Role |
|-------|------|
| 0001–0008 | API surface, embed entry, static lib, netif/subprocess |
| 0009–0010 | LevelDB durable writes; faster DB write interval (30–60 s) |

## Build (Windows / CI)

```
fetch pin → apply patches 0001–0010
  → cmake WindowsStore + x64-uwp (VS 18 2026)
  → build bitcoin_embed + static deps
  → write xbb-core-libs.props
  → MSBuild UWP app (v145) XbbWithCore → ship event.dll → sign MSIX
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
- [x] Toolchain aligned with Core (VS2026 for `uwp-core`)
- [x] Core static libs full compile for UWP on GHA (VS2026)
- [x] MSIX WithCore links `BitcoindMain` (CI green)
- [x] Deploy WithCore package to Series S
- [x] Console: `DefaultUWPContentTypeToGame=true` + `BitcoindMain` / `debug.log`
- [x] Node running on Series S (v31.1, prune=550, LevelDB/chainstate open)
- [x] Status dashboard (RPC live metrics, Start/Stop, log tail)
- [x] Chain state conserved across soft stop (see [persistence.md](persistence.md))
- [ ] mainnet IBD complete / long-run stability

### Implementation status

| Layer | Status |
|-------|--------|
| Scaffold UI + probes | **Done**, deployed on console |
| Desktop Core pin v31.1 | **Done**, MSVC 137/137 on VS2026 |
| UWP patches 0001–0010 | **Done** (API + durability; no language hacks) |
| `build-core-uwp` / `-WithCore` | **Done** (`bitcoin_embed` + package `event.dll`) |
| CI `uwp-core` | **Green** on VS2026 |
| Package on Series S | **WithCore** running (manifest base `0.1.0.0`; deploy bumps e.g. `0.1.0.42`) |
| Full node sync | **In progress** (mainnet pruned IBD) |
| Soft-stop persistence | **Verified** 2026-07-31 |

## Risks (known)

| Risk | Mitigation |
|------|------------|
| VS2026 image missing UWP VC | CI step installs Universal + UWP.VC components |
| Boost/libevent UWP | x64-uwp + current libevent (0004) |
| LevelDB / LocalState | datadir only under LocalState; 0009/0010 |
| Hard kill loses unflushed tip | Soft stop via suspend; faster write interval |
| RAM / dbcache | start 128–256 MiB; Game package |
| Long CI | cache vcpkg + core build dir (keyed by VS2026) |

## Out of scope v1

Wallet, mining UI, Store, Lightning, `listen=1`, archival node.
