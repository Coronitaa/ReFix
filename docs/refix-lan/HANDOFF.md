# ReFix LAN / Re:Goldberg — Handoff

## Estado Actual
- **CURRENT PHASE:** Phase 3.6.1 VERIFIED
- **CURRENT COMMIT:** pending commit
- **WORKTREE STATUS:** modifications in `src/unreal_steam_emu.cpp`, `src/steam_proxy.cpp`, `src/network/refix_net_test.cpp`, configs, and docs.
- **TEST RESULTS:** 
  - ABI static assertions verified: `sizeof(SteamNetConnectionInfo_t) == 696`, `sizeof(SteamNetworkingIdentity) == 136`, `sizeof(SteamNetworkingIPAddr) == 18`, `sizeof(SteamNetworkingMessage_t) == 216`.
  - In-process ABI and vtable routing: PASS (all slots strictly matching SDK).
  - Message lifetime allocator: PASS (10,000 cycles, 0 leaks, 0 crashes).
  - Multithreaded locking: PASS (10,000 iterations across 4 threads, 0 deadlocks, 0 crashes, 0 races).
  - Internet-Zero isolation: PASS (public WAN IPs blocked, LAN/loopback allowed).
  - Adversarial Reliable Protocol: PASS across multiple seeds (10% drop / 5% dup / 50-200ms jitter and 20% drop / 10% dup / 50-250ms jitter): 1000/1000 reliable messages delivered, 0 missing, 0 duplicates, 0 sequence order violations.
  - Unreliable protocol: PASS (750+ messages received without stall or retransmission storm).
- **ONLINE REGRESSION STATUS:** UNVERIFIED (Proxy boundary routes straight to Valve DLL in online mode, but runtime online verification is impossible in this offline testbed).
- **LAN STATUS:** Fully Verified.
- **RESOLVED ROOT CAUSES:**
  1. `SteamNetConnectionInfo_t` ABI size: Corrected from erroneous 416 bytes to true SDK size of 696 bytes.
  2. `SocketsHandshakeAck` parsing bug: Receiver was reading `protocolVersion` (1) instead of `ack->sessionId`, causing client connection ACK lookup to fail.
  3. Bidirectional discovery: Implemented unicast discovery ping replies so peers learn each other's endpoints before connecting.
  4. Interface routing: `steam_proxy.cpp` properly routes `SteamNetworkingSockets` and `SteamNetworkingUtils` to `UnrealSteamEmu` in LAN mode, while strictly preserving native Valve DLL passthrough in Online mode.
  5. Localhost multi-process port collision: Configured distinct listen ports (47584 Host, 47585 Client) on localhost so Winsock can direct unicast UDP packets correctly.
- **NEXT EXACT ACTION:** Do NOT advance to Phase 4 until user explicitly directs.

## 2026-11-02 — ReFix LAN standalone (branch `refix_lan`)
- **Online merged selectively** (`feature/photon-p2p-phasmophobia-interop`): Steam core keeps the LAN
  architecture; Steam-independent fixes ported (log truncation, AppID log-once, deploy/uninstall fixes).
  Online-only modules (Photon emulator, Fusion Shared, Party Animals, store/inventory, IL2CPP, UNAE) are in
  the tree but not compiled into the LAN build.
- **LAN edition** (`/DREFIX_LAN_EDITION=1` in `build.bat`): `NetworkModeManager` never selects Online
  (ini, env or `ReFix_SetNetworkMode(0)` all clamp to LAN), so the Valve DLL and Steam client are never used.
- **Identity** (`[User] IdentityMode`): `auto` (default) derives the SteamID from the PC's MachineGuid + instance
  slot and ignores SteamIDs copied in `ReFix.ini` / `steam_settings`; `fixed` honours them. Name falls back to
  the Windows user name. A peer announcing our SteamID from another PC is logged as `IDENTITY COLLISION`.
- **Deploy (lan mode)**: no Goldberg backend on x64; in auto mode no `force_steamid.txt` is written.
- **Tests**: all unit suites, `refix_net_test entrypoints`, `tests/test_identity_mode.ps1`,
  `tests/test_steam_adapter_multiprocess.ps1` PASS. Two physical PCs: not yet validated.

