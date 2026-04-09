Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.IO.Compression.FileSystem

function Ensure-PathExists
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    if (-not (Test-Path $Path))
    {
        throw $Message
    }
}

function Invoke-NativeCommand
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$ArgumentList = @()
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0)
    {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($ArgumentList -join ' ')"
    }
}

function Invoke-RobocopyMirror
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination,

        [string[]]$ExcludeDirectories = @()
    )

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null

    $arguments = @(
        $Source,
        $Destination,
        '/MIR',
        '/NFL',
        '/NDL',
        '/NJH',
        '/NJS',
        '/NC',
        '/NS',
        '/NP'
    )

    if ($ExcludeDirectories.Count -gt 0)
    {
        $arguments += '/XD'
        $arguments += $ExcludeDirectories
    }

    $previousNativeErrorPreference = $null
    $hasNativeErrorPreference = $PSVersionTable.PSVersion.Major -ge 7 -and (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue)
    if ($hasNativeErrorPreference)
    {
        $previousNativeErrorPreference = $PSNativeCommandUseErrorActionPreference
        $PSNativeCommandUseErrorActionPreference = $false
    }

    try
    {
        & robocopy @arguments
    }
    finally
    {
        if ($hasNativeErrorPreference)
        {
            $PSNativeCommandUseErrorActionPreference = $previousNativeErrorPreference
        }
    }

    if ($LASTEXITCODE -gt 7)
    {
        throw "robocopy failed with exit code $LASTEXITCODE"
    }
}

function Get-DefaultReleaseSettingsJson
{
    @'
{
   "positionSwapEnabled": false,
   "musicVolume": 31,
   "soundVolume": 14,
   "manualCamera": false,
   "battleSpeed": 1,
   "simplifiedChinese": false,
   "showBattleLog": false
}
'@
}

function Get-ReleaseVersionConfigContent
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Version
    )

    @"
[release]
version = $Version
"@
}

function Set-Utf8NoBomFileContent
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Content
    )

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Copy-ReleaseGameAssets
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceGameDir,

        [Parameter(Mandatory = $true)]
        [string]$DestinationGameDir,

        [string]$Version
    )

    Ensure-PathExists -Path $SourceGameDir -Message "Game directory not found: $SourceGameDir"

    $sourceSaveDir = Join-Path $SourceGameDir 'save'
    Ensure-PathExists -Path (Join-Path $sourceSaveDir 'game.db') -Message "Required save data missing: $(Join-Path $sourceSaveDir 'game.db')"
    Ensure-PathExists -Path (Join-Path $sourceSaveDir '0.json') -Message "Required save data missing: $(Join-Path $sourceSaveDir '0.json')"

    Invoke-RobocopyMirror -Source $SourceGameDir -Destination $DestinationGameDir -ExcludeDirectories @('save')

    $destSaveDir = Join-Path $DestinationGameDir 'save'
    if (Test-Path $destSaveDir)
    {
        Remove-Item -Recurse -Force $destSaveDir
    }
    New-Item -ItemType Directory -Force -Path $destSaveDir | Out-Null

    Copy-Item (Join-Path $sourceSaveDir 'game.db') $destSaveDir -Force
    Copy-Item (Join-Path $sourceSaveDir '0.json') $destSaveDir -Force

    foreach ($archive in Get-ChildItem -Path $sourceSaveDir -Filter '*.grp.zip' -File -ErrorAction SilentlyContinue)
    {
        Copy-Item $archive.FullName $destSaveDir -Force
    }

    Set-Utf8NoBomFileContent -Path (Join-Path $destSaveDir 'setting.json') -Content (Get-DefaultReleaseSettingsJson)

    if (-not [string]::IsNullOrWhiteSpace($Version))
    {
        Set-Utf8NoBomFileContent -Path (Join-Path $DestinationGameDir 'config\release.ini') -Content (Get-ReleaseVersionConfigContent -Version $Version)
    }
}

function New-ZipFromDirectory
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,

        [Parameter(Mandatory = $true)]
        [string]$ZipPath
    )

    Ensure-PathExists -Path $SourceDir -Message "Source directory not found: $SourceDir"

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ZipPath) | Out-Null
    if (Test-Path $ZipPath)
    {
        Remove-Item -Force $ZipPath
    }

    [System.IO.Compression.ZipFile]::CreateFromDirectory($SourceDir, $ZipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
}

function Format-FileSize
{
    param(
        [Parameter(Mandatory = $true)]
        [long]$Bytes
    )

    if ($Bytes -ge 1GB)
    {
        return '{0:N2} GB' -f ($Bytes / 1GB)
    }

    if ($Bytes -ge 1MB)
    {
        return '{0:N2} MB' -f ($Bytes / 1MB)
    }

    if ($Bytes -ge 1KB)
    {
        return '{0:N2} KB' -f ($Bytes / 1KB)
    }

    "$Bytes B"
}

function Get-VersionedReleasePath
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$Version
    )

    $trimmedBasePath = $BasePath.TrimEnd('\', '/')
    $parent = Split-Path -Parent $trimmedBasePath
    $leaf = Split-Path -Leaf $trimmedBasePath
    if ([string]::IsNullOrWhiteSpace($leaf))
    {
        throw "Base path must include a directory name: $BasePath"
    }

    if ([string]::IsNullOrWhiteSpace($parent))
    {
        return "$leaf+$Version"
    }

    Join-Path $parent "$leaf+$Version"
}

function Convert-VersionToAndroidVersionCode
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Version
    )

    $match = [regex]::Match($Version, '^(\d+)\.(\d+)\.(\d+)$')
    if (-not $match.Success)
    {
        throw "Version must use major.minor.patch format for Android versionCode: $Version"
    }

    $major = [int]$match.Groups[1].Value
    $minor = [int]$match.Groups[2].Value
    $patch = [int]$match.Groups[3].Value
    return $major * 10000 + $minor * 100 + $patch
}