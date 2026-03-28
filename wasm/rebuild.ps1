[CmdletBinding()]
param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$EmsdkRoot = $env:EMSDK,
    [string]$VcpkgWasm = $env:VCPKG_WASM
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'WasmCommon.ps1')

$paths = Get-WasmPaths -WasmDir $PSScriptRoot -VcpkgRoot $VcpkgRoot -EmsdkRoot $EmsdkRoot -VcpkgWasm $VcpkgWasm
Set-EmscriptenEnvironment -Paths $paths

$imguiLibrary = Join-Path $paths.VcpkgWasm 'lib\libimgui.a'
$overlayTriplets = Join-Path $paths.WasmDir 'triplets'
$overlayPorts = Join-Path $paths.WasmDir 'ports'
$installCommand = @(
    "`"$($paths.VcpkgRoot)\vcpkg.exe`" install",
    "--overlay-triplets=`"$overlayTriplets`"",
    "--overlay-ports=`"$overlayPorts`"",
    'imgui[sdl3-binding,sdl3-renderer-binding]:wasm32-emscripten',
    'sqlite3:wasm32-emscripten yaml-cpp:wasm32-emscripten',
    'libzip[bzip2]:wasm32-emscripten marisa-trie:wasm32-emscripten',
    'sdl3-image[png,webp]:wasm32-emscripten sdl3-ttf[svg]:wasm32-emscripten',
    'sdl3-mixer[fluidsynth]:wasm32-emscripten'
) -join ' '
Ensure-PathExists -Path $imguiLibrary -Message "WASM deps not installed. Run from vcpkg root:`n$installCommand"

$gameTarget = Join-Path $paths.ProjectDir 'work\game-dev'
Ensure-GameJunction -LinkPath (Join-Path $paths.BuildDir 'kys\game') -TargetPath $gameTarget

Write-Host '=== Generating manifest ==='
Invoke-ProjectPython -ProjectDir $paths.ProjectDir -ArgumentList @(
    (Join-Path $paths.WasmDir 'gen_manifest.py'),
    $gameTarget,
    (Join-Path $paths.BuildDir 'kys_manifest.js')
)

Invoke-WasmConfigureBuild -WasmDir $paths.WasmDir -BuildDir $paths.BuildDir -DepsDir $paths.VcpkgWasm

Copy-Item -Force (Join-Path $paths.WasmDir 'serve.py') (Join-Path $paths.BuildDir 'serve.py')

$indexPath = Join-Path $paths.BuildDir 'index.html'
Set-Content -Path $indexPath -NoNewline -Value @'
<!DOCTYPE html>
<html>
<head><meta http-equiv="refresh" content="0;url=kyschess.html"></head>
<body><a href="kyschess.html">kyschess</a></body>
</html>
'@

$headersPath = Join-Path $paths.BuildDir '_headers'
Set-Content -Path $headersPath -NoNewline -Value @'
/*
  Cache-Control: public, max-age=3600
'@