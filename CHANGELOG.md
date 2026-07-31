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

- `docs/tracking.md` + GitHub Issue templates (`bug`, `ops`) and labels
- Responsive 10-foot dashboard: scale-aware density, primary/secondary metrics,
  dual progress bars, session sparkline, title-safe budget layout
- IBD console conf defaults (`dbcache=512`, `maxconnections=16`, `blocksonly=1`)
  + `scripts/apply-console-conf.sh`
- Off-LAN console access via standard OpenSSH LocalForward (Odroid Tailscale)
- Auto-start node after probes (WithCore)
- `SECURITY.md`, this changelog, `NOTICE` (since v0.1.0)
- Release automation, ops tooling, hourly IBD timer, `v1-close-check.sh`
- Path-filtered CI; Core vs MSIX build split

### Changed

- Probes no longer overwrite operator `bitcoin.conf`
- Fetch Bitcoin Core by pin **COMMIT** (quiet annotated-tag noise)
- package-uwp: assert UWP workload before VS installer when possible
- Soft-stop UI state (`STOPPING`, “Stop soft”); richer RPC metrics
- Docs reconciled to package **0.1.0.63** / live IBD tracking

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
