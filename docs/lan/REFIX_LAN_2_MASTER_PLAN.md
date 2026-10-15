# ReFix LAN 2.0 — Memoria Técnica y Plan Maestro de Arquitectura

**Versión del Documento:** 2.0.0  
**Fecha:** 15 de Octubre 2026  
**Rama Activa:** `refix_lan_infrastructure` (Target de entrega: `origin/feature/phase3-sockets`)  
**HEAD Base Verificado:** `6bb62a9a864b2b0e8a16d45df917ffe42ac20734`  
**Checkpoint Histórico Baseline:** `7ef361a` (`feat(refix-lan): implement ReFix Universal LAN Transport and Core architecture`)  
**Estado del Working Tree:** Implementado, verificado y certificado E2E multi-motor  
**Fase Actual:** FASE 2 / 3 — Auditoría E2E Multi-Engine y Corrección Universal LAN (COMPLETADA)  

---

## 1. Filosofía e Invariantes Innegociables del Proyecto

ReFix LAN 2.0 es una **infraestructura universal de compatibilidad multijugador local, autónoma y verificable**. No es una colección de hacks o parches individuales por juego ni un fork dependiente de motores gráficos específicos.

### Invariantes Fundamentales:
1. **Autonomía Total (Internet-Zero):** LAN/Offline no requiere Steam instalado, `Steam.exe`, cuentas de usuario, autenticación remota ni salida a Internet. Tráfico WAN, SDR de Valve y multicast global son bloqueados estrictamente en el egress del socket.
2. **Descubrimiento LAN Automático:** Dos o más instancias en la misma red local se descubren mutuamente mediante multicast (`239.255.71.84:47584..47599`) y broadcast sin configuración manual de IPs ni port-forwarding.
3. **Transporte Local Real:** Paquetes, sesiones y P2P fluyen por endpoints IPv4 locales (RFC 1918, APIPA, Loopback) con motor ARQ confiable (secuenciación monotónica, ACKs acumulativos, SACK, reensamblado de fragmentos).
4. **Compatibilidad Genérica:** Cero IPs hardcodeadas, AppIDs especiales o esperas arbitrarias. Comportamiento fiel al contrato semántico de las APIs emuladas.
5. **Aislamiento Estricto Online / LAN:** Las rutas de ejecución Online (passthrough a DLL de Valve) y LAN/Offline permanecen completamente desacopladas sin contaminación cruzada.
6. **Multi-instancia Concurrente en 1 PC:** Capacidad de ejecutar múltiples instancias del juego en el mismo equipo (directorios separados) con slots automáticos (`0..15`), identidades (`PeerId` 128-bit derivado de MachineId + PID, SteamID offseteado por slot, nombres limpios con deduplicación dinámica en colisión), puertos unicast efímeros independientes y sockets de transporte aislados.
7. **Pruebas Reproducibles Basadas en Evidencia:** Cada afirmación técnica se sustenta en resultados de ejecución verificables (`BUILD PASS`, `UNIT PASS`, `MULTIPROCESS PASS`, `INTERNET-ZERO PASS`).

---

## 2. Arquitectura Objetivo por Capas

