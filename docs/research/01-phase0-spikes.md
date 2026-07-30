# Phase 0 spike checklist

Run these before committing to full Bitcoin Core CMake/UWP integration.  
Mark results in this file or in linked notes under `docs/research/spikes/`.

## 0. Environment inventory

| Check | Status | Notes |
|-------|--------|-------|
| Windows 10/11 PC available | [ ] | Required for VS + UWP packaging |
| Visual Studio with C++ desktop + UWP / Windows app workloads | [ ] | |
| Xbox Series X or S | [x] | **Series S** shared with xllama (`docs/console.md`) |
| Microsoft developer account | [x] | Already used for xllama Dev Mode |
| Console in Developer Mode | [x] | Device Portal HTTP 200 (2026-07-30) |
| Host credentials | [x] | `~/.config/xllama/xbox-env` via `scripts/env.sh` |
| Probe / deploy scripts | [x] | `scripts/probe-console.sh`, `scripts/deploy.sh` |
| Ethernet preferred for IBD tests | [ ] | Preferred; Wi‑Fi may work for tests |
| External USB drive (≥64–128 GB free) | [ ] | For datadir experiments |

**Blocker if no Windows machine:** packaging cannot complete; research/docs and Device Portal tooling work from Linux.

---

## 1. API / sandbox matrix

**Goal:** list Windows APIs Bitcoin Core needs and whether UWP allows them.

Suggested method:

1. Pin a Bitcoin Core tag (record below).
2. Grep / audit `src/` for Windows-specific code (`#ifdef WIN32`, `ws2_32`, `filesystem`, random, time).
3. Map to UWP/WinRT alternatives or known desktop-bridge gaps.

| Area | Core usage (fill in) | UWP allowed? | Plan |
|------|----------------------|--------------|------|
| TCP client sockets | winsock / select | OK w/ caps | outbound first |
| TCP listen / bind | accept listeners | RISK | `listen=0` v1 |
| Filesystem (datadir) | CSIDL AppData default | RISK | force LocalState |
| Memory-mapped files | LevelDB mmap limited | RISK | probe / limit 0 |
| Threads / sync | std threads | OK | modest par |
| RNG / entropy | Win crypto | OK expected | smoke |
| Process / spawn | CreateProcess | BLOCK | disable notifies |
| HTTP RPC server | libevent | RISK | localhost only |
| DNS seed resolution | system/libevent | OK | default |

**Output artifact:** `docs/research/spikes/api-matrix.md` (**written**)

---

## 2. Hello UWP on Series hardware

**Goal:** prove package install + resources + network + disk.

Minimal app should:

1. Install via Device Portal as **App**, then as **Game** (compare).
2. Write a multi-GB tree of **small** files under LocalState (e.g. many 128 MB files) without hitting the 2 GB single-file limit.
3. Open outbound TCP (e.g. connect to a public HTTPS or raw TCP echo / DNS over TCP test host).
4. Run a busy loop / allocate memory in steps; record OOM threshold.
5. Stay running 30+ minutes in foreground.

| Metric | App class | Game class |
|--------|-----------|------------|
| Approx. max working set before failure | | |
| LocalState large multi-file write | | |
| USB / broad filesystem access | | |
| Outbound TCP | | |
| Inbound listen (optional) | | |

**Output artifact:** `docs/research/spikes/uwp-hello.md`

---

## 3. Desktop bitcoind baseline (Windows MSVC)

**Goal:** known-good binary and config before porting.

**Pin (locked):** `v31.1` / `9be056a8a72b624dae9623b2f7bded92c2a21c91`  
Files: `config/bitcoin-core.pin`, `scripts/fetch-bitcoin-core.sh`, `scripts/build-msvc-baseline.ps1`  
Doc: `docs/build-msvc-baseline.md`

```powershell
# On Windows Developer PowerShell for VS
.\scripts\build-msvc-baseline.ps1
```

```bash
# Optional same-pin smoke on Arch
./scripts/build-linux-smoke.sh
```

Console-oriented conf sketch: `config/bitcoin.conf.console` (`prune=550`, `listen=0`, …).

| Check | Status | Notes |
|-------|--------|-------|
| Tag pinned in repo | [x] | v31.1 |
| `fetch-bitcoin-core.sh` checks out commit | [x] | verified on host |
| MSVC scripts + docs | [x] | run on Windows still required |
| Builds with GUI off (MSVC) | [x] | GHA run 30582397400 PASS |
| Unit tests (MSVC ctest) | [x] | **137/137** passed |
| Linux smoke build | [x] | local + CI PASS |
| Regtest smoke | [ ] | optional |
| Testnet or mainnet pruned start | [ ] | optional, long |

**Pinned Core version:** `v31.1`  
**Commit:** `9be056a8a72b624dae9623b2f7bded92c2a21c91`

**Output artifact:** `docs/research/spikes/desktop-baseline.md`

---

## 4. Storage design decision

| Option | Pros | Cons | Decision |
|--------|------|------|----------|
| Internal LocalState only | Simple | Dev storage / free space limits | |
| USB folder (picker / capability) | Fits pruned + headroom | UWP access may be flaky | |
| Hybrid (config internal, blocks USB) | Flexible | Complexity | |

Target prune default: `prune=550` (or higher if disk allows).  
Indexes (`txindex`, etc.): **off** for v1.

**Decision:** `_TBD after spike 2_`

---

## 5. Host model decision

| Style | Spike result | Choose? |
|-------|--------------|---------|
| A. In-process library | | |
| B. Hosted process + IPC | | |
| C. SSH Win32 canary only | | |

**Decision:** `_TBD_`

---

## Go / no-go

Proceed to Phase 1 (build skeleton) only if:

- [x] Outbound TCP from UWP works (console probe)  
- [x] Multi-file LocalState writes work (16 MiB probe; full prune scale TBD)  
- [ ] Measured RAM ≥ ~1.5–2 GB usable for node process (ideally more with Game class)  
- [x] Core tag pinned and desktop build documented (MSVC 137/137)  
- [ ] Host model A or B chosen  
- [x] Static API matrix complete  
- [x] Hello-UWP MSIX builds (CI) and runs on Series S  

**Overall:** **GO** for Phase 1 — link Bitcoin Core into UWP.  
**Date:** 2026-07-30  
**Author notes:** Scaffold deployed; all AppContainer probes green.
