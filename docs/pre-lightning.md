# Pre-Lightning integration plan

**Goal:** make `xbox_bitcoind` a **stable, standard pruned full node** that a
Lightning implementation can treat as a normal Core backend — **without** putting
CLN inside this package in v1.

**Non-goal:** shipping Lightning on the Series S in this repo. That is a sibling
project (e.g. `xbox_lightning` or host-side CLN). See [roadmap.md](roadmap.md).

**Principles**

1. Stock Bitcoin Core options only (no custom consensus).  
2. RPC remains **loopback + cookie** until a deliberate, documented change.  
3. Finish **v1 ops gates** before any LN code path.  
4. Prefer **CLN (or LND) off-console** first; on-console LN is a later spike.

Live status: [tracking.md](tracking.md). Day-to-day ops: [ops.md](ops.md).

---

## 1. Architecture boundary

```text
┌─────────────────────────────────────────────────────────┐
│  Xbox Series S Dev Mode                                 │
│  ┌───────────────────────────────────────────────────┐  │
│  │  xbox_bitcoind (UWP Game)                         │  │
│  │    bitcoind prune · P2P outbound · RPC 127.0.0.1  │  │
│  │    cookie: LocalState\bitcoin\.cookie             │  │
│  │    UI dashboard (ops only)                        │  │
│  └───────────────────────┬───────────────────────────┘  │
│                          │  (no public RPC)             │
└──────────────────────────┼──────────────────────────────┘
                           │
     ┌─────────────────────┴─────────────────────┐
     │  Integration options (choose later)       │
     │                                           │
     │  A. Host / Odroid CLN  ──tunnel/RPC?──►   │
     │     (cookie copy or SSH + local bitcoind) │
     │                                           │
     │  B. Sibling UWP “xbox_lightning”          │
     │     same console, separate package        │
     │     talk to bitcoind via loopback only    │
     │     if OS allows cross-app localhost      │
     │                                           │
     │  C. Host CLN + remote pruned node (not    │
     │     this Xbox) — LN never touches console │
     └───────────────────────────────────────────┘
```

| Layer | Owns | Does not own |
|-------|------|--------------|
| **xbox_bitcoind** | Validating chain, prune, soft-stop, UI health | Keys, channels, invoices, gossip |
| **Lightning (later)** | Channels, HTLCs, wallet of LN, backup of seeds | Consensus, IBD, Game package lifecycle |

**CLN does not need `txindex`** with prune; do not enable `txindex` on this node.

---

## 2. Hard gates before any LN work

All must be **true**. Track on GitHub `v1-close` + [tracking.md](tracking.md).

