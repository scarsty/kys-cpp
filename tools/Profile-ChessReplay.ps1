#Requires -Version 7.0
<#
Run from a PowerShell 7 terminal in VS Code. Collect with Visual Studio
Diagnostics, then export CPU stack tables without opening Visual Studio.
This uses the Diagnostics CPU collector (ETW internally), not a separate ETW capture.
#>
param(
    [Parameter(Mandatory)][string]$Replay,
    [string]$CliPath = 'x64\Release\kys_chess_cli.exe',
    [string]$OutputPrefix = ('output\profiles\chess-' + (Get-Date -Format 'yyyyMMdd-HHmmss')),
    [ValidateRange(1, 100)][int]$Runs = 7,
    [string]$XperfPath = 'C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\xperf.exe'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
function Resolve-RepoPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}
$cliFile = (Resolve-Path -LiteralPath (Resolve-RepoPath $CliPath)).Path
$replayFile = (Resolve-Path -LiteralPath (Resolve-RepoPath $Replay)).Path
$prefix = Resolve-RepoPath $OutputPrefix
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $prefix) | Out-Null
$cliArguments = @('verify', $replayFile, '--data-root', "$repoRoot\work\game-dev", '--config-root', "$repoRoot\config")

function Invoke-ReplayValidation {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $cliFile
    $info.WorkingDirectory = $repoRoot
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [Text.Encoding]::UTF8
    $info.StandardErrorEncoding = [Text.Encoding]::UTF8
    foreach ($argument in $cliArguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        $timer = [Diagnostics.Stopwatch]::StartNew()
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $timer.Stop()
        $output = $stdout.GetAwaiter().GetResult()
        $errors = $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0 -or $output.TrimEnd().Split("`n")[-1].Trim() -ne '重播驗證成功') {
            throw "Replay verification failed ($($process.ExitCode)): $output $errors"
        }
        return [ordered]@{ wallSeconds = $timer.Elapsed.TotalSeconds; cpuSeconds = $process.TotalProcessorTime.TotalSeconds; exitCode = $process.ExitCode }
    } finally { $process.Dispose() }
}

$measurements = [ordered]@{
    gitRevision = (& git -C $repoRoot rev-parse HEAD)
    workingTree = @(& git -C $repoRoot status --short)
    executable = $cliFile
    executableSha256 = (Get-FileHash -LiteralPath $cliFile).Hash
    pdbSha256 = (Get-FileHash -LiteralPath ([IO.Path]::ChangeExtension($cliFile, '.pdb'))).Hash
    replay = $replayFile
    replaySha256 = (Get-FileHash -LiteralPath $replayFile).Hash
    workingDirectory = $repoRoot
    arguments = $cliArguments
    warmup = (Invoke-ReplayValidation)
    runs = @()
}
for ($runIndex = 1; $runIndex -le $Runs; ++$runIndex) {
    $run = Invoke-ReplayValidation
    $measurements.runs += $run
    Write-Host ("Run {0}: {1:F6}s wall, {2:F6}s CPU" -f $runIndex, $run.wallSeconds, $run.cpuSeconds)
}
$sorted = @($measurements.runs.wallSeconds | Sort-Object)
$middle = [int][Math]::Floor($Runs / 2)
$measurements.medianWallSeconds = if ($Runs % 2) { $sorted[$middle] } else { ($sorted[$middle - 1] + $sorted[$middle]) / 2 }
$measurements | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$prefix-benchmark.json" -Encoding utf8
Write-Host ("Unprofiled median: {0:F6}s" -f $measurements.medianWallSeconds)

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products * -property installationPath
$collector = Join-Path $vsRoot 'Team Tools\DiagnosticsHub\Collector\VSDiagnostics.exe'
$config = Join-Path (Split-Path -Parent $collector) 'AgentConfigs\CpuUsageHigh.json'
$sessionId = [Guid]::NewGuid().ToString()
$scratch = "$prefix-scratch"
$sessionFile = "$prefix.diagsession"
New-Item -ItemType Directory -Path $scratch | Out-Null
$launchArguments = 'verify "{0}" --data-root "{1}\work\game-dev" --config-root "{1}\config"' -f $replayFile, $repoRoot
$launchedAfter = Get-Date
& $collector start $sessionId "/launch:$cliFile" "/launchArgs:$launchArguments" "/loadConfig:$config" "/scratchLocation:$scratch"
if ($LASTEXITCODE -ne 0) { throw "VSDiagnostics start failed: $LASTEXITCODE" }
try {
    $deadline = (Get-Date).AddSeconds(5)
    do {
        $target = Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($cliFile)) -ErrorAction SilentlyContinue |
            Where-Object { $_.StartTime -ge $launchedAfter -and $_.Path -eq $cliFile } |
            Sort-Object StartTime -Descending | Select-Object -First 1
        if (-not $target) { Start-Sleep -Milliseconds 20 }
    } while (-not $target -and (Get-Date) -lt $deadline)
    if ($target) { $target.WaitForExit() }
} finally {
    & $collector stop $sessionId "/output:$sessionFile"
    if ($LASTEXITCODE -ne 0) { throw "VSDiagnostics stop failed: $LASTEXITCODE" }
}

$extract = "$prefix-extract"
[IO.Compression.ZipFile]::ExtractToDirectory($sessionFile, $extract)
$etl = (Get-ChildItem -LiteralPath $extract -Recurse -Filter 'sc.user_aux.etl' | Select-Object -First 1).FullName
if (-not $etl) { throw 'Diagnostics session has no CPU ETL data' }
$report = "$prefix-stacks.html"
$exportInfo = [Diagnostics.ProcessStartInfo]::new()
$exportInfo.FileName = $XperfPath
$exportInfo.UseShellExecute = $false
$exportInfo.Environment['_NT_SYMBOL_PATH'] = (Split-Path -Parent $cliFile) + ";srv*$repoRoot\output\profiles\symbols*https://msdl.microsoft.com/download/symbols"
$exportInfo.Environment['_NT_SYMCACHE_PATH'] = "$repoRoot\output\profiles\symcache"
foreach ($argument in @('-i', $etl, '-symbols', '-a', 'stack', '-butterfly', '1', '-process', [IO.Path]::GetFileName($cliFile), '-ao', $report)) { $exportInfo.ArgumentList.Add($argument) }
$exportProcess = [Diagnostics.Process]::Start($exportInfo)
try {
    $exportProcess.WaitForExit()
    if ($exportProcess.ExitCode -ne 0) { throw "CPU stack export failed: $($exportProcess.ExitCode)" }
} finally { $exportProcess.Dispose() }
& python "$PSScriptRoot\summarize_cpu_stacks.py" $report
if ($LASTEXITCODE -ne 0) { throw 'CPU stack summary failed' }
Write-Host "Capture: $sessionFile"
Write-Host "VS Code summary: $prefix-stacks.md"
