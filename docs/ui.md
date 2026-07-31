# UI — xbox_bitcoind dashboard

Controller-first **10-foot** status UI for Series S Dev Mode (programmatic XAML).
Screenshot: [assets/screenshot-console.png](assets/screenshot-console.png).

## Architecture: self-discovery + responsive

Layout is **not** fixed to 1920×1080. The page discovers its live size and applies a token set.

```
Page.SizeChanged
    → DiscoverLayout(width, height)   // pure: size → UiLayout tokens
    → ApplyLayout(tokens)             // mutate fonts, pads, gaps, visibility
    → RelayoutMetricGrid(columns)     // 4-col or 2-col metric grid
```

| Piece | Role |
|-------|------|
| `UiDensity` | `Compact` · `Standard` · `Comfort` |
| `UiLayout` | All scale tokens (pad, type, card min height, spark, log min, …) |
| `DiscoverLayout` | Breakpoints from **viewport height/width** (TV fit is height-driven) |
| `ApplyLayout` | Pushes tokens into already-built controls (no full rebuild) |
| `RelayoutMetricGrid` | 8 metric cards → 2×4 or 4×2 |

### Breakpoints (DIPs)

| Density | When | Goal |
|---------|------|------|
| **Compact** | `h < 860` or `w < 1100` | Fit critical chrome; optional spark |
| **Standard** | default (1080p TV + overscan) | **All primary UI on-screen, no page scroll** |
| **Comfort** | `h ≥ 1200` and `w ≥ 1600` | Roomier type/cards, section labels, wrapped meta |

Columns: `metric_columns = 2` if `w < 1280`, else `4`.

### Why not page ScrollViewer

TV 10-foot UIs should not require scrolling the whole dashboard. Only **`debug.log`** scrolls internally. Meta path is shortened on Standard so vertical budget stays for metrics + sync + log.

## Visualization

| Element | Practice |
|---------|----------|
| Dual progress bars | **Headers** (cyan): blocks÷headers · **Verified** (orange): `verificationprogress` |
| Sparkline | Session trend (last ~90 samples); hidden in tight Compact |
| Semantic colors | Peers red if 0; behind orange if large; progress green when synced |
| Metric cards | Equal-width responsive grid, high-contrast type |
| Status pill + clock | Coarse state + local `updated HH:MM:SS` |
| Gamepad | XY-focus Start ↔ Stop soft ↔ Refresh → log |

## Layout (Standard density)

```
Header:   ₿ title · version | STATUS PILL | chain · prune · IBD
Metrics:  2×4 cards (HEIGHT…UPTIME) — no section labels
SYNC:     dual bars + sparkline + short meta
Actions:  Start | Stop soft | Refresh
Log:      debug.log (* row, MinHeight from tokens)
```

### Status pill

`NO CORE` · `STOPPED` · `ERROR` · `STARTING` · `STOPPING` · `SYNCING` · `SYNCED` · `NET OFF`

## Data sources

| Field | RPC |
|-------|-----|
| Chain tip / disk / IBD / warnings | `getblockchaininfo` |
| Peers / UA / networkactive | `getconnectioncount`, `getnetworkinfo` |
| Mempool | `getmempoolinfo` |
| Uptime | `uptime` |
| Stop | `stop` |
| Log | `debug.log` tail |

## Files

`uwp/MainPage.h` (`UiLayout`, `DiscoverLayout`) · `uwp/MainPage.cpp` · `rpc_client.*` · `node_host.*` · `App.*`
