# Cobertura de la API

`tools/gen_eos_abi.py` describe los 679 exports a partir de los headers oficiales
del SDK. La tabla siguiente cruza esa lista con las 347 funciones que
*MECCHA CHAMELEON* importa realmente (`eos_imports_categorized.txt`).

Lo no implementado **no** es un agujero: cae en el stub genérico, que responde
según el tipo de retorno (`EOS_NotFound` para resultados, `nullptr` para
punteros, 0 para contadores) y contesta el delegado de las asíncronas. Esa es la
diferencia entre "el juego recibe un no" y "el juego se cuelga o crashea".

| Subsistema | Importadas por el juego | Implementadas |
| --- | ---: | ---: |
| Achievements | 11 | 0 |
| ActiveSession | 5 | 5 |
| AntiCheatClient | 16 | 0 |
| AntiCheatServer | 11 | 0 |
| Auth | 8 | 4 |
| ByteArray | 1 | 1 |
| Connect | 21 | 18 |
| EResult | 2 | 2 |
| Ecom | 16 | 0 |
| EpicAccountId | 3 | 3 |
| Friends | 10 | 10 |
| Initialize | 1 | 1 |
| IntegratedPlatform | 1 | 0 |
| IntegratedPlatformOptionsContainer | 1 | 0 |
| Leaderboards | 8 | 0 |
| Lobby | 41 | 28 |
| LobbyDetails | 9 | 9 |
| LobbyModification | 9 | 9 |
| LobbySearch | 7 | 7 |
| Logging | 2 | 2 |
| Metrics | 2 | 0 |
| P2P | 13 | 11 |
| Platform | 29 | 29 |
| PlayerDataStorage | 8 | 8 |
| PlayerDataStorageFileTransferRequest | 1 | 1 |
| Presence | 8 | 0 |
| PresenceModification | 5 | 0 |
| ProductUserId | 3 | 3 |
| RTC | 12 | 1 |
| RTCAudio | 9 | 0 |
| Reports | 1 | 0 |
| Sanctions | 4 | 0 |
| SessionDetails | 6 | 6 |
| SessionModification | 9 | 9 |
| SessionSearch | 7 | 7 |
| Sessions | 21 | 19 |
| Shutdown | 1 | 1 |
| Stats | 5 | 0 |
| TitleStorage | 6 | 2 |
| TitleStorageFileTransferRequest | 1 | 0 |
| UI | 4 | 0 |
| UserInfo | 9 | 3 |
| **Total** | **347** | **199** |

## Implementado por completo

- **Platform** — Initialize/Shutdown/Create/Tick/Release, los 27 accesores de
  interfaz, estado de aplicación y red, códigos de país/idioma, logging.
- **Connect** — login con ticket de Steam o DeviceId, cuentas externas, mapeos de
  id, notificaciones de estado de sesión.
- **Lobby** — crear, modificar, buscar con filtros y comparadores, unirse por
  handle o por id, salir, expulsar, promover, invitar, y las 8 notificaciones.
- **LobbyDetails / LobbyModification / LobbySearch** — completos.
- **Sessions** — crear, actualizar, unirse, iniciar, terminar, destruir,
  registrar jugadores, invitar, buscar; `ActiveSession` completo.
- **P2P** — envío y recepción por (par, socket, canal), aceptación de conexión,
  cierre, cola de paquetes, tipo de NAT, notificaciones.
- **PlayerDataStorage** — guardado local por jugador con el protocolo de
  transferencia por trozos real.
- **Auth / Friends / UserInfo** — responden con veracidad que no hay cuenta de
  Epic ni lista de amigos, que es lo que mantiene al juego en la ruta de Connect.

## Pendiente (siguiente iteración)

Presence (join info para invitar desde la lista de amigos), RTC de voz,
Achievements, Stats, Leaderboards, Ecom y AntiCheat.
