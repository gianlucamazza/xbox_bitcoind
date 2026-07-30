# Continuous Integration (GitHub Actions)

Phase A CI builds the **pinned Bitcoin Core** tree and verifies desktop baselines.
It does **not** deploy to the Xbox (use `scripts/deploy.sh` locally).

## Workflows

| Workflow | Runner | Purpose |
|----------|--------|---------|
| [`ci-linux.yml`](../.github/workflows/ci-linux.yml) | `ubuntu-24.04` | shellcheck, pin validation, Linux `bitcoind` smoke |
| [`ci-msvc-baseline.yml`](../.github/workflows/ci-msvc-baseline.yml) | `windows-2025-vs2026` | MSVC + vcpkg desktop baseline (VS 2026) |

Future (after `uwp/` exists): `build-uwp.yml` → MSIX artifact (no auto-deploy).

## Action versions (keep current)

| Action | Pin |
|--------|-----|
| `actions/checkout` | `v7` |
| `actions/upload-artifact` | `v7` |
| `actions/cache` | `v6` |

Dependabot (`.github/dependabot.yml`) opens weekly PRs for Action bumps.
Lint also compares `config/bitcoin-core.pin` `TAG` to the latest GitHub release (warning only if behind).

## Triggers

- `push` to `main`
- all `pull_request`s
- `workflow_dispatch` (manual, with inputs)

Concurrency: one run per workflow + branch; newer pushes cancel older runs.

## Test policy

| Event | Linux unit tests | MSVC `ctest` |
|-------|------------------|--------------|
| PR | off (`CI_SKIP_TESTS=1`) | **off** |
| push `main` | off by default | **on** |
| `workflow_dispatch` | input `run_tests` | input `run_tests` (default true) |

Wallet stays **off** unless dispatch sets `enable_wallet`.

## Runner note (MSVC)

Bitcoin Core **v31.1** CMake preset is `vs2026` only. The job pins
`windows-2025-vs2026` (VS 2026 Enterprise, Native Desktop, system vcpkg at
`C:\vcpkg`). Do **not** use `windows-2022` for this workflow.

## Local equivalents

```bash
# Linux
./scripts/fetch-bitcoin-core.sh
CI_SKIP_TESTS=1 ./scripts/build-linux-smoke.sh
```

```powershell
# Windows (Developer PowerShell for VS / GHA-equivalent)
.\scripts\fetch-bitcoin-core.ps1
.\scripts\build-msvc-baseline.ps1            # with tests
.\scripts\build-msvc-baseline.ps1 -SkipTests # PR-like
```

## Artifacts

| Name | Contents |
|------|----------|
| `bitcoind-linux-x64` | `bitcoind`, `bitcoin-cli` |
| `bitcoind-msvc-x64` | `bitcoind.exe`, `bitcoin-cli.exe` |

Retention: 14 days. Download from the Actions run UI.

## Caching

MSVC job caches:

- `third_party/bitcoin/build-msvc-baseline/vcpkg_installed`
- `~/AppData/Local/vcpkg/archives`

Cache key includes `config/bitcoin-core.pin` and Core’s `vcpkg.json` after fetch.
Cold first run can take **1–3 hours**; warm runs should be much faster.

## Secrets

Phase A: **none**.

Later (UWP): signing cert / password (document when `build-uwp.yml` lands).
Device Portal credentials stay on the operator machine (`~/.config/xllama/xbox-env`).

## Pin bumps

1. Edit `config/bitcoin-core.pin` (`TAG` + peeled `COMMIT`).
2. Push; CI re-fetches Core and rebuilds (new cache key).
3. Update `docs/build-msvc-baseline.md` and spike log if needed.

## Badges

See [README](../README.md) (replace `OWNER/REPO` if the remote path differs).
