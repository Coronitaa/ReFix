# Steam Callback & SDR Boundary Audit Report (ReFix / ReGoldberg)

## 1. Overview
This report maps the sizes, sources, and runtime behaviors of critical Steam callbacks and Steam Datagram Relay (SDR) functions across the boundary between Online (Native Steamworks) and LAN (ReFix / Goldberg Emulator). Understanding these differences is crucial for ensuring game engines (like Unreal and Godot) do not hang while waiting for cloud-only infrastructure (like SDR).

## 2. Callback Mappings

| Callback Name | ID | Size (bytes) | Struct Layout Summary |
| ------------- | -- | ------------ | --------------------- |
| `LobbyMatchList_t` | 510 | 4 | `uint32 m_nLobbiesMatching;` |
| `LobbyEnter_t` | 504 | 17-24 (padded) | `uint64 m_ulSteamIDLobby; uint32 m_rgfChatPermissions; bool m_bLocked; uint32 m_EChatRoomEnterResponse;` |
| `GameLobbyJoinRequested_t` | 333 | 16 | `CSteamID m_steamIDLobby; CSteamID m_steamIDFriend;` |
| `GameRichPresenceJoinRequested_t` | 337 | 264 | `CSteamID m_steamIDFriend; char m_rgchConnect[256];` |
| `SteamRelayNetworkStatusChanged_t` | 1281 | 272 | `int m_eAvail; int m_bPingMeasurementInProgress; int m_eAvailNetworkConfig; int m_eAvailAnyRelay; char m_debugMsg[256];` |
| `SteamAPICallCompleted_t` | 703 | 16 | `SteamAPICall_t m_hAsyncCall; int m_iCallback; uint32 m_cubParam;` |

## 3. Behavior Differences: Online vs. LAN

### `LobbyMatchList_t` & `LobbyEnter_t`
- **Online (Native Steam)**: Triggered asynchronously by Steam's backend infrastructure after a matchmaking search (`RequestLobbyList`) or join (`JoinLobby`) completes. Subject to WAN latency.
- **LAN (ReFix/Emu)**: Triggered via local memory injection when ReFix completes a UDP broadcast search or resolves an internal lobby dictionary (`LobbyDirectory`). `steam_proxy.cpp` synthesizes `LobbyEnter_t` explicitly for certain engines (like Godot) where the native emulator might miss edge cases.

### `GameLobbyJoinRequested_t` & `GameRichPresenceJoinRequested_t`
- **Online**: Fired when a user accepts an invite from the Steam overlay or uses "Join Game" via the friends list.
- **LAN**: Since the native Steam Overlay is inaccessible, ReFix synthesizes these callbacks by parsing command-line parameters (e.g., `+connect_lobby`) or through its custom LAN GUI/server browser, manually dispatching the callbacks to tell the game to connect.

### `SteamAPICallCompleted_t`
- **Online**: Steam manages an internal table of async handles and dispatches this when tasks complete.
- **LAN**: ReFix emulates async returns (like Authentication ticket generation or Lobby creation). It manually allocates fake handles and injects `SteamAPICallCompleted_t` directly into the manual dispatch queue or callback interceptors (as seen in `ManualDispatch_GetNextCallback`).

### `SteamRelayNetworkStatusChanged_t`
- **Online**: Fired asynchronously when the Steam client successfully allocates routing through Valve's SDR datacenters. Games often block multiplayer menus until this fires with `k_ESteamNetworkingAvailability_Current` (value 100).
- **LAN**: Synthesized immediately by ReFix. See SDR short-circuit logic below.

## 4. SDR / Relay Hooks Short-Circuiting Analysis

SDR (Steam Datagram Relay) functions (`SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus`, `SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess`) are responsible for masking IP addresses and routing traffic through Valve's datacenters. These MUST be short-circuited in a LAN environment to prevent infinite loading screens.

**Findings: SDR hooks are CORRECTLY short-circuited.**

1. **`InitRelayNetworkAccess` Intercept**:
   In `steam_proxy.cpp`, the flat API `SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess` is hooked. When the game calls this, ReFix immediately flags `g_syntheticRelayPending = true` and calls `DispatchRelayCallbacks()`. This manually constructs a `SteamRelayNetworkStatusChanged_t` callback with `m_eAvail = k_eRelayAvail_Current` (100) and injects it into the game's callback queue on the next `RunCallbacks` tick.

2. **`GetRelayNetworkStatus` Intercept**:
   Similarly, `SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus` is intercepted. It immediately populates the struct with `k_eRelayAvail_Current` (simulated ready) and returns the same value, bypassing any actual WAN ping measurement.

3. **Unreal Engine / Goldberg Integration (`unreal_steam_emu.cpp`)**:
   Within the `CSteamNetworkingUtilsEmu` class wrapper, `GetRelayNetworkStatus()` is hardcoded to return `k_ESteamNetworkingAvailability_Current`.

**Conclusion**: The implementation successfully fools titles heavily dependent on SteamSockets/SDR into believing they are connected to Valve's backbone instantly, permitting seamless offline/LAN multiplayer bridging over local sockets.
