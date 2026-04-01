$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sdkRoot = 'D:\Android'
$javaHome = 'C:\Program Files\Amazon Corretto\jdk17.0.18_9'
$avdName = 'kys_api35'
$packageName = 'com.kysgame.kyschess'
$apkPath = Join-Path $repoRoot 'android\app\build\outputs\apk\release\app-release.apk'
$adb = Join-Path $sdkRoot 'platform-tools\adb.exe'
$emulator = Join-Path $sdkRoot 'emulator\emulator.exe'

$env:JAVA_HOME = $javaHome
$env:PATH = "$env:JAVA_HOME\bin;" + $env:PATH

try {
    $deviceState = & $adb get-state 2>$null
} catch {
    $deviceState = ''
}

if ($deviceState -ne 'device') {
    Start-Process -FilePath $emulator -ArgumentList @('-avd', $avdName, '-no-snapshot', '-netdelay', 'none', '-netspeed', 'full') | Out-Null

    for ($i = 1; $i -le 60; $i++) {
        $state = & $adb get-state 2>$null
        $boot = & $adb shell getprop sys.boot_completed 2>$null
        if ($state -eq 'device' -and $boot -eq '1') {
            break
        }
        Start-Sleep -Seconds 5
    }
}

Push-Location (Join-Path $repoRoot 'android')
try {
    .\gradlew.bat assembleRelease
} finally {
    Pop-Location
}

if (-not (Test-Path $apkPath)) {
    throw "APK not found: $apkPath"
}

& $adb install -r $apkPath
& $adb shell monkey -p $packageName -c android.intent.category.LAUNCHER 1