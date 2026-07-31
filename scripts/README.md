# Scripts

All paths relative to the repo root. Credentials: `source scripts/env.sh`
(or `~/.config/xllama/xbox-env`).

## Release

| Script | Purpose |
|--------|---------|
| `cut-release.sh` | Annotated tag `vX.Y.Z` + push → `.github/workflows/release.yml` |

```bash
./scripts/cut-release.sh 0.2.0
./scripts/cut-release.sh 0.2.0 --dry-run
```

## Operate (Linux host → Series S)

| Script | Purpose |
|--------|---------|
| `env.sh` | Resolve Device Portal credentials + package identity |
| `probe-console.sh` | OS/packages smoke probe |
| `deploy.sh` | Install MSIX, start/stop, logs, **status**, **package-gc**, **soft-stop-test** |
| `node-status.sh` | IBD tip, RAM, datadir snapshot (`--json`, `--loop N`) |
| `soft-stop-test.sh` | Automated soft-stop persistence check |
| `ibd-sample.sh` | Append one JSONL sample (+ milestones / stuck detection) |
| `ibd-report.sh` | Summarize JSONL history, rate, rough ETA, errors |
| `install-ibd-timer.sh` | Enable hourly user systemd timer |
| `v1-close-check.sh` | Gate: IBD done + 24h stability (exit 0 = close v1 ops) |
| `apply-console-conf.sh` | Soft-stop → push `config/bitcoin.conf.console` → start (IBD knobs) |
| `test-ui-layout.sh` | Pure layout unit tests (no Xbox; used by ci-linux) |
| `generate-uwp-assets.py` | Rebuild splash/tiles from Core `share/pixmaps` icons |

```bash
# Off home LAN: standard SSH local forward (see docs/device-portal.md)
ssh -N -L 127.0.0.1:11443:192.168.1.44:11443 odroid-ts   # other terminal
export XBOX_IP_OVERRIDE=127.0.0.1

./scripts/deploy.sh status
./scripts/node-status.sh --loop 3600
./scripts/ibd-sample.sh                 # one line to state log
./scripts/soft-stop-test.sh
./scripts/deploy.sh stop-app            # soft stop only
./scripts/apply-console-conf.sh         # re-apply conf (dbcache/peers/blocksonly)
```

## Fetch pin

| Script | Platform |
|--------|----------|
| `fetch-bitcoin-core.sh` | Linux / Git Bash |
| `fetch-bitcoin-core.ps1` | Windows / GHA |

## Build (split stages)

```text
fetch → apply patches (via core script) → CoreOnly → package SkipCoreBuild
```

| Script | Purpose |
|--------|---------|
| `apply-uwp-patches.sh` / `.ps1` | Patch pin tree (idempotent marker) |
| `build-core-uwp.ps1` | Core UWP static libs (`-SkipIfFresh`, `-Force`) |
| `build-uwp.ps1` | Scaffold MSIX, or `-CoreOnly`, or `-WithCore` / `-SkipCoreBuild` |
| `build-msvc-baseline.ps1` | Desktop MSVC pin (`-SkipTests`, `-EnableWallet`) |
| `build-linux-smoke.sh` | Linux pin smoke (Ninja + ccache when available) |

### Recommended Windows iterate

```powershell
.\scripts\fetch-bitcoin-core.ps1
.\scripts\build-uwp.ps1 -CoreOnly                 # once / pin-patches change
.\scripts\build-uwp.ps1 -WithCore -SkipCoreBuild  # every UI/app change
```

Monolithic: `.\scripts\build-uwp.ps1 -WithCore`

CI layout: [docs/ci.md](../docs/ci.md). Ops: [docs/ops.md](../docs/ops.md).
