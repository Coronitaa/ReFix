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

## 2026-11-03 (8) - CS:GO 1.37 (Source, SDK ~1.39): generic Steamworks gaps
- Missing export `SteamGameServerInternal_CreateInterface` (pre-1.44 SDKs): the loader stopped on a hidden
  "entry point not found" box. Exported (x86 and x64 .def); Online forwards to Valve's DLL.
- `ISteamClient017::GetISteamUnifiedMessages` (marked DEPRECATED_ in our header) returned null and Source's
  CSteamAPIContext aborted with "Failed to connect with local Steam Client process". The adapter generator now routes
  `DEPRECATED_GetISteam*` through the version lookup and the emulator serves a stub ISteamUnifiedMessages.
- `SteamController00N` resolved to the ISteamInput object (different vtable; inputsystem.dll crashed). New
  CSteamControllerEmu plus a `controller` family in `tools/gen_steam_iface_adapters.py` (003-007 adapters; 002 skipped,
  its header has placeholders).
- `SteamGameCoordinator001`: stub that accepts and drops messages and never delivers any. The menu shows "connecting
  to the CS:GO network" forever, which is expected (official matchmaking, inventory and ranks need Valve's GC).
- Game servers no longer claim VAC-secure (`BSecure` / `GSPolicyResponse_t`): a VAC-secure listen server rejected
  clients started with `-insecure`. There is no VAC on LAN.
- Diagnostics: `REFIX_CRASH_ALL=1` also logs fatal exceptions outside ReFix (vtable mismatches crash in the game)
  and prints printable strings found on the stack, which is how the fatal error text above was found.
- Verified (two instances, one PC): `+sv_lan 1 +map de_dust2` listen server with bots; the second instance (own
  SteamID, `Player_2`) joins with `connect 192.168.1.3:27015`. `connect 127.0.0.1:27015` from the second instance
  fails on one PC (its own socket also holds 27015 on loopback); use the LAN IP.
- Not verified: LAN tab of the server browser, two PCs. `SteamGameStats001` / `SteamAppDisableUpdate001` stay
  unrecognised (null) and CS:GO copes.

## 2026-11-03 (9) - Generic GC session handshake (src/steam_gc_emu.cpp)
- ISteamGameCoordinator now answers the GC SDK base handshake shared by Valve's GC games: proto ClientHello (4006)
  -> ClientWelcome (4004: hello version echoed + empty SO cache for the local SteamID) + ConnectionStatus (4009,
  HAVE_SESSION); ServerHello (4007) -> ServerWelcome (4005) + ServerConnectionStatus (4010). Replies are queued and
  announced with GCMessageAvailable_t; a pipe with unread replies gets no new ones.
- The game-server pipe has its own coordinator (`GetGenericInterfaceForUser`; the adapter generator passes the user
  handle for GetISteamGenericInterface), so a listen server and its client do not read each other's messages.
