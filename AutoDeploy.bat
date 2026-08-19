@echo off
setlocal enabledelayedexpansion
title ReFix - Universal AutoDeploy Tool v6.0

set "SCRIPT_DIR=%~dp0"
if "!SCRIPT_DIR:~-1!"=="\" set "SCRIPT_DIR=!SCRIPT_DIR:~0,-1!"
set "BIN_DIR=!SCRIPT_DIR!\bin"

echo ====================================================================
echo                   ReFix Universal AutoDeploy Tool
echo ====================================================================
echo:

if not exist "!BIN_DIR!\steam_api64.dll" (
    echo [ERROR] Required binaries not found in "!BIN_DIR!"!
    echo Please make sure the 'bin' folder contains steam_api64.dll, etc.
    goto ERROR_EXIT
)

if not "%~1"=="" (
    set "TARGET_DIR=%~1"
    goto GOT_TARGET_DIR
)

echo Select deployment target mode:
echo [1] Deploy to game in CURRENT folder (!SCRIPT_DIR!)
echo [2] Select game folder using File Explorer (GUI Picker)
echo [3] Enter game folder path manually
echo:
set "TARGET_CHOICE=2"
set /p "TARGET_CHOICE=Select option [1-3] (Default: 2): "
if "!TARGET_CHOICE!"=="" set "TARGET_CHOICE=2"

if "!TARGET_CHOICE!"=="1" (
    set "TARGET_DIR=!SCRIPT_DIR!"
    goto GOT_TARGET_DIR
)
if "!TARGET_CHOICE!"=="3" goto MANUAL_TARGET_INPUT

:: Option 2: GUI Picker
echo:
echo [INFO] Opening File Explorer folder selection dialog...
if exist "!BIN_DIR!\select_folder.ps1" (
    for /f "usebackq delims=" %%I in (`powershell -NoProfile -ExecutionPolicy Bypass -File "!BIN_DIR!\select_folder.ps1" ^<nul`) do (
        set "TARGET_DIR=%%I"
    )
)
goto GOT_TARGET_DIR

:MANUAL_TARGET_INPUT
echo:
set /p "TARGET_DIR=Enter target game directory path: "
goto GOT_TARGET_DIR

:GOT_TARGET_DIR
if "!TARGET_DIR!"=="" (
    echo:
    echo [NOTICE] No folder was selected or operation was cancelled by user.
    goto ERROR_EXIT
)

:: Strip trailing backslash and quotes if present
set "TARGET_DIR=!TARGET_DIR:"=!"
if "!TARGET_DIR:~-1!"=="\" set "TARGET_DIR=!TARGET_DIR:~0,-1!"

if not exist "!TARGET_DIR!" (
    echo:
    echo [ERROR] Directory does not exist: "!TARGET_DIR!"
    goto ERROR_EXIT
)

echo:
echo ====================================================================
echo Analyzing Game Directory: "!TARGET_DIR!"
echo ====================================================================
echo:

:: Run smart game detector to find exact EXE_DIR, GAME_EXE_PATH, ENGINE_TYPE, GAME_NAME, and DETECTED_APPID
set "ENGINE_TYPE=Unity"
set "EXE_DIR=!TARGET_DIR!"
set "GAME_EXE_PATH="
set "DEFAULT_GAME_NAME="
set "DETECTED_APPID="

if exist "!BIN_DIR!\detect_game.ps1" (
    for /f "usebackq tokens=1,* delims==" %%A in (`powershell -NoProfile -ExecutionPolicy Bypass -File "!BIN_DIR!\detect_game.ps1" -TargetDir "!TARGET_DIR!" ^<nul`) do (
        if /i "%%A"=="ENGINE_TYPE" set "ENGINE_TYPE=%%B"
        if /i "%%A"=="EXE_DIR" set "EXE_DIR=%%B"
        if /i "%%A"=="GAME_EXE_PATH" set "GAME_EXE_PATH=%%B"
        if /i "%%A"=="GAME_NAME" set "DEFAULT_GAME_NAME=%%B"
        if /i "%%A"=="DETECTED_APPID" set "DETECTED_APPID=%%B"
    )
)

