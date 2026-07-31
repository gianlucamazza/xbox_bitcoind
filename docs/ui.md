# UI — xbox_bitcoind dashboard

Controller-first status UI for Series S Dev Mode (programmatic XAML, no `.xaml`
markup). Live frame: [assets/screenshot-console.png](assets/screenshot-console.png).

## Layout

- **Header**: brand, status pill, chain label (`main` · `prune` · `IBD`)
- **Metrics row 1**: Height · Headers · Progress · Peers  
- **Metrics row 2**: Behind (headers−blocks) · Disk · Mempool · Uptime  
- **Bar**: verification progress + meta (datadir, UA, message, warnings)  
- **Actions**: Start · **Stop soft** · Refresh  
- **Log**: last ~40 lines of `LocalState\bitcoin\debug.log`

### Status pill

| Pill | Meaning |
|------|---------|
| `NO CORE` | Scaffold build without `XBB_WITH_CORE` |
| `STOPPED` / `ERROR` | Node not running (error if last exit ≠ 0) |
| `STARTING` | Thread up, RPC cookie not ready |
| `STOPPING` | Soft stop in progress (RPC `stop` + join) |
| `SYNCING` | IBD or `verificationprogress` < 0.999 |
| `SYNCED` | Near tip |
| `NET OFF` | `networkactive=false` |

## Data sources (standard bitcoind RPC)

| Field | RPC / source |
|-------|----------------|
| Running | node thread flag |
| blocks / headers / progress / IBD / prune / disk / warnings | `getblockchaininfo` |
| peers / networkactive / subversion | `getconnectioncount` + `getnetworkinfo` |
| mempool tx / usage | `getmempoolinfo` |
| uptime | `uptime` |
| Stop | `stop` then join thread |
| Log | file tail of `debug.log` |

RPC: `http://127.0.0.1:8332` with Basic auth from `datadir\.cookie`.

## Behaviour

1. Launch → probes → auto-`NodeStart` when Core is linked (`XBB_WITH_CORE`)  
2. `DispatcherTimer` 2s refreshes RPC + log off the UI thread  
3. Scaffold builds (`!XBB_WITH_CORE`): pill `NO CORE`, Start disabled  
4. **Stop soft** / app suspend → clean node shutdown ([persistence.md](persistence.md))  

## Files

- `uwp/MainPage.*` — dashboard  
- `uwp/rpc_client.*` — HTTP JSON-RPC client  
- `uwp/node_host.*` — process lifecycle + live status  
- `uwp/App.*` — `OnSuspending` → `NodeStop`  
