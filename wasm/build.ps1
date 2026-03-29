[CmdletBinding()]
param(
    [string]$DepsDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'WasmCommon.ps1')

$paths = Get-WasmPaths -WasmDir $PSScriptRoot
Set-EmscriptenEnvironment -Paths $paths

if ([string]::IsNullOrWhiteSpace($DepsDir))
{
    $localDeps = Join-Path $PSScriptRoot 'wasm_deps'
    if (Test-Path (Join-Path $localDeps 'lib'))
    {
        $DepsDir = $localDeps
    }
    else
    {
        $DepsDir = $paths.VcpkgWasm
    }
}

Ensure-PathExists -Path (Join-Path $DepsDir 'lib') -Message "Dependencies not found at $DepsDir. Run rebuild.ps1 for the vcpkg flow or populate wasm_deps first."

Invoke-WasmConfigureBuild -WasmDir $paths.WasmDir -BuildDir $paths.BuildDir -DepsDir $DepsDir

Write-Host ''
Write-Host '=== Build complete ==='
Write-Host "Output: $(Join-Path $paths.BuildDir (Get-WasmMainHtmlFileName))"
Write-Host ''
Write-Host 'To test locally:'
Write-Host "  Set-Location '$($paths.BuildDir)'"
Write-Host '  python -m http.server 8080'
Write-Host "  Open http://localhost:8080/$(Get-WasmMainHtmlFileName)"