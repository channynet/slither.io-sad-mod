@echo off
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy.ps1"
set "DEPLOY_RESULT=%ERRORLEVEL%"
pause
exit /b %DEPLOY_RESULT%
