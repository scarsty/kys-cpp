[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ConfigurePreset,

    [Parameter(Mandatory = $true)]
    [string]$BuildPreset
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$sourceDir = Split-Path -Parent $PSScriptRoot
$presetsPath = Join-Path $sourceDir 'CMakePresets.json'
$presets = Get-Content -LiteralPath $presetsPath -Raw | ConvertFrom-Json
$configurePresets = @{}
foreach ($preset in $presets.configurePresets) {
    $configurePresets[$preset.name] = $preset
}

function Resolve-ConfigurePresetBinaryDir {
    param(
        [string]$PresetName,
        [string[]]$InheritancePath = @()
    )

    if ($InheritancePath -contains $PresetName) {
        throw "Configure preset inheritance cycle: $($InheritancePath + $PresetName -join ' -> ')"
    }
    if (-not $configurePresets.ContainsKey($PresetName)) {
        throw "Unknown configure preset '$PresetName'."
    }

    $preset = $configurePresets[$PresetName]
    $binaryDirProperty = $preset.PSObject.Properties['binaryDir']
    if ($binaryDirProperty) {
        return [string]$binaryDirProperty.Value
    }

    $inheritsProperty = $preset.PSObject.Properties['inherits']
    if ($inheritsProperty) {
        foreach ($parentPresetName in @($inheritsProperty.Value)) {
            $binaryDir = Resolve-ConfigurePresetBinaryDir `
                -PresetName $parentPresetName `
                -InheritancePath ($InheritancePath + $PresetName)
            if ($binaryDir) {
                return $binaryDir
            }
        }
    }

    return $null
}

function Resolve-CMakeExecutable {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $bundledCMake = @(& $vswhere -latest -products * -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe') |
            Select-Object -First 1
        if ($bundledCMake) {
            return $bundledCMake
        }
    }

    throw 'CMake was not found on PATH or in the latest Visual Studio installation.'
}

$binaryDirExpression = Resolve-ConfigurePresetBinaryDir -PresetName $ConfigurePreset
if (-not $binaryDirExpression) {
    throw "Configure preset '$ConfigurePreset' does not define or inherit binaryDir."
}

$sourceParentDir = Split-Path -Parent $sourceDir
$sourceDirName = Split-Path -Leaf $sourceDir
$binaryDir = $binaryDirExpression `
    -replace [regex]::Escape('${sourceDir}'), $sourceDir `
    -replace [regex]::Escape('${sourceParentDir}'), $sourceParentDir `
    -replace [regex]::Escape('${sourceDirName}'), $sourceDirName `
    -replace [regex]::Escape('${presetName}'), $ConfigurePreset

if ($binaryDir -match '\$(?:env|penv)?\{') {
    throw "Unsupported macro in binaryDir for configure preset '$ConfigurePreset': $binaryDir"
}
if (-not [System.IO.Path]::IsPathRooted($binaryDir)) {
    $binaryDir = Join-Path $sourceDir $binaryDir
}
$binaryDir = [System.IO.Path]::GetFullPath($binaryDir)

$cachePath = Join-Path $binaryDir 'CMakeCache.txt'
$generationStampPath = Join-Path $binaryDir 'CMakeFiles\generate.stamp'
$configureInputs = @($presetsPath, (Join-Path $sourceDir 'CMakeUserPresets.json')) |
    Where-Object { Test-Path -LiteralPath $_ }

$configureReason = $null
if (-not (Test-Path -LiteralPath $cachePath)) {
    $configureReason = "CMake cache is missing at '$cachePath'"
} elseif (-not (Test-Path -LiteralPath $generationStampPath)) {
    $configureReason = "CMake generation stamp is missing at '$generationStampPath'"
} else {
    $generationTime = (Get-Item -LiteralPath $generationStampPath).LastWriteTimeUtc
    $newerConfigureInput = $configureInputs |
        Where-Object { (Get-Item -LiteralPath $_).LastWriteTimeUtc -gt $generationTime } |
        Select-Object -First 1
    if ($newerConfigureInput) {
        $configureReason = "'$newerConfigureInput' changed after the last CMake generation"
    }
}

$cmake = Resolve-CMakeExecutable
Push-Location $sourceDir
try {
    if ($configureReason) {
        Write-Host "Configuring preset '$ConfigurePreset': $configureReason."
        & $cmake --preset $ConfigurePreset
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } else {
        Write-Host "CMake cache is ready; skipping configure preset '$ConfigurePreset'."
    }

    & $cmake --build --preset $BuildPreset
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
