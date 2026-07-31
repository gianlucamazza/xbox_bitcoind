# third_party

## bitcoin/

Pinned **Bitcoin Core** sources. Not committed (see repo `.gitignore`). Materialize:

```bash
./scripts/fetch-bitcoin-core.sh
# Windows: .\scripts\fetch-bitcoin-core.ps1
```

| Field | Value |
|-------|--------|
| Tag | `v31.1` |
| Commit | `9be056a8a72b624dae9623b2f7bded92c2a21c91` |
| Upstream | https://github.com/bitcoin/bitcoin |
| Pin file | [`../config/bitcoin-core.pin`](../config/bitcoin-core.pin) |

| Build | Doc / script |
|-------|----------------|
| Desktop MSVC baseline | [build-msvc-baseline.md](../docs/build-msvc-baseline.md) |
| Linux smoke | `../scripts/build-linux-smoke.sh` |
| UWP Core (patched) | `apply-uwp-patches` + `build-core-uwp` — [patches/uwp](../patches/uwp/README.md) |

Do not commit a patched tree — only files under `patches/uwp/`.