```text
+-----------------------------------------------------------------------------------+
|                                   Juego / Motor                                   |
|                         (Unreal Engine, Unity, Godot, etc.)                       |
+-----------------------------------------------------------------------------------+
                                          |
                                          v  [Llamadas ABI / Interfaces Exportadas]
+-----------------------------------------------------------------------------------+
|                             Capa de Adaptadores ReFix                             |
|  - steam_api64.dll / steam_proxy.cpp (Interceptor, Export Table & Flat C-APIs)    |
|  - unreal_steam_emu.cpp (Emulador Steamworks vtables + C-APIs)                    |
|  - steam_sockets_flat.cpp (SteamNetworkingSockets Flat C-API y structs canónicos) |
|  - EOSSDK-Win64-Shipping.dll / RedboneEOS.dll (Emulador Epic Online Services v3)  |
|  - steam_p2p_hook.cpp (MinHook Winsock: Redirección P2P UDP & Port Virtualization)|
+-----------------------------------------------------------------------------------+
                                          |
                                          v  [Contrato Directo C++: ILanCore]
+-----------------------------------------------------------------------------------+
|                                  ReFix LAN Core                                   |
|                             (src/lan_core/refix_lan_core)                         |
|                                                                                   |
|  • LocalIdentityService: MachineId, PeerId (128-bit UUID PID-scoped), DisplayName |
|  • PeerRegistry: Multi-índice bidireccional (PeerId <-> ExtId <-> Endpoint)       |
|  • DiscoveryService: Beacons multicast/broadcast, queries, responses, heartbeat   |
|  • LobbyService: Replicación autoritativa del host, revisiones, metadatos         |
|  • MatchmakingService: Consultas filtradas (EQUAL, CONTAINS, ANY_OF)              |
|  • CallbackDispatcher: Cola reentrante thread-safe despachada en hilo principal   |
|  • SessionService: Ciclo de vida de sesiones y validación de participantes         |
|  • LocalRelayService: Abstracción de envío directo / fallback local              |
+-----------------------------------------------------------------------------------+
                                          |
                                          v  [Contrato C++: ILanTransport]
+-----------------------------------------------------------------------------------+
|                          ReFix Universal LAN Transport                            |
|                          (src/lan_core/refix_lan_transport)                       |
|                                                                                   |
|  • Reactor Doble Socket: Puerto Multicast/Broadcast (47584..47599)                |
|                          + Puerto Unicast Efímero Port 0 (Datos)                  |
|  • Framing Canónico Wire: Magic 0x52464958 ('RFIX'), Version 1, 36 bytes header  |
|  • Motor ARQ de Confiabilidad: Secuencias monotónicas, ACK + SACK mask            |
|  • Aritmética Modular RFC 1982: Wrap de 32-bit en número de secuencia y sesión    |
|  • Control de Fragmentación y Reensamblado: Bounded memory budgets (4 MiB max)    |
|  • Firewall Internet-Zero Centralizado: Filtrado RFC1918 / Loopback / APIPA       |
+-----------------------------------------------------------------------------------+
                                          |
                                          v  [Winsock OS / Red Física]
+-----------------------------------------------------------------------------------+
|                           Red Local / Localhost Loopback                          |
+-----------------------------------------------------------------------------------+
```

---

## 3. Diagnóstico y Certificación Multi-Motor E2E

### 3.1 Matriz de Validación de Juegos Reales

| Juego / Motor | Sintomatología Previa | Causa Raíz Identificada | Corrección Implementada | Estado |
| :--- | :--- | :--- | :--- | :--- |
| **Shift At Midnight** (Unity / Fusion) | Control positivo | Ninguna regresión detectada | Conservado intacto; deduplicación de callback 505 y preservación de CSteamID canónico. | **VERIFIED PASS** |
| **How to Fish** (Unity / FishNet / Steam Relay) | Desconexión en handshake; `ConnectP2P: Invalid identity type=0` | Funciones planas C `SteamAPI_SteamNetworkingIdentity_*` y `SteamAPI_SteamNetworkingIPAddr_*` no mapeadas en `s_lanExports` en `steam_proxy.cpp`, cayendo en stub passthrough. | Implementadas 40+ funciones en `steam_sockets_flat.cpp` y enlazadas directamente en `s_lanExports` en `steam_proxy.cpp`. | **VERIFIED PASS** |
| **Machine Party** (Godot / SteamMultiplayerPeer) | Conexión se inicia pero se cuelga en handshake; host descarta `RfxsConnectAccept` | `LanEvent` no preservaba el `LanEndpoint` del remitente, impidiendo a `SendRfxsPacket` conocer la IP/puerto del cliente en el primer paquete; además, la búsqueda de lobbies solo consideraba al owner. | Se agregó `LanEndpoint endpoint` a `LanEvent`, se pobló en `OnInboundData`, se guardó `remoteEndpoint` en `ReFixConnection`, y se extendió la búsqueda a `allLobbies.members`. | **VERIFIED PASS** |
| **GRAIN ROT** (Unreal Engine) | Instancias no se encuentran; vaciado a 0 salas en 50ms; violación de acceso `0xC0000005` al despachar callbacks | 1) `DrainPendingSearches` vaciaba la búsqueda a 0 salas de inmediato si los paquetes broadcast aún estaban en tránsito. 2) Fallback SEH llamaba a `vtable[1]` en lugar de `vtable[0]` en callbacks auth (168/163). | 1) Espera de hasta 1500ms en `DrainPendingSearches` mientras no haya matches y la búsqueda esté pendiente. 2) Invocación con `hAPICall=0` y fallback con captura SEH en `SafeCallRun`. | **VERIFIED PASS** |
| **MECCHA CHAMELEON** (Unreal Engine + EOS) | Peers se ven pero falla JoinLobby/JoinSession; no hay transporte P2P | 1) Derivación de `RealAppId` usaba hash del nombre de jugador (`Player1_Host` vs `Player2_Client`), generando AppIDs distintos. 2) Endianness invertida en `HostAddress.Ipv4` (network vs host byte order). | 1) Normalización de AppID vía `steam_appid.txt` o hash del ejecutable real. 2) Inversión con `_byteswap_ulong` antes de invocar `ReFix_RegisterP2PPeer`. | **VERIFIED PASS** |

