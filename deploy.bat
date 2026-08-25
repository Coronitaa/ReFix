@echo off
REM =============================================================================
REM ReFix Deploy Script v1.1.0
REM =============================================================================
REM Automated deployment of open-source binaries to game executable directory.
REM Creates safe backups (*_o.dll) of existing game DLLs and initializes ReFix.log.
REM Supports RedpointEOS custom plugin layout.
REM =============================================================================

setlocal enabledelayedexpansion

set "TARGET_DIR=%~1"
if not defined TARGET_DIR set "TARGET_DIR=D:\EOS_REFIX\MECCHA CHAMELEON"
if not exist "!TARGET_DIR!" set "TARGET_DIR=D:\Games\MECCHA CHAMELEON"
if not exist "!TARGET_DIR!" set "TARGET_DIR=C:\Users\Valen\Desktop\STEAM_CRACKING\DepotDownloader\depots\4704691\24176442"

if "!TARGET_DIR:~-1!"=="\" set "TARGET_DIR=!TARGET_DIR:~0,-1!"

set "DEPOT=!TARGET_DIR!"
set "GAME_BIN=!DEPOT!\Chameleon\Binaries\Win64"
if not exist "!GAME_BIN!" set "GAME_BIN=!DEPOT!"
set "STEAM_DIR=!DEPOT!\Engine\Binaries\ThirdParty\Steamworks\Steamv157\Win64"
if not exist "!STEAM_DIR!" set "STEAM_DIR=!GAME_BIN!"
set "EOS_DIR=!GAME_BIN!\RedpointEOS"
set "BUILD_DIR=%~dp0build"

echo.
echo  ====================================
echo   ReFix Deployment Script v1.1.0
echo  ====================================
echo   Target: !DEPOT!
echo   Game Bin: !GAME_BIN!
echo   Steam Dir: !STEAM_DIR!
echo  ====================================
echo.

REM --- Verify build outputs exist ---
if not exist "%BUILD_DIR%\winmm.dll" (
    echo [!] ERROR: winmm.dll not found in build\. Run build.bat first.
    exit /b 1
)
if not exist "%BUILD_DIR%\steam_api64.dll" (
    echo [!] ERROR: steam_api64.dll not found in build\. Run build.bat first.
    exit /b 1
)
if not exist "%BUILD_DIR%\EOSSDK-Win64-Shipping.dll" (
    echo [!] ERROR: EOSSDK-Win64-Shipping.dll not found in build\. Run build.bat first.
    exit /b 1
)
echo [OK] All build artifacts verified in build\.

echo.
echo  --- Step 1/5: Deploy winmm.dll (Loader + ImGui Server Browser Overlay) ---
echo.

if exist "!GAME_BIN!\winmm.dll" if not exist "!GAME_BIN!\winmm_o.dll" (
    echo [*] Creating safe backup: winmm_o.dll...
    copy /Y "!GAME_BIN!\winmm.dll" "!GAME_BIN!\winmm_o.dll" >nul
)
copy /Y "%BUILD_DIR%\winmm.dll" "!GAME_BIN!\winmm.dll" >nul
echo [OK] Deployed winmm.dll to game directory

echo.
echo  --- Step 2/5: Deploy Steam API Proxy (Metadata Tagging: game_filter=mechachameleon) ---
echo.

if not exist "!STEAM_DIR!\steam_api64_valve.dll" (
    if exist "!STEAM_DIR!\steam_api64.dll" (
        echo [*] Creating safe backup: steam_api64_valve.dll in Steamworks folder...
        rename "!STEAM_DIR!\steam_api64.dll" "steam_api64_valve.dll"
    )
) else (
    echo [*] steam_api64_valve.dll already exists.
)

if exist "!STEAM_DIR!\steam_api64.dll" if not exist "!STEAM_DIR!\steam_api64_o.dll" (
    copy /Y "!STEAM_DIR!\steam_api64.dll" "!STEAM_DIR!\steam_api64_o.dll" >nul
)

