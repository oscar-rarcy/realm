@echo off
setlocal

REM Realm Windows GUI build/run script.
REM Run it from PowerShell or double-click it.
REM Pass "clean" or "--clean" to remove build outputs before rebuilding.

set "MSYS2=C:\msys64"
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO=%%~fI"

REM Keep the log outside build/, because optional clean builds delete build/.
set "LOG_DIR=%REPO%\logs"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
set "LOG=%LOG_DIR%\windows-gui-build.log"

set "CLEAN=0"
:parse_args
if "%~1"=="" goto after_args
if /I "%~1"=="clean" set "CLEAN=1"
if /I "%~1"=="--clean" set "CLEAN=1"
shift
goto parse_args
:after_args

echo Realm Windows GUI build
echo Repo: %REPO%
echo MSYS2: %MSYS2%
echo Log: %LOG%
if "%CLEAN%"=="1" echo Build mode: clean
if not "%CLEAN%"=="1" echo Build mode: incremental
echo.

if not exist "%MSYS2%\msys2_shell.cmd" (
    echo ERROR: MSYS2 was not found at:
    echo %MSYS2%
    echo.
    echo Install MSYS2, or edit the MSYS2 path at the top of this file.
    pause
    exit /b 1
)

cd /d "%REPO%"

echo Installing/checking MSYS2 UCRT64 build dependencies...
if "%CLEAN%"=="1" (
    echo Cleaning and building GUI target...
    set "BUILD_CMD=mingw32-make clean && mingw32-make gfx"
) else (
    echo Building GUI target...
    set "BUILD_CMD=mingw32-make gfx"
)
echo.

call "%MSYS2%\msys2_shell.cmd" -ucrt64 -defterm -no-start -where "%REPO%" -c "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf mingw-w64-ucrt-x86_64-libpng && %BUILD_CMD% && test -f bin/realm.exe" > "%LOG%" 2>&1

set "EXITCODE=%ERRORLEVEL%"

echo.
echo Finished build with exit code: %EXITCODE%
echo.

if not "%EXITCODE%"=="0" (
    echo Build failed. Log output:
    echo ----------------------------------------
    type "%LOG%"
    echo ----------------------------------------
    pause
    exit /b %EXITCODE%
)

echo Build succeeded.
echo Starting Realm GUI...
echo.

REM Run the actual executable produced by the Makefile.
"%REPO%\bin\realm.exe"

set "EXITCODE=%ERRORLEVEL%"

echo.
echo Realm exited with code: %EXITCODE%
echo Log saved to:
echo %LOG%
echo.

pause
exit /b %EXITCODE%
