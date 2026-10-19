@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0app\tools\start-browser.ps1" %*
if errorlevel 1 pause
