#Requires -Version 5.1
<#
.SYNOPSIS
  Build the xbox_bitcoind UWP MSIX for Xbox Series S|X (Dev Mode).

.DESCRIPTION
  MSBuild uwp/xbox_bitcoind.sln, generate test cert if needed, produce signed .msix.
  Requires Visual Studio with UWP + C++ workloads and Windows SDK 10.0.22621.

.PARAMETER Configuration
  Debug or Release (default Release)

.PARAMETER BuildRevision
  MSIX Identity Version revision (4th component). CI passes GITHUB_RUN_NUMBER.

.EXAMPLE
  .\scripts\build-uwp.ps1
  .\scripts\build-uwp.ps1 -Configuration Debug -ForceNewCert
#>
param(
    [ValidateSet("Release", "Debug")]
    [string] $Configuration = "Release",
    [string] $Platform = "x64",
    [switch] $ForceNewCert = $false,
    [int] $BuildRevision = $(if ($env:GITHUB_RUN_NUMBER) { [int]$env:GITHUB_RUN_NUMBER } else { 0 }),
    [string] $PlatformToolsetOverride = "",
    # Build+link Bitcoin Core UWP static libs into the package (slow; needs vcpkg x64-uwp).
    [switch] $WithCore = $false,
    [string] $CoreBuildDir = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path $PSScriptRoot -Parent
$SlnPath = Join-Path $RepoRoot "uwp\xbox_bitcoind.sln"
$UwpDir = Join-Path $RepoRoot "uwp"
$PfxPath = Join-Path $UwpDir "xbox_bitcoind-dev.pfx"
$CerPath = Join-Path $UwpDir "xbox_bitcoind-dev.cer"
$CertPwd = "xbox_bitcoind-test"
$CertSubject = "CN=xbox_bitcoind-dev"

if (-not $IsWindows -and $PSVersionTable.PSEdition -eq "Core" -and -not $env:OS.StartsWith("Windows")) {
    # PowerShell 5 on Windows has no $IsWindows
}
if ($env:OS -and $env:OS -notlike "*Windows*") {
    Write-Error "UWP packaging requires Windows with Visual Studio UWP workload."
    exit 1
}

if (-not (Test-Path $SlnPath)) {
    Write-Error "Solution not found: $SlnPath"
    exit 1
}

# --- certificate ---
$cert = $null
if ((-not $ForceNewCert) -and (Test-Path $PfxPath) -and (Test-Path $CerPath)) {
    Write-Host "Reusing existing test certificate ..."
    try {
        $cert = Get-PfxCertificate -FilePath $PfxPath -Password (ConvertTo-SecureString -String $CertPwd -Force -AsPlainText)
    } catch {
        Write-Warning "Failed to load existing PFX; regenerating."
        $cert = $null
    }
}

if (-not $cert) {
    Write-Host "Generating self-signed test certificate ($CertSubject) ..."
    $cert = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $CertSubject `
        -KeyUsage DigitalSignature `
        -FriendlyName "xbox_bitcoind test cert" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
    $pwd = ConvertTo-SecureString -String $CertPwd -Force -AsPlainText
    Export-PfxCertificate -Cert "Cert:\CurrentUser\My\$($cert.Thumbprint)" -FilePath $PfxPath -Password $pwd | Out-Null
    Export-Certificate -Cert "Cert:\CurrentUser\My\$($cert.Thumbprint)" -FilePath $CerPath | Out-Null
}

Write-Host "Certificate thumbprint: $($cert.Thumbprint)"
Write-Host "CER (install on console once): $CerPath"

# --- MSBuild ---
$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $VsWhere)) {
    Write-Error "vswhere.exe not found. Install Visual Studio with C++ UWP workload."
    exit 1
}

$MsBuild = & $VsWhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $MsBuild) {
    Write-Error "MSBuild not found."
    exit 1
}
Write-Host "Using MSBuild: $MsBuild"

# --- NuGet restore ---
Write-Host "Restoring NuGet packages ..."
$nuget = Get-Command nuget -ErrorAction SilentlyContinue
if ($nuget) {
    & nuget restore $SlnPath
} else {
    # nuget.exe may be missing; use msbuild restore if available
    & $MsBuild $SlnPath /t:Restore /p:Configuration=$Configuration /p:Platform=$Platform /nologo
}

