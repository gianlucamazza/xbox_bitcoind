# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to follow [Semantic Versioning](https://semver.org/) for
**git tags / GitHub Releases** (`vMAJOR.MINOR.PATCH`).

> **Note:** The UWP package identity version (`Major.Minor.Build.Revision`) may use
> CI `run_number` as the fourth component (e.g. `0.1.0.42`). That revision is
> **not** the same as the git tag.

## [Unreleased]

### Added

- `SECURITY.md`, this changelog, `NOTICE`
- Release automation (`.github/workflows/release.yml`, `scripts/cut-release.sh`)
- README / docs restructure, `CONTRIBUTING.md`
- Ops tooling (`node-status`, `soft-stop-test`, `ibd-sample`, `ibd-report`)
- Hourly **user systemd** IBD sampler (`install-ibd-timer.sh`, `contrib/systemd/user/`)
- Path-filtered CI; Core vs MSIX build split (`SkipIfFresh`, `workflow_call`)

### Changed

- GitHub Actions majors (paths-filter v4, setup-msbuild v3, setup-nuget v4, cache v6)
- `ibd-sample.sh`: soft-fail for timers, milestone log, stuck-tip detection

## [0.1.0] — 2026-07-31

First public release.

### Added

- Bitcoin Core **v31.1** in-process embed for UWP (AppContainer patches 0001–0010)
- Status dashboard (RPC metrics, Start/Stop, log tail)
- Soft-stop lifecycle (suspend → RPC stop → LevelDB flush)
- MSIX package `GianlucaMazza.xboxbitcoind` (WithCore), e.g. revision **0.1.0.42**
- Device Portal deploy scripts and docs (`docs/ops.md`, plan, CI)

### Limits (v0.1)

- Dev Mode only; no Store submission  
- Mainnet IBD can take a long time on a fresh datadir  
- No wallet UI; `listen=0` by default  

[Unreleased]: https://github.com/gianlucamazza/xbox_bitcoind/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/gianlucamazza/xbox_bitcoind/releases/tag/v0.1.0
