# ReFix v2.6.2

## Summary of Changes Since v2.6.1

ReFix v2.6.2 improves multiplayer lobby discovery and joining across Photon and Steamworks games, fixes multi-instance name conflicts, adds live display and resolution management across games, and verifies Authenticode signatures across all binaries.

---

### Multiplayer, Lobbies & Multi-Instance
- **Generic Steamworks Lobby & P2P Fixes:** Fixed friend and public lobby discovery (`GetFriendGamePlayed`, `RequestLobbyData`), separate `LobbyDataUpdate_t` deduplication across standard and `ManualDispatch` callback queues, deferred `JoinLobby` call results, `SteamNetworkingMessagesSessionRequest_t` delivery, and expanded lobby metadata string buffers.
- **Photon Realtime & Friend Rooms (#40):** Fixed Photon Realtime room setup event routing (`Actor 0` routing and Fusion vs. Realtime/PUN 2 discrimination), added ENet keepalive pings, and surfaced friends' Photon rooms in public lobby lists via Steam Rich Presence.
- **Multi-Instance Profile Isolation:** Updates game player XML configs (`config_player.xml`) with each instance's ReFix profile or guest name so multiple instances on one PC no longer collide with duplicate host names.
- **SteamInput & Proxy Compatibility:** Added `ISteamInput` interface support, missing legacy `ISteamFriends` method stubs, and updated `winhttp.dll` proxy forwarding.

### Overlay & Display Settings
- **Live Display & Resolution Settings:** Added OS-supported resolution detection (Windows and Wine/Proton) for Windowed, Fullscreen, and Fullscreen Windowed modes with a 15-second Keep / Auto-Revert confirmation modal.
- **Cross-Game Persistence:** Automatically applies your saved display mode and resolution whenever you launch any game, including re-checking right after game startup.
- **Generic Overlay Invite Detection:** Automatically detects when a game does not implement Steam overlay invite/join callbacks and prompts you to join from the in-game lobby list.

### Security & Binaries
- **Authenticode Signing & Metadata:** Updated `winhttp.dll` to v2.6.2.0, fixed signature status verification, and signed all 24 distribution binaries across `lan`, `lan_offline`, `online`, and `x86`/`x64`.
