# Target console (shared with xllama)

Same **Xbox Series S** as [`xllama`](https://github.com/gianlucamazza/xllama) (Dev Mode + Device Portal).

## Live baseline

| Field         | Value                                                                                                                                                  |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Platform      | **Xbox Series S**                                                                                                                                      |
| ComputerName  | `XBOX`                                                                                                                                                 |
| OS version    | `26100.8866.amd64fre.xb_flt_2607ge.260630-2200`                                                                                                        |
| OsEdition     | SystemOS                                                                                                                                               |
| Device Portal | `https://192.168.1.44:11443` (DHCP — update if IP changes)                                                                                             |
| Credentials   | `~/.config/xllama/xbox-env` (see `scripts/env.sh`)                                                                                                     |
| Sibling       | `GianlucaMazza.xllama_1.5.2.836_x64__pj67f1fcj4n14`                                                                                                    |
| This package  | `GianlucaMazza.xboxbitcoind_0.1.0.10019_x64__m0e4707sws2jw` (release **[v0.1.4](https://github.com/gianlucamazza/xbox_bitcoind/releases/tag/v0.1.4)**) |

**Last full ops check:** 2026-08-02 — deploy **v0.1.4** MSIX **`0.1.0.10019`**: tip conserved across upgrade (`nBestHeight=671223`, IBD ~44.5%), soft-stop-test PASS on the new package, health green. Re-check **App type → Game** in Dev Home after this install. The mid-IBD soft-stop field PASS (8s clean, no DELETE) was on package **0.1.0.75** — see [persistence.md](persistence.md). App type → Game already confirmed after this install (2026-08-01).

**Version labels:** Bitcoin Core pin is **v31.1**; `0.1.0.10019` is the **MSIX app** revision only.

Live tracking [tracking.md](tracking.md). Day-to-day: [ops.md](ops.md).

```bash
./scripts/probe-console.sh
./scripts/deploy.sh probe
```

Note: `/api/devices/file/usage` returns **HTTP 404** on this OS build. Free space
is managed in Dev Home → Manage Dev Storage (~90 GB from xllama notes).

## Storage

| Item                          | Value                                |
| ----------------------------- | ------------------------------------ |
| Dev Mode allocation           | ~**90 GB**                           |
| Per-file limit (Dev Mode UWP) | ~**2 GB**                            |
| Package resource class        | **Game** (set after every reinstall) |

Pruned datadir should fit the 90 GB allocation; USB later if chain + xllama models
contend for space.

## Memory / CPU (Series S, Game)

| Item                      | Value                          | Relevance                                                      |
| ------------------------- | ------------------------------ | -------------------------------------------------------------- |
| System RAM                | 10 GB GDDR6 shared             | AppContainer quota lower                                       |
| GPU process budget (Game) | ~3801 MB measured              | Mostly irrelevant (CPU node)                                   |
| CPU                       | 8× Zen 2; ~6–7 usable          | Keep `par` / threads modest                                    |
| App vs Game               | Game → more RAM/CPU            | **Always Game** after install; does **not** block Home suspend |
| Home / suspend            | Leaving the title suspends UWP | Soft-stop node; IBD pauses until you re-open the app           |

Packaged default is `dbcache=512` (`config/bitcoin.conf.console`; 256 was the early default).

## Network

- Manifest: `internetClient`, `privateNetworkClientServer`, `removableStorage`
- v1: `listen=0` (outbound peers only)
- RPC: `127.0.0.1:8332`, cookie under datadir

## Package identity

| Field                 | Value                                             |
| --------------------- | ------------------------------------------------- |
| Identity Name         | `GianlucaMazza.xboxbitcoind`                      |
| Application Id        | `App`                                             |
| Executable            | `xbox_bitcoind.exe`                               |
| Manifest base version | `0.1.0.0` (CI/deploy stamps revision, e.g. `.42`) |
| Datadir               | `LocalState\bitcoin`                              |
| Node log              | `LocalState\bitcoin\debug.log`                    |
| App log               | `LocalState\bitcoind.log`                         |

## Operational notes

1. No secrets in the repo — only `config/xbox-env.example`.
2. DHCP may change IP — update `xbox-env` if probe fails.
3. xllama and xbox_bitcoind share the Dev partition — watch free space during IBD.
4. After every MSIX install: **App type → Game**.
5. Prefer `./scripts/deploy.sh stop-app` (soft stop) — never hard-kill mid-IBD.
6. Leave the app open while syncing; use `./scripts/deploy.sh status` to sample progress.
7. Full ops playbook: [ops.md](ops.md) · persistence: [persistence.md](persistence.md).
