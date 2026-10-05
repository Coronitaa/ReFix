# EOS Integration & Audit Report (ReGoldberg / ReFix)

## 1. Overview
The ReFix project provides a robust emulation and proxy layer for Epic Online Services (EOS) and Steamworks, effectively decoupling titles from Epic's backend servers. The repository contains two distinct implementations of the EOS proxy:
- **Legacy EOS Emulator** (`src\eos_proxy.cpp`): A monolithic implementation providing fundamental stubs for EOS initialization, Connect, and Lobby APIs.
- **ReFix EOS v3 Emulator** (`src\eossdk\`): A modern, modular SDK replacement that maps EOS functionality directly onto Steam's networking and matchmaking infrastructure.

## 2. EOS Proxy and Steam Bridges
The core philosophy of the EOS v3 emulator is to bridge EOS interfaces to Steamworks APIs (`ISteamMatchmaking`, `ISteamNetworkingMessages`, `ISteamFriends`), allowing EOS games to utilize Steam (or Steam emulators) for networking.
- **Steam Backend Integration** (`src\eossdk\net\steam_backend.cpp`): Dynamically resolves exports from `steam_api64.dll`. Instead of running an independent lobby server, it seamlessly publishes EOS metadata (such as AppID and PUIDs) as Steam Lobby Data.
- **P2P Hooks**: By integrating with `ISteamNetworkingMessages` and `ISteamNetworkingSockets`, the proxy intercepts EOS P2P requests and routes them over Steam's P2P network, ensuring compatibility with other emulators.

## 3. PUID (Product User ID) and Authentication
Identity management is fully localized, bypassing Epic Account Services completely.
- **ID Derivation** (`src\eossdk\core\eos_identity.cpp`): Product User IDs (PUID) and Epic Account IDs (EAID) are computed locally. The system attempts to extract a `SteamId` from a provided Steam session ticket. If no ticket is provided, it falls back to a configured `SteamId` or generates a hardware fingerprint (ComputerName + Volume Serial Number).
- **Mapping & Spoofing**: Connect APIs (`src\eossdk\api\eos_api_connect.cpp`) spoof successful authentications and return static, locally-derived JWT tokens. 
- **External Accounts**: EOS External Account mappings (Steam <-> PUID) are emulated dynamically, translating Steam Persona Names to EOS Display Names transparently.

## 4. Sessions and Lobbies
- **Lobbies** (`src\eossdk\api\eos_api_lobby.cpp`): EOS Lobbies are emulated by wrapping Steam Matchmaking lobbies. Lobby attributes and membership statuses are mapped 1:1. Because Steam limits lobby attribute sizes (8 KB per key), the emulator base64-encodes the `LobbyRecord` state and chunks it across multiple keys (e.g., `refix_eos_0`, `refix_eos_1`).
- **Sessions** (`src\eossdk\api\eos_api_sessions.cpp`): Operations on EOS Sessions are redirected to the underlying Lobby Directory, as many titles use the two interchangeably.
- **Lobby Directory** (`src\eossdk\net\lobby_directory.cpp`): Maintains an in-memory representation of local and discovered lobbies, synchronizing state with the `SteamBackend` on the tick thread.

## 5. Public IP and Network Layer
- **UPnP and Windows Firewall** (`src\upnp_firewall.cpp`): Uses COM interfaces (`INetFwPolicy2` and `IUPnPNAT`) to automatically open necessary UDP ports (7777-7779 for game traffic, 27015 for Steam Query) and map UPnP ports on the router.
- **Local IP Detection**: Uses a dummy UDP socket connected to `1.1.1.1` to trigger Windows routing logic. `getsockname()` is then called to resolve the correct outbound local interface IP, preventing the system from improperly binding to `127.0.0.1`.

## 6. Cloud Dependencies (Cloud vs. Local)
- **Local Execution**: The EOS emulation is strictly local. **Zero requests** are made to Epic Games' backend infrastructure (`epicgames.com`, `eos.epicgames.com`).
- **Cloud Requests**:
  - `http://api.ipify.org`: The only direct outbound HTTP call present in the codebase. It is invoked via `WinINet` in `src\upnp_firewall.cpp` (`GetPublicIP()`) to determine the host's public IP address. This is utilized by both the legacy EOS emulator and the Steam API proxy.
  - **Steam Infrastructure**: Because the v3 emulator bridges EOS over Steamworks, it implicitly depends on Steam's backend for matchmaking and relay servers. However, if paired with a LAN Steam emulator (e.g., Goldberg), the entire stack can operate 100% offline.
