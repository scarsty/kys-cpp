$ErrorActionPreference = 'Stop'

$installRoot = $PSScriptRoot
$env:KYS_CHESS_MCP_CURRENT = Join-Path $installRoot 'current.json'
$env:KYS_CHESS_MCP_SAVE_DIR = Join-Path $installRoot 'saves'
$python = Join-Path $installRoot 'bridge\Scripts\python.exe'

if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
    throw "找不到 KYS 自走棋 MCP Python 執行期：$python"
}

& $python -m kys_chess_mcp.server
exit $LASTEXITCODE
