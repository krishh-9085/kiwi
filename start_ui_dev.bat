@echo off
cd /d "%~dp0"
title Kiwi Robot - Local UI Dev Server
echo =======================================================
echo    Starting Kiwi Robot Local UI Development Server...
echo =======================================================
echo.
if exist "tools\web_dev\server.py" (
    python tools\web_dev\server.py
) else if exist "MyRobot_Modular\tools\web_dev\server.py" (
    cd MyRobot_Modular
    python tools\web_dev\server.py
) else (
    echo Error: tools\web_dev\server.py not found!
    pause
    exit /b 1
)
pause
