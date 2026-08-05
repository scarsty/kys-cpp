@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

set PACKAGE_RESOURCES=1
if /i "%~1"=="no-assets" set PACKAGE_RESOURCES=0
if not "%~1"=="" if /i not "%~1"=="no-assets" (
    echo 用法: %~nx0 [no-assets]
    exit /b 1
)

:: ============================================================
:: 资源目录（修改此处为本地 game 资源根目录）
set RESOURCE_DIR=D:\kys-all\_cpp\cpp版资源\game-dragon
:: ============================================================

set ANDROID_DIR=%~dp0
set PROJECT_ROOT=%ANDROID_DIR%..
set ASSETS_DIR=%ANDROID_DIR%app\src\main\assets
set GAME_ZIP=%ASSETS_DIR%\game.zip
set ZIP_SCRIPT=%PROJECT_ROOT%\build-android-game-zip.ps1
set OUTPUT_APK=%ANDROID_DIR%app\build\outputs\apk\release\kys-cpp-release.apk
set ROOT_APK=%ANDROID_DIR%kys-cpp-release.apk
if %PACKAGE_RESOURCES% EQU 0 set ROOT_APK=%ANDROID_DIR%kys-cpp-release-no-assets.apk
set OUTPUT_SO=%ANDROID_DIR%app\build\intermediates\stripped_native_libs\release\stripReleaseDebugSymbols\out\lib\arm64-v8a\libkys_cpp.so
set EXPORTED_SO=%ANDROID_DIR%libkys_cpp.so
set SDK_DIR=C:\Users\%USERNAME%\AppData\Local\Android\Sdk
set ADB=%SDK_DIR%\platform-tools\adb.exe
set APP_PACKAGE=org.libsdl.kys_cpp
set APP_ACTIVITY=org.libsdl.app.SDLActivity

echo ============================================================
echo kys-cpp Android Release Build
if %PACKAGE_RESOURCES% EQU 1 echo 资源目录: %RESOURCE_DIR%
if %PACKAGE_RESOURCES% EQU 0 echo 资源: 不打包
echo ============================================================

if %PACKAGE_RESOURCES% EQU 1 (
    if not exist "%RESOURCE_DIR%\" (
        echo [错误] 资源目录不存在: %RESOURCE_DIR%
        exit /b 1
    )
)

if not exist "%ASSETS_DIR%\" mkdir "%ASSETS_DIR%"

echo.
echo [0/5] 同步 so 库文件...
set VCPKG_LIB=%VCPKG_ROOT%\installed\arm64-android-dynamic\lib
set JNI_LIB=%ANDROID_DIR%app\lib\arm64-v8a
for %%F in (libSDL3_image.so libwebp.so libwebpdecoder.so libwebpdemux.so libwebpmux.so libpng16.so libsharpyuv.so) do (
    if exist "%VCPKG_LIB%\%%F" (
        copy /y "%VCPKG_LIB%\%%F" "%JNI_LIB%\%%F" >nul
        echo   已同步: %%F
    )
)

echo.
echo [1/5] 正在处理资源...
if exist "%GAME_ZIP%" del /f /q "%GAME_ZIP%"

if %PACKAGE_RESOURCES% EQU 1 (
    echo 使用 PowerShell ZipArchive 压缩...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ZIP_SCRIPT%" -SourceDir "%RESOURCE_DIR%" -Destination "%GAME_ZIP%"
    if !ERRORLEVEL! NEQ 0 (
        echo [错误] PowerShell 压缩失败
        exit /b 1
    )

    if not exist "%GAME_ZIP%" (
        echo [错误] game.zip 未生成
        exit /b 1
    )
    echo game.zip 生成完成。
)
if %PACKAGE_RESOURCES% EQU 0 echo 已移除旧 game.zip，APK 不包含游戏资源。

echo.
echo [2/5] 正在编译 so 并生成 Release APK...
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
if not exist "%OUTPUT_SO%" (
    echo [错误] Release so 未找到: %OUTPUT_SO%
    exit /b 1
)

echo.
echo [3/5] 正在导出 libkys_cpp.so...
copy /y "%OUTPUT_SO%" "%EXPORTED_SO%" >nul
if %ERRORLEVEL% NEQ 0 (
    echo [错误] 复制 Release so 文件失败。
    exit /b 1
)

echo.
echo [4/5] 正在移动 APK 到脚本目录...
move /y "%OUTPUT_APK%" "%ROOT_APK%" >nul
if %ERRORLEVEL% NEQ 0 (
    echo [错误] APK 移动失败: %ROOT_APK%
    exit /b 1
)

echo.
echo ============================================================
echo 构建成功！
echo APK: %ROOT_APK%
echo so: %EXPORTED_SO%
echo ============================================================

echo.
echo [5/5] 检测 Android 设备...

if not exist "%ADB%" (
    echo adb 未找到（%ADB%），跳过安装。
    goto :done
)

"%ADB%" start-server >nul 2>nul

set DEVICE=
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "& '%ADB%' devices | Select-String '\tdevice$' | ForEach-Object { $_.ToString().Split([char]9)[0] } | Select-Object -First 1"`) do (
    set DEVICE=%%D
)

if not defined DEVICE (
    echo 未检测到实体设备，跳过安装。
    goto :done
)

echo 检测到设备: %DEVICE%
echo 正在安装 APK...
"%ADB%" -s %DEVICE% install -r "%ROOT_APK%"
if %ERRORLEVEL% NEQ 0 (
    echo [警告] APK 安装失败
    goto :done
)
echo 安装成功，正在启动应用...
"%ADB%" -s %DEVICE% shell am start -n "%APP_PACKAGE%/%APP_ACTIVITY%"
if %ERRORLEVEL% NEQ 0 echo [警告] 启动应用失败

:done
echo.
echo 完成。