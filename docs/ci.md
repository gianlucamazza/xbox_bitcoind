# Continuous Integration (GitHub Actions)

CI validates the **pinned Bitcoin Core** tree and the UWP package. It does **not**
deploy to the Xbox (use `scripts/deploy.sh` locally).

## Workflows (separated, no intentional overlap)

| Workflow                                                                | Runner(s)                                           | What it proves                                                                                                                                                                          | What it does **not** do |
| ----------------------------------------------------------------------- | --------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------- |
| [`ci-linux.yml`](../.github/workflows/ci-linux.yml)                     | `ubuntu-24.04`                                      | lint wall (shellcheck `-S style`, actionlint, PSScriptAnalyzer, ruff), UI layout + JSON extractor tests, conf-fallback sync, pin + `xbb_version.generated.h` sync, optional Linux smoke | MSVC, UWP, Xbox         |
| [`ci-msvc-baseline.yml`](../.github/workflows/ci-msvc-baseline.yml)     | `windows-2025-vs2026`                               | Desktop MSVC pin (unpatched)                                                                                                                                                            | UWP / MSIX              |
| [`build-uwp.yml`](../.github/workflows/build-uwp.yml)                   | patch-check `ubuntu-24.04`; scaffold `windows-2022` | Patch set applies on the pin (PRs too), scaffold MSIX; delegates product to `build-product-msix`                                                                                        | Desktop MSVC            |
| [`build-product-msix.yml`](../.github/workflows/build-product-msix.yml) | `windows-2025-vs2026`                               | Core UWP libs + WithCore MSIX (single source; called by `build-uwp` and `release`)                                                                                                      | PR gates                |
| [`release.yml`](../.github/workflows/release.yml)                       | calls `build-product-msix` + `ubuntu-24.04` publish | Tag `v*` → MSIX + `SHA256SUMS` + GitHub Release                                                                                                                                         | Xbox deploy             |

**Desktop MSVC ≠ UWP Core:** same pin, different targets (`x64-windows` vs
WindowsStore `x64-uwp` + patches). They only co-fire on pin / shared fetch changes.

## Build pipeline split (speed)

Local and CI use the **same stage boundaries**:

```text
fetch pin → apply patches → build-core-uwp  →  build-uwp -WithCore -SkipCoreBuild
              (slow, cached)                    (fast MSIX package)
```

| Stage         | Script                                           | Typical cost              |
| ------------- | ------------------------------------------------ | ------------------------- |
| Core UWP libs | `build-core-uwp.ps1` / `build-uwp.ps1 -CoreOnly` | high (minutes–hours cold) |
| Package MSIX  | `build-uwp.ps1 -WithCore -SkipCoreBuild`         | low–medium                |
| Scaffold only | `build-uwp.ps1`                                  | low                       |

**SkipIfFresh:** when pin + `patches/uwp/*.patch` stamp matches and
`bitcoin_embed.lib` + `xbb-core-libs.props` exist, Core rebuild is a no-op.
CI sets `XBB_CORE_SKIP_IF_FRESH=1`. Force with `-Force` / dispatch `force_core`.

### `build-uwp` jobs

| Job            | When                               | Work                                         |
| -------------- | ---------------------------------- | -------------------------------------------- |
| `patch-check`  | always (PRs included)              | Linux: pinned fetch + full patch-set apply   |
| `uwp-scaffold` | PR (or dispatch `with_core=false`) | MSIX without Core                            |
| `product`      | main push / dispatch WithCore      | calls `build-product-msix.yml` (Core → MSIX) |

App-only changes (`uwp/**`) still run the product path, but **core is SkipIfFresh**
when the pin/patches cache is warm → package stage dominates wall time.

## Path filters

