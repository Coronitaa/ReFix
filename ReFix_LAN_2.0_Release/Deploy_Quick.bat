@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
title ReFix LAN 2.0 - Fast Deploy Tool

set "RELEASE_DIR=%~dp0"
if "%RELEASE_DIR:~-1%"=="\" set "RELEASE_DIR=%RELEASE_DIR:~0,-1%"
set "BIN_DIR=%RELEASE_DIR%\bin"

echo ====================================================================
echo             ReFix LAN 2.0 - Quick Deployment Tool
echo        (ISteamNetworkingSockets + Universal LanCore Engine)
echo ====================================================================
echo.

set "TARGET_DIR=%~1"
if not defined TARGET_DIR (
    echo Arrastra la carpeta del juego sobre este archivo .bat
    echo o ingresa la ruta completa a continuacion.
    echo.
    set /p "TARGET_DIR=Ruta del juego: "
)

if not defined TARGET_DIR (
    echo [ERROR] No se especifico ninguna ruta de destino.
    pause
    exit /b 1
)

:: Limpiar comillas
set "TARGET_DIR=%TARGET_DIR:"=%"
if "%TARGET_DIR:~-1%"=="\" set "TARGET_DIR=%TARGET_DIR:~0,-1%"

if not exist "%TARGET_DIR%" (
    echo [ERROR] La carpeta especificada no existe: "%TARGET_DIR%"
    pause
    exit /b 1
)

echo [*] Analizando directorio: "%TARGET_DIR%"
echo.

set "FOUND_ANY=0"

:: -------------------------------------------------------------------------
:: 1. DETECCION UNREAL ENGINE (Engine\Binaries\ThirdParty\Steamworks\*\Win64)
:: -------------------------------------------------------------------------
for /d %%D in ("%TARGET_DIR%\Engine\Binaries\ThirdParty\Steamworks\*") do (
    if exist "%%D\Win64" (
        echo [*] Detectada ruta de Steamworks en Unreal Engine: "%%D\Win64"
        call :DEPLOY_TO_DIR "%%D\Win64"
        set "FOUND_ANY=1"
    )
)

:: Buscar tambien en <Project>\Binaries\Win64
for /d %%P in ("%TARGET_DIR%\*") do (
    if exist "%%P\Binaries\Win64" (
        echo [*] Detectada carpeta de binarios de juego Unreal: "%%P\Binaries\Win64"
        call :DEPLOY_TO_DIR "%%P\Binaries\Win64"
        set "FOUND_ANY=1"
    )
)

:: -------------------------------------------------------------------------
:: 2. DETECCION UNITY (*_Data\Plugins\x86_64 o *_Data\Plugins)
:: -------------------------------------------------------------------------
for /d %%D in ("%TARGET_DIR%\*_Data") do (
    if exist "%%D\Plugins\x86_64" (
        echo [*] Detectada ruta de plugins Unity x64: "%%D\Plugins\x86_64"
        call :DEPLOY_TO_DIR "%%D\Plugins\x86_64"
        set "FOUND_ANY=1"
    ) else if exist "%%D\Plugins" (
        echo [*] Detectada ruta de plugins Unity: "%%D\Plugins"
        call :DEPLOY_TO_DIR "%%D\Plugins"
        set "FOUND_ANY=1"
    )
)

:: -------------------------------------------------------------------------
:: 3. DETECCION RAIZ / NATIVO (Directorio raiz o subdirectorios con steam_api64.dll)
:: -------------------------------------------------------------------------
if exist "%TARGET_DIR%\steam_api64.dll" (
    echo [*] Detectado steam_api64.dll en la raiz del juego: "%TARGET_DIR%"
    call :DEPLOY_TO_DIR "%TARGET_DIR%"
    set "FOUND_ANY=1"
)

:: Si no se detecto ninguna estructura conocida, desplegar en la carpeta indicada
if "%FOUND_ANY%"=="0" (
    echo [!] No se detecto ninguna estructura conocida de Unreal o Unity.
    echo [*] Desplegando directamente en: "%TARGET_DIR%"
    call :DEPLOY_TO_DIR "%TARGET_DIR%"
)

