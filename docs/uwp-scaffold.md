# UWP scaffold (xbox_bitcoind)

Minimal **C++/WinRT** UWP app for Xbox Series S Dev Mode. Runs AppContainer
probes and shows status UI. Bitcoin Core is **not linked yet** — that is the
next integration step after probes pass on console.

## Layout

```
uwp/
  AppxManifest.xml          # GianlucaMazza.xboxbitcoind, capabilities
  xbox_bitcoind.sln|.vcxproj
  packages.config           # Microsoft.Windows.CppWinRT only
  App.* / MainPage.*        # programmatic XAML UI
  probes.cpp                # LocalState, VirtualAlloc, outbound TCP, datadir
  node_host.cpp             # stub for future bitcoind
  log.cpp                   # LocalState\bitcoind.log
  Assets/*.png
scripts/build-uwp.ps1
```

## Build (Windows)

```powershell
.\scripts\build-uwp.ps1
# outputs under uwp\AppPackages\ + xbox_bitcoind-dev.cer
```

Requirements: VS 2022+ with **Universal Windows Platform** + C++ desktop, Windows
SDK **10.0.22621**, `nuget`.

CI: `.github/workflows/build-uwp.yml` on `windows-2022`.

## Deploy (Linux host → Series S)

```bash
source ~/.config/xllama/xbox-env   # or scripts/env.sh
./scripts/deploy.sh path/to/xbox_bitcoind_*.msix
# first time / new cert:
./scripts/deploy.sh install-cert path/to/xbox_bitcoind-dev.cer
./scripts/deploy.sh start-app
./scripts/deploy.sh get-log        # LocalState\bitcoind.log
./scripts/deploy.sh list-localstate
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

## Next integration (not in scaffold)

1. Link Bitcoin Core pin (static libs or object set) into the UWP project.  
2. Call `AppInitMain` / bitcoind entry with `-datadir=<LocalState>\bitcoin`.  
3. Disable subprocess notify hooks; keep `listen=0`.  
4. Stream status (block height, connections) into `MainPage`.  

See `docs/research/spikes/api-matrix.md`.
