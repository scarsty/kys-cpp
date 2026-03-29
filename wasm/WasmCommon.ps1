Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($PSVersionTable.PSVersion.Major -ge 7)
{
    $PSNativeCommandUseErrorActionPreference = $true
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

function Get-WasmPaths
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$WasmDir,

        [string]$VcpkgRoot = $env:VCPKG_ROOT,
        [string]$EmsdkRoot = $env:EMSDK,
        [string]$VcpkgWasm
    )

    if ([string]::IsNullOrWhiteSpace($VcpkgRoot))
    {
        $VcpkgRoot = 'D:\projects\vcpkg'
    }

    if ([string]::IsNullOrWhiteSpace($EmsdkRoot))
    {
        $EmsdkRoot = 'D:\projects\emsdk'
    }

    $projectDir = Split-Path -Parent $WasmDir
    $buildDir = Join-Path $WasmDir 'build'
    $vcpkgManifestRoot = $WasmDir
    $vcpkgInstallRoot = Join-Path $WasmDir 'vcpkg_installed'

    if ([string]::IsNullOrWhiteSpace($VcpkgWasm))
    {
        $VcpkgWasm = Join-Path $vcpkgInstallRoot 'wasm32-emscripten'
    }

    [pscustomobject]@{
        WasmDir = $WasmDir
        ProjectDir = $projectDir
        BuildDir = $buildDir
        VcpkgRoot = $VcpkgRoot
        EmsdkRoot = $EmsdkRoot
        VcpkgManifestRoot = $vcpkgManifestRoot
        VcpkgInstallRoot = $vcpkgInstallRoot
        VcpkgWasm = $VcpkgWasm
        CMakeDir = Join-Path $VcpkgRoot 'downloads\tools\cmake-3.31.10-windows\cmake-3.31.10-windows-x86_64\bin'
        NinjaDir = Join-Path $VcpkgRoot 'downloads\tools\ninja-1.13.2-windows'
        EmsdkEnv = Join-Path $EmsdkRoot 'emsdk_env.ps1'
    }
}

function Install-WasmDependencies
{
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Paths
    )

    $vcpkgExe = Join-Path $Paths.VcpkgRoot 'vcpkg.exe'
    Ensure-PathExists -Path $vcpkgExe -Message "vcpkg.exe not found at $vcpkgExe"

    $manifestFile = Join-Path $Paths.VcpkgManifestRoot 'vcpkg.json'
    Ensure-PathExists -Path $manifestFile -Message "WASM vcpkg manifest not found at $manifestFile"

    $overlayTriplets = Join-Path $Paths.WasmDir 'triplets'
    $overlayPorts = Join-Path $Paths.WasmDir 'ports'

    Write-Host '=== Installing WASM dependencies ==='
    New-Item -ItemType Directory -Force -Path $Paths.VcpkgInstallRoot | Out-Null

    $arguments = @(
        'install',
        '--x-wait-for-lock',
        "--x-manifest-root=$($Paths.VcpkgManifestRoot)",
        "--x-install-root=$($Paths.VcpkgInstallRoot)",
        '--triplet=wasm32-emscripten',
        "--overlay-triplets=$overlayTriplets"
    )

    if (Test-Path $overlayPorts)
    {
        $arguments += "--overlay-ports=$overlayPorts"
    }

    Invoke-NativeCommand -FilePath $vcpkgExe -ArgumentList $arguments
}

function Add-PathEntry
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathEntry
    )

    if (-not (Test-Path $PathEntry))
    {
        return
    }

    $separator = [IO.Path]::PathSeparator
    $entries = @($env:PATH -split [regex]::Escape($separator))
    if ($entries -notcontains $PathEntry)
    {
        $env:PATH = "$PathEntry$separator$env:PATH"
    }
}

function Set-EmscriptenEnvironment
{
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Paths
    )

    if (-not (Test-Path $Paths.EmsdkEnv))
    {
        throw "emsdk_env.ps1 not found at $($Paths.EmsdkEnv)"
    }

    & $Paths.EmsdkEnv | Out-Null

    $env:EMSDK = $Paths.EmsdkRoot
    $env:EMSCRIPTEN_ROOT = Join-Path $Paths.EmsdkRoot 'upstream\emscripten'
    $env:EM_CONFIG = Join-Path $Paths.EmsdkRoot '.emscripten'

    Add-PathEntry -PathEntry $Paths.CMakeDir
    Add-PathEntry -PathEntry $Paths.NinjaDir
}

function Invoke-ProjectPython
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectDir,

        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList
    )

    $venvPython = Join-Path $ProjectDir '.venv\Scripts\python.exe'
    if (Test-Path $venvPython)
    {
        Invoke-NativeCommand -FilePath $venvPython -ArgumentList $ArgumentList
        return
    }

    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($null -ne $python)
    {
        Invoke-NativeCommand -FilePath $python.Source -ArgumentList $ArgumentList
        return
    }

    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($null -ne $py)
    {
        Invoke-NativeCommand -FilePath $py.Source -ArgumentList (@('-3') + $ArgumentList)
        return
    }

    throw 'Python was not found. Install Python or create .venv before running this script.'
}

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

function Ensure-GameJunction
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$LinkPath,

        [Parameter(Mandatory = $true)]
        [string]$TargetPath
    )

    if (Test-Path $LinkPath)
    {
        return
    }

    Write-Host '=== Creating game junction ==='
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LinkPath) | Out-Null
    Invoke-NativeCommand -FilePath 'cmd.exe' -ArgumentList @('/c', 'mklink', '/J', $LinkPath, $TargetPath)
}

function Invoke-WasmConfigureBuild
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$WasmDir,

        [Parameter(Mandatory = $true)]
        [string]$BuildDir,

        [Parameter(Mandatory = $true)]
        [string]$DepsDir,

        [string]$BuildType = 'Release'
    )

    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

    Push-Location $BuildDir
    try
    {
        Write-Host '=== Configuring ==='
        Invoke-NativeCommand -FilePath 'emcmake' -ArgumentList @(
            'cmake',
            $WasmDir,
            "-DCMAKE_BUILD_TYPE=$BuildType",
            "-DWASM_DEPS_DIR=$DepsDir",
            "-DCMAKE_PREFIX_PATH=$DepsDir",
            '-G',
            'Ninja'
        )

        Write-Host ''
        Write-Host '=== Building ==='
        Invoke-NativeCommand -FilePath 'emmake' -ArgumentList @('ninja')
    }
    finally
    {
        Pop-Location
    }
}

function Get-WasmMainHtmlFileName
{
    'kyschess.html'
}

function Get-WasmManifestFileName
{
    'kys_manifest.js'
}

function Get-WasmBuildArtifactNames
{
    @(
        (Get-WasmMainHtmlFileName)
        'kyschess.js'
        (Get-WasmManifestFileName)
        'kyschess.wasm'
    )
}

function Get-WasmBuildArtifactPaths
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir
    )

    foreach ($artifactName in Get-WasmBuildArtifactNames)
    {
        Join-Path $BuildDir $artifactName
    }
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