echo.
echo ====================================================================
echo                    DESPLIEGUE COMPLETADO
echo ====================================================================
echo  - Proxies y transporte LAN canonico instalados exitosamente.
echo  - Backups de DLLs originales preservados como *_o.dll.
echo  - Listo para jugar en LAN/Offline sin Steam ejecutandose.
echo.
pause
exit /b 0

:: =========================================================================
:: FUNCION DE DESPLIEGUE A DIRECTORIO ESPECIFICO
:: =========================================================================
:DEPLOY_TO_DIR
set "DEST=%~1"
echo   [*] Destino: "!DEST!"

:: Backup de steam_api64.dll
if exist "!DEST!\steam_api64.dll" if not exist "!DEST!\steam_api64_o.dll" (
    echo      [*] Creando backup original: steam_api64_o.dll
    copy /Y "!DEST!\steam_api64.dll" "!DEST!\steam_api64_o.dll" >nul
)
copy /Y "%BIN_DIR%\steam_api64.dll" "!DEST!\steam_api64.dll" >nul
echo      [OK] steam_api64.dll - Proxy ReFix LAN con Canal 250 instalado.

:: Copiar ReFix.ini y steam_appid.txt
if not exist "!DEST!\ReFix.ini" (
    copy /Y "%RELEASE_DIR%\ReFix.ini" "!DEST!\ReFix.ini" >nul
    echo      [OK] ReFix.ini copiado.
)
if not exist "!DEST!\steam_appid.txt" (
    copy /Y "%RELEASE_DIR%\steam_appid.txt" "!DEST!\steam_appid.txt" >nul
    echo      [OK] steam_appid.txt copiado - AppID 480.
)

:: Copiar steam_settings si no existe
if not exist "!DEST!\steam_settings" (
    xcopy /E /I /Q /Y "%RELEASE_DIR%\steam_settings" "!DEST!\steam_settings" >nul
    echo      [OK] Carpeta steam_settings copiada.
)

:: Soporte EOS si el juego usa EOSSDK-Win64-Shipping.dll
if exist "!DEST!\EOSSDK-Win64-Shipping.dll" (
    if not exist "!DEST!\EOSSDK_original.dll" (
        echo      [*] Creando backup original: EOSSDK_original.dll
        copy /Y "!DEST!\EOSSDK-Win64-Shipping.dll" "!DEST!\EOSSDK_original.dll" >nul
    )
    copy /Y "%BIN_DIR%\EOSSDK-Win64-Shipping.dll" "!DEST!\EOSSDK-Win64-Shipping.dll" >nul
    copy /Y "%BIN_DIR%\RedboneEOS.dll" "!DEST!\RedboneEOS.dll" >nul
    echo      [OK] Emuladores EOS actualizados - RedboneEOS y EOSSDK.
)

:: Soporte RedpointEOS si existe la subcarpeta
if exist "!DEST!\RedpointEOS\EOSSDK-Win64-Shipping.dll" (
    if not exist "!DEST!\RedpointEOS\EOSSDK_original.dll" (
        echo      [*] Creando backup original RedpointEOS: EOSSDK_original.dll
        copy /Y "!DEST!\RedpointEOS\EOSSDK-Win64-Shipping.dll" "!DEST!\RedpointEOS\EOSSDK_original.dll" >nul
    )
    copy /Y "%BIN_DIR%\EOSSDK-Win64-Shipping.dll" "!DEST!\RedpointEOS\EOSSDK-Win64-Shipping.dll" >nul
    copy /Y "%BIN_DIR%\RedboneEOS.dll" "!DEST!\RedpointEOS\RedboneEOS.dll" >nul
    echo      [OK] Emuladores EOS actualizados en subcarpeta RedpointEOS.
)

:: Copiar winmm.dll (overlay) si esta presente en el juego o se solicita
if exist "!DEST!\winmm.dll" if not exist "!DEST!\winmm_o.dll" (
    copy /Y "!DEST!\winmm.dll" "!DEST!\winmm_o.dll" >nul
    copy /Y "%BIN_DIR%\winmm.dll" "!DEST!\winmm.dll" >nul
    echo      [OK] Overlay winmm.dll actualizado.
)

goto :eof