if "!DEFAULT_GAME_NAME!"=="" (
    for %%I in ("!TARGET_DIR!") do set "DEFAULT_GAME_NAME=%%~nxI"
)

echo [DETECTION] Detected Engine Type: !ENGINE_TYPE!
echo [DETECTION] Executable Location: "!EXE_DIR!"
if not "!GAME_EXE_PATH!"=="" echo [DETECTION] Game Executable: "!GAME_EXE_PATH!"
if not "!DETECTED_APPID!"=="" echo [DETECTION] Detected Steam AppID: !DETECTED_APPID!
echo:
set "USER_ENGINE_CHOICE="
set /p "USER_ENGINE_CHOICE=Confirm Engine [Unity/Godot/Unreal] (Default: !ENGINE_TYPE!): "
if not "!USER_ENGINE_CHOICE!"=="" set "ENGINE_TYPE=!USER_ENGINE_CHOICE!"

:: Read existing settings from ReFix.ini if it already exists in target
set "EXISTING_REAL_APPID=!DETECTED_APPID!"
set "EXISTING_USERNAME="
set "EXISTING_STEAMID="
set "EXISTING_LAN_PORT=47584"
if exist "!EXE_DIR!\ReFix.ini" (
    for /f "usebackq tokens=1,* delims==" %%A in ("!EXE_DIR!\ReFix.ini") do (
        if /i "%%A"=="RealAppId" (
            if not "%%B"=="" if not "%%B"=="0" if not "%%B"=="480" set "EXISTING_REAL_APPID=%%B"
        )
        if /i "%%A"=="Name" set "EXISTING_USERNAME=%%B"
        if /i "%%A"=="SteamId" set "EXISTING_STEAMID=%%B"
        if /i "%%A"=="ListenPort" set "EXISTING_LAN_PORT=%%B"
    )
)
if "!EXISTING_REAL_APPID!"=="" set "EXISTING_REAL_APPID=480"

set "GAME_NAME="
set /p "GAME_NAME=Enter Game Name (Default: !DEFAULT_GAME_NAME!): "
if "!GAME_NAME!"=="" set "GAME_NAME=!DEFAULT_GAME_NAME!"

echo:
echo ====================================================================
echo                  Select Connectivity Mode
echo ====================================================================
echo:
echo  [1] ReFix Online por Steam
echo      - Utiliza cliente Steam y Spacewar (AppID 480).
echo      - Permite juego online multijugador a traves de servidores de Steam.
echo:
echo  [2] Re:Goldberg LAN sin Steam
echo      - Emulacion 100%% autonoma y local basada en gbe_fork backend.
echo      - Permite jugar en LAN sin requerir Steam instalado ni abierto.
echo      - Descubrimiento broadcast local, saves portables e identidad persistente.
echo:
set "ONLINE_MODE_CHOICE="
set /p "ONLINE_MODE_CHOICE=Select Option [1-2] (Default: 1): "
if "!ONLINE_MODE_CHOICE!"=="" set "ONLINE_MODE_CHOICE=1"

set "ONLINE_MODE_NAME=valve"
if "!ONLINE_MODE_CHOICE!"=="2" set "ONLINE_MODE_NAME=goldberg"

if "!ONLINE_MODE_NAME!"=="goldberg" goto GOLDBERG_PROMPTS

:VALVE_PROMPTS
echo:
echo [INFO] Modo seleccionado: ReFix Online por Steam
echo:
set "MASK_APPID=480"
set "REAL_APPID="
if not "!EXISTING_REAL_APPID!"=="" if not "!EXISTING_REAL_APPID!"=="480" (
    set /p "REAL_APPID=Enter Real Steam AppID (Default: !EXISTING_REAL_APPID!): "
    if "!REAL_APPID!"=="" set "REAL_APPID=!EXISTING_REAL_APPID!"
) else (
    set /p "REAL_APPID=Enter Real Steam AppID: "
    if "!REAL_APPID!"=="" set "REAL_APPID=480"
)

