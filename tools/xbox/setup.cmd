@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1"
if errorlevel 1 echo Xbox setup failed. Read the error above and check that the dashboard opens.
pause
