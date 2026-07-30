# Xbox Device Portal (xbox_bitcoind)

Same console and workflow as **xllama**. Credentials live outside the repo.

## Enable (already done on the shared Series S)

1. Console in **Developer Mode** (Dev Home, green header).
2. **Device Portal** enabled in Dev Home.
3. Credentials set under Dev Home → Device Portal credentials.
4. Portal URL: `https://<console-ip>:11443` (TLS self-signed → curl `-k`).

Full activation notes: sibling project
`../xllama/docs/device-portal.md`.

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
| `./scripts/deploy.sh get-log` | `LocalState/bitcoind.log` |
| `./scripts/deploy.sh list-localstate` | List app data |
| `./scripts/deploy.sh upload-file …` | Push files into LocalState |
| `./scripts/deploy.sh start-app` / `stop-app` | Lifecycle |

WDP POST/DELETE need a CSRF token; `deploy.sh` extracts it from the portal cookie (same pattern as xllama).

## Manual curl examples

```bash
source scripts/env.sh
BASE="https://${XBOX_IP}:${XBOX_PORT}"
AUTH=(--basic -u "${XBOX_USER}:${XBOX_PASS}" -k -sS)

curl "${AUTH[@]}" "${BASE}/api/os/info" | jq .
curl "${AUTH[@]}" "${BASE}/api/app/packagemanager/packages" | jq .
```

## File Explorer

Browser UI: `https://<ip>:11443/#fileExplorer`

## Screenshot (optional)

```bash
curl "${AUTH[@]}" -o frame.png "https://${XBOX_IP}:${XBOX_PORT}/ext/screenshot"
```

## After installing a package

1. Confirm it appears in Dev Home.
2. Set **App type → Game** (resource class).
3. Launch once; check `./scripts/deploy.sh get-log` and `list-localstate`.
