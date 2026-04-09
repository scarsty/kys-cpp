[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReleasePath,

    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$GameDir = (Join-Path $PSScriptRoot 'work\game-dev'),

    [string]$JavaHome = $env:JAVA_HOME
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'tools\ReleasePackagingCommon.ps1')

$outputDir = Get-VersionedReleasePath -BasePath $ReleasePath -Version $Version
$artifactBaseName = Split-Path -Leaf $outputDir
$stageRoot = Join-Path $PSScriptRoot 'build\release-staging'
$stageDir = Join-Path $stageRoot $artifactBaseName
$preparedGameDir = Join-Path $stageDir 'game'
$windowsPackageDir = Join-Path $stageDir 'windows'
$windowsZipPath = Join-Path $outputDir "$artifactBaseName.zip"
$wasmZipPath = Join-Path $outputDir 'dist.zip'
$apkOutputPath = Join-Path $outputDir "$artifactBaseName.apk"
$androidVersionCode = Convert-VersionToAndroidVersionCode -Version $Version
$androidDir = Join-Path $PSScriptRoot 'android'
$apkSourcePath = Join-Path $androidDir 'app\build\outputs\apk\release\app-release.apk'

if (Test-Path $outputDir)
{
    throw "Output directory already exists: $outputDir"
}

Write-Host "========================================"
Write-Host "Building All Release Artifacts"
Write-Host "Version: $Version"
Write-Host "Output:  $outputDir"
Write-Host "========================================"

Write-Host '[1/5] Preparing shared game assets...'
if (Test-Path $preparedGameDir)
{
    Remove-Item -Recurse -Force $preparedGameDir
}
if (Test-Path $stageDir)
{
    Remove-Item -Recurse -Force $stageDir
}
Copy-ReleaseGameAssets -SourceGameDir $GameDir -DestinationGameDir $preparedGameDir -Version $Version

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Write-Host '[2/5] Packaging Windows release...'
& (Join-Path $PSScriptRoot 'package_release.ps1') -PkgDir $windowsPackageDir -GameDir $preparedGameDir -ZipPath $windowsZipPath

Write-Host '[3/5] Building and packaging WASM release...'
& (Join-Path $PSScriptRoot 'wasm\rebuild.ps1') -GameDir $preparedGameDir
& (Join-Path $PSScriptRoot 'wasm\package.ps1') -GameDir $preparedGameDir -ZipPath $wasmZipPath

Write-Host '[4/5] Building Android release...'
if (-not [string]::IsNullOrWhiteSpace($JavaHome))
{
    $env:JAVA_HOME = $JavaHome
    $env:PATH = "$env:JAVA_HOME\bin;$env:PATH"
}

Push-Location $androidDir
try
{
    Invoke-NativeCommand -FilePath (Join-Path $androidDir 'gradlew.bat') -ArgumentList @(
        'assembleRelease',
        "-PgameDir=$preparedGameDir",
        "-PreleaseVersionName=$Version",
        "-PreleaseVersionCode=$androidVersionCode"
    )
}
finally
{
    Pop-Location
}

Ensure-PathExists -Path $apkSourcePath -Message "APK not found after Android build: $apkSourcePath"
Copy-Item $apkSourcePath $apkOutputPath -Force

Write-Host '[5/5] Finalizing output...'
if (Test-Path $windowsPackageDir)
{
    Remove-Item -Recurse -Force $windowsPackageDir
}
if (Test-Path $preparedGameDir)
{
    Remove-Item -Recurse -Force $preparedGameDir
}

$windowsZipInfo = Get-Item $windowsZipPath
$wasmZipInfo = Get-Item $wasmZipPath
$apkInfo = Get-Item $apkOutputPath

Write-Host ''
Write-Host 'Artifacts:'
Write-Host "  Windows: $windowsZipPath ($(Format-FileSize -Bytes $windowsZipInfo.Length))"
Write-Host "  WASM:    $wasmZipPath ($(Format-FileSize -Bytes $wasmZipInfo.Length))"
Write-Host "  Android: $apkOutputPath ($(Format-FileSize -Bytes $apkInfo.Length))"
Write-Host ''
Write-Host 'Deploy WASM with:'
Write-Host "  .\wasm\deploy.ps1 -ServerIp <server-ip> -PackagePath '$wasmZipPath'"