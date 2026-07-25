[CmdletBinding()]
param(
    [string]$GameDir,
    [string]$Version,
    [string]$ZipPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'WasmCommon.ps1')
. (Join-Path $PSScriptRoot '..\tools\ReleasePackagingCommon.ps1')

$paths = Get-WasmPaths -WasmDir $PSScriptRoot
$buildDir = $paths.BuildDir
$gameDir = $GameDir
if ([string]::IsNullOrWhiteSpace($gameDir))
{
    $gameDir = Join-Path $paths.ProjectDir 'work\game-dev'
}
$distDir = Join-Path $paths.WasmDir 'dist'
if ([string]::IsNullOrWhiteSpace($ZipPath))
{
    $ZipPath = Join-Path $paths.WasmDir 'dist.zip'
}

foreach ($file in Get-WasmBuildArtifactPaths -BuildDir $buildDir)
{
    Ensure-PathExists -Path $file -Message "Build output missing: $file. Run rebuild.ps1 first."
}

Ensure-PathExists -Path $gameDir -Message "Game assets not found at $gameDir"

& powershell -ExecutionPolicy Bypass -File (Join-Path $paths.ProjectDir 'tools\GenerateAppIcons.ps1') -Target Wasm
if ($LASTEXITCODE -ne 0)
{
    throw 'WASM icon generation failed.'
}

Write-Host '=== Packaging into dist/ ==='
if (Test-Path $distDir)
{
    Remove-Item -Recurse -Force $distDir
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $distDir 'kys') | Out-Null

Copy-Item -Force -Path (Get-WasmBuildArtifactPaths -BuildDir $buildDir) -Destination $distDir
foreach ($assetName in Get-WasmSiteAssetNames)
{
    Copy-Item -Force (Join-Path $paths.WasmDir $assetName) $distDir
}
$distGameDir = Join-Path $distDir 'kys\game'
Copy-ReleaseGameAssets -SourceGameDir $gameDir -DestinationGameDir $distGameDir -Version $Version
Build-WasmPromoPage -ProjectDir $paths.ProjectDir -GameDir $gameDir -OutputPath (Join-Path $distDir 'index.html')
Write-WasmAssetManifest `
    -ProjectDir $paths.ProjectDir `
    -WasmDir $paths.WasmDir `
    -GameDir $distGameDir `
    -OutputPath (Join-Path $distDir (Get-WasmManifestFileName))

# Set-Content -Path (Join-Path $distDir '_headers') -NoNewline -Value @'
# /*
#   Cache-Control: public, max-age=3600
# '@

# Copy-Item -Force (Join-Path $paths.WasmDir 'edgeone.json') $distDir

Assert-WasmDeploymentDirectory -DistDir $distDir
New-ZipFromDirectory -SourceDir $distDir -ZipPath $ZipPath
Assert-WasmDeploymentArchive -PackagePath $ZipPath

$zipInfo = Get-Item $ZipPath
Write-Host "  Created $ZipPath ($(Format-FileSize -Bytes $zipInfo.Length))"
Write-Host "  Promo page: index.html ($(Format-FileSize -Bytes (Get-Item (Join-Path $distDir 'index.html')).Length))"

$distFiles = Get-ChildItem -File -Recurse $distDir
$totalBytes = ($distFiles | Measure-Object -Property Length -Sum).Sum
if ($null -eq $totalBytes)
{
    $totalBytes = 0
}

Write-Host "  $($distFiles.Count) files, $(Format-FileSize -Bytes $totalBytes) total"
Write-Host ''
Write-Host 'Deploy with:'
Write-Host "  .\deploy.ps1 -ServerIp <server-ip> -PackagePath '$ZipPath'"
