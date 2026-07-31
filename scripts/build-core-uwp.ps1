#Requires -Version 5.1
<#
.SYNOPSIS
  Build Bitcoin Core static libraries for UWP (x64-uwp) from the pinned tree.

.DESCRIPTION
  Fetches pin if needed, applies UWP patches, configures CMake with WindowsStore
  + vcpkg triplet x64-uwp, builds bitcoin_node stack (no GUI/wallet).
  Output: third_party/bitcoin/build-uwp/ (+ xbb-core-libs.props)

  Use -SkipIfFresh to no-op when pin+patches stamp and bitcoin_embed.lib match
  (CI path: app-only changes reuse the cached Core build).

.PARAMETER Configuration
  Release (default) or Debug.

.PARAMETER BuildDirName
  CMake build directory name under third_party/bitcoin (default build-uwp).

.PARAMETER SkipIfFresh
  Skip configure/build when stamp matches and embed lib + props exist.

.PARAMETER Force
  Ignore SkipIfFresh / stamp; always rebuild.
#>
param(
    [string] $Configuration = "Release",
    [string] $BuildDirName = "build-uwp",
    [switch] $SkipIfFresh = $false,
    [switch] $Force = $false
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Src = Join-Path $Root "third_party\bitcoin"
$BuildDir = Join-Path $Src $BuildDirName
$PinFile = Join-Path $Root "config\bitcoin-core.pin"
$PatchDir = Join-Path $Root "patches\uwp"
$StampPath = Join-Path $BuildDir "xbb-core-uwp.stamp"
$PropsPath = Join-Path $BuildDir "xbb-core-libs.props"

function Get-CoreInputStamp {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $ms = New-Object System.IO.MemoryStream
    $w = New-Object System.IO.StreamWriter($ms)
    $w.WriteLine("v1")
    if (Test-Path $PinFile) {
        $w.WriteLine((Get-Content -Raw $PinFile))
    }
    Get-ChildItem $PatchDir -Filter "*.patch" -ErrorAction SilentlyContinue |
        Sort-Object Name |
        ForEach-Object {
            $w.WriteLine($_.Name)
            $w.WriteLine([System.BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($_.FullName))))
        }
    $w.Flush()
    $ms.Position = 0
    $hash = [System.BitConverter]::ToString($sha.ComputeHash($ms)).Replace("-", "").ToLowerInvariant()
    $w.Dispose()
    $ms.Dispose()
    $sha.Dispose()
    return $hash
}

function Test-CoreArtifactsPresent {
    if (-not (Test-Path $PropsPath)) { return $false }
    $embed = Get-ChildItem $BuildDir -Recurse -Filter "bitcoin_embed.lib" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    return [bool]$embed
}

if (-not (Test-Path (Join-Path $Src "CMakeLists.txt"))) {
    Write-Host "Fetching Bitcoin Core pin ..."
    & (Join-Path $PSScriptRoot "fetch-bitcoin-core.ps1")
}

$wantStamp = Get-CoreInputStamp
$haveStamp = if (Test-Path $StampPath) { (Get-Content -Raw $StampPath).Trim() } else { "" }

if (-not $Force -and ($SkipIfFresh -or $env:XBB_CORE_SKIP_IF_FRESH -eq "1") -and
    $haveStamp -eq $wantStamp -and (Test-CoreArtifactsPresent)) {
    Write-Host "SkipIfFresh: Core UWP libs match stamp $wantStamp (pin+patches unchanged)."
    Write-Host "  props: $PropsPath"
    if ($env:GITHUB_OUTPUT) {
        "core_build_dir=$BuildDir" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
        "core_skipped=true" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
    }
    exit 0
}

