@echo off
cd /d "%~dp0"
title Kiwi Robot - Sync UI to C++ Firmware
echo =======================================================
echo    Syncing HTML Templates to C++ Source Files...
echo =======================================================
echo.
if exist "tools\web_dev\sync_to_cpp.py" (
    python tools\web_dev\sync_to_cpp.py
) else if exist "MyRobot_Modular\tools\web_dev\sync_to_cpp.py" (
    cd MyRobot_Modular
    python tools\web_dev\sync_to_cpp.py
) else (
    echo Error: tools\web_dev\sync_to_cpp.py not found!
    pause
    exit /b 1
)
echo.
echo =======================================================
echo    Sync Complete! You can now compile in Arduino IDE.
echo =======================================================
pause
