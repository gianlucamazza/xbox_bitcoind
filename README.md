# xbox_bitcoind

**Bitcoin Core (`bitcoind`) pruned full node** on **Xbox Series S** (Dev Mode),
shipped as a **UWP Game package** via Device Portal. Same console as
[xllama](../xllama/).

[![ci-linux](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-linux.yml)
[![ci-msvc-baseline](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-msvc-baseline.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-msvc-baseline.yml)
[![build-uwp](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/build-uwp.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/build-uwp.yml)

> **Status (2026-07-31):** bitcoind **v31.1** on Series S · package **`0.1.0.42`** ·
> dashboard UI · soft-stop persistence **verified** · mainnet IBD **in progress**
> (~height 318k, ~3% progress) · CI path-filtered on **VS 2026**.

![Console dashboard](docs/assets/screenshot-console.png)

## What works today

| Capability | Detail |
|------------|--------|
| Node | In-process `BitcoindMain` (`BITCOIND_EMBED`), pin **v31.1** |
| Network | Mainnet, `prune=550`, outbound P2P (`listen=0`) |
| Data | `LocalState\bitcoin` (LevelDB + block files) |
| UI | Controller-first XAML dashboard — height, headers, peers, progress, log |
| RPC | Loopback `127.0.0.1:8332`, cookie auth |
| Lifecycle | Soft stop: Device Portal suspend → `OnSuspending` → RPC `stop` → flush |
| Package | `GianlucaMazza.xboxbitcoind`, App Id `App`, type **Game** |
| Toolchain | WithCore MSIX: **VS 2026 18.3+**; scaffold-only: VS2022 OK |
| CI | Path-scoped workflows — docs-only commits cost **zero** minutes |

Persistence check: [docs/persistence.md](docs/persistence.md) (tip ~102k preserved after soft stop).

## Goals (v1)

- Validate Bitcoin consensus on-console (pruned full node)
- P2P sync (outbound-first)
- Local RPC for UI and debugging
- Minimal controller-friendly status UI
- Install via Xbox Device Portal (Dev Mode)

## Non-goals (v1)

- Archival (non-pruned) chain · mining · Microsoft Store · Lightning
  (sibling `xbox_lightning` later) · original Xbox / 360

## Architecture

```
xbox_bitcoind.exe  (UWP AppContainer, Game class)
├── MainPage          status dashboard (programmatic XAML)
├── probes            AppContainer capability checks
├── rpc_client        loopback JSON-RPC + cookie
└── node_host  →  BitcoindMain(argc, argv)   [XBB_WITH_CORE]
        │
        ├── -datadir=<LocalState>\bitcoin
        ├── -conf=…\bitcoin\bitcoin.conf
        ├── prune=550  listen=0  server=1  dbcache=256
        └── static stack: bitcoin_embed + node + util + common + deps (x64-uwp)
```

**In-process only** (no `CreateProcess`). Patches: [`patches/uwp/`](patches/uwp/README.md)
(0001–0010). Defaults: [`config/bitcoin.conf.console`](config/bitcoin.conf.console).

Details: [docs/plan-core-uwp.md](docs/plan-core-uwp.md).

## Console (shared with xllama)

| | |
|--|--|
| Hardware | **Xbox Series S** in Dev Mode |
| Portal | `https://<ip>:11443` — `~/.config/xllama/xbox-env` |
| Storage | Dev partition ~**90 GB** |
| After install | Dev Home → package → **App type → Game** |

```bash
./scripts/probe-console.sh
./scripts/deploy.sh path/to/xbox_bitcoind_*.msix
./scripts/deploy.sh start-app
./scripts/deploy.sh stop-app          # soft stop (suspend first)
```

[console](docs/console.md) · [device-portal](docs/device-portal.md) ·
[constraints](docs/uwp-constraints.md) · [UI](docs/ui.md)

## Pin + builds

| | |
|--|--|
| Pin | **v31.1** (`9be056a8…`) — [config/bitcoin-core.pin](config/bitcoin-core.pin) |
| Tree | `third_party/bitcoin` via `./scripts/fetch-bitcoin-core.sh` (gitignored) |
| Desktop MSVC | [docs/build-msvc-baseline.md](docs/build-msvc-baseline.md) |
| UWP WithCore | `apply-uwp-patches` → `build-core-uwp` → `build-uwp -WithCore` |
| Linux smoke | `./scripts/build-linux-smoke.sh` |
| CI | [docs/ci.md](docs/ci.md) — path filters, no workflow overlap |

```bash
./scripts/fetch-bitcoin-core.sh
# Windows (VS 2026 Developer PowerShell):
#   .\scripts\fetch-bitcoin-core.ps1
#   .\scripts\apply-uwp-patches.ps1
#   .\scripts\build-core-uwp.ps1
#   .\scripts\build-uwp.ps1 -WithCore
```

## Repo layout

```
config/                 pin, bitcoin.conf.console, xbox-env.example
scripts/                fetch, patch, MSVC/UWP build, deploy, probe
uwp/                    C++/WinRT app (UI, node_host, rpc, probes, manifest)
patches/uwp/            AppContainer + durability (0001–0010)
third_party/bitcoin/    fetched pin (gitignored)
docs/                   index + guides → docs/README.md
.github/workflows/      ci-linux · ci-msvc-baseline · build-uwp
LICENSE                 MIT (this repo’s glue)
```

## Roadmap

| Phase | Focus | Status |
|-------|--------|--------|
| **0–0d** | Research, pin, baselines, Hello-UWP probes | **done** |
| **1** | Core in UWP (`-WithCore`, VS2026 CI) | **done** |
| **2** | Mainnet pruned + dashboard on console | **running** (IBD) |
| **2b** | Soft-stop chain persistence | **verified** |
| **2c** | Docs map + path-filtered CI (no overlap/extra cost) | **done** |
| **3** | IBD complete / long-run stability · optional wallet | **open** |

## Documentation

| Doc | Topic |
|-----|--------|
| [docs/README.md](docs/README.md) | **Index** (start here for depth) |
| [docs/plan-core-uwp.md](docs/plan-core-uwp.md) | Architecture + checklist |
| [docs/ui.md](docs/ui.md) · [persistence.md](docs/persistence.md) | Runtime |
| [docs/console.md](docs/console.md) · [device-portal.md](docs/device-portal.md) | Operate Series S |
| [docs/uwp-scaffold.md](docs/uwp-scaffold.md) · [ci.md](docs/ci.md) | Build & CI |
| [patches/uwp/README.md](patches/uwp/README.md) | Patch list |

Research archive: [docs/research/](docs/research/00-feasibility.md).

## Requirements

**Host:** Linux/Windows with `curl` + `python3`; network path to the console.

**Build (WithCore):** Windows, **VS 2026 18.3+** (C++ desktop + UWP), vcpkg.
Scaffold-only: VS2022 OK.

**Run:** Series S Dev Mode, free space for pruned datadir, network.

## License

- This repo (code, scripts, docs): [MIT](LICENSE).
- Bitcoin Core: MIT (upstream); keep notices when redistributing trees or derived patches.

## Disclaimer

Not affiliated with Microsoft or Bitcoin Core. Dev Mode use is subject to Microsoft
terms. Running a node is not mining and earns no block rewards. You are responsible
for legal and network usage in your jurisdiction.
