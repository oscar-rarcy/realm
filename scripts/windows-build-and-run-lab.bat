@echo off
setlocal

REM Realm Windows local tileset lab build/run script.
REM Run it from PowerShell, Command Prompt, or double-click it.
REM Pass "smoke" as the first argument to write non-interactive screenshots.
REM Pass "nopause" as either argument to skip the final pause.

set "MSYS2=C:\msys64"
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO=%%~fI"

REM Keep the log outside build/, because `make clean` deletes build/.
set "LOG_DIR=%REPO%\logs"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
set "LOG=%LOG_DIR%\windows-lab-build.log"

set "SMOKE=0"
set "NOPAUSE=0"
if /I "%~1"=="smoke" set "SMOKE=1"
if /I "%~1"=="nopause" set "NOPAUSE=1"
if /I "%~2"=="nopause" set "NOPAUSE=1"

echo Realm Windows local tileset lab
echo Repo: %REPO%
echo MSYS2: %MSYS2%
echo Log: %LOG%
if "%SMOKE%"=="1" echo Mode: smoke screenshots
echo.

if not exist "%MSYS2%\msys2_shell.cmd" (
    echo ERROR: MSYS2 was not found at:
    echo %MSYS2%
    echo.
    echo Install MSYS2, or edit the MSYS2 path at the top of this file.
    if not "%NOPAUSE%"=="1" pause
    exit /b 1
)

cd /d "%REPO%"

echo Installing/checking MSYS2 UCRT64 lab dependencies...
echo Building lab target...
echo.

call "%MSYS2%\msys2_shell.cmd" -ucrt64 -defterm -no-start -where "%REPO%" -c "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf mingw-w64-ucrt-x86_64-libpng && mingw32-make lab && test -f bin/realm-lab.exe" > "%LOG%" 2>&1

set "EXITCODE=%ERRORLEVEL%"

echo.
echo Finished build with exit code: %EXITCODE%
echo.

if not "%EXITCODE%"=="0" (
    echo Build failed. Log output:
    echo ----------------------------------------
    type "%LOG%"
    echo ----------------------------------------
    if not "%NOPAUSE%"=="1" pause
    exit /b %EXITCODE%
)

echo Build succeeded.
if "%SMOKE%"=="1" (
    echo Starting Realm lab smoke run...
    set "REALM_LAB_SMOKE=1"
) else (
    echo Starting Realm lab...
)
echo.

"%REPO%\bin\realm-lab.exe"

set "EXITCODE=%ERRORLEVEL%"

echo.
echo Realm lab exited with code: %EXITCODE%
echo Log saved to:
echo %LOG%
if "%SMOKE%"=="1" echo Screenshots: %REPO%\build\lab-screenshots
echo.

if not "%NOPAUSE%"=="1" pause
exit /b %EXITCODE%
