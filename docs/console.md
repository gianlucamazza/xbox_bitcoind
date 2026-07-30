# Target console (shared with xllama)

This project uses the **same Xbox Series S** already configured for
[`xllama`](../../xllama/) (Dev Mode + Device Portal).

## Live baseline

Probed from the Linux host via Device Portal (`scripts/probe-console.sh`).

| Field | Value |
|-------|--------|
| Platform | **Xbox Series S** |
| ComputerName | `XBOX` |
| OS version | `26100.8866.amd64fre.xb_flt_2607ge.260630-2200` |
| OsEdition | SystemOS |
| Device Portal | `https://192.168.1.44:11443` (DHCP — update if IP changes) |
| Credentials | `~/.config/xllama/xbox-env` (shared; see `scripts/env.sh`) |
| Sibling package | `GianlucaMazza.xllama_*` installed |

**Last probe:** 2026-07-30 — HTTP 200 on `/api/os/info`.

```
Platform: Xbox Series S
OsVersion: 26100.8866.amd64fre.xb_flt_2607ge.260630-2200
env: ~/.config/xllama/xbox-env
sibling: GianlucaMazza.xllama_1.5.2.825_x64__pj67f1fcj4n14
```

Note: `/api/devices/file/usage` returns **HTTP 404** on this OS build — free space is still managed in Dev Home → Manage Dev Storage (~90 GB allocation from xllama notes). Prefer the Dev Home UI or Device Portal File Explorer until a working API path is found.

Re-run:

```bash
./scripts/probe-console.sh
# or
./scripts/deploy.sh probe
```

## Storage (measured in xllama on this console)

| Item | Value | Source |
|------|--------|--------|
| Dev Mode storage allocation | raised to **~90 GB** | xllama `uwp-constraints.md` §9 (2026-07-08) |
| Default Dev storage after activation | ~2.2–2.5 GB free (before raise) | same |
| Per-file limit (Dev Mode UWP) | **~2 GB** | xllama §8–§9 / community |
| Package type for budgets | **Game** (not App) | xllama §5 — set after each reinstall |

Pruned `bitcoind` datadir is expected to fit in the 90 GB allocation; still plan for USB later if chain + indexes grow or xllama models share the partition.

## Memory / CPU (Series S, Game package)

| Item | Value | Relevance to bitcoind |
|------|--------|------------------------|
| System RAM | 10 GB GDDR6 shared (Series S) | AppContainer quota is lower |
| GPU process budget (Game) | **~3801 MB** measured | Mostly irrelevant (CPU node) |
| CPU | 8× Zen 2; ~6–7 usable | Keep `par` / thread defaults modest |
| App vs Game | Game grants more OS resources | **Always set Game** after install |

Exact working-set ceiling for a pruned IBD is **not yet measured** under bitcoind; start with `dbcache=256` (`config/bitcoin.conf.console`).

## Network

- Outbound Internet works for xllama downloads / LAN API patterns.
- Manifest will declare `internetClient` + `privateNetworkClientServer` (same as xllama).
- v1 default: `listen=0` (outbound peers only).

## Operational notes

1. **Do not** put secrets in this repo; only `config/xbox-env.example`.
2. IP may change with DHCP — update `xbox-env` from Dev Home if probe fails.
3. xllama and future xbox_bitcoind **share disk** on the Dev partition — watch free space before IBD.
4. After first MSIX install: Dev Home → package tile → **View details → App type → Game**.

## Package identity (planned)

| Field | Value |
|-------|--------|
| Identity Name | `GianlucaMazza.xboxbitcoind` (AppX: no underscores) |
| Application Id | `xbox_bitcoind` |
| Log file (LocalState) | `bitcoind.log` |
