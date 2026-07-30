# Bitcoin Core MSVC baseline (desktop)

**Pin:** see [`config/bitcoin-core.pin`](../config/bitcoin-core.pin)  
**Current:** **v31.1** @ `9be056a8a72b624dae9623b2f7bded92c2a21c91` (2026-07-08 release)

This is the **Windows desktop reference build** of the same tree we will port to
UWP. It is **not** an Xbox package.

Upstream guide: [bitcoin/doc/build-windows-msvc.md](https://github.com/bitcoin/bitcoin/blob/v31.1/doc/build-windows-msvc.md)

## Why MSVC first

| Reason | Detail |
|--------|--------|
| Official Windows path | CMake + vcpkg presets (`vs2026`, `vs2026-static`) |
| Closest to UWP toolchain | Same MSVC family as Appx packaging later |
| Catch Core issues early | Unit tests on desktop before AppContainer |

Linux smoke (`scripts/build-linux-smoke.sh`) only proves the pin configures/builds
on Arch; ship decisions still require the MSVC artifact.

## Prerequisites (Windows)

Minimum (from Core v31.1 docs):

- **Visual Studio 2026** (18.3+) with **Desktop development with C++**
- CMake + **vcpkg** (bundled with VS or standalone `VCPKG_ROOT`)
- Git for Windows
- Python 3 (for `ctest` suite)

Same Windows 11 VM used for xllama UWP is fine if it has the **NativeDesktop**
workload (xllama’s UWP workload alone is not enough for Core’s desktop preset).

```powershell
# Optional: install VS Community with C++ desktop (elevated)
winget install --id Microsoft.VisualStudio.Community --override "--wait --quiet --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.Git --includeRecommended"
```

Open **Developer PowerShell for VS** for the steps below.

## Fetch the pin

From the repo root (Git Bash or WSL can run the shell script; PowerShell can
clone manually):

```powershell
# Git Bash / WSL / Linux host:
./scripts/fetch-bitcoin-core.sh

# Or manual:
# git clone --branch v31.1 --depth 1 https://github.com/bitcoin/bitcoin.git third_party/bitcoin
# git -C third_party/bitcoin rev-parse HEAD
# # expect 9be056a8a72b624dae9623b2f7bded92c2a21c91
```

## Build (wrapper script)

```powershell
.\scripts\build-msvc-baseline.ps1
# with wallet:
.\scripts\build-msvc-baseline.ps1 -EnableWallet
# faster iterate:
.\scripts\build-msvc-baseline.ps1 -SkipTests
```

Default flags (aligned with pin / console v1):

| CMake / vcpkg | Value |
|---------------|--------|
| Preset | `vs2026` (dynamic `x64-windows`) |
| `BUILD_GUI` | OFF |
| `ENABLE_WALLET` | OFF (unless `-EnableWallet`) |
| `WITH_ZMQ` | OFF |
| `ENABLE_IPC` | OFF |
| `BUILD_TESTS` | ON (unless `-SkipTests`) |
| `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` | ON (no Qt / default features) |
| Features | `tests` (+ `wallet` if enabled) |

Equivalent manual commands:

```powershell
cd third_party\bitcoin
cmake -B build-msvc-baseline --preset vs2026 `
  -DBUILD_GUI=OFF -DENABLE_WALLET=OFF -DWITH_ZMQ=OFF -DENABLE_IPC=OFF `
  -DBUILD_TESTS=ON `
  -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON `
  -DVCPKG_MANIFEST_FEATURES=tests
cmake --build build-msvc-baseline --config Release
ctest --test-dir build-msvc-baseline --build-config Release --output-on-failure
.\build-msvc-baseline\Release\bitcoind.exe -version
```

If vcpkg hits “buildtrees path too long”:

```powershell
cmake ... -DVCPKG_INSTALL_OPTIONS="--x-buildtrees-root=C:\vcpkg"
```

## Optional regtest smoke

```powershell
$d = Join-Path $env:TEMP bitcoin-regtest-baseline
New-Item -ItemType Directory -Force -Path $d | Out-Null
# Use config from repo as a starting point (paths will differ on Windows)
& .\build-msvc-baseline\Release\bitcoind.exe -regtest -datadir=$d -server=1 -listen=0
```

Stop with `bitcoin-cli -regtest -datadir=$d stop` from the same build tree.

## Record results

Fill in [docs/research/spikes/desktop-baseline.md](research/spikes/desktop-baseline.md)
after a successful MSVC run (host OS, VS version, ctest summary, binary path).

## Relation to Xbox

```
third_party/bitcoin @ v31.1
        │
        ├─ MSVC desktop baseline  ← you are here
        │     (bitcoind.exe, tests)
        │
        └─ later: UWP host + AppContainer patches
              (MSIX, LocalState datadir, Game class)
```

Do **not** expect this `bitcoind.exe` to run on the console as-is (desktop Win32
vs UWP). The pin and flags are what we carry forward.

## Changing the pin

1. Edit `config/bitcoin-core.pin` (`TAG` + peeled `COMMIT`).
2. `./scripts/fetch-bitcoin-core.sh`
3. Rebuild MSVC baseline; update this doc’s “Current” line and spike log.
