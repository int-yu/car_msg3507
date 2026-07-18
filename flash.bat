@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo ========================================
echo   烧录 MSPM0G3507 (pyOCD)
echo ========================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash.ps1" %*
set "FLASH_EXIT_CODE=%ERRORLEVEL%"
echo.
echo ========================================
pause
exit /b %FLASH_EXIT_CODE%
