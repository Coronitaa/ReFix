# Party Animals offline / Steam P2P — working notes

Game: Party Animals, IL2CPP Unity 2022.3.62f3 + HybridCLR hot-update. Game dir `D:\Games\Party Animals\`.
Deploy `build\steam_api64.dll` to the game root and `PartyAnimals_Data\Plugins\x86_64\`. Test: `python scratch\test_run.py`
(logs: `ReFix.log` in game dir, Unity `Player.log`).

## Modules (steam_api64.dll)
| File | Role |
|---|---|
| `src/refix_il2cpp.cpp` | Bootstrap, camera safeguard, transition probes, watchdog, main-thread tick |
| `src/refix_il2cpp_scene.{h,cpp}` | Generic IL2CPP helpers (class lookup, fields, getters, strings) |
| `src/refix_pa_offline.{h,cpp}` | Login-free mode (ModeType P2P=4 + isLanMode), fake id `fake_<SteamID>`, local JWT, skip first-launch modals |
| `src/refix_il2cpp_logfwd.{h,cpp}` | Forwards RecreateLogger / Debug.LogException to ReFix.log (`[P2P] ForwardGameLog`) |
| `src/refix_pa_backend.{h,cpp}` | Loopback HTTP server; hooks `UriHelper.NewUri` to redirect `*partyanimalsgame.com*` and empty URLs |
| `src/refix_pa_services.{h,cpp}` | GraphQL op router (`changeDisplayName`, persisted in `ReFix_Profile.ini`) |
| `src/refix_pa_gql.{h,cpp}` | Shared JSON / GraphQL text helpers and LambdaResult envelope |
| `src/refix_pa_rooms.{h,cpp}` | P2P room ops (createRoom, describeRoom, listRoom, updateRoom, jwt, ping...) backed by Steam rich presence `refix_pa_room` |
| `src/refix_pa_relay.{h,cpp}` | ServerlessP2P relay on `127.0.0.1:27100`, bridging Bolt payloads over SDR channel `[P2P] RelayChannel` (7) |
| `src/refix_steam_flat.{h,cpp}` | Steam flat-API access (rich presence, SDR messages, session accept); picks the initialized steam_api64 copy |

## IL2CPP gotchas
- `il2cpp_field_(static_)set_value` stores reference values as given: pass the object pointer, not `&ptr`.
- `AutoCreateSingleton<T>` instance lives in the parent generic class static `s_Singleton`.
- Hot-update DLLs: AES-128-ECB/PKCS7, key `CryptoManager.hotUpdateKey`; many AOT methods are DHE `ret` stubs.
- `ReFixLog` must use `_vsnprintf_s(..., _TRUNCATE)`; `vsprintf_s` fail-fasts on >1 KiB messages.

## Backend protocol
- POST `{"Query":"<GraphQL>","SendTime":..,"RequestId":..}`, headers `jwt`, `X-magic-number`.
- Reply `{"code":0,"httpStatus":200,"result":{"<op>":<data>},"message":""}`.
- Any HTTP 503 => MaintenanceManager maintenance screen (10053). Unhandled ops therefore drop the connection.

## Client config (ConfigRetrieverResult)
- ConfigAsyncRetriever.m_ConfigLazy (UniTask AsyncLazy) starts in Steam mode and never finishes offline; anything
  awaiting it (PingHelperForGateway.GetPingForCurrentRegion -> P2P createRoom) hangs.
- ReFix completes `m_ConfigLazy.completionSource.TrySetResult(...)` with a local result: code "0", gatewayRegion,
  service URLs `https://lambda.partyanimalsgame.com/<field>` (routed to the local backend). The backend also answers
  `{"operation":"request-config-for-client-v3"}` with the same data.
- `P2PRoomUtils.useFakeData` is now false when `[P2P] LocalBackend` is on (fake data bypassed the backend).

## Hosting is stripped from the shipping client (blocker)
- `P2PRoomLogic.JoinRoomAsHost` is empty; `StartP2PServer` is intact but uncalled. Calling it works up to
  `BoltLauncher.StartServer` (relay SERVER_LOGIN ok, applicationType Hybrid), then stalls.
- `Network.ServerManager` has 21 emptied methods (ConnectRequest, BoltStartDone, OnEvent, Update,
  SceneLoadLocalDone, DisconnectClient, PaConnected...): the authoritative server logic is not in the client.
- So a client can create/list rooms and run the client join path, but nobody can host a match.

## Multiplayer (ServerlessP2P relay) — implemented; blocked by stripped hosting (see above)
- roomId = host SteamID64; roomOwnerId = `fake_<SteamID64>`; listRoom = Steam friends whose rich presence
  `refix_pa_room` = `v1|gamers|isGaming|curMapId|mode|mapList|name`.
- Relay: virtual peer endpoints 10.80.x.y; Steam envelope `[1=to host|2=to client][Bolt payload]`.
- Sessions: callback 1251 accepted in `SteamAPI_ManualDispatch_GetNextCallback`, plus hosts sweep friends every 2 s.
- Party Animals loads two steam_api64_valve copies (root + Plugins); only one is initialized.
- Local probe: `scratchpad/relay_probe.py <backendPort>` (HTTP ops + UDP relay handshake).
- Relay at `p2pServerIp:27100` (`P2PBaseProxy.p2pServerPort`). Packets: `[0x01][json]` control, `[0x02][ip4][port2][payload]` Bolt data.
- Host (`P2PServerProxy`): `SERVER_LOGIN{accountId}` -> `SERVER_LOGIN_REPLAY{isSuccess,ip,port}`; one local socket per client endpoint.
- Client (`P2PClientProxy`): `CLIENT_LOGIN`, `CLIENT_QUERY{targetId}` -> `CLIENT_QUERY_REPLY{isSuccess,ip,port}`.
- Plan: ReFix relay on `127.0.0.1:27100` per PC, bridging over ISteamNetworkingMessages on a dedicated channel
  (`steam_p2p_hook` owns channels 0/1); room ops (CreateRoom, DescribeRoom, CreateP2PJwt, CheckP2PJwt, ListRoom, ...)
  backed by Spacewar (480) lobbies, always returning `p2pServerIp = "127.0.0.1"`.

## Out of scope
Faking real-money purchase verification (Nemo Bucks / Steam MicroTxn) is not implemented.
