# Pruebas

## Prueba de interconexión de dos procesos

`tests/test_two_instance_lobby.cpp` no usa mocks: carga la DLL **compilada** con
`LoadLibrary`, resuelve los mismos exports que resuelve un juego y ejecuta la API
real sobre el transporte UDP real.

```
test_two_instance_lobby.exe host   EOSSDK-Win64-Shipping.dll 30
test_two_instance_lobby.exe client EOSSDK-Win64-Shipping.dll 30
```

El lanzador les da valores distintos de `REFIX_USER_INSTANCE`, así que un PC hace
de dos jugadores.

Lo que verifica, en orden:

| Comprobación | Qué demuestra |
| --- | --- |
| `EOS_Connect_Login` da un PUID | la identidad se deriva de verdad |
| el cliente encuentra la sala | el descubrimiento funciona entre procesos |
| `MaxMembers == 4` | las plazas son las configuradas, no un valor por defecto |
| `BucketId` coincide | los atributos viajan y vuelven intactos |
| el dueño tiene un PUID **distinto** | se acabó el PUID compartido de la v1 |
| `EOS_Lobby_JoinLobby` → `EOS_Success` | el host aceptó de verdad |
| el host ve 2 miembros | la lista de miembros es la del anfitrión |
| ping/pong P2P | los paquetes del juego llegan a su destino |

## Prueba en el juego

*MECCHA CHAMELEON* (Unreal 5 + RedpointEOS + Steamworks) es el título de
referencia. `D:\EOS_REFIX\_agent\!RUN.bat` compila, despliega, lanza y recoge
`ReFix.log` y cualquier minidump.

Progresión medida durante el desarrollo:

| Estado | Resultado |
| --- | --- |
| proxy heredado (v2) | crash `0xC0000005` justo después del login |
| v3, núcleo + Connect | arranca y se mantiene; identidad = cuenta real de Steam |
| v3 + Lobby | el juego crea sus lobbies (`Game:Parties`, `CrossPlatformPresence`) con 15–19 atributos propios |
| v3 + PlayerDataStorage | supera la pantalla de guardado en la nube |

## Diagnóstico de crashes

Los minidumps de Unreal (`%LOCALAPPDATA%\<Proyecto>\Saved\Crashes`) se leen sin
depurador: `build.bat` genera `build/eos.map`, y con el mapa se resuelven las
direcciones de la pila del hilo que falló hasta símbolos del emulador. Así se
localizó la inversión de campos en `EOS_Connect_ExternalAccountInfo`.
