# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to follow [Semantic Versioning](https://semver.org/) for
**git tags / GitHub Releases** (`vMAJOR.MINOR.PATCH`).

> **Note:** The UWP package identity version (`Major.Minor.Build.Revision`) may use
> CI `run_number` as the fourth component (e.g. `0.1.0.42`). That revision is
> **not** the same as the git tag. Bitcoin Core pin (**v31.1**) is also separate
> from the app package revision.

## [Unreleased]

### Added

- `uwp/json_extract.h` — JSON extractors split out of `rpc_client.cpp`, host-testable via
  new `scripts/test-rpc-client.sh` (wired into ci-linux)

### Changed

- RPC: cookie cached and re-read once on HTTP 401; redundant `getconnectioncount` call
  dropped (getnetworkinfo already carries connections); PEERS shows `—` (unknown) instead
  of a red 0 when only `getnetworkinfo` fails

### Fixed

- Node `warnings` were never shown: Core v31.1 returns an array, the extractor only
  accepted a string; both forms are now parsed (joined for display)
- RPC error states distinguished in the status message: auth failure (`stale cookie?`) and
  warm-up (HTTP 503) no longer render as generic "RPC not ready"
- JSON result-object scan no longer miscounts braces inside string values

- UWP host concurrency: `NodeStart`/`NodeStop` serialized by a lifecycle mutex (concurrent
  `std::thread` join/detach was UB); resume now waits out an in-flight suspend soft-stop
  before restarting the node (fast Home in/out race)
- Dashboard could freeze permanently if a refresh worker threw (`m_refreshing` stuck);
  refresh flag is now atomic and re-armed on failure
- `NodeStart` (thread join + datadir I/O) no longer runs on the UI thread (Start button,
  post-probe auto-start)
- Data races: probe note now mutex-guarded; `LocalStatePath()` init is thread-safe
- Session sparkline/ETA history reset on node stop/start (pre-suspend samples poisoned the slope)

## [0.1.3] — 2026-08-01

Docs and field-verify closure after v0.1.2. Bitcoin Core pin remains **v31.1**.  
Product code same lineage as v0.1.2 (tip-age UI, IsRunning soft-stop, resume).

### Changed

- Mid-IBD soft-stop field verify: clean **8s** no DELETE, tip conserved (367530→367533); [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4) closed
- Console on **v0.1.2** package **0.1.0.10017** documented
- README + ops: measured **sync benchmarks** (throughput ~43k blk/h mid-IBD, WS ~0.7–1.1 GiB, soft-stop timing)

## [0.1.2] — 2026-08-01

Architecture/UI ops closure after v0.1.1. Bitcoin Core pin remains **v31.1**.

### Added

- Auto-restart bitcoind on **resume** after Xbox Home suspend (if it was running)
- UI tip age from `mediantime`; pills **HEADERS** / **STALE** (operational consensus)
- `scripts/health-check.sh` (+ `deploy.sh health`) — ops hygiene one-shot (exit 0/1/2)
- Conf profiles: `config/bitcoin.conf.tip`, `config/README.md`; `apply-console-conf.sh --profile`
- Pre-Lightning plan: `docs/pre-lightning.md`

### Changed

- Architecture SSOT: lifecycle state machine, host ops plane, UI consensus scope
- **`deploy.sh stop-app`**: Device Portal `IsRunning` (not ImageName alone); residual shell OK; DELETE only if still active ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4))
- `release.yml`: compute MSIX revision offset in shell (GHA expressions have no `+`)
- Document Home suspend: Game class does not keep IBD running in background
- Ops hygiene / best practices; conf tip profile guard until progress ≥ 0.99
- Console screenshot + tracking for package **0.1.0.75** path; Game class confirmed

## [0.1.1] — 2026-07-31

Ops polish, UI/branding, and operator tooling after first public release.
Bitcoin Core pin remains **v31.1**.

### Added

- `docs/tracking.md` + GitHub Issue templates (`bug`, `ops`) and labels
- Responsive 10-foot dashboard: scale-aware density, primary/secondary metrics,
  dual progress bars, session sparkline, title-safe budget layout
- Session **ETA** on progress label; **STOPPING Ns** elapsed soft-stop feedback
- Pure layout helpers `uwp/ui_layout.h` + `scripts/test-ui-layout.sh` (Linux CI)
- `ibd-report.sh` recent rate + rough height/progress ETA
- IBD console conf defaults (`dbcache=512`, `maxconnections=16`, `blocksonly=1`)
  - `scripts/apply-console-conf.sh`
- Off-LAN console access via standard OpenSSH LocalForward (Odroid Tailscale)
- Auto-start node after probes (WithCore)
- `scripts/generate-version-header.py` — pin → `uwp/xbb_version.generated.h`
- `scripts/generate-uwp-assets.py` — Core official icons for splash/tiles
- `deploy.sh package-list` / `package-gc` for multi-revision cleanup

### Changed

- Header subtitle: **Bitcoin Core v\<pin\> · app \<MSIX\>** (Core pin vs package identity)
- Splash/tile assets use Bitcoin Core official icons (`share/pixmaps`)
- Progress % display: **1 decimal** while syncing (e.g. `14.0%`), whole near tip
- Metric card label/value text optically centered
- Soft-stop host wait default **180s** (`XBB_SOFT_STOP_MAX_WAIT`), re-suspend at 45s;
  in-app node join wait **150s** (mid-IBD flush)
- Ops scripts: temp/logs under `$STATE_DIR`
- Probes no longer overwrite operator `bitcoin.conf`
- Fetch Bitcoin Core by pin **COMMIT** (quiet annotated-tag noise)
- package-uwp: assert UWP workload before VS installer when possible
- Docs: roadmap / tracking / ops / ci reconciled (console **0.1.0.65**, IBD in progress)

### Known limits (unchanged)

- Dev Mode only; mainnet IBD still wall-clock
- Mid-IBD soft-stop may still fall back to DELETE after long host wait (tip usually conserved)
- No wallet UI; `listen=0` by default

## [0.1.0] — 2026-07-31

First public release.

### Added

- Bitcoin Core **v31.1** in-process embed for UWP (AppContainer patches 0001–0010)
- Status dashboard (RPC metrics, Start/Stop, log tail)
- Soft-stop lifecycle (suspend → RPC stop → LevelDB flush)
- MSIX package `GianlucaMazza.xboxbitcoind` (WithCore), e.g. revision **0.1.0.42**
- Device Portal deploy scripts and docs (`docs/ops.md`, plan, CI)
- `SECURITY.md`, release automation, ops tooling, hourly IBD timer, `v1-close-check.sh`
- Path-filtered CI; Core vs MSIX build split

### Limits (v0.1)

- Dev Mode only; no Store submission
- Mainnet IBD can take a long time on a fresh datadir
- No wallet UI; `listen=0` by default

[Unreleased]: https://github.com/gianlucamazza/xbox_bitcoind/compare/v0.1.1...HEAD
[0.1.1]: https://github.com/gianlucamazza/xbox_bitcoind/releases/tag/v0.1.1
[0.1.0]: https://github.com/gianlucamazza/xbox_bitcoind/releases/tag/v0.1.0
