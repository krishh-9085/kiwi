@echo off
cd /d "%~dp0"
title Kiwi Robot - Sync React UI to C++ Firmware
echo =======================================================
echo    Building and Syncing React UI to C++ Firmware...
echo =======================================================
echo.
python tools\web_dev\sync_react_to_cpp.py
echo.
echo =======================================================
echo    Sync Complete! You can now compile and flash.
echo =======================================================
pause
