@echo off
setlocal

REM Realm Windows terminal build/run script.
REM Run it from PowerShell, Command Prompt, or double-click it.
REM The terminal renderer is built and run inside WSL.

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO=%%~fI"

REM Keep the log outside build/, because `make clean` deletes build/.
set "LOG=%REPO%\windows-terminal-build-log.txt"

echo Realm Windows terminal build
echo Repo: %REPO%
echo Log: %LOG%
echo.

where wsl.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: WSL was not found.
    echo.
    echo Install WSL, then run this script again:
    echo   wsl --install
    echo.
    pause
    exit /b 1
)

cd /d "%REPO%"

echo Cleaning and building terminal target in WSL...
echo.

wsl.exe --cd "%REPO%" bash -lc "make clean && make terminal && test -x bin/realm" > "%LOG%" 2>&1

set "EXITCODE=%ERRORLEVEL%"

echo.
echo Finished build with exit code: %EXITCODE%
echo.

if not "%EXITCODE%"=="0" (
    echo Build failed. Log output:
    echo ----------------------------------------
    type "%LOG%"
    echo ----------------------------------------
    echo.
    echo If WSL is missing build dependencies, open WSL in this repo and run:
    echo   sudo apt update ^&^& sudo apt install -y build-essential pkg-config libncurses-dev
    echo.
    pause
    exit /b %EXITCODE%
)

echo Build succeeded.
echo Starting Realm terminal renderer in WSL...
echo.

wsl.exe --cd "%REPO%" bash -lc "./bin/realm"

set "EXITCODE=%ERRORLEVEL%"

echo.
echo Realm exited with code: %EXITCODE%
echo Log saved to:
echo %LOG%
echo.

pause
exit /b %EXITCODE%
