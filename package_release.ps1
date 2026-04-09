param(
    [string]$PkgDir = "release_package",
    [string]$GameDir = (Join-Path $PSScriptRoot 'work\game-dev'),
    [string]$Version,
    [string]$ZipPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'tools\ReleasePackagingCommon.ps1')

Write-Host "========================================"
Write-Host "Building Release Package"
Write-Host "Target: $PkgDir"
Write-Host "========================================"

# Step 1: Build MSVC Release
Write-Host "[1/6] Building MSVC Release..."
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "Visual Studio not found!"
    exit 1
}

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
$dumpbin = & $vswhere -latest -find "VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe" | Select-Object -First 1

if (-not $msbuild) {
    Write-Error "MSBuild not found!"
    exit 1
}
if (-not $dumpbin) {
    Write-Error "dumpbin not found!"
    exit 1
}

& $msbuild kys.sln /p:Configuration=Release /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit 1
}

# Step 2: Create package structure
Write-Host "[2/6] Creating package structure..."
if (Test-Path $PkgDir) {
    Remove-Item -Recurse -Force $PkgDir
}
New-Item -ItemType Directory -Path "$PkgDir\bin" -Force | Out-Null

# Step 3: Copy exe and DLLs
Write-Host "[3/6] Copying executable and dependencies..."
Copy-Item "x64\Release\kys.exe" "$PkgDir\bin\" -Force

# Copy direct dependencies from the workspace vcpkg manifest tree
$vcpkgBin = Join-Path $PSScriptRoot 'vcpkg_installed\x64-windows\bin'
if (Test-Path $vcpkgBin) {
    $processedDlls = @{}
    $scannedFiles = @{}
    $toScan = @("$PkgDir\bin\kys.exe")
    $maxIterations = 5000
    $iteration = 0

    while ($toScan.Count -gt 0 -and $iteration -lt $maxIterations) {
        $iteration++
        $currentFile = $toScan[0]
        if ($toScan.Count -eq 1) {
            $toScan = @()
        } else {
            $toScan = $toScan[1..($toScan.Count-1)]
        }

        if ($scannedFiles.ContainsKey($currentFile)) { continue }
        $scannedFiles[$currentFile] = $true

        $dumpOutput = & $dumpbin /dependents $currentFile 2>$null
        $dlls = $dumpOutput | Where-Object { $_ -match '^\s+\S+\.dll$' } | ForEach-Object { $_.Trim() }

        foreach ($dll in $dlls) {
            if ($processedDlls.ContainsKey($dll)) { continue }
            $processedDlls[$dll] = $true

            if ($dll -match '^(KERNEL32|USER32|WS2_32|ADVAPI32|SHELL32|ole32|OLEAUT32|GDI32|msvcrt|VCRUNTIME|ucrtbase|api-ms-)') {
                continue
            }

            $sourcePath = Join-Path $vcpkgBin $dll
            if (Test-Path $sourcePath) {
                $targetPath = "$PkgDir\bin\$dll"
                Copy-Item $sourcePath $targetPath -Force
                $toScan += $targetPath
            }
        }
    }
}

# Copy DLLs from local directory (smallpot)
$localDlls = @("smallpot.dll")
foreach ($dll in $localDlls) {
    $sourcePath = "work\$dll"
    if (Test-Path $sourcePath) {
        Copy-Item $sourcePath "$PkgDir\bin\" -Force
    }
}

# Step 4: Copy resources
Write-Host "[4/6] Copying resources..."
Copy-ReleaseGameAssets -SourceGameDir $GameDir -DestinationGameDir "$PkgDir\game" -Version $Version

# Step 5: Copy changelog and create play.bat
Write-Host "[6/6] Finalizing package..."
$changelog = Get-ChildItem "docs\*.md" | Where-Object { $_.Name -match '\u66f4\u65b0\u65e5\u5fd7' } | Select-Object -First 1
if ($changelog) {
    Copy-Item $changelog.FullName "$PkgDir\" -Force
}

@"
@echo off
cd /d "%~dp0"
start bin\kys.exe game
"@ | Out-File -FilePath "$PkgDir\play.bat" -Encoding ASCII

Write-Host "========================================"
Write-Host "Package created in: $PkgDir"
Write-Host "========================================"

if (-not [string]::IsNullOrWhiteSpace($ZipPath)) {
    Write-Host "[7/7] Creating Windows zip..."
    New-ZipFromDirectory -SourceDir $PkgDir -ZipPath $ZipPath
    $zipInfo = Get-Item $ZipPath
    Write-Host "Windows zip created: $ZipPath ($(Format-FileSize -Bytes $zipInfo.Length))"
}

