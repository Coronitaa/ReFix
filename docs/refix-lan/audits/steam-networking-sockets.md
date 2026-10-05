# Phase 2.2 Audit: ISteamNetworkingSockets

## Diagnóstico y Funcionalidad

| Export / Función | Estado | Detalle |
| :--- | :--- | :--- |
| `CreateListenSocketIP` | STUB | Retorna success y un handler virtual local, pero no levanta sockets reales. |
| `ConnectByIPAddress` | STUB | Simula success, pero no efectúa handshake. |
| `CreateListenSocketP2P` | INTERCEPTED | Redirigido a emulación en LAN. |
| `ConnectP2P` | INTERCEPTED | Redirigido a emulación en LAN. |
| `AcceptConnection` | STUB | Retorna success silente. |
| `CloseConnection` | STUB | Retorna success silente. |
| `SendMessageToConnection` | STUB | Retorna success silente y **suelta (drops) el payload**. |
| `SendMessages` | STUB | Equivalente a la anterior, descartado asíncrono. |
| `FlushMessagesOnConnection`| STUB | N/A |
| `ReceiveMessagesOnConnection`| STUB | Retorna 0 (sin mensajes). |

## Backend y Transport
Actualmente, el emulador de red implementa las API clásicas P2P (`SendP2PPacket` / `ReadP2PPacket`) ruteando a través de un backend UDP que efectúa llamadas broadcast (`SendBroadcast()`). Sin embargo, el adaptador para las API modernas de *Sockets* (`ISteamNetworkingSockets`) está compuesto principalmente por stubs vacíos que aparentan éxito frente al juego pero que no envían bits a través del cable.

**Conclusión pre-Phase 3:** En LAN, los juegos Unreal 5 que dependen estrictamente de `ISteamNetworkingSockets` podrán inicializar el NetDriver gracias a los falsos positivos de los stubs, pero no podrán transferir datos, lo que prevendrá que un jugador ingrese en el lobby de otro. Es imperativo reimplementar `SendMessageToConnection` mapeándolo al transporte subyacente.
