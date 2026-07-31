# Chain state persistence (Xbox / UWP)

Soft stop is the supported lifecycle path. Ops guide: [ops.md](ops.md).
Portal: [device-portal.md](device-portal.md). UI Stop / suspend: [ui.md](ui.md).

## Mitigations in tree

1. **`App::OnSuspending`** → `NodeStop()` (RPC `stop` + join) with deferral; join wait up to **~150s** before detach  
2. **`deploy.sh stop-app`**: POST suspend, wait up to **180s** for process exit (`XBB_SOFT_STOP_MAX_WAIT`, re-suspend at 45s), then DELETE only if needed  
3. **Patch 0009**: LevelDB no-mmap + WRITE_THROUGH on UWP  
4. **Patch 0010**: UWP write interval **30–60 s**

## Verified results (package `0.1.0.42`)

### Early IBD (2026-07-31)

Soft stop then restart:

| Metric | Before stop | After restart (load) |
|--------|-------------|----------------------|
| Validated tip (`nBestHeight`) | ~99 004 | **102 031** |
| Block tree (headers) | — | **960 375** |
| Next `UpdateTip` | — | **102 032+** |

### Mid IBD (2026-07-31, automated)

```bash
./scripts/soft-stop-test.sh --wait-load 240
```

| Metric | Value |
|--------|--------|
| Pre-stop tip height | **326 716** |
| Soft-stop duration | **36 s** clean exit (no DELETE) |
| Post-restart `Loaded best chain` | **326 947** |
| Block tree | **960 384** |
| **Verdict** | **PASS** (tip conserved; IBD resumed) |

### Deploy upgrade mid IBD (2026-07-31, 0.1.0.63 → 0.1.0.65)

| Metric | Value |
|--------|--------|
| Pre-stop tip height | **452 198** |
| Soft-stop | `XBB_SOFT_STOP_MAX_WAIT=300` → still running → **DELETE** |
| Post-restart `Loaded best chain` / nBestHeight | **452 201** |
| Live tip shortly after | **452 212** |
| **Verdict** | **PASS tip conservation**; **FAIL clean exit** (DELETE path — [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |

## How to re-test

```bash
./scripts/soft-stop-test.sh
# or manually:
./scripts/deploy.sh status          # note tip
./scripts/deploy.sh stop-app
./scripts/deploy.sh start-app
# wait for load, then:
./scripts/deploy.sh status
```

Expect `loaded` / `nBestHeight` near the pre-stop tip, not genesis-only.

## What failed earlier (hard kill)

Hard DELETE without flush: `nBestHeight = 0`, orphan `blk*.dat`. Expected when
Core has not written the index (upstream interval 50–70 min without patch 0010).