| # | Gate | How | Status driver |
|---|------|-----|----------------|
| G1 | IBD complete | `tip_progress >= 0.999` · `v1-close-check.sh` | [#1](https://github.com/gianlucamazza/xbox_bitcoind/issues/1) |
| G2 | ≥24h stable at tip | Hourly `ibd.jsonl` near tip | [#2](https://github.com/gianlucamazza/xbox_bitcoind/issues/2) |
| G3 | Soft-stop @ tip documented | `soft-stop-test.sh` → [persistence.md](persistence.md) | [#3](https://github.com/gianlucamazza/xbox_bitcoind/issues/3) |
| G4 | Soft-stop story accepted mid/tip | Field notes · [#4](https://github.com/gianlucamazza/xbox_bitcoind/issues/4) | reliability |
| G5 | Disk/RAM headroom at tip | `node-status` WS + datadir + Dev free space | ops sample |
| G6 | Health green when focused | `./scripts/health-check.sh` exit 0 | [ops hygiene](ops.md#ops-hygiene--best-practices) |

**Until G1–G3:** no CLN config, no RPC exposure experiments, no sibling package wiring.

---

## 3. Core conf transition (IBD → tip → pre-LN)

Current package defaults: [bitcoin.conf.console](../config/bitcoin.conf.console).

| Phase | Conf intent | Notes |
|-------|-------------|--------|
| **IBD (now)** | `blocksonly=1`, `dbcache=512`, `maxconnections=16`, `prune=550`, `listen=0` | Minimize mempool/tx noise |
| **Tip stable** | Keep prune/listen/RPC; **disable `blocksonly`** | Normal mempool for fee/LN prep |
| **Pre-LN** | Raise `maxmempool` if needed (e.g. 100–300); optional `dbcache=1024` if WS allows | Re-check soft-stop + Game class |
| **Never (this host)** | `txindex=1` with prune | Incompatible |
| **Avoid until deliberate** | `listen=1`, public `rpcallowip`, cleartext RPC on LAN | Security / UWP / NAT |

### Tip / pre-LN conf profile (in repo — apply only at tip)

| File | Profile |
|------|---------|
| [config/bitcoin.conf.console](../config/bitcoin.conf.console) | `console` / `ibd` (current IBD) |
| [config/bitcoin.conf.tip](../config/bitcoin.conf.tip) | `tip` (mempool on) |
| [config/README.md](../config/README.md) | Profile index |

```bash
# Dry-run anytime (no console change)
./scripts/apply-console-conf.sh --profile tip --dry-run

# When G1 near tip (script refuses if progress < 0.99 unless --force)
./scripts/apply-console-conf.sh --profile tip
./scripts/health-check.sh
./scripts/soft-stop-test.sh
```

---

## 4. RPC contract for a Lightning backend

What LN needs from Core (CLN-oriented, stock):

| Requirement | xbox_bitcoind today | Pre-LN action |
|-------------|---------------------|---------------|
| Synced chain | G1–G2 | Wait |
| `server=1` + cookie | Yes | Keep |
| Loopback RPC | `127.0.0.1` only | Keep until Option A needs tunnel |
| Pruned node OK | Yes (CLN supports prune) | Document prune floor |
| ZMQ | Off | Only if chosen LN stack requires it (CLN: optional) |
| Wallet RPC | Off / no wallet UI | LN has own wallet; Core wallet not required for CLN |

### Cookie / auth access patterns

| Pattern | Use case | Risk |
|---------|----------|------|
| **In-process UI only** (today) | Dashboard | Lowest |
| **Same-device loopback** | Sibling UWP LN app | Depends on AppContainer isolation — **spike required** |
| **Cookie fetch via Portal** | Host tooling debug | Ops only; never commit cookie |
| **SSH tunnel to Portal host** | Remote ops | Portal ≠ bitcoind RPC; bitcoind is not exposed on 8332 to LAN |
| **Expose RPC on private LAN** | Host CLN → Xbox bitcoind | Requires conf change + threat model; **last resort** |

**Important:** Device Portal (11443) is **not** Bitcoin RPC. LN cannot use Portal as a Core API. Integration must reach `bitcoind` JSON-RPC (cookie), not WDP.

---

## 5. Integration options (decision after G1–G3)

### Option A — CLN on host/Odroid (recommended first spike)

```text
Host/Odroid CLN  →  needs a bitcoind backend
                 →  either: local bitcoind on host
                 →  or: Xbox bitcoind if RPC can be reached safely
```

| Pros | Cons |
|------|------|
| No UWP LN packaging | Xbox RPC not on LAN today |
| Mature CLN ops | Tunneling cookie/RPC is non-trivial on UWP |
| Fast to experiment | Two machines to reason about |

**Spike questions:** Is cross-host RPC worth it vs running bitcoind on the host for LN only? For production LN keys, prefer **host bitcoind or dedicated NUC**, Xbox as **watch/secondary** or drop Xbox from LN path.

### Option B — Sibling package `xbox_lightning` on same console

| Pros | Cons |
|------|------|
| All-on-console story | AppContainer: can app B open `127.0.0.1:8332` of app A? **Unknown — must spike** |
| Shared physical device | Double RAM (bitcoind + CLN); Game class contention with xllama |
| | Lifecycle: Home suspends both; LN hates random stops |

**Spike:** minimal UWP “RPC ping” package using xbox_bitcoind cookie path — only if isolation allows.

### Option C — Xbox = full node only; LN entirely elsewhere

| Pros | Cons |
|------|------|
| Clean separation; Xbox stays “validator appliance” | No “Lightning on Xbox” product claim |
| Matches current architecture best | |

**Default recommendation:** **C for money/keys**; **A for experiments**; **B only after isolation spike**.

---

## 6. Work breakdown (executable checklist)

### Phase PL-0 — Blocked on IBD (now)

- [x] Engineering v1 complete  
- [x] Ops hygiene (`health-check.sh`, golden rules)  
- [ ] G1 IBD tip  
- [ ] G2 24h stable  
- [ ] G3 soft-stop @ tip  
- [ ] Keep conf IBD (`blocksonly=1`); no tip profile apply yet  

### Phase PL-1 — Tip hardening (after G1)

| ID | Task | Deliverable |
|----|------|-------------|
| PL-1.1 | Sample WS + disk at tip | Note in tracking / ops budgets |
| PL-1.2 | Soft-stop @ tip | persistence.md + close #3 |
| PL-1.3 | Add `config/bitcoin.conf.tip` | File in repo |
| PL-1.4 | Apply tip conf (drop `blocksonly`) | `apply-console-conf` + health green |
| PL-1.5 | Soft-stop retest with tip conf | Note result |
| PL-1.6 | Optional `dbcache=1024` trial | WS + soft-stop only if headroom |

### Phase PL-2 — Integration design freeze

| ID | Task | Deliverable |
|----|------|-------------|
| PL-2.1 | Choose Option A / B / C | Decision note (this doc or `/decision`) |
| PL-2.2 | Document RPC contract + prune policy for LN | Section update here |
| PL-2.3 | Threat model if any non-loopback RPC | SECURITY.md addendum |
| PL-2.4 | Backup policy for LN seeds (never only LocalState) | ops.md |

### Phase PL-3 — Spike (only after PL-1 + PL-2)

| ID | Task | Exit |
|----|------|------|
| PL-3.A | Host CLN against **host** bitcoind (learning path) | CLN starts, getinfo |
| PL-3.B | If B: UWP localhost RPC probe → xbox_bitcoind | Pass/fail AppContainer |
| PL-3.C | If A-remote: design RPC access without Portal | Spec only unless approved |

### Phase PL-4 — Product (new repo)

- Scaffold `xbox_lightning` **or** host playbook  
- No merge of CLN into `xbox_bitcoind` MSIX without a new ADR  

---

## 7. Risks specific to Lightning + this console

| Risk | Mitigation |
|------|------------|
| Home suspend kills/stops node | Keep title focused; resume auto-restart (main); LN must tolerate disconnect |
| Uninstall wipes chain | Never uninstall for LN experiments; clone conf only |
| RAM: bitcoind + CLN + xllama | Prefer host CLN; measure before on-console |
| Prune too aggressive offline | Raise prune if console offline days (pre-LN conf) |
| Cookie leakage via Portal scripts | Never log cookie; no git commit of `.cookie` |
| Operators expose RPC “to make CLN work” | Forbid by default; review in PL-2.3 |

---

## 8. What we deliberately defer

- In-app LN UI  
- Channel management on TV  
- `listen=1` for LN (not required for outbound-only backend use cases)  
- Microsoft Store  
- Core wallet as LN funding UX  

---

## 9. Success criteria (“pre-Lightning ready”)

| Criterion | Measure |
|-----------|---------|
| Node is a boring full node at tip | G1–G3, health 0 |
| Conf is tip-shaped | `blocksonly` off; prune/RPC deliberate |
| Soft-stop trusted | persistence tip PASS |
| Integration choice documented | A/B/C decision |
| No secrets on console beyond cookie | Seeds off-device |
| Path to spike clear | PL-3 checklist |

When all true: **open sibling LN work**. Until then: only IBD + hygiene.

---

## 10. Immediate actions (while IBD runs)

1. Do **not** apply `--profile tip` yet (mempool conf mid-IBD is optional only with `--force`).  
2. Do **not** open RPC.  
3. Keep `health-check` / timer / focused app.  
4. Conf templates **ready** in `config/` + `apply-console-conf.sh --profile …`.  
5. Decision A/B/C can be written early; **implementation waits for tip**.

### Config inventory (sync-independent — done)

| Item | Path |
|------|------|
| IBD conf | `config/bitcoin.conf.console` |
| Tip/pre-LN conf | `config/bitcoin.conf.tip` |
| Profile docs | `config/README.md` |
| Apply tool | `scripts/apply-console-conf.sh --profile console\|tip` |
| Host env example | `config/xbox-env.example` |
| Core pin | `config/bitcoin-core.pin` |

---

*Plan version: 2026-07-31 — conf profiles complete; apply tip only after G1; IBD still running.*
