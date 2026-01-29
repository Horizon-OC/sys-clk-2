@echo off
setlocal enabledelayedexpansion

REM --- Root directory ---
set ROOT_DIR=%~dp0
set DIST_DIR=%ROOT_DIR%dist

REM --- Number of CPU cores ---
set CORES=%NUMBER_OF_PROCESSORS%

REM --- Optional first argument as DIST_DIR ---
if not "%~1"=="" set DIST_DIR=%~1

echo DIST_DIR: %DIST_DIR%
echo CORES: %CORES%

REM ========================
REM sysmodule
REM ========================
echo *** sysmodule ***

REM Extract TITLE_ID from perms.json using findstr (rough approximation)
for /f "tokens=2 delims=: " %%A in ('findstr /i "title_id" "%ROOT_DIR%sysmodule\perms.json"') do (
    set TITLE_ID=%%A
)

REM Remove quotes and 0x prefix
set TITLE_ID=!TITLE_ID:"=!
set TITLE_ID=!TITLE_ID:0x=!

REM Build sysmodule
pushd "%ROOT_DIR%sysmodule"
make -j %CORES%
popd

REM Copy sysmodule files to dist
if not exist "%DIST_DIR%\atmosphere\contents\%TITLE_ID%\flags" mkdir "%DIST_DIR%\atmosphere\contents\%TITLE_ID%\flags"
copy /Y "%ROOT_DIR%sysmodule\out\horizon-oc.nsp" "%DIST_DIR%\atmosphere\contents\%TITLE_ID%\exefs.nsp"
type nul > "%DIST_DIR%\atmosphere\contents\%TITLE_ID%\flags\boot2.flag"
copy /Y "%ROOT_DIR%sysmodule\toolbox.json" "%DIST_DIR%\atmosphere\contents\%TITLE_ID%\toolbox.json"

REM ========================
REM overlay
REM ========================
echo *** overlay ***
pushd "%ROOT_DIR%overlay"
make -j %CORES%
popd

if not exist "%DIST_DIR%\switch\.overlays" mkdir "%DIST_DIR%\switch\.overlays"
copy /Y "%ROOT_DIR%overlay\out\horizon-oc-overlay.ovl" "%DIST_DIR%\switch\.overlays\horizon-oc-overlay.ovl"

REM ========================
REM assets
REM ========================
echo *** assets ***
if not exist "%DIST_DIR%\config\horizon-oc" mkdir "%DIST_DIR%\config\horizon-oc"
copy /Y "%ROOT_DIR%config.ini.template" "%DIST_DIR%\config\horizon-oc\config.ini.template"
copy /Y "%ROOT_DIR%..\..\README.md" "%DIST_DIR%\README.md"

endlocal
