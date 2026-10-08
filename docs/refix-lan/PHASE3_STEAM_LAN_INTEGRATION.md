# ReFix Phase 3 — Steamworks LAN Core Integration & Transport Hardening

**Document Version:** 1.0.0  
**Target Architecture:** ReFix Autonomous LAN / Offline Stack  
**Target Branch:** `feature/phase3-sockets`  
**Baseline Commit:** `7ef361a`  
**Date:** October 2026  

---

## 1. Executive Summary & Architectural Objective

The goal of Phase 3 is to establish the first fully functional, autonomous architectural milestone of the ReFix LAN/Offline stack:

```
+-----------------------------------------------------------------------------------+
|                                    Game Engine                                    |
|                         (Unreal Engine, Unity, Custom Engine)                     |
+-----------------------------------------------------------------------------------+
                                          |
                                          v  [Steamworks API Calls]
+-----------------------------------------------------------------------------------+
|                     Steamworks Compatibility Layer                                |
|          (src/unreal_steam_emu.cpp, src/steam_proxy.cpp, steam_api64.dll)          |
+-----------------------------------------------------------------------------------+
                                          |
                                          v  [ILanCore Interface / C++ Direct]
+-----------------------------------------------------------------------------------+
|                                  ReFix LAN Core                                   |
|                             (src/lan_core/refix_lan_core)                        |
|                                                                                   |
|  • LocalIdentityService: MachineId, PeerId (128-bit UUID), DisplayName, Scope     |
|  • PeerRegistry: Multi-index mapping (PeerId <-> ExtId <-> Endpoint <-> State)    |
|  • DiscoveryService: Multicast/Broadcast beacons, queries, responses, TTL         |
|  • LobbyService: Host-authoritative replication, revisioning, metadata sync       |
|  • MatchmakingService: Filter-based lobby queries, LAN discovery                  |
|  • CallbackDispatcher: Thread-safe event queue pumped on game main thread         |
|  • LocalRelayService: Direct send abstraction (relay fallback hook)               |
+-----------------------------------------------------------------------------------+
                                          |
                                          v  [ILanTransport Interface]
+-----------------------------------------------------------------------------------+
|                          ReFix Universal LAN Transport                            |
|                          (src/lan_core/refix_lan_transport)                       |
|                                                                                   |
|  • Dual-Socket Reactor: Port 47584 (Discovery) + Ephemeral Port 0 (Unicast Data)  |
|  • Canonical Wire Framing: Magic 0x52464958 ('RFIX'), Version 1, 36-byte header  |
|  • ARQ Reliability Engine: Sequence numbering, Cumulative + SACK bitmask         |
|  • Inbound/Outbound Queues: Reordering map, retry exhaustion cleanup              |
|  • Internet-Zero Firewall: Centralized RFC1918 / APIPA / Loopback drop filter     |
+-----------------------------------------------------------------------------------+
                                          |
                                          v  [OS Winsock / Physical Network]
+-----------------------------------------------------------------------------------+
|                        Localhost / Physical LAN Subnet                            |
+-----------------------------------------------------------------------------------+
```

### Autonomy Guarantee (Zero External Dependencies)
In LAN/Offline mode, this stack functions with:
- **Zero Steam Client Requirement:** Neither Steam running nor Steam installed on the machine.
- **Zero Internet / Cloud Backends:** No connection to Valve servers, Valve SDR relays, EOS cloud, Photon cloud, or Unity Relay.
- **Zero Goldberg Dependency:** Goldberg emulator binaries are completely detached from the LAN execution path.
- **Zero Automatic Fallback:** Never falls back to cloud or remote relays if LAN peer discovery fails.

---

## 2. Steamworks-to-LanCore Mapping Matrix

The Steamworks compatibility layer (`src/unreal_steam_emu.cpp` and `src/steam_proxy.cpp`) translates legacy Steamworks calls directly into ReFix LAN Core operations:

