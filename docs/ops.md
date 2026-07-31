# Operations (IBD + lifecycle)

How to run and care for the Series S node day-to-day. Product overview:
root [README](../README.md). Architecture: [plan-core-uwp.md](plan-core-uwp.md).  
Live checklist: [tracking.md](tracking.md).

## Ops hygiene & best practices

**Hygiene** = habits and checks that keep the node **running, measurable, and
not self-sabotaged** on Xbox Dev Mode (not a Linux systemd daemon).

### One-shot health (host)

```bash
./scripts/health-check.sh           # human; exit 0/1/2
./scripts/health-check.sh --json    # monitoring
./scripts/health-check.sh --strict  # warn → exit 2
```

| Exit | Meaning |
|------|---------|
| **0** | Healthy — portal up, package present, process running |
| **1** | Degraded — running but timer off, stale samples, stuck tip, … |
| **2** | Critical — portal down, package missing, process stopped |

### Daily / weekly

| Cadence | Action |
|---------|--------|
| Daily | `./scripts/health-check.sh` (or `--json` for automation) |
| Daily | Glance `./scripts/ibd-report.sh` during IBD |
| After Home | Re-open title; if stopped → `deploy.sh start-app` (or wait for resume auto-restart on next package) |
| Weekly | `v1-close-check.sh` when near tip; timer still enabled |
| Never mid-IBD “for fun” | Redeploy, uninstall, hard DELETE |

### Lifecycle (Xbox-specific)

| Do | Don't |
|----|--------|
| Keep **xbox_bitcoind focused** during long IBD | Expect sync while in **Home** (UWP **suspends**; Game ≠ background daemon) |
| Soft-stop: `deploy.sh stop-app` | Taskmanager DELETE / hard kill as normal path |
| Treat **clean stop** as `IsRunning=false` or process gone (shell residual OK) | Assume “process still listed” means bitcoind is still flushing |
| Re-open app after leaving Home | Leave suspended for hours without noticing |
| **Game** class after every reinstall | Confuse Game with “always running” |

`stop-app` polls Device Portal **`IsRunning`** (not mere ImageName presence), optional app-log markers, then DELETE only if still active after `XBB_SOFT_STOP_MAX_WAIT`. Details: [persistence.md](persistence.md).

### Deploy / package

| Do | Don't |
|----|--------|
| Prefer GitHub **Release** assets | Uninstall to install a **lower** MSIX revision (wipes **LocalState**/chain) |
| Soft-stop before deploy; wait long enough mid-IBD (`XBB_SOFT_STOP_MAX_WAIT`) | Redeploy continuously during IBD |
| Keep `Dependencies/x64/*.appx` (VCLibs) next to the `.msix` for `deploy.sh` | Deploy bare MSIX then wonder why launch fails (HTTP 400 / file not found) |
| Set **App type → Game** after install | Skip Game under memory pressure |

### Observability

| Tool | Role |
|------|------|
| `health-check.sh` | Single green/amber/red signal |
| `node-status.sh` | Live tip / RAM / errors |
| Hourly timer + `ibd-sample` | History, stuck, milestones |
| `ibd-report.sh` | Rate/ETA (wipe-aware segments) |
| `v1-close-check.sh` | “Can we close v1 ops?” |

### Conf & data

