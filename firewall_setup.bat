@echo off
REM =============================================================================
REM ReFix Firewall Rules — Shift At Midnight P2P
REM =============================================================================
REM Run this as Administrator to create firewall rules for P2P multiplayer.
REM =============================================================================

REM Check admin privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] This script requires Administrator privileges.
    echo     Right-click and select "Run as Administrator".
    pause
    exit /b 1
)

set "GAME_EXE=C:\Users\Valen\Desktop\STEAM_CRACKING\DepotDownloader\depots\4704691\24176442\Chameleon\Binaries\Win64\PenguinHotel-Win64-Shipping.exe"

echo.
echo  ====================================
echo   ReFix Firewall Rules — P2P Setup
echo  ====================================
echo.

REM Remove old rules
echo [*] Cleaning up old rules...
netsh advfirewall firewall delete rule name="ReFix - Penguin Hotel (TCP Inbound)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Penguin Hotel (UDP Inbound)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Penguin Hotel (TCP Outbound)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Penguin Hotel (UDP Outbound)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Shift At Midnight (TCP Inbound)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Shift At Midnight (UDP Inbound)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Shift At Midnight (TCP Outbound)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Shift At Midnight (UDP Outbound)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Steam P2P Ports (UDP)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix - Game P2P Ports (UDP)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix Game P2P" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix Game P2P (7777 UDP)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix Game P2P (7778 UDP)" >nul 2>&1
netsh advfirewall firewall delete rule name="ReFix Game P2P (7779 UDP)" >nul 2>&1

echo [*] Creating new rules...

REM Allow game executable — full network access (inbound + outbound)
netsh advfirewall firewall add rule name="ReFix - Penguin Hotel (TCP Inbound)" dir=in action=allow program="%GAME_EXE%" protocol=tcp enable=yes profile=any
netsh advfirewall firewall add rule name="ReFix - Penguin Hotel (UDP Inbound)" dir=in action=allow program="%GAME_EXE%" protocol=udp enable=yes profile=any
netsh advfirewall firewall add rule name="ReFix - Penguin Hotel (TCP Outbound)" dir=out action=allow program="%GAME_EXE%" protocol=tcp enable=yes profile=any
netsh advfirewall firewall add rule name="ReFix - Penguin Hotel (UDP Outbound)" dir=out action=allow program="%GAME_EXE%" protocol=udp enable=yes profile=any

REM Open Steam P2P ports (27015-27020 UDP) inbound + outbound
netsh advfirewall firewall add rule name="ReFix - Steam P2P Ports (UDP)" dir=in action=allow protocol=udp localport=27015-27020 enable=yes profile=any
netsh advfirewall firewall add rule name="ReFix - Steam P2P Ports Out (UDP)" dir=out action=allow protocol=udp remoteport=27015-27020 enable=yes profile=any

REM Open game P2P ports (7777-7779 UDP) inbound + outbound
netsh advfirewall firewall add rule name="ReFix - Game P2P Ports (UDP)" dir=in action=allow protocol=udp localport=7777-7779 enable=yes profile=any
netsh advfirewall firewall add rule name="ReFix - Game P2P Ports Out (UDP)" dir=out action=allow protocol=udp remoteport=7777-7779 enable=yes profile=any

echo.
echo  ====================================
echo   Firewall Rules Created:
echo  ====================================
echo.
echo   [1] PenguinHotel-Win64-Shipping.exe — TCP/UDP Inbound  (all ports)
echo   [2] PenguinHotel-Win64-Shipping.exe — TCP/UDP Outbound (all ports)
echo   [3] Steam P2P: UDP 27015-27020 Inbound + Outbound
echo   [4] Game P2P:  UDP 7777-7779   Inbound + Outbound
echo.
echo   NOTA: El firewall del PC ya quedo abierto.
echo   Si amigos no pueden conectarse, tambien deben abrir
echo   el puerto UDP 7777 en su ROUTER (port-forward).
echo.
echo   Para eliminar todas las reglas despues:
echo     netsh advfirewall firewall delete rule name="ReFix"
echo.
pause
