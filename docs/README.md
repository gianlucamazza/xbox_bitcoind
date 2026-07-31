# Documentation

**Product status and quick start:** root [README](../README.md).

| | |
|--|--|
| Pin | Bitcoin Core **v31.1** |
| Console package | `GianlucaMazza.xboxbitcoind` **0.1.0.42** (WithCore, Game) |
| Datadir | `LocalState\bitcoin` |
| Last status pass | 2026-07-31 — IBD ~height **327k**, soft-stop mid-IBD **PASS**, WS ~1 GiB |

## Map

### Operate (Series S)

| Doc | Contents |
|-----|----------|
| [ops.md](ops.md) | **Day-to-day IBD ops**, budgets, monitor commands |
| [console.md](console.md) | Shared Series S baseline, package identity |
| [device-portal.md](device-portal.md) | Portal env, `deploy.sh` / soft stop / screenshots |
| [ui.md](ui.md) | Dashboard metrics, RPC sources |
| [persistence.md](persistence.md) | Soft-stop flush + verified conservation |

### Build & integrate

| Doc | Contents |
|-----|----------|
| [plan-core-uwp.md](plan-core-uwp.md) | Architecture, toolchain, checklist |
| [uwp-scaffold.md](uwp-scaffold.md) | UWP tree, probes, local build/deploy |
| [uwp-constraints.md](uwp-constraints.md) | AppContainer / disk / RAM |
| [build-msvc-baseline.md](build-msvc-baseline.md) | Desktop Core pin (not Xbox) |
| [ci.md](ci.md) | GHA: path filters, no overlap, cost matrix |
| [`../patches/uwp/README.md`](../patches/uwp/README.md) | Patches 0001–0010 |

### Assets

| Path | Description |
|------|-------------|
| [assets/screenshot-console.png](assets/screenshot-console.png) | Live dashboard (README hero) |

### Code entry points

| Path | Role |
|------|------|
| [`../uwp/`](../uwp/) | C++/WinRT app |
| [`../scripts/`](../scripts/) | Fetch, patch, build, deploy — [scripts/README.md](../scripts/README.md) |
| [`../config/`](../config/) | Pin, `bitcoin.conf.console`, env example |

### Research archive (phase 0)

Historical only — not day-to-day ops.

| Doc | Topic |
|-----|--------|
| [research/00-feasibility.md](research/00-feasibility.md) | Feasibility |
| [research/01-phase0-spikes.md](research/01-phase0-spikes.md) | Spike plan |
| [research/SOURCES.md](research/SOURCES.md) | Sources |
| [research/spikes/api-matrix.md](research/spikes/api-matrix.md) | Win32 / UWP API matrix |
| [research/spikes/desktop-baseline.md](research/spikes/desktop-baseline.md) | MSVC results log |

## Reading order

1. Root README  
2. `ops.md` (if running the console node)  
3. `plan-core-uwp.md`  
4. `persistence.md` + `ui.md`  
5. `device-portal.md` + `console.md`  
6. When building: `uwp-scaffold.md` → `ci.md` → `patches/uwp/README.md`

## Doc conventions

- **English** for all project docs.
- Root README is SSOT for status; this index is SSOT for navigation.
- `docs/**` and README-only edits do **not** start CI (see [ci.md](ci.md)).
- Prefer updating an existing doc over adding a new top-level file.
