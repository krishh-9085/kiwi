@echo off
cd /d "%~dp0"
title Kiwi Robot - React UI Dev Server
echo =======================================================
echo    Starting Kiwi Robot Next-Gen React UI Portal...
echo =======================================================
echo.
if not exist "react_ui\node_modules" (
    echo Installing node dependencies for first time run...
    cd react_ui
    call npm install
    cd ..
)

echo Starting Vite dev server at http://localhost:5173
start http://localhost:5173
cd react_ui
call npm run dev
pause
