[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerIp,

    [string]$RemoteUser = 'root',

    [string]$DistDir,

    [string]$PackagePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'WasmCommon.ps1')

$paths = Get-WasmPaths -WasmDir $PSScriptRoot
$stagingDir = Join-Path $env:TEMP 'kys-deploy'
$tarPath = Join-Path $env:TEMP 'kys-deploy.tar.gz'
$remote = "$RemoteUser@$ServerIp"

if (-not [string]::IsNullOrWhiteSpace($DistDir) -and -not [string]::IsNullOrWhiteSpace($PackagePath))
{
    throw 'Specify either DistDir or PackagePath, not both.'
}

if ([string]::IsNullOrWhiteSpace($DistDir) -and [string]::IsNullOrWhiteSpace($PackagePath))
{
    $DistDir = Join-Path $paths.WasmDir 'dist'
}

Write-Host '=== Checking build files ==='
if (-not [string]::IsNullOrWhiteSpace($PackagePath))
{
    Ensure-PathExists -Path $PackagePath -Message "WASM package not found: $PackagePath"

    if (Test-Path $stagingDir)
    {
        Remove-Item -Recurse -Force $stagingDir
    }

    Write-Host "=== Expanding package $PackagePath ==="
    Expand-Archive -Path $PackagePath -DestinationPath $stagingDir -Force

    foreach ($file in Get-WasmBuildArtifactPaths -BuildDir $stagingDir)
    {
        $info = Get-Item $file
        Write-Host "  $($info.Name) $(Format-FileSize -Bytes $info.Length)"
    }
}
else
{
    Ensure-PathExists -Path (Join-Path $DistDir 'kys\game') -Message "Game assets not found in $DistDir. Run package.ps1 first."

    foreach ($file in Get-WasmBuildArtifactPaths -BuildDir $DistDir)
    {
        $info = Get-Item $file
        Write-Host "  $($info.Name) $(Format-FileSize -Bytes $info.Length)"
    }
}

Write-Host ''
Write-Host '=== Packaging ==='
if ([string]::IsNullOrWhiteSpace($PackagePath))
{
    if (Test-Path $stagingDir)
    {
        Remove-Item -Recurse -Force $stagingDir
    }

    New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null
    Copy-Item -Recurse -Force (Join-Path $DistDir '*') $stagingDir
}

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