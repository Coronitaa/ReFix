# SteamNetworkingSockets Implementation Matrix

| API                         | Exists | Implemented | Stub | Current backend | LAN behavior | Online behavior | Test |
| --------------------------- | ------ | ----------- | ---- | --------------- | ------------ | --------------- | ---- |
| CreateListenSocketP2P       | YES    | YES         | NO   | None            | Dummy listener | Valve Native  | PASS |
| ConnectP2P                  | YES    | YES         | NO   | ReFixTransport  | Handshake sent | Valve Native  | PASS |
| AcceptConnection            | YES    | YES         | NO   | ReFixTransport  | Transitions | Valve Native   | PASS |
| SendMessageToConnection     | YES    | YES         | NO   | ReFixTransport  | Enqueues UDP | Valve Native   | PASS |
| ReceiveMessagesOnConnection | YES    | YES         | NO   | ReFixTransport  | Pops queue | Valve Native    | PASS |
| CloseConnection             | YES    | YES         | NO   | ReFixTransport  | Clears state | Valve Native    | PASS |

## Estado Post-Phase 3
El emulador `CSteamNetworkingSocketsEmu` ha sido implementado utilizando un esquema de mapeo dinámico en `unreal_steam_emu.cpp`. 
Las llamadas ahora se rutean transparentemente como un nuevo paquete (MsgType 6: Sockets Payload, MsgType 7: Sockets Handshake, MsgType 8: ACK) hacia el backend UDP LAN previamente validado.
