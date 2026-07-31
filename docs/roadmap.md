# Roadmap

Product overview: [README](../README.md). Architecture checklist: [plan-core-uwp.md](plan-core-uwp.md).  
Live ops status: [tracking.md](tracking.md).

## v1 — pruned full node on Series S Dev Mode

### Engineering (complete)

| Work package | Status |
|--------------|--------|
| AppContainer patches + Core embed | **done** |
| Dashboard UI + loopback RPC | **done** (10-foot layout, dual bars, spark, ETA, tip age, HEADERS/STALE, Core-vs-app version) |
| App lifecycle (suspend/resume) | **done** (soft-stop on Home; auto-restart node on resume) |
| Soft-stop persistence (early + mid IBD) | **verified** tip conservation; clean exit mid-IBD still flaky ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |
| Path-filtered CI + Core/MSIX build split | **done** |
| Release automation (`v*` → MSIX + GitHub Release) | **done** |
| Docs / SECURITY / CHANGELOG / CONTRIBUTING | **done** |
| Ops tooling + hourly IBD monitor timer | **done** (`ibd-report` rate/ETA, `v1-close-check`) |
| Public release **v0.1.0** | **published** |
| Release **v0.1.1** (UI/ops polish) | **published** — [GitHub Release](https://github.com/gianlucamazza/xbox_bitcoind/releases/tag/v0.1.1) |
| Branding (Core icons splash/tiles) + version header | **in v0.1.1** |

**Verdict: v1 engineering is complete.** Remaining work is **ops wall-clock** (IBD) plus optional package upgrades.

### Operations closure (time-bound)

| Gate | How to verify | Status |
|------|----------------|--------|
| Mainnet IBD finished | `tip_progress >= 0.999` via `node-status` / UI | **in progress** (~0.4% @ height ~**192k**, package **0.1.0.6** / v0.1.1; restarted after wipe) — [#1](https://github.com/gianlucamazza/xbox_bitcoind/issues/1) |
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

**Full plan:** [pre-lightning.md](pre-lightning.md) (architecture, conf transition, options A/B/C, phases PL-0…PL-4).

Do these **before** any CLN work. Prefer stock Core options and normal operator practice.

| # | Item | Status / how |
|---|------|----------------|
| 1 | IBD complete + tip stable 24–48h | Ops — `v1-close-check.sh` · gates G1–G2 |
| 2 | Soft-stop @ tip documented | Ops — `soft-stop-test.sh` → [persistence.md](persistence.md) · G3 |
| 3 | Disk/RAM headroom at tip | Ops — UI Disk + `node-status` / samples · G5 |
| 4 | Prune policy deliberate (`prune=550` min; raise if space) | Conf — tip profile in pre-lightning plan |
| 5 | Keep RPC loopback + cookie; no public RPC | **Done** (defaults) · keep until PL-2 decision |
| 6 | Dashboard shows standard node health | **Done** (tip age + HEADERS/SYNCING/SYNCED/STALE on main) |
| 7 | Backup/restore story | [ops.md](ops.md) § Backup · LN seeds **off** LocalState |
| 8 | Tip conf (`blocksonly` off) | After G1 — `bitcoin.conf.tip` (planned) |
| 9 | Integration choice A/B/C | [pre-lightning.md §5](pre-lightning.md#5-integration-options-decision-after-g1g3) |

Only then: CLN spike / sibling `xbox_lightning` (not a conf flag). **Default lean:** Xbox stays full node; LN keys/host off-console (option C/A).

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

*Last roadmap reconciliation: 2026-07-31 late — engineering + architecture/UI complete on main; **v0.1.1** on console **0.1.0.6**; IBD ~307k; leave node running (no mid-IBD redeploy).*
