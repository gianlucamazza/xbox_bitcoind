# Chain state persistence (Xbox / UWP)

## Result (2026-07-31, package `0.1.0.42`)

Soft stop (Device Portal **suspend** → `App::OnSuspending` → RPC `stop`) then restart:

| Metric | Before stop | After restart (load) |
|--------|-------------|----------------------|
| Validated tip (`nBestHeight`) | ~99 004 | **102 031** |
| Block tree (headers) | — | **960 375** |
| Next `UpdateTip` | — | **102 032+** (continues IBD) |

**Verdict: chain state is conserved** under clean shutdown.

## What failed earlier

Hard kill via `taskmanager` DELETE **without** flush:

- Bitcoin Core keeps block index / chainstate in memory until
  `DATABASE_WRITE_INTERVAL` (upstream **50–70 min**) or clean shutdown.
- After kill: `nBestHeight = 0`, `block tree size = 1`, orphan `blk*.dat`.

## Mitigations in tree

1. **`App::OnSuspending`** → `NodeStop()` (RPC `stop` + join) with deferral  
2. **`deploy.sh stop-app`**: POST `suspend` first, wait for process exit, then DELETE  
3. **Patch 0009**: LevelDB no-mmap + WRITE_THROUGH on UWP  
4. **Patch 0010**: UWP write interval **30–60 s** (crash-friendlier flushes)

## How to re-test

```bash
./scripts/deploy.sh start-app
# wait until debug.log shows UpdateTip height >> 0
./scripts/deploy.sh stop-app    # soft stop
./scripts/deploy.sh start-app
./scripts/deploy.sh fetch-file $(./scripts/deploy.sh pfn) debug.log /tmp/d.log bitcoin
rg "nBestHeight|Loaded best chain|block tree size" /tmp/d.log | tail
```

Expect `Loaded best chain` / `nBestHeight` near the pre-stop tip, not genesis-only.
