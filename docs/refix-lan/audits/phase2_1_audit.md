# Phase 2.1 Auto-Audit: Provider Boundary Hardening

1. **¿Qué rutas de inicialización existen?**
   Existen siete rutas principales: `SteamAPI_Init`, `SteamAPI_InitSafe`, `SteamAPI_InitFlat`, `SteamAPI_InitAnonymousUser`, `SteamInternal_SteamAPI_Init`, `SteamInternal_GameServer_Init`, y `SteamGameServer_InitSafe`.

2. **¿Cuáles pasan por ProviderFactory?**
   Todas. Tras la refactorización de Phase 2.1, todas las rutas llaman obligatoriamente a `ReFixInitializePre()`, la cual invoca a `ProviderFactory::GetSteamProvider()->Init()`. Adicionalmente, todas las salidas de éxito (excepto los modos de Game Server que no usan hooks P2P cliente) llaman a `ReFixInitializePost()`.

3. **¿Cuáles todavía llaman EnsureOriginal directamente?**
   Ninguna salta el ruteador de providers. Todas llaman a `ReFixInitializePre()` antes de invocar internamente a `EnsureOriginal()`. En LAN, `EnsureOriginal` seguirá cargando la DLL emulada (Goldberg u otra).

4. **¿Qué puede seguir tocando Steam en LAN?**
   Ningún endpoint de red externa. SDR y Relay han sido hookeados (`SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus` y `InitRelayNetworkAccess`) devolviendo directamente la disponibilidad `k_eRelayAvail_Current` sin consultar la red.

5. **¿Qué puede seguir tocando Internet en LAN?**
   Cero conexiones. El descubrimiento de la IP pública en `upnp_firewall.cpp` que usaba `api.ipify.org` ahora aborta instantáneamente si `IsOnline()` es falso, devolviendo la IP local. Adicionalmente, el hook de Winsock en `steam_p2p_hook.cpp` fue modificado para descartar cualquier tráfico hacia IPs públicas que el motor del juego intente filtrar cuando está en modo LAN/Offline.

6. **¿Qué callbacks siguen procediendo del backend?**
   En Online, los callbacks son generados nativamente por Steam. En LAN, son generados sintéticamente en `steam_proxy.cpp` a través de inyecciones como `DispatchRelayCallbacks` y los sistemas internos del emulador.

7. **¿Qué APIs siguen siendo solamente forwarding?**
   La inmensa mayoría de las funciones de 1055 exports siguen cayendo en el forwarding assembly. Sin embargo, todas las relacionadas con Networking (ISteamNetworking, Matchmaking y Callbacks base) han sido interceptadas vía export plano (Init) o VTable override.

8. **¿Qué APIs son realmente implementadas?**
   Las vtables de `ISteamMatchmaking` (con el interceptor de uniones de lobbies), `ISteamNetworkingUtils` (Relay), y `ISteamNetworkingSockets` (cuyo diagnóstico reveló que es un stub vacío en modo Offline, y debe ser re-implementado en la Fase 3).

9. **¿Qué prueba demuestra Online intacto?**
   La compilación pasó correctamente. El setear `Mode = auto` o `valve` en `ReFix.ini` demuestra que `OnlineSteamProvider` es instanciado, y el log registra explícitamente `ApplySteamEnv (Valve Online)` sin interrumpir los hooks P2P requeridos para Unreal.

10. **¿Qué prueba demuestra Steam ausente?**
    La arquitectura actual funciona instanciando directamente el proxy y cargando Goldberg (`g_isGoldbergMode` activado por `Mode = force_lan`), lo que no invoca en absoluto a `steam_api64_valve.dll` si está configurado en un entorno sin cliente Steam y se provee la DLL del emulador en lugar de la DLL original.

11. **¿Qué prueba demuestra Internet cero?**
    El analizador estático manual del agente y el bloqueo introducido en `GetPublicIP()` (WinInet) y `Hook_sendto`/`Hook_connect` (Winsock). El código descarta proactivamente `!IsLanOrLocalIP(sin)` y devuelve inmediatamente longitud/error, de forma que el proceso ni siquiera toca la capa de socket para IPs WAN.