### 2.1 Initialization & Main-Thread Event Pump
| Steamworks API | ReFix LAN Core Equivalent | Implementation Notes |
| :--- | :--- | :--- |
| `SteamAPI_Init()`, `SteamAPI_InitSafe()` | `ILanCore::Get().Initialize(config)` | Configures default discovery port 47584, data port 0, binds local `SteamID64` to `PeerId` in `PeerRegistry`, registers local display name. |
| `SteamAPI_RunCallbacks()` | `ILanCore::Get().Tick()` | Pumping `Tick()` processes transport retransmissions, checks discovery heartbeats, and flushes thread-safe `CallbackDispatcher` events to the game main thread. |
| `SteamAPI_Shutdown()` | `ILanCore::Get().Shutdown()` | Gracefully announces node departure, halts background reactor thread, closes Winsock sockets, clears registries. |
| `ReFix_GetLanCore()` | `return &ILanCore::Get();` | C-export in `steam_proxy.cpp` allowing game mods, diagnostics, and external adapters to access `ILanCore`. |

### 2.2 Matchmaking & Lobby Management
Steamworks uses numeric 64-bit Lobby IDs (`CSteamID`), whereas `ILanCore` uses alphanumeric string identifiers (e.g., `LOBBY_2e8d59b3_1`). A thread-safe bidirectional bijection (`EnsureSteamLobbyID` and `GetCoreLobbyId`) bridges the two spaces deterministically.

| Steamworks API | ReFix LAN Core Method | Wire Packet / Flow |
| :--- | :--- | :--- |
| `CreateLobby(eType, nMaxMembers)` | `Lobby().CreateLobby(cfg)` | Creates lobby locally, assigns ownership to local `PeerId`, marks revision 1, and broadcasts `MsgType::LobbyAnnouncement` (`0x0D`) across LAN. Dispatches `LobbyCreated_t` and `LobbyEnter_t`. |
| `JoinLobby(steamIDLobby)` | `Lobby().JoinLobby(lobbyId)` | Sends join intent or imports lobby state, marks membership, and dispatches `LobbyEnter_t`. |
| `LeaveLobby(steamIDLobby)` | `Lobby().LeaveLobby(lobbyId)` | Removes member, broadcasts updated announcement or tombstone, cleans local state. |
| `SetLobbyData(steamIDLobby, key, value)` | `Lobby().SetLobbyData(lobbyId, key, value)` | Updates lobby attribute map, increments revision number, and broadcasts updated `LobbyAnnouncement` (`0x0D`) to LAN. Dispatches `LobbyDataUpdate_t`. |
| `GetLobbyData(steamIDLobby, key)` | `Lobby().GetLobbyData(lobbyId, key)` | Reads directly from local replicated lobby attribute map. Synchronized across peers. |
| `RequestLobbyList()` | `Matchmaking().RefreshLobbyList()` | Broadcasts `MsgType::LobbyQuery` (`0x0E`) over discovery port (47584). Remote hosts respond with their hosted `LobbyAnnouncement` packets. Dispatches `LobbyMatchList_t`. |
| `GetNumLobbyMembers(steamIDLobby)` | `Lobby().GetLobby(lobbyId)->members.size()` | Returns member count from local replicated state. |
| `GetLobbyMemberByIndex(steamIDLobby, i)` | `Lobby().GetLobby(lobbyId)->members[i]` | Returns member `CSteamID` via `PeerRegistry::FindExternalId(peerId, ExternalIdType::SteamId)`. |

