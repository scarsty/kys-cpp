@echo off
chcp 65001 >nul

call "%~dp0build-android-release.bat" no-assets
exit /b %ERRORLEVEL%