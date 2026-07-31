# UI — xbox_bitcoind dashboard

Controller-first status UI for Series S Dev Mode (programmatic XAML, no `.xaml`
markup). Live frame: [assets/screenshot-console.png](assets/screenshot-console.png).

## Layout

- **Header**: brand, status pill (`NO CORE` / `STOPPED` / `STARTING` / `SYNCING` / `SYNCED` / `ERROR`), chain label  
- **Metrics**: Height · Headers · Progress · Peers  
- **Bar**: verification progress + datadir / message  
- **Actions**: Start · Stop · Refresh  
- **Log**: last ~40 lines of `LocalState\bitcoin\debug.log`

## Data sources

| Field | Source |
|-------|--------|
| Running | node thread flag |
| blocks / headers / IBD / prune / chain | RPC `getblockchaininfo` (loopback + cookie) |
| peers | RPC `getconnectioncount` |
| Stop | RPC `stop` then join thread |
| Log | file tail of `debug.log` |

RPC: `http://127.0.0.1:8332` with Basic auth from `datadir\.cookie`.

## Behaviour

1. Launch → probes → auto-`NodeStart` when Core is linked (`XBB_WITH_CORE`)  
2. `DispatcherTimer` 2s refreshes RPC + log off the UI thread  
3. Scaffold builds (`!XBB_WITH_CORE`): pill `NO CORE`, Start disabled  
4. App suspend / Stop → clean node shutdown (see [persistence.md](persistence.md))  

## Files

- `uwp/MainPage.*` — dashboard  
- `uwp/rpc_client.*` — HTTP JSON-RPC client  
- `uwp/node_host.*` — process lifecycle + live status  
- `uwp/App.*` — `OnSuspending` → `NodeStop`  