| Workflow           | Starts when                                                                                                    |
| ------------------ | -------------------------------------------------------------------------------------------------------------- |
| `ci-linux`         | `scripts/**`, pin, `config/bitcoin.conf.console`, tested `uwp/` headers + conf-fallback sources, version-SSOT docs (README, CHANGELOG, tracking/console/plan snapshots), workflow file |
| `ci-msvc-baseline` | pin, MSVC fetch/build scripts, workflow file                                                                   |
| `build-uwp`        | `uwp/**`, pin, `patches/**`, UWP build/fetch/apply scripts, both product workflow files                        |

**No CI:** most `docs/**`, `LICENSE`, `config/xbox-env.example` — except the version-SSOT docs above, which run the (fast) lint job.

### Within `ci-linux`

| Job     | When                                                                                                                                                                                    |
| ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `lint`  | every workflow start — shellcheck `-S style`, actionlint (pinned), PSScriptAnalyzer (`PSScriptAnalyzerSettings.psd1`), ruff (`.ruff.toml`), host-side unit tests, conf-sync, pin checks |
| `smoke` | pin / fetch / build-linux-smoke / workflow (or dispatch)                                                                                                                                |

Smoke uses **Ninja + ccache** when available; caches build dir + `.ccache-linux-smoke`.

### Within `ci-msvc-baseline`

Caches full `build-msvc-baseline` tree + vcpkg archives; `cmake --build --parallel`.

## Action versions (latest majors as of 2026-07-31)

| Action                            | Pin                 | Latest tag               |
| --------------------------------- | ------------------- | ------------------------ |
| `actions/checkout`                | `v7`                | `v7.0.1`                 |
| `actions/upload-artifact`         | `v7`                | `v7.0.1`                 |
| `actions/download-artifact`       | `v8`                | `v8.0.1`                 |
| `actions/cache`                   | `v6`                | `v6.1.0`                 |
| `actions/cache/restore`           | `v6`                | (same)                   |
| `actions/cache/save`              | `v6`                | (same)                   |
| `dorny/paths-filter`              | commit SHA (`# v4`) | third-party → SHA-pinned |
| `microsoft/setup-msbuild`         | `v3`                | `v3.0.0`                 |
| `nuget/setup-nuget`               | `v4`                | `v4.0`                   |
| `actions/attest-build-provenance` | `v3`                | release provenance       |

Dependabot (`.github/dependabot.yml`) opens weekly PRs for further bumps
(github-actions and `uwp/` NuGet). Floating major tags (`@v7`) track the latest
compatible patch automatically; the one non-vendor action stays SHA-pinned.

## Triggers

- `push` / `pull_request`: path filters only
- `workflow_dispatch`: manual (UWP: `with_core`, `force_core`)

Concurrency: `ci-linux` / `ci-msvc-baseline` — one run per workflow + branch, newer
cancels older. `build-uwp` cancels only PR runs (main pushes queue). `release` and
`build-product-msix` never cancel in-progress runs.

## Warnings policy

CI should **fail** on product quality, not on vendor/runner chatter.

| Class                 | Examples                                                                          | Policy                                                   |
| --------------------- | --------------------------------------------------------------------------------- | -------------------------------------------------------- |
| **Fail**              | MSVC/UWP compile errors, shellcheck, missing MSIX, pin malformed, `HEAD ≠ COMMIT` | Hard fail                                                |
| **Signal**            | Bitcoin Core pin behind GitHub `latest` release                                   | `::warning` only (bump is a deliberate product decision) |
| **Quiet / avoid**     | VS Installer channel-feed cancels (`aka.ms/vs/channels`), git annotated-tag noise | Fix at source (see below)                                |
| **Accept / document** | Upstream vcpkg `vcpkg_replace_string made no changes` (cold cache)                | Ignore; disappears with cache hit                        |

**How we keep logs clean:**

1. **Fetch pin by `COMMIT`** (`scripts/fetch-bitcoin-core.{sh,ps1}`) — never shallow-clone the annotated tag alone, so git does not print `refs/tags/v… is not a commit!`.
2. **UWP workload step asserts first** (`package-uwp`): multi-probe `vswhere` + package catalog + on-disk MSBuild UWP targets; only if all fail does it run `setup.exe modify`, and installer stdout goes to `$RUNNER_TEMP/vs-uwp-modify.log` (printed only on non-zero exit).
3. **Pin lag** stays a soft annotation in `ci-linux` lint — do not fail the workflow when Core publishes a newer tag.