### 2.3 P2P Networking & Transport Routing
| Steamworks API | ReFix LAN Core Equivalent | Wire Transport Path |
| :--- | :--- | :--- |
| `SendP2PPacket(steamIDRemote, data, size, k_EP2PSendReliable, channel)` | `Peers().FindBySteamId(steamID)` -> `Transport().SendReliable(endpoint, data, size, channel)` | Encapsulated in canonical 36-byte `'RFIX'` header with `MsgType::DataReliable` (`0x04`). ARQ engine guarantees in-order delivery and retransmission. |
| `SendP2PPacket(steamIDRemote, data, size, k_EP2PSendUnreliable, channel)` | `Peers().FindBySteamId(steamID)` -> `Transport().SendUnreliable(endpoint, data, size, channel)` | Encapsulated in `'RFIX'` header with `MsgType::DataUnreliable` (`0x05`). Shipped directly over ephemeral unicast socket without ARQ retransmission overhead. |
| `IsP2PPacketAvailable(size, channel)` | Checked against `g_p2pIncoming[channel]` | Filled by `DataPacketReceived` event from `DiscoveryServiceImpl::OnInboundData()` which preserves the wire `channel` byte. |
| `ReadP2PPacket(dest, size, msgSize, remoteSteamID, channel)` | Popped from `g_p2pIncoming[channel]` | Delivers packet payload and sender's `CSteamID` to the game engine. |
| `AcceptP2PSessionWithUser(steamIDRemote)` | Auto-accept or mapped to peer | Grants P2P session rights and records peer endpoint in active routing table. |
| `CloseP2PSessionWithUser(steamIDRemote)` | Peer session cleanup | Releases connection context in `PeerRegistry` and transport tracking. |

---

## 3. Wire Protocol & State Machine Lifecycle

### 3.1 Packet Framing (`src/lan_core/refix_lan_wire.h`)
All packets transmitted over ReFix Universal LAN Transport use a fixed 36-byte little-endian header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Magic: 0x52464958 ('RFIX')              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Version (1) |  MsgType (1B) |          Flags (2B)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                   Sender UUID (Bytes 0-7)                     +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                   Sender UUID (Bytes 8-15)                    +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Session ID (4B)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Channel (1B) |                   Reserved (3B)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number (4B)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Acknowledgment Number (4B)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length (4B)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Data (Variable)                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 3.2 Protocol Lifecycle
```
[Node A: Host]                                              [Node B: Client]
       |                                                           |
       |  1. SteamAPI_Init()                                       |  1. SteamAPI_Init()
       |     - Bind UDP 47584 (Discovery)                          |     - Bind UDP 47584 (Discovery)
       |     - Bind UDP Ephemeral (Data)                           |     - Bind UDP Ephemeral (Data)
       |                                                           |
       |  2. CreateLobby()                                         |  2. RequestLobbyList()
       |     - Local Lobby Created                                 |     - Broadcast LobbyQuery (0x0E)
       |     - Broadcast LobbyAnnouncement (0x0D) ---------------->|       on port 47584
       |<----------------------------------------------------------+
       |                                                           |
       |  3. Respond to Query                                      |  3. Process Inbound Announcement
       |     - Send LobbyAnnouncement (0x0D) --------------------->|     - Import lobby to Matchmaking
       |                                                           |     - Fire LobbyMatchList_t
       |                                                           |
       |                                                           |  4. JoinLobby()
       |  4. Peer Discovered / Handshake                           |     - Register Host in PeerRegistry
       |<----------------- Handshake (0x01) -----------------------|     - Send Handshake
       |------------------ HandshakeAck (0x02) ------------------->|
       |                                                           |
       |  5. In-Game P2P Data Exchange                             |  5. In-Game P2P Data Exchange
       |     (Channel 0/1 via Ephemeral Unicast)                   |     (Channel 0/1 via Ephemeral Unicast)
       |<================= DataReliable (0x04, Seq=1) =============|
       |================== Ack (0x06, Ack=1) =====================>|
       |                                                           |
       |  6. Periodic Heartbeat (every 2s)                         |  6. Periodic Heartbeat (every 2s)
       |<----------------- DiscoveryBeacon (0x0B) -----------------|
       |------------------ DiscoveryBeacon (0x0B) ---------------->|
       |                                                           |
       |  7. SteamAPI_Shutdown()                                   |  7. SteamAPI_Shutdown()
       |     - Broadcast NodeLeaving                               |     - Prune disconnected peer
       |     - Close Sockets                                       |     - Close Sockets
```

---

## 4. Internet-Zero Policy & Security Hardening

