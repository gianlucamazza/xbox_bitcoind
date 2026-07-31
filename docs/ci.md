# Continuous Integration (GitHub Actions)

CI validates the **pinned Bitcoin Core** tree and the UWP package. It does **not**
deploy to the Xbox (use `scripts/deploy.sh` locally).

## Workflows (no intentional overlap)

| Workflow | Runner(s) | What it proves | What it does **not** do |
|----------|-----------|----------------|-------------------------|
| [`ci-linux.yml`](../.github/workflows/ci-linux.yml) | `ubuntu-24.04` | shellcheck, pin file, optional Linux `bitcoind` smoke | MSVC, UWP, Xbox deploy |
| [`ci-msvc-baseline.yml`](../.github/workflows/ci-msvc-baseline.yml) | `windows-2025-vs2026` | Desktop MSVC pin (unpatched Core) | UWP / AppContainer / MSIX |
| [`build-uwp.yml`](../.github/workflows/build-uwp.yml) | scaffold: `windows-2022`; core: `windows-2025-vs2026` | UWP MSIX (scaffold on PR; WithCore on main) | Desktop MSVC baseline |

**Desktop MSVC and UWP Core are not duplicates:** same pin, different targets
(`x64-windows` vs WindowsStore `x64-uwp` + patches). Both are required for different
risks; they only co-fire when the **pin** (or shared fetch scripts) changes.

## Path filters (cost control)

Workflows **do not start** on docs-only / unrelated paths:

| Workflow | Starts when these change |
|----------|--------------------------|
| `ci-linux` | `scripts/**`, `config/bitcoin-core.pin`, this workflow file |
| `ci-msvc-baseline` | pin, `fetch-bitcoin-core.ps1`, `build-msvc-baseline.ps1`, this workflow file |
| `build-uwp` | `uwp/**`, pin, `patches/**`, UWP build/fetch/apply scripts, this workflow file |

Not in any filter (no CI minutes): `docs/**`, `README.md`, `LICENSE`,
`config/bitcoin.conf.console`, `config/xbox-env.example`.

### Within `ci-linux`

| Job | When |
|-----|------|
| `lint` | every time the workflow runs (shellcheck + pin syntax) |
| `smoke` | only if pin / `fetch-bitcoin-core.sh` / `build-linux-smoke.sh` / workflow file changed (or `workflow_dispatch`) |

Editing `scripts/deploy.sh` alone → lint only, **no** Core rebuild.

### Within `build-uwp`

| Job | When |
|-----|------|
| `uwp-scaffold` | **PR** only (or dispatch with `with_core=false`) |
| `uwp-core` | **push to main** (path match) or dispatch with `with_core=true` |

On main, scaffold is skipped so a path-matching push does **not** pay for two
Windows UWP packages. Product artifact is `xbox_bitcoind-msix-core`.

## Action versions

| Action | Pin |
|--------|-----|
| `actions/checkout` | `v7` |
| `actions/upload-artifact` | `v7` |
| `actions/cache` | `v6` |
| `dorny/paths-filter` | `v3` (ci-linux smoke gate) |

Dependabot (`.github/dependabot.yml`) opens weekly PRs for Action bumps.

## Triggers summary

- `push` / `pull_request`: only if path filters match  
- `workflow_dispatch`: always available (manual full runs)

Concurrency: one run per workflow + branch; newer pushes cancel older runs.

## Test policy

| Event | Linux unit tests | MSVC `ctest` |
|-------|------------------|--------------|
| PR | off | **off** |
| push `main` (when msvc workflow runs) | off by default | **on** |
| `workflow_dispatch` | input `run_tests` | input `run_tests` (default true) |

Wallet stays **off** unless MSVC dispatch sets `enable_wallet`.

## Runner note

Bitcoin Core **v31.1** requires **Visual Studio 2026 18.3+** for Core builds.

| Job | Runner | Reason |
|-----|--------|--------|
| `ci-msvc-baseline` | `windows-2025-vs2026` | Core desktop pin |
| `uwp-core` | `windows-2025-vs2026` | AppContainer Core + MSIX WithCore |
| `uwp-scaffold` | `windows-2022` | UWP v143 workload (no Core) |

Do **not** build Core on VS2022 (C7595 / C++20).

## Local equivalents

```bash
# Linux
./scripts/fetch-bitcoin-core.sh
CI_SKIP_TESTS=1 ./scripts/build-linux-smoke.sh
shellcheck -x -S warning scripts/*.sh
```

```powershell
# Windows (Developer PowerShell for VS / GHA-equivalent)
.\scripts\fetch-bitcoin-core.ps1
.\scripts\build-msvc-baseline.ps1            # with tests
.\scripts\build-msvc-baseline.ps1 -SkipTests # PR-like
.\scripts\build-uwp.ps1 -WithCore
```

## Artifacts (retention 7 days)

| Name | Contents |
|------|----------|
| `bitcoind-linux-x64` | `bitcoind`, `bitcoin-cli` (smoke job) |
| `bitcoind-msvc-x64` | `bitcoind.exe`, `bitcoin-cli.exe` |
| `xbox_bitcoind-msix-scaffold` | Scaffold MSIX + test cert (PR) |
| `xbox_bitcoind-msix-core` | WithCore MSIX (`uwp-core`) |

```bash
./scripts/deploy.sh path/to/xbox_bitcoind_*.msix
```

## Caching

| Job | Cached paths | Key inputs |
|-----|--------------|------------|
| Linux smoke | `build-linux-smoke` | pin |
| MSVC baseline | `vcpkg_installed` + archives | pin + Core `vcpkg.json` |
| UWP core | `build-uwp` + vcpkg `x64-uwp` | pin + `patches/uwp/**` |

Cold first run can take **1–3 hours** on Windows; warm runs are much faster.

## Secrets

**None required in GitHub Actions.** MSIX is signed with a CI-generated dev cert
in the artifact. Device Portal credentials stay on the operator machine
(`~/.config/xllama/xbox-env`).

## Expected minutes (order of magnitude)

| Change type | Workflows that run | Cost class |
|-------------|--------------------|------------|
| `docs/**` only | none | free |
| `scripts/deploy.sh` | ci-linux lint only | seconds |
| pin bump | linux smoke + msvc + uwp-core | high (intentional) |
| `uwp/**` or `patches/**` | uwp-core (main) or scaffold (PR) | medium–high |
| MSVC script only | ci-msvc-baseline | high |

## Pin bumps

1. Edit `config/bitcoin-core.pin` (`TAG` + peeled `COMMIT`).
2. Push; all three workflow path filters match; rebuilds use new cache keys.
3. Update `docs/build-msvc-baseline.md` and spike log if needed.

## Badges

See root [README](../README.md) (`gianlucamazza/xbox_bitcoind`).