If you see long `setup.exe` / channel-feed stacks again, the detection probes regressed or the runner image dropped UWP.VC — treat that as an infrastructure bug, not a product failure.

## Test policy

| Event                   | Linux unit tests | MSVC `ctest`         |
| ----------------------- | ---------------- | -------------------- |
| PR                      | off              | **off**              |
| push `main` (msvc runs) | off default      | **on**               |
| `workflow_dispatch`     | input            | input (default true) |

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

## Artifacts

| Name                          | From                             | Retention |
| ----------------------------- | -------------------------------- | --------- |
| `bitcoind-linux-x64`          | smoke                            | 7 days    |
| `bitcoind-msvc-x64`           | msvc                             | 7 days    |
| `xbox_bitcoind-msix-scaffold` | PR scaffold                      | 7 days    |
| `xbox_bitcoind-msix-core`     | build-product-msix `package-uwp` | 14 days   |

## Expected minutes

| Change                          | Cost class                           |
| ------------------------------- | ------------------------------------ |
| `docs/**`                       | free                                 |
| `scripts/deploy.sh`             | lint only (seconds)                  |
| `uwp/**` only (warm Core cache) | package-uwp medium; core SkipIfFresh |
| pin / patches                   | core rebuild high + package          |
| MSVC script / pin               | msvc high                            |

## Releases (automated)

| Trigger                              | Effect                                                                    |
| ------------------------------------ | ------------------------------------------------------------------------- |
| `git push origin vX.Y.Z`             | `release.yml` → **`build-product-msix.yml`** (Core+MSIX) → GitHub Release |
| Actions → **release** → Run workflow | Rebuild/publish for an **existing** tag                                   |
| Actions → **build-product-msix**     | Manual product MSIX without cutting a tag                                 |

Day-to-day app iteration stays on **`build-uwp.yml`** (path-filtered, SkipIfFresh).  
Releases use **`build-product-msix.yml`** (no PR/path gates; always Core + package).

Local helper:

```bash
./scripts/cut-release.sh 0.2.0           # tag + push → CI release
./scripts/cut-release.sh 0.2.0 --dry-run
./scripts/cut-release.sh 0.2.0 --force   # skip the blocking CHANGELOG gate
```

Requirements: clean tree; tag must not already exist; `CHANGELOG.md` must have a
`## [X.Y.Z]` section (release notes embed it). **MSIX version derives from the
tag**: `vX.Y.Z` → package `X.Y.Z.(10000+run_number)`, so the installed version
maps back to the release. Dev builds from `build-uwp.yml` keep the manifest base.

Release assets ship `SHA256SUMS` and a build **provenance attestation**
(`actions/attest-build-provenance` on the MSIX + checksums; verify with
`gh attestation verify <file> -R gianlucamazza/xbox_bitcoind`).

Manual first release was `v0.1.0`; later tags should use this path only.

## Pin bumps

Full procedure (single source): **[upgrade.md](upgrade.md)**. CI angle:
`ci-linux` fails if `uwp/xbb_version.generated.h` does not match the pin, and the
`patch-check` job fails the PR if the patch set no longer applies on the new pin.

### Version labels (Core vs app)

| Label                  | Source                                                                                                                        |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| Bitcoin Core **v31.1** | `config/bitcoin-core.pin` → `scripts/generate-version-header.py` → `uwp/xbb_version.generated.h`                              |
| App **X.Y.Z.N**        | MSIX identity; releases stamp `X.Y.Z` from the tag + `N = 10000+run_number`; dev builds keep the manifest base + `run_number` |

UI subtitle format: `Bitcoin Core v31.1 · app X.Y.Z.N` ([ui.md](ui.md)).

## Badges

Root [README](../README.md) (`gianlucamazza/xbox_bitcoind`).
