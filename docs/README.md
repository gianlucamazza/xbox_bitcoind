# Documentation

|                      |                                                        |
| -------------------- | ------------------------------------------------------ |
| **Product overview** | Root [README](../README.md)                            |
| **Language**         | English                                                |
| **SSOT**             | README = status & quick start · this file = navigation |

Snapshot (see also [ops.md](ops.md) · [tracking.md](tracking.md)):

|                 |                                                                                                                    |
| --------------- | ------------------------------------------------------------------------------------------------------------------ |
| Pin             | Bitcoin Core **v31.1** (not the MSIX revision)                                                                     |
| Console package | `GianlucaMazza.xboxbitcoind` **0.1.5.10020** (WithCore, **v0.1.5**, deployed 2026-08-02)     |
| Screenshot      | [assets/screenshot-console.png](assets/screenshot-console.png) (capture from an earlier package; refresh optional) |
| Datadir         | `LocalState\bitcoin`                                                                                               |
| IBD             | Mainnet ~45% — [tracking.md](tracking.md)                                                                           |
| Version labels  | **Core pin** vs **app package** — see [ui.md](ui.md) · `generate-version-header.py`                                |

---

## By audience

### Operators (run the node)

| Doc                                      | Contents                                         |
| ---------------------------------------- | ------------------------------------------------ |
| [ops.md](ops.md)                         | Golden rules, status/monitor, budgets, soft-stop |
| [tracking.md](tracking.md)               | **Live status, open work, issue map**            |
| [roadmap.md](roadmap.md)                 | v1 complete vs ops-pending IBD                   |
| [pre-lightning.md](pre-lightning.md)     | Pre-LN gates, conf tip, CLN integration options  |
| [console.md](console.md)                 | Series S baseline, identity, storage             |
| [device-portal.md](device-portal.md)     | Portal env, `deploy.sh` surface                  |
| [persistence.md](persistence.md)         | Soft-stop verification results                   |
| [ui.md](ui.md)                           | Dashboard layout & RPC sources                   |
| [troubleshooting.md](troubleshooting.md) | Symptom → cause → fix (deploy, node, build)      |
| [upgrade.md](upgrade.md)                 | Core pin bump + console MSIX upgrade             |

### Developers (build & change code)

| Doc                                                  | Contents                                    |
| ---------------------------------------------------- | ------------------------------------------- |
| [plan-core-uwp.md](plan-core-uwp.md)                 | Architecture, toolchain, success checklist  |
| [uwp-scaffold.md](uwp-scaffold.md)                   | App tree, probes, local MSIX build          |
| [uwp-constraints.md](uwp-constraints.md)             | AppContainer / disk / RAM limits            |
| [build-msvc-baseline.md](build-msvc-baseline.md)     | Desktop Core pin (not Xbox)                 |
| [ci.md](ci.md)                                       | Workflows, path filters, release automation |
| [../patches/uwp/README.md](../patches/uwp/README.md) | Patch list 0001–0010                        |
| [../scripts/README.md](../scripts/README.md)         | All scripts                                 |

### Maintainers

| Task             | Doc / command                                                                   |
| ---------------- | ------------------------------------------------------------------------------- |
| Cut a release    | `./scripts/cut-release.sh X.Y.Z` · [ci.md § Releases](ci.md#releases-automated) |
| Pin bump         | [upgrade.md](upgrade.md) · `config/bitcoin-core.pin`                            |
| Security reports | [../SECURITY.md](../SECURITY.md)                                                |
| Changelog        | [../CHANGELOG.md](../CHANGELOG.md)                                              |

---

## Code map

| Path                                                             | Role                                                              |
| ---------------------------------------------------------------- | ----------------------------------------------------------------- |
| [`../uwp/`](../uwp/)                                             | C++/WinRT application                                             |
| [`../scripts/`](../scripts/)                                     | Fetch, build, deploy, release                                     |
| [`../config/`](../config/)                                       | Pin, conf defaults, env example                                   |
| [`../patches/uwp/`](../patches/uwp/)                             | Core UWP patches                                                  |
| [`assets/screenshot-console.png`](assets/screenshot-console.png) | Console dashboard (README + [ui.md](ui.md); keep current package) |

---

## Suggested reading order

1. [README](../README.md) — install & features
2. [ops.md](ops.md) — if you operate the Series S
3. [pre-lightning.md](pre-lightning.md) — after tip, before any Lightning work
4. [plan-core-uwp.md](plan-core-uwp.md) — how the node is embedded
5. [uwp-scaffold.md](uwp-scaffold.md) + [ci.md](ci.md) — if you build

---

## Research archive (phase 0)

Historical feasibility only — not day-to-day ops.

| Doc                                                                        | Topic                  |
| -------------------------------------------------------------------------- | ---------------------- |
| [research/00-feasibility.md](research/00-feasibility.md)                   | Feasibility            |
| [research/01-phase0-spikes.md](research/01-phase0-spikes.md)               | Spike plan             |
| [research/SOURCES.md](research/SOURCES.md)                                 | Sources                |
| [research/spikes/api-matrix.md](research/spikes/api-matrix.md)             | Win32 / UWP API matrix |
| [research/spikes/desktop-baseline.md](research/spikes/desktop-baseline.md) | MSVC results log       |

---

## Conventions

- Prefer updating an existing doc over adding a new top-level file.
- Version SSOT: console package rev lives in [tracking.md](tracking.md), latest release
  in the [CHANGELOG](../CHANGELOG.md); `scripts/check-doc-versions.sh` (CI) keeps the
  other docs in agreement — update the SSOT first, then the mirrors.
- `docs/**` and README-only commits do **not** start CI ([ci.md](ci.md)).
- Keep operational “how to” in `ops.md`; keep architecture/checklist in `plan-core-uwp.md`.