## 2026-11-02 (2) — Dual product AutoDeploy, 32-bit LAN, physical tests
- **Layout**: `bin\online\` = ReFix Online (built by `build_online.bat` from a detached worktree of
  `feature/photon-p2p-phasmophobia-interop` in `build\online_src`: steam_api64, winmm, EOS, x86 proxy, ReFix\FusionShared).
  `bin\lan\` = ReFix LAN (`build.bat`, which also runs `build_x86_lan.bat` for `bin\lan\x86\steam_api.dll`).
  AutoDeploy option 1 deploys only `bin\online` + its ReFix.ini; option 2 only `bin\lan`. deploy_helper refuses to
  run if the chosen product folder is missing (no fallback that could mix products).
- **32-bit LAN**: modern SDK games (exports `SteamInternal_ContextInit`, e.g. Overcooked! 2 via CSteamworks) get the
  native x86 ReFix LAN emulator; legacy SDK games (CS 1.6, early CS:GO, L4D2) get standalone Goldberg x86
  (no Steam either). Legacy old-interface vtables (SteamUser016, SteamFriends013...) are not in the ReFix emulator yet.
- **One emulator per process** (`src/refix_module_dedupe.cpp`): a second ReFix copy loaded in the same process
  (root + Plugins) redirects its exports to the first via MinHook.
- **Fixes from physical runs**: PeerDiscovered re-fired when a peer's SteamID arrives late (one-way discovery);
  EOS→P2P hook IP byte order; AutoDeploy firewall prompt honoured "N" (was always applied); unattended deploy
  (`REFIX_NON_INTERACTIVE=1`, `REFIX_DEPLOY_MODE=1|2`) never touches the firewall unless `REFIX_FIREWALL=Y`.
- **Physical harness**: `tests\physical\run_lan_pair.ps1` (+ `lan_matrix.csv`): backup → deploy → 2 instances →
  log analysis → restore (byte-identical). `-Product online` verifies the Online deploy statically.

## 2026-11-02 (3) — Source/GoldSrc, multi-instance and discovery hardening
- Versioned interface adapters now include SteamGameServer001-014 (149 adapters); x86 exports used by Source
  engines (g_pSteamClientGameServer, SteamGameServer_GetIPCCallCount, SteamInternal_GameServer_Init_V2).
- Multi-instance for Valve engines: ReFix LAN closes the process's own handle to `ValveHalfLifeLauncherMutex` /
  `hl2_singleton_mutex` after init (`src/refix_multi_instance.cpp`, `[User] AllowMultipleInstances`, default on).
- Instance slots keyed by exe name; AppID learned late (game-written steam_appid.txt, RestartAppIfNecessary)
  is adopted at runtime and moves LanCore to the new scope (`ILanCore::SetAppScope`).
- Discovery: EOS uses ports 47610-47615; local peer refreshed before stale pruning (beacons used to carry
  SteamID 0 after 12 s); idle Tick pump thread for games that stop calling SteamAPI_RunCallbacks;
  PeerDiscovered re-posted on SteamID change and replayed after subscription.
- Diagnostics: `REFIX_LOG_VERBOSE=1` (pid/tid/module prefix), `REFIX_LAN_TRACE=1` (packet trace).
- Games that enforce a single instance by themselves (Scam With Your Friends, Shift At Midnight) or need UI
  input before Steam init (Lethal Company's ONLINE/LAN menu) need two PCs or manual input to test.

## 2026-11-02 (4) - Create/join verified in-game (two instances, one PC)
- MECCHA CHAMELEON (Unreal + Redpoint EOS): enters the same room. Fixes: no raw-UDP relay redirection in LAN
  builds (`kRedirectRawUdpToRelay`), EOS directory sends/joins direct over UDP, Steam-mirror lobby TTL clock,
  `EOS_P2P_SetPacketQueueSize(0)` = unlimited, P2P receiver installed at startup, SendPacket implicit accept,
  EOS_Sanctions answered with "no sanctions".
- Left 4 Dead (Source x86): lobby creation (LobbyEnter_t grace for late listeners), lobby list (case-insensitive
  lobby keys, GetLobbyDataCount/ByIndex for LAN lobbies), join (lobby chat over LanCore channel 249).
  Both instances load into the same map.
- Valheim (Unity, Steamworks.NET): world listed under Community (GetLobbyGameServer for LAN lobbies and
  SteamID-only P2P hosts); SteamNetworkingIdentity/IPAddr flat helpers implemented, so ConnectP2P reaches
  Connected on both sides and the host's password prompt arrives. Password step not completed under automation.
- Release package is generated by `tools/make_release.ps1`; never edit ReFix_LAN_2.0_Release by hand.
- `REFIX_LOG_VERBOSE=1` also traces lobby reads (`[LobbyRead]`); `[Sockets]` logs connection state changes.

## 2026-11-02 (6) - Physical verification of (5)
- `run_lan_pair.ps1` over all 13 games of `lan_matrix.csv`: PASS (2 instances, distinct SteamIDs/slots, mutual
  discovery), including Scam With Your Friends and Shift At Midnight (Unity single-instance) and How to Fish (no
  more InvalidProgramException in SteamAPI.InitEx).
- Valheim in-game: host with password, second instance joins from Community, enters the password and spawns in
  the world; world save and Steam cloud save (`Cloud Save: ... Succeess`) without "Cloud save failed".
- Unity "Force Single Instance": the mutex is released when the first instance reaches Steam init (our DLL is not
  loaded earlier), so the second instance must be started after the first reaches its menu. The harness default
  gap is now 15 s (with 10 s, Scam's second instance sometimes exited).
- R.E.P.O logs a Photon name-server connect failure: Photon cloud is internet-only and blocked in LAN builds.

## 2026-11-02 (5) - Generic fixes (not yet verified in-game)
- Flat API: every `SteamAPI_ISteamXxx_*` export without a hand-written LAN wrapper used to hit
  `SafeUnsupportedExportStub` (return 0) - 892 of 1141 names, e.g. cloud-save streams, friends, stats.
  `tools/gen_steam_flat_dispatch.py` now generates `src/generated/flat_dispatch.cpp` from `steam_api_flat.h`:
  one thunk per method of the latest SDK class, calling the emulator object (`ReFixLatestInterface`).
  `LookupLanExport` order: hand-written table, generated thunk, stub. Re-run the generator after SDK updates.
  The game's `self` is used only when it has the latest object's vtable; adapters and null fall back to it.
- ISteamRemoteStorage: FileWriteStream* buffer and write on Close (Valheim logged "Cloud save failed"),
  FileReadAsync/FileReadAsyncComplete return real data, sub-folders in cloud file names are created.
- Unity "Force Single Instance": the `...-SingleInstanceMutex-...` mutex is released on Steam init, like the
  GoldSrc/Source ones (Scam With Your Friends and Shift At Midnight only started one instance).
- Mono.Cecil patches clear ExceptionHandlers with the body (How to Fish: InvalidProgramException in
  SteamAPI.InitEx from a stale try/finally) and re-patch from the `.orig` backup when it has the same assembly
  identity, so broken earlier patches are repaired on redeploy.
- `refix_net_test all` fails "Host failed to create incoming connection from handshake" (legacy UDP path);
  it fails the same way on 0b599d6, so it is not caused by these changes.

## 2026-11-03 (7) - Photon over LAN, GRAIN ROT public list, CS 1.6 / Garry's Mod start
- **Photon in ReFix LAN**: the Photon emulator (`src/photon`, shared with Online) is now built into the LAN DLLs.
  `src/photon/pn_lan_backend.cpp` implements the `ReFixSteamFlat` calls it uses on top of UnrealSteamEmu + LanCore
  (LanCore channel 248, delivered on the transport thread via the new `ILanCore::SetChannelHandler`, so tunnel
  traffic skips the per-frame callback queue). Started from `ReFixInitializePost`; enabled by `[Photon] Backend =
  ReFixCloud` or, without that key, automatically when the game ships Photon/Fusion (Mono assemblies or IL2CPP
  metadata). Several instances on one PC: instance k listens on `127.0.0.(1+k)` (DNS hook follows) and puts its
  forwarders in `127.(80+k)/16`.
- **Photon room lists**: hosts re-announce their public rooms every 3 s, peers drop rooms silent for 10 s, and
  JoinLobby waits 300 ms for answers before sending the GameList (R.E.P.O. reads only the first list and
  disconnects); rooms that arrive later go out as GameListUpdate (event 229).
- **Verified in-game (2 instances, one PC)**: R.E.P.O. public server list shows the host's server, the second
  instance joins (room + voice room) and both load the level when the host starts.
- **GRAIN ROT (UE 5.7)**: the public list stayed empty because the emulator added untyped keys (`HostAddress`,
  `HostSteamID`, `SERVER_IP`, plus LanCore `__` bookkeeping copied by RequestLobbyData) and OnlineSubsystemSteam
  dropped the lobby. Games now see only the keys they set. Verified: listed and joined (P2P connection state 3).
  Note: the game has 5 save slots; "Nueva partida" does nothing when all are used.
- **Counter-Strike 1.6**: crashed creating a server: SDK <= 1.47 `SteamInternal_GameServer_Init` has an extra
  `usSteamPort`; both layouts are now told apart by the 5th argument. Verified: `+map de_dust2` listen server runs.
- **Garry's Mod**: hung on start (spin on `CheckFileSignature`, which returned call 0) and then crashed
  (`STEAMUSERSTATS_INTERFACE_VERSION013` matched the "STEAMUSER" prefix and got ISteamUser). Verified: menu and
  `+map gm_construct`.
- SDK 1.61 behaviour: `UserStatsReceived_t` for the local user is posted 1 s after init.
- Diagnostics: `src/refix_crash_log.cpp` logs fatal exceptions raised inside ReFix with a stack scan
  (module+offset, resolve with the .map). `REFIX_LOG_VERBOSE=1` also traces lobby reads by index and ISteamUserStats.
- Not verified: two-PC runs of these games; CS/GMod joining a second instance's server.
