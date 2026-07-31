# Documentation

Entry point for humans and agents. Product status and quick start live in the
root [README](../README.md).

## Current product

| Doc | What it covers |
|-----|----------------|
| [plan-core-uwp.md](plan-core-uwp.md) | Architecture, toolchain, work packages, success checklist |
| [ui.md](ui.md) | Status dashboard (metrics, RPC, Start/Stop, log tail) |
| [persistence.md](persistence.md) | Soft-stop flush path and verified chain conservation |
| [console.md](console.md) | Shared Series S baseline (OS, storage, package identity) |
| [device-portal.md](device-portal.md) | Device Portal env, `deploy.sh` / `probe-console.sh` |
| [uwp-scaffold.md](uwp-scaffold.md) | UWP app layout, probes, build & deploy |
| [uwp-constraints.md](uwp-constraints.md) | AppContainer / disk / RAM constraints (bitcoind view) |
| [ci.md](ci.md) | GitHub Actions (Linux, MSVC, UWP scaffold + core) |
| [build-msvc-baseline.md](build-msvc-baseline.md) | Desktop Core pin build on Windows |

## Assets

| Path | Description |
|------|-------------|
| [assets/screenshot-console.png](assets/screenshot-console.png) | Live Series S dashboard (README hero) |

## Code & patches

| Path | Description |
|------|-------------|
| [`../uwp/`](../uwp/) | C++/WinRT app source |
| [`../patches/uwp/`](../patches/uwp/README.md) | Core AppContainer + durability patches (0001–0010) |
| [`../scripts/`](../scripts/) | Fetch, patch, build, deploy |
| [`../config/`](../config/) | Pin, console `bitcoin.conf`, env example |

## Research archive (phase 0)

Historical feasibility and spikes — not day-to-day ops.

| Doc | Topic |
|-----|--------|
| [research/00-feasibility.md](research/00-feasibility.md) | Initial feasibility |
| [research/01-phase0-spikes.md](research/01-phase0-spikes.md) | Spike plan |
| [research/SOURCES.md](research/SOURCES.md) | Source list |
| [research/spikes/api-matrix.md](research/spikes/api-matrix.md) | Win32 / UWP API matrix |
| [research/spikes/desktop-baseline.md](research/spikes/desktop-baseline.md) | MSVC baseline results log |

## Suggested reading order

1. Root README (status + architecture)  
2. `plan-core-uwp.md` (how the node is embedded)  
3. `persistence.md` + `ui.md` (runtime behaviour)  
4. `device-portal.md` + `console.md` (operate the Series S)  
5. `uwp-scaffold.md` / `ci.md` / `patches/uwp/README.md` when building
