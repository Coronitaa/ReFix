# ROOT CAUSE ANALYSIS: WAN Connectivity Regression in ReFix (Post-v1.23)

## 1. Síntoma
Aparición de una regresión grave y total de interconexión multijugador por Internet (WAN) tras ReFix v1.23 (commit `2341927`).
- **Afectación multi-motor:** Ocurre tanto en juegos desarrollados en **Unity** (Mirror, FishNet, Telepathy, KCP, LiteNetLib) como en **Unreal Engine** (UE4/UE5 con `UIpNetDriver`) que utilizan UDP / Steam multiplayer y NO dependen de RedboneEOS.
- **Servicios de Steam intactos:** El Steam Overlay funciona correctamente, Spacewar (AppID 480) arranca con normalidad, la autenticación de usuario y los lobbies de Steamworks (`ISteamMatchmaking`) se crean y se unen con éxito.
- **Momento del fallo:** La conexión se congela o aborta en cuanto el motor de juego intenta iniciar el intercambio de datagramas UDP para la sincronización de la partida.

---

## 2. Primera Versión Afectada y Commits Relevantes
- **Línea Base Funcional:** v1.23 (commit `23419271eaa97965f070e8534d1b6c5437896bad`)
- **Primera Versión Afectada:** v1.3.0 (commit `e68bd5443f65ba10fedf9226e69588991544726e`)
- **Versión Bajo Análisis:** v1.3.1 (commit `577f1bc379df03d8ea2302aa361167904cb9d013`)
- **Commits Clave en la Cadena de Regresión:**
  1. `0375bdf` (*feat(p2p): enable universal winsock p2p hook for unity...*): Habilitó la instalación de `SteamP2PHook` en Unity sin aislar modelos de sockets efímeros ni I/O no bloqueante.
  2. `e68bd54` (*feat(unae): Implement Universal Network Arbitration Engine v3.0...*): Reescribió completamente `src/steam_p2p_hook.cpp`, introduciendo `SocketContext`, colas por socket, el encabezado `ReFixP2PHeader` (12 bytes), clasificación rígida `ClassifyPort`, y compuerta `UNAE::IsDirectP2PAllowed()`.
  3. `577f1bc` (*fix(proxy): fix Steam Relay/GetAppID/UDP holdBuffer for FishNet...*): Eliminó la instrucción `return len;` en el Pre-Lobby Hold Buffer de `Hook_sendto`, provocando fuga masiva a Winsock crudo, doble emisión de datagramas y errores `WSAECONNRESET` / `WSAEHOSTUNREACH`.

---

## 3. Comportamiento Antiguo (v1.23) vs Comportamiento Nuevo (v1.3.1)

| Aspecto | Comportamiento en v1.23 | Comportamiento en v1.3.1 |
| :--- | :--- | :--- |
| **Colas de Recepción** | Cola global única `g_recvQueue` (`std::deque<RecvPacket>`). Cualquier socket UDP que ejecutaba `recvfrom()` consumía los paquetes P2P disponibles. | Colas por socket en `SocketContext::recvQueue`, más dos colas globales de huérfanos (`g_pendingGamePackets` y `g_pendingVoicePackets`). |
| **Notificación en `select()`** | Si `!g_recvQueue.empty()` y `result <= 0`, restauraba incondicionalmente todos los descriptores en `readfds` y retornaba `fd_count`. El motor siempre leía los datos. | Solo marca el socket si está en `g_socketContexts` Y tiene `serviceType == SocketServiceType::Game`. Sockets efímeros o no clasificados son ignorados. |
| **Sockets Efímeros (`bind(0)`)** | No se hookeaba `bind()`. `recvfrom()` entregaba paquetes sin requerir conocimiento previo del puerto local. | `Hook_bind()` almacena `ctx.localPort = 0` y `serviceType = Unknown`. No llama a `getsockname()`. Los paquetes nunca se asocian al socket. |
| **Framing de Red P2P** | Paquetes crudos directos en Canal 0 (`k_EP2PSendUnreliable`). Cero sobrecarga de protocolo. | Cabecera binaria `ReFixP2PHeader` de 12 bytes (`magic: 0x58464552`). Incompatible con receptores que no parsean el envelope. |
| **IPs Sintéticas de Pares** | Unificada en `127.0.0.1 \| (SteamID & 0x00FFFFFF)` tanto en recepción como en `recvfrom`. | `P2PPumpStep` genera `127.x.x.x`, pero `UpdateP2PPeers` genera `10.x.x.x`, borrando la entrada `127.x.x.x` de `g_ipToSteamID`. |
| **Retención Pre-Lobby** | Sin retención; paquetes sin par mapeado caían a Winsock crudo. | Buffer con TTL de 3000 ms, pero en `577f1bc` se eliminó `return len;`: el paquete se copia al buffer y **se envía de inmediato a Winsock crudo**, y se retransmite al expirar el TTL. |
| **Clasificación de Puertos** | Agnóstica: si el puerto era 7777, 7778, 27015 o había 1 par, redirigía a P2P. | Rígida (`ClassifyPort`): solo 7770-7799, 27015-27035, 5055-5056. Cualquier otro puerto es `Unknown`. |
| **Alcance de Motores** | `SteamP2PHook` solo se instalaba si `g_unrealIsEngine == true`. | Se instala para todos los motores bajo la condición `UNAE::IsDirectP2PAllowed()`. |

