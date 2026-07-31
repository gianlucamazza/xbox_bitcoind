# Tracking

**Single place** for open work, status snapshot, and links to GitHub Issues.
Update this file when gates move; use Issues for discussion and assignment.

| SSOT | Role |
|------|------|
| This file | Live checklist + issue map |
| [roadmap.md](roadmap.md) | Product phases (v1 / pre-Lightning / out of scope) |
| [plan-core-uwp.md](plan-core-uwp.md) | Architecture + engineering checklist |
| [CHANGELOG.md](../CHANGELOG.md) | User-facing shipped changes |
| GitHub Issues | Discussion, bugs, ops tasks |

---

## Status snapshot (2026-08-01)

| Area | State |
|------|--------|
| Engineering v1 | **Complete** |
| Architecture + UI | **Complete** (lifecycle SSOT, tip age, HEADERS/STALE) — [plan-core-uwp.md](plan-core-uwp.md) · [ui.md](ui.md) |
| Git tag / Release | **[v0.1.1](https://github.com/gianlucamazza/xbox_bitcoind/releases/tag/v0.1.1)** (baseline); console ahead via CI MSIX |
| Console package | **0.1.0.75** WithCore · reinstall **complete** (cert + VCLibs + MSIX + start) |
| Game class | Console `DefaultUWPContentTypeToGame=true`; WS ~0.9–1.0 GiB mid-IBD — glance Dev Home if App type shows App |
| Pin | Bitcoin Core **v31.1** |
| UI on console | **Live** Core **v31.1 · app 0.1.0.75** — tip age, dual bars, spark, ETA (screenshot refreshed) |
| Soft-stop | Tip conserved across deploy; mid-IBD host wait sometimes DELETE after node already exited ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |
| Mainnet IBD | **In progress** (~**357k**, progress ~**5%**) — leave focused; no further redeploy |
| 24h stable at tip | **Pending** IBD |
| Soft-stop at tip | **Pending** tip |
| Pre-Lightning | Blocked on IBD closure gates |

### Reinstall checklist (0.1.0.75) — closed

| Step | Status |
|------|--------|
| Soft-stop previous package | done (node `rc=0`; host may still DELETE) |
| Install cert + VCLibs + MSIX | done |
| Package identity | `…_0.1.0.75_…` only (package-gc keep=1) |
| Probes 4/4 | done |
| Conf profile | `console` kept (`dbcache=512`, `prune=550`) |
| Auto-start node + IBD continue | tip advanced past pre-deploy height |
| Health / timer | healthy · timer active |
| Screenshot / UI verify | done (`docs/assets/screenshot-console.png`) |

Verify live:

```bash
./scripts/health-check.sh          # ops hygiene (prefer first)
./scripts/node-status.sh
./scripts/v1-close-check.sh
./scripts/ibd-report.sh
```

**Ops stance:** leave app **focused** (Home suspends IBD); hourly timer enabled; soft-stop only; no uninstall (wipes LocalState).  
Full hygiene guide: [ops.md § Ops hygiene](ops.md#ops-hygiene--best-practices).

---

## Open work (by priority)

### P0 — Ops wall-clock (no code required)

| ID | Task | Exit criteria |
|----|------|----------------|
| ops-ibd | Finish mainnet IBD | `v1-close-check.sh` `sync_near_tip` PASS · [#1](https://github.com/gianlucamazza/xbox_bitcoind/issues/1) |
| ops-24h | ≥24h stable at tip | Hourly `ibd.jsonl` near tip · [#2](https://github.com/gianlucamazza/xbox_bitcoind/issues/2) |
| ops-soft-tip | Soft-stop @ tip | `soft-stop-test.sh` + [persistence.md](persistence.md) · [#3](https://github.com/gianlucamazza/xbox_bitcoind/issues/3) |
| ops-game | Confirm Dev Home **App type → Game** | **Mostly done** — `DefaultUWPContentTypeToGame=true`; optional Dev Home glance |

### P1 — Quality / reliability

| ID | Task | Notes |
|----|------|--------|
| soft-stop-timeout | Clean exit without DELETE | Mitigated host wait + re-suspend; field-verify · [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4) |
| release-hygiene | Monotonic MSIX rev + VCLibs in release | **on main** (`c4e92a2`) — next cut ≥ rev 10000+run_number |
| package-gc | Stale revisions | **Done** · [#5](https://github.com/gianlucamazza/xbox_bitcoind/issues/5) closed |

### P2 — Pre-Lightning (after P0)

**Plan:** [pre-lightning.md](pre-lightning.md) (gates G1–G6, conf tip, options A/B/C, phases PL-0…4).

| Now (IBD) | After tip |
|-----------|-----------|
| No conf change for LN | Tip conf, headroom, soft-stop @ tip |
| No RPC exposure | Decision A/B/C then spike only |

Default: **Xbox = validator appliance**; LN elsewhere until isolation/RAM spikes pass.

### Out of scope (do not open as v1 work)

Wallet UI · Store · `listen=1` · CLN on-console · USB datadir UX

---

## GitHub Issues

| Issue | Title | Status |
|-------|-------|--------|
| [#1](https://github.com/gianlucamazza/xbox_bitcoind/issues/1) | Complete mainnet IBD (v1 close) | open — ~357k / ~5% |
| [#2](https://github.com/gianlucamazza/xbox_bitcoind/issues/2) | 24h stability at tip (v1 close) | open |
| [#3](https://github.com/gianlucamazza/xbox_bitcoind/issues/3) | Soft-stop retest at tip (v1 close) | open |
| [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4) | Soft-stop → DELETE fallback | mitigated / field |
| [#5](https://github.com/gianlucamazza/xbox_bitcoind/issues/5) | package-gc | **closed** |

```bash
gh issue list --label v1-close
```

---

## Doc map (who edits what)

| Change type | Update |
|-------------|--------|
| New feature / fix shipped | CHANGELOG Unreleased + relevant guide |
| Ops gate progress | **this file** + roadmap ops table |
| Console package rev | console.md + this snapshot |
| UI / screenshot | ui.md + assets |
| CI / release | ci.md |

---

*Last consolidated: 2026-08-01 — reinstall **0.1.0.75** complete; IBD ~357k / ~5%; leave focused.*