To maintain strict compliance with the **Internet-Zero** mandate, no game traffic, discovery packets, or emulator calls may reach the public Internet or cloud infrastructure.

### 4.1 3-Tier Centralized Firewall Enforcement
1. **Tier 1: Destination Classification (`refix_lan_firewall.cpp`)**
   - **Permitted LAN Subnets:** Loopback (`127.0.0.0/8`), Private LAN (RFC 1918: `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`), APIPA (`169.254.0.0/16`), Administratively Scoped Multicast (`239.0.0.0/8`), Link-Local Multicast (`224.0.0.0/24`), Limited Broadcast (`255.255.255.255`).
   - **Strictly Dropped / Blocked:** All Public WAN IPs (e.g. `8.8.8.8`, `1.1.1.1`), Valve SDR Relay subnets (`162.254.192.0/18`), Globally Routable Multicast (`224.0.1.0+`, `225.0.0.0/8`), and all IPv6 traffic (`AF_INET6`).
2. **Tier 2: Dual-Socket Egress Enforcement**
   - Every datagram sent through `LanTransport::SendUnicastPacket()` or broadcast routines must pass through `LanFirewall::IsAllowed(endpoint)`. Non-compliant destinations return `LanError::BlockedByFirewall` and drop immediately before reaching `sendto`.
3. **Tier 3: Winsock Hooking Interception**
   - `Hook_sendto`, `Hook_recvfrom`, `Hook_send`, and `Hook_recv` intercept socket traffic.
   - Connected UDP sockets (`connect()` followed by `send()`) are intercepted and checked against the LAN firewall policy.
   - Pre-Lobby Hold Buffer drops non-LAN packets unconditionally to prevent buffered egress leakage.
   - In LAN mode, Valve DLL loading is bypassed completely (`EnsureOriginalDll()` returns `nullptr`), preventing original proxy code from calling Steam cloud servers.

---

## 5. Verification Matrix & Classification

Every test result is certified according to the strict classification schema: `PASS`, `PARTIAL`, `CONDITIONAL`, `UNVERIFIED`, `BLOCKED`.

| Test Area | Scope / Target | Result | Evidence / Details |
| :--- | :--- | :--- | :--- |
| **LAN Core Identity & UUID** | `test_refix_lan_core.exe` | **PASS** | Generates stable `MachineId`, process-scoped `PeerId` (128-bit), custom display name. |
| **Internet-Zero Egress Firewall** | `test_refix_lan_core.exe` & `refix_net_test.exe` Suite 10 | **PASS** | Blocks 162.254.192.0, 8.8.8.8, 1.1.1.1, 224.0.1.1, IPv6. Allows RFC 1918, 127.0.0.1, 239.255.71.84. |
| **Wire Protocol Framing** | `test_refix_lan_core.exe` | **PASS** | Round-trip serialization/deserialization of header and payloads with byte-level fidelity. |
| **Lobby & Matchmaking Lifecycle** | `test_refix_lan_core.exe` | **PASS** | Create, update attributes, query with filters, join, leave, replicate revisions. |
| **PeerRegistry Multi-Index Mapping**| `test_refix_lan_core.exe` | **PASS** | Indexing by `PeerId`, `Endpoint`, `SteamID`, and `EOS PUID`. Reconnect preserves bindings. |
| **CallbackDispatcher Re-entrancy**  | `test_refix_lan_core.exe` | **PASS** | Dispatches nested callbacks without deadlock via snapshot queue copying. |
| **PeerRegistry Reconnect Pruning**  | `test_refix_lan_core.exe` | **PASS** | Destructive pruning eliminated; endpoint updates safely transfer without dropping external IDs. |
| **Transport Loopback & ARQ**       | `test_refix_lan_core.exe` | **PASS** | Dual-socket ephemeral communication, cumulative ACK + SACK, zero packet drop in loopback. |
| **Steamworks -> LanCore Adapter**   | `test_steam_lancore_adapter.exe` | **PASS** | `SteamAPI_Init` initializes `ILanCore`, `CreateLobby` updates LanCore, `SendP2PPacket` routes over transport. |
| **Multi-Threaded Locking Stress**   | `refix_net_test.exe` Suite 9 | **PASS** | 10,000 concurrent API operations without deadlock. Data races marked UNVERIFIED (no TSan on MSVC). |
| **Fault Injection Tolerance (ARQ)**| `refix_net_test.exe` Suite 11 | **PASS** | Handled dropped packets, dropped ACKs, reordered delivery, duplicated payloads with zero loss. |
| **Steamworks Bootstrap & Routing**  | `refix_net_test.exe` Suite 13 | **PASS** | Complete C++ vtable & flat interface compatibility, zero Valve DLL loads in LAN mode. |
| **Multi-Process Localhost Execution**| `test_lan_multiprocess.ps1` | **PASS** | Two separate OS processes exchange packets on localhost without port contention. |
| **Path Robustness & Spaces**        | `test_path_robustness.ps1` | **PASS** | 8/8 path test cases passed across directory structures with spaces and special characters. |
| **Physical 2-PC LAN Verification**  | 2 Physical PCs over Ethernet/Wi-Fi | **UNVERIFIED** | Requires execution on two physically distinct PC endpoints (see protocol in Section 6). |

