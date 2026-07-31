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
| Git tag / Release | **v0.1.0** published |
| Console package | **0.1.0.65** WithCore · App type **Game** |
| Pin | Bitcoin Core **v31.1** (`config/bitcoin-core.pin`) |
| Version labeling | UI must show **Core pin** vs **app MSIX** separately (`xbb_version.generated.h` + package identity) — on `main`; console until next deploy still older subtitle |
| Dashboard | 10-foot UI: primary/secondary metrics, dual bars, sparkline, ETA, centered KPI text |
| Splash / tiles | Core official icons (regenerate: `scripts/generate-uwp-assets.py`) — on `main` |
| Soft-stop (early / mid IBD) | Tip conservation **PASS**; mid-IBD host stop may still **DELETE** after 300s ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |
| Mainnet IBD | **In progress** (~14.2% tip progress, height ~**453k**) |
| 24h stable at tip | **Pending** IBD |
| Soft-stop at tip | **Pending** tip |
| Pre-Lightning | Blocked on IBD closure gates |

Verify live:

```bash
./scripts/node-status.sh
./scripts/v1-close-check.sh
./scripts/ibd-report.sh
```

---

## Open work (by priority)

### P0 — Ops wall-clock (no code required)

| ID | Task | Exit criteria |
|----|------|----------------|
| ops-ibd | Finish mainnet IBD | `v1-close-check.sh` `sync_near_tip` PASS |
| ops-24h | ≥24h stable at tip | Hourly `ibd.jsonl` samples near tip |
| ops-soft-tip | Soft-stop @ tip | `soft-stop-test.sh` PASS + note in persistence.md |

Track on GitHub: labels `ops`, `v1-close`.

### P1 — Quality / reliability

| ID | Task | Notes |
|----|------|--------|
| soft-stop-timeout | Soft-stop wait / DELETE fallback | **Mitigated** (180s host + 150s join); mid-IBD field still DELETE @300s — leave open ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |
| package-gc | Remove stale package revisions | **Done** tooling — `deploy.sh package-gc` ([#5](https://github.com/gianlucamazza/xbox_bitcoind/issues/5) closed) |
| conf-apply | Conf only via `apply-console-conf` after MSIX | Done; probes must not overwrite |
| next-msix | Deploy `main` (icons, centered metrics, Core·app subtitle) | Optional — prefer after IBD milestone or when needed |

### P2 — Pre-Lightning (after P0)

See [roadmap.md § Pre-Lightning](roadmap.md#pre-lightning-standard-node-only).

### Polishing (closed on main)

| Wave | Status |
|------|--------|
| A host/docs | **done** |
| B package UI (ETA, STOPPING Ns, join 150s) | **on console 0.1.0.65** |
| C layout tests / conf docs | **done** |
| Icons + metric center + version subtitle | **on main** — next package |

### Out of scope (do not open as v1 work)

Wallet UI · Store · `listen=1` · CLN on-console · USB datadir UX

---

## GitHub Issues

| Label | Use |
|-------|-----|
| `ops` | Console / IBD / monitoring |
| `bug` | Defects |
| `enhancement` | Non-blocking product improvements |
| `docs` | Documentation only |
| `v1-close` | Required to close v1 ops |

| Issue | Title | Status |
|-------|-------|--------|
| [#1](https://github.com/gianlucamazza/xbox_bitcoind/issues/1) | Complete mainnet IBD (v1 close) | open |
| [#2](https://github.com/gianlucamazza/xbox_bitcoind/issues/2) | 24h stability at tip (v1 close) | open |
| [#3](https://github.com/gianlucamazza/xbox_bitcoind/issues/3) | Soft-stop retest at tip (v1 close) | open |
| [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4) | Soft-stop → DELETE fallback mid-IBD | mitigated / field notes |
| [#5](https://github.com/gianlucamazza/xbox_bitcoind/issues/5) | Remove stale package revisions | **closed** |

```bash
gh issue list --label v1-close
gh issue list --label ops
```

---

## Doc map (who edits what)

| Change type | Update |
|-------------|--------|
| New feature / fix shipped | CHANGELOG Unreleased + relevant guide |
| Ops gate progress | **this file** + roadmap ops table |
| Architecture | plan-core-uwp.md |
| Console identity / package rev | console.md + this snapshot |
| UI behaviour | ui.md (+ screenshot if visual) |
| CI / release / pin process | ci.md |
| Core pin bump | `config/bitcoin-core.pin` + `./scripts/generate-version-header.py` |

**Rule:** Prefer editing an existing doc over adding a top-level file ([docs/README.md](README.md)).

---

## Related paths

| Path | Role |
|------|------|
| [ops.md](ops.md) | Day-to-day operator runbook |
| [console.md](console.md) | Series S identity & storage |
| [persistence.md](persistence.md) | Soft-stop evidence |
| [ui.md](ui.md) | Dashboard + 10-foot layout |
| [ci.md](ci.md) | Workflows & releases |
| [ops/odroid-sshd-gmazza-local-forward.conf](ops/odroid-sshd-gmazza-local-forward.conf) | Lab SSH forward policy (audit copy) |

---

*Last consolidated: 2026-07-31 (docs pass — IBD ~453k / package 0.1.0.65 / main polish noted).*
