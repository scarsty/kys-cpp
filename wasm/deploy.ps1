[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerIp,

    [string]$RemoteUser = 'root'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'WasmCommon.ps1')

$paths = Get-WasmPaths -WasmDir $PSScriptRoot
$distDir = Join-Path $paths.WasmDir 'dist'
$stagingDir = Join-Path $env:TEMP 'kys-deploy'
$tarPath = Join-Path $env:TEMP 'kys-deploy.tar.gz'
$remote = "$RemoteUser@$ServerIp"

Ensure-PathExists -Path (Join-Path $distDir 'kys\game') -Message "Game assets not found in $distDir. Run package.ps1 first."

Write-Host '=== Checking build files ==='
foreach ($file in Get-WasmBuildArtifactPaths -BuildDir $distDir)
{
    $info = Get-Item $file
    Write-Host "  $($info.Name) $(Format-FileSize -Bytes $info.Length)"
}

Write-Host ''
Write-Host '=== Packaging ==='
if (Test-Path $stagingDir)
{
    Remove-Item -Recurse -Force $stagingDir
}

New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null
Copy-Item -Recurse -Force (Join-Path $distDir '*') $stagingDir

if (Test-Path $tarPath)
{
    Remove-Item -Force $tarPath
}

Invoke-NativeCommand -FilePath 'tar.exe' -ArgumentList @('-czf', $tarPath, '-C', $env:TEMP, 'kys-deploy')

$tarInfo = Get-Item $tarPath
Write-Host "  Created $tarPath ($(Format-FileSize -Bytes $tarInfo.Length))"

Write-Host ''
Write-Host "=== Uploading to $remote ==="
Invoke-NativeCommand -FilePath 'scp' -ArgumentList @($tarPath, "${remote}:/tmp/")

Write-Host ''
Write-Host 'Done. On the remote host run:'
Write-Host '  cd /tmp && tar xzf kys-deploy.tar.gz'
Write-Host '  CONTAINER=$(docker ps -q)'
Write-Host '  docker exec $CONTAINER mkdir -p /var/www/html/kys'
Write-Host '  docker cp /tmp/kys-deploy/. $CONTAINER:/var/www/html/kys/'