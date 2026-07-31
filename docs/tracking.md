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

## Status snapshot (2026-07-31)

| Area | State |
|------|--------|
| Engineering v1 | **Complete** |
| Git tag / Release | **[v0.1.1](https://github.com/gianlucamazza/xbox_bitcoind/releases/tag/v0.1.1)** · MSIX `0.1.0.6` (+ VCLibs for deploy) |
| Console package | **0.1.0.6** WithCore · set **App type → Game** after reinstall |
| Pin | Bitcoin Core **v31.1** |
| UI | Core **v31.1 · app 0.1.0.6** subtitle, 10-foot metrics, dual bars, spark, ETA |
| Soft-stop | Tip conserved; mid-IBD sometimes clean (~38s), sometimes DELETE ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |
| Mainnet IBD | **In progress** after datadir wipe (~**192k**, progress ~**0.4%**) — do **not** redeploy |
| 24h stable at tip | **Pending** IBD |
| Soft-stop at tip | **Pending** tip |
| Pre-Lightning | Blocked on IBD closure gates |

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
| ops-game | Confirm Dev Home **App type → Game** | Manual after each reinstall |

### P1 — Quality / reliability

| ID | Task | Notes |
|----|------|--------|
| soft-stop-timeout | Clean exit without DELETE | Mitigated host wait + re-suspend; field-verify · [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4) |
| release-hygiene | Monotonic MSIX rev + VCLibs in release | **on main** (`c4e92a2`) — next cut ≥ rev 10000+run_number |
| package-gc | Stale revisions | **Done** · [#5](https://github.com/gianlucamazza/xbox_bitcoind/issues/5) closed |

### P2 — Pre-Lightning (after P0)

See [roadmap.md § Pre-Lightning](roadmap.md#pre-lightning-standard-node-only).

### Out of scope (do not open as v1 work)

Wallet UI · Store · `listen=1` · CLN on-console · USB datadir UX

---

## GitHub Issues

| Issue | Title | Status |
|-------|-------|--------|
| [#1](https://github.com/gianlucamazza/xbox_bitcoind/issues/1) | Complete mainnet IBD (v1 close) | open — ~192k / ~0.4% post-wipe |
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

*Last consolidated: 2026-07-31 evening — v0.1.1 on console; IBD restarted; timer active; no redeploy.*
