#Requires -Version 5.1
<#
.SYNOPSIS
  Build a desktop MSVC baseline of pinned Bitcoin Core (no GUI, no wallet by default).

.DESCRIPTION
  Run from "Developer PowerShell for VS" (or any shell with cmake + MSVC + vcpkg).
  Reads config/bitcoin-core.pin and builds third_party/bitcoin with the vs2026
  CMake preset (Bitcoin Core v31.x).

  This is the Windows reference binary path before UWP porting — not an Xbox package.

.PARAMETER Configuration
  MSVC config: Release (default) or Debug.

.PARAMETER EnableWallet
  If set, build with SQLite wallet (vcpkg feature "wallet").

.PARAMETER SkipTests
  If set, do not build or run unit tests.

.PARAMETER BuildDir
  CMake binary dir (default: third_party/bitcoin/build-msvc-baseline).

.EXAMPLE
  .\scripts\build-msvc-baseline.ps1
  .\scripts\build-msvc-baseline.ps1 -EnableWallet
  .\scripts\build-msvc-baseline.ps1 -Configuration Debug -SkipTests
#>
[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string] $Configuration = "Release",
    [switch] $EnableWallet,
    [switch] $SkipTests,
    [string] $BuildDir = ""
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$PinFile = Join-Path $Root "config\bitcoin-core.pin"
$Src = Join-Path $Root "third_party\bitcoin"

function Read-Pin {
    param([string] $Path)
    $map = @{}
    Get-Content $Path | ForEach-Object {
        if ($_ -match '^\s*#' -or $_ -match '^\s*$') { return }
        if ($_ -match '^\s*([A-Za-z0-9_]+)=(.*)\s*$') {
            $map[$matches[1]] = $matches[2].Trim()
        }
    }
    return $map
}

if (-not (Test-Path $PinFile)) {
    throw "Missing pin file: $PinFile"
}
if (-not (Test-Path (Join-Path $Src "CMakeLists.txt"))) {
    throw "Bitcoin Core source missing at $Src — run scripts/fetch-bitcoin-core.sh (or clone v31.1) first."
}

$pin = Read-Pin $PinFile
$expected = $pin["COMMIT"]
$tag = $pin["TAG"]

Push-Location $Src
try {
    $head = (git rev-parse HEAD).Trim()
    if ($expected -and $head -ne $expected) {
        Write-Warning "HEAD is $head, pin expects $expected ($tag). Continue only if intentional."
    } else {
        Write-Host "OK: source $tag @ $head"
    }
} finally {
    Pop-Location
}

if (-not $BuildDir) {
    $BuildDir = Join-Path $Src "build-msvc-baseline"
}

# Prefer explicit VCPKG_ROOT, then GHA image var, then VS-bundled vcpkg
if (-not $env:VCPKG_ROOT) {
    if ($env:VCPKG_INSTALLATION_ROOT -and (Test-Path (Join-Path $env:VCPKG_INSTALLATION_ROOT "scripts\buildsystems\vcpkg.cmake"))) {
        $env:VCPKG_ROOT = $env:VCPKG_INSTALLATION_ROOT
        Write-Host "Using VCPKG_INSTALLATION_ROOT=$($env:VCPKG_ROOT)"
    }
}
if (-not $env:VCPKG_ROOT) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $candidate = Join-Path $installPath "VC\vcpkg"
        if (Test-Path (Join-Path $candidate "scripts\buildsystems\vcpkg.cmake")) {
            $env:VCPKG_ROOT = $candidate
        }
    }
}
if (-not $env:VCPKG_ROOT) {
    Write-Warning "VCPKG_ROOT not set. CMake preset vs2026 expects vcpkg (Visual Studio component or standalone)."
}

# Fail loud in CI if HEAD does not match pin
if ($env:CI -eq "true" -and $expected) {
    Push-Location $Src
    try {
        $headCi = (git rev-parse HEAD).Trim()
        if ($headCi -ne $expected) {
            throw "CI pin mismatch: HEAD=$headCi expected=$expected ($tag)"
        }
    } finally {
        Pop-Location
    }
}

$buildTests = -not $SkipTests
$wallet = [bool] $EnableWallet

# vcpkg features: avoid qt/zeromq defaults for a lean baseline
$features = @()
if ($buildTests) { $features += "tests" }
if ($wallet) { $features += "wallet" }
$featureArg = ($features -join ";")

Write-Host "=== MSVC baseline ==="
Write-Host "Source:  $Src"
Write-Host "Build:   $BuildDir"
Write-Host "Config:  $Configuration"
Write-Host "Wallet:  $wallet  Tests: $buildTests"
Write-Host "vcpkg features: $(if ($featureArg) { $featureArg } else { '(none — deps only)' })"

$cmakeArgs = @(
    "-B", $BuildDir,
    "--preset", "vs2026",
    "-DBUILD_GUI=OFF",
    "-DWITH_ZMQ=OFF",
    "-DENABLE_IPC=OFF",
    "-DENABLE_WALLET=$(if ($wallet) { 'ON' } else { 'OFF' })",
    "-DBUILD_TESTS=$(if ($buildTests) { 'ON' } else { 'OFF' })",
    "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON"
)
if ($featureArg) {
    $cmakeArgs += "-DVCPKG_MANIFEST_FEATURES=$featureArg"
}

Push-Location $Src
try {
    Write-Host "cmake configure..."
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)" }

    Write-Host "cmake build ($Configuration)..."
    & cmake --build $BuildDir --config $Configuration
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($LASTEXITCODE)" }

    if ($buildTests) {
        Write-Host "ctest..."
        & ctest --test-dir $BuildDir --build-config $Configuration --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "ctest failed ($LASTEXITCODE)" }
    }
} finally {
    Pop-Location
}

$bitcoind = Join-Path $BuildDir "$Configuration\bitcoind.exe"
if (-not (Test-Path $bitcoind)) {
    # multi-config vs single-config layout
    $alt = Get-ChildItem -Path $BuildDir -Recurse -Filter bitcoind.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($alt) { $bitcoind = $alt.FullName }
}

Write-Host ""
Write-Host "Baseline OK."
if (Test-Path $bitcoind) {
    Write-Host "bitcoind: $bitcoind"
    & $bitcoind -version
    if ($env:GITHUB_OUTPUT) {
        "bitcoind=$bitcoind" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
        "build_dir=$BuildDir" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
    }
} else {
    Write-Warning "bitcoind.exe not found under $BuildDir — check CMake output paths."
    if ($env:CI -eq "true") {
        throw "bitcoind.exe missing after CI build"
    }
}

Write-Host ""
Write-Host "Smoke (optional, not run automatically):"
Write-Host "  # regtest in a temp datadir"
Write-Host "  `$d = Join-Path `$env:TEMP bitcoin-regtest-baseline"
Write-Host "  & `"$bitcoind`" -regtest -datadir=`$d -server=1 -listen=0"
Write-Host ""
Write-Host "Record results in docs/research/spikes/desktop-baseline.md"