---

## 4. Evidencia Concreta de Código

### A. Deadlock en `Hook_select()` (`src/steam_p2p_hook.cpp:868-885`)
```cpp
868:     if (hasInRead) {
869:         std::lock_guard<std::mutex> lg(g_socketMutex);
870:         for (u_int i = 0; i < inRead.fd_count; ++i) {
871:             SOCKET s = inRead.fd_array[i];
872:             auto it = g_socketContexts.find(s);
873:             if (it != g_socketContexts.end() && !it->second.recvQueue.empty()) {
874:                 FD_SET(s, &readySet);
875:                 p2pReadyCount++;
876:             } else if (!g_pendingGamePackets.empty() || !g_pendingVoicePackets.empty()) {
877:                 if (it != g_socketContexts.end()) {
878:                     if ((it->second.serviceType == SocketServiceType::Game && !g_pendingGamePackets.empty()) ||
879:                         (it->second.serviceType == SocketServiceType::Voice && !g_pendingVoicePackets.empty())) {
880:                         FD_SET(s, &readySet);
881:                         p2pReadyCount++;
882:                     }
883:                 }
884:             }
885:         }
886:     }
```
*Impacto:* Si `it == g_socketContexts.end()` o `it->second.serviceType == SocketServiceType::Unknown`, el socket no se marca en `readySet`. La función llama a `g_orig_select`, que retorna 0 por timeout (los datos están en memoria P2P, no en el socket nativo). El motor del juego nunca invoca `recvfrom(s)`.

### B. Fallo de Sockets Efímeros en `Hook_bind()` (`src/steam_p2p_hook.cpp:943-952`)
```cpp
943:         // Successful normal bind
944:         {
945:             std::lock_guard<std::mutex> lg(g_socketMutex);
946:             auto& ctx = g_socketContexts[s];
947:             ctx.socket = s;
948:             ctx.localPort = requestedPort; // Si requestedPort == 0, almacena 0!
949:             ctx.serviceType = (service != SocketServiceType::Unknown) ? service : ClassifyPort(requestedPort);
950:             ctx.lastActivity = GetTickCount();
951:         }
```
*Impacto:* Cuando el socket cliente enlaza con puerto 0, `ctx.localPort` permanece en 0 y `serviceType` en `Unknown`. No se ejecuta `getsockname()` para obtener el puerto real asignado por el sistema.

### C. Bloqueo de Drenaje en `Hook_recvfrom()` (`src/steam_p2p_hook.cpp:767-775`)
```cpp
767:         } else {
768:             auto& ctx = it->second;
769:             if (ctx.serviceType == SocketServiceType::Game && !g_pendingGamePackets.empty()) {
770:                 while (!g_pendingGamePackets.empty() && ctx.recvQueue.size() < SocketContext::kMaxQueueSize) {
771:                     ctx.recvQueue.push_back(std::move(g_pendingGamePackets.front()));
772:                     g_pendingGamePackets.pop_front();
773:                 }
```
*Impacto:* Como `ctx.serviceType == Unknown`, la línea 769 es falsa. Los paquetes en `g_pendingGamePackets` nunca son transferidos a `ctx.recvQueue`. La función cae a `g_orig_recvfrom`, que retorna `WSAEWOULDBLOCK`.

