# Photon P2P (ReFixP2P backend)

Replaces Photon Cloud for PUN 2 / Photon Realtime games with a local emulator plus Steam P2P (Spacewar 480).
Works at the network level, so it is independent of Mono vs IL2CPP and of the game's code.
Enabled with `[Photon] Backend = ReFixCloud` in `ReFix.ini` (legacy alias `ReFixP2P` still works).
ReFix's deploy tooling (`deploy_helper.ps1`) picks this automatically: when it detects a Photon stack
(PUN 2 / Realtime / Voice / **Fusion**) and the deploy mode avoids third-party servers (valve / goldberg /
offline / lan) it writes `Backend = ReFixCloud`; otherwise `OfficialPhoton` (real Photon cloud).

## How it works
1. `pn_dns` hooks `getaddrinfo` / `GetAddrInfoW` / `gethostbyname`: `*.photonengine.io`, `*.photonengine.com`,
   `*.exitgames.com` (`[Photon] HostSuffixes`) resolve to 127.0.0.1.
2. `pn_p2p` listens on 127.0.0.1 UDP: NameServer 5058/27000, Master 5055/27001, GameServer 5056/27002 (default and
   `PhotonPortDefinition.AlternativeUdpPorts`). It also echoes region pings (12 x 0x7D + id).
3. `pn_enet` is the server side of Photon's UDP transport (connect/verify, acks, ordered reliable delivery,
   fragments, unsequenced sends, retransmits, timeouts).
4. `pn_crypto` does the Photon key exchange (Oakley 768-bit DH, generator 22, AES-256-CBC with key SHA-256(secret)
   and a zero IV). Clients encrypt GetRegions and Authenticate.
5. `pn_server` emulates the LoadBalancing roles:
   - NameServer: GetRegions, Authenticate.
   - Master: JoinLobby, Create, Join, JoinRandom.
   - GameServer: rooms, actors, properties with CAS, RaiseEvent with receivers, targets, groups and the event
     cache (add, global, remove by code/sender/content), master-client hand-over, leave and disconnect.
6. P2P: the player who creates a room hosts its GameServer in-process. Other players get a per-host loopback
   address `127.80.x.y:<gamePort>`. Datagrams to it travel over ISteamNetworkingMessages channel `[Photon]
   SteamChannel` (9) as `[1|2][clientPort:2][Photon datagram]`.
7. Finding a room's host (`ResolveHost`), in order:
   1. A room hosted on this machine.
   2. A SteamID64 prefix in the room name (rooms named by the emulator are `<SteamID64>-xxxxxxxx`).
   3. A Steam lobby ID used as the name, which resolves to the lobby owner. R.E.P.O. uses `<lobbyId>` and `<lobbyId>_voice_`.
   4. A friend whose rich presence `refix_photon_room` equals the room name.

## Session acceptance
Incoming ISteamNetworkingMessages sessions are accepted **reactively** on the game's own thread:
`steam_proxy` intercepts `SteamNetworkingMessagesSessionRequest_t` (callback 1251) inside
`ManualDispatch_GetNextCallback` and calls `ReFixSteamFlat::OnSessionRequestCallback`. The emulator's
background pump thread must never enumerate the Steam friends list (`GetFriendByIndex`): doing so races
the game's own Steam calls (e.g. the auth burst during Photon room creation) and crashes inside the
Valve DLL. The pump only touches Steam once the game is actually using Photon (`g_photonActive`).

## Interaction with steam_p2p_hook
Emulator sockets are registered with `SteamP2PHook::ExcludeSocket`. `PreserveLoopback` now protects every
127/8 destination; before this, sends to 5055/5056 on loopback were tunnelled to the single known lobby peer.

## Status (2026-10-25)
- Photon Fusion (Roadside Research, AppId 3643170, Fusion 2.0.8): hosting works on one PC (Host -> New game
  loads into the room). The emulator plays Photon Cloud's Fusion room plugin (`pn_fusion`): it confirms the
  client's Join (event 100 to actor 0) and sends Start pointing at the actor that joined as Server. Shared mode
  (plugin-simulated) is not supported, and joining as a second PC is not yet tested. Fusion detected
  automatically; config is Topology B (ReFixCloud emulator + Steam SDR, port 27015).
- R.E.P.O. (Mono, PUN 2 + Voice): hosting works on one PC. The lobby loads, the room and voice room are created on
  the emulator, and START GAME loads a level. Not yet tested across two PCs.
- Mono games load steam_api64 late, which is too late for the Steam overlay to hook D3D11. Deploy ReFix's
  `winmm.dll` next to the exe as well: it sets the Spacewar environment and injects GameOverlayRenderer64 at
  process start. Verified in R.E.P.O.: Shift+Tab opens the overlay.
- Phasmophobia: out of scope. Its integrity checks detect injected modules (e.g. winmm.dll). It then sends a
  self-report through Cloud Code (`CloudEvents.SelfReport` -> `ModerationModuleBindings.SelfPlayerReport`) and
  aborts. Making it work would mean evading its anti-cheat, so it is not supported.
- Tests: `tests\run_photon_tests.bat` (codec, transport, crypto, room flow, master routing).

## Not implemented
- Photon TCP and WebSocket (WSS) transports. AuthOnceWss would need TLS.
- Datagram encryption, legacy merge/replace caching, webhooks, lobby room lists.
