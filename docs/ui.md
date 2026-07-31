# UI — xbox_bitcoind dashboard

Controller-first **10-foot** status UI for Series S Dev Mode (programmatic XAML).
Screenshot: [assets/screenshot-console.png](assets/screenshot-console.png).

## Visualization (modern practices)

| Element | Practice |
|---------|----------|
| Dual progress bars | **Headers** (cyan): blocks÷headers · **Verified** (orange): `verificationprogress` |
| Sparkline | Session trend of verification (last ~90 samples / ~3 min) |
| Semantic colors | Peers red if 0; behind orange if large; progress green when synced |
| Metric cards | Equal-width grid, left accent strip, high-contrast type |
| Status pill + clock | Coarse state + local `updated HH:MM:SS` |
| Primary action | Orange **Start** only when stopped |
| Gamepad | XY-focus ring Start ↔ Stop soft ↔ Refresh → log |
| Overscan | Root padding 40×32 for TV safe area |
| Redraw thrash | Update text only on change; log scroll only on new content |

## Layout

```
Header:  ₿ title · version | STATUS PILL | chain labels
         updated HH:MM:SS
CHAIN:   Height | Headers | Progress | Peers
NODE:    Behind | Disk | Mempool | Uptime
SYNC:    dual bars + sparkline + meta
Actions: Start | Stop soft | Refresh
Log:     debug.log (stretch)
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

`uwp/MainPage.*` · `rpc_client.*` · `node_host.*` · `App.*` (`OnSuspending` → soft stop)
