# UI — xbox_bitcoind dashboard

Controller-first **10-foot** status UI for Series S Dev Mode (programmatic XAML,
no `.xaml` markup). Screenshot: [assets/screenshot-console.png](assets/screenshot-console.png).

## Layout (top → bottom)

| Region | Content |
|--------|---------|
| **Header** | Title + package version · status pill · chain labels |
| **CHAIN** | Height · Headers · Progress · Peers (equal-width cards) |
| **NODE** | Behind · Disk · Mempool · Uptime |
| **Verification** | Progress bar + % label + meta line |
| **Actions** | Start (accent) · Stop soft · Refresh — gamepad focus ring |
| **Log** | Stretching `debug.log` tail (48 lines), auto-scroll on change |

### Status pill

| Pill | Meaning |
|------|---------|
| `NO CORE` | Scaffold without `XBB_WITH_CORE` |
| `STOPPED` / `ERROR` | Not running |
| `STARTING` | Thread up, RPC not ready |
| `STOPPING` | Soft stop in progress |
| `SYNCING` | IBD or progress &lt; 0.999 |
| `SYNCED` | Near tip |
| `NET OFF` | `networkactive=false` |

## Design notes (10-foot)

- Dark Bitcoin-adjacent palette; orange accent on primary **Start**
- Metric cards: equal columns (`Grid` `*`), 1px border, min height 100px
- Labels in small caps-style tracking; values 26px semi-bold
- Gamepad: `XYFocusKeyboardNavigation`, D-pad between actions and log
- Avoid redraw thrash: metric text only updates when changed; log scroll only on content change

## Data sources (stock bitcoind RPC)

| Field | Source |
|-------|--------|
| blocks / headers / progress / IBD / prune / disk / warnings | `getblockchaininfo` |
| peers / networkactive / subversion | `getconnectioncount` + `getnetworkinfo` |
| mempool | `getmempoolinfo` |
| uptime | `uptime` |
| Stop | `stop` + join |
| Log | `debug.log` tail |

RPC: `http://127.0.0.1:8332`, cookie auth from `datadir\.cookie`.

## Behaviour

1. Launch → probes → auto-start when Core linked  
2. 2s timer refreshes RPC + log off the UI thread  
3. Soft stop / suspend → clean shutdown ([persistence.md](persistence.md))  

## Files

| File | Role |
|------|------|
| `uwp/MainPage.h/.cpp` | Layout builders + status binding |
| `uwp/rpc_client.*` | JSON-RPC |
| `uwp/node_host.*` | Lifecycle + `NodeStatusLive` |
| `uwp/App.*` | `OnSuspending` → `NodeStop` |
