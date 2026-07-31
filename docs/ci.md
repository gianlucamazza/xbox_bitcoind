# Continuous Integration (GitHub Actions)

CI builds the **pinned Bitcoin Core** tree (desktop + UWP WithCore) and the
UWP scaffold. It does **not** deploy to the Xbox (use `scripts/deploy.sh` locally).

## Workflows

| Workflow | Runner | Purpose |
|----------|--------|---------|
| [`ci-linux.yml`](../.github/workflows/ci-linux.yml) | `ubuntu-24.04` | shellcheck, pin validation, Linux `bitcoind` smoke |
| [`ci-msvc-baseline.yml`](../.github/workflows/ci-msvc-baseline.yml) | `windows-2025-vs2026` | MSVC + vcpkg desktop baseline (VS 2026) |
| [`build-uwp.yml`](../.github/workflows/build-uwp.yml) | scaffold: `windows-2022`; **core: `windows-2025-vs2026`** | Scaffold MSIX; WithCore links pin on VS 2026 |

No auto-deploy to the console; download the MSIX artifact and use `scripts/deploy.sh`.

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

## Runner note (MSVC / UWP Core)

Bitcoin Core **v31.1** requires **Visual Studio 2026 18.3+** (consteval C++20).

| Job | Runner | Reason |
|-----|--------|--------|
| `ci-msvc-baseline` | `windows-2025-vs2026` | Core desktop pin |
| `uwp-core` | `windows-2025-vs2026` | same MSVC; AppContainer Core + MSIX WithCore |
| `uwp-scaffold` | `windows-2022` | UWP v143 workload preinstalled (no Core) |

Do **not** build Core (desktop or UWP) on VS2022 — expect **C7595** and other
C++20 failures. `uwp-core` may install Universal/UWP.VC components on the
VS2026 image when missing.

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
| `xbox_bitcoind-msix-scaffold` | Scaffold UWP MSIX + test cert |
| `xbox_bitcoind-msix-core` | WithCore MSIX (`uwp-core` job) |

Download the MSIX from the Actions run and install with `./scripts/deploy.sh <msix>`.
| `xbox_bitcoind-msix-core` | WithCore MSIX (when `uwp-core` green) |

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
