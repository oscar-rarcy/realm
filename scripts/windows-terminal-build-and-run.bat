@echo off
setlocal

REM Realm Windows terminal build/run script.
REM Run it from PowerShell, Command Prompt, or double-click it.
REM The terminal renderer is built and run inside WSL.
REM Pass "clean" or "--clean" to remove build outputs before rebuilding.

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO=%%~fI"

REM Keep the log outside build/, because optional clean builds delete build/.
set "LOG_DIR=%REPO%\logs"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
set "LOG=%LOG_DIR%\windows-terminal-build.log"

set "CLEAN=0"
:parse_args
if "%~1"=="" goto after_args
if /I "%~1"=="clean" set "CLEAN=1"
if /I "%~1"=="--clean" set "CLEAN=1"
shift
goto parse_args
:after_args

echo Realm Windows terminal build
echo Repo: %REPO%
echo Log: %LOG%
if "%CLEAN%"=="1" echo Build mode: clean
if not "%CLEAN%"=="1" echo Build mode: incremental
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

if "%CLEAN%"=="1" (
    echo Cleaning and building terminal target in WSL...
    set "BUILD_CMD=make clean && make terminal"
) else (
    echo Building terminal target in WSL...
    set "BUILD_CMD=make terminal"
)
echo.

wsl.exe --cd "%REPO%" bash -lc "%BUILD_CMD% && test -x bin/realm" > "%LOG%" 2>&1

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
