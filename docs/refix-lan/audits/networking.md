# ReFix-LAN Networking Audit Report

## 1. Overview
The networking architecture of the Re:Goldberg Unreal Steam Emulation Layer (ReFix-LAN) relies exclusively on raw UDP sockets to emulate Steam's proprietary networking infrastructure. It intercepts Steam API calls and standard Windows Sockets (Winsock) traffic, routing everything over the local area network (LAN) without requiring an internet connection.

## 2. ISteamNetworking
Implemented via `CSteamNetworkingEmu` (`src/unreal_steam_emu.cpp`).
*   **P2P Traffic (`SendP2PPacket` / `ReadP2PPacket` / `IsP2PPacketAvailable`)**: Fully implemented. When a game calls `SendP2PPacket`, the payload is prepended with a 4-byte channel identifier and sent via UDP. The underlying transport uses `BroadcastNetPacket(5, ...)`, which wraps the payload in a `NetPacketHeader` (Magic: `0x52464958`, Message Type: 5) and broadcasts it to `255.255.255.255` (and any custom IPs) on the configured `g_listenPort` (default 47584). 
*   **Socket Functions (`CreateListenSocket`, `CreateP2PConnectionSocket`, etc.)**: These are stubbed. They return a dummy handle (`1`) and simulate success, but methods like `IsDataAvailableOnSocket` and `RetrieveDataFromSocket` return false.
*   **State / Metadata**: `GetP2PSessionState` returns mocked success (active, not using relay, remote IP `127.0.0.1:7777`).

## 3. ISteamNetworkingSockets (SteamSockets NetDriver)
Implemented via `CSteamNetworkingSocketsEmu` (`src/unreal_steam_emu.cpp`).
*   **Status**: This interface is heavily stubbed to simulate connection success without actually routing data.
*   **Implementation Details**:
    *   Connection methods (`ConnectByIPAddress`, `ConnectP2P`, `CreateListenSocketIP`) immediately return a fake handle (`1`).
    *   `SendMessageToConnection` immediately returns `k_EResultOK` but **does not transmit the data**.
    *   `ReceiveMessagesOnConnection` immediately returns `0` (no messages).
    *   `GetConnectionInfo` mocks a `k_ESteamNetworkingConnectionState_Connected` state.
*   **Impact**: Games that rely exclusively on `ISteamNetworkingSockets` for their core data transport will likely fail to synchronize unless they have a fallback to `ISteamNetworkingMessages` or raw sockets. The current implementation bypasses engine initialization checks but drops the payloads.

## 4. ISteamNetworkingMessages
Implemented via `CSteamNetworkingMessagesEmu` (`src/unreal_steam_emu.cpp`).
*   **Status**: Fully functional.
*   **Implementation Details**:
    *   `SendMessageToUser` formats messages as Type 5 (P2P) with a channel header: `[int32 channel][payload]`.
    *   **Unicast Optimization**: It first attempts to resolve the `targetSteamID` against the `g_peers` map (populated by lobby discovery). If the peer's IP and port are known, it sends the packet directly via unicast UDP.
    *   **Broadcast Fallback**: If the peer's IP is unknown, it falls back to broadcasting the packet to the LAN (`BroadcastNetPacket`).
    *   `ReceiveMessagesOnChannel` correctly reads from the `g_p2pIncoming` queue and allocates `SteamNetworkingMessage_t` structures.

## 5. Steam Datagram Relay (SDR) & ISteamNetworkingUtils
Intercepted in `src/steam_proxy.cpp` and emulated in `src/unreal_steam_emu.cpp`.
*   **The "Checking Steam Relay..." Fix**: A common issue in cracked games using SDR is a hang during initialization while the game waits for Valve's relay network. ReFix intercepts the flat export and VTable for `SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus`.
*   **Behavior**: It simulates a brief connection delay. For the first 500ms, it returns `k_ESteamNetworkingAvailability_Retrying` (3). Afterward, it returns `k_ESteamNetworkingAvailability_Current` (100). This safely bypasses the SDR wait sequence.
*   **Synthetic Callbacks**: `InitRelayNetworkAccess` is hooked to dispatch a synthetic relay callback to satisfy game logic.
*   **Ping / Topology**: `GetPingToDataCenter` and `EstimatePingTime...` unconditionally return `5ms`.

## 6. Transparent P2P Winsock Hook (`src/steam_p2p_hook.cpp`)
Designed for games that rely on raw Windows Sockets (UDP) for traffic but use Steam solely for matchmaking/lobbies.
*   **Intercepted API**: Hooks `ws2_32.dll` functions: `sendto`, `recvfrom`, `connect`, `select`, `bind`, `closesocket`.
*   **Routing**: When `sendto` targets an IP associated with a known `SteamID` (resolved from lobby metadata), the payload is intercepted, wrapped in a `ReFixP2PHeader`, and routed through `ISteamNetworking::SendP2PPacket`.
*   **Red Team Hardening**:
    *   **V-01 Multi-Socket Contexts**: Maintains isolated queues and contexts (`g_socketContexts`) distinguishing between Game ports (e.g., 7777) and Voice ports (e.g., 5058).
    *   **V-02 Pre-Lobby Hold Buffer**: Buffers outgoing packets for up to 3000ms if the target `SteamID` is not yet known. This solves race conditions in networking engines (like Photon Fusion) that attempt UDP handshakes before the Steam Lobby metadata has fully propagated over LAN. If the TTL expires, the packet falls back to raw Winsock.
    *   **V-04 Port Virtualization**: If `bind()` fails with `WSAEADDRINUSE` (10048), the hook auto-increments the port (up to +30) and virtualizes the binding, preventing server startup crashes when multiple instances run on the same machine.

## 7. Transport Layer & Connectivity
*   **Internet Dependency**: **None**. All Steam networking API calls are intercepted locally and translated into standard UDP packets on the LAN.
*   **UPnP & Firewall (`src/upnp_firewall.cpp`)**: The DLL automatically creates Windows Firewall rules and maps UPnP ports on the router for common game ports (`7777`, `7778`, `7779`) and the query port (`27015`). This ensures LAN peer discovery works seamlessly without user intervention.