set "DLCS="
set /p "DLCS=Enter DLC AppIDs [Comma-separated, e.g. 12345,67890] (Optional): "

set "CUSTOM_USERNAME="
set /p "CUSTOM_USERNAME=Enter Custom Username (Optional - Enter to use Steam name): "
if "!CUSTOM_USERNAME!"=="" set "CUSTOM_USERNAME=!EXISTING_USERNAME!"
set "LAN_PORT=47584"
set "CUSTOM_BROADCASTS="
goto AFTER_MODE_PROMPTS

:GOLDBERG_PROMPTS
echo:
echo [INFO] Modo seleccionado: Re:Goldberg LAN sin Steam
echo:
set "CUSTOM_USERNAME="
set /p "CUSTOM_USERNAME=Enter Player Username (Dejar vacio para auto-generar): "
if "!CUSTOM_USERNAME!"=="" set "CUSTOM_USERNAME=!EXISTING_USERNAME!"

set "REAL_APPID="
if not "!EXISTING_REAL_APPID!"=="" if not "!EXISTING_REAL_APPID!"=="480" (
    set /p "REAL_APPID=Enter Real Steam AppID (Default: !EXISTING_REAL_APPID!): "
    if "!REAL_APPID!"=="" set "REAL_APPID=!EXISTING_REAL_APPID!"
) else (
    set /p "REAL_APPID=Enter Real Steam AppID: "
    if "!REAL_APPID!"=="" set "REAL_APPID=480"
)
set "MASK_APPID=!REAL_APPID!"

set "LAN_PORT="
set /p "LAN_PORT=Enter LAN Listen Port (Default: !EXISTING_LAN_PORT!): "
if "!LAN_PORT!"=="" set "LAN_PORT=!EXISTING_LAN_PORT!"

set "CUSTOM_BROADCASTS="
set /p "CUSTOM_BROADCASTS=Enter Custom Broadcast IPs [Comma-separated, Optional]: "
set "DLCS="
goto AFTER_MODE_PROMPTS

:AFTER_MODE_PROMPTS
set "PHOTON_APPID="
set "PHOTON_REGION="
set "CUSTOM_PUBLIC_IP="
set "FIREWALL_AUTO_APPLY=false"

if "!ONLINE_MODE_NAME!"=="goldberg" (
    echo:
    echo ====================================================================
    echo             Configuracion de Firewall para LAN
    echo ====================================================================
    echo:
    echo  Deseas autorizar automaticamente el juego y el puerto UDP !LAN_PORT!
    echo  en el Firewall de Windows para asegurar el descubrimiento LAN?
    echo:
    set "FW_CHOICE="
    set /p "FW_CHOICE=Aplicar reglas de Firewall? [S/N] (Default: S): "
    if "!FW_CHOICE!"=="" set "FW_CHOICE=S"
    if /i "!FW_CHOICE!"=="N" (
        set "FIREWALL_AUTO_APPLY=false"
    ) else (
        set "FIREWALL_AUTO_APPLY=true"
    )
)

echo:
echo ====================================================================
echo Starting ReFix Deployment... (Mode: !ONLINE_MODE_NAME!)
echo ====================================================================

:: Step 1: Copy winmm.dll proxy only if in Valve mode
if "!ONLINE_MODE_NAME!"=="goldberg" goto SKIP_WINMM_DEPLOY
if not exist "!BIN_DIR!\winmm.dll" goto SKIP_WINMM_DEPLOY

echo [1/6] Deploying winmm.dll proxy...
copy /y "!BIN_DIR!\winmm.dll" "!EXE_DIR!\winmm.dll" >nul
echo [OK] Deployed winmm.dll to "!EXE_DIR!"
goto AFTER_WINMM_DEPLOY

