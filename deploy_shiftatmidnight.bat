@echo off
setlocal enabledelayedexpansion

set "GAME_DIR=D:\Games\Shift At Midnight"
set "CLEAN_DEPLOY=C:\Users\Valen\Desktop\STEAM_CRACKING\ReFix_deploy"
set "REFIX_SRC=%~dp0"
if "%REFIX_SRC:~-1%"=="\" set "REFIX_SRC=%REFIX_SRC:~0,-1%"
set "BUILD_DIR=%REFIX_SRC%\build"

echo.
echo ====================================================================
echo   ReFix Deploy v3.0 - Shift At Midnight (Steam Spacewar Mode)
echo ====================================================================
echo.

if not exist "%BUILD_DIR%\steam_api64.dll" (
    echo [!] ERROR: %BUILD_DIR%\steam_api64.dll missing.
    exit /b 1
)
if not exist "%BUILD_DIR%\winmm.dll" (
    echo [!] ERROR: %BUILD_DIR%\winmm.dll missing.
    exit /b 1
)
echo [OK] Build binaries verified in "%BUILD_DIR%".

echo.
echo [*] Preparing clean deploy directory: "%CLEAN_DEPLOY%"...
if not exist "%CLEAN_DEPLOY%" mkdir "%CLEAN_DEPLOY%"
if not exist "%CLEAN_DEPLOY%\ShiftAtMidnight_Data\Plugins\x86_64" mkdir "%CLEAN_DEPLOY%\ShiftAtMidnight_Data\Plugins\x86_64"

copy /Y "%BUILD_DIR%\winmm.dll" "%CLEAN_DEPLOY%\winmm.dll" >nul
copy /Y "%BUILD_DIR%\steam_api64.dll" "%CLEAN_DEPLOY%\ShiftAtMidnight_Data\Plugins\x86_64\steam_api64.dll" >nul
echo 480> "%CLEAN_DEPLOY%\steam_appid.txt"
echo 480> "%CLEAN_DEPLOY%\ShiftAtMidnight_Data\Plugins\x86_64\steam_appid.txt"
copy /Y "%REFIX_SRC%\ReFix_ShiftAtMidnight.ini" "%CLEAN_DEPLOY%\ReFix.ini" >nul
echo [OK] Clean deploy directory populated successfully.

if not exist "%GAME_DIR%" (
    echo [WARN] Target game directory "%GAME_DIR%" not found. Skipping live game deployment.
    goto FINISH
)

echo.
echo [*] Deploying to target game: "%GAME_DIR%"...
set "GAME_PLUGINS=%GAME_DIR%\ShiftAtMidnight_Data\Plugins\x86_64"

if not exist "%GAME_PLUGINS%" mkdir "%GAME_PLUGINS%"

if exist "%GAME_PLUGINS%\steam_api64.dll" if not exist "%GAME_PLUGINS%\steam_api64_valve.dll" (
    echo [*] Creating safe backup: steam_api64_valve.dll...
    copy /Y "%GAME_PLUGINS%\steam_api64.dll" "%GAME_PLUGINS%\steam_api64_valve.dll" >nul
)

copy /Y "%BUILD_DIR%\winmm.dll" "%GAME_DIR%\winmm.dll" >nul
copy /Y "%BUILD_DIR%\steam_api64.dll" "%GAME_PLUGINS%\steam_api64.dll" >nul

echo 480> "%GAME_DIR%\steam_appid.txt"
echo 480> "%GAME_PLUGINS%\steam_appid.txt"

copy /Y "%REFIX_SRC%\ReFix_ShiftAtMidnight.ini" "%GAME_DIR%\ReFix.ini" >nul
echo [ReFix System Audit Log Started] > "%GAME_DIR%\ReFix.log"

echo [OK] Live game deployment complete.

:FINISH
echo.
echo ====================================================================
echo   DEPLOYMENT COMPLETE
echo   Game: Shift At Midnight  ^|  Mode: Valve (Spacewar AppID 480)
echo   Clean Package: %CLEAN_DEPLOY%
echo ====================================================================
echo.
