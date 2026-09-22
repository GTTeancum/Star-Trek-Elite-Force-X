param(
    [Parameter(Mandatory=$true)][string]$Output,
    [ValidateRange(3,3600)][int]$Seconds = 300,
    [ValidateRange(1,10)][int]$Interval = 3,
    [string]$StopFile
)
$ErrorActionPreference = 'Stop'
$gpuOutputPath = [System.IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $gpuOutputPath) { throw "Output already exists: $gpuOutputPath" }
$gpuStream = [System.IO.StreamWriter]::new($gpuOutputPath, $false, [System.Text.UTF8Encoding]::new($false))
try {
    $gpuClock = [System.Diagnostics.Stopwatch]::StartNew()
    # Process creation/exit can invalidate individual GPU counter instances.
    # Keep collecting valid instances and expose invalid counts; never silently
    # terminate the entire sidecar because one sample is unavailable.
    while ($gpuClock.Elapsed.TotalSeconds -lt $Seconds -and (-not $StopFile -or -not (Test-Path -LiteralPath $StopFile))) {
        # Reopen the wildcard query to discover newly launched processes.
        # Use the second reading for a warmed utilization delta.
        $gpuErrors = @()
        $gpuBatch = @(Get-Counter '\GPU Engine(*)\Utilization Percentage' -SampleInterval $Interval -MaxSamples 2 -ErrorAction SilentlyContinue -ErrorVariable gpuErrors)
        if (!$gpuBatch.Count) {
            $gpuStream.WriteLine((@{epoch=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0;unavailable=$true;counter_errors=$gpuErrors.Count;engines=@()} | ConvertTo-Json -Compress))
            $gpuStream.Flush()
            Start-Sleep -Seconds $Interval
            continue
        }
        $gpuSample = $gpuBatch[-1]
        $gpuSampleTime = [DateTimeOffset]$gpuSample.Timestamp
        $gpuEngines = @($gpuSample.CounterSamples | Where-Object { $_.Status -le 1 -and $_.CookedValue -gt 0.01 } | ForEach-Object {
            $gpuPid = if ($_.InstanceName -match '^pid_(\d+)_') { [int]$Matches[1] } else { 0 }
            $gpuProcess = Get-Process -Id $gpuPid -ErrorAction SilentlyContinue
            [ordered]@{instance=$_.InstanceName;pid=$gpuPid;name=$gpuProcess.ProcessName;percent=$_.CookedValue;status=$_.Status}
        })
        $gpuInvalid = @($gpuSample.CounterSamples | Where-Object { $_.Status -gt 1 }).Count
        $gpuRow = [ordered]@{epoch=$gpuSampleTime.ToUnixTimeMilliseconds()/1000.0;engines=$gpuEngines;invalid_instances=$gpuInvalid;sample_instances=@($gpuSample.CounterSamples).Count;counter_errors=$gpuErrors.Count;refresh_instances=$true;source='Windows GPU Engine performance counters; read-only; no UI or process changes'}
        $gpuStream.WriteLine(($gpuRow | ConvertTo-Json -Compress -Depth 4))
        $gpuStream.Flush()
    }
} finally { $gpuStream.Dispose() }
