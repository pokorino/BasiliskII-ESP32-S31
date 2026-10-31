@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash.ps1"
set "flash_exit=%errorlevel%"
echo.
pause
exit /b %flash_exit%
