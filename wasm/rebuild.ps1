[CmdletBinding()]
param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$EmsdkRoot = $env:EMSDK,
    [string]$VcpkgWasm
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'WasmCommon.ps1')

$paths = Get-WasmPaths -WasmDir $PSScriptRoot -VcpkgRoot $VcpkgRoot -EmsdkRoot $EmsdkRoot -VcpkgWasm $VcpkgWasm
Set-EmscriptenEnvironment -Paths $paths

$imguiLibrary = Join-Path $paths.VcpkgWasm 'lib\libimgui.a'

Install-WasmDependencies -Paths $paths
Ensure-PathExists -Path $imguiLibrary -Message "WASM dependency install did not produce $imguiLibrary"

$gameTarget = Join-Path $paths.ProjectDir 'work\game-dev'
Ensure-GameJunction -LinkPath (Join-Path $paths.BuildDir 'kys\game') -TargetPath $gameTarget

Write-Host '=== Generating manifest ==='
Invoke-ProjectPython -ProjectDir $paths.ProjectDir -ArgumentList @(
    (Join-Path $paths.WasmDir 'gen_manifest.py'),
    $gameTarget,
    (Join-Path $paths.BuildDir (Get-WasmManifestFileName))
)

Invoke-WasmConfigureBuild -WasmDir $paths.WasmDir -BuildDir $paths.BuildDir -DepsDir $paths.VcpkgWasm

Copy-Item -Force (Join-Path $paths.WasmDir 'serve.py') (Join-Path $paths.BuildDir 'serve.py')

$indexPath = Join-Path $paths.BuildDir 'index.html'
$mainHtmlFileName = Get-WasmMainHtmlFileName
Set-Content -Path $indexPath -NoNewline -Value @"
<!DOCTYPE html>
<html>
<head><meta http-equiv="refresh" content="0;url=$mainHtmlFileName"></head>
<body><a href="$mainHtmlFileName">kyschess</a></body>
</html>
"@

$headersPath = Join-Path $paths.BuildDir '_headers'
Set-Content -Path $headersPath -NoNewline -Value @'
/*
  Cache-Control: public, max-age=3600
'@
