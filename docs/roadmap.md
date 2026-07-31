# Roadmap

Product overview: [README](../README.md). Architecture checklist: [plan-core-uwp.md](plan-core-uwp.md).  
Live ops status: [tracking.md](tracking.md).

## v1 — pruned full node on Series S Dev Mode

### Engineering (complete)

| Work package | Status |
|--------------|--------|
| AppContainer patches + Core embed | **done** |
| Dashboard UI + loopback RPC | **done** (10-foot layout, dual bars, spark, ETA, Core-vs-app version) |
| Soft-stop persistence (early + mid IBD) | **verified** tip conservation; clean exit mid-IBD still flaky ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |
| Path-filtered CI + Core/MSIX build split | **done** |
| Release automation (`v*` → MSIX + GitHub Release) | **done** |
| Docs / SECURITY / CHANGELOG / CONTRIBUTING | **done** |
| Ops tooling + hourly IBD monitor timer | **done** (`ibd-report` rate/ETA, `v1-close-check`) |
| Public release **v0.1.0** | **published** |
| Release **v0.1.1** (UI/ops polish) | **tagged** — MSIX via `release.yml` |
| Branding (Core icons splash/tiles) + version header | **in v0.1.1** |

**Verdict: v1 engineering is complete.** Remaining work is **ops wall-clock** (IBD) plus optional package upgrades.

### Operations closure (time-bound)

| Gate | How to verify | Status |
|------|----------------|--------|
| Mainnet IBD finished | `tip_progress >= 0.999` via `node-status` / UI | **in progress** (~14% @ height ~**453k**, package **0.1.0.65**) — [#1](https://github.com/gianlucamazza/xbox_bitcoind/issues/1) |
| ≥24h stable at tip | Hourly samples in `ibd.jsonl` all running near tip | **pending** IBD — [#2](https://github.com/gianlucamazza/xbox_bitcoind/issues/2) |
| Soft-stop at tip | `./scripts/soft-stop-test.sh` + note in [persistence.md](persistence.md) | **pending** tip — [#3](https://github.com/gianlucamazza/xbox_bitcoind/issues/3) |

Automated assessment:

```bash
./scripts/v1-close-check.sh        # human
./scripts/v1-close-check.sh --json
./scripts/ibd-report.sh            # rate + rough ETA from samples
```

When `v1-close-check` exits **0**:

1. Tick the last checkbox in [plan-core-uwp.md](plan-core-uwp.md).  
2. Record soft-stop tip result in [persistence.md](persistence.md).  
3. Optionally `./scripts/cut-release.sh 0.1.1` (or `0.2.0`) with CHANGELOG notes.  

Until then: leave the app open, timer enabled (`install-ibd-timer.sh`), prefer soft-stop only; avoid redeploy mid-IBD unless required.

### Known ops caveat (non-blocking for IBD)

Mid-IBD `deploy.sh stop-app` may still hit **DELETE** after 180–300s host wait even though tip is conserved ([persistence.md](persistence.md), [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)). Field-verify again at tip.

## Pre-Lightning (standard node only)

Do these **before** any CLN work. Prefer stock Core options and normal operator practice.

| # | Item | Status / how |
|---|------|----------------|
| 1 | IBD complete + tip stable 24–48h | Ops — `v1-close-check.sh` |
| 2 | Soft-stop @ tip documented | Ops — `soft-stop-test.sh` → [persistence.md](persistence.md) |
| 3 | Disk/RAM headroom at tip | Ops — UI Disk + `node-status` / samples |
| 4 | Prune policy deliberate (`prune=550` min; raise if space) | Conf — [bitcoin.conf.console](../config/bitcoin.conf.console) |
| 5 | Keep RPC loopback + cookie; no public RPC | **Done** (defaults) |
| 6 | Dashboard shows standard node health | **Done** (height/peers/disk/mempool/uptime + bars/ETA) |
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

- Formal CODE_OF_CONDUCT  
- Automated soft-stop on milestone via notify (human still confirms)  
- Richer mempool / peer detail panels  
- Soft-stop without DELETE under deep IBD (host RPC stop path / longer wait policy)

---

Live checklist + issue map: [tracking.md](tracking.md).

---

*Last roadmap reconciliation: 2026-07-31 — engineering complete; console **0.1.0.65**; IBD ~14% / ~453k; docs + main polish ahead of next package.*
