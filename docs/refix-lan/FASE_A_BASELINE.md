# ReFix LAN/Offline — Informe de Baseline de la Fase A

**Fecha:** 8 de Octubre de 2026  
**Rama:** `feature/phase3-sockets`  
**Commit Inicial:** `2f46591`  
**Base Común con v1.3.3:** `1c6218c`  
**Estado:** Baseline Audit Completado (12/12 suites de test unitario ejecutadas exitosamente)

---

## 1. Resumen Ejecutivo del Estado Actual

La rama `feature/phase3-sockets` contiene una implementación avanzada de emulación de sockets para Steamworks (`ISteamNetworkingSockets`) y un arnés de pruebas de 12 suites (`refix_net_test.exe`) con simulación de fallos (`FaultInjector`). Sin embargo, el estado actual adolece de cuatro problemas fundamentales:

1. **Fragmentación y Triplicación del Transporte:**
   - Coexisten tres stacks independientes:
     * `src/unreal_steam_emu.cpp`: Protocolo RFIX (`0x52464958`), socket UDP crudo con ventana deslizante pero dependiente del loop de render (`RunCallbacks()`), sin hilo reactor de I/O, inundando con broadcasts `255.255.255.255` en `SendP2PPacket` y con bucle infinito de retransmisión en `Connected`.
     * `src/steam_p2p_hook.cpp`: Hooks de Winsock (`ws2_32.dll`) con MinHook. Posee un bug crítico de **doble envío en el Hold Buffer** (envía por Winsock crudo inmediatamente Y encola) y carece de hooks para `WSASendTo`/`WSARecvFrom` y sockets conectados.
     * `src/eossdk/net/refix_transport.cpp`: Protocolo RFX3 (`0x33584652`), descubrimiento multicast `239.255.71.84:47584` y socket unicast efímero `bind(0)` con hilo background. Es el diseño más limpio pero está aislado dentro de EOS.
2. **Defectos Críticos de ABI y Callbacks en Steamworks:**
   - **`ISteamUser` VTable Slot Shift:** Slot 14 desplazado por `GetAuthTicketForWebApi` en `SteamUser023`, corrompiendo `BeginAuthSession` en juegos compilados con `SteamUser021` (Unreal Engine 4.25-4.27).
   - **`ISteamGameServer` VTable Slot Shift:** Desplazamiento severo de métodos entre versión 012 y 015; `SendUserConnectAndAuthenticate` (slot 24 vs 39) produce crashes en servidores dedicados.
   - **`SteamAPI_ManualDispatch_*` Desconectado:** En modo LAN, `steam_proxy.cpp` retorna falso inmediatamente, dejando sin callbacks a motores modernos (GodotSteam, Unreal con manual dispatch).
   - **`ISteamNetworkingUtils::GetRelayNetworkStatus` Contradicción:** Retorna `CannotTry` en lugar de `Current` (100).
   - **Lobby Discovery / Sync Roto:** `PollNetwork()` no maneja `msgType 3` (`LobbyQuery`), impidiendo que jugadores que inician después descubran salas existentes. `SetLobbyData` y `JoinLobby` no transmiten paquetes de sincronización.
3. **Brechas en EOS, Photon y Unity Stack:**
   - En `src/eossdk/api/eos_api_user.cpp`, `EOS_Auth_Login` retorna `EOS_InvalidAuth` hardcodeado, rompiendo juegos dependientes de Epic Account Services (EAS).
   - En `src/providers/provider_factory.cpp`, `GetEOSProvider()`, `GetPhotonProvider()` y `GetNetworkProvider()` devuelven `nullptr`.
   - Photon PUN y Realtime dependen de conexión al Master Server; bajo LAN estricto se cuelgan en timeout si no hay redirección local.
