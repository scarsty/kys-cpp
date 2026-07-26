[CmdletBinding()]
param(
    [string]$InstallRoot = (Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'kys_chess_mcp'),
    [string]$Python = 'python',
    [string]$McpName = 'kys_chess',
    [switch]$DeployOnly,
    [switch]$NoBuild,
    [switch]$SkipRegistration
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $repoRoot '.github\build-command.ps1'
$publishRoot = Join-Path $repoRoot 'x64\Release\kys_chess_cli.publish'
$resolvedInstallRoot = [IO.Path]::GetFullPath($InstallRoot)
$versionsRoot = Join-Path $resolvedInstallRoot 'versions'
$currentManifest = Join-Path $resolvedInstallRoot 'current.json'

function Get-PublishVersion([string]$Path) {
    $resolvedPath = [IO.Path]::GetFullPath($Path)
    $records = foreach ($file in Get-ChildItem -LiteralPath $Path -Recurse -File | Sort-Object FullName) {
        $relative = $file.FullName.Substring($resolvedPath.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$relative`:$hash"
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($records -join "`n"))
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha256.ComputeHash($bytes)
    }
    finally {
        $sha256.Dispose()
    }
    return (-join ($digest | ForEach-Object { $_.ToString('x2') })).Substring(0, 20)
}

function Invoke-CliSmokeTest([string]$Executable) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Arguments = 'new --difficulty normal --seed 1 --json'
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            throw "CLI 冒煙測試失敗，exit=$($process.ExitCode)：$stderr"
        }
        try {
            [void]($stdout | ConvertFrom-Json)
        }
        catch {
            throw "CLI 冒煙測試未傳回有效 JSON：$stdout`n$stderr"
        }
    }
    finally {
        $process.Dispose()
    }
}

function Write-CurrentManifest([string]$Version) {
    $manifest = [ordered]@{
        schema_version = 1
        version = $Version
        activation = [Guid]::NewGuid().ToString('N')
        executable = "versions/$Version/bin/kys_chess_cli.exe"
    }
    $temporary = "$currentManifest.$([Guid]::NewGuid().ToString('N')).tmp"
    [IO.File]::WriteAllText(
        $temporary,
        ($manifest | ConvertTo-Json),
        [Text.UTF8Encoding]::new($false))
    if (Test-Path -LiteralPath $currentManifest) {
        $backup = "$temporary.bak"
        try {
            [IO.File]::Replace($temporary, $currentManifest, $backup)
        }
        finally {
            [IO.File]::Delete($backup)
        }
    }
    else {
        Move-Item -LiteralPath $temporary -Destination $currentManifest
    }
}

if (-not $NoBuild) {
    Write-Host '建置 Release kys_chess_cli...'
    & $buildScript -Configuration Release -Platform x64 -Target kys_chess_cli
    if ($LASTEXITCODE -ne 0) {
        throw "Release CLI 建置失敗，exit=$LASTEXITCODE"
    }
}

if (-not (Test-Path -LiteralPath $publishRoot -PathType Container)) {
    throw "找不到 MCP 發佈目錄：$publishRoot"
}

New-Item -ItemType Directory -Path $versionsRoot -Force | Out-Null
$version = Get-PublishVersion $publishRoot
$versionRoot = Join-Path $versionsRoot $version
if (-not (Test-Path -LiteralPath $versionRoot -PathType Container)) {
    $stagingRoot = Join-Path $versionsRoot (".$version." + [Guid]::NewGuid().ToString('N') + '.tmp')
    New-Item -ItemType Directory -Path $stagingRoot | Out-Null
    try {
        Copy-Item -Path (Join-Path $publishRoot '*') -Destination $stagingRoot -Recurse -Force
        Invoke-CliSmokeTest (Join-Path $stagingRoot 'bin\kys_chess_cli.exe')
        Move-Item -LiteralPath $stagingRoot -Destination $versionRoot
    }
    catch {
        $resolvedStagingRoot = [IO.Path]::GetFullPath($stagingRoot)
        if ($resolvedStagingRoot.StartsWith(
                $versionsRoot + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase) -and
            (Test-Path -LiteralPath $resolvedStagingRoot)) {
            Remove-Item -LiteralPath $resolvedStagingRoot -Recurse -Force
        }
        throw
    }
}
else {
    Invoke-CliSmokeTest (Join-Path $versionRoot 'bin\kys_chess_cli.exe')
}

New-Item -ItemType Directory -Path $resolvedInstallRoot -Force | Out-Null
Write-CurrentManifest $version
Write-Host "已啟用 KYS 自走棋執行期 $version"

if (-not $DeployOnly) {
    $bridgeRoot = Join-Path $resolvedInstallRoot 'bridge'
    $bridgePython = Join-Path $bridgeRoot 'Scripts\python.exe'
    if (-not (Test-Path -LiteralPath $bridgePython -PathType Leaf)) {
        $pythonCommand = Get-Command $Python -ErrorAction Stop
        & $pythonCommand.Source -m venv $bridgeRoot
        if ($LASTEXITCODE -ne 0) {
            throw "無法建立 MCP Python 執行期，exit=$LASTEXITCODE"
        }
    }
    $bridgePackage = Join-Path $repoRoot 'tools\kys_chess_mcp'
    & $bridgePython -m pip install --disable-pip-version-check --upgrade $bridgePackage
    if ($LASTEXITCODE -ne 0) {
        throw "無法安裝 KYS 自走棋 MCP bridge，exit=$LASTEXITCODE"
    }
    & $bridgePython -m pip install --disable-pip-version-check --no-deps --force-reinstall $bridgePackage
    if ($LASTEXITCODE -ne 0) {
        throw "無法更新 KYS 自走棋 MCP bridge，exit=$LASTEXITCODE"
    }

    $launcherSource = Join-Path $repoRoot 'tools\kys_chess_mcp\Start-KysChessMcp.ps1'
    $launcher = Join-Path $resolvedInstallRoot 'Start-KysChessMcp.ps1'
    Copy-Item -LiteralPath $launcherSource -Destination $launcher -Force

    if (-not $SkipRegistration) {
        $codexCommand = Get-Command codex -ErrorAction Stop
        & $codexCommand.Source mcp get $McpName *> $null
        if ($LASTEXITCODE -eq 0) {
            & $codexCommand.Source mcp remove $McpName
            if ($LASTEXITCODE -ne 0) {
                throw "無法移除既有 MCP 設定 $McpName，exit=$LASTEXITCODE"
            }
        }
        & $codexCommand.Source mcp add $McpName -- powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $launcher
        if ($LASTEXITCODE -ne 0) {
            throw "無法註冊 Codex MCP $McpName，exit=$LASTEXITCODE"
        }
        Write-Host "已註冊 Codex MCP：$McpName"
    }
}

Write-Host '既有 MCP 工作階段會由 C++ CLI 寫入原生自動存檔，並在下一次工具呼叫前切換執行期及接續棋局.'