### 3.2 Estado de Verificación y Pruebas Ejecutadas
| Arnés de Prueba | Comando | Nivel de Certificación | Resultado | Evidencia / Observaciones |
| :--- | :--- | :--- | :--- | :--- |
| **Compilación General** | `build.bat` | `BUILD PASS` | **PASS** | Todas las DLLs y ejecutables compilan y se sincronizan a `bin/` y carpetas de despliegue sin advertencias ni errores. |
| **LAN Core & Transport** | `bin/test_refix_lan_core.exe` | `UNIT PASS` | **PASS** | 100% de suites pasadas (ARQ, reordenamiento, SACK, presupuestos de memoria, wrap 32-bit RFC 1982, reactores reales, TTL 10s). |
| **Transport Ownership** | `bin/test_transport_ownership.exe` | `UNIT PASS` | **PASS** | H-01 verificado: Ownership seguro y libre de fugas o dobles destrucciones (0 fallos). |
| **Concurrency Contract** | `bin/test_lan_concurrency_contract.exe` | `CONCURRENCY PASS` | **PASS** | 4/4 suites pasadas, 54/54 aserciones verificadas sin condiciones de carrera. |
| **Steamworks -> LanCore Adapter** | `bin/test_steam_lancore_adapter.exe --standalone` | `UNIT PASS` | **PASS** | Aislamiento EAT, CSteamID canónico, rollback preservation, async dispatch de lobbies certificados. |
| **ISteamNetworkingSockets LanCore**| `bin/test_steam_sockets_lancore.exe` | `UNIT PASS` | **PASS** | 8/8 suites pasadas, 61/61 aserciones verificadas en flat C-API e interfaces orientadas a conexión. |
| **Multiproceso Dual Sockets** | `tests/test_steam_sockets_multiprocess.ps1` | `MULTIPROCESS PASS`| **PASS** | Host/Client en procesos independientes: handshake, sockets virtuales, bidireccionalidad E2E verificada. |
| **Multiproceso Dual E2E** | `tests/test_steam_adapter_multiprocess.ps1` | `MULTIPROCESS PASS` | **PASS** | Host y Client en procesos independientes en 1 PC: descubrimiento mutuo, unión a lobby, ping/pong ch0, fragmentación 8192B ch1, teardown limpio. |
| **Local Multi-Instance Mesh** | `tests/test_multi_instance_mesh.ps1` | `MESH PASS` | **PASS** | Malla local de 3 nodos independientes con descubrimiento cruzado sin colisiones de identidad. |
| **Internet-Zero Firewall** | `bin/refix_net_test.exe isolation` (Suite 10) | `INTERNET-ZERO PASS` | **PASS** | 6/6 destinos WAN y multicast global bloqueados en el egress; subredes LAN/Loopback permitidas; cero fugas. |
| **Loopback Adapter Suites 1-4, 9-11, 13** | `bin/refix_net_test.exe <suite>` | `UNIT PASS` | **PASS** | ABI, VTable routing, Zero Fallback, 100 conexiones simultáneas sin colisión, lifetime de mensajes, fault injection, bootstrap. |

---

## 4. Preservación de Invariantes y Contratos

1. **Invarianza Estricta de Suites 5-8:**
   - `src/network/refix_net_test.cpp` no ha sido modificado en absoluto (0 líneas alteradas). Las suites 5-8 y `AcquireInstanceSlot` permanecen íntegras según lo requerido en la especificación rectora.
2. **Entrega No Destructiva de Ramas:**
   - La rama de trabajo `refix_lan_infrastructure` coincide en su ancestro con `origin/feature/phase3-sockets` (`6bb62a9`), permitiendo entregar la totalidad de los cambios directamente a `origin/feature/phase3-sockets` mediante fast-forward push.
3. **Despliegue a Juegos:**
   - Los binarios actualizados (`steam_api64.dll`, `EOSSDK-Win64-Shipping.dll`, `RedboneEOS.dll`, `winmm.dll`) han sido desplegados y verificados en las 10 instancias activas bajo `D:\Games\`.
