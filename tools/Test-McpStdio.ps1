param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\x64\Release\kys.exe'),
    [string]$GameDirectory = (Join-Path $PSScriptRoot '..\work\game-dev')
)
$ErrorActionPreference = 'Stop'
$start = [System.Diagnostics.ProcessStartInfo]::new()
$start.FileName = [System.IO.Path]::GetFullPath($Executable)
$start.Arguments = '--mcp-stdio "' + [System.IO.Path]::GetFullPath($GameDirectory) + '"'
$start.UseShellExecute = $false
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $start
function Assert($Condition, $Message) {
    if (-not $Condition) { throw $Message }
}
function Read-Response {
    $read = $process.StandardOutput.ReadLineAsync()
    if (-not $read.Wait(30000)) { throw 'Timed out waiting for MCP response' }
    Assert ($null -ne $read.Result) 'Unexpected EOF on protocol stdout'
    try { return ($read.Result | ConvertFrom-Json) }
    catch { throw "Non-JSON stdout: $($read.Result)" }
}
function Send-Request($Id, $Method, $Parameters) {
    $request = @{ jsonrpc = '2.0'; id = $Id; method = $Method; params = $Parameters }
    $process.StandardInput.WriteLine(($request | ConvertTo-Json -Depth 20 -Compress))
    $process.StandardInput.Flush()
    $response = Read-Response
    Assert ($response.id -eq $Id) "Incorrect response ID for $Method"
    return $response
}
try {
    Assert ($process.Start()) 'Failed to start game'
    $logs = $process.StandardError.ReadToEndAsync()
    $response = Send-Request 'init' 'initialize' @{
        protocolVersion = '2025-06-18'; capabilities = @{}; clientInfo = @{ name = 'stdio-smoke'; version = '1' }
    }
    Assert ($response.result.serverInfo.name -eq 'kys-cpp') 'Initialization failed'
    $process.StandardInput.WriteLine('{"jsonrpc":"2.0","method":"notifications/initialized"}')
    $response = Send-Request 2 'tools/list' @{}
    Assert ($response.result.tools.Count -eq 7) 'Unexpected tools list'
    $response = Send-Request 3 'tools/call' @{ name = 'get_game_state'; arguments = @{} }
    $state = $response.result.content[0].text | ConvertFrom-Json
    Assert ($state.context -eq 'menu') 'Title menu is not the active input context'
    foreach ($name in @('get_ui_state', 'get_party', 'get_inventory')) {
        $response = Send-Request $name 'tools/call' @{ name = $name; arguments = @{} }
        Assert (-not $response.result.isError) "$name failed"
        $null = $response.result.content[0].text | ConvertFrom-Json
    }
    Start-Sleep -Milliseconds 300
    $response = Send-Request 4 'tools/call' @{ name = 'press_key'; arguments = @{ key = 'right' } }
    Assert (-not $response.result.isError) 'Key press rejected'
    Start-Sleep -Milliseconds 100
    $response = Send-Request 5 'tools/call' @{ name = 'get_ui_state'; arguments = @{} }
    $ui = $response.result.content[0].text | ConvertFrom-Json
    Assert ($ui.selected -eq 1) "Right key did not select the next menu entry: $($response.result.content[0].text)"
    $null = Send-Request 6 'tools/call' @{ name = 'press_key'; arguments = @{ key = 'left' } }
    Start-Sleep -Milliseconds 100
    $response = Send-Request 7 'tools/call' @{ name = 'capture_screen'; arguments = @{} }
    Assert ($response.result.content[0].mimeType -eq 'image/png') 'Screenshot missing'
    $png = [Convert]::FromBase64String($response.result.content[0].data)
    Assert (($png[0..7] -join ',') -eq '137,80,78,71,13,10,26,10') 'Invalid PNG screenshot'
    $response = Send-Request 'click-ui' 'tools/call' @{ name = 'get_ui_state'; arguments = @{} }
    $ui = $response.result.content[0].text | ConvertFrom-Json
    $bounds = $ui.children[1].bounds
    $response = Send-Request 'load-click' 'tools/call' @{ name = 'click_mouse'; arguments = @{
        x = [int]($bounds.x + $bounds.width / 2); y = [int]($bounds.y + $bounds.height / 2); button = 'left'
    } }
    Assert (-not $response.result.isError) 'Left mouse click rejected'
    Start-Sleep -Milliseconds 200
    $response = Send-Request 'load-ui' 'tools/call' @{ name = 'get_ui_state'; arguments = @{} }
    $ui = $response.result.content[0].text | ConvertFrom-Json
    Assert ($ui.type -eq 'menu' -and $ui.children.Count -gt 3) 'Nested load menu unavailable'
    $response = Send-Request 'load-cancel' 'tools/call' @{ name = 'click_mouse'; arguments = @{ x = 10; y = 10; button = 'right' } }
    Assert (-not $response.result.isError) 'Right mouse click rejected'
    Start-Sleep -Milliseconds 200
    $response = Send-Request 'title-ui' 'tools/call' @{ name = 'get_ui_state'; arguments = @{} }
    $ui = $response.result.content[0].text | ConvertFrom-Json
    Assert ($ui.type -eq 'menu' -and $ui.children.Count -eq 3) 'Cancel did not return to title menu'
    $response = Send-Request 8 'unknown_method' @{}
    Assert ($response.error.code -eq -32601) 'Missing method-not-found error'
    $response = Send-Request 9 'tools/call' @{ name = 'press_key'; arguments = @{ key = 'invalid' } }
    Assert ($response.error.code -eq -32602) 'Invalid key accepted'
    $response = Send-Request 10 'tools/call' @{ name = 'get_party'; arguments = @{ unexpected = 1 } }
    Assert ($response.error.code -eq -32602) 'Unexpected arguments accepted'
    foreach ($arguments in @(
        @{ x = -1; y = 0; button = 'left' },
        @{ x = $state.ui_width; y = 0; button = 'left' },
        @{ x = 0; y = $state.ui_height; button = 'left' },
        @{ x = 1.5; y = 0; button = 'left' },
        @{ x = '1'; y = 0; button = 'left' },
        @{ x = 0; y = 0; button = 'middle' },
        @{ x = 0; y = 0 }
    )) {
        $response = Send-Request 'invalid-click' 'tools/call' @{ name = 'click_mouse'; arguments = $arguments }
        Assert ($response.error.code -eq -32602) 'Invalid mouse arguments accepted'
    }
    $process.StandardInput.WriteLine('not json')
    $response = Read-Response
    Assert ($response.error.code -eq -32700) 'Missing parse error'
    $process.StandardInput.WriteLine(('x' * 65537))
    $response = Read-Response
    Assert ($response.error.code -eq -32700) 'Oversized request accepted'
    $response = Send-Request 11 'ping' @{}
    Assert ($null -ne $response.result) 'Session did not recover from malformed messages'
    $process.StandardInput.Close()
    Assert ($process.WaitForExit(10000)) 'Game did not exit after stdin closed'
    Assert ($process.ExitCode -eq 0) "Game exited with $($process.ExitCode)"
    Assert ($logs.Result.Contains('Game path is')) 'Game logs were not routed to stderr'
    Write-Output 'PASS: handshake, tools, state/UI/party/inventory, left/right mouse clicks, coordinate validation, nested menu input, PNG capture, recovery, stderr separation and EOF shutdown.'
}
finally {
    if ($process.Id -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    $process.Dispose()
}