- CS:GO result: the "connecting to the CS:GO network" banner is gone and the official Play menu opens (Casual /
  Deathmatch / Wingman maps selectable). Starting a queue is refused by CS:GO itself: with `-insecure` ("insecure
  mode"), without it because the ReFix steam_api.dll is not Valve-signed (VAC file-signature check). That check is
  anti-cheat and is deliberately not bypassed. Game-specific GC messages (matchmaking, ranks, items) still get no
  answer; a real LAN matchmaker for CS:GO would be an addon, not core.
- Listen server (`+map de_dust2`) still verified with the coordinator active (the server reads its welcome once
  the map runs); second-instance join not re-tested after this change.

## 2026-11-03 (10) - ReFix LAN overlay (src/overlay): profiles, LAN players, invites
- Own overlay instead of Steam's: a top-most DirectComposition window over the game's client area (D3D11 + ImGui
  on its own thread). Nothing is injected into the game's renderer, no game or system code is patched and there are
  no global hooks (Shift+Tab is polled with GetAsyncKeyState only while a window of the game has the focus).
  `steam_overlay_block.cpp` only sets `DISABLE_VK_LAYER_VALVE_steam_overlay_1` and `SteamNoOverlayUIDrawing`.
- Flow: DllMain -> `ReFixOverlay::ArmEarly` (LAN only); `LoadConfig` -> `WaitForRestartParent` + `ResolveIdentity`
  (only with IdentityMode=auto and no SteamID forced through the environment); `ReFixInitializePost` -> `Start`
  after `UnrealSteamEmu::Initialize`. With a profile active the instance slot no longer offsets the SteamID (each
  instance holds a different profile; the listen port offset stays).
- Profile choice: `REFIX_PROFILE_ID` (waits up to 30 s for its per-exe mutex), then the last used if free, then the
  most recent free one; none at all -> first-run dialog ("Ahora no" keeps the machine identity); all in use ->
  "¿Quién juega?" picker. Console processes and `-batchmode` / `-nographics` / `-server` never use profiles or the
  overlay, so the console tests and dedicated servers never block on a window. `[Overlay] Enabled`,
  `BlockSteamOverlay`, `IsolateSaves` in ReFix.ini (all true by default; added to ReFix.ini, deploy_helper.ps1 and
  AutoDeploy.sh) and `REFIX_NO_OVERLAY=1`.
- Profile switch restarts the game: the old process restores the identity variables it was launched with,
  sets `REFIX_PROFILE_ID` and `REFIX_RESTART_PID`, relaunches and terminates; the new process waits for it.
- Emulator: `ActivateGameOverlay*` (LobbyInvite -> invite picker, ToUser, WebPage/Store -> browser, InviteDialog,
  InviteDialogConnectString, RemotePlayTogether), `InviteUserToGame`, `InviteUserToLobby` and `IsOverlayEnabled`
  go to the overlay; `Overlay_LocalLobby`, `Overlay_ConnectString` (rich presence under a new mutex) and
  `Overlay_SetPersonaName` (buffer reserved to 256 so `GetPersonaName()` pointers stay valid) implemented.
- Fix found while testing: `GetFriendCount`/`GetFriendByIndex`/`HasFriend` ignored the flags, so every LAN peer was
  also "blocked". Lethal Company refused to join ("An error occured!", "blocked user: ..."). LAN peers now count
  only for `k_EFriendFlagImmediate`.
- Saves, without hooks (`save_redirect.cpp` stays out of the build):
  (a) Steam Cloud per profile: `CSteamRemoteStorageEmu` uses `%APPDATA%\ReFix LAN\Profiles\<id>\Cloud\<appid>`
      (UTF-8 names, wide paths); without a profile it is the old relative `saves/` with ANSI names, unchanged.
      `GetUserDataFolder` returns that folder as an 8.3 path (falls back to "saves" when it cannot be ANSI).
  (b) Known save folders swapped at start, before the game reads them (`save_swap.cpp`): Unreal
      `%LOCALAPPDATA%\<Project>\Saved\SaveGames`, Unity `LocalLow\<Company>\<Product>` from `<exe>_Data\app.info`
      (Player.log / Player-prev.log / output_log.txt / Unity stay). The folder's owner is in
      `%APPDATA%\ReFix LAN\saves.ini`; the owner's files go to `Profiles\<owner>\Saves\<label>` and the new
      profile's come back. All or nothing per folder (refuses on name clashes, rolls back on a locked file).
      Saves found before any profile belong to the profile created with "Usar las partidas guardadas que ya hay
      en este PC", or to `%APPDATA%\ReFix LAN\Saves\_pc`; nothing is deleted.
  Not covered: two instances of the same game open at once with different profiles (the second one finds the
  first running, skips the swap and plays on the folder of the profile that is in place); playing without a
  profile ("Ahora no") uses whatever profile's saves are in place; Unity PlayerPrefs in the registry; saves in
  Documents, Saved Games, the game folder or engine-specific paths; Unity mod settings stored in LocalLow are
  swapped together with the saves; a crash in the middle of a swap leaves the moved files in the stash (logged
  as `[Saves]`).
- Build: `src\overlay\*.cpp` (except save_redirect) + imgui (core, draw, widgets, tables, impl_win32, impl_dx11) in
  the LAN DLL, the test DLL and x86. d3d11, dcomp, d3dcompiler_47, shell32, ole32, dwmapi, gdi32 and imm32 are
  delay-loaded (dumpbin: the normal import table is the same as before minus ole32, which moved to delayed);
  dxgi and windowscodecs are not linked because nothing imports them (LNK4199). LAN builds now use `/utf-8`
  (Spanish UI strings were compiled to CP1252 and showed "?" / mojibake).
- Verified on this PC with Lethal Company (two instances, exclusive fullscreen): first-run dialog, "Importar desde
  Steam" error with Steam closed, picker when the profile is in use, distinct SteamIDs per instance (profile IDs,
  slot 1 without offset), welcome toast, Shift+Tab overlay with the community card, player list ("En una sala ·
  1/4"), favourites per profile, Join from the overlay (GameLobbyJoinRequested -> the second instance entered the
  host's ship), the game's "Invite friends" opening the invite picker, the invite toast on the other instance,
  restart into another profile with the save swap back. Existing tests pass (identity modes, multi-instance,
  module dedupe, iface versions, 3-instance mesh).
- Fixed during the test: the host thread deadlocked against the game thread (GetWindowText sends WM_GETTEXT;
  now InternalGetWindowText); fullscreen games that minimise when the overlay takes the focus keep the overlay
  on their last rect (solid backdrop) and are restored on close; a fullscreen game that lost the focus to the
  profile dialog is minimised and restored so it presents again; Shift+Tab taps shorter than the poll interval;
  Escape closing a menu no longer closes the overlay too.
- Known limits: toasts cannot be clicked while the game holds the mouse (in-game cursor lock): open Shift+Tab, the
  notifications are drawn inside the overlay there. No overlay over exclusive-fullscreen games that do not
  minimise and do not let a top-most window through. "Importar imagen…" (IFileOpenDialog), the profile editor
  in the overlay and the Community links were not exercised in-game; two real PCs not tested.

## 2026-11-03 (11) - Overlay: English/Spanish, second-instance picker, guest, delete profiles, test release
- Languages: every UI string goes through `i18n::T("English", "Español")` / `TW(...)` (`overlay_i18n.h/.cpp`).
  English by default; the choice is `[General] Language=en|es` in `%APPDATA%\ReFix LAN\profiles.ini` and changes
  live from the EN|ES pill (top bar of the overlay, top right of the profile dialog).
- Second instance of the same exe on one PC: always gets "Who's playing?" (no silent pick of the last profile).
  Profiles held by a running game are dimmed with a lock and "Playing now"; free ones say "Available"; a "+" card
  creates a new profile (up to 10); "I'd rather play as a guest" gives a guest. The first-run dialog only shows
  with no profiles and no other instance. "In use" is a second mutex `Local\ReFixLAN_ProfileAny_<id>` held for
  any game, next to the per-exe one.
- Guest: id `guest`, standard name "Guest"/"Invitado", stock avatar, its own random SteamID, never written to
  disk; no favourites, no save swap; Steam Cloud goes to `%TEMP%\ReFix LAN\Guest-<pid>\Cloud\<appid>`, removed
  on a later start once that pid is gone. Games that save outside Steam Cloud (Lethal Company: LocalLow) share
  the save folder in place, same as any second instance (see (10), "Not covered").
- Delete: trash icon on hover in the picker and in the chip menu, only for profiles no game is using (not the
  active one). Confirmation modal with an "I understand, delete it" box that enables the red button.
  `profiles::Remove` refuses a profile in use, removes profile.ini then the folder (avatar, Cloud, stashed
  saves), clears LastProfile, and `saves::ForgetOwner` drops it from saves.ini so the folder in place is treated
  as pre-profile data.
- Verified in-game (Lethal Company, two instances): picker on both instances, lock on the profile of the other
  instance, delete with the box unticked does nothing and ticked removes the folder (`[Profiles] Profile ...
  deleted`), ES switch in the picker and EN switch in the overlay, guest "Invitado" with its own SteamID and the
  other instance in its player list. x64 and x86 rebuilt; identity modes, multi-instance, module dedupe, iface
  versions and the 3-instance mesh pass.
- Test package: `tools\make_release.ps1 -OutDir ReFix_LAN_Test_Release` (kept out of git with .git/info/exclude;
  `ReFix_LAN_2.0_Release` is the tracked package and was not touched).

## 2026-11-03 (12) - Overlay browser (WebView2), AutoDeploy option 2 closing, "Welcome to ReFix"
- AutoDeploy.bat closed itself on option [2] (ReFix LAN): the working copy had LF line endings (the index is LF,
  `.gitattributes` asks for CRLF but the file had been rewritten), and with LF cmd.exe misses labels
  ("No se encuentra la etiqueta por lotes especificada: GOLDBERG_PROMPTS" / SUCCESS_EXIT) and exits.
  make_release.ps1 copied the file as it was. Working copies of AutoDeploy.bat, build.bat and build_x86_lan.bat
  are CRLF again, and make_release.ps1 now writes every .bat/.cmd of the package as CRLF and every .sh as LF
  (bash would fail with `$'\r'` the other way round). Checked: LAN deploy of a Unity test folder with the CRLF
  copy succeeds and with an LF copy fails on GOLDBERG_PROMPTS; `bash AutoDeploy.sh <folder>` with option 2 works.
- Web pages in the overlay, like Steam: the Community hub tiles and the game's `ActivateGameOverlayToWebPage` /
  `ActivateGameOverlayToStore` open a browser modal inside the overlay (back, forward, reload/stop, title and
  address, open in the system browser, close; Esc closes it, also while the page has the keyboard).
  `overlay_web.cpp` hosts WebView2 (windowed hosting) as a child window of the overlay window on the host thread,
  started on the first page only; the DirectComposition target is now created non-topmost so that child window is
  drawn above the ImGui visual. Only http/https pages; links that open a new window stay in the same view. The page
  stays loaded while the overlay is closed and comes back with it; closing the modal unloads it (video/sound stop).
  Browser data (cookies, logins) in `%APPDATA%\ReFix LAN\WebView2`. Without the WebView2 runtime the old behaviour
  stays (system browser).
- WebView2 SDK 1.0.2903.40 vendored in `src/webview2` (headers + WebView2LoaderStatic.lib x64/x86, BSD licence):
  the static loader is linked, nothing extra ships. dumpbin: the import tables of the x64 and x86 DLLs are the same
  as before (no new DLL loaded at start); the DLLs grow by about 0.9 MB.
- First-run dialog title: "Welcome to ReFix" / "Te damos la bienvenida a ReFix".
- Verified in Lethal Company: Store, Community and News pages in the modal, scrolling, video, a link, Back,
  Shift+Tab with the page focused (closes and reopens with the page), Esc in the page, Esc again closes the overlay,
  X button; no msedgewebview2 process left after the game exits. x64/x86 rebuilt, tests pass.
- Known limits: ImGui tooltips and menus cannot draw over the page area (it is a separate window); the page is not
  dimmed behind other overlay dialogs (none open while the browser is up).

## 2026-11-03 (13) - Profile dialog over the game (no ghost window), import from another ReFix game
- The game "minimised" behind the profile dialog: it is not minimised. The game thread waits inside
  `SteamAPI_Init` (ResolveIdentity -> RunDialog) and after 5 s Windows treats its window as hung and puts a dwm
  "Ghost" window in its place, which cannot show DirectX/flip content, so the desktop showed through. RunDialog now
  calls `DisableProcessWindowsGhosting()` (documented user32, no hook) just before the wait, only in games that show
  the dialog: the game's last frame stays under the dialog. Checked with two Lethal Company instances: no Ghost
  window, `IsHungAppWindow` true but the game visible around the picker.
- The minimise/restore "kick" after the dialog is gone (it was hiding the same ghost problem): after the dialog the
  game keeps presenting without it. Closing the dialog/overlay now hands the foreground back to the game before
  hiding the overlay window (hiding first activated the desktop and then SetForegroundWindow was refused, so the
  game was left without focus and Shift+Tab did nothing until it was clicked).
- "Import from another ReFix game..." in the profile editor (`identity_import.cpp`, editor importers moved to
  `overlay_ui_import.cpp`): pick a game folder (any level above its ReFix.ini, up to 4 folders down) or a ReFix LAN
  profile folder. Name and SteamID come from the last `[ConfigDiag]` lines of that game's ReFix.log (what the
  emulator really used), then ReFix.ini [User], then steam_settings (force_steamid.txt, user_steam_id.txt,
  force_account_name.txt, configs.user.ini); IdentityMode=auto without a fixed SteamId gives this PC's automatic
  SteamID (`UnrealSteamEmu::Overlay_MachineSteamID`). Picture: steam_settings `account_avatar.*` or the profile's
  avatar.png. A game whose log shows `SteamID Source: 'profile'` is refused (its profiles are already the shared
  ones). Duplicate SteamIDs are refused like the Steam import. `tests/test_identity_import.cpp` (23 checks, built by
  build.bat); checked in game with D:\Games\CSGO (Player_4903 + the PC's automatic SteamID, saved in profile.ini).
- make_release.ps1 empties the output folder instead of deleting it (fails when File Explorer has it open).
- Open question: one run started from AutoDeploy at 17:42 showed the first-run dialog with three profiles on disk
  and moved the existing Lethal Company saves to `Saves\_pc` although saves.ini named their owner: in that process
  the existing ini files read back empty. Logging was off (EnableLog=false in the deployed ReFix.ini) and
  launching the same way again did not reproduce it. The saves were put back by hand (owner Valen; the new profile's
  data in its own stash).

## 2026-11-03 (14) - Overlay over fullscreen games without minimising them, controllers, scale, GPU loss
- Shift+Tab minimised fullscreen-window Unity games: they minimise on `WM_ACTIVATE(WA_INACTIVE)` (checked by
  posting it to Lethal Company; `WM_KILLFOCUS` alone does nothing). While the overlay is open its window is now a
  child of the game window (`BecomeChild`/`BecomePopup` in overlay_host.cpp): keyboard focus moves into the child
  and the game's top-level window never loses the activation. Dialogs and toasts stay popups. Games in real
  exclusive Direct3D fullscreen (`SHQueryUserNotificationState` == `QUNS_RUNNING_D3D_FULL_SCREEN`) keep the popup
  (and minimise as before): a child would be hidden behind the exclusive image. Do not use WS_EX_TOPMOST as the
  test: Unity sets it on its fullscreen window. The decision is logged when the popup is used.
- While the overlay is a child the game still has the activation, so its raw mouse/keyboard (camera, movement)
  kept running: the process's mouse/keyboard raw-input registrations are removed while the overlay is open and
  re-registered exactly as they were (`GetRegisteredRawInputDevices`, documented API, no hook). The cursor clip is
  freed every frame and ImGui draws its own cursor when the OS cursor is hidden. Checked in a Lethal Company
  match: pointer usable, the camera does not move under the overlay, the game is never minimised.
- Controllers (`overlay_gamepad.cpp`): XInput (xinput1_4/1_3/9_1_0 loaded on use, ordinal 100 for the Guide
  button) and DualShock 4 / DualSense over USB and Bluetooth read from HID (shared, read-only; hid.dll and
  setupapi.dll delay-loaded). Guide/PS or View+Menu (Share+Options) toggles the overlay; ImGui gamepad
  navigation inside (A/Cross activates, B/Circle goes back and closes the overlay or the browser). ImGui's own
  XInput code is off (`IMGUI_IMPL_WIN32_DISABLE_GAMEPAD` in imconfig.h). Read only while the game has the
  foreground or the overlay is open. `tests/test_gamepad.cpp` (19 checks: report layouts, toggle) and a live read
  of the Xbox 360 pad on this PC; the buttons were not pressed in a game here. Games keep reading the pad too
  (XInput has no exclusivity) while the overlay is open.
- Scale: from the game's client height (monitor height when there is no game picture), 1080 px -> 1.2, clamp
  0.8..3.0 in 0.05 steps; caption font 14.5 px. Text and every size follow the game resolution.
- Welcome toast: "Press [Shift] [Tab] to open ReFix", every piece centred on one line (`KeycapAt`).
- Icons are centred on their glyph box (`GlyphCentered`); the browser toolbar's tooltips open above the buttons
  (`SetTooltipsAbove`), the page is a separate window that covered them.
- GPU: `Present`/`ResizeBuffers` failures with DEVICE_REMOVED/RESET recreate device, swap chain, composition
  and ImGui's DX11 objects (driver update, TDR, laptop GPU switch); D3D11 feature levels 11_0..10_0, WARP if no
  hardware device. Nothing in the overlay is vendor specific (D3D11 + DirectComposition only).
- 32-bit: the x86 DLL was run in Left 4 Dead (Source, D3D9): profile picked, overlay started, welcome toast
  window created, no D3D11/DirectComposition errors (Shift+Tab inside it not exercised). Import tables of the x64
  and x86 DLLs unchanged; hid/setupapi only in the delay-load table.

## 2026-11-03 (15) - Controller navigation, game input under the overlay, sounds, deploy/uninstall
- Controller (tested by the user: Guide opened the overlay, B closed it, but the D-pad did nothing and the game
  kept moving): ImGui 1.91 keeps `InvisibleButton` out of keyboard/gamepad navigation unless it gets
  `ImGuiButtonFlags_EnableNav`, and every tile, card, row and icon button of the overlay is one. They all go
  through `ui::NavButton` now. Found with ImGui's own nav log (`ImGuiDebugLogFlags_EventNav`): the nav init
  skipped the whole top bar and picked the search box. Also: child windows use `ImGuiChildFlags_NavFlattened`, the
  overlay window gets the focus when it opens, the Tab of the Shift+Tab hotkey no longer reaches ImGui (it
  started editing the search box), and Escape/B do not close the overlay while a field is being edited.
  Checked in Lethal Company with the arrow keys + Enter (the same navigation the D-pad + A use).
- Game input while the overlay is open: joystick/gamepad/multi-axis raw input (PlayStation pads, DirectInput)
  is suspended with mouse and keyboard; XInput cannot be taken away without hooks, so the game is told it went to
  the background with `WM_ACTIVATEAPP(FALSE)` (checked: Lethal Company does not minimise on it, unlike
  `WM_ACTIVATE`) and `WM_ACTIVATEAPP(TRUE)` on close. Games that keep reading XInput in the background will still
  see the pad; not verified with a pad here.
- Sounds (`overlay_sound.cpp`): open, close, select, focus move (keyboard/controller only), invite, not
  possible. Sine tones synthesised in memory, soft attack, mostly under -20 dBFS, on the overlay's own waveOut
  stream (not PlaySound, which would cut a game's own PlaySound); winmm delay-loaded. `[Overlay] Sounds = false`
  turns them off.
- Overlay choice: AutoDeploy.bat / AutoDeploy.sh ask in LAN mode for ReFix overlay / Steam overlay / none
  (`REFIX_OVERLAY_MODE` 1-3 or refix/steam/none for unattended deploys; 4th argument of AutoDeploy.sh;
  `deploy_helper.ps1 -OverlayMode`): Enabled/BlockSteamOverlay true/true, false/false, false/true. The Steam
  overlay block no longer depends on Enabled (it did, so "none" left Steam's overlay on).
- Shipped ini: `VerboseArbitrationLog = false` (was true) in ReFix.ini and the deploy template; EnableLog and
  EnableConsole were already false.
- Uninstall: `bin/restore_saves.ps1` (Uninstall_ReFix.bat) and `restore_saves` in `AutoDeploy.sh --uninstall`
  (Wine/Proton prefix, iconv for the UTF-16 inis) give the game this PC's own saves back before the DLLs are
  restored: the profile that kept the PC's saves (NativeSaves=1) or `Saves\_pc`; the saves in place go to their
  profile. Nothing deleted, profiles stay. Tested in sandboxes for both cases and on a real uninstall.

## 2026-11-03 (16) - %APPDATA%\ReFix, profiles per game, "Import from another game", 2.1.0
- Profile root renamed to `%APPDATA%\ReFix` (guest Cloud folders: `%TEMP%\ReFix`). `RootDir()` merges the old
  `%APPDATA%\ReFix LAN` into it once per process (mutex `Local\ReFix_RootMigration`): missing entries are moved,
  folders on both sides merged one level down, saves.ini owners combined (the new file wins), profiles.ini and
  the WebView2 cache of the new folder kept; what is in use moves on a later start. `%APPDATA%\ReFix` already
  existed on this PC (ReFix Online's EOS `user_profile.json`), so a plain rename was never possible.
  Found while testing: this PC had a second set of profiles in `ReFix LAN` that the first version of the migration
  left behind (Rayito, Lunita, Spanish, LastProfile=Lunita) next to the set the game was using (Valen, Mona China);
  how the two sets came about is not known. That second set is what the 17:42 run of (13) saw, which explains its
  first-run dialog. Both are merged now; the save owner (Valen) kept.
- Profiles per game: profile.ini has `[Profile] OriginGame=<exe key>` and `[Games] <exe key>=<name>` (exe file name,
  replaced by the window title once the overlay sees it). A game lists the profiles used in it; profiles from
  before this (no OriginGame) are listed in every game, so nothing disappears. 10 profiles per game, 100 on the PC.
  `PickProfile`, the picker, the first-run decision, the chip menu and the save swap only use this game's
  profiles; unique ids/SteamIDs and the Steam import duplicate check use all of them (a Steam account already used
  in another game: "import it from that game").
- "Import from another game" (editor button when creating, and a "From another game" card in the picker): a grid
  of every profile of the PC this game does not list yet, with picture, name and origin game ("R.E.P.O. +1" when
  one more game used it). Choosing one adds this game to it (same profile: name, picture, SteamID), then plays with
  it (dialog) or offers the restart (overlay). The folder-based importer of (13) is gone (`identity_import.*`,
  its test and `Overlay_MachineSteamID`). Profiles are reloaded every time the overlay opens.
- Checked in Lethal Company with three profiles written as other games would write them (R.E.P.O. +1, PEAK,
  Overcooked! 2 +2, one custom picture): grid, import (Lethal Company added to its [Games]), switch confirmation,
  listed in the chip menu afterwards. Test profiles removed afterwards.
- Uninstallers look in `%APPDATA%\ReFix` (and `ReFix LAN` when it was never migrated); AutoDeploy title, README and
  `make_release.ps1` default output: 2.1.0 (`release\ReFix_v2.1.0`).

## 2026-11-04 (17) - Profile per-game strict isolation and multi-profile savedata inheritance
- **Strict per-game profile isolation**: `UsedIn(p, gameKey)` now strictly requires `gameKey` to be present in `p.games` (or `p.originGame == gameKey`). Removed the legacy `if (p.originGame.empty()) return true;` fallback that caused profiles with an empty originGame to bleed into every new game's active profile list. Legacy profiles or profiles from other games now exclusively appear in "Import from another game" (`FromOtherGames`).
- **Per-game LastProfile**: `LastProfileId(gameKey)` and `SetLastProfileId(id, gameKey)` now read and write per-game entries under `[LastProfile] <gameKey>=<id>` in `profiles.ini` (while maintaining `[General] LastProfile` as fallback only if used in that game). Brand new games have no last profile recorded, `ForGame` is empty, and `PickProfile` returns empty, correctly routing to the FirstRun dialog instead of auto-logging into a foreign profile.
- **Pristine original savedata preservation & multi-profile inheritance**:
  - `save_swap.cpp` now creates a pristine snapshot of pre-ReFix saves in `Saves\_pc\<label>` upon first encounter before any gameplay modifications occur.
  - `HasOriginalSaves()` and `InheritOriginalSaves(profileId)` allow any profile (during FirstRun, picker, or newly created from the overlay) to inherit the original saves by copying `Saves\_pc\<label>` into `Profiles\<id>\Saves\<label>`.
  - Multiple profiles can each inherit the original save independently; each plays with its own isolated copy without mutating `_pc` or other profiles.
  - Uninstaller helper (`restore_saves.ps1` and `AutoDeploy.sh`) prioritizes the pristine `Saves\_pc\<label>` snapshot when restoring saves, guaranteeing original savedata is never corrupted.
- **Unit testing**: `tests/test_lan_profiles.cpp` compiled into `bin/test_lan_profiles.exe` via `build.bat`, verifying profile scoping, `ForGame`/`FromOtherGames` filtering, and per-game `LastProfileId` isolation.

## 2026-11-04 (18) - Fix explorer.exe hang and fullscreen minimization on game startup
- **Explorer.exe freeze resolution**:
  - In `App::RunDialog` (`src/overlay/overlay_app.cpp`), replaced the thread-freezing `DisableProcessWindowsGhosting()` and `WaitForSingleObject(dialogDone_, INFINITE)` with a proper non-blocking message pump using `MsgWaitForMultipleObjectsEx` and a `PeekMessageW` / `TranslateMessage` / `DispatchMessageW` loop.
  - Previously, Windows Shell (`explorer.exe`) sent synchronous messages to the game window upon focus changes / minimization; because the game thread was blocked without message processing and process ghosting was permanently disabled, Windows Shell deadlocked and froze completely. With active message pumping, the window remains responsive to OS/DWM messages, preventing explorer.exe freezes.
- **Child window hosting for dialogs & minimization fix**:
  - In `src/overlay/overlay_host.cpp`, enabled child-window embedding for `Mode::Dialog` (`(m == Mode::Overlay || m == Mode::Dialog) && CanHostAsChild(gw)`). When the game window exists, the dialog is hosted directly as a child of the game window (`BecomeChild`), centered within the game's client area, rather than as an independent top-level application window.
  - Replaced `WS_EX_APPWINDOW` on dialogs with `WS_EX_TOOLWINDOW` so showing a dialog does not create a separate taskbar button or trigger an OS application switch that forces fullscreen Unity/Unreal games to minimize via `WM_ACTIVATE(WA_INACTIVE)`.
  - Added `FreeMouseForOverlay()` and cursor unclip during `Mode::Dialog` so dialog controls remain accessible even if the game captured or hid the hardware cursor.
- **Non-blocking first-run boot**:
  - Added `[Overlay] FirstRunDialog = false` (default false) to `ReFix.ini`, `AutoDeploy.sh`, and `bin/deploy_helper.ps1`.
  - In `src/overlay/refix_overlay.cpp` (`ResolveIdentity`), on the first run of a game without profiles (`mine.empty() && !otherInstance`), ReFix no longer halts `SteamAPI_Init` or disrupts fullscreen boot unless `FirstRunDialog = true` is explicitly configured. Instead, the game boots directly and smoothly with machine identity.
  - In `App::Frame` (`src/overlay/overlay_app.cpp`), once the game window reaches the foreground, a welcome toast is displayed ("ReFix LAN: Pulsa [Shift] [Tab] para abrir ReFix" / "Press [Shift] [Tab] to open ReFix"). The player can open the overlay at any time in-game to create or import their profile, with save data inheritance preserved.

## 2026-11-04 (19) - Steam Avatar Provider (resolves solid blue square avatars in-game)
- **Root cause of blue avatars**:
  - `ISteamFriends::GetSmallFriendAvatar`, `GetMediumFriendAvatar`, and `GetLargeFriendAvatar` in both `src/unreal_steam_emu.cpp` and `src/steam_proxy.cpp` returned a hardcoded handle `1`.
  - `ISteamUtils::GetImageSize` returned hardcoded `32x32`.
  - `ISteamUtils::GetImageRGBA` filled all pixels with hardcoded `RGBA(70, 130, 220, 255)` (`#4682DC` Steel Blue), rendering a solid blue square for all players in games that query Steam avatars (e.g., PEAK, Lethal Company, Phasmophobia).
- **Dynamic Avatar Provider & Image Cache**:
  - Implemented `ReFixOverlay::GetAvatarHandle(steamId, size)`, `GetImageSize`, `GetImageRGBA`, and `InvalidateAvatar` in `src/overlay/refix_overlay.cpp`.
  - **Custom avatars**: When a profile has a custom avatar (`avatar.png`), or when `steam_settings\avatar.png` exists, loads and scales the image via WIC (`image::LoadFile`), outputting genuine high-quality 32-bit RGBA pixels matching the requested size (Small 32x32, Medium 64x64, Large 184x184).
  - **Stock avatars**: Algorithmically renders smooth anti-aliased avatars featuring the ReFix theme color gradient (from `kStock` palette) with a soft translucent white player silhouette. Each player gets a distinct theme color based on their profile's stock avatar index or deterministic SteamID.
  - **Remote LAN peers**: Decodes remote JPEG avatars received over LAN presence (`net::FetchAvatar`) and scales them via WIC `image::DecodeMemory(data, len, out, size)`.
  - **Cache invalidation**: Modifying or importing an avatar in the overlay calls `ReFixOverlay::InvalidateAvatar(steamId)` and broadcasts `AvatarImageLoaded_t` (callback 334) so the in-game UI updates in real time.

## 2026-11-04 (20) - Exact stock avatar glyph rasterization (matches ReFix overlay badges)
- **Problem**: Stock avatars previously rendered a generic head/shoulder silhouette with gradient background. In games like PEAK, players with stock avatars (e.g., 'Pisita' with stock index 9 / pizza icon) did not display the actual avatar badge chosen in ReFix.
- **Glyph rasterization via Win32 GDI DIB sections**:
  - `GenerateStockAvatar` in `src/overlay/refix_overlay.cpp` now renders the exact glyph for each stock avatar index (0..11) from Windows icon fonts (`Segoe MDL2 Assets` / `Segoe Fluent Icons`) using a 32-bit memory DIB section:
    - 0: Game controller (`0xE7FC`)
    - 1: Robot (`0xE99A`)
    - 2: Bolt (`0xE945`)
    - 3: Sun (`0xE706`)
    - 4: Moon (`0xE708`)
    - 5: World (`0xE909`)
    - 6: Puzzle (`0xEA86`)
    - 7: Smile (`0xE76E`)
    - 8: Music note (`0xE8D6`)
    - 9: Pizza slice (`0xED56`)
    - 10: Heart (`0xEB51`)
    - 11: Thumbs up (`0xE8E1`)
  - Automatically measures the glyph's alpha mask bounding box to mathematically center the icon inside the circular badge (compensating for font baseline offsets).
  - Renders the circular badge with anti-aliasing, linear color gradient (`kFrom[stockIdx]` to `kTo[stockIdx]`), blended with 92% opacity white glyph.
  - Adds the subtle accent glow ring and frames the badge on the dark ReFix panel background (`RGB(26, 31, 40)`), matching the exact look of avatars in the ReFix overlay.
- **Verification**: Built and verified across x64 and x86 targets, updated `release\ReFix_v2.1.0`, and deployed directly to `D:\Games\PEAK`.
