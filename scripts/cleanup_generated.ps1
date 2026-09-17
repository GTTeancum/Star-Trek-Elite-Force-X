[CmdletBinding()]
param(
    [string]$CurrentIso = "",
    [switch]$PreserveStage,
    [switch]$RemoveHardwareStage,
    [switch]$Aggressive,
    [int]$KeepReports = 8,
    [int]$KeepLogs = 20,
    [int]$KeepImages = 20,
    [int]$KeepDumps = 1,
    [int]$KeepOther = 20
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$repoPrefix = $repoRoot.TrimEnd('\') + '\'
$removedFiles = 0
$removedBytes = [int64]0

function Get-RepoPath {
    param([Parameter(Mandatory=$true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if ($fullPath -eq $repoRoot -or
        -not $fullPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean outside the repository: $fullPath"
    }
    return $fullPath
}

function Remove-GeneratedItem {
    param([Parameter(Mandatory=$true)][string]$Path)

    $fullPath = Get-RepoPath -Path $Path
    if (-not (Test-Path -LiteralPath $fullPath)) {
        return
    }

    $item = Get-Item -LiteralPath $fullPath -Force
    if ($item.PSIsContainer) {
        $files = @(Get-ChildItem -LiteralPath $fullPath -Force -Recurse -File -ErrorAction SilentlyContinue)
        $script:removedFiles += $files.Count
        if ($files.Count -gt 0) {
            $measure = $files | Measure-Object -Property Length -Sum
            if ($null -ne $measure.Sum) {
                $script:removedBytes += [int64]$measure.Sum
            }
        }
        # A staged file can disappear between PowerShell's recursive
        # enumeration and deletion (for example while a prior packager handle
        # is closing). Retry the exact, already repo-bounded root so a benign
        # FileNotFound race cannot abort the next build or XEMU run.
        for ($attempt = 0; $attempt -lt 3 -and (Test-Path -LiteralPath $fullPath); $attempt++) {
            Remove-Item -LiteralPath $fullPath -Recurse -Force -ErrorAction SilentlyContinue
        }
        if (Test-Path -LiteralPath $fullPath) {
            Remove-Item -LiteralPath $fullPath -Recurse -Force
        }
    }
    else {
        try {
            Remove-Item -LiteralPath $fullPath -Force
        }
        catch {
            # A running emulator may retain its mounted ISO until the user
            # closes it.  Do not let that unrelated handle block packaging a
            # replacement under a different final name.
            Write-Warning "Could not remove in-use generated file: $fullPath"
            return
        }
        $script:removedFiles++
        $script:removedBytes += [int64]$item.Length
    }
}

function Add-NewestToKeepSet {
    param(
        [object[]]$Files,
        [int]$Count,
        [System.Collections.Generic.HashSet[string]]$KeepSet
    )

    if ($Count -le 0) {
        return
    }
    foreach ($file in ($Files | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First $Count)) {
        [void]$KeepSet.Add($file.FullName)
    }
}

function Remove-EmptyOutputDirectories {
    param([Parameter(Mandatory=$true)][string]$Root)

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        return
    }
    foreach ($dir in (Get-ChildItem -LiteralPath $Root -Force -Recurse -Directory |
        Sort-Object { $_.FullName.Length } -Descending)) {
        if (-not (Get-ChildItem -LiteralPath $dir.FullName -Force | Select-Object -First 1)) {
            Remove-Item -LiteralPath $dir.FullName -Force
        }
    }
}

$xemuDir = Join-Path $repoRoot "build\xemu"
if (Test-Path -LiteralPath $xemuDir -PathType Container) {
    $keepIsoPath = ""
    if (-not [string]::IsNullOrWhiteSpace($CurrentIso)) {
        $candidate = [System.IO.Path]::GetFullPath($CurrentIso)
        $xemuPrefix = ([System.IO.Path]::GetFullPath($xemuDir)).TrimEnd('\') + '\'
        if ($candidate.StartsWith($xemuPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $keepIsoPath = $candidate
        }
    }
    if ([string]::IsNullOrWhiteSpace($keepIsoPath)) {
        $latestIso = Get-ChildItem -LiteralPath $xemuDir -File -Filter "*.iso" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
        if ($null -ne $latestIso) {
            $keepIsoPath = $latestIso.FullName
        }
    }

    $keepIsoProfilePath = if ([string]::IsNullOrWhiteSpace($keepIsoPath)) {
        ""
    }
    else {
        "$keepIsoPath.smoke-profile.json"
    }
    foreach ($file in Get-ChildItem -LiteralPath $xemuDir -Force -File) {
        if (-not [string]::IsNullOrWhiteSpace($keepIsoPath) -and
            $file.FullName.Equals($keepIsoPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        if (-not [string]::IsNullOrWhiteSpace($keepIsoProfilePath) -and
            $file.FullName.Equals($keepIsoProfilePath, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        Remove-GeneratedItem -Path $file.FullName
    }
    foreach ($dir in Get-ChildItem -LiteralPath $xemuDir -Force -Directory) {
        if ($PreserveStage -and $dir.Name -eq "shared_stage") {
            continue
        }
        Remove-GeneratedItem -Path $dir.FullName
    }
}

foreach ($relativePath in @(
    "build\proof",
    "build\proofs",
    "build\temp",
    "build\tmp",
    "build\logs",
    "build\cxbx_runtime",
    "build\cxbx_capture_runtime",
    "build\cxbx-native-stage",
    "build\diagnostics",
    "build\research\zone-contract",
    "build\pak0.pk3",
    "codemp\goblib\Release",
    "codemp\x_exe\Release",
    "code\x_exe\__pycache__",
    "build\release\default!.xbe",
    "build\release\default_baseline.xbe",
    "build\release\defaultx.xbe",
    "scripts\__pycache__"
)) {
    Remove-GeneratedItem -Path (Join-Path $repoRoot $relativePath)
}

if ($RemoveHardwareStage) {
    Remove-GeneratedItem -Path (Join-Path $repoRoot "build\hardware")
}

foreach ($relativePath in @(
    "same",
    "set cg_thirdPerson 1",
    "set cg_thirdPersonAngle 0",
    "set cg_thirdPersonRange 80",
    "set stefx_smoke_fasttime 0",
    "set stefx_splitScreenMode coop",
    "set stefx_splitScreenP2Entity -1",
    "set stefx_splitScreenPlayers 2",
    "set timescale 1"
)) {
    Remove-GeneratedItem -Path (Join-Path $repoRoot $relativePath)
}

$buildRoot = Join-Path $repoRoot "build"
if (Test-Path -LiteralPath $buildRoot -PathType Container) {
    foreach ($file in Get-ChildItem -LiteralPath $buildRoot -Force -File) {
        if ($file.Extension -in @(".log", ".txt", ".pid", ".name", ".bin")) {
            Remove-GeneratedItem -Path $file.FullName
        }
    }
}

$trackedOutput = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$trackedOutputLines = @(& git -c "safe.directory=$repoRoot" -C $repoRoot ls-files --full-name scripts/output)
if ($LASTEXITCODE -ne 0) {
    throw "Could not enumerate tracked scripts/output files; cleanup aborted."
}
foreach ($tracked in $trackedOutputLines) {
    $trackedFullPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $tracked))
    [void]$trackedOutput.Add($trackedFullPath)
}

$outputDir = Join-Path $repoRoot "scripts\output"
if (Test-Path -LiteralPath $outputDir -PathType Container) {
    $untrackedFiles = @(
        Get-ChildItem -LiteralPath $outputDir -Force -Recurse -File |
        Where-Object { -not $trackedOutput.Contains($_.FullName) }
    )
    $keep = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $reports = @($untrackedFiles | Where-Object { $_.Name -like "*.report.txt" })
    $logs = @($untrackedFiles | Where-Object { $_.Extension -eq ".log" })
    $images = @($untrackedFiles | Where-Object { $_.Extension -in @(".png", ".jpg", ".jpeg") })
    $dumps = @($untrackedFiles | Where-Object { $_.Extension -in @(".bin", ".dmp", ".raw") })
    $categorized = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($file in @($reports + $logs + $images + $dumps)) {
        [void]$categorized.Add($file.FullName)
    }
    $other = @($untrackedFiles | Where-Object { -not $categorized.Contains($_.FullName) })

    Add-NewestToKeepSet -Files $reports -Count $KeepReports -KeepSet $keep
    Add-NewestToKeepSet -Files $logs -Count $KeepLogs -KeepSet $keep
    Add-NewestToKeepSet -Files $images -Count $KeepImages -KeepSet $keep
    Add-NewestToKeepSet -Files $dumps -Count $KeepDumps -KeepSet $keep
    Add-NewestToKeepSet -Files $other -Count $KeepOther -KeepSet $keep

    foreach ($file in $untrackedFiles) {
        if (-not $keep.Contains($file.FullName)) {
            Remove-GeneratedItem -Path $file.FullName
        }
    }
    Remove-EmptyOutputDirectories -Root $outputDir
}

foreach ($pattern in @(
    "build_*.log",
    "repack_*.log",
    "xemu*.log",
    "hash_header.obj",
    "xxhash.obj",
    "*.pid",
    "*.name"
)) {
    foreach ($file in Get-ChildItem -LiteralPath $repoRoot -Force -File -Filter $pattern) {
        Remove-GeneratedItem -Path $file.FullName
    }
}

if ($Aggressive) {
    Remove-GeneratedItem -Path (Join-Path $repoRoot "artifacts")
    Remove-GeneratedItem -Path (Join-Path $repoRoot "build\beta")
    Remove-GeneratedItem -Path (Join-Path $repoRoot "build\release\proof")
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repoRoot "build\release") -Force -File -ErrorAction SilentlyContinue) {
        if ($file.Extension -eq ".log" -or $file.Name -like "codex_*_ef_sp_log.txt") {
            Remove-GeneratedItem -Path $file.FullName
        }
    }
}

# The shipping movie path decodes the original retail BIK files directly.
# Remove artifacts from the abandoned BIK-to-XMV experiment so packaging can
# never prefer or accidentally retain a second generated movie set.
$releaseVideoDir = Join-Path $repoRoot "build\release\BaseEF\video"
if (Test-Path -LiteralPath $releaseVideoDir -PathType Container) {
    foreach ($file in Get-ChildItem -LiteralPath $releaseVideoDir -File -Filter "*.xmv" -ErrorAction SilentlyContinue) {
        Remove-GeneratedItem -Path $file.FullName
    }
    Remove-GeneratedItem -Path (Join-Path $releaseVideoDir "xbox_video_assets_manifest.json")
}

$removedGiB = [math]::Round(($removedBytes / 1GB), 2)
Write-Host "Generated-artifact cleanup removed $removedFiles files ($removedGiB GiB logical)."
