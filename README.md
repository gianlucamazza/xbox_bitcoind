# xbox_bitcoind

**Bitcoin Core (`bitcoind`) pruned full node** on **Xbox Series S** (Dev Mode),
shipped as a **UWP Game package** via Device Portal. Same console as
[xllama](../xllama/).

[![ci-linux](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-linux.yml)
[![ci-msvc-baseline](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-msvc-baseline.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-msvc-baseline.yml)
[![build-uwp](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/build-uwp.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/build-uwp.yml)

> **Status:** bitcoind **v31.1** embedded and running on Series S · dashboard UI
> (RPC metrics, Start/Stop, log tail) · chain state **persists** across soft stop ·
> CI `uwp-core` green on **VS 2026**.

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
| Package | Identity `GianlucaMazza.xboxbitcoind`, App Id `App`, type **Game** |
| Toolchain | Core + WithCore MSIX: **VS 2026 18.3+**; scaffold-only: VS2022 OK |

See [docs/persistence.md](docs/persistence.md) for the soft-stop verification
(nBestHeight ~102k preserved after restart).

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

**In-process only** (no `CreateProcess`). AppContainer shims live in
[`patches/uwp/`](patches/uwp/README.md) (0001–0010). Defaults:
[`config/bitcoin.conf.console`](config/bitcoin.conf.console).

Full plan and checklist: [docs/plan-core-uwp.md](docs/plan-core-uwp.md).

## Console (shared with xllama)

| | |
|--|--|
| Hardware | **Xbox Series S** in Dev Mode |
| Portal | `https://<ip>:11443` — credentials in `~/.config/xllama/xbox-env` |
| Storage | Dev partition ~**90 GB** |
| After install | Dev Home → package → **App type → Game** |

```bash
./scripts/probe-console.sh
./scripts/deploy.sh os-info | packages | disk-usage
./scripts/deploy.sh path/to/xbox_bitcoind_*.msix   # install
./scripts/deploy.sh start-app
./scripts/deploy.sh stop-app                        # soft stop (suspend first)
```

Docs: [console](docs/console.md) · [device-portal](docs/device-portal.md) ·
[constraints](docs/uwp-constraints.md) · [UI](docs/ui.md)

## Bitcoin Core pin + builds

| | |
|--|--|
| Pin | **v31.1** (`9be056a8…`) — [config/bitcoin-core.pin](config/bitcoin-core.pin) |
| Tree | `third_party/bitcoin` via `./scripts/fetch-bitcoin-core.sh` (gitignored) |
| Desktop MSVC | [docs/build-msvc-baseline.md](docs/build-msvc-baseline.md) |
| UWP Core | `.\scripts\build-core-uwp.ps1` then `.\scripts\build-uwp.ps1 -WithCore` |
| Linux smoke | `./scripts/build-linux-smoke.sh` (not a substitute for MSVC) |
| CI | [docs/ci.md](docs/ci.md) — Linux + MSVC baseline + UWP scaffold/core |

```bash
./scripts/fetch-bitcoin-core.sh
# Windows (Developer PowerShell for VS 2026):
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
patches/uwp/            AppContainer + durability patches (0001–0010)
third_party/bitcoin/    fetched pin (gitignored)
docs/                   index + guides (see docs/README.md)
.github/workflows/      ci-linux, ci-msvc-baseline, build-uwp
```

## Roadmap

| Phase | Focus | Status |
|-------|--------|--------|
| **0** | Research + shared console config | **done** |
| **0b** | Pin Core + MSVC / Linux baselines + GHA | **done** |
| **0c–0d** | API matrix + Hello-UWP probes | **done** |
| **1** | Link Core into UWP (`-WithCore`, VS2026 CI) | **done** |
| **2** | Mainnet pruned node + dashboard on console | **running** (IBD) |
| **2b** | Soft-stop chain persistence | **verified** |
| **3** | Long-run IBD / stability, optional wallet, docs polish | **open** |

Research archive: [docs/research/](docs/research/00-feasibility.md).

## Documentation map

| Doc | Topic |
|-----|--------|
| [docs/README.md](docs/README.md) | Full index |
| [docs/plan-core-uwp.md](docs/plan-core-uwp.md) | Architecture, toolchain, checklist |
| [docs/ui.md](docs/ui.md) | Dashboard layout and RPC sources |
| [docs/persistence.md](docs/persistence.md) | Soft stop + LevelDB durability |
| [docs/console.md](docs/console.md) | Series S baseline |
| [docs/device-portal.md](docs/device-portal.md) | Portal API / deploy scripts |
| [docs/uwp-scaffold.md](docs/uwp-scaffold.md) | UWP package build & probes |
| [docs/ci.md](docs/ci.md) | GitHub Actions |
| [patches/uwp/README.md](patches/uwp/README.md) | Patch list |

## Requirements

**Host (deploy/probe):** Linux or Windows with `curl` + `python3`; path to the console.

**Build (WithCore MSIX):** Windows 10/11, **Visual Studio 2026 18.3+** (C++ desktop +
UWP), vcpkg, signing cert. Scaffold-only packages can build on VS2022.

**Run:** This Series S in Dev Mode, free space for pruned datadir, network.

## License

- Original code, scripts, and docs in this repo: [MIT](LICENSE).
- Bitcoin Core remains under its own license (MIT); preserve upstream notices
  when redistributing a built tree or patches derived from it.

## Disclaimer

Not affiliated with Microsoft or Bitcoin Core. Dev Mode and console software use
are subject to Microsoft terms. Running a node is not mining and does not earn
block rewards. You are responsible for legal and network usage in your jurisdiction.