---

## 6. Physical 2-PC LAN Verification Protocol

Because automated test harnesses run on a single machine (localhost/virtual adapters), final physical validation must be performed between two independent machines connected over a local network.

### Equipment & Environment Prerequisites
- **PC 1 (Host):** Windows 10/11 x64, assigned static or DHCP IPv4 (e.g., `192.168.1.50`).
- **PC 2 (Client):** Windows 10/11 x64, assigned static or DHCP IPv4 (e.g., `192.168.1.51`).
- **Physical Network:** Direct Ethernet cable or unmanaged LAN switch/router.
- **Firewall:** Windows Defender Firewall configured to allow UDP inbound on port `47584` and game ports.

### Execution Checklist
1. **Deployment:**
   - Copy compiled `bin/steam_api64.dll` to target game folder on both PC 1 and PC 2.
   - Ensure NO Steam client process is running on either machine (`taskkill /F /IM steam.exe`).
   - Disconnect WAN/Internet gateway (unplug WAN cable or disable router upstream) to enforce zero-Internet conditions.
2. **Host Launch (PC 1):**
   - Launch target Unreal Engine game or test runner on PC 1.
   - Verify logs:
     ```
     [ReFix LAN] Initializing ReFix LAN Core...
     [ReFix LAN] Local PeerId: <UUID_1> (DisplayName: HostPC)
     [ReFix LAN] Bound discovery socket to UDP port 47584.
     [ReFix LAN] Bound data socket to UDP port <Ephemeral_1>.
     ```
   - In-game: Host a lobby/match.
3. **Client Launch (PC 2):**
   - Launch target game on PC 2.
   - Verify logs:
     ```
     [ReFix LAN] Initializing ReFix LAN Core...
     [ReFix LAN] Local PeerId: <UUID_2> (DisplayName: ClientPC)
     [ReFix LAN] Bound discovery socket to UDP port 47584 (SO_REUSEADDR).
     [ReFix LAN] Bound data socket to UDP port <Ephemeral_2>.
     ```
   - In-game: Search for LAN matches / Browse lobbies.
4. **Validation Points:**
   - [ ] **Lobby Discovery:** Client displays Host's lobby within 2 seconds of broadcast.
   - [ ] **Handshake & Session:** Client joins lobby; Host logs `PeerDiscovered` and `MemberJoined` for Client's `PeerId`.
   - [ ] **Gameplay P2P Traffic:** High-frequency game datagrams flow between PC 1 and PC 2 via ephemeral UDP sockets.
   - [ ] **Graceful Disconnect:** Client exits match; Host logs `MemberLeft` and updates lobby member list.
   - [ ] **Reconnect:** Client rejoins match; session re-establishes without restarting host process.