### D. Fuga a Winsock Crudo y Doble Emisión en Hold Buffer (`src/steam_p2p_hook.cpp:693-714`)
```cpp
693:         if (service != SocketServiceType::Unknown || IsGamePort(destPort)) {
694:             std::lock_guard<std::mutex> lg(g_holdBufferMutex);
695:             if (g_holdBuffer.size() < k_maxHoldBufferSize) {
696:                 BufferedHoldPacket hpkt;
...
707:                 g_holdBuffer.push_back(std::move(hpkt));
708:             }
709:         }
710:     }
711: 
712:     // Fall through to real Winsock for raw packets
713:     return g_orig_sendto(s, buf, len, flags, to, tolen);
```
*Impacto:* En commit `577f1bc` se eliminó `return len;` en la línea 707. El paquete se encola en `g_holdBuffer` y simultáneamente cae a `g_orig_sendto`. En WAN, la emisión física cruda falla hacia IPs privadas o puertos cerrados, generando `WSAEHOSTUNREACH` o `WSAECONNRESET`. Al expirar el TTL (línea 532), se envía por segunda vez a `g_orig_sendto`.

### E. Desincronización de IPs Sintéticas entre Submódulos
- En `src/steam_p2p_hook.cpp:577`:
  `uint32_t syntheticIP = 0x7F000001u | (uint32_t)(fromID & 0x00FFFFFFu);` (`127.x.x.x`).
- En `src/steam_proxy.cpp:1551`:
  `ip4 = 0x0A000001u | (uint32_t)(memberID & 0x00FFFFFFu);` (`10.x.x.x`).
- En `src/steam_p2p_hook.cpp:1104-1108`:
  `RegisterPeer` elimina la entrada anterior (`127.x.x.x`) de `g_ipToSteamID`. Cuando el motor del juego responde a la IP aprendida en `recvfrom` (`127.x.x.x`), `Hook_sendto` ya no puede resolver el SteamID.

### F. Sobrescritura de la IP del Host en el Bucle de Miembros (`src/steam_proxy.cpp:1535-1558`)
```cpp
1535:     for (int i = 0; i < count; i++) {
1536:         uint64_t memberID = g_pfn_GetLobbyMemberByIndex(matchmaking, lobbyID, i);
...
1542:         if (serverIP && serverIP[0] != '\0') {
1543:             struct in_addr addr;
1544:             if (inet_pton(AF_INET, serverIP, &addr) == 1) {
1545:                 ip4 = ntohl(addr.s_addr); // ASIGNADO A TODOS LOS MIEMBROS!
1546:             }
1547:         }
...
1556:         SteamP2PHook::RegisterPeer(memberID, ip4, lobbyPort);
```
*Impacto:* `serverIP` (la dirección del host) se asocia al último miembro del lobby procesado. En partidas multijugador, la IP del host apunta al cliente equivocado en `g_ipToSteamID`.

---

## 5. Cadena Causal y Reproducción
```
1. El juego inicializa SteamAPI (Spacewar 480).
2. El motor crea un socket UDP y ejecuta bind(s, port 0).
   -> Hook_bind almacena localPort = 0 y serviceType = Unknown (omite getsockname).
3. El cliente se une al lobby de Steamworks y aprende la IP del host.
4. El cliente envía el handshake inicial UDP mediante Hook_sendto.
   -> Como el par aún no está registrado en g_ipToSteamID:
      a) El paquete se almacena en g_holdBuffer.
      b) [Regresión 577f1bc]: Cae inmediatamente a g_orig_sendto hacia una IP privada o WAN.
      c) El router descarta el paquete crudo o devuelve ICMP Unreachable -> WSAECONNRESET.
5. El host envía un paquete por Steam P2P hacia el puerto efímero del cliente.
6. El hilo P2PPumpStep en el cliente lee el datagrama por Steam Networking.
   -> RouteIncomingPacket intenta enrutar hacia destPort (ej. 52341).
   -> Como localPort es 0 y serviceType es Unknown, el paquete va a g_pendingGamePackets.
7. El hilo de red del cliente ejecuta select() antes de leer.
   -> Hook_select evalúa el socket: serviceType == Unknown, por lo que ignora g_pendingGamePackets.
   -> Hook_select delega en g_orig_select, que retorna 0 (TIMEOUT) porque el socket nativo está vacío.
8. DEADLOCK: El cliente cree que no hay datos y jamás llama a recvfrom().
9. Al no haber respuesta en T+5000ms, el motor del juego aborta la conexión WAN por timeout.
```

---

