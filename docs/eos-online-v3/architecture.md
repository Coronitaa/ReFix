# Arquitectura

```
Juego (Unreal + OnlineSubsystemRedpoint)
        │  C ABI del SDK de EOS
        ▼
EOSSDK-Win64-Shipping.dll  (= RedboneEOS.dll)
 ├── src/eos_fwd.asm .......... 679 trampolines, cada uno salta a g_eosProcs[i]
 ├── eos_module.cpp ........... tabla de exports, stub genérico, registro
 ├── api/ ..................... capa ABI: una unidad por subsistema
 │     eos_api_platform.cpp      Init/Platform/Tick/ids/resultados
 │     eos_api_connect.cpp       Connect: credenciales → ProductUserId
 │     eos_api_user.cpp          Auth / UserInfo / Friends
 │     eos_api_lobby.cpp         Lobby / Modification / Details / Search
 │     eos_api_sessions.cpp      Sessions / ActiveSession / Details / Search
 │     eos_api_p2p.cpp           P2P
 │     eos_api_storage.cpp       PlayerDataStorage / TitleStorage
 ├── core/ .................... estado autoritativo, sin nada de red
 │     refix_config / refix_log / refix_hash / refix_common
 │     eos_ids ................. interning de PUID / EpicAccountId
 │     eos_identity ............ quién es el jugador local
 │     eos_dispatch ............ cola de callbacks + registro de notificaciones
 │     eos_handles / lobby_handles / session_handles
 │     lobby_model ............. lobby, miembros, atributos, filtros de búsqueda
 │     eos_platform / eos_online
 └── net/ ..................... transporte
       refix_wire .............. codificación binaria con longitud
       refix_transport ......... socket UDP + grupo multicast + broadcast
       lobby_directory ......... protocolo: anuncio, búsqueda, join, update, P2P
```

## Capa ABI

Los 679 exports salen del `.def` como alias a stubs en ensamblador que saltan por
`g_eosProcs[i]`. `eos_module.cpp` llena esa tabla:

- todas las entradas apuntan primero a un stub propio por export, generado en
  memoria ejecutable, que registra la llamada y devuelve un valor **seguro para
  su tipo de retorno** (tomado de la tabla ABI generada);
- después, cada subsistema sobrescribe las entradas que implementa de verdad.

El stub por export existe para que el log diga *qué* función pidió el juego. Es
lo que permitió descubrir en una sola ejecución que MECCHA CHAMELEON se quedaba
bloqueado en `EOS_PlayerDataStorage_QueryFileList`.

## Callbacks

Regla del SDK real que el proxy heredado rompía: una API asíncrona **nunca**
invoca su delegado en línea. Aquí todo se encola y se despacha desde
`EOS_Platform_Tick`, en el hilo que el juego eligió. `Dispatcher` también ofrece
`PostAfter`, usado por las búsquedas, que necesitan un instante real de red antes
de poder decir "no hay partidas".

Las notificaciones (`EOS_*_AddNotify*`) se guardan en un registro por categoría y
se disparan cuando el evento ocurre de verdad. En la versión heredada devolvían
un entero y descartaban el puntero, que es por lo que las invitaciones nunca
llegaban.

## Handles

Cada handle opaco del SDK es aquí un struct real con palabra mágica. Un handle
caducado se rechaza en vez de desreferenciarse. `LobbyDetails` guarda una
*instantánea*, como promete el SDK: seguir describiendo lo que describía cuando
se copió.
