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
$tempRoot = Join-Path $env:TEMP "kys-deploy-$([guid]::NewGuid().ToString('N'))"
$stagingDir = Join-Path $tempRoot 'kys-deploy'
$tarPath = Join-Path $tempRoot 'kys-deploy.tar.gz'
$remote = "$RemoteUser@$ServerIp"

if (-not [string]::IsNullOrWhiteSpace($DistDir) -and -not [string]::IsNullOrWhiteSpace($PackagePath))
{
    throw 'Specify either DistDir or PackagePath, not both.'
}

if ([string]::IsNullOrWhiteSpace($DistDir) -and [string]::IsNullOrWhiteSpace($PackagePath))
{
    $DistDir = Join-Path $paths.WasmDir 'dist'
}

try
{
    New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

    if (-not [string]::IsNullOrWhiteSpace($PackagePath))
    {
        Assert-WasmDeploymentArchive -PackagePath $PackagePath
        Write-Host "=== Expanding package $PackagePath ==="
        Expand-Archive -Path $PackagePath -DestinationPath $stagingDir -Force
    }
    else
    {
        Assert-WasmDeploymentDirectory -DistDir $DistDir
        New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null
        Copy-Item -Recurse -Force (Join-Path $DistDir '*') $stagingDir
    }

    Assert-WasmDeploymentDirectory -DistDir $stagingDir

    Write-Host '=== Deployment files ==='
    foreach ($file in Get-WasmDeploymentArtifactPaths -DistDir $stagingDir)
    {
        $info = Get-Item $file
        $description = if ($info.Name -eq 'index.html') { ' (promo page)' } else { '' }
        Write-Host "  $($info.Name)$description $(Format-FileSize -Bytes $info.Length)"
    }

    Write-Host ''
    Write-Host '=== Packaging deployment archive ==='
    Invoke-NativeCommand -FilePath 'tar.exe' -ArgumentList @('-czf', $tarPath, '-C', $tempRoot, 'kys-deploy')

    $tarInfo = Get-Item $tarPath
    Write-Host "  Created $tarPath ($(Format-FileSize -Bytes $tarInfo.Length))"

    Write-Host ''
    Write-Host "=== Uploading to $remote ==="
    Invoke-NativeCommand -FilePath 'scp' -ArgumentList @($tarPath, "${remote}:/tmp/kys-deploy.tar.gz")

    Write-Host ''
    Write-Host 'Done. On the remote host run:'
    Write-Host '  cd /tmp && tar xzf kys-deploy.tar.gz'
    Write-Host '  CONTAINER=$(docker ps -q)'
    Write-Host '  docker exec $CONTAINER mkdir -p /var/www/html/kys'
    Write-Host '  docker cp /tmp/kys-deploy/. $CONTAINER:/var/www/html/kys/'
}
finally
{
    if (Test-Path $tempRoot)
    {
        Remove-Item -Recurse -Force $tempRoot
    }
}