## 6. Causas Raíz Identificadas (Resumen Técnico)
1. **Deadlock de `Hook_select`:** Exige pertenencia a `SocketServiceType::Game` para reportar legibilidad cuando hay paquetes en `g_pendingGamePackets`.
2. **Inanición de Sockets Efímeros en `Hook_bind`:** Omitir `getsockname()` al enlazar con puerto 0 deja `localPort = 0` y sella el socket en `Unknown`.
3. **Fuga Cruda y Desincronización del Hold Buffer:** La eliminación de `return len;` en commit `577f1bc` rompió la retención de datagramas previos al registro de pares.
4. **Desincronización de IPs Sintéticas:** Conflicto irreparable entre el rango `127.0.0.0/8` de `P2PPumpStep` y el rango `10.0.0.0/8` de `UpdateP2PPeers`.
5. **Corrupción de Mapeo en Lobbies Multijugador:** `UpdateP2PPeers` asigna la IP del host a todos los clientes del lobby en lugar de aislarla a `ownerID`.
6. **Iteración No Determinista en `RouteIncomingPacket`:** El Paso 3 entrega paquetes al primer socket que devuelva `std::unordered_map`, desbalanceando partidas con múltiples sockets.
7. **Bloqueo Heurístico en Lobbies con Más de Un Par:** Restricción estricta `size() == 1` que anula el fallback P2P en partidas de 3 o más participantes.
8. **Falso Positivo de Topología C en UNAE:** Catalogar `hasPhotonVoice` como Topología C inhabilita `SteamP2PHook::Install()` en juegos con transporte Steam P2P y voz auxiliar.

---

## 7. Por Qué Puede Parecer Funcionar en LAN
En una red de área local (LAN), las dos máquinas comparten el mismo segmento de red (`192.168.1.0/24`). Cuando el paquete cae erróneamente a `g_orig_sendto`, el paquete UDP **viaja físicamente a través del switch o router local y llega a la IP LAN directa de la otra máquina**. El receptor lo procesa mediante el socket Winsock nativo sin requerir Steam P2P. En WAN (Internet), las IPs privadas o públicas tras NAT requieren obligatoriamente la encapsulación SDR de Steam P2P; al filtrarse a Winsock crudo, los paquetes son descartados.

---

## 8. Por Qué el Overlay y Spacewar Pueden Seguir Funcionando
El Steam Overlay se ejecuta inyectándose en las APIs gráficas (DirectX / Vulkan) y comunicándose con `steam.exe` por tuberías IPC nombradas de Windows. Spacewar (AppID 480) y la gestión de lobbies se ejecutan mediante llamadas a `steam_api64_valve.dll` por IPC hacia el cliente local de Steam. Ninguno de estos módulos interactúa con los sockets UDP de `ws2_32.dll`. La infraestructura de Steam funciona al 100%, pero la capa de transporte UDP del juego queda incomunicada.

---

## 9. Corrección Propuesta (Principios de Diseño)
1. **Restaurar Retención Exclusiva en Hold Buffer:** Si un paquete no tiene par mapeado pero califica para juego/voz, encolar en `g_holdBuffer` y **retornar `len` inmediatamente** sin caer a `g_orig_sendto`.
2. **Reparación de Sockets Efímeros en `Hook_bind`:** Si `requestedPort == 0`, ejecutar inmediatamente `getsockname(s)` para registrar el puerto real efímero asignado por Windows.
3. **Eliminar Deadlock en `Hook_select`:** Si `!g_pendingGamePackets.empty()`, marcar como legibles todos los sockets UDP de lectura que no sean explícitamente de voz.
4. **Drenaje Permisivo en `Hook_recvfrom`:** Si `ctx.recvQueue` está vacía y existen paquetes en `g_pendingGamePackets`, transferirlos a `ctx.recvQueue` y promover el socket a `SocketServiceType::Game`.
5. **Unificación Canónica de IPs Sintéticas:** Utilizar una sola subred canónica (`10.0.0.0/8` o `127.0.0.0/8`) en todo el proyecto y mantener alias si una IP cambia.
6. **Aislamiento de `serverIP` en `UpdateP2PPeers`:** Asignar `serverIP` únicamente si `memberID == ownerID`.
7. **Desacoplar `PhotonVoice` de Topología C en UNAE:** Permitir que la presencia aislada de voz no desactive `SteamP2PHook`.

---

## 10. Riesgos de Regresión y Mitigaciones
- **Riesgo:** Reintroducir el timeout en juegos que usan UDP local offline.  
  *Mitigación:* Limitar el Hold Buffer a datagramas dirigidos a puertos de juego conocidos, loopback, o rangos privados comunes donde hay un lobby de Steam activo.
- **Riesgo:** Confusión entre tráfico de voz y juego en multi-socket.  
  *Mitigación:* Mantener la separación estricta por canal de Steam (Canal 0 = Game, Canal 1 = Voice) y canalizar la voz prioritariamente al socket puerto 5058.