4. **Divergencia con el Release v1.3.3 (`1bb1629`):**
   - La rama `feature/phase3-sockets` carece de todas las mejoras de robustez de v1.3.3: `-LiteralPath` en PowerShell, eliminación de delayed expansion (`!VAR!`) en `.bat`, verificación física post-despliegue `Verify-ReFixDeployment`, expansión de 1096 a 1141 exportaciones (Steamworks 1.60), y búsqueda jerárquica de `ReFix.ini` en directorios ascendentes (`..\` y `..\..\`).
   - `Uninstall_ReFix.bat` borra de forma destructiva e incondicional la carpeta de partidas `saves/`.

---

## 2. Mapa Arquitectónico Objetivo (Layered Architecture)

```
+===================================================================================+
|                                    GAME ENGINE                                    |
|              (Unreal Engine, Unity, Godot, Native C++, Custom Engines)            |
+===================================================================================+
                                         |
                                         | Original API / ABI Calls
                                         v
+===================================================================================+
|                         REFIX COMPATIBILITY LAYERS                                |
|                                                                                   |
|  [Steamworks Adapter]       [EOS Adapter]            [Photon / Winsock Adapter]  |
|  - ISteamClient / User /    - Connect / Auth (EAS)   - Transparent Winsock Hooks  |
|    Friends / Matchmaking /  - Lobbies & Sessions     - Hold Buffer (No Duplicate) |
|    Networking / Sockets     - EOS P2P Datagrams      - Port Virtualization        |
|  - ABI Version Thunks                                - Channel Mux (Game / Voice) |
|  - Manual Dispatch Pipeline                                                       |
+===================================================================================+
                                         |
                                         v
+===================================================================================+
|                              REFIX LAN CORE                                       |
|                                                                                   |
|  * Universal Peer Registry (MachineID <-> SteamID <-> PUID <-> Endpoint)          |
|  * Unified Discovery Engine (Multicast 239.255.71.84 + Broadcast 47584)           |
|  * Local Session & Lobby State Machine (Host-Authoritative, Metadata Sync)         |
|  * Deterministic Local Identity Service (Hardware / User Hashing)                 |
|  * Local Matchmaking & Callback Dispatcher                                        |
|  * Local Relay Service (Topologías Star / Multi-peer cuando sea necesario)        |
+===================================================================================+
                                         |
                                         v
+===================================================================================+
|                        UNIVERSAL LAN TRANSPORT (ULT)                              |
|                                                                                   |
|  * Dual Sockets: Multicast Discovery + Unicast bind(0) Efímero                   |
|  * Background I/O Reactor Thread (select / WSAWaitForMultipleEvents)              |
|  * Universal Wire Protocol ('ULTX' / framing unificado)                           |
|  * Reliable Overlay: Sliding Window, Cumulative ACK + SACK, Jacobson RTO          |
|  * Connection Lifecycle: Keep-Alive Heartbeats, Max Retries, Clean Disconnect     |
|  * Packet Queues & Channel Multiplexer (Game Ch0, Voice Ch1, Signaling Ch2)       |
|  * Strict Internet-Zero Egress Guard (RFC 1918 / Loopback Enforcement)            |
+===================================================================================+
                                         |
                                         v
+===================================================================================+
|                                 LOCAL NETWORK                                     |
|               (127.0.0.1, RFC 1918 Private LAN, Local VPN / ZeroTier)             |
+===================================================================================+
```

---

## 3. Matriz de Clasificación de Pruebas de Calidad (QA)

| Categoría | Estado Actual | Evidencia |
| :--- | :--- | :--- |
| **Static ABI Struct Asserts** | **compile PASS** | `static_assert` de tamaños en `refix_net_test.cpp:27-31`. |
| **VTable Alignment & Ordering** | **compile / unit PASS** | 47 métodos de `ISteamNetworkingSockets` verificados en suite 1. |
| **Provider Routing & Zero Fallback** | **runtime PASS** | Suite 2 valida 0 llamadas a Valve en LAN y 0 llamadas a emu en Online. |
| **Message Lifetime & Wire Simulation**| **integration PASS** | Suites 4-8 simulan loopback UDP monoproceso con inyector de fallos. |
| **Locking & Deadlock Watchdog** | **runtime PASS** | Suite 9 ejecuta 10.000 operaciones en 5 hilos sin deadlocks. |
| **Concurrencia / Data Race Detection** | **UNVERIFIED** | MSVC CRT carece de ThreadSanitizer; requiere validación estricta de jerarquía de locks. |
| **Internet-Zero Egress Policy** | **runtime PASS** | Suite 10 descarta 6/6 paquetes salientes WAN. |
| **Whole-Process Internet-Zero** | **CONDITIONAL** | Transporte interno certificado; llamadas Winsock crudas no hookeadas del juego quedan sin auditar. |
| **Adversarial Multiprocess Harness** | **CONDITIONAL** | 500 R + 500 U funcional en `RunAdversarialHarness`, pero requiere ejecución manual en 2 consolas. |
| **Two-PC Physical LAN Harness** | **UNVERIFIED** | Procedimiento documentado en `TWO_PC_LAN_HARNESS.md` nunca automatizado. |
| **LAN 3+ PCs Mesh & Discovery** | **UNVERIFIED** | No existen pruebas automatizadas multi-nodo. |
| **Lobby Lifecycle (Create/Query/Join)**| **UNVERIFIED (Defectuoso)**| `refix_net_test.cpp` no tiene pruebas de `ISteamMatchmaking`; `PollNetwork()` omite `msgType 3`. |
| **Separación Juego / Voz (Canal 0/1)** | **UNVERIFIED** | Puertos 7777 vs 5058 no evaluados en suites de tests. |
| **bind(0) Sockets Efímeros** | **UNVERIFIED (Defectuoso)**| `unreal_steam_emu.cpp` fuerza 47584; `Hook_bind` no llama `getsockname()`. |

---

## 4. Lista Consolidada de Riesgos Técnicos

1. **Riesgo 1: Crash por VTable Slot Shift en Juegos Compilados con SDKs Anteriores:**
   - *Severidad:* Crítica.
   - *Mitigación:* Implementar thunks / wrappers específicos de versión (`ISteamUser021`, `ISteamGameServer012`) que mapeen exactamente los índices de vtable esperados por el binario del juego.
2. **Riesgo 2: Fuga de Tráfico WAN por Funciones Winsock No Interceptadas:**
   - *Severidad:* Alta.
   - *Mitigación:* Añadir detours para `WSASendTo`, `WSARecvFrom`, `WSASend`, `WSARecv`, `send`, `recv`, y bloquear resoluciones DNS públicas.
3. **Riesgo 3: Doble Envío e Inundación de Red en Juegos Winsock / Photon:**
   - *Severidad:* Alta.
   - *Mitigación:* Reparar el fallthrough prematuro de `Hook_sendto` en `g_holdBuffer`; reemplazar broadcasts `255.255.255.255` en `SendP2PPacket` por transmisiones unicast dirigidas.
4. **Riesgo 4: Packet Stealing y Colisión de Puertos en 1 PC (Multi-Instancia):**
   - *Severidad:* Alta.
   - *Mitigación:* Migrar sockets de datos a `bind(0)` efímero con consulta `getsockname()`, reservando el puerto fijo `47584` exclusivamente para el socket de descubrimiento multicast (`SO_REUSEADDR`).
5. **Riesgo 5: Pérdida de Partidas Guardadas al Desinstalar:**
   - *Severidad:* Alta.
   - *Mitigación:* Modificar `Uninstall_ReFix.bat` para preservar la carpeta `saves/` o pedir confirmación interactiva.
6. **Riesgo 6: Regresión en Despliegue con Títulos de Caracteres Especiales:**
   - *Severidad:* Media.
   - *Mitigación:* Portar de inmediato las mejoras de v1.3.3 (`disabledelayedexpansion`, `-LiteralPath`, `Verify-ReFixDeployment`).
