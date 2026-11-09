@echo off
chcp 65001 >nul
powershell -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0tools\open_client_admin.ps1"
pause
