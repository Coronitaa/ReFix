================================================================================
          ReFix LAN 2.0 — ISteamNetworkingSockets & Universal LanCore
================================================================================

Este paquete contiene la versión oficial compilada y certificada de ReFix LAN 2.0,
que integra soporte completo de ISteamNetworkingSockets (SteamSockets) sobre el
transporte canónico Universal LanCore (Canal 250).

Permite jugar juegos que utilizan Steamworks P2P y Steam Sockets en modo LAN u
Offline sin necesidad de tener Steam.exe ejecutándose ni conexión a Internet.

--------------------------------------------------------------------------------
1. COMPATIBILIDAD DE MOTORES (UNREAL ENGINE Y UNITY)
--------------------------------------------------------------------------------
¿Funciona sólo en Unreal Engine o también en Unity?
-> FUNCIONA EN AMBOS (Y EN CUALQUIER MOTOR NATIVO):

1. Unreal Engine 4 / Unreal Engine 5:
   - Soporte total para OnlineSubsystemSteam y SteamSocketsNetDriver.
   - Manejo exacto de hListenSocket en callbacks de conexión.
   - Colas de conexión saliente asíncronas (Connecting queue).
   - Fragmentación y reensamblado automático de paquetes no fiables > 1113 bytes.
   - Enrutamiento transparente por Canal 250 de LanTransport.

2. Unity Engine:
   - Compatible con juegos que usen Steamworks.NET o Facepunch.Steamworks.
   - Al implementar el estándar binario oficial de Steamworks (C vtable y Flat API),
     las llamadas de Unity a ConnectP2P, CreateListenSocketP2P, SendMessages,
     ReceiveMessagesOnConnection, etc., funcionan de forma idéntica y sin modificaciones.

3. Juegos Nativos / Otros Motores (Godot, C++, etc.):
   - Cualquier ejecutable de 64 bits que cargue steam_api64.dll.

--------------------------------------------------------------------------------
2. CÓMO INSTALAR EN UN JUEGO (AUTO-DEPLOY)
--------------------------------------------------------------------------------
Tienes dos formas automáticas y seguras:

OPCIÓN A (La más rápida - Arrastrar y Soltar):
  1. Toma la carpeta donde está instalado tu juego (por ejemplo, "MiJuego").
  2. Arrástrala y suéltala directamente sobre "Deploy_Quick.bat".
  3. El script detectará automáticamente si es Unreal Engine, Unity o Nativo,
     creará un backup de la DLL original (steam_api64_o.dll) y copiará los binarios
     actualizados.

OPCIÓN B (Asistente Universal Interactivo):
  1. Haz doble clic en "AutoDeploy.bat".
  2. Selecciona la opción [2] para abrir el explorador de Windows y elegir
     la carpeta del juego.
  3. El asistente analizará el juego, detectará el AppID y desplegará todo.

--------------------------------------------------------------------------------
3. CÓMO JUGAR EN RED LOCAL (LAN) ENTRE 2 COMPUTADORAS
--------------------------------------------------------------------------------
1. Instala el paquete en el juego en la PC 1 (Host) y en la PC 2 (Cliente).
2. Asegúrate de que ambas PCs estén conectadas a la misma red local (cable o Wi-Fi)
   y que el Firewall de Windows no bloquee el juego (el puerto UDP por defecto es 47584).
3. En la PC 2 (Cliente), edita el archivo:
     steam_settings\force_account_name.txt -> Ponle un nombre diferente (ej: Jugador2)
     steam_settings\force_steamid.txt      -> Ponle otro ID (ej: 76561198000000002)
4. Abre el juego en la PC 1 y crea la partida / lobby.
5. Abre el juego en la PC 2: el descubrimiento multicast automático de LanCore
   encontrará al Host y se conectará mediante SteamSockets por el canal 250.

--------------------------------------------------------------------------------
4. CÓMO PROBAR EN 1 SOLA PC (MULTI-INSTANCIA LOCAL)
--------------------------------------------------------------------------------
Si quieres probar el juego dos veces en la misma PC:
- Crea dos carpetas o copias del juego (o lanza una segunda instancia).
- Para la segunda instancia, debes usar un puerto y SteamID distintos. Puedes:
  a) Editar en la segunda carpeta:
     steam_settings\force_listen_port.txt -> 47586
     steam_settings\force_steamid.txt      -> 76561198000000002
     steam_settings\force_account_name.txt -> Player2
  b) O lanzar la segunda instancia desde un script .bat con variables de entorno:
     set REFIX_LISTEN_PORT=47586
     set REFIX_STEAM_ID=76561198000000002
     set REFIX_STEAM_PERSONA_NAME=Player2
     start "" "Juego.exe"

--------------------------------------------------------------------------------
5. ARCHIVOS INCLUIDOS EN ESTE PAQUETE
--------------------------------------------------------------------------------
- bin\steam_api64.dll           : Proxy Steamworks de producción con Canal 250 y Sockets.
- bin\EOSSDK-Win64-Shipping.dll : Emulador de Epic Online Services (ReFix EOS).
- bin\RedboneEOS.dll            : Backend de enlace EOS a LanCore.
- bin\winmm.dll                 : Loader y overlay del buscador de servidores (F1/Insert).
- ReFix.ini                     : Archivo de configuración central.
- steam_appid.txt               : Identificador de aplicación Steam (por defecto 480).
- steam_settings\               : Configuración de nombre, SteamID y puertos LAN.
- Deploy_Quick.bat              : Despliegue rápido por arrastrar y soltar.
- AutoDeploy.bat                : Asistente gráfico universal de autodespliegue.

================================================================================
ReFix LAN 2.0 — Listo para usar.
================================================================================
