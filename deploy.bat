@echo off
REM =============================================================================
REM ReFix Deploy Script
REM =============================================================================
REM Applies the ReFix clean crack to the game depot.
REM Target: C:\Users\Valen\Desktop\STEAM_CRACKING\DepotDownloader\depots\4704691\24176442
REM =============================================================================

setlocal enabledelayedexpansion

set "DEPOT=C:\Users\Valen\Desktop\STEAM_CRACKING\DepotDownloader\depots\4704691\24176442"
set "GAME_BIN=%DEPOT%\Chameleon\Binaries\Win64"
set "STEAM_DIR=%DEPOT%\Engine\Binaries\ThirdParty\Steamworks\Steamv157\Win64"
set "EOS_DIR=%GAME_BIN%\RedpointEOS"
set "BUILD_DIR=%~dp0build"
set "GOLDBERG=C:\Users\Valen\Desktop\STEAM_CRACKING\Goldberg_Lan_Steam_Emu_master--475342f0"

echo.
echo  ====================================
echo   ReFix Deployment Script v1.0
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
echo [OK] All build artifacts found.

REM --- Verify target depot exists ---
if not exist "%GAME_BIN%\PenguinHotel-Win64-Shipping.exe" (
    echo [!] ERROR: Game executable not found at %GAME_BIN%
    exit /b 1
)
echo [OK] Game depot verified at %DEPOT%

echo.
echo  --- Step 1/5: Deploy winmm.dll (Loader) ---
echo.

copy /Y "%BUILD_DIR%\winmm.dll" "%GAME_BIN%\winmm.dll" >nul
echo [OK] Copied winmm.dll to game directory

echo.
echo  --- Step 2/5: Deploy Steam API Proxy ---
echo.

REM Rename original Valve DLL if not already renamed
if not exist "%STEAM_DIR%\steam_api64_valve.dll" (
    echo [*] Renaming original steam_api64.dll to steam_api64_valve.dll...
    rename "%STEAM_DIR%\steam_api64.dll" "steam_api64_valve.dll"
) else (
    echo [*] steam_api64_valve.dll already exists, skipping rename.
)

REM Copy our proxy as steam_api64.dll
copy /Y "%BUILD_DIR%\steam_api64.dll" "%STEAM_DIR%\steam_api64.dll" >nul
echo [OK] Deployed steam_api64.dll proxy

REM Create steam_appid.txt with Spacewar ID in both locations
echo 480> "%STEAM_DIR%\steam_appid.txt"
echo [OK] Created steam_appid.txt (AppID 480 - Spacewar)
echo 480> "%GAME_BIN%\steam_appid.txt"
echo [OK] Created steam_appid.txt in game bin directory

echo.
echo  --- Step 3/5: Deploy EOS SDK Proxy ---
echo.

REM Rename original EOS SDK if not already renamed
if not exist "%EOS_DIR%\EOSSDK_original.dll" (
    echo [*] Renaming original EOSSDK-Win64-Shipping.dll to EOSSDK_original.dll...
    rename "%EOS_DIR%\EOSSDK-Win64-Shipping.dll" "EOSSDK_original.dll"
) else (
    echo [*] EOSSDK_original.dll already exists, skipping rename.
)

REM Copy our EOS proxy
copy /Y "%BUILD_DIR%\EOSSDK-Win64-Shipping.dll" "%EOS_DIR%\EOSSDK-Win64-Shipping.dll" >nul
echo [OK] Deployed EOSSDK-Win64-Shipping.dll proxy

echo.
echo  --- Step 4/5: Deploy Configuration ---
echo.

copy /Y "%~dp0ReFix.ini" "%GAME_BIN%\ReFix.ini" >nul
echo [OK] Copied ReFix.ini configuration

echo.
echo  --- Step 5/5: Copy Steam Interfaces ---
echo.

REM Create steam_settings directory if it doesn't exist
if not exist "%STEAM_DIR%\steam_settings" mkdir "%STEAM_DIR%\steam_settings"

REM Copy steam_interfaces.txt from the OnlineFix version (known good)
set "OF_STEAM=D:\MECCHA.CHAMELEON.v2.3.1\Engine\Binaries\ThirdParty\Steamworks\Steamv157\Win64\steam_settings"
if exist "%OF_STEAM%\steam_interfaces.txt" (
    copy /Y "%OF_STEAM%\steam_interfaces.txt" "%STEAM_DIR%\steam_settings\steam_interfaces.txt" >nul
    echo [OK] Copied steam_interfaces.txt
) else (
    echo [WARN] steam_interfaces.txt not found, Steam may use defaults
)

echo.
echo  ====================================
echo   DEPLOYMENT COMPLETE
echo  ====================================
echo.
echo  Game is ready to launch from:
echo    %GAME_BIN%\PenguinHotel-Win64-Shipping.exe
echo.
echo  Or via the launcher:
echo    %DEPOT%\PenguinHotel.exe
echo.
echo  Requirements:
echo    - Steam client must be running
echo    - Steam user must own Spacewar (free, all users have it)
echo.
echo  Configuration: %GAME_BIN%\ReFix.ini
echo.
