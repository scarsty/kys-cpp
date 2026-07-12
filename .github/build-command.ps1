<#
PowerShell build script for kys-cpp
Locates MSBuild using vswhere and invokes it on the solution with sensible defaults.
Usage:
  powershell -ExecutionPolicy Bypass -File .\.github\build-command.ps1
  or from PowerShell: .\.github\build-command.ps1 -Configuration Release -Platform x64
  defaults to building kys and kys_tests; pass -Target kys to build only one target
#>

param(
    [string]$Solution = 'd:\projects\kys-cpp\kys-cpp\kys.sln',
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [string[]]$Target = @('kys', 'kys_tests', 'kys_chess_cli'),
    [string]$Verbosity = 'minimal',
    [switch]$NoLogo
)

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'

Write-Host "[build-command.ps1] Solution: $Solution"
Write-Host "[build-command.ps1] Configuration: $Configuration | Platform: $Platform | Target(s): $($Target -join ', ')"

$msbuild = $null
if (Test-Path $vswhere) {
    Write-Host "Locating MSBuild via vswhere..."
    try {
        $instancesJson = & "$vswhere" -all -products * -requires Microsoft.Component.MSBuild -format json
        $instances = $instancesJson | ConvertFrom-Json
        $instance = $instances | Sort-Object { [version]$_.installationVersion } -Descending | Select-Object -First 1
        if ($instance) {
            $candidate = Join-Path $instance.installationPath 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path $candidate) {
                $msbuild = $candidate
            }
        }
    } catch {
        Write-Warning "vswhere invocation failed: $_"
    }
} else {
    Write-Host "vswhere not found at $vswhere"
}

if (-not $msbuild) {
    Write-Host "Falling back to 'msbuild.exe' (must be on PATH)."
    $msbuild = 'msbuild.exe'
}

# Verify msbuild exists or is callable
$msbuildPath = $null
if (Test-Path $msbuild) { $msbuildPath = $msbuild } else {
    $cmd = Get-Command $msbuild -ErrorAction SilentlyContinue
    if ($cmd) { $msbuildPath = $cmd.Source }
}

if (-not $msbuildPath) {
    Write-Error "MSBuild not found. Install Visual Studio with MSBuild or ensure msbuild.exe is on PATH."
    exit 2
}

$exitCode = 0
foreach ($targetName in $Target) {
    $msbuildArgs = @(
        "$Solution",
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/m",
        "/t:$targetName",
        "/verbosity:$Verbosity"
    )
    if (-not $NoLogo) { $msbuildArgs += "/nologo" }

    Write-Host "Invoking MSBuild: $msbuildPath"
    Write-Host "Arguments: $($msbuildArgs -join ' ')"

    # Invoke MSBuild directly so callers track a single process and receive the
    # actual completion/exit code even for long-running builds.
    & $msbuildPath @msbuildArgs
    $targetExitCode = $LASTEXITCODE
    Write-Host "MSBuild target '$targetName' exited with code $targetExitCode"

    if ($targetExitCode -ne 0 -and $exitCode -eq 0) {
        $exitCode = $targetExitCode
    }
}

Write-Host "MSBuild exited with code $exitCode"
exit $exitCode