:SKIP_WINMM_DEPLOY
echo [1/6] Skipping winmm.dll proxy for offline/native mode...

:AFTER_WINMM_DEPLOY

:: Steps 2 & 3: Run PowerShell helper for deployment & configuration synchronization
if exist "!BIN_DIR!\deploy_helper.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "& '!BIN_DIR!\deploy_helper.ps1' -TargetDir '!TARGET_DIR!' -BinDir '!BIN_DIR!' -ExeDir '!EXE_DIR!' -EngineType '!ENGINE_TYPE!' -OnlineMode '!ONLINE_MODE_NAME!' -GameName '!GAME_NAME!' -UserName '!CUSTOM_USERNAME!' -RealAppId '!REAL_APPID!' -MaskAppId '!MASK_APPID!' -Language 'english' -DLCs '!DLCS!' -ListenPort '!LAN_PORT!' -CustomBroadcasts '!CUSTOM_BROADCASTS!' -PhotonAppId '!PHOTON_APPID!' -PhotonRegion '!PHOTON_REGION!'"
) else (
    echo [ERROR] !BIN_DIR!\deploy_helper.ps1 missing!
    goto ERROR_EXIT
)

:: Step 4 & 5: For Valve mode only, write additional legacy ReFix.ini and steam_appid.txt
if "!ONLINE_MODE_NAME!"=="goldberg" goto SKIP_VALVE_MANUAL_INI

echo [4/6] Generating steam_appid.txt...
echo !MASK_APPID!> "!EXE_DIR!\steam_appid.txt"
echo [OK] Created steam_appid.txt (AppID: !MASK_APPID!)

echo [5/6] Writing ReFix.ini configuration...
set "GAME_FILTER_VALUE="
if not "!REAL_APPID!"=="" set "GAME_FILTER_VALUE=!REAL_APPID!"

(
echo [Game]
echo GameName=!GAME_NAME!
echo EngineType=!ENGINE_TYPE!
echo.
echo [Matchmaking]
echo EnableLobbyFilter=false
echo LobbyFilterKey=game_filter
echo LobbyFilterValue=!GAME_FILTER_VALUE!
echo LobbyDistanceFilter=Worldwide
echo MaxLobbyResults=50
echo.
echo [ServerBrowser]
echo OverrideServerListAppId=false
echo ServerListAppId=480
echo Language=english
echo.
echo [Steam]
echo MaskAppId=!MASK_APPID!
echo RealAppId=!REAL_APPID!
echo Language=english
echo.
echo [Overlay]
echo EnableOverlay=true
echo OverlayAppId=!MASK_APPID!
echo.
echo [EOS]
echo DeviceIdAuth=true
echo.
echo [User]
echo Name=!CUSTOM_USERNAME!
echo SteamId=
echo.
echo [Online]
echo Mode=!ONLINE_MODE_NAME!
echo.
echo [Network]
echo GameFilter=!GAME_FILTER_VALUE!
echo PublicIP=!CUSTOM_PUBLIC_IP!
echo LocalIP=
echo.
echo [P2P]
echo EnableWAN=true
echo P2PPort=7777
echo AllowRelay=true
echo ForcePublicIPInLobby=true
) > "!EXE_DIR!\ReFix.ini"
echo [OK] Configured ReFix.ini in "!EXE_DIR!"

:SKIP_VALVE_MANUAL_INI

:: Step 6: Copy Steam Shortcut Installer & PowerShell helper (Valve mode only)
if "!ONLINE_MODE_NAME!"=="valve" (
    echo [6/6] Copying Steam Shortcut scripts...
    if exist "!BIN_DIR!\add_steam_shortcut.ps1" (
        copy /y "!BIN_DIR!\add_steam_shortcut.ps1" "!EXE_DIR!\add_steam_shortcut.ps1" >nul
    )
    if exist "!BIN_DIR!\Install_ReFix_Steam_Shortcut.bat" (
        copy /y "!BIN_DIR!\Install_ReFix_Steam_Shortcut.bat" "!EXE_DIR!\Install_ReFix_Steam_Shortcut.bat" >nul
    )
    echo [OK] Deployed Steam Shortcut Installer scripts to "!EXE_DIR!"
) else (
    echo [6/6] Finalizing portable configuration...
)

