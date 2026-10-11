function Sync-GameConfig
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectDir,

        [Parameter(Mandatory = $true)]
        [string]$GameDir
    )

    $sourceConfigDir = Join-Path $ProjectDir 'config'
    $destinationConfigDir = Join-Path $GameDir 'config'
    Ensure-PathExists -Path $sourceConfigDir -Message "Canonical config directory not found at $sourceConfigDir"

    Write-Host '=== Synchronizing game config ==='
    New-Item -ItemType Directory -Force -Path $destinationConfigDir | Out-Null
    Copy-Item -Path (Join-Path $sourceConfigDir '*') -Destination $destinationConfigDir -Recurse -Force

    $changelog = Join-Path $ProjectDir 'docs\更新日志.md'
    if (Test-Path $changelog)
    {
        Copy-Item -Force $changelog (Join-Path $destinationConfigDir 'changelog.md')
    }
}
