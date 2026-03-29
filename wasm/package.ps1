[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.IO.Compression.FileSystem

. (Join-Path $PSScriptRoot 'WasmCommon.ps1')

$paths = Get-WasmPaths -WasmDir $PSScriptRoot
$buildDir = $paths.BuildDir
$gameDir = Join-Path $paths.ProjectDir 'work\game-dev'
$distDir = Join-Path $paths.WasmDir 'dist'
$zipPath = Join-Path $paths.WasmDir 'dist.zip'

foreach ($file in Get-WasmBuildArtifactPaths -BuildDir $buildDir)
{
    Ensure-PathExists -Path $file -Message "Build output missing: $file. Run rebuild.ps1 first."
}

Ensure-PathExists -Path $gameDir -Message "Game assets not found at $gameDir"

Write-Host '=== Packaging into dist/ ==='
if (Test-Path $distDir)
{
    Remove-Item -Recurse -Force $distDir
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $distDir 'kys') | Out-Null

Copy-Item -Force (Join-Path $buildDir 'index.html') $distDir
Copy-Item -Force -Path (Get-WasmBuildArtifactPaths -BuildDir $buildDir) -Destination $distDir
Copy-Item -Recurse -Force $gameDir (Join-Path $distDir 'kys\game')

# Set-Content -Path (Join-Path $distDir '_headers') -NoNewline -Value @'
# /*
#   Cache-Control: public, max-age=3600
# '@

# Copy-Item -Force (Join-Path $paths.WasmDir 'edgeone.json') $distDir

if (Test-Path $zipPath)
{
    Remove-Item -Force $zipPath
}

[System.IO.Compression.ZipFile]::CreateFromDirectory($distDir, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)

$zipInfo = Get-Item $zipPath
Write-Host "  Created $zipPath ($(Format-FileSize -Bytes $zipInfo.Length))"

$distFiles = Get-ChildItem -File -Recurse $distDir
$totalBytes = ($distFiles | Measure-Object -Property Length -Sum).Sum
if ($null -eq $totalBytes)
{
    $totalBytes = 0
}

Write-Host "  $($distFiles.Count) files, $(Format-FileSize -Bytes $totalBytes) total"
Write-Host ''
Write-Host 'Deploy with:'
Write-Host "  Cloudflare: wrangler pages deploy $distDir --project-name=kyschess"
Write-Host "  EdgeOne:    upload $zipPath"