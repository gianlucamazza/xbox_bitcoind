# Config profiles

All files here are **in-repo templates**. Applying a profile to the console soft-stops
bitcoind and overwrites `LocalState\bitcoin\bitcoin.conf`.

| File | Profile | When to use |
|------|---------|-------------|
| [bitcoin-core.pin](bitcoin-core.pin) | — | Core tag/commit pin (build/CI); not uploaded to console |
| [bitcoin.conf.console](bitcoin.conf.console) | `console` / `ibd` | **Default during IBD** (`blocksonly=1`, dbcache=512) |
| [bitcoin.conf.tip](bitcoin.conf.tip) | `tip` | **After tip** / pre-Lightning (mempool on, no `blocksonly`) |
| [xbox-env.example](xbox-env.example) | — | Device Portal credentials template (host only) |

## Apply to console

```bash
# IBD / current default (safe anytime if you want IBD knobs back)
./scripts/apply-console-conf.sh
./scripts/apply-console-conf.sh --profile console
./scripts/apply-console-conf.sh --profile ibd      # alias of console

# Tip / pre-LN — only after sync near tip (script warns + requires --force mid-IBD)
./scripts/apply-console-conf.sh --profile tip --dry-run
./scripts/apply-console-conf.sh --profile tip

# Show keys without touching console
./scripts/apply-console-conf.sh --profile tip --dry-run
```

See [docs/pre-lightning.md](../docs/pre-lightning.md) and [docs/ops.md](../docs/ops.md).

## Host env (not uploaded)

```bash
cp config/xbox-env.example ~/.config/xbox_bitcoind/xbox-env
# or reuse ~/.config/xllama/xbox-env
```

Optional overrides (export before tools):

| Variable | Purpose |
|----------|---------|
| `XBOX_IP_OVERRIDE` | e.g. `127.0.0.1` with `ssh -L` |
| `XBOX_PORT_OVERRIDE` | Portal port override |
| `XBB_SOFT_STOP_MAX_WAIT` | Soft-stop max wait seconds before DELETE if still active (default 180) |
| `XBB_SOFT_STOP_MIN_GRACE` | Min seconds before accepting `IsRunning=false` alone (default 8) |
| `XBB_SOFT_STOP_REQUIRE_EXIT` | If `1`, require process fully gone (not residual shell); default `0` |
| `XBB_STATE_DIR` | Host state (`ibd.jsonl`, …) |
| `XBB_ASSUME_TIP_HEIGHT` | ETA denominator for `ibd-report` |
