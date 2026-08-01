# Chain state persistence (Xbox / UWP)

Soft stop is the supported lifecycle path. Ops guide: [ops.md](ops.md).
Portal: [device-portal.md](device-portal.md). UI Stop / suspend: [ui.md](ui.md).

## Mitigations in tree

1. **`App::OnSuspending`** → `NodeStop()` (RPC `stop` + join) with deferral; join wait up to **~150s** before detach
2. **`deploy.sh stop-app`**: POST suspend → poll until clean, then DELETE only if still **actively** running
3. **Patch 0009**: LevelDB no-mmap + WRITE_THROUGH on UWP
4. **Patch 0010**: UWP write interval **30–60 s**

### Host soft-stop success criteria (2026-08-01)

| Signal                                                                     | Meaning                                  | DELETE?               |
| -------------------------------------------------------------------------- | ---------------------------------------- | --------------------- |
| Process row **gone**                                                       | Full exit                                | No — clean            |
| `IsRunning=false` (after grace ~8s)                                        | Residual UWP shell; node already stopped | No — clean            |
| App log: `OnSuspending complete` / `node thread joined` **and** not active | Durable stop confirmed                   | No — clean            |
| Still `IsRunning=true` after `XBB_SOFT_STOP_MAX_WAIT`                      | Flush stuck or suspend missed            | **Yes** — last resort |

**Root cause of false DELETE ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)):** host treated “ImageName listed” as running. After clean OnSuspending the shell often remains listed while the node has already exited (`BitcoindMain rc=0` in a few seconds). Host now uses Device Portal **`IsRunning`**.

Env: `XBB_SOFT_STOP_MAX_WAIT` (default 180), `XBB_SOFT_STOP_MIN_GRACE` (default 8), `XBB_SOFT_STOP_REQUIRE_EXIT=1` to force full process exit.

## Verified results (package `0.1.0.42`)

### Early IBD (2026-07-31)

Soft stop then restart:

| Metric                        | Before stop | After restart (load) |
| ----------------------------- | ----------- | -------------------- |
| Validated tip (`nBestHeight`) | ~99 004     | **102 031**          |
| Block tree (headers)          | —           | **960 375**          |
| Next `UpdateTip`              | —           | **102 032+**         |

### Mid IBD (2026-07-31, automated)

```bash
./scripts/soft-stop-test.sh --wait-load 240
```

| Metric                           | Value                                 |
| -------------------------------- | ------------------------------------- |
| Pre-stop tip height              | **326 716**                           |
| Soft-stop duration               | **36 s** clean exit (no DELETE)       |
| Post-restart `Loaded best chain` | **326 947**                           |
| Block tree                       | **960 384**                           |
| **Verdict**                      | **PASS** (tip conserved; IBD resumed) |

### Deploy upgrade mid IBD (2026-07-31, 0.1.0.63 → 0.1.0.65)

| Metric                                         | Value                                                                                                                        |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Pre-stop tip height                            | **452 198**                                                                                                                  |
| Soft-stop                                      | `XBB_SOFT_STOP_MAX_WAIT=300` → still running → **DELETE**                                                                    |
| Post-restart `Loaded best chain` / nBestHeight | **452 201**                                                                                                                  |
| Live tip shortly after                         | **452 212**                                                                                                                  |
| **Verdict**                                    | **PASS tip conservation**; **FAIL clean exit** (DELETE path — [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |

### Mid IBD + IsRunning-aware host (2026-08-01, package `0.1.0.75`)

```bash
XBB_SOFT_STOP_MAX_WAIT=180 ./scripts/soft-stop-test.sh --wait-load 300
```

| Metric                              | Value                                                         |
| ----------------------------------- | ------------------------------------------------------------- |
| Pre-stop tip height                 | **367 530**                                                   |
| Soft-stop                           | **8 s** — `IsRunning=false` residual shell; **no DELETE**     |
| Post-restart `nBestHeight` / loaded | **367 533**                                                   |
| Live tip shortly after              | **367 538**                                                   |
| Block tree                          | **960 474**                                                   |
| **Verdict**                         | **PASS tip conservation** + **PASS clean exit** (host Wave A) |

### Upgrade + soft-stop on v0.1.4 (2026-08-02, `0.1.0.10017` → `0.1.0.10019`)

In-place deploy of the v0.1.4 package (first build carrying the serialized
node-lifecycle fixes), then `./scripts/soft-stop-test.sh` on the new package.

| Metric                 | Value                                                                           |
| ---------------------- | ------------------------------------------------------------------------------- |
| Pre-upgrade tip height | **671 204** (IBD ~44.5%)                                                        |
| Upgrade                | in place, tip conserved — `nBestHeight` **671 223**, no `nBestHeight=0`         |
| Soft-stop test         | clean stop, `loaded` **671 223** vs pre-tip **671 275** (Δ within flush window) |
| **Verdict**            | **PASS tip conservation** + **PASS clean exit**; `health-check` exit 0          |

Mid-IBD persistence with the fixed host path is **closed**. Remaining ops gate: soft-stop **at tip** ([#3](https://github.com/gianlucamazza/xbox_bitcoind/issues/3)).

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
