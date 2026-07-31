# UWP package (xbox_bitcoind)

**C++/WinRT** UWP app for Xbox Series S Dev Mode: AppContainer probes, **status
dashboard** (RPC metrics / Start-Stop / log tail), Bitcoin Core embed via
`-WithCore`. UI: [ui.md](ui.md). Architecture: [plan-core-uwp.md](plan-core-uwp.md).

## Layout

```
uwp/
  AppxManifest.xml          # GianlucaMazza.xboxbitcoind, capabilities
  xbox_bitcoind.sln|.vcxproj
  packages.config           # Microsoft.Windows.CppWinRT
  App.* / MainPage.*        # lifecycle + programmatic XAML dashboard
  probes.cpp                # LocalState, VirtualAlloc, outbound TCP, datadir
  node_host.cpp             # BitcoindMain thread (XBB_WITH_CORE) / stub
  rpc_client.cpp            # loopback JSON-RPC + cookie
  log.cpp                   # LocalState\bitcoind.log
  bitcoind_embed.h          # BitcoindMain declaration
  Assets/*.png
scripts/build-uwp.ps1
scripts/build-core-uwp.ps1
```

## Build (Windows)

Scaffold only (no Core — fast iterate on UI/probes):

```powershell
.\scripts\build-uwp.ps1
# outputs under uwp\AppPackages\ + xbox_bitcoind-dev.cer
```

Full node package (**VS 2026 18.3+**):

```powershell
.\scripts\fetch-bitcoin-core.ps1
.\scripts\apply-uwp-patches.ps1
.\scripts\build-core-uwp.ps1
.\scripts\build-uwp.ps1 -WithCore
```

Requirements:

| Build | Toolchain |
|-------|-----------|
| Scaffold | VS 2022+ with UWP C++, Windows SDK (auto / props) |
| WithCore | **VS 2026 18.3+**, C++ desktop + UWP, vcpkg `x64-uwp` |

CI: `.github/workflows/build-uwp.yml` — `uwp-scaffold` on `windows-2022`,
`uwp-core` on `windows-2025-vs2026`.

## Deploy (Linux host → Series S)

```bash
source ~/.config/xllama/xbox-env   # or scripts/env.sh
./scripts/deploy.sh path/to/xbox_bitcoind_*.msix
# first time / new cert:
./scripts/deploy.sh install-cert path/to/xbox_bitcoind-dev.cer
./scripts/deploy.sh start-app
./scripts/deploy.sh stop-app       # soft stop (suspend → flush)
./scripts/deploy.sh get-log        # LocalState\bitcoind.log
./scripts/deploy.sh list-localstate
# Node debug log:
./scripts/deploy.sh fetch-file "$(./scripts/deploy.sh pfn)" debug.log /tmp/d.log bitcoin
```

**After install:** Dev Home → package tile → **View details → App type → Game**.

## Probes (on launch)

| Probe | Expectation |
|-------|-------------|
| `localstate_write` | ~16 MiB across 4 files under `LocalState\probe` |
| `virtual_alloc` | 64 MiB `VirtualAlloc`+touch (`VirtualLock` not available in UWP) |
| `outbound_tcp` | connect to `one.one.one.one:80` |
| `datadir_layout` | creates `LocalState\bitcoin\bitcoin.conf` |

Results also in `LocalState\probe-results.txt`.

## Related

- [persistence.md](persistence.md) — soft stop and chain conservation  
- [device-portal.md](device-portal.md) — full `deploy.sh` surface  
- [patches/uwp/README.md](../patches/uwp/README.md) — Core patch set  
- [research/spikes/api-matrix.md](research/spikes/api-matrix.md) — API matrix  
