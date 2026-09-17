param(
    [string]$Version = "Beta-20260801",
    [string]$Iso = "build\xemu\StarTrekEliteForceX_Beta.iso"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not [System.IO.Path]::IsPathRooted($Iso)) {
    $Iso = Join-Path $repoRoot $Iso
}
$Iso = [System.IO.Path]::GetFullPath($Iso)

$extractXiso = "C:\nxdk\tools\extract-xiso\build\extract-xiso.exe"
$pythonCommand = Get-Command "python.exe" -CommandType Application -ErrorAction SilentlyContinue |
    Select-Object -First 1
$pythonExe = if ($null -ne $pythonCommand) {
    $pythonCommand.Source
}
else {
    Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
}
$releaseRoot = Join-Path $repoRoot "build\beta"
$packageDir = Join-Path $releaseRoot "StarTrekEliteForceX-$Version"
$packageIso = Join-Path $packageDir "StarTrekEliteForceX-$Version.iso"
$packageIso = [System.IO.Path]::GetFullPath($packageIso)
$betaPrefix = [System.IO.Path]::GetFullPath($releaseRoot).TrimEnd('\') + '\'
if (-not $packageIso.StartsWith($betaPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Beta output must remain under $releaseRoot"
}
if ($packageIso.Equals($Iso, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Beta output must differ from the source ISO"
}

if (-not (Test-Path -LiteralPath $Iso -PathType Leaf)) {
    throw "Beta ISO not found: $Iso"
}
if (-not (Test-Path -LiteralPath $extractXiso -PathType Leaf)) {
    throw "extract-xiso not found: $extractXiso"
}
if (-not (Test-Path -LiteralPath $pythonExe -PathType Leaf)) {
    throw "Python interpreter not found: $pythonExe"
}

$listing = @(& $extractXiso -l $Iso)
if ($LASTEXITCODE -ne 0) {
    throw "Could not list beta ISO: $Iso"
}

$markerPattern = "(?i)(ef_(?:sp|mp|runtime)|ja_sp|stefx_xemu|memmap).*\.(?:txt|done)"
$markers = @($listing | Select-String -Pattern $markerPattern)
if ($markers.Count -gt 0) {
    throw "Beta ISO contains diagnostic marker(s): $($markers.Line -join ', ')"
}

foreach ($required in @(
    "\default.xbe",
    "\efmp.xbe",
    "\BaseEF\soundbank\sound.bnk",
    "\BaseEF\soundbank\sound.tbl",
    "\BaseEF\xbox0.pk3",
    "\BaseEF\xbox1.pk3"
)) {
    if (-not ($listing | Select-String -SimpleMatch $required -Quiet)) {
        throw "Beta ISO is missing required payload: $required"
    }
}

New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
if (Test-Path -LiteralPath $packageIso -PathType Leaf) {
    Remove-Item -LiteralPath $packageIso -Force
}
# Keep the beta independent of the working ISO: probes temporarily modify it.
Copy-Item -LiteralPath $Iso -Destination $packageIso -Force

$componentPaths = [ordered]@{
    "default.xbe" = Join-Path $repoRoot "build\release\default.xbe"
    "efmp.xbe" = Join-Path $repoRoot "build\release\efmp.xbe"
    "BaseEF/xbox0.pk3" = Join-Path $repoRoot "build\release\BaseEF\xbox0.pk3"
    "BaseEF/xbox1.pk3" = Join-Path $repoRoot "build\release\BaseEF\xbox1.pk3"
    "BaseEF/soundbank/sound.bnk" = Join-Path $repoRoot "build\release\BaseEF\soundbank\sound.bnk"
    "BaseEF/soundbank/sound.tbl" = Join-Path $repoRoot "build\release\BaseEF\soundbank\sound.tbl"
}

$components = [ordered]@{}
foreach ($entry in $componentPaths.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "Built beta component not found: $($entry.Value)"
    }
    $item = Get-Item -LiteralPath $entry.Value
    $hash = Get-FileHash -LiteralPath $entry.Value -Algorithm SHA256
    $components[$entry.Key] = [ordered]@{
        bytes = $item.Length
        sha256 = $hash.Hash
    }
}

$isoItem = Get-Item -LiteralPath $packageIso
$isoHash = Get-FileHash -LiteralPath $packageIso -Algorithm SHA256
$gitRevision = (& git -C $repoRoot rev-parse HEAD).Trim()
$gitDirty = @(& git -C $repoRoot status --porcelain).Count -gt 0

$manifest = [ordered]@{
    name = "Star Trek: Elite Force X"
    version = $Version
    generatedUtc = (Get-Date).ToUniversalTime().ToString("o")
    sourceRevision = $gitRevision
    sourceTreeDirty = $gitDirty
    architecture = [ordered]@{
        entryPoint = "default.xbe"
        singlePlayerAndCoop = "default.xbe"
        holomatch = "efmp.xbe"
        sharedRuntime = "BaseEF"
        deprecatedCodempDependency = $false
    }
    iso = [ordered]@{
        file = [System.IO.Path]::GetFileName($packageIso)
        bytes = $isoItem.Length
        sha256 = $isoHash.Hash
        diagnosticMarkers = 0
    }
    components = $components
    qualification = @(
        "notes/mp_optimization_2026-09-10.md",
        "HOLOMATCH_QUALIFICATION.md"
    )
    qualificationScope = "Historical XEMU evidence applies to its recorded executable hashes; package creation does not qualify new binaries."
    pendingChecks = @(
        "Final packaged executable verification",
        "Retail Xbox performance and physical controllers"
    )
    deferred = @(
        "Co-op: not finished",
        "Muddy positional dialogue: investigation paused by user"
    )
}

$manifestPath = Join-Path $packageDir "release_manifest.json"
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
& $pythonExe (Join-Path $repoRoot "scripts\verify_beta_iso_components.py") `
    --iso $packageIso --manifest $manifestPath --release-root (Join-Path $repoRoot "build\release") > (Join-Path $packageDir "iso_component_verification.json")
if ($LASTEXITCODE -ne 0) {
    throw "Packaged ISO components do not match the release manifest"
}
$manifest.components = Get-Content -LiteralPath (Join-Path $packageDir "iso_component_verification.json") -Raw | ConvertFrom-Json
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

$checksumsPath = Join-Path $packageDir "SHA256SUMS.txt"
"$($isoHash.Hash.ToLowerInvariant())  $([System.IO.Path]::GetFileName($packageIso))" |
    Set-Content -LiteralPath $checksumsPath -Encoding ASCII

$readme = @"
Star Trek: Elite Force X - $Version

This is a beta candidate for testing.

- default.xbe: single-player campaign; co-op is not finished
- efmp.xbe: Holomatch, including four-player split screen
- BaseEF: shared runtime assets and game data

Preserve the directory layout when extracting the XISO for Xbox installation.

Recorded Holomatch performance evidence:

Four moving players and four active bots completed ten-minute XEMU runs on
hm_borg1 and hm_voy1, averaging 30.57 and 28.62 guest-clock FPS. Short windows
fell below 20 FPS. Those results apply to the executable hashes in the linked
qualification notes, not automatically to a newly packaged executable.
Retail Xbox performance and physical-controller checks remain outstanding.

Known limitations:

- Co-op is not finished and is the lowest release priority.
- Positional character voices remain muddy on Xbox; audio work is paused.
- Campaign route completion, the ending, and retail rendering checks remain
  open. See GAME_TODO.md for the current list.

The MP optimization item is closed under the user's 20+ average beta target
for the recorded XEMU workload. No additional 30 FPS pass is deferred here.

Verify the XISO with SHA256SUMS.txt. Detailed hashes and proof references are in
release_manifest.json.
"@
$readme | Set-Content -LiteralPath (Join-Path $packageDir "README.txt") -Encoding ASCII
Copy-Item -LiteralPath (Join-Path $repoRoot "HOLOMATCH_QUALIFICATION.md") `
    -Destination (Join-Path $packageDir "QUALIFICATION.md") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "GAME_TODO.md") `
    -Destination (Join-Path $packageDir "GAME_TODO.md") -Force

$verificationPath = Join-Path $packageDir "holomatch_verification.json"
& $pythonExe (Join-Path $repoRoot "scripts\check_mp_holomatch_ui.py") `
    --repo-root $repoRoot `
    --pk3 (Join-Path $repoRoot "build\release\BaseEF\xbox1.pk3") `
    --xbe (Join-Path $repoRoot "build\release\efmp.xbe") `
    --direct-map hm_borg1 `
    --code-only > $verificationPath
if ($LASTEXITCODE -ne 0) {
    throw "Holomatch beta verification failed with exit code $LASTEXITCODE"
}

Write-Host "Beta package ready: $packageDir"
Write-Host "ISO SHA256: $($isoHash.Hash)"
