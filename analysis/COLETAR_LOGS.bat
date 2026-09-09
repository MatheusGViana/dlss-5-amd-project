@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0COLETAR_LOGS.ps1" -GameDir "%~dp0"
if errorlevel 1 (pause & exit /b 1)
echo Send OptiScaler-Diagnostics.txt for support.
pause
