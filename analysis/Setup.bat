@echo off
setlocal
title Setup
powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0Setup.GUI.ps1"
if errorlevel 1 pause
endlocal
