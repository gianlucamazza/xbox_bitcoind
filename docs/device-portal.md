# Xbox Device Portal (xbox_bitcoind)

Same console and workflow as **xllama**. Credentials live outside the repo.

## Enable (already done on the shared Series S)

1. Console in **Developer Mode** (Dev Home, green header).
2. **Device Portal** enabled in Dev Home.
3. Credentials set under Dev Home → Device Portal credentials.
4. Portal URL: `https://<console-ip>:11443` (TLS self-signed → curl `-k`).

Full activation notes: sibling project `../xllama/docs/device-portal.md`.

## Host env

```bash
# Prefer shared xllama credentials (default)
source ~/.config/xllama/xbox-env

# Or project-local
# cp config/xbox-env.example ~/.config/xbox_bitcoind/xbox-env
# source ~/.config/xbox_bitcoind/xbox-env

# Scripts resolve this automatically:
source scripts/env.sh
./scripts/probe-console.sh
```

Resolution order is documented in `scripts/env.sh`.

## Off-LAN via Tailscale + Odroid

When the laptop is **not** on `192.168.1.0/24` but Odroid is (Tailscale /
Headscale host `odroid-ts` → `100.64.0.2`), reach Device Portal through Odroid.

**Constraint:** Odroid `sshd` has `AllowTcpForwarding no`, so plain
`ssh -L 11443:192.168.1.44:11443` is refused. We use a **socat + ssh + nc**
bridge instead (TCP over an SSH remote command, not SSH port-forwarding).

```bash
# once per session (needs: socat, ssh Host odroid-ts, console powered + Portal on)
source scripts/console-via-odroid.sh    # start bridge → 127.0.0.1:11443
# sets XBOX_IP_OVERRIDE=127.0.0.1 so env.sh / deploy use the tunnel

./scripts/probe-console.sh
./scripts/node-status.sh
./scripts/apply-console-conf.sh         # optional: push IBD conf

scripts/console-via-odroid.sh status
scripts/console-via-odroid.sh stop
```

| Piece | Role |
|-------|------|
| `ssh odroid-ts` | Tailnet jump (`~/.ssh/config` HostName `100.64.0.2`, port 2233) |
| Odroid `192.168.1.103` | Same LAN as Xbox `192.168.1.44` |
| Bridge listen | `127.0.0.1:11443` → `nc 192.168.1.44 11443` on Odroid |
| Creds | Still from `xbox-env` (`XBOX_USER` / `XBOX_PASS`); only IP is overridden |

Overrides (optional env before `source`):

| Variable | Default |
|----------|---------|
| `ODROID_SSH` | `odroid-ts` |
| `XBOX_LAN_IP` | `192.168.1.44` |
| `XBOX_PORT_LOCAL` | `11443` |

If you later enable `AllowTcpForwarding yes` (or `Match User`) on Odroid, a
classic `ssh -N -L 127.0.0.1:11443:192.168.1.44:11443 odroid-ts` also works;
keep the same `XBOX_IP_OVERRIDE=127.0.0.1`.

## Scripts

| Command | Purpose |
|---------|---------|
| `./scripts/probe-console.sh` | OS info, disk usage, sibling packages |
| `./scripts/deploy.sh probe` | Same |
| `./scripts/deploy.sh os-info` | Raw JSON |
| `./scripts/deploy.sh packages` | Installed packages |
| `./scripts/deploy.sh disk-usage` | Dev storage usage API |
| `./scripts/deploy.sh path/to/app.msix` | Install package (+ companion `.cer` if present) |
| `./scripts/deploy.sh install-cert file.cer` | Trust signing cert |
| `./scripts/deploy.sh pfn` | Package full name (when installed) |
| `./scripts/deploy.sh get-log` | `LocalState/bitcoind.log` (app log) |
| `./scripts/deploy.sh list-localstate` | List app data |
| `./scripts/deploy.sh fetch-file <pfn> <name> <out> [subdir]` | Pull a LocalState file |
| `./scripts/deploy.sh upload-file …` | Push files into LocalState |
| `./scripts/deploy.sh start-app` | Launch package |
| `./scripts/deploy.sh stop-app` | **Soft stop**: suspend → wait ≤90s → DELETE if needed |
| `./scripts/deploy.sh status` | IBD/process snapshot (`node-status.sh`) |
| `./scripts/deploy.sh soft-stop-test` | Persistence self-check |
| `./scripts/deploy.sh diagnose-startup` | Startup diagnostics |

WDP POST/DELETE need a CSRF token; `deploy.sh` extracts it from the portal cookie
(same pattern as xllama).

### Soft stop (required for chain durability)

`stop-app` posts taskmanager **suspend** so `App::OnSuspending` can run RPC
`stop` and flush LevelDB before the process is removed. Hard DELETE without
suspend can drop unflushed tip progress — see [persistence.md](persistence.md).

```bash
./scripts/deploy.sh stop-app
./scripts/deploy.sh start-app
# Node log under datadir:
PFN=$(./scripts/deploy.sh pfn)
./scripts/deploy.sh fetch-file "$PFN" debug.log /tmp/debug.log bitcoin
```

### Console settings used by this project

| Setting / API | Purpose |
|---------------|---------|
| `/ext/settings` `DefaultUWPContentTypeToGame` | Prefer Game resource class for UWP |
| `/ext/screenshot` | Live frame for docs/README |
| `taskmanager` start / stop / **suspend** | Lifecycle + clean shutdown |

## Manual curl examples

```bash
source scripts/env.sh
BASE="https://${XBOX_IP}:${XBOX_PORT}"
AUTH=(--basic -u "${XBOX_USER}:${XBOX_PASS}" -k -sS)

curl "${AUTH[@]}" "${BASE}/api/os/info" | jq .
curl "${AUTH[@]}" "${BASE}/api/app/packagemanager/packages" | jq .
curl "${AUTH[@]}" -o frame.png "${BASE}/ext/screenshot"
```

## File Explorer

Browser UI: `https://<ip>:11443/#fileExplorer`

## After installing a package

1. Confirm it appears in Dev Home.
2. Set **App type → Game** (resource class).
3. Launch once; check `./scripts/deploy.sh get-log` and `list-localstate`.
4. For node progress, fetch `LocalState\bitcoin\debug.log` as above.
