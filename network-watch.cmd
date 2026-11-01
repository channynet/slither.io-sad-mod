@echo off
cd /d "%~dp0"
where python >nul 2>nul && (python tools\network_watch.py) || (py -3 tools\network_watch.py)
pause
