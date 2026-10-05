# Photon Auditing Report: ReGoldberg / ReFix LAN

## 1. Overview
The implementation of Photon compatibility within ReGoldberg relies on two complementary components working in tandem across the application and transport layers:
1. **UniversalPhotonFix.dll**: A C# BepInEx Plugin that patches Unity games to override Photon cloud dependencies, AppIDs, and settings at runtime.
2. **steam_p2p_hook.cpp**: A C++ Winsock redirect layer that intercepts low-level UDP packets and transparently routes Photon Realtime, Fusion, and Voice traffic over Steam's P2P network (`ISteamNetworking`).

## 2. UniversalPhotonFix (Application Layer)
This component ensures the game client does not reach out to official Photon Cloud services using official AppIDs, bypassing cloud authorization checks and facilitating local emulator routing.

### Hooking Mechanism
- **Framework**: BepInEx + Harmony (`HarmonyPatch`).
- **Target Methods**: Intercepts `Photon.Pun.PhotonNetwork.ConnectUsingSettings` (PUN) and `Photon.Realtime.AppSettings`.
- **Implementation**: Uses a Harmony Prefix to hot-swap settings dynamically before the connection is initialized by the engine.

### Configuration Injection
The plugin reads a local configuration file (using `File.ReadAllLines` combined with `Application.dataPath`, likely `ReFix.ini` or similar) to override the following cloud dependencies:
- `AppIdRealtime`
- `AppIdVoice`
- `AppIdChat`
- `AppIdFusion`
- `FixedRegion`

By doing this, the plugin achieves local compatibility and severs official cloud tracking and external matchmaking dependencies.

## 3. P2P Socket Routing (Transport Layer)
The core mechanism for achieving transparent offline LAN play (or routing over Steam Spacewar) is located in `D:\ReGoldberg\src\steam_p2p_hook.cpp`. It hooks `ws2_32.dll` Winsock functions (`sendto`, `recvfrom`, `connect`, `select`, `bind`, `closesocket`) with MinHook.

### Protocol Analysis
- Incoming and outgoing UDP packets are forwarded through `ISteamNetworking_SendP2PPacket` and `ISteamNetworking_ReadP2PPacket` instead of traversing physical network interfaces.
- Packets are encapsulated with a custom `ReFixP2PHeader` framing structure to manage metadata:
  - **Magic Byte**: `0x58464552` ('REFX')
  - **Ports**: `srcPort`, `dstPort`
  - **Service type**: 1 = Game, 2 = Voice
  - **Payload length**

### Port Classification (V-01 Hardening)
The C++ hook specifically classifies UDP traffic into independent service queues to prevent Game and Voice packets from stalling one another or being misrouted.
- **Game Ports** (Photon Realtime, Fusion, PUN): UDP 5055, 5056, 7777, 27015. Sent using `k_EP2PSendUnreliable` on Steam channel `0`.
- **Voice Port** (Photon Voice): UDP 5058. Sent using `k_EP2PSendUnreliableNoDelay` on Steam channel `1`.

### Pre-Lobby Hold Buffer (V-02 Hardening)
Photon Fusion and `nanosockets` frequently broadcast an initial burst of UDP handshakes before Steam lobbies fully resolve the IP-to-SteamID mappings. To prevent the loss of these critical early packets, `steam_p2p_hook.cpp` implements a 3000 ms TTL Hold Buffer (`g_holdBuffer`).
- Packets aimed at known Game/Voice ports are held in a `std::vector<BufferedHoldPacket>` while the Steam lobby handshake completes.
- A background thread (`P2PPumpThread`) continually polls this buffer and flushes the held packets via Steam P2P once the remote peer's SteamID becomes known.
- If the 3000 ms TTL expires, packets seamlessly fall back to raw Winsock `sendto`, ensuring pure LAN/offline fallback operates without infinite blocking.

## 4. Matchmaking, Chat, and Cloud Connectivity Bypass
- **Photon Chat**: Photon Chat typically operates over TCP or WebSockets. `UniversalPhotonFix` intercepts the `AppIdChat` setting natively. At the transport layer, `steam_p2p_hook.cpp` only monitors TCP via `Hook_connect` for IP-to-SteamID registration (for logging purposes) but passes the actual connection to the native Winsock API `g_orig_connect`. This means local LAN emulation smoothly inherits standard TCP loopback configurations.
- **Matchmaking**: Official Photon Matchmaking servers are completely bypassed. The emulator manages lobbies by utilizing Steam's P2P Matchmaking APIs (`ISteamMatchmaking`), automatically publishing games to standard Steam P2P sessions over SDR (Steam Datagram Relay) using fake IPs (`0x7F000001` with a synthesized suffix).

## 5. Conclusions
- The codebase securely and efficiently partitions networking traffic using isolated service queues.
- Packet dropping during initial P2P handshakes is effectively mitigated via the Pre-Lobby Hold Buffer.
- Cloud dependencies are fully neutralized via the `UniversalPhotonFix` plugin, successfully sandboxing the application for seamless local area networking or localized Steam emulation.
- **Compatibility**: The implementation successfully covers Photon PUN, Realtime, Voice, Fusion, and Chat.
