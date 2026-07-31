# Continuous Integration (GitHub Actions)

CI validates the **pinned Bitcoin Core** tree and the UWP package. It does **not**
deploy to the Xbox (use `scripts/deploy.sh` locally).

## Workflows (separated, no intentional overlap)

| Workflow | Runner(s) | What it proves | What it does **not** do |
|----------|-----------|----------------|-------------------------|
| [`ci-linux.yml`](../.github/workflows/ci-linux.yml) | `ubuntu-24.04` | shellcheck, pin, optional Linux smoke | MSVC, UWP, Xbox |
| [`ci-msvc-baseline.yml`](../.github/workflows/ci-msvc-baseline.yml) | `windows-2025-vs2026` | Desktop MSVC pin (unpatched) | UWP / MSIX |
| [`build-uwp.yml`](../.github/workflows/build-uwp.yml) | scaffold `windows-2022`; core/package `windows-2025-vs2026` | UWP product pipeline | Desktop MSVC |

**Desktop MSVC ≠ UWP Core:** same pin, different targets (`x64-windows` vs
WindowsStore `x64-uwp` + patches). They only co-fire on pin / shared fetch changes.

## Build pipeline split (speed)

Local and CI use the **same stage boundaries**:

```text
fetch pin → apply patches → build-core-uwp  →  build-uwp -WithCore -SkipCoreBuild
              (slow, cached)                    (fast MSIX package)
```

| Stage | Script | Typical cost |
|-------|--------|--------------|
| Core UWP libs | `build-core-uwp.ps1` / `build-uwp.ps1 -CoreOnly` | high (minutes–hours cold) |
| Package MSIX | `build-uwp.ps1 -WithCore -SkipCoreBuild` | low–medium |
| Scaffold only | `build-uwp.ps1` | low |

**SkipIfFresh:** when pin + `patches/uwp/*.patch` stamp matches and
`bitcoin_embed.lib` + `xbb-core-libs.props` exist, Core rebuild is a no-op.
CI sets `XBB_CORE_SKIP_IF_FRESH=1`. Force with `-Force` / dispatch `force_core`.

### `build-uwp` jobs

| Job | When | Work |
|-----|------|------|
| `uwp-scaffold` | PR (or dispatch `with_core=false`) | MSIX without Core |
| `core-uwp` | main push / dispatch WithCore | Core libs only; cache save per `run_id` |
| `package-uwp` | after `core-uwp` | restore that cache → MSIX only |

App-only changes (`uwp/**`) still run both jobs, but **core is SkipIfFresh** when
the pin/patches cache is warm → package stage dominates wall time.

## Path filters

| Workflow | Starts when |
|----------|-------------|
| `ci-linux` | `scripts/**`, pin, workflow file |
| `ci-msvc-baseline` | pin, MSVC fetch/build scripts, workflow file |
| `build-uwp` | `uwp/**`, pin, `patches/**`, UWP build/fetch/apply scripts, workflow file |

**No CI:** `docs/**`, `README.md`, `LICENSE`, `config/bitcoin.conf.console`,
`config/xbox-env.example`.

### Within `ci-linux`

| Job | When |
|-----|------|
| `lint` | every workflow start |
| `smoke` | pin / fetch / build-linux-smoke / workflow (or dispatch) |

Smoke uses **Ninja + ccache** when available; caches build dir + `.ccache-linux-smoke`.

### Within `ci-msvc-baseline`

Caches full `build-msvc-baseline` tree + vcpkg archives; `cmake --build --parallel`.

## Action versions

| Action | Pin |
|--------|-----|
| `actions/checkout` | `v7` |
| `actions/upload-artifact` | `v7` |
| `actions/cache` / `restore` / `save` | `v6` / `v4` |
| `dorny/paths-filter` | `v3` |

Dependabot (`.github/dependabot.yml`) bumps Actions weekly.

## Triggers

- `push` / `pull_request`: path filters only  
- `workflow_dispatch`: manual (UWP: `with_core`, `force_core`)

Concurrency: one run per workflow + branch; newer cancels older.

## Test policy

| Event | Linux unit tests | MSVC `ctest` |
|-------|------------------|--------------|
| PR | off | **off** |
| push `main` (msvc runs) | off default | **on** |
| `workflow_dispatch` | input | input (default true) |

## Local equivalents

```bash
# Linux smoke (Ninja+ccache if installed)
./scripts/fetch-bitcoin-core.sh
CI_SKIP_TESTS=1 ./scripts/build-linux-smoke.sh
```

```powershell
# Split UWP (recommended iterate)
.\scripts\fetch-bitcoin-core.ps1
.\scripts\build-uwp.ps1 -CoreOnly                          # slow once
.\scripts\build-uwp.ps1 -WithCore -SkipCoreBuild           # package only
# or monolithic:
.\scripts\build-uwp.ps1 -WithCore

# Desktop MSVC
.\scripts\build-msvc-baseline.ps1 -SkipTests
```

## Artifacts (7 days)

| Name | From |
|------|------|
| `bitcoind-linux-x64` | smoke |
| `bitcoind-msvc-x64` | msvc |
| `xbox_bitcoind-msix-scaffold` | PR scaffold |
| `xbox_bitcoind-msix-core` | package-uwp |

## Expected minutes

| Change | Cost class |
|--------|------------|
| `docs/**` | free |
| `scripts/deploy.sh` | lint only (seconds) |
| `uwp/**` only (warm Core cache) | package-uwp medium; core SkipIfFresh |
| pin / patches | core rebuild high + package |
| MSVC script / pin | msvc high |

## Pin bumps

1. Edit `config/bitcoin-core.pin`  
2. Push → linux smoke + msvc + full core rebuild (new cache keys)  
3. Update baseline docs if needed  

## Badges

Root [README](../README.md) (`gianlucamazza/xbox_bitcoind`).
