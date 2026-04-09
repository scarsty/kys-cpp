param(
    [string]$GameDir = (Join-Path $PSScriptRoot "..\work\game-dev"),
    [string]$Version
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '..\tools\ReleasePackagingCommon.ps1')

$dest = Join-Path $PSScriptRoot 'app\src\main\assets\game'

Write-Host "Copying Android assets from $GameDir to $dest"
Copy-ReleaseGameAssets -SourceGameDir $GameDir -DestinationGameDir $dest -Version $Version
Write-Host 'Assets copied successfully.'
