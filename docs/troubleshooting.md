# Troubleshooting

Symptom → cause → fix, consolidated from field notes. Ops context in
[ops.md](ops.md), persistence details in [persistence.md](persistence.md).

## Deploy / Device Portal

| Symptom                                           | Likely cause                                 | Fix                                                                                                                       |
| ------------------------------------------------- | -------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| `probe-console.sh` cannot connect                 | Console asleep, or DHCP gave it a new IP     | Wake console; check IP in Dev Home; update `xbox-env` (or use a DHCP reservation)                                         |
| HTTP 401 from every portal call                   | Wrong `XBOX_USER`/`XBOX_PASS` in `xbox-env`  | Re-pair from Dev Home → Remote Access                                                                                     |
| App install OK but launch fails with HTTP 400     | VCLibs dependency missing on first install   | Keep `Dependencies/x64/*.appx` next to the `.msix` for `deploy.sh`, or install the VCLibs `.appx` from the release assets |
| `/api/devices/file/usage` returns 404             | Endpoint not available on all OS builds      | Harmless — scripts treat it as optional                                                                                   |
| App starts then suspends immediately, IBD stalls  | Title lost focus (Home suspends UWP games)   | Keep the title focused during IBD; suspend soft-stops the node by design                                                  |
| App behaves like a UWP "App" (aggressive suspend) | App type reverted after a remove+add install | Dev Home → package → **App type → Game** (re-check after every reinstall)                                                 |

## Node / IBD

| Symptom                                                  | Likely cause                                                        | Fix                                                                                                             |
| -------------------------------------------------------- | ------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| Dashboard pill stuck on `STARTING` with "RPC warming up" | Node is loading block index (normal after restart, minutes mid-IBD) | Wait; check log tail for `Loading block index`                                                                  |
| Status shows "RPC auth failed (stale cookie?)"           | `.cookie` from a previous run while a new instance starts           | Transient — the client re-reads the cookie; persistent = two instances or a wedged shutdown → restart the app   |
| PEERS shows `—`                                          | `getnetworkinfo` failed this tick (chain data still fresh)          | Transient; persistent with SYNCED pill = check network                                                          |
| Height restarts from 0 (`nBestHeight=0`) after a kill    | Hard kill mid-flush corrupted chainstate; orphan `blk*.dat` remain  | Expected after DELETE/power loss mid-IBD; node rewinds/re-syncs. Always prefer soft stop (`deploy.sh stop-app`) |
| Soft stop takes minutes                                  | LevelDB flush mid-IBD (dbcache full)                                | Normal (patch 0010 shortens the write interval); host waits and only DELETEs if still running past the budget   |
| Uninstall wiped the chain                                | Uninstall removes `LocalState` (datadir)                            | Never uninstall to downgrade — update in place ([upgrade.md](upgrade.md))                                       |

## Build / patches

| Symptom                                                              | Likely cause                                        | Fix                                                                                                                                                    |
| -------------------------------------------------------------------- | --------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `fetch-bitcoin-core` fails with "local changes would be overwritten" | Old fetch script vs patched tree                    | Current script resets/cleans first — update checkout; or `git -C third_party/bitcoin reset --hard && git clean -fdx -e build-uwp -e build-linux-smoke` |
| `apply-uwp-patches` says "Stale patch marker"                        | Tree patched for a different pin/patch set          | Re-run `fetch-bitcoin-core` (resets tree), then re-apply                                                                                               |
| Core build fails with missing `bitcoin_embed` target                 | Patches not applied (0008 creates the target)       | Check the patch marker; re-fetch + re-apply                                                                                                            |
| `cut-release.sh` refuses: dirty tree from `uwp/AppxManifest.xml`     | Version stamp left behind by an old `build-uwp.ps1` | Current script restores the manifest; `git checkout uwp/AppxManifest.xml` if it happened with an old build                                             |
