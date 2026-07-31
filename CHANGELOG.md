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

- Auto-restart bitcoind on **resume** after Xbox Home suspend (if it was running)
- UI tip age from `mediantime`; pills **HEADERS** / **STALE** (operational consensus)
- `scripts/health-check.sh` (+ `deploy.sh health`) — ops hygiene one-shot (exit 0/1/2)
- Conf profiles: `config/bitcoin.conf.tip`, `config/README.md`; `apply-console-conf.sh --profile`
- Pre-Lightning plan: `docs/pre-lightning.md`

### Changed

- Architecture doc: lifecycle, host ops plane, UI consensus scope ([plan-core-uwp.md](docs/plan-core-uwp.md))
- README architecture diagram + features aligned with SSOT (resume, tip age, pills)
- Document Home suspend: Game class does not keep IBD running in background
- Ops hygiene / best practices section in `docs/ops.md`
- `apply-console-conf` guards tip profile until progress ≥ 0.99 (override `--force`)
- Docs screenshot refreshed for package **0.1.0.6** / release **v0.1.1**
- Tracking/roadmap: architecture+UI marked complete on main; live IBD snapshot

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
  + `scripts/apply-console-conf.sh`
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