| Do | Don't |
|----|--------|
| Change conf via `apply-console-conf.sh` | Expect MSIX reinstall to refresh conf (it won't if LocalState conf exists) |
| Treat pruned chain as **not** a full backup | Assume uninstall keeps datadir |
| Watch Dev storage (~90 GB shared with xllama) | Fill disk silently |

---

## Backup & restore (standard node practice)

| Asset | Where | Notes |
|-------|--------|--------|
| `bitcoin.conf` | `LocalState\bitcoin\bitcoin.conf` | Recreated from package defaults if missing |
| Chain / UTXO | `blocks/`, `chainstate/` under datadir | Pruned — **not** a full archival backup |
| Cookie | `datadir\.cookie` | Ephemeral; recreated each run |
| App log | `LocalState\bitcoind.log` | Host diagnostics |

**Restore after wipe:** reinstall MSIX → set **Game** → `start-app` → full IBD again  
(there is no lightweight “restore pruned chain” path). For anything valuable later
(Lightning seeds, etc.), backup **outside** LocalState on a separate medium.

Pull conf for inspection:

```bash
PFN=$(./scripts/deploy.sh pfn)
./scripts/deploy.sh fetch-file "$PFN" bitcoin.conf /tmp/bitcoin.conf bitcoin
```

## Golden rules

1. **Leave the app open / focused during IBD.**  
   **Game class ≠ keep running in Home.** Returning to the Xbox Home **suspends**
   the UWP title: we soft-stop `bitcoind` (flush) so LevelDB is not frozen mid-write.
   Sync **does not** continue while suspended. Re-open the title to resume (v0.1.2+:
   auto-restart on resume if it was running). Prefer: leave `xbox_bitcoind` on-screen,
   or only leave briefly.
2. **Stop only via soft stop**: `./scripts/deploy.sh stop-app`  
   (suspend → `OnSuspending` → RPC `stop` → LevelDB flush; DELETE only after  
   `XBB_SOFT_STOP_MAX_WAIT` seconds, default **180**). Mid-IBD may still need
   DELETE after long waits — tip is usually conserved; see [persistence.md](persistence.md).
3. **Never** use raw taskmanager DELETE / hard kill as the normal path.
4. After every MSIX reinstall: Dev Home → **App type → Game**.
5. Watch free space on the shared Dev partition (xllama + bitcoind).
6. **Versions:** package `0.1.0.N` is the **app**; Bitcoin Core pin is **v31.1**.  
   Live package: `./scripts/node-status.sh` · gates: [tracking.md](tracking.md).
7. **Never uninstall** just to install a lower package revision — **LocalState/datadir is deleted**. Prefer higher revision stamps. Deploy with `Dependencies/x64/*.appx` (VCLibs) beside the `.msix` or launch fails.
8. Prefer GitHub **Release** assets over random CI artifacts; keep `Dependencies/x64` layout for `deploy.sh`.

## Quick commands

```bash
source scripts/env.sh   # or rely on scripts that source env.sh

./scripts/health-check.sh               # ops hygiene one-shot (prefer this first)
./scripts/node-status.sh
./scripts/ibd-report.sh                 # rate + rough ETA from hourly samples
./scripts/v1-close-check.sh

./scripts/deploy.sh status              # alias → node-status.sh
./scripts/node-status.sh --json
./scripts/node-status.sh --loop 3600    # sample every hour (Ctrl-C)

./scripts/deploy.sh start-app
./scripts/deploy.sh stop-app            # soft stop (default wait 180s)
# XBB_SOFT_STOP_MAX_WAIT=300 ./scripts/deploy.sh stop-app
./scripts/soft-stop-test.sh
./scripts/deploy.sh soft-stop-test
```

Fetch node log:

```bash
PFN=$(./scripts/deploy.sh pfn)
./scripts/deploy.sh fetch-file "$PFN" debug.log /tmp/debug.log bitcoin
```

Live open work: [tracking.md](tracking.md).

### Package revisions on console

```bash
./scripts/deploy.sh package-list
./scripts/deploy.sh package-gc --keep 1          # dry-run
./scripts/deploy.sh package-gc --keep 1 --yes    # uninstall older
```

## Measured budgets (package `0.1.0.42`, 2026-07-31; re-sample at tip)

Captured during mainnet IBD (~height 320–327k, progress ~3.5%):

| Metric | Observed | Notes |
|--------|----------|--------|
| Working set | **~0.7–1.0 GiB** | Peak during active `UpdateTip` |
| Private WS | **~0.7–0.9 GiB** | |
| Log `cache=` | **~240–520 MiB** | UTXO/cache lines at `dbcache=256`; expect higher with package default 512 |
| Datadir ≈ | **~1.5–2.0 GiB** mid-IBD | blocks + chainstate via portal listings (grows then prunes) |
| `debug.log` | **~60 MiB** then rotated/truncated after restart | Expect growth during long IBD |
| Soft-stop exit | **~36 s** | Clean process exit after suspend (no DELETE needed) |

### Guidance

| Resource | v1 policy |
|----------|-----------|
| RAM | Keep Game class; package default `dbcache=512` — try 1024 only after WS re-sample |
| Disk | Dev ~90 GB shared; pruned node still needs headroom for blk* during IBD |
| CPU | Series S will peg cores during verification — expected |
| Thermals | Unmeasured; if console throttles, reduce concurrent xllama load |

Re-sample with `./scripts/node-status.sh` after major height milestones
(500k, 700k, tip).

## Soft-stop persistence

| When | Pre tip | Post load | Result |
|------|---------|-----------|--------|
| Early IBD (~100k) | ~99k | **102031** | PASS ([persistence.md](persistence.md)) |
| Mid IBD (~327k) | **326716** | **326947** | PASS (2026-07-31, `soft-stop-test.sh`) |
| Mid IBD + IsRunning host (~367k) | **367530** | **367533** | **PASS clean 8s, no DELETE** (2026-08-01) |

Re-run:

```bash
./scripts/soft-stop-test.sh --wait-load 240
```

## IBD monitoring plan (until tip)

| Cadence | Action |
|---------|--------|
| **Hourly (recommended)** | User systemd timer → `ibd-sample.sh -q` (JSONL + stuck/milestone hooks) |
| Daily | `./scripts/ibd-report.sh` — rate, last tip, errors |
| Milestone | Soft-stop test at ~500k / near tip (`milestones.log` reminds you) |
| On error | `node-status.sh --json`, `bitcoind.log`, tail of `debug.log` |

### Automated hourly samples (best practice)

Uses a **user** systemd timer (no `/etc`, survives login, `Persistent=true`):

```bash
./scripts/install-ibd-timer.sh          # enable + first sample now
./scripts/install-ibd-timer.sh --status
./scripts/ibd-report.sh                 # human summary
./scripts/install-ibd-timer.sh --uninstall
```

| Path | Contents |
|------|----------|
| `~/.local/state/xbox_bitcoind/ibd.jsonl` | One JSON sample per hour |
| `~/.local/state/xbox_bitcoind/ibd-errors.jsonl` | Portal/script failures, stuck-tip alerts |
| `~/.local/state/xbox_bitcoind/milestones.log` | Once-each markers (500k, 700k, …) |

Units are generated from [contrib/systemd/user/](../contrib/systemd/user/) into
`~/.config/systemd/user/`. The sampler **exits 0** after logging errors so a flaky
Device Portal does not fail the user session.

Manual one-shot:

```bash
./scripts/ibd-sample.sh
```

### Close v1 ops (when IBD finishes)

```bash
./scripts/v1-close-check.sh     # exit 0 when progress≈1 and 24h samples look stable
./scripts/soft-stop-test.sh     # once at tip; record in persistence.md
```

Then tick the last checkbox in [plan-core-uwp.md](plan-core-uwp.md).  
Roadmap split (engineering vs ops): [roadmap.md](roadmap.md).

## Sync performance (IBD)

IBD speed is mostly **script/UTXO work + peer block download**, not the UWP UI.
Use **stock** Core options only (see `config/bitcoin.conf.console`).

| Knob | Early default | Current package default | Effect |
|------|---------------|-------------------------|--------|
| `dbcache` | 256 | **512** | Primary lever: larger UTXO/LevelDB cache → fewer flushes |
| `maxconnections` | 8 | **16** | More outbound peers for parallel block fetch (`listen=0`) |
| `blocksonly` | off | **1** (IBD) | Skip mempool/tx relay until tip / Lightning |
| `maxmempool` | 50 | 50 | Small; irrelevant while `blocksonly=1` |
| `prune` | 550 | 550 | Disk, not IBD CPU |

Measured mid-IBD (package `0.1.0.42`, `dbcache=256`): WS ~0.7–1.0 GiB,  
~30k+ blocks/h early mainnet — rate **falls** as height grows (heavier scripts).

### Apply conf to a live console

`bitcoin.conf` is **not** overwritten on reinstall if LocalState already has one.
Profiles: [config/README.md](../config/README.md).

```bash
./scripts/apply-console-conf.sh --dry-run
./scripts/apply-console-conf.sh                    # IBD profile (console)
./scripts/apply-console-conf.sh --profile tip      # only near tip (guarded)
./scripts/health-check.sh
./scripts/node-status.sh
```

### Safe further tuning

| Change | When |
|--------|------|
| `dbcache=1024` | Tip or mid-IBD with no heavy concurrent Dev apps; re-check WS + soft-stop |
| Comment out `blocksonly` | Near tip / need mempool / before CLN |
| `maxconnections=24` | Only if peers stay low and network is healthy |
| Do **not** set `txindex` with prune | Incompatible |

### What not to expect

- Consensus shortcuts / non-Core clients  
- `listen=1` for “faster” sync (inbound is optional; download is outbound)  
- Raising prune to speed IBD (it does not)

## What not to do mid-IBD

- Redeploy / wipe LocalState  
- Switch App→Game mid-run without need  
- Raise `dbcache` past ~1 GiB without a new WS + soft-stop check  
- USB datadir migration (post-tip task)
