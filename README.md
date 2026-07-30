# xbox_bitcoind

Port of **Bitcoin Core (`bitcoind`)** to **Xbox Series S** (shared Dev Mode console with [xllama](../xllama/)) as a **pruned validating full node**, delivered as a **UWP package** via Device Portal.

[![ci-linux](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-linux.yml)
[![ci-msvc-baseline](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-msvc-baseline.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/ci-msvc-baseline.yml)
[![build-uwp](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/build-uwp.yml/badge.svg)](https://github.com/gianlucamazza/xbox_bitcoind/actions/workflows/build-uwp.yml)

> Status: **UWP scaffold ready** — Core **v31.1** desktop CI green; Hello-UWP probes package under `uwp/`. Bitcoind not linked into MSIX yet.

## Goals (v1)

- Validate Bitcoin consensus rules on-console (pruned full node)
- P2P sync (outbound-first; listening optional)
- Local RPC for debugging and future UI
- Minimal controller-friendly status UI
- Install via Xbox Device Portal (Dev Mode)

## Non-goals (v1)

- Archival (non-pruned) chain storage
- Mining
- Microsoft Store submission
- Lightning (sibling `xbox_lightning` later)
- Original Xbox / Xbox 360

## Console (shared with xllama)

| | |
|--|--|
| Hardware | **Xbox Series S** in Dev Mode |
| Portal | `https://<ip>:11443` (see `~/.config/xllama/xbox-env`) |
| Storage | Dev partition raised to **~90 GB** |
| After install | Set package **App type → Game** |

Details: [docs/console.md](docs/console.md) · Device Portal: [docs/device-portal.md](docs/device-portal.md) · Constraints: [docs/uwp-constraints.md](docs/uwp-constraints.md)

### Quick probe (Linux host)

```bash
# Credentials: reuses ~/.config/xllama/xbox-env by default
./scripts/probe-console.sh
```

```bash
./scripts/deploy.sh os-info
./scripts/deploy.sh packages
./scripts/deploy.sh disk-usage
```

When an MSIX exists:

```bash
./scripts/deploy.sh path/to/xbox_bitcoind_*.msix
# Dev Home → package → View details → App type → Game
```

## Architecture (planned)

```
UWP package (Game class)
├── Shell UI (C++/WinRT)
├── Bitcoin Core node (library or hosted)
├── datadir: LocalState\bitcoin (USB later)
└── Network: P2P out, RPC local
```

Draft node defaults: [config/bitcoin.conf.console](config/bitcoin.conf.console)

## Bitcoin Core pin + MSVC baseline

| | |
|--|--|
| Pin | **v31.1** (`9be056a8…`) — [config/bitcoin-core.pin](config/bitcoin-core.pin) |
| Tree | `third_party/bitcoin` via `./scripts/fetch-bitcoin-core.sh` |
| Windows MSVC | [docs/build-msvc-baseline.md](docs/build-msvc-baseline.md) · `.\scripts\build-msvc-baseline.ps1` |
| Linux smoke | `./scripts/build-linux-smoke.sh` (same pin; not a substitute for MSVC) |
| CI | [docs/ci.md](docs/ci.md) — `ci-linux` + `ci-msvc-baseline` (GHA) |
| Results log | [docs/research/spikes/desktop-baseline.md](docs/research/spikes/desktop-baseline.md) |

```bash
./scripts/fetch-bitcoin-core.sh
# On Windows (Developer PowerShell for VS):
#   .\scripts\fetch-bitcoin-core.ps1
#   .\scripts\build-msvc-baseline.ps1
# On Arch host (optional smoke):
#   CI_SKIP_TESTS=1 ./scripts/build-linux-smoke.sh
```

## Repo layout

```
config/               # pin, xbox-env.example, bitcoin.conf.console
scripts/              # fetch Core, MSVC/UWP build, console deploy
uwp/                  # C++/WinRT Hello-UWP + probes (scaffold)
third_party/bitcoin/  # fetched pin (gitignored clone)
docs/
  uwp-scaffold.md
  build-msvc-baseline.md
  ci.md
  console.md
  research/
README.md
```

### UWP build & deploy

```powershell
# Windows
.\scripts\build-uwp.ps1
```

```bash
# Linux host → Series S
source scripts/env.sh
./scripts/deploy.sh uwp/AppPackages/**/xbox_bitcoind_*.msix
./scripts/deploy.sh get-log
```

Details: [docs/uwp-scaffold.md](docs/uwp-scaffold.md)

## Roadmap

| Phase | Focus | Status |
|-------|--------|--------|
| **0** | Research + console config (shared xllama) | **done** |
| **0b** | Pin Core + MSVC baseline docs/scripts | **done** |
| **0b′** | GitHub Actions (Linux + MSVC) | **green on main** |
| **0c** | API matrix | **done** (static) |
| **0d** | Hello-UWP scaffold + probes | **scaffold in tree** |
| **1** | Link Core into UWP / package on console | pending |
| **2** | Node: regtest → testnet → mainnet pruned | pending |
| **3** | Hardening, docs, optional wallet | pending |

Research: [docs/research/00-feasibility.md](docs/research/00-feasibility.md) · spikes: [docs/research/01-phase0-spikes.md](docs/research/01-phase0-spikes.md)

## Requirements

**Host (deploy/probe):** Linux or Windows with `curl` + `python3`; network path to the console.

**Build (package, later):** Windows 10/11, Visual Studio (C++ / UWP), signing cert.

**Run:** This Series S in Dev Mode, free space for pruned datadir, network.

## License

- Original docs and glue in this repo: TBD when code lands.
- Bitcoin Core remains under its own license (MIT); preserve upstream notices.

## Disclaimer

Not affiliated with Microsoft or Bitcoin Core. Dev Mode and console software use are subject to Microsoft terms. Running a node is not mining and does not earn block rewards. You are responsible for legal and network usage in your jurisdiction.
