#Requires -Version 5.1
<#
.SYNOPSIS
  Clone or update third_party/bitcoin to the pinned tag/commit (Windows).

.DESCRIPTION
  Mirror of scripts/fetch-bitcoin-core.sh for native PowerShell / GitHub Actions.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$PinFile = Join-Path $Root "config\bitcoin-core.pin"
$Dest = Join-Path $Root "third_party\bitcoin"

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

$pin = Read-Pin $PinFile
$tag = $pin["TAG"]
$commit = $pin["COMMIT"]
$repo = $pin["REPO_URL"]
if (-not $tag -or -not $commit -or -not $repo) {
    throw "Pin must define TAG, COMMIT, REPO_URL"
}

New-Item -ItemType Directory -Force -Path (Join-Path $Root "third_party") | Out-Null

if (-not (Test-Path (Join-Path $Dest ".git"))) {
    Write-Host "Cloning $repo ($tag) -> $Dest"
    git clone --branch $tag --depth 1 $repo $Dest
    if ($LASTEXITCODE -ne 0) { throw "git clone failed" }
} else {
    Write-Host "Updating existing clone at $Dest"
    Push-Location $Dest
    try {
        git fetch --depth 1 origin "refs/tags/${tag}:refs/tags/${tag}" 2>$null
        if ($LASTEXITCODE -ne 0) {
            git fetch --depth 1 origin tag $tag
        }
        git checkout --detach $commit 2>$null
        if ($LASTEXITCODE -ne 0) {
            git checkout --detach "tags/$tag"
        }
    } finally {
        Pop-Location
    }
}

Push-Location $Dest
try {
    $head = (git rev-parse HEAD).Trim()
    if ($head -ne $commit) {
        Write-Host "HEAD $head != pin $commit; fetching commit..."
        git fetch --depth 1 origin $commit 2>$null
        git checkout --detach $commit
        if ($LASTEXITCODE -ne 0) { throw "checkout $commit failed" }
        $head = (git rev-parse HEAD).Trim()
    }
    if ($head -ne $commit) {
        throw "checkout is $head, expected $commit ($tag)"
    }
    Write-Host "OK: Bitcoin Core $tag @ $head"
    git describe --tags --always
    Write-Host "Tree: $Dest"
} finally {
    Pop-Location
}
