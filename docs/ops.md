# Operations (IBD + lifecycle)

How to run and care for the Series S node day-to-day. Product overview:
root [README](../README.md). Architecture: [plan-core-uwp.md](plan-core-uwp.md).

## Golden rules

1. **Leave the app open** during IBD (Game class package foreground/background
   as the OS allows). Closing the title from the Xbox UI may kill without flush.
2. **Stop only via soft stop**: `./scripts/deploy.sh stop-app`  
   (suspend → `OnSuspending` → RPC `stop` → LevelDB flush → DELETE).
3. **Never** use raw taskmanager DELETE / hard kill as the normal path.
4. After every MSIX reinstall: Dev Home → **App type → Game**.
5. Watch free space on the shared Dev partition (xllama + bitcoind).

## Quick commands

```bash
source scripts/env.sh   # or rely on scripts that source env.sh

./scripts/deploy.sh status              # alias → node-status.sh
./scripts/node-status.sh                # human
./scripts/node-status.sh --json         # machine
./scripts/node-status.sh --loop 3600    # hourly sample (Ctrl-C)

./scripts/deploy.sh start-app
./scripts/deploy.sh stop-app            # soft stop (up to ~90s wait)
./scripts/soft-stop-test.sh             # full persistence self-check
./scripts/deploy.sh soft-stop-test      # same
```

Fetch node log:

```bash
PFN=$(./scripts/deploy.sh pfn)
./scripts/deploy.sh fetch-file "$PFN" debug.log /tmp/debug.log bitcoin
```

## Measured budgets (package `0.1.0.42`, 2026-07-31)

Captured during mainnet IBD (~height 320–327k, progress ~3.5%):

| Metric | Observed | Notes |
|--------|----------|--------|
| Working set | **~0.9–1.0 GiB** | Peak during active `UpdateTip` |
| Private WS | **~0.9 GiB** | |
| Log `cache=` | **~400–520 MiB** | UTXO/cache lines; conf `dbcache=256` is a floor not a hard cap |
| Datadir ≈ | **~1.1–1.2 GiB** | blocks + chainstate via portal listings (non-recursive) |
| `debug.log` | **~60 MiB** then rotated/truncated after restart | Expect growth during long IBD |
| Soft-stop exit | **~36 s** | Clean process exit after suspend (no DELETE needed) |

### Guidance

| Resource | v1 policy |
|----------|-----------|
| RAM | Keep Game class; do not raise `dbcache` until a full-tip WS sample exists |
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

**v1 complete** when: not in IBD, peers > 0, tip near network, soft-stop OK near tip,
no OOM/corrupt for ≥24h. Then tick the checklist in [plan-core-uwp.md](plan-core-uwp.md).

## What not to do mid-IBD

- Redeploy / wipe LocalState  
- Switch App→Game mid-run without need  
- Raise `dbcache` without a new WS measurement  
- USB datadir migration (post-tip task)
