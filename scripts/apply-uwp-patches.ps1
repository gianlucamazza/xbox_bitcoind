#Requires -Version 5.1
# Apply Xbox UWP patches to third_party/bitcoin
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Src = Join-Path $Root "third_party\bitcoin"
$PatchDir = Join-Path $Root "patches\uwp"
$Marker = Join-Path $Src ".xbb-uwp-patches-applied"

if (-not (Test-Path (Join-Path $Src "CMakeLists.txt"))) {
    throw "Core tree missing; run fetch-bitcoin-core first"
}
if (Test-Path $Marker) {
    Write-Host "UWP patches already applied ($((Get-Content $Marker)))"
    exit 0
}

$patches = Get-ChildItem $PatchDir -Filter "*.patch" | Sort-Object Name
if (-not $patches) { throw "No patches in $PatchDir" }

Push-Location $Src
try {
    foreach ($p in $patches) {
        Write-Host "Applying $($p.Name) ..."
        git apply --whitespace=nowarn $p.FullName
        if ($LASTEXITCODE -ne 0) { throw "git apply failed: $($p.Name)" }
    }
} finally {
    Pop-Location
}
Set-Content -Path $Marker -Value "uwp-$(Get-Date -Format o)"
Write-Host "OK: applied $($patches.Count) UWP patches"
