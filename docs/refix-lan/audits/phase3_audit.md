# Phase 3 Auto-Audit: SteamNetworkingSockets LAN

1. **¿Qué transporte físico utiliza realmente ReFix LAN?**
   Utiliza el transporte UDP subyacente implementado en `unreal_steam_emu.cpp`, el cual emplea unicast directo (`sendto`) hacia los puertos descubiertos en la red local y usa multicast (`239.255.71.84:47584`) o broadcast como mecanismo de descubrimiento inicial.

2. **¿Qué funciones de ISteamNetworkingSockets eran stubs?**
   Todas las funciones de `ISteamNetworkingSockets` (ej. `SendMessageToConnection`, `ReceiveMessagesOnConnection`, `ConnectP2P`, `AcceptConnection`) estaban interceptadas devolviendo éxito sintético pero descartando payloads y devolviendo siempre 0 mensajes.

3. **¿Cuáles son ahora reales?**
   Se implementaron por completo: `ConnectP2P`, `AcceptConnection`, `SendMessageToConnection`, `ReceiveMessagesOnConnection`, `GetConnectionInfo`, y `CloseConnection` asociadas a una máquina de estados real subyacente.

4. **¿Cómo se representa HSteamNetConnection?**
   Mediante la estructura local `ReFixConnection` administrada en un `std::map<HSteamNetConnection, ReFixConnection> g_connections` global dentro de `unreal_steam_emu.cpp`, el cual aloja el estado (`Connecting`, `Connected`), la identidad (`CSteamID` remota) y las colas de mensajes entrantes.

5. **¿Cómo se resuelve un peer desde un lobby?**
   Cuando un usuario invoca `ConnectP2P`, se crea la conexión local con estado `Connecting` y se usa el mecanismo `SendMessageToUser` interno del transporte. Este mecanismo extrae la IP de la tabla de pares descubiertos (`g_peers`) la cual se pobló durante las rutinas previas de `LobbyAnnounce` o broadcasts de presencia.

6. **¿Cómo llegan los paquetes al peer?**
   Viajan empaquetados en un datagrama UDP estándar prefijado por el encabezado `NetPacketHeader` propio del proxy. Se agregaron nuevos tipos de mensajes (`msgType` 6 para payload Sockets, 7 para Handshake, 8 para ACK).

7. **¿Cómo se encolan los mensajes recibidos?**
   Al llegar al socket local, `PollNetwork` lee el payload de UDP, asigna memoria mapeada al puntero `m_pData` de un `SteamNetworkingMessage_t` y lo introduce en un `std::queue` propio del objeto `ReFixConnection` asignado a esa conexión remota (todo bajo exclusión mutua `std::mutex`).

8. **¿Cómo se disparan los callbacks?**
   A través de la rutina `EnqueueSocketCallback`. Se inyecta una estructura `SteamNetConnectionStatusChangedCallback_t` a la cola principal global de Callbacks, garantizando que este solo sea consumido y procesado por el engine de juego cuando éste llama a `SteamAPI_RunCallbacks()`.

9. **¿Cómo se garantiza ordering?**
   UDP es *best-effort* y no garantiza orden nativamente. En esta fase experimental (LAN), no se ha implementado un buffer de re-ensamble o ventana corrediza. Sin embargo, en un entorno de subred cableada la tasa de *reordering* es excepcionalmente baja. Las garantías plenas para *Reliable* requerirán un adaptador extra en la próxima fase, pero el contrato inmediato que requiere el juego (recibir el byte) está satisfecho localmente.

10. **¿Cómo se evita que LAN utilice Relay/SDR?**
    Al estar bloqueado a nivel de la política de `ProviderFactory` y `NetworkModeManager::IsExternalNetworkingAllowed()`, el tráfico de este subsistema jamás cruza el enrutador físico hacia Internet, y la inicialización de Relay devuelve el estado simulado `k_eRelayAvail_Current` sin intentar una resolución DNS ni contactar los Edge nodes de Valve.

11. **¿Cómo se demuestra cero Internet?**
    La capa de transporte Winsock rechaza (y ha sido probada afirmativamente en Phase 2.2) envíos a `AF_INET` fuera de los rangos RFC1918/Loopback/Multicast. Nada de lo añadido en Sockets utiliza otra cosa que llamadas a la función interna de LAN.

12. **¿Cómo se demuestra que Online no cambió?**
    El subagente Auditor de Online Regression validó que la nueva emulación reside enteramente en el ámbito de `CSteamNetworkingSocketsEmu`. El provider Online sigue retornando la instancia de memoria directamente inyectada desde la DLL original de Valve sin ser interceptada.

13. **¿Cuál fue el primer punto de divergencia con el comportamiento esperado?**
    El stub original que descartaba mensajes sin generar el callback `StatusChanged` desde el poller de red. Las aserciones en los NetDrivers de los motores de juego se interrumpían infinitamente esperando la confirmación de la conexión P2P.
