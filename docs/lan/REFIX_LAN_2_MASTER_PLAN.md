# ReFix LAN 2.0 — Memoria Técnica y Plan Maestro de Arquitectura

**Versión del Documento:** 1.0.0  
**Fecha:** Octubre 2026  
**Rama Activa:** `refix_lan_infrastructure`  
**HEAD Local:** `43b56c9` (`build: synchronize compiled binaries with socket isolation and multi-instance fixes`)  
**Checkpoint Histórico Baseline:** `7ef361a` (`feat(refix-lan): implement ReFix Universal LAN Transport and Core architecture`)  
**Estado del Working Tree:** Limpio (0 cambios pendientes sin commit)  
**Fase Actual:** FASE 0 (Auditoría del Estado Actual y Establecimiento del Baseline) — COMPLETADA  

---

## 1. Filosofía e Invariantes Innegociables del Proyecto

ReFix LAN 2.0 es una **infraestructura universal de compatibilidad multijugador local, autónoma y verificable**. No es una colección de hacks o parches individuales por juego ni un fork dependiente de motores gráficos específicos.

### Invariantes Fundamentales:
1. **Autonomía Total (Internet-Zero):** LAN/Offline no requiere Steam instalado, `Steam.exe`, cuentas de usuario, autenticación remota ni salida a Internet. Tráfico WAN, SDR de Valve y multicast global son bloqueados estrictamente en el egress del socket.
2. **Descubrimiento LAN Automático:** Dos o más instancias en la misma red local se descubren mutuamente mediante multicast (`239.255.71.84:47584..47599`) y broadcast sin configuración manual de IPs ni port-forwarding.
3. **Transporte Local Real:** Paquetes, sesiones y P2P fluyen por endpoints IPv4 locales (RFC 1918, APIPA, Loopback) con motor ARQ confiable (secuenciación monotónica, ACKs acumulativos, SACK, reensamblado de fragmentos).
4. **Compatibilidad Genérica:** Cero IPs hardcodeadas, AppIDs especiales o esperas arbitrarias. Comportamiento fiel al contrato semántico de las APIs emuladas.
5. **Aislamiento Estricto Online / LAN:** Las rutas de ejecución Online (passthrough a DLL de Valve) y LAN/Offline permanecen completamente desacopladas sin contaminación cruzada.
6. **Multi-instancia Concurrente en 1 PC:** Capacidad de ejecutar múltiples instancias del juego en el mismo equipo (directorios separados) con slots automáticos (`0..15`), identidades (`PeerId` 128-bit derivado de MachineId + PID, SteamID offseteado por slot, nombres `Player`, `Player_2`), puertos unicast efímeros independientes y sockets de transporte aislados.
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
|  - steam_api64.dll / steam_proxy.cpp (Interceptor y Jump Table 1141 exports)      |
|  - unreal_steam_emu.cpp (Emulador Steamworks vtables + C-APIs)                    |
|  - EOSSDK-Win64-Shipping.dll / RedboneEOS.dll (Emulador Epic Online Services v3)  |
|  - steam_p2p_hook.cpp (MinHook Winsock: Redirección P2P UDP & Port Virtualization)|
|  - UniversalPhotonFix.dll (BepInEx C# Harmony: Redirección Photon Cloud)          |
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

## 3. Diagnóstico de la Fase 0 — Estado Real del Repositorio

### 3.1 Inventario de Binarios y Scripts
- `build.bat`: Compilador MSVC x64 de producción y pruebas (`steam_api64.dll`, `steam_api64_test.dll`, `EOSSDK-Win64-Shipping.dll`, `RedboneEOS.dll`, `winmm.dll`, `refix_net_test.exe`, `test_refix_lan_core.exe`, `test_steam_lancore_adapter.exe`).
- `bin/deploy_helper.ps1`: Motor de despliegue multi-motor (Unity, Unreal, Godot).
- `bin/set_lan_identity.ps1`: Script de asignación forzada de identidad LAN (SteamID / PersonaName).

### 3.2 Estado de Verificación y Pruebas Ejecutadas
| Arnés de Prueba | Comando | Nivel de Certificación | Resultado | Evidencia / Observaciones |
| :--- | :--- | :--- | :--- | :--- |
| **Compilación General** | `build.bat` | `BUILD PASS` | **PASS** | Todas las DLLs y ejecutables compilan y se sincronizan a `bin/` sin errores. |
| **LAN Core & Transport** | `bin/test_refix_lan_core.exe` | `UNIT PASS` | **PASS** | 100% pruebas unitarias y de estrés pasadas (ARQ, OOO, SACK, budgets 4MiB, wrap 32-bit RFC 1982, reactores reales, TTL 10s). |
| **Steamworks -> LanCore Adapter** | `bin/test_steam_lancore_adapter.exe --standalone` | `UNIT PASS` | **PASS** | Aislamiento EAT, CSteamID canónico, rollback preservation, async dispatch de lobbies certificados. |
| **Multiproceso Dual E2E** | `tests/test_steam_adapter_multiprocess.ps1` | `MULTIPROCESS PASS` | **PASS** | Host y Client en procesos independientes en 1 PC: descubrimiento mutuo, unión a lobby, intercambio Ping/Pong ch0, fragmentación 8192B ch1, teardown limpio. |
| **Multiproceso Canónico Estricto** | `... -DisableFallbacks` | `MULTIPROCESS PASS` | **PASS** | 100% funcional con `REFIX_DISABLE_LEGACY_FALLBACKS=1` activo. |
| **Internet-Zero Firewall** | `bin/refix_net_test.exe isolation` (Suite 10) | `INTERNET-ZERO PASS` | **PASS** | 6/6 destinos WAN y multicast global bloqueados en el egress; subredes LAN/Loopback permitidas; cero cargas de DLL externa en LAN. |
| **Loopback Adapter Suites 1-4, 9-11, 13** | `bin/refix_net_test.exe <suite>` | `UNIT PASS` | **PASS** | ABI, VTable routing, Zero Fallback, 100 conexiones simultáneas sin colisión, lifetime de mensajes, fault injection, bootstrap. |
| **Loopback Legacy Suites 5-8** | `bin/refix_net_test.exe reorder/duplication/handshake/callback` | `UNIT PASS` | **FAIL** | Fallan por desacoplamiento entre el arnés de prueba de `refix_net_test.cpp` (que asume sockets UDP legacy en `unreal_steam_emu.cpp`) y la arquitectura canónica. |

---

## 4. Hallazgos Críticos y Deuda Técnica Detectada

1. **Dualidad de Transporte Legacy vs Canónico:**
   - `src/unreal_steam_emu.cpp` contiene tanto la llamada al transporte canónico (`ILanCore`) como una implementación legacy de sockets UDP (`InitSockets`, `BroadcastNetPacket`, `SendLanPacket`, `PollNetwork`) protegida por `ShouldUseLegacyFallback()`.
   - `refix_net_test.exe` Suites 5 a 8 envían paquetes con cabecera legacy de 17 bytes (`NetPacketHeader`), lo que genera fallos en los tests unitarios de loopback de esa suite específica.
2. **Duplicación en EOS SDK v3:**
   - `src/eossdk/net/refix_transport.cpp` mantiene un transporte propio con socket de grupo y unicast, cabecera de cable `refix_wire.h` (`RFX3`) y reactor `select()`, en lugar de consumir `ILanTransport` de `lan_core`. Carece de confiabilidad ARQ nativa.
3. **Incompletitud en Factoría de Proveedores:**
   - En `src/providers/provider_factory.cpp`, `GetEOSProvider()`, `GetPhotonProvider()` y `GetNetworkProvider()` retornan `nullptr`. No hay abstracción común instanciable para dichos motores.
4. **Dependencia de BepInEx para Photon:**
   - La compatibilidad con Photon requiere inyección C# (`UniversalPhotonFix.dll`) y hooks de Winsock para redirección P2P. No existe un servidor LAN Photon nativo en C++ autónomo.

---

## 5. Decisiones Técnicas Registradas

- **D-01: Separación de Sockets Multicast y Datos Unicast:**
  `LanTransport` utiliza socket de grupo en `47584..47599` (sin `SO_REUSEADDR` en Windows para evitar port hijacking) y un socket unicast con puerto efímero 0 para datos, garantizando multi-instancia determinista en 1 PC.
- **D-02: Aislamiento de Sockets Internos en MinHook:**
  Todos los sockets UDP de infraestructura interna (`LanTransport` y `UnrealSteamEmu`) se registran en `SteamP2PHook::RegisterInternalSocket()` para evitar intercepciones recursivas en `Hook_sendto` y bucles infinitos de envoltura.
- **D-03: Mapeo Canónico CSteamID de Lobbies:**
  Los identificadores de lobby en Steamworks se derivan de forma biyectiva 1:1 mediante `SteamLobbyRegistry`, formateando los 32 bits superiores como `0x01840000` (`k_EUniversePublic`, `k_EAccountTypeChat`, `k_EChatInstanceFlagLobby`).
- **D-04: Canales P2P Dedicados para Hooks de Winsock:**
  Para evitar que los hooks transparentes de Winsock colisionen con el tráfico P2P directo de Steamworks (canales 0 y 1), `SteamP2PHook` opera exclusivamente en canales `250` (Juego) y `251` (Voz) con cabecera `'REFX'`.

---

## 6. Siguiente Unidad de Trabajo Ejecutable (Roadmap Fase 1)

**Objetivo:** Consolidación y Fiabilidad del Núcleo LAN Común.
- **Tarea 1.1:** Investigar y corregir el desacoplamiento de las Suites 5, 6, 7 y 8 en `refix_net_test.cpp`, alineando el arnés de pruebas con la arquitectura canónica de `ILanCore` / `LanTransport`.
- **Tarea 1.2:** Eliminar la duplicación y las carreras residuales en `unreal_steam_emu.cpp`, permitiendo operar 100% sobre `ILanCore` de forma limpia y auditada.
- **Tarea 1.3:** Certificar `ALL PASS` en la totalidad de las suites de `refix_net_test.exe`.
