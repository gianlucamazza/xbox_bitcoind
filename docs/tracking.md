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
| Console package | **0.1.0.63** WithCore · App type **Game** |
| Pin | Bitcoin Core **v31.1** |
| Dashboard | Responsive 10-foot UI (primary/secondary metrics, dual bars, sparkline) |
| Soft-stop (early / mid IBD) | **Verified** — [persistence.md](persistence.md) |
| Mainnet IBD | **In progress** (~13.7% tip progress, height ~**450k**) |
| 24h stable at tip | **Pending** IBD |
| Soft-stop at tip | **Pending** tip |
| Pre-Lightning | Blocked on IBD closure gates |
| Soft-stop wait mitigation | Host **180s** live (`deploy.sh`); in-app **150s** join on next package after `37a2bdc` |

Verify live:

```bash
./scripts/node-status.sh
./scripts/v1-close-check.sh
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
| soft-stop-timeout | Soft-stop wait / DELETE fallback | **Mitigated** — default wait 180s + re-suspend @45s + in-app join 150s ([#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4)) |
| package-gc | Remove stale package revisions | **Tooling** — `deploy.sh package-gc` ([#5](https://github.com/gianlucamazza/xbox_bitcoind/issues/5)) |
| conf-apply | Conf only via `apply-console-conf` after MSIX | Done; probes must not overwrite |

### P2 — Pre-Lightning (after P0)

See [roadmap.md § Pre-Lightning](roadmap.md#pre-lightning-standard-node-only).

### Polishing (non-blocking)

| Wave | Items | Status |
|------|--------|--------|
| **A** host/docs | shellcheck SC2028; sample/soft-stop temp under `$STATE_DIR`; `ibd-report` rate+ETA; README feature parity; research checklists historical | **done** |
| **B** next package | in-app join 150s + UI ETA + `STOPPING Ns` (code on main); field-verify #4; screenshot after deploy | **code done** — deploy deferred mid-IBD |
| **C** code health | pure `ui_layout.h` + `test-ui-layout.sh`; conf fallback documented; rpc_client limits documented | **done** |

Do **not** redeploy mid-IBD solely for polish. Host tools apply immediately.

### Out of scope (do not open as v1 work)

Wallet UI · Store · `listen=1` · CLN on-console · USB datadir UX

---

## GitHub Issues

Issues are created with labels:

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
| [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4) | Soft-stop >90s → DELETE fallback | mitigated (field-verify) |
| [#5](https://github.com/gianlucamazza/xbox_bitcoind/issues/5) | Remove stale package revisions on console | **closed** |

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
| CI / release process | ci.md |

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

*Last consolidated: 2026-07-31.*