if ($Force) {
    Write-Host "Force: rebuilding Core UWP (ignoring stamp)."
} elseif ($haveStamp -and $haveStamp -ne $wantStamp) {
    Write-Host "Stamp changed ($haveStamp → $wantStamp); rebuilding Core UWP."
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

# Bitcoin Core v31.1 requires VS 2026 18.3+ (same as desktop baseline / vs2026 preset).
$Generator = "Visual Studio 17 2022"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsVer = & $vswhere -latest -property catalog_productLineVersion
    $vsMaj = & $vswhere -latest -property installationVersion
    Write-Host "VS productLineVersion=$vsVer installationVersion=$vsMaj"
    if ($vsVer -eq "2026" -or $vsVer -eq "18" -or ($vsMaj -and $vsMaj.ToString().StartsWith("18."))) {
        $Generator = "Visual Studio 18 2026"
    } else {
        Write-Warning "VS 2026 not detected. Core v31.1 needs VS 2026 18.3+; expect consteval (C7595) failures on older MSVC."
    }
}

if (Test-Path (Join-Path $BuildDir "CMakeCache.txt")) {
    $genLine = Select-String -Path (Join-Path $BuildDir "CMakeCache.txt") -Pattern 'CMAKE_GENERATOR:' -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty Line
    if ($genLine -match 'Ninja') {
        Write-Host "Removing stale Ninja build dir (VS generator required)"
        Remove-Item -Recurse -Force $BuildDir
    } elseif ($genLine) {
        Write-Host "Existing CMake cache: $genLine"
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
    "-DBUILD_DAEMON=ON",
    "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON",
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
Write-Host "Building Core UWP libraries (parallel): $($CoreTargets -join ', ') ..."
$targetArgs = @()
foreach ($t in $CoreTargets) { $targetArgs += @("--target", $t) }
# Single cmake --build with all targets + MSBuild parallelism
& cmake --build $BuildDir --config $Configuration --parallel @targetArgs
if ($LASTEXITCODE -ne 0) { throw "cmake build failed (Core UWP static libs)" }

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

$vcpkgLibCandidates = @(
    (Join-Path $BuildDir "vcpkg_installed\$Triplet\lib"),
    (Join-Path $env:VCPKG_ROOT "installed\$Triplet\lib")
)
$vcpkgLib = $null
foreach ($c in $vcpkgLibCandidates) {
    if (Test-Path $c) { $vcpkgLib = $c; break }
}
$vcpkgBinCandidates = @(
    (Join-Path $BuildDir "vcpkg_installed\$Triplet\bin"),
    (Join-Path $env:VCPKG_ROOT "installed\$Triplet\bin")
)
$vcpkgBin = $null
foreach ($c in $vcpkgBinCandidates) {
    if (Test-Path $c) { $vcpkgBin = $c; break }
}
if ($vcpkgLib) {
    Write-Host "vcpkg libs: $vcpkgLib"
} else {
    Write-Warning "vcpkg lib dir not found (link may miss libevent)"
}
if ($vcpkgBin) {
    Write-Host "vcpkg bin: $vcpkgBin"
} else {
    Write-Warning "vcpkg bin dir not found (MSIX may miss event.dll — app will fail to launch)"
}

$allLibDirs = @($libDirs) + @($vcpkgLib) | Where-Object { $_ } | Sort-Object -Unique

if ($env:GITHUB_OUTPUT) {
    "core_build_dir=$BuildDir" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
    "core_lib_dir=$found" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
    "core_skipped=false" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8
    if ($vcpkgLib) { "vcpkg_lib_dir=$vcpkgLib" | Out-File $env:GITHUB_OUTPUT -Append -Encoding utf8 }
}

$libDirsXml = ($allLibDirs | ForEach-Object { "      $_;" }) -join "`n"
@"
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <XbbCoreLibDir>$found</XbbCoreLibDir>
    <XbbVcpkgLibDir>$vcpkgLib</XbbVcpkgLibDir>
    <XbbVcpkgBinDir>$vcpkgBin</XbbVcpkgBinDir>
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
"@ | Set-Content -Path $PropsPath -Encoding UTF8
Set-Content -Path $StampPath -Value $wantStamp -NoNewline -Encoding ASCII
Write-Host "Wrote $PropsPath"
Write-Host "Wrote stamp $StampPath = $wantStamp"
Write-Host "OK: Core UWP build finished ($BuildDir)"
