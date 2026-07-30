# third_party

## bitcoin/

Pinned **Bitcoin Core** sources. Not committed as a full tree by default (see
repo `.gitignore`); materialize with:

```bash
./scripts/fetch-bitcoin-core.sh
```

Pin file: [`../config/bitcoin-core.pin`](../config/bitcoin-core.pin)

| Field | Current |
|-------|---------|
| Tag | `v31.1` |
| Commit | `9be056a8a72b624dae9623b2f7bded92c2a21c91` |
| Upstream | https://github.com/bitcoin/bitcoin |

Build docs: [`../docs/build-msvc-baseline.md`](../docs/build-msvc-baseline.md)
