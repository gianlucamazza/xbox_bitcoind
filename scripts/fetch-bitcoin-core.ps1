#Requires -Version 5.1
<#
.SYNOPSIS
  Clone or update third_party/bitcoin to the pinned commit (Windows).

.DESCRIPTION
  Mirror of scripts/fetch-bitcoin-core.sh for native PowerShell / GitHub Actions.
  Checks out COMMIT (not TAG) so annotated release tags do not emit git's
  "refs/tags/… is not a commit!" warning on shallow clones.
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

function Sync-PinnedCommit {
    param(
        [Parameter(Mandatory = $true)][string] $Dir,
        [Parameter(Mandatory = $true)][string] $Commit
    )
    Push-Location $Dir
    try {
        git fetch --depth 1 origin $Commit
        if ($LASTEXITCODE -ne 0) { throw "git fetch $Commit failed" }
        git checkout --detach FETCH_HEAD
        if ($LASTEXITCODE -ne 0) { throw "git checkout FETCH_HEAD failed" }
    } finally {
        Pop-Location
    }
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

# Cache may restore only build-uwp/ under Dest without a git tree — treat as incomplete.
if ((Test-Path $Dest) -and -not (Test-Path (Join-Path $Dest ".git"))) {
    Write-Host "Removing incomplete third_party/bitcoin (no .git; typically cache residue) ..."
    # Keep build-uwp if present by moving it aside.
    $buildUwp = Join-Path $Dest "build-uwp"
    $stash = Join-Path $Root "third_party\bitcoin-build-uwp-stash"
    if (Test-Path $buildUwp) {
        if (Test-Path $stash) { Remove-Item -Recurse -Force $stash }
        Move-Item $buildUwp $stash
    }
    Remove-Item -Recurse -Force $Dest
    if (Test-Path $stash) {
        New-Item -ItemType Directory -Force -Path $Dest | Out-Null
        Move-Item $stash (Join-Path $Dest "build-uwp")
    }
}

if (-not (Test-Path (Join-Path $Dest ".git"))) {
    Write-Host "Cloning $repo @ $commit ($tag) -> $Dest"
    # Clone into a temp dir if Dest already has build-uwp leftovers after partial clean.
    if ((Test-Path $Dest) -and (Get-ChildItem $Dest -Force | Measure-Object).Count -gt 0) {
        $tmp = Join-Path $Root "third_party\bitcoin-clone-tmp"
        if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
        New-Item -ItemType Directory -Force -Path $tmp | Out-Null
        git -C $tmp init
        if ($LASTEXITCODE -ne 0) { throw "git init failed" }
        git -C $tmp remote add origin $repo
        if ($LASTEXITCODE -ne 0) { throw "git remote add failed" }
        Sync-PinnedCommit -Dir $tmp -Commit $commit
        Get-ChildItem $tmp -Force | ForEach-Object {
            $target = Join-Path $Dest $_.Name
            if (-not (Test-Path $target)) {
                Move-Item $_.FullName $target
            }
        }
        Remove-Item -Recurse -Force $tmp
    } else {
        if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
        New-Item -ItemType Directory -Force -Path $Dest | Out-Null
        git -C $Dest init
        if ($LASTEXITCODE -ne 0) { throw "git init failed" }
        git -C $Dest remote add origin $repo
        if ($LASTEXITCODE -ne 0) { throw "git remote add failed" }
        Sync-PinnedCommit -Dir $Dest -Commit $commit
    }
} else {
    Write-Host "Updating existing clone at $Dest"
    # Ensure origin points at the pin repo (cache/local may differ).
    Push-Location $Dest
    try {
        git remote set-url origin $repo 2>$null
        if ($LASTEXITCODE -ne 0) {
            git remote add origin $repo
            if ($LASTEXITCODE -ne 0) { throw "git remote add origin failed" }
        }
        # A previously patched tree makes checkout fail ("local changes would be
        # overwritten"): reset tracked files and drop untracked ones (incl. the
        # patch marker), but keep build dirs — caches live under them.
        git reset --hard
        if ($LASTEXITCODE -ne 0) { throw "git reset --hard failed" }
        git clean -fdx -e build-uwp -e build-linux-smoke
        if ($LASTEXITCODE -ne 0) { throw "git clean failed" }
    } finally {
        Pop-Location
    }
    Sync-PinnedCommit -Dir $Dest -Commit $commit
}

Push-Location $Dest
try {
    $head = (git rev-parse HEAD).Trim()
    if ($head -ne $commit) {
        throw "checkout is $head, expected $commit ($tag)"
    }
    Write-Host "OK: Bitcoin Core $tag @ $head"
    git describe --tags --always 2>$null | Out-Host
    Write-Host "Tree: $Dest"
} finally {
    Pop-Location
}
