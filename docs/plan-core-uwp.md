# Plan: Bitcoin Core → UWP full node

Implementation plan and live checklist. Status: root [README](../README.md).
Index: [docs/README.md](README.md).

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
        ├── prune=550 listen=0 server=1 dbcache=512 (console defaults)
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
| CI `uwp-core` | `windows-2025-vs2026` | same family as `ci-msvc-baseline` |
| CI `uwp-scaffold` | `windows-2022` (PR only) | UWP v143 workload, no Core |
| UWP app toolset | **v145** on VS2026 hosts | match Core objects |

Building Core for UWP on VS2022 produces **C7595** (`consteval`). Unsupported
toolchain — do **not** paper over it with language hacks in Core headers.

## Work packages

| # | Package | Deliverable | Status |
|---|---------|-------------|--------|
| 1 | **Patches** | AppContainer + durability (`0001`–`0010`) | **done** |
| 2 | **Core UWP build** | `build-core-uwp.ps1` → `bitcoin_embed` + stack | **done** |
| 3 | **Embed + UI** | `node_host` + dashboard RPC | **done** |
| 4 | **Package** | `build-uwp.ps1 -WithCore` MSIX (+ `event.dll`) | **done** |
| 5 | **CI** | Path-filtered workflows; no scaffold+core double bill on main | **done** |
| 6 | **Console** | deploy, Game class, mainnet pruned node | **done** (IBD ongoing) |
| 7 | **Persistence** | soft stop + LevelDB durability (early + mid IBD) | **verified** |
| 8 | **Docs** | README + docs map + SECURITY/CHANGELOG | **done** |
| 9 | **Ops tooling** | status / soft-stop-test / ibd-sample / timer | **done** |
| 10 | **Build pipeline** | Core vs MSIX split, SkipIfFresh, CI stages | **done** |
| 11 | **IBD monitor** | hourly user timer + report/stuck/milestones | **done** |
| 12 | **IBD to tip** | wall-clock mainnet sync + ≥24h stable | **ops pending** |

**Engineering packages 1–11: complete.** Package 12 is not a coding task — see
[roadmap.md](roadmap.md) and `./scripts/v1-close-check.sh`.

## Patch set (`patches/uwp/`)

See [patches/uwp/README.md](../patches/uwp/README.md).

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

CI matrix and path filters: [ci.md](ci.md).

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
- [x] Console: Game class + `BitcoindMain` / `debug.log`
- [x] Node running (v31.1, prune=550, LevelDB/chainstate open)
- [x] Status dashboard (RPC live metrics, Start/Stop, log tail)
- [x] Chain state conserved across soft stop ([persistence.md](persistence.md))
- [x] Path-filtered CI (docs free; no scaffold+core on main)
- [x] Ops tooling + mid-IBD soft-stop retest + RAM/disk samples ([ops.md](ops.md))
- [x] Build stages split (CoreOnly / SkipCoreBuild / CI core→package) ([ci.md](ci.md))
- [x] IBD monitor automation (user timer + report + close-check script)
- [ ] **Ops only:** mainnet IBD complete + ≥24h stable at tip  
      (`./scripts/v1-close-check.sh` → exit 0; then soft-stop at tip)

### Implementation status

| Layer | Status |
|-------|--------|
| Scaffold UI + probes | **Done** |
| Desktop Core pin v31.1 | **Done** (MSVC baseline on VS2026) |
| UWP patches 0001–0010 | **Done** (API + durability; no language hacks) |
| `build-core-uwp` / `-WithCore` | **Done** (`bitcoin_embed` + `event.dll`) |
| CI + release automation | **Done** |
| Package on Series S | **`0.1.0.65`** WithCore, mainnet IBD ~14% / ~453k |
| Full node sync | **Ops pending** — [tracking.md](tracking.md) · issues #1–#3 |
| Soft-stop persistence | Tip conserved early + mid IBD; DELETE path still seen mid-IBD ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |
| Ops tooling | status / soft-stop-test / ibd-sample / ibd-report / **v1-close-check** |
| Version automation | pin → `xbb_version.generated.h` · package identity at runtime |
| **v1 engineering** | **Complete** (2026-07-31) |

## Risks (known)

| Risk | Mitigation |
|------|------------|
| VS2026 image missing UWP VC | CI installs Universal + UWP.VC when needed |
| Boost/libevent UWP | x64-uwp + current libevent (0004) |
| LevelDB / LocalState | datadir only under LocalState; 0009/0010 |
| Hard kill loses unflushed tip | Soft stop via suspend; faster write interval |
| RAM / dbcache | start 128–256 MiB; Game package |
| Long CI | path filters + caches (keyed by pin / patches) |

## Out of scope v1

Wallet, mining UI, Store, Lightning, `listen=1`, archival node.
