# xbox_bitcoind

Pruned **Bitcoin Core (`bitcoind`)** full node for **Xbox Series S|X Developer Mode**,
packaged as a **UWP Game** app and installed over Device Portal.

Same console as [xllama](../xllama/). **Not** affiliated with Microsoft or Bitcoin Core.

[![ci-linux](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-linux.yml)
[![ci-msvc-baseline](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-msvc-baseline.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-msvc-baseline.yml)
[![build-uwp](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/build-uwp.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/build-uwp.yml)
[![Release](https://img.shields.io/github/v/release/gianlucamazza/xbox_bitcoind)](https://github.com/gianlucamazza/xbox_bitcoind/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

![Console dashboard](docs/assets/screenshot-console.png)

## Features

- **Bitcoin Core v31.1** embedded in-process (`BitcoindMain` / `BITCOIND_EMBED`)
- Mainnet pruned node (`prune=550`, outbound P2P, local RPC)
- Controller-first **status dashboard** (height, peers, progress, log tail)
- **Soft-stop** flush path (suspend → RPC `stop` → durable LevelDB)
- Path-filtered CI + automated **GitHub Releases** on `v*` tags

| | |
|--|--|
| Package | `GianlucaMazza.xboxbitcoind` · App Id `App` · type **Game** |
| Datadir | `LocalState\bitcoin` |
| Pin | [config/bitcoin-core.pin](config/bitcoin-core.pin) |

## Quick start (console)

**Requirements:** Series S|X in **Dev Mode**, Device Portal credentials
(default `~/.config/xllama/xbox-env`), Linux or Windows host with `curl` + `python3`.

1. Download **[latest release](https://github.com/gianlucamazza/xbox_bitcoind/releases/latest)**  
   (`*.msix` + `xbox_bitcoind-dev.cer`).
2. Install and run:

```bash
source ~/.config/xllama/xbox-env   # or: source scripts/env.sh
./scripts/deploy.sh install-cert path/to/xbox_bitcoind-dev.cer
./scripts/deploy.sh path/to/xbox_bitcoind_*.msix
# Dev Home → package → View details → App type → Game
./scripts/deploy.sh start-app
./scripts/deploy.sh status
```

3. **Stop only** with soft stop (never hard-kill mid-IBD):

```bash
./scripts/deploy.sh stop-app
```

Day-to-day ops: **[docs/ops.md](docs/ops.md)**.

## Build from source

| Target | Host | Command |
|--------|------|---------|
| Scaffold MSIX (no Core) | Windows + UWP workload | `.\scripts\build-uwp.ps1` |
| Core libs only | **VS 2026 18.3+**, vcpkg | `.\scripts\build-uwp.ps1 -CoreOnly` |
| Product MSIX (reuse Core) | same | `.\scripts\build-uwp.ps1 -WithCore -SkipCoreBuild` |
| Product MSIX (monolithic) | same | `.\scripts\build-uwp.ps1 -WithCore` |
| Desktop pin baseline | VS 2026 | `.\scripts\build-msvc-baseline.ps1` |
| Linux pin smoke | Linux | `./scripts/fetch-bitcoin-core.sh && CI_SKIP_TESTS=1 ./scripts/build-linux-smoke.sh` |

Details: [docs/uwp-scaffold.md](docs/uwp-scaffold.md) · [docs/ci.md](docs/ci.md) · [scripts/README.md](scripts/README.md).

## Architecture

```
xbox_bitcoind.exe  (UWP AppContainer, Game)
├── MainPage / rpc_client / probes
└── node_host → BitcoindMain  [XBB_WITH_CORE]
       datadir = LocalState\bitcoin
       static: bitcoin_embed + node stack (x64-uwp)
```

In-process only (no `CreateProcess`). AppContainer patches: [patches/uwp/](patches/uwp/README.md).  
Full design + checklist: [docs/plan-core-uwp.md](docs/plan-core-uwp.md).

## Repository layout

```
config/       pin, bitcoin.conf.console, xbox-env.example
uwp/          C++/WinRT app (UI, node host, manifest)
patches/uwp/  Core AppContainer + durability patches
scripts/      fetch, build, deploy, release helpers
docs/         guides — start at docs/README.md
.github/      CI + release automation
```

## Documentation

| Audience | Start here |
|----------|------------|
| Run on console | [docs/ops.md](docs/ops.md) |
| Full doc index | [docs/README.md](docs/README.md) |
| Architecture | [docs/plan-core-uwp.md](docs/plan-core-uwp.md) |
| CI / costs | [docs/ci.md](docs/ci.md) |
| Scripts map | [scripts/README.md](scripts/README.md) |

Research (phase 0 archive): [docs/research/](docs/research/00-feasibility.md).

## Status

| Area | State |
|------|--------|
| WithCore on Series S | **Working** (package e.g. `0.1.0.42`) |
| Soft-stop persistence | **Verified** (early + mid IBD) |
| Mainnet IBD | **In progress** on fresh sync (long-running) |
| Wallet / Store / `listen=1` | Out of scope v1 |

Roadmap detail: [docs/plan-core-uwp.md](docs/plan-core-uwp.md).

## Maintainers

```bash
# New GitHub Release (CI builds MSIX + publishes assets)
./scripts/cut-release.sh 0.2.0
```

See [docs/ci.md](docs/ci.md#releases-automated).

## License

- This repository (glue, scripts, docs): [MIT](LICENSE).
- Bitcoin Core remains MIT; preserve upstream notices when redistributing derived trees or patches.

## Disclaimer

Dev Mode and console software are subject to Microsoft terms. Running a node is not
mining and does not earn block rewards. You are responsible for legal and network
usage in your jurisdiction.
