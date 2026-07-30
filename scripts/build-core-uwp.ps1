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

# Visual Studio generator is reliable on GHA windows-2022 (Ninja often missing from PATH).
$Generator = "Visual Studio 17 2022"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsVer = & $vswhere -latest -property catalog_productLineVersion
    if ($vsVer -eq "2026" -or $vsVer -eq "18") {
        $Generator = "Visual Studio 18 2026"
    }
}

Write-Host "Configuring Core for UWP (triplet $Triplet, generator $Generator) ..."
$cmakeArgs = @(
    "-S", $Src,
    "-B", $BuildDir,
    "-G", $Generator,
    "-A", "x64",
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
    "-DVCPKG_TARGET_TRIPLET=$Triplet",
    "-DVCPKG_HOST_TRIPLET=x64-windows",
    "-DCMAKE_SYSTEM_NAME=WindowsStore",
    "-DCMAKE_SYSTEM_VERSION=10.0",
    "-DBUILD_GUI=OFF",
    "-DENABLE_WALLET=OFF",
    "-DENABLE_EXTERNAL_SIGNER=OFF",
    "-DWITH_ZMQ=OFF",
    "-DENABLE_IPC=OFF",
    "-DBUILD_TESTS=OFF",
    "-DBUILD_BENCH=OFF",
    "-DBUILD_FUZZ_BINARY=OFF",
    "-DBUILD_CLI=OFF",
    "-DBUILD_TX=OFF",
    "-DBUILD_UTIL=OFF",
    "-DBUILD_BITCOIN_BIN=OFF",
    # Daemon target pulls bitcoin_node static lib; we only need libs for the UWP app embed.
    "-DBUILD_DAEMON=ON",
    "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON",
    # libevent marks !uwp in vcpkg; allow-unsupported to attempt AppContainer build (Dev Mode).
    "-DVCPKG_INSTALL_OPTIONS=--allow-unsupported;--x-buildtrees-root=C:/vcpkg-bt"
)

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

# Explicitly build every static lib the UWP app links. MSVC does not merge
# PRIVATE static deps into bitcoin_node.lib, and EXCLUDE_FROM_ALL targets may
# not emit .lib files unless named as build targets.
$CoreTargets = @(
    "bitcoin_clientversion",
    "bitcoin_crypto",
    "bitcoin_consensus",
    "bitcoin_util",
    "bitcoin_common",
    "leveldb",
    "crc32c",
    "minisketch",
    "secp256k1",
    "univalue",
    "bitcoin_node",
    "bitcoin_embed"
)
Write-Host "Building Core UWP libraries: $($CoreTargets -join ', ') ..."
$targetArgs = @()
foreach ($t in $CoreTargets) { $targetArgs += @("--target", $t) }
& cmake --build $BuildDir --config $Configuration --parallel @targetArgs
if ($LASTEXITCODE -ne 0) { throw "cmake build failed (Core UWP static libs)" }

# Collect every directory that holds a .lib under the Core build (MSVC multi-config scatters some).
$allLibs = Get-ChildItem $BuildDir -Recurse -Filter "*.lib" -ErrorAction SilentlyContinue
if (-not $allLibs) {
    throw "No .lib files produced under $BuildDir"
}
Write-Host "Core .lib inventory ($($allLibs.Count) files):"
$allLibs | ForEach-Object { Write-Host ("  " + $_.FullName.Substring($BuildDir.Length)) }

$libDirs = $allLibs | ForEach-Object { $_.DirectoryName } | Sort-Object -Unique
$found = ($libDirs | Where-Object { Test-Path (Join-Path $_ "bitcoin_embed.lib") } | Select-Object -First 1)
if (-not $found) {
    $found = ($libDirs | Where-Object { Test-Path (Join-Path $_ "bitcoin_node.lib") } | Select-Object -First 1)
}
if (-not $found) { throw "bitcoin_embed.lib / bitcoin_node.lib missing after build" }
Write-Host "Primary Core lib dir: $found"
Write-Host "All Core lib dirs: $($libDirs -join ';')"

# vcpkg installed libs (libevent, etc.) for final UWP link
$vcpkgLibCandidates = @(
    (Join-Path $BuildDir "vcpkg_installed\$Triplet\lib"),
    (Join-Path $env:VCPKG_ROOT "installed\$Triplet\lib")
)
$vcpkgLib = $null
foreach ($c in $vcpkgLibCandidates) {
    if (Test-Path $c) { $vcpkgLib = $c; break }
}
if ($vcpkgLib) {
    Write-Host "vcpkg libs: $vcpkgLib"
    Get-ChildItem $vcpkgLib -Filter "*.lib" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host ("  vcpkg: " + $_.Name) }
} else {
    Write-Warning "vcpkg lib dir not found (link may miss libevent)"
}

# Semicolon-separated path list for MSBuild AdditionalLibraryDirectories.
# Do NOT pass this via /p: (MSBuild splits on ';'). Put it in a .props file instead.
$allLibDirs = @($libDirs) + @($vcpkgLib) | Where-Object { $_ } | Sort-Object -Unique
$libPathList = $allLibDirs -join ";"

if ($env:GITHUB_OUTPUT) {
    "core_build_dir=$BuildDir" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
    "core_lib_dir=$found" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
    if ($vcpkgLib) { "vcpkg_lib_dir=$vcpkgLib" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8 }
}

# Props file imported by the UWP project when WithCore=true (avoids /p: semicolon split).
$propsPath = Join-Path $BuildDir "xbb-core-libs.props"
$libDirsXml = ($allLibDirs | ForEach-Object { "      $_;" }) -join "`n"
@"
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <XbbCoreLibDir>$found</XbbCoreLibDir>
    <XbbVcpkgLibDir>$vcpkgLib</XbbVcpkgLibDir>
    <XbbWithCore>true</XbbWithCore>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <Link>
      <AdditionalLibraryDirectories>
$libDirsXml
        %(AdditionalLibraryDirectories)
      </AdditionalLibraryDirectories>
    </Link>
  </ItemDefinitionGroup>
</Project>
"@ | Set-Content -Path $propsPath -Encoding UTF8
Write-Host "Wrote $propsPath"
Write-Host "OK: Core UWP build finished ($BuildDir)"
