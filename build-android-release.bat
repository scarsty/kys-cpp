@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ============================================================
:: 资源目录（修改此处为本地 game 资源根目录）
set RESOURCE_DIR=D:\kys-all\_cpp\cpp版资源\game2x
:: ============================================================

set PROJECT_ROOT=%~dp0
set ANDROID_DIR=%PROJECT_ROOT%kys-cpp-androidstudio
set ASSETS_DIR=%ANDROID_DIR%\app\src\main\assets
set GAME_ZIP=%ASSETS_DIR%\game.zip
set OUTPUT_APK=%ANDROID_DIR%\app\build\outputs\apk\release\kys-cpp-release.apk
set SDK_DIR=C:\Users\%USERNAME%\AppData\Local\Android\Sdk
set ADB=%SDK_DIR%\platform-tools\adb.exe
set SEVENZIP=C:\Program Files\7-Zip\7z.exe
set APP_PACKAGE=org.libsdl.kys_cpp
set APP_ACTIVITY=.SDLActivity

echo ============================================================
echo kys-cpp Android Release Build
echo 资源目录: %RESOURCE_DIR%
echo ============================================================

:: 检查资源目录
if not exist "%RESOURCE_DIR%\" (
    echo [错误] 资源目录不存在: %RESOURCE_DIR%
    exit /b 1
)

:: 确保 assets 目录存在
if not exist "%ASSETS_DIR%\" mkdir "%ASSETS_DIR%"

:: ---- 步骤 1: 打包资源为 game.zip ----
echo.
echo [1/3] 正在打包资源为 game.zip...
if exist "%GAME_ZIP%" del /f /q "%GAME_ZIP%"

if exist "%SEVENZIP%" (
    echo 使用 7-Zip 压缩...
    "%SEVENZIP%" a -tzip -mx=1 "%GAME_ZIP%" "%RESOURCE_DIR%\*" -r
    if !ERRORLEVEL! NEQ 0 (
        echo [错误] 7-Zip 压缩失败
        exit /b 1
    )
) else (
    echo 使用 PowerShell Compress-Archive 压缩...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$src = '%RESOURCE_DIR%'; $dst = '%GAME_ZIP%'; Compress-Archive -Path \"$src\*\" -DestinationPath $dst -Force"
    if !ERRORLEVEL! NEQ 0 (
        echo [错误] PowerShell 压缩失败
        exit /b 1
    )
)

if not exist "%GAME_ZIP%" (
    echo [错误] game.zip 未生成
    exit /b 1
)
echo game.zip 生成完成。

:: ---- 步骤 2: Gradle assembleRelease ----
echo.
echo [2/3] 正在编译 so 并生成 Release APK...
cd /d "%ANDROID_DIR%"
call gradlew.bat assembleRelease
if %ERRORLEVEL% NEQ 0 (
    echo [错误] Gradle 构建失败，请检查日志
    exit /b 1
)

if not exist "%OUTPUT_APK%" (
    echo [错误] APK 未找到: %OUTPUT_APK%
    exit /b 1
)

echo.
echo ============================================================
echo 构建成功！
echo APK: %OUTPUT_APK%
echo ============================================================

:: ---- 步骤 3: 检测实体机并安装运行 ----
echo.
echo [3/3] 检测 Android 设备...

if not exist "%ADB%" (
    echo adb 未找到（%ADB%），跳过安装。
    goto :done
)

:: 启动 adb server
"%ADB%" start-server >nul 2>nul

:: 获取已连接的实体机（过滤掉模拟器和空行）
set DEVICE=
"%ADB%" devices > "%TEMP%\adb_devices.txt" 2>nul
for /f "tokens=1" %%D in ('findstr /v /i "List emulator" "%TEMP%\adb_devices.txt" ^| findstr "device"') do (
    if not defined DEVICE set DEVICE=%%D
)

if not defined DEVICE (
    echo 未检测到实体设备，跳过安装。
    goto :done
)

echo 检测到设备: %DEVICE%
echo 正在安装 APK...
"%ADB%" -s %DEVICE% install -r "%OUTPUT_APK%"
if %ERRORLEVEL% NEQ 0 (
    echo [警告] APK 安装失败
    goto :done
)
echo 安装成功，正在启动应用...
"%ADB%" -s %DEVICE% shell am start -n "%APP_PACKAGE%/%APP_PACKAGE%%APP_ACTIVITY%"
if %ERRORLEVEL% NEQ 0 (
    echo [警告] 启动应用失败
)

:done
echo.
echo 完成。
