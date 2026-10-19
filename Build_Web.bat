@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0app\tools\build-browser.ps1" %*
if errorlevel 1 pause
