# UI — xbox_bitcoind dashboard

Controller-first **10-foot** status UI for Series S Dev Mode (programmatic XAML).

![Status dashboard on console](assets/screenshot-console.png)

*Live capture, package **0.1.0.63** (responsive primary/secondary metrics, dual bars, sparkline, log).*

## Best practices (10-foot / TV)

| Practice | How we apply it |
|----------|-----------------|
| **Measure usable surface** | `VisibleBounds` × `RawPixelsPerViewPixel` + ~5% title-safe inset (Xbox often reports 960×540@2× for 1080p) |
| **No silent clipping** | Height **budget** + progressive disclosure (never drop primary KPIs invisibly) |
| **No page scroll** | Full-viewport Grid shell; only `debug.log` scrolls |
| **Primary vs secondary data** | P1: Height/Progress/Peers/Behind always; P3 secondary row or folded into meta |
| **Decorative first to go** | Sparkline is P4 — hide when budget tight |
| **Safe padding** | Overscan inset + content pad from tokens |
| **Focus order** | Start → Stop soft → Refresh → log |
| **High contrast** | Dark cards, semantic colors, large type on TV |

### Priority table

| P | Content | When tight |
|---|---------|------------|
| P0 | Header + actions | always |
| P1 | Primary 4 metrics | always |
| P2 | Dual bars + one-line meta | always |
| P3 | Secondary 4 metrics | collapse → fold into meta |
| P4 | Sparkline | hide |
| P5 | DEBUG.LOG | shrink min height (floor 72 DIP) |

## Architecture

```
Page.SizeChanged / VisibleBounds
  → GetUsableSize()           // bounds − title-safe
  → DiscoverLayout()          // density + scale tokens
  → PlanSections(usable_h)    // progressive flags
  → ApplyLayout(tokens, plan) // mutate live tree
```

| Type | Role |
|------|------|
| `UiDensity` | Compact · Standard · Comfort |
| `UiLayout` | Fonts, pads, card/spark/log sizes, columns |
| `LayoutPlan` | `show_secondary`, `show_spark`, `log_min_h`, … |

### Shell rows

```
0 Header
1 Metrics block (primary row + optional secondary row)
2 Sync (bars + optional spark + meta)
3 Actions
4 Log  (* remaining height)
```

Primary metrics: **HEIGHT · PROGRESS · PEERS · BEHIND**  
Secondary: **HEADERS · DISK · MEMPOOL · UPTIME** (or meta line when hidden)

## Visualization

| Element | Practice |
|---------|----------|
| Dual progress bars | Headers (cyan) = blocks÷headers; Verified (orange) = `verificationprogress` |
| Sparkline | Session trend; optional under budget |
| ETA | Rough remaining time from session progress samples (meta line / progress label) |
| Semantic colors | Peers red if 0; behind orange if large; progress green when synced |
| Status pill | Coarse state + `updated HH:MM:SS`; **STOPPING Ns** while soft-stop joins |
| Header subtitle | **Bitcoin Core v31.1 · app 0.1.0.xx** — Core pin (build-time) + MSIX package (runtime) |

### Status pill

`NO CORE` · `STOPPED` · `ERROR` · `STARTING` · `STOPPING` · `SYNCING` · `SYNCED` · `NET OFF`

## Verification checklist (post-deploy)

- [ ] P0–P2 fully on-screen (no crop under TV overscan)
- [ ] Either 8 metrics **or** 4 primary + secondary data in meta — never half a grid missing
- [ ] Spark visible **or** intentionally absent (log says `spark=0`)
- [ ] Log ≥ ~3 lines
- [ ] Start / Stop soft / Refresh fully visible and focusable
- [ ] Host log: `[ui] layout usable=… density=… secondary=… spark=…`

## Data sources

| Field | RPC |
|-------|-----|
| Chain / IBD / disk | `getblockchaininfo` |
| Peers / network | `getconnectioncount`, `getnetworkinfo` |
| Mempool | `getmempoolinfo` |
| Uptime | `uptime` |
| Stop | `stop` |
| Log | `debug.log` tail |

## Files

`uwp/ui_layout.h` (pure density/plan/ETA helpers) · `uwp/MainPage.h` · `uwp/MainPage.cpp` ·
`rpc_client.*` · `node_host.*` · `App.*`

Host unit test (no Xbox): `./scripts/test-ui-layout.sh`
