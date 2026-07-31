# Feasibility research: bitcoind on Xbox Series X|S

**Date:** 2026-07-30 (research)  
**Status:** Research complete (archive). **Product (2026-07-31):** Core v31.1 on
Series S, dashboard UI, soft-stop verified, mainnet IBD in progress — root
[README](../../README.md) is SSOT.  
**Target:** Pruned validating full node as UWP package, Dev Mode sideload

## Summary

A pruned Bitcoin Core full node on Xbox Series X|S is **plausible but unsupported and non-trivial**. There is no prior public `bitcoind` port for modern Xbox. The viable path is **native UWP (C++)** under **Developer Mode**, not Linux dual-boot, not retail Win32, and not Wine/BoxedWine.

Highest risks: **UWP RAM quotas**, **filesystem/sandbox limits** (including the reported **2 GB single-file** Dev Mode limit), and **Win32 API gaps** versus desktop Bitcoin Core.

---

## Prior art

| Item | Relevance |
|------|-----------|
| No known Bitcoin Core / bitcoind Xbox One/Series port | Greenfield project |
| Bitcointalk Xbox 360 mining threads (2011) | Mining only |
| [XNAMiner](https://github.com/Generalkidd/XNAMiner) | XNA miner, not a node |
| UWP pool miners in Store / sideload catalogs | Often broken; Store bans on-device mining |
| RetroArch / homebrew UWP scene | Proves Dev Mode packaging and large native C++ apps |

Closest reusable engineering surface: **Bitcoin Core’s official Windows MSVC + CMake + vcpkg** build, then adapt for UWP packaging and sandbox.

---

## Platform model

### Why not Linux / full Win32?

- Series hardware is locked (secure boot / co-processors); public Linux is not a practical target.
- Dev Mode gives **UWP sideloading**, SSH (`DevToolsUser`), SMB, and development scratch storage — not a general desktop OS for end users.
- Community experiments show **mingw Win32 EXEs** can run under SSH tooling, but with ~**1.5 GB** RAM and no proper app lifecycle. Rejected as the product target; useful only as a canary.

### Delivery model (locked)

- **Xbox Series S** (same unit as xllama — see [`docs/console.md`](../console.md))
- Developer Mode (already active; Device Portal verified 2026-07-30)
- Proper **UWP package** (**Game** resource class — measured beneficial on this console)
- Sideload via Device Portal / Visual Studio
- Host tooling: `scripts/env.sh` reuses `~/.config/xllama/xbox-env`

### Useful references

- [Xbox UWP apps (Microsoft)](https://docs.microsoft.com/en-us/windows/uwp/xbox-apps/)
- [Dev Mode setup (Xbox One Research wiki)](https://xboxoneresearch.github.io/wiki/development/setup-dev-mode/)
- [Dev Mode exploration notes (ku.nz)](https://ku.nz/blog/xbox.html)
- [Bitcoin Core Windows MSVC build](https://github.com/bitcoin/bitcoin/blob/master/doc/build-windows-msvc.md)
- [Microsoft Store Policies 7.19](https://learn.microsoft.com/en-us/windows/apps/publish/store-policies) (esp. 10.2.6 crypto)

---

## Hardware vs node requirements

| Resource | Series X|S (order of magnitude) | Pruned bitcoind |
|----------|----------------------------------|-----------------|
| CPU | Zen 2, many cores, shared | IBD CPU-heavy; no mining required |
| RAM | 16 GB GDDR6 shared with GPU; **app quota much lower** | Need measured budget; desktop guides often assume multi-GB |
| Disk | Internal usable hundreds of GB; Dev storage default often ~5 GB | Full chain too large; **prune required** (~10–50+ GB class) |
| External USB | Supported for data (with UWP access caveats) | Preferred for `datadir` if sandbox allows |
| Network | Home NAT, Ethernet preferred | Outbound P2P sufficient; inbound 8333 optional |

**v1 is pruned only.** Archival full chain is deferred.

---

## Constraint checklist

### Confirmed or widely reported

1. **UWP single-file size limit ~2 GB (Dev Mode)**  
   Default `blk*.dat` max is 128 MB → compatible. Still audit any large artifacts (logs, wallets, bootstrap files).

2. **Development storage often starts at ~5 GB**  
   Must raise in Dev Home; do not assume internal free space is available to the app.

3. **App vs Game resource class**  
   SSH tools ~1.5 GB RAM anecdotes; Game packages reported higher (~5 GB class). **Measure on device.**

4. **Reduced Win32 surface under UWP**  
   Expect filesystem, socket, path, and process API friction. Not a clean desktop binary drop-in.

5. **Capability manifest for networking**  
   Declare internet/private network client (and server if listening). Start with `listen=0` / outbound-only.

6. **Store vs sideload**  
   - Mining on-device: **disallowed** on Store (policy 10.2.6).  
   - Full node is not mining; wallets/trading have extra rules and company-account requirements.  
   - **v1 = Dev Mode sideload only.** Store is optional later.  
   - Microsoft has reduced Dev Mode access for accounts with no Store presence — operational risk.

### Bitcoin Core build notes (2025–2026)

- CMake-only; MSVC via Visual Studio + vcpkg presets.
- Minimum useful deps without GUI: Boost, libevent, (SQLite if wallet).
- Disable for Xbox v1: Qt GUI, ZMQ, UPnP/NAT-PMP unless proven useful, mining-related tooling.
- Official OS support does **not** include Xbox.

---

## Proposed architecture

```
UWP package (Game class preferred)
├── Shell UI (C++/WinRT): status, logs, start/stop, controller input
├── Node core: Bitcoin Core (library or tightly hosted daemon)
├── datadir: LocalState and/or user-selected USB folder
└── Network: outbound P2P; local RPC for debug / future UI
```

| Style | Role |
|-------|------|
| **A. In-process library** | Preferred product shape if CMake allows linking node into UWP host |
| **B. Hosted daemon + IPC** | Closer to stock `bitcoind`; harder under UWP lifecycle |
| **C. SSH Win32 EXE** | Spike-only canary for APIs/RAM |

---

## Risk register

| Risk | Severity | Mitigation |
|------|----------|------------|
| RAM too low for IBD / UTXO cache | High | Game package; small `dbcache`; prune; early device measurement |
| FS sandbox / 2 GB file limit / USB access | High | Layout audit; folder picker / declared capabilities; external datadir tests |
| Socket or capability denials | High | Outbound-only first; capability declarations |
| Missing Win32 APIs | High | API matrix spike; shims; UWP-specific ifdefs |
| App suspension when not focused | Medium | Document foreground use; Game category |
| Dev Mode account policy | Medium | Real development activity; sideload docs |
| Windows+VS build dependency | Medium | Document Windows package pipeline; Linux for research/docs only |
| Long IBD / bandwidth | Low–Med | Progress UI; patience; no illegal bootstrap claims |

---

## Policy / legal notes (non-advice)

- This project is a **validating full node**, not a miner.
- Sideload homebrew may conflict with Microsoft account/Dev Mode terms; operators accept that risk.
- Do not submit mining functionality. Wallet/RPC financial UX may trigger Store financial policies if published.
- Respect local law for running Bitcoin software.

---

## v1 scope

**In**

- Pruned mainnet-capable node (after regtest/testnet)
- P2P validation, local RPC
- Minimal controller-friendly status UI
- Packaged `bitcoin.conf` defaults for console

**Out**

- Archival node
- Mining
- Microsoft Store submission
- Lightning (see sibling `xbox_lightning` later)
- Original Xbox / 360
- Emulated desktop bitcoind (BoxedWine, etc.)

---

## Phase 0 spikes (gate Phase 1)

See [01-phase0-spikes.md](./01-phase0-spikes.md).

Go/no-go must answer:

1. Measured RAM class for Game UWP package  
2. Writable large datadir path (internal and/or USB)  
3. Outbound TCP works from UWP  
4. Which Bitcoin Core tag to pin  
5. Style A vs B host model  

---

## Sources (selected)

- https://docs.microsoft.com/en-us/windows/uwp/xbox-apps/
- https://xboxoneresearch.github.io/wiki/development/setup-dev-mode/
- https://ku.nz/blog/xbox.html
- https://github.com/bitcoin/bitcoin/blob/master/doc/build-windows-msvc.md
- https://learn.microsoft.com/en-us/windows/apps/publish/store-policies
- https://www.howtogeek.com/703443/how-to-put-your-xbox-series-x-or-s-into-developer-mode/
- Community reports on Dev Mode storage caps, 2 GB file limit, App vs Game resources
