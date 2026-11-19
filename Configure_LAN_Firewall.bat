@echo off
setlocal enabledelayedexpansion
title ReFix - Configurador de Red LAN y Firewall

:: Solicitar elevación de Administrador si no se está ejecutando como tal
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [*] Solicitando permisos de Administrador...
    powershell -Command "Start-Process cmd -ArgumentList '/c `\"%~f0`\"' -Verb RunAs"
    exit /b
)

echo ====================================================================
echo        ReFix - Configurador de Reglas de Red LAN y Firewall
echo ====================================================================
echo.

echo [*] 1. Configurando perfil de red activa a "Privada"...
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Get-NetConnectionProfile | Where-Object { $_.NetworkCategory -ne 'Private' } | Set-NetConnectionProfile -NetworkCategory Private -ErrorAction SilentlyContinue; Write-Host '    -> Perfil de red cambiado a Privada exitosamente.' -ForegroundColor Green } catch { Write-Host '    -> No se pudo cambiar el perfil automaticamente: ' $_.Exception.Message -ForegroundColor Yellow }"

echo [*] 2. Habilitando Deteccion de Redes en el Firewall...
netsh advfirewall firewall set rule group="Network Discovery" new enable=Yes >nul 2>&1
netsh advfirewall firewall set rule group="Deteccion de redes" new enable=Yes >nul 2>&1

echo [*] 3. Agregando reglas de puertos de ReFix...
:: ReFix Overlay Presence (UDP 47690, 47691-47698)
netsh advfirewall firewall delete rule name="ReFix - Overlay Presence (UDP-In)" >nul 2>&1
netsh advfirewall firewall add rule name="ReFix - Overlay Presence (UDP-In)" dir=in action=allow protocol=UDP localport=47690-47698 enable=yes profile=any >nul 2>&1

netsh advfirewall firewall delete rule name="ReFix - Overlay Presence (UDP-Out)" >nul 2>&1
netsh advfirewall firewall add rule name="ReFix - Overlay Presence (UDP-Out)" dir=out action=allow protocol=UDP remoteport=47690-47698 enable=yes profile=any >nul 2>&1

:: ReFix Goldberg Discovery (UDP 47584-47599)
netsh advfirewall firewall delete rule name="ReFix - LAN Discovery (UDP-In)" >nul 2>&1
netsh advfirewall firewall add rule name="ReFix - LAN Discovery (UDP-In)" dir=in action=allow protocol=UDP localport=47584-47599 enable=yes profile=any >nul 2>&1

netsh advfirewall firewall delete rule name="ReFix - LAN Discovery (UDP-Out)" >nul 2>&1
netsh advfirewall firewall add rule name="ReFix - LAN Discovery (UDP-Out)" dir=out action=allow protocol=UDP remoteport=47584-47599 enable=yes profile=any >nul 2>&1

echo [*] 4. Agregando reglas para ejecutables de juegos conocidos...
:: CS:GO
if exist "C:\Users\andre\Games\Counter-Strike_Global Offensive\csgo.exe" (
    netsh advfirewall firewall delete rule name="ReFix - CSGO (In)" >nul 2>&1
    netsh advfirewall firewall add rule name="ReFix - CSGO (In)" dir=in action=allow program="C:\Users\andre\Games\Counter-Strike_Global Offensive\csgo.exe" enable=yes profile=any >nul 2>&1
    netsh advfirewall firewall delete rule name="ReFix - CSGO (Out)" >nul 2>&1
    netsh advfirewall firewall add rule name="ReFix - CSGO (Out)" dir=out action=allow program="C:\Users\andre\Games\Counter-Strike_Global Offensive\csgo.exe" enable=yes profile=any >nul 2>&1
    echo    -> Reglas para CS:GO agregadas.
)

:: Among Us 3D
if exist "C:\Users\andre\Games\Among Us 3D\Among Us 3D.exe" (
    netsh advfirewall firewall delete rule name="ReFix - Among Us 3D (In)" >nul 2>&1
    netsh advfirewall firewall add rule name="ReFix - Among Us 3D (In)" dir=in action=allow program="C:\Users\andre\Games\Among Us 3D\Among Us 3D.exe" enable=yes profile=any >nul 2>&1
    netsh advfirewall firewall delete rule name="ReFix - Among Us 3D (Out)" >nul 2>&1
    netsh advfirewall firewall add rule name="ReFix - Among Us 3D (Out)" dir=out action=allow program="C:\Users\andre\Games\Among Us 3D\Among Us 3D.exe" enable=yes profile=any >nul 2>&1
    echo    -> Reglas para Among Us 3D agregadas.
)

:: Barotrauma
if exist "C:\Users\andre\Games\Barotrauma\Barotrauma.exe" (
    netsh advfirewall firewall delete rule name="ReFix - Barotrauma (In)" >nul 2>&1
    netsh advfirewall firewall add rule name="ReFix - Barotrauma (In)" dir=in action=allow program="C:\Users\andre\Games\Barotrauma\Barotrauma.exe" enable=yes profile=any >nul 2>&1
    netsh advfirewall firewall delete rule name="ReFix - Barotrauma (Out)" >nul 2>&1
    netsh advfirewall firewall add rule name="ReFix - Barotrauma (Out)" dir=out action=allow program="C:\Users\andre\Games\Barotrauma\Barotrauma.exe" enable=yes profile=any >nul 2>&1
    echo    -> Reglas para Barotrauma agregadas.
)

:: refix_steamhost.exe (x86 y x64)
netsh advfirewall firewall delete rule name="ReFix - SteamHost (In)" >nul 2>&1
netsh advfirewall firewall add rule name="ReFix - SteamHost (In)" dir=in action=allow protocol=UDP localport=any enable=yes profile=any >nul 2>&1

echo.
echo ====================================================================
echo [OK] Configuracion de Firewall completada con exito.
echo Nota: Tu amigo (IMasterKMC) tambien debe ejecutar este script en su PC
echo o asegurarse de tener su red Wi-Fi en modo 'Privada'.
echo ====================================================================
pause
