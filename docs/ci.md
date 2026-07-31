# Continuous Integration (GitHub Actions)

CI validates the **pinned Bitcoin Core** tree and the UWP package. It does **not**
deploy to the Xbox (use `scripts/deploy.sh` locally).

## Workflows (separated, no intentional overlap)

| Workflow | Runner(s) | What it proves | What it does **not** do |
|----------|-----------|----------------|-------------------------|
| [`ci-linux.yml`](../.github/workflows/ci-linux.yml) | `ubuntu-24.04` | shellcheck, UI layout tests, pin + `xbb_version.generated.h` sync, optional Linux smoke | MSVC, UWP, Xbox |
| [`ci-msvc-baseline.yml`](../.github/workflows/ci-msvc-baseline.yml) | `windows-2025-vs2026` | Desktop MSVC pin (unpatched) | UWP / MSIX |
| [`build-uwp.yml`](../.github/workflows/build-uwp.yml) | scaffold `windows-2022`; core/package `windows-2025-vs2026` | UWP product pipeline (also `workflow_call`) | Desktop MSVC |
| [`release.yml`](../.github/workflows/release.yml) | calls `build-uwp` + `ubuntu-24.04` publish | Tag `v*` → MSIX + GitHub Release | Xbox deploy |

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

## Action versions (latest majors as of 2026-07-31)

| Action | Pin | Latest tag |
|--------|-----|------------|
| `actions/checkout` | `v7` | `v7.0.1` |
| `actions/upload-artifact` | `v7` | `v7.0.1` |
| `actions/download-artifact` | `v8` | `v8.0.1` |
| `actions/cache` | `v6` | `v6.1.0` |
| `actions/cache/restore` | `v6` | (same) |
| `actions/cache/save` | `v6` | (same) |
| `dorny/paths-filter` | `v4` | `v4.0.2` |
| `microsoft/setup-msbuild` | `v3` | `v3.0.0` |
| `nuget/setup-nuget` | `v4` | `v4.0` |

Dependabot (`.github/dependabot.yml`) opens weekly PRs for further bumps.
Floating major tags (`@v7`) track the latest compatible patch automatically.

## Triggers

- `push` / `pull_request`: path filters only  
- `workflow_dispatch`: manual (UWP: `with_core`, `force_core`)

Concurrency: one run per workflow + branch; newer cancels older.

## Warnings policy

CI should **fail** on product quality, not on vendor/runner chatter.

| Class | Examples | Policy |
|-------|----------|--------|
| **Fail** | MSVC/UWP compile errors, shellcheck, missing MSIX, pin malformed, `HEAD ≠ COMMIT` | Hard fail |
| **Signal** | Bitcoin Core pin behind GitHub `latest` release | `::warning` only (bump is a deliberate product decision) |
| **Quiet / avoid** | VS Installer channel-feed cancels (`aka.ms/vs/channels`), git annotated-tag noise | Fix at source (see below) |
| **Accept / document** | Upstream vcpkg `vcpkg_replace_string made no changes` (cold cache) | Ignore; disappears with cache hit |

**How we keep logs clean:**

1. **Fetch pin by `COMMIT`** (`scripts/fetch-bitcoin-core.{sh,ps1}`) — never shallow-clone the annotated tag alone, so git does not print `refs/tags/v… is not a commit!`.
2. **UWP workload step asserts first** (`package-uwp`): multi-probe `vswhere` + package catalog + on-disk MSBuild UWP targets; only if all fail does it run `setup.exe modify`, and installer stdout goes to `$RUNNER_TEMP/vs-uwp-modify.log` (printed only on non-zero exit).
3. **Pin lag** stays a soft annotation in `ci-linux` lint — do not fail the workflow when Core publishes a newer tag.

If you see long `setup.exe` / channel-feed stacks again, the detection probes regressed or the runner image dropped UWP.VC — treat that as an infrastructure bug, not a product failure.

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

## Releases (automated)

| Trigger | Effect |
|---------|--------|
| `git push origin vX.Y.Z` | `release.yml`: build WithCore MSIX → GitHub Release + assets |
| Actions → **release** → Run workflow | Rebuild/publish for an **existing** tag |

`build-uwp` reusable jobs must allow **tag refs** (`refs/tags/v*`) as well as `main`
(caller context keeps `event_name=push` on the tag, not `workflow_call`).

Local helper:

```bash
./scripts/cut-release.sh 0.2.0           # tag + push → CI release
./scripts/cut-release.sh 0.2.0 --dry-run
```

Requirements: clean tree; tag must not already exist. Package revision uses
`GITHUB_RUN_NUMBER` (manifest base stays `0.1.0.0` unless you bump AppxManifest).

Manual first release was `v0.1.0`; later tags should use this path only.

## Pin bumps

1. Edit `config/bitcoin-core.pin` (`TAG` + peeled `COMMIT`)  
2. `./scripts/fetch-bitcoin-core.sh` (or `.ps1` on Windows)  
3. `./scripts/generate-version-header.py` — updates UI Core version header (also run by `build-uwp.ps1`)  
4. Commit pin + generated header  
5. Push → linux smoke + msvc + full core rebuild (new cache keys)  
6. Update baseline docs if needed  

`ci-linux` fails if `uwp/xbb_version.generated.h` does not match the pin.

### Version labels (Core vs app)

| Label | Source |
|-------|--------|
| Bitcoin Core **v31.1** | `config/bitcoin-core.pin` → `scripts/generate-version-header.py` → `uwp/xbb_version.generated.h` |
| App **0.1.0.N** | MSIX identity; CI stamps revision with `GITHUB_RUN_NUMBER` |

UI subtitle format: `Bitcoin Core v31.1 · app 0.1.0.N` ([ui.md](ui.md)).

## Badges

Root [README](../README.md) (`gianlucamazza/xbox_bitcoind`).