# --- version stamp ---
$ManifestPath = Join-Path $UwpDir "AppxManifest.xml"
if ($BuildRevision -gt 0) {
    $manifestText = Get-Content -Raw $ManifestPath
    $rx = [regex]'(?<!\w)Version="(\d+)\.(\d+)\.(\d+)\.\d+"'
    $newText = $rx.Replace($manifestText, {
            param($m)
            'Version="{0}.{1}.{2}.{3}"' -f $m.Groups[1].Value, $m.Groups[2].Value, $m.Groups[3].Value, $BuildRevision
        }, 1)
    Set-Content -Path $ManifestPath -Value $newText -NoNewline
    $stamped = ([regex]::Match($newText, '(?<!\w)Version="(\d+\.\d+\.\d+\.\d+)"')).Groups[1].Value
    Write-Host "Version stamped: $stamped"
}

$CoreLibDir = ""
if ($WithCore) {
    Write-Host "=== WithCore: building Bitcoin Core for UWP ==="
    & (Join-Path $PSScriptRoot "build-core-uwp.ps1") -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "build-core-uwp.ps1 failed" }
    $coreRoot = if ($CoreBuildDir) { $CoreBuildDir } else { Join-Path $RepoRoot "third_party\bitcoin\build-uwp" }
    foreach ($c in @(
            (Join-Path $coreRoot "src\$Configuration"),
            (Join-Path $coreRoot "lib\$Configuration"),
            (Join-Path $coreRoot "src"),
            (Join-Path $coreRoot "lib"),
            $coreRoot
        )) {
        if (Test-Path (Join-Path $c "bitcoin_node.lib")) { $CoreLibDir = $c; break }
    }
    if (-not $CoreLibDir) {
        throw "WithCore set but bitcoin_node.lib not found under $coreRoot"
    }
    Write-Host "Linking Core from $CoreLibDir"
}

Write-Host "Building $Configuration|$Platform ..."
$MsBuildArgs = @(
    $SlnPath,
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:AppxPackageSigningEnabled=true",
    "/p:PackageCertificateKeyFile=$PfxPath",
    "/p:PackageCertificatePassword=$CertPwd",
    "/p:PackageCertificateThumbprint=$($cert.Thumbprint)",
    "/p:AppxPackageDir=$(Join-Path $UwpDir 'AppPackages')\",
    "/p:UapAppxPackageBuildMode=SideloadOnly",
    "/m",
    "/nologo",
    "/v:m"
)
if ($PlatformToolsetOverride) {
    $MsBuildArgs += "/p:PlatformToolsetOverride=$PlatformToolsetOverride"
}
if ($WithCore -and $CoreLibDir) {
    $MsBuildArgs += "/p:XbbWithCore=true"
    $MsBuildArgs += "/p:XbbCoreLibDir=$CoreLibDir"
    $MsBuildArgs += "/p:XbbCoreSrcDir=$(Join-Path $RepoRoot 'third_party\bitcoin\src')"
    $MsBuildArgs += "/p:XbbCoreBuildDir=$(if ($CoreBuildDir) { $CoreBuildDir } else { Join-Path $RepoRoot 'third_party\bitcoin\build-uwp' })"
}

& $MsBuild @MsBuildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "MSBuild failed ($LASTEXITCODE)"
    exit $LASTEXITCODE
}

$Msix = Get-ChildItem -Path (Join-Path $UwpDir "AppPackages") -Filter "*.msix" -Recurse -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $Msix) {
    # Some SDKs emit .appx
    $Msix = Get-ChildItem -Path (Join-Path $UwpDir "AppPackages") -Filter "*.appx" -Recurse -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
}

Write-Host "Build succeeded."
if ($Msix) {
    Write-Host "Package: $($Msix.FullName)"
    Write-Host "Deploy:  source ~/.config/xllama/xbox-env && ./scripts/deploy.sh $($Msix.FullName)"
    Write-Host "Then:    Dev Home → package → App type → Game"
    if ($env:GITHUB_OUTPUT) {
        "msix=$($Msix.FullName)" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
        "cer=$CerPath" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
    }
} else {
    Write-Warning "No .msix found under uwp\AppPackages — check MSBuild Appx output layout."
}
