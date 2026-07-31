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
| Mainnet IBD finished | `tip_progress >= 0.999` via `node-status` / UI | **in progress** (~5% as of last sample) |
| ≥24h stable at tip | Hourly samples in `ibd.jsonl` all running near tip | **pending** IBD |
| Soft-stop at tip | `./scripts/soft-stop-test.sh` + note in [persistence.md](persistence.md) | **pending** tip |

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

## Out of scope v1 (future)

| Item | Notes |
|------|--------|
| Wallet UI | Optional later |
| USB datadir | Manifest has capability; needs UX |
| `listen=1` inbound | NAT / UWP |
| Microsoft Store | Policy + signing |
| Lightning | Sibling project |

## v1.1+ ideas (not scheduled)

- UI ETA / richer metrics  
- Formal CODE_OF_CONDUCT  
- Automated soft-stop on milestone via notify (human still confirms)

---

*Last roadmap reconciliation: 2026-07-31 — engineering complete; ops closure = IBD wall-clock.*