:: ============================================================
:: Step 7: Apply Windows Firewall Rules dynamically
:: ============================================================
if "!FIREWALL_AUTO_APPLY!" neq "true" goto SKIP_FIREWALL

echo:
echo [7/7] Configurando reglas del Firewall de Windows para juego y red LAN...
if exist "!BIN_DIR!\apply_firewall.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "!BIN_DIR!\apply_firewall.ps1" -GameExe "!GAME_EXE_PATH!" -GameName "!GAME_NAME!" -LanPort "!LAN_PORT!" -Mode "!ONLINE_MODE_NAME!"
)

:SKIP_FIREWALL

echo:
echo ====================================================================
echo  DEPLOY COMPLETO
echo  Juego: !GAME_NAME!  ^|  Motor: !ENGINE_TYPE!
if "!ONLINE_MODE_NAME!"=="goldberg" goto SUMMARY_GOLDBERG

:SUMMARY_VALVE
echo  Modo: ReFix Online por Steam [Spacewar 480]
echo  Ubicacion: "!EXE_DIR!"
echo  AppID: MaskAppId=!MASK_APPID!  /  RealAppId=!REAL_APPID!
goto AFTER_SUMMARY

:SUMMARY_GOLDBERG
echo  Modo: Re:Goldberg LAN sin Steam [Offline / gbe_fork]
echo  Ubicacion: "!EXE_DIR!"
echo  AppID: RealAppId=!REAL_APPID!
echo  Puerto LAN: !LAN_PORT! [UDP Broadcast Discovery]
echo  Almacenamiento: Portable en saves\ [Compatible con USB / Pendrive]
echo  Firewall Helper: "!EXE_DIR!\Configurar_Firewall_LAN.bat"
goto AFTER_SUMMARY

:AFTER_SUMMARY
echo ====================================================================
echo:

:: Offer one-click launch if game executable is found
if "!GAME_EXE_PATH!"=="" goto SKIP_LAUNCH_OFFER

echo  Ejecutable listo: "!GAME_EXE_PATH!"
echo:
set "LAUNCH_NOW=S"
set /p "LAUNCH_NOW=Lanzar el juego ahora? [S/N] (Default: S): "
if /i "!LAUNCH_NOW!"=="N" goto SKIP_LAUNCH_OFFER

echo:
echo [LAUNCH] Iniciando "!GAME_NAME!"...
start "" "!GAME_EXE_PATH!"
echo [OK] Juego iniciado.
goto SUCCESS_EXIT

:SKIP_LAUNCH_OFFER

if "!ONLINE_MODE_NAME!"=="valve" (
    set "RUN_INSTALLER=S"
    set /p "RUN_INSTALLER=Agregar a Steam como Non-Steam Game? [S/N] (Default: S): "
    if /i "!RUN_INSTALLER!"=="N" goto SUCCESS_EXIT

    echo:
    echo [INFO] Ejecutando Steam Shortcut Auto-Installer...
    pushd "!EXE_DIR!"
    if exist "Install_ReFix_Steam_Shortcut.bat" (
        call Install_ReFix_Steam_Shortcut.bat "!GAME_NAME!" "!GAME_EXE_PATH!"
    )
    popd
)

:SUCCESS_EXIT
echo:
echo Deployment completado exitosamente.
echo:
echo Presiona cualquier tecla para salir...
pause >nul
exit /b 0

:ERROR_EXIT
echo:
echo [ERROR FATAL] La operacion no se completo. Revisa los mensajes arriba.
echo:
echo Presiona cualquier tecla para salir...
pause >nul
exit /b 1
