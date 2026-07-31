# Roadmap

Product overview: [README](../README.md). Architecture checklist: [plan-core-uwp.md](plan-core-uwp.md).

## v1 — pruned full node on Series S Dev Mode

### Engineering (complete)

| Work package | Status |
|--------------|--------|
| AppContainer patches + Core embed | **done** |
| Dashboard UI + loopback RPC | **done** |
| Soft-stop persistence (early + mid IBD) | **verified** |
| Path-filtered CI + Core/MSIX build split | **done** |
| Release automation (`v*` → MSIX + GitHub Release) | **done** |
| Docs / SECURITY / CHANGELOG / CONTRIBUTING | **done** |
| Ops tooling + hourly IBD monitor timer | **done** |
| Public release **v0.1.0** | **published** |

**Verdict: v1 engineering is complete.** No further code is required to meet the
original product shape (pruned validating node + UI + deploy + CI).

### Operations closure (time-bound)

| Gate | How to verify | Status |
|------|----------------|--------|
| Mainnet IBD finished | `tip_progress >= 0.999` via `node-status` / UI | **in progress** (~13.7% @ height ~450k, package 0.1.0.63) — [#1](https://github.com/gianlucamazza/xbox_bitcoind/issues/1) |
| ≥24h stable at tip | Hourly samples in `ibd.jsonl` all running near tip | **pending** IBD — [#2](https://github.com/gianlucamazza/xbox_bitcoind/issues/2) |
| Soft-stop at tip | `./scripts/soft-stop-test.sh` + note in [persistence.md](persistence.md) | **pending** tip — [#3](https://github.com/gianlucamazza/xbox_bitcoind/issues/3) |

Automated assessment:

```bash
./scripts/v1-close-check.sh        # human
./scripts/v1-close-check.sh --json
```

When it exits **0**:

1. Tick the last checkbox in [plan-core-uwp.md](plan-core-uwp.md).  
2. Record soft-stop tip result in [persistence.md](persistence.md).  
3. Optionally `./scripts/cut-release.sh 0.1.1` (or `0.2.0`) with CHANGELOG notes.  

Until then: leave the app open, timer enabled (`install-ibd-timer.sh`), do not hard-kill.

## Pre-Lightning (standard node only)

Do these **before** any CLN work. Prefer stock Core options and normal operator practice.

| # | Item | Status / how |
|---|------|----------------|
| 1 | IBD complete + tip stable 24–48h | Ops — `v1-close-check.sh` |
| 2 | Soft-stop @ tip documented | Ops — `soft-stop-test.sh` → [persistence.md](persistence.md) |
| 3 | Disk/RAM headroom at tip | Ops — UI Disk + `node-status` / samples |
| 4 | Prune policy deliberate (`prune=550` min; raise if space) | Conf — [bitcoin.conf.console](../config/bitcoin.conf.console) |
| 5 | Keep RPC loopback + cookie; no public RPC | **Done** (defaults) |
| 6 | Dashboard shows standard node health | **Done** (height/peers/disk/mempool/uptime) |
| 7 | Backup/restore story | [ops.md](ops.md) § Backup |

Only then: CLN spike / sibling `xbox_lightning` (not a conf flag).

## Out of scope v1 (future)

| Item | Notes |
|------|--------|
| Wallet UI | Optional later |
| USB datadir | Manifest has capability; needs UX |
| `listen=1` inbound | NAT / UWP |
| Microsoft Store | Policy + signing |
| Lightning (CLN) | After pre-Lightning gates; separate port |

## v1.1+ ideas (not scheduled)

- UI ETA / richer metrics  
- Formal CODE_OF_CONDUCT  
- Automated soft-stop on milestone via notify (human still confirms)

---

Live checklist + issue map: [tracking.md](tracking.md).

---

*Last roadmap reconciliation: 2026-07-31 — engineering complete; ops closure = IBD wall-clock (~13.7% / ~450k). Do not redeploy mid-IBD unless required.*
