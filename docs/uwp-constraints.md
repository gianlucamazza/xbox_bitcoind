# UWP constraints for xbox_bitcoind

Bitcoind-focused subset. **Measured Series S numbers** live in the sibling
project and are treated as SSOT unless re-measured here:

→ [`../xllama/docs/uwp-constraints.md`](../../xllama/docs/uwp-constraints.md)

This console is documented in [`console.md`](./console.md).

## 1. No POSIX `mmap`

AppContainer does not provide POSIX `mmap`. xllama disabled desktop mmap for
GGUF loads and saw no win from `CreateFileMappingFromApp` experiments.

**For Bitcoin Core:** audit Windows file / leveldb / block-index paths for
mmap assumptions. Prefer buffered I/O or App-compatible mapping APIs if
needed. Spike before IBD.

## 2. Sandboxed filesystem

Writable data: **`ApplicationData.LocalFolder`** (`LocalState` in Device Portal).
No arbitrary `C:\…` paths.

**For bitcoind:** set `-datadir` to a path under LocalState (e.g.
`LocalState\bitcoin`). Optional later: USB via `removableStorage` + user grant
(xllama uses this pattern for large assets).

Device Portal can upload/download LocalState for debugging (`scripts/deploy.sh`).

## 3. ~2 GB per-file limit (Dev Mode)

Confirmed relevant on this console’s Dev Mode stack (xllama §8–§9).

Bitcoin Core default block files are **128 MB** → OK. Avoid shipping or creating
single multi-GB files (bootstrap monofiles, huge logs without rotation).

## 4. Dev Mode disk budget

On **this** Series S, Dev storage was raised to **~90 GB** (xllama, 2026-07-08).
Default activation is only a few GB free — always verify with
`./scripts/deploy.sh disk-usage`.

MSIX install stages a copy; leave headroom beyond the package size.
Pruned node (`prune=550` and up) is the v1 design point.

## 5. App vs Game resource class

Dev Home can mark a sideloaded package **App** or **Game**. **Game** grants
Game OS resources (more RAM/CPU; full GPU for games — irrelevant for us except
as “more resources”).

xllama on this console is **Game**; GPU budget measured **3801 MB** under that
class. **After every xbox_bitcoind reinstall, set App type → Game** and re-check
if RAM during IBD is tight.

## 6. Thread / CPU budget

Series S: 8 Zen 2 cores; Dev Mode apps typically see ~6–7 usable. Do not assume
desktop-class parallel verification defaults without measuring thermals and
scheduler behavior.

## 7. Network capabilities

Declared in `uwp/AppxManifest.xml` (same pattern as xllama):

- `internetClient` — P2P outbound, DNS, seeds  
- `privateNetworkClientServer` — LAN RPC/debug if needed  
- `removableStorage` — optional USB datadir  

v1: `listen=0` (outbound-only). RPC binds `127.0.0.1:8332` with cookie auth.
Inbound 8333 is optional and may fight NAT/UWP.

## 8. No unsigned `dlopen` / desktop-only Win32

UWP will not load arbitrary unsigned desktop DLLs. Link dependencies
**app-local** into the MSIX (static or side-by-side). WithCore packages ship
`event.dll` from vcpkg `x64-uwp` next to the exe; Core itself is static
`bitcoin_embed` + stack.

AppContainer gaps are covered by [`patches/uwp/`](../patches/uwp/README.md)
(no VirtualLock, CreateProcess stubs, CreateFile2/CSIDL/netif shims, LevelDB
no-mmap + durable writes).

## 9. Process lifecycle vs chain flush

Bitcoin Core may keep block index / chainstate dirty until
`DATABASE_WRITE_INTERVAL` or clean shutdown. On UWP:

- Prefer **suspend** → `OnSuspending` → RPC `stop` (see [persistence.md](persistence.md))
- Patches 0009–0010 tighten LevelDB durability and flush interval (30–60 s)

Hard kill without flush can leave `nBestHeight = 0` with orphan `blk*.dat`.

## 9. Long-running process / suspension

Keep the app in the foreground (Game) during IBD. Background suspension
behavior for a multi-hour sync is unproven — document “leave node app open”
until measured.

## 10. Signing

Packages must be signed; console must trust the cert
(`deploy.sh install-cert`). Decide at first MSIX whether to reuse the xllama
dev cert or create `xbox_bitcoind-dev`.

## Checklist before IBD on console

> **Historical research checklist.** v1 console path is done; live status is
> [tracking.md](tracking.md). Kept for design context only.

- [x] MSIX installs via `deploy.sh`  
- [x] App type = **Game**  
- [x] LocalState writable; multi-file tree > pruned size works  
- [x] Outbound TCP to peers works  
- [x] `dbcache` / peak WS measured; no OOM (mid-IBD WS ~0.7–1.2 GiB @ dbcache=512)  
- [x] No single file ≥ 2 GB in datadir (pruned; portal per-file limit noted)  
- [x] Disk free after xllama models still enough for prune target  
