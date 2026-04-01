# copy_assets.ps1 — Copy game assets into Android build tree
# Usage: .\copy_assets.ps1 [-GameDir <path>]
# Defaults to ..\work\game-dev

param(
    [string]$GameDir = (Join-Path $PSScriptRoot "..\work\game-dev")
)

$Dest = Join-Path $PSScriptRoot "app\src\main\assets\game"

if (-not (Test-Path $GameDir)) {
    Write-Error "Game directory not found: $GameDir"
    exit 1
}

Write-Host "Copying assets from $GameDir → $Dest"
robocopy $GameDir $Dest /MIR /XD save /NFL /NDL /NJH /NJS /NC /NS /NP
# robocopy exit codes 0-7 are success
if ($LASTEXITCODE -gt 7) {
    Write-Error "robocopy failed with exit code $LASTEXITCODE"
    exit 1
}

# Populate save/ with required game data and clean user saves (mirrors package_release.ps1)
$SaveDir = Join-Path $Dest "save"
$SrcSaveDir = Join-Path $GameDir "save"
if (-not (Test-Path $SaveDir)) {
    New-Item -ItemType Directory -Path $SaveDir -Force | Out-Null
}
Remove-Item "$SaveDir\*.json" -Force -ErrorAction SilentlyContinue
# game.db (shared game database — required for all save/load)
Copy-Item (Join-Path $SrcSaveDir "game.db") "$SaveDir\" -Force -ErrorAction SilentlyContinue
# 0.json (new-game save slot)
Copy-Item (Join-Path $SrcSaveDir "0.json") "$SaveDir\" -Force -ErrorAction SilentlyContinue
# .grp.zip archives (allsin/alldef data)
Copy-Item (Join-Path $SrcSaveDir "*.grp.zip") "$SaveDir\" -Force -ErrorAction SilentlyContinue
$settingJson = @"
{
   "positionSwapEnabled": false,
   "musicVolume": 31,
   "soundVolume": 14,
   "manualCamera": false,
   "battleSpeed": 1,
   "simplifiedChinese": false,
   "showBattleLog": false
}
"@
[System.IO.File]::WriteAllText("$SaveDir\setting.json", $settingJson, [System.Text.UTF8Encoding]::new($false))

$LASTEXITCODE = 0
Write-Host "Assets copied successfully."
