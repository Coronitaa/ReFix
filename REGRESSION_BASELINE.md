# REGRESSION BASELINE: WAN Connectivity Regression in ReFix

## Identificadores de Versión
* **Baseline funcional (v1.23):** Commit `2341927` (`23419271eaa97965f070e8534d1b6c5437896bad`)
* **Versión bajo análisis (v1.3.1):** Commit `577f1bc` (`577f1bc379df03d8ea2302aa361167904cb9d013`)
* **Rama de trabajo dedicada:** `fix/wan-regression-v1.3.1`
* **Worktree de referencia:** `d:\EOS_REFIX\ReFix-v1.23`

---

## Síntomas Conocidos
1. **Regresión grave de conectividad WAN** tras v1.23: los juegos ya no logran conectarse entre sí a través de Internet (WAN).
2. **Afecta a múltiples motores:** Ocurre tanto en juegos desarrollados sobre **Unity** como sobre **Unreal Engine** que utilizan UDP/Steam multiplayer y NO dependen de RedboneEOS.
3. **Steam Overlay y Spacewar intactos:** El overlay de Steam se inicializa correctamente y la emulación/uso de Spacewar (AppID 480) arranca sin problemas aparentes, descartando fallos triviales de crash al arranque de SteamAPI.
4. **Comportamiento en LAN:** Puede parecer que funciona en LAN o en ciertos entornos locales si el fallback Winsock original alcanza IPs locales directamente, pero falla en WAN.

---

## Archivos Candidatos
* `src/steam_p2p_hook.cpp`: Lógica de intercepción de Winsock (`sendto`, `recvfrom`, `select`, `bind`, `connect`, etc.), enrutamiento (`RouteIncomingPacket`), `SocketContext`, clasificación de puertos (`ClassifyPort`), cola de recepción y hold buffer.
* `src/steam_p2p_hook.h`: Definiciones de estructuras de socket, colas, paquetes y firmas de hooks.
* `src/steam_proxy.cpp`: Inicialización de SteamAPI (`SteamAPI_Init`, `SteamAPI_InitFlat`, `SteamInternal_SteamAPI_Init`), vtables de Steam Networking/Matchmaking, filtros de lobby, gating de instalación del hook.
* `src/unae/unae.cpp`, `src/unae/capability_detector.cpp`, `src/unae/cascade_arbiter.cpp`: Lógica de Universal Network Arbitration Engine v3.0, en particular `UNAE::IsDirectP2PAllowed()` y detección de topología de red.
* `bin/deploy_helper.ps1` & `AutoDeploy.bat`: Configuración generada en `ReFix.ini`, parches de ensamblados (Cecil IL rewriting), modos Goldberg vs Valve/Online.

---

## Hipótesis Iniciales

### Hipótesis 1: Gate condicional de `SteamP2PHook::Install` (UNAE::IsDirectP2PAllowed)
En `src/steam_proxy.cpp` (v1.3/v1.3.1), la llamada a `SteamP2PHook::Install()` fue condicionada a `UNAE::IsDirectP2PAllowed()`. Si UNAE no detecta de forma inmediata o marca deshabilitado el P2P directo para títulos Unity o Unreal sin EOS, los hooks de Winsock **nunca se instalan**, haciendo que todo el tráfico UDP del juego intente salir por la interfaz física a IPs privadas de LAN no enrutables en WAN.

### Hipótesis 2: Pérdida o mala entrega de paquetes en `RouteIncomingPacket()`
En v1.23 existía una cola global o directa de paquetes P2P entrantes consumida por cualquier socket que hiciera `recvfrom()`. En v1.3/v1.3.1 se introdujo una arquitectura per-socket (`SocketContext`, `recvQueue`, `ClassifyPort`), enrutando paquetes según coincidencia estricta de puerto o servicio. Si el juego abre múltiples sockets, utiliza puertos efímeros, o el `dstPort` en el paquete de Steam P2P no coincide con el puerto local conocido del socket (o es 0), el paquete se descarta o se asigna a una cola que el socket activo nunca lee.

### Hipótesis 3: Ruptura de transmisión y fallback en `Hook_sendto()` / Hold Buffer
En v1.23, `Hook_sendto()` tenía un mecanismo simple donde si había un peer de Steam conocido (o un único peer en la sesión de lobby), se enviaba el paquete vía Steam P2P incluso sin mapping previo de IP. En v1.3/v1.3.1, la introducción de envelopes, hold buffer de 3 segundos y posterior modificación en `577f1bc` (fallthrough directo a `g_orig_sendto` tras el hold buffer) puede estar derivando paquetes handshake a Winsock nativo en lugar de redirigirlos por Steam P2P.

### Hipótesis 4: Inconsistencia con `select()` / `WSARecv`
Muchos motores (como Unity con transportes como Mirror, Telepathy, FishNet, o Unreal SocketSubsystem) usan `select()` para verificar si un socket UDP tiene datos legibles antes de llamar a `recvfrom()`. Si `Hook_select()` no reporta legible el socket cuando hay datos en su `recvQueue` virtual (o si `Hook_select()` no consulta las colas de todos los sockets potenciales), el juego nunca invoca `recvfrom()` y los paquetes se estancan.

### Hipótesis 5: Despliegue, modo Goldberg vs Online y AppID Masking
Sospecha sobre `deploy_helper.ps1`: generación de configuración errónea, reescritura de IL que desactiva Steamworks.NET o altera la detección de backend, dejando al juego en un modo híbrido inconsistente.
