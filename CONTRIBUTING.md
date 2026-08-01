# Contributing

Thanks for interest in **xbox_bitcoind**. This project targets Xbox **Developer Mode** only.

## Before you start

1. Read the root [README](README.md) and [docs/README.md](docs/README.md).
2. Check open work: [docs/tracking.md](docs/tracking.md) and [GitHub Issues](https://github.com/gianlucamazza/xbox_bitcoind/issues).
3. For console work: [docs/ops.md](docs/ops.md) (soft-stop rules).
4. For Core/UWP changes: [docs/plan-core-uwp.md](docs/plan-core-uwp.md) and [patches/uwp/README.md](patches/uwp/README.md).

## Development setup

- **Scaffold UI only:** Windows + UWP C++ → `.\scripts\build-uwp.ps1`
- **WithCore:** Visual Studio **2026 18.3+**, vcpkg → see README “Build from source”
- **Host scripts:** Linux/macOS/Windows with Bash, `curl`, `python3`

Script index: [scripts/README.md](scripts/README.md).

## Pull requests

- Keep changes focused; prefer small PRs.
- Match existing style (concise comments in English).
- Do not commit `third_party/bitcoin/`, certs, or secrets.
- Docs-only and README-only commits intentionally skip heavy CI.
- Touching `patches/uwp` or the Core pin will rebuild Core (slow CI).

## Releases

Maintainers:

```bash
./scripts/cut-release.sh X.Y.Z
```

CI builds the MSIX and publishes the GitHub Release. Details: [docs/ci.md](docs/ci.md#releases-automated).

## Security

Do **not** file public issues for vulnerabilities. See [SECURITY.md](SECURITY.md).

## Issues

- Prefer existing labels: `ops`, `bug`, `enhancement`, `docs`, `v1-close`.
- Ops gates that close v1: issues labeled `v1-close` (see tracking.md).
- Templates under `.github/ISSUE_TEMPLATE/`.

## Changelog

User-facing changes: [CHANGELOG.md](CHANGELOG.md). Update the **Unreleased** section
in the same PR when behaviour or packaging changes meaningfully.
When an ops gate moves, update [docs/tracking.md](docs/tracking.md) in the same change.

## Code of conduct

See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) — short version: be respectful; treat
console and network use responsibly.