copy /Y "%BUILD_DIR%\steam_api64.dll" "!STEAM_DIR!\steam_api64.dll" >nul
if exist "!GAME_BIN!" (
    if exist "!GAME_BIN!\steam_api64.dll" if not exist "!GAME_BIN!\steam_api64_o.dll" (
        copy /Y "!GAME_BIN!\steam_api64.dll" "!GAME_BIN!\steam_api64_o.dll" >nul
    )
    copy /Y "%BUILD_DIR%\steam_api64.dll" "!GAME_BIN!\steam_api64.dll" >nul
)
echo [OK] Deployed steam_api64.dll proxy

echo 480> "!STEAM_DIR!\steam_appid.txt"
echo 480> "!GAME_BIN!\steam_appid.txt"
echo [OK] Created steam_appid.txt (AppID 480 - Spacewar)

echo.
echo  --- Step 3/5: Deploy EOS SDK Proxy ^& RedpointEOS Integration ---
echo.

if not exist "!EOS_DIR!" mkdir "!EOS_DIR!"

if not exist "!EOS_DIR!\EOSSDK_original.dll" (
    if exist "!EOS_DIR!\EOSSDK-Win64-Shipping.dll" (
        echo [*] Creating safe backup: EOSSDK_original.dll...
        rename "!EOS_DIR!\EOSSDK-Win64-Shipping.dll" "EOSSDK_original.dll"
    )
) else (
    echo [*] EOSSDK_original.dll already exists.
)

copy /Y "%BUILD_DIR%\EOSSDK-Win64-Shipping.dll" "!EOS_DIR!\EOSSDK-Win64-Shipping.dll" >nul
copy /Y "%BUILD_DIR%\RedboneEOS.dll" "!EOS_DIR!\RedboneEOS.dll" >nul

if exist "!GAME_BIN!\EOSSDK-Win64-Shipping.dll" if not exist "!GAME_BIN!\EOSSDK_original.dll" (
    copy /Y "!GAME_BIN!\EOSSDK-Win64-Shipping.dll" "!GAME_BIN!\EOSSDK_original.dll" >nul
)
copy /Y "%BUILD_DIR%\EOSSDK-Win64-Shipping.dll" "!GAME_BIN!\EOSSDK-Win64-Shipping.dll" >nul
copy /Y "%BUILD_DIR%\RedboneEOS.dll" "!GAME_BIN!\RedboneEOS.dll" >nul

echo [OK] Deployed EOSSDK-Win64-Shipping.dll and RedboneEOS.dll proxies

echo.
echo  --- Step 4/5: Deploy Configuration ^& Logfile ---
echo.

copy /Y "%~dp0ReFix.ini" "!GAME_BIN!\ReFix.ini" >nul
echo [OK] Copied ReFix.ini configuration

echo [ReFix System Audit Log Started] > "!GAME_BIN!\ReFix.log"
echo [OK] Initialized ReFix.log audit log

echo.
echo  --- Step 5/5: Copy Steam Interfaces ---
echo.

if not exist "%STEAM_DIR%\steam_settings" mkdir "%STEAM_DIR%\steam_settings"
set "OF_STEAM=D:\MECCHA.CHAMELEON.v2.3.1\Engine\Binaries\ThirdParty\Steamworks\Steamv157\Win64\steam_settings"
if exist "%OF_STEAM%\steam_interfaces.txt" (
    copy /Y "%OF_STEAM%\steam_interfaces.txt" "%STEAM_DIR%\steam_settings\steam_interfaces.txt" >nul
    echo [OK] Copied steam_interfaces.txt
) else (
    echo [WARN] steam_interfaces.txt not found, using defaults.
)

echo.
echo  ====================================
echo   DEPLOYMENT COMPLETE
echo  ====================================
echo.
echo  Game Executable:
echo    %GAME_BIN%\PenguinHotel-Win64-Shipping.exe
echo.
echo  Audit Log: %GAME_BIN%\ReFix.log
echo  Hotkeys: F1 or Insert (Toggle Source-style ImGui Server Browser)
echo.
