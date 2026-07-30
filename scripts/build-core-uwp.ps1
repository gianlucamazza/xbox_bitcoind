#Requires -Version 5.1
<#
.SYNOPSIS
  Build Bitcoin Core static libraries for UWP (x64-uwp) from the pinned tree.

.DESCRIPTION
  Fetches pin if needed, applies UWP patches, configures CMake with WindowsStore
  + vcpkg triplet x64-uwp, builds bitcoin_node stack (no GUI/wallet).
  Output: third_party/bitcoin/build-uwp/ (libs under src/ or lib/)
#>
param(
    [string] $Configuration = "Release",
    [string] $BuildDirName = "build-uwp"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Src = Join-Path $Root "third_party\bitcoin"
$BuildDir = Join-Path $Src $BuildDirName

if (-not (Test-Path (Join-Path $Src "CMakeLists.txt"))) {
    Write-Host "Fetching Bitcoin Core pin ..."
    & (Join-Path $PSScriptRoot "fetch-bitcoin-core.ps1")
}

Write-Host "Applying UWP patches ..."
& (Join-Path $PSScriptRoot "apply-uwp-patches.ps1")

if (-not $env:VCPKG_ROOT) {
    if ($env:VCPKG_INSTALLATION_ROOT) { $env:VCPKG_ROOT = $env:VCPKG_INSTALLATION_ROOT }
}
if (-not $env:VCPKG_ROOT) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $ip = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $cand = Join-Path $ip "VC\vcpkg"
        if (Test-Path (Join-Path $cand "scripts\buildsystems\vcpkg.cmake")) {
            $env:VCPKG_ROOT = $cand
        }
    }
}
if (-not $env:VCPKG_ROOT) {
    throw "VCPKG_ROOT not set (need vcpkg with x64-uwp support)"
}
Write-Host "VCPKG_ROOT=$($env:VCPKG_ROOT)"

$Toolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
$Triplet = "x64-uwp"

# Prefer Ninja Multi-Config if ninja is available; else VS generator for UWP is awkward.
# Use Ninja + clang-cl or MSVC with explicit WindowsStore.
$Generator = "Ninja"
$ninja = Get-Command ninja -ErrorAction SilentlyContinue
if (-not $ninja) {
    # Fall back: Visual Studio generator still works with WindowsStore toolchain file from vcpkg
    $Generator = "Visual Studio 17 2022"
}

Write-Host "Configuring Core for UWP (triplet $Triplet, generator $Generator) ..."
$cmakeArgs = @(
    "-S", $Src,
    "-B", $BuildDir,
    "-G", $Generator,
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
    "-DVCPKG_TARGET_TRIPLET=$Triplet",
    "-DVCPKG_HOST_TRIPLET=x64-windows",
    "-DCMAKE_SYSTEM_NAME=WindowsStore",
    "-DCMAKE_SYSTEM_VERSION=10.0",
    "-DBUILD_GUI=OFF",
    "-DENABLE_WALLET=OFF",
    "-DWITH_ZMQ=OFF",
    "-DENABLE_IPC=OFF",
    "-DBUILD_TESTS=OFF",
    "-DBUILD_BENCH=OFF",
    "-DBUILD_FUZZ_BINARY=OFF",
    "-DBUILD_CLI=OFF",
    "-DBUILD_TX=OFF",
    "-DBUILD_UTIL=OFF",
    "-DBUILD_BITCOIN_BIN=OFF",
    "-DBUILD_DAEMON=ON",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON"
)

if ($Generator -like "Visual Studio*") {
    $cmakeArgs += @("-A", "x64")
    # strip CMAKE_BUILD_TYPE for multi-config
    $cmakeArgs = $cmakeArgs | Where-Object { $_ -notlike "-DCMAKE_BUILD_TYPE=*" }
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

Write-Host "Building Core UWP libraries ..."
if ($Generator -like "Visual Studio*") {
    & cmake --build $BuildDir --config $Configuration --parallel
} else {
    & cmake --build $BuildDir --parallel
}
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

# Export paths for the UWP app build
$libCandidates = @(
    (Join-Path $BuildDir "src\$Configuration"),
    (Join-Path $BuildDir "lib\$Configuration"),
    (Join-Path $BuildDir "src"),
    (Join-Path $BuildDir "lib"),
    $BuildDir
)
$found = $null
foreach ($c in $libCandidates) {
    if (Test-Path (Join-Path $c "bitcoin_node.lib")) { $found = $c; break }
    if (Test-Path (Join-Path $c "bitcoin_node.a")) { $found = $c; break }
}
if (-not $found) {
    Write-Warning "Could not locate bitcoin_node.lib — listing build dir:"
    Get-ChildItem $BuildDir -Recurse -Filter "bitcoin_node*" -ErrorAction SilentlyContinue | Select-Object -First 20 FullName
} else {
    Write-Host "Core libs: $found"
}

if ($env:GITHUB_OUTPUT) {
    "core_build_dir=$BuildDir" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
    if ($found) { "core_lib_dir=$found" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8 }
}

Write-Host "OK: Core UWP build finished ($BuildDir)"
