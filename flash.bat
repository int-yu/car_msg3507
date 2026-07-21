@echo off
setlocal
cd /d "%~dp0"
echo ========================================
echo   Flash MSPM0G3507 via pyOCD
echo ========================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash.ps1" %*
set "FLASH_EXIT_CODE=%ERRORLEVEL%"
echo(
echo ========================================
pause
exit /b %FLASH_EXIT_CODE%
