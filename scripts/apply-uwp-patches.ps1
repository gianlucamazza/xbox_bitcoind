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

$patches = Get-ChildItem $PatchDir -Filter "*.patch" | Sort-Object Name
if (-not $patches) { throw "No patches in $PatchDir" }

# Idempotent marker bound to pin commit + patch-set hash (same format as the .sh
# applicator: sha256 of the concatenated patch bytes). A marker from a different
# pin or patch set must not skip application.
$pinLine = Select-String -Path (Join-Path $Root "config\bitcoin-core.pin") -Pattern '^COMMIT=(.+)$'
if (-not $pinLine) { throw "COMMIT missing from config/bitcoin-core.pin" }
$PinCommit = $pinLine.Matches[0].Groups[1].Value.Trim()

$tmp = [System.IO.Path]::GetTempFileName()
try {
    $out = [System.IO.File]::Create($tmp)
    try {
        foreach ($p in $patches) {
            $bytes = [System.IO.File]::ReadAllBytes($p.FullName)
            $out.Write($bytes, 0, $bytes.Length)
        }
    } finally {
        $out.Close()
    }
    $PatchHash = (Get-FileHash $tmp -Algorithm SHA256).Hash.ToLower()
} finally {
    Remove-Item -Force $tmp
}
$Expect = "${PinCommit}:${PatchHash}"

if (Test-Path $Marker) {
    $current = (Get-Content $Marker -Raw).Trim()
    if ($current -eq $Expect) {
        Write-Host "UWP patches already applied ($Expect)"
        exit 0
    }
    Write-Error "Stale patch marker (tree patched for $current, want $Expect). Re-run fetch-bitcoin-core to reset the tree, then retry."
    exit 1
}

Push-Location $Src
try {
    foreach ($p in $patches) {
        Write-Host "Applying $($p.Name) ..."
        git apply --check --whitespace=nowarn $p.FullName
        if ($LASTEXITCODE -ne 0) { throw "git apply --check failed: $($p.Name)" }
        git apply --whitespace=nowarn $p.FullName
        if ($LASTEXITCODE -ne 0) { throw "git apply failed: $($p.Name)" }
    }
} finally {
    Pop-Location
}
Set-Content -Path $Marker -Value $Expect
Write-Host "OK: applied $($patches.Count) UWP patches ($Expect)"
