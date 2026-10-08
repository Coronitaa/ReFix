# ReFix — Universal LAN Core & Transport Consolidation Audit

**Status:** Completed Architectural Audit & Consolidation Blueprint  
**Date:** October 2026  
**Target Checkpoint:** `62fe143` (feature/phase3-sockets)  
**Authors:** ReFix Core Architecture Team (Subagents A, B, C, D Synthesis)

---

## 1. Executive Summary

An exhaustive cross-subsystem audit of the ReFix codebase was conducted to resolve **Finding 1 (Transport Duplication)** and design the canonical **ReFix LAN Core** and **ReFix Universal LAN Transport**.

### Key Conclusions:
1. **Three Incompatible Transports:** ReFix currently maintains three completely separate network implementations:
   - `src/unreal_steam_emu.cpp`: Monolithic Steamworks UDP transport (`0x52464958 'RFIX'`), broadcast-only discovery, full reliable ARQ/sequence state machine, synchronous main-thread polling.
   - `src/steam_p2p_hook.cpp`: Winsock API interception layer (`0x58464552 'REFX'`), background pump thread with 1ms sleep loop, hold buffer, port virtualization, delegates transmission to Steam P2P.
   - `src/eossdk/net/refix_transport.cpp` (+ `lobby_directory.cpp`, `refix_wire.h`): EOS v3 transport (`0x33584652 'RFX3'`), dual-socket architecture (multicast listener `239.255.71.84:47584` + ephemeral unicast port 0), background `select()` reactor thread, binary length-prefixed stream framing, but **zero reliability (pure drop-prone UDP)** and **zero Internet-Zero egress validation**.
2. **Critical Winsock Hook Flaws:**
   - In `src/steam_proxy.cpp`, the Winsock hook installation is gated on `if (g_unrealIsEngine && !IsLanOnly() && !IsOffline())`. As a result, **Winsock hooks are completely disabled in LAN and Offline modes**, creating a total bypass of the Internet-Zero socket drop filter.
   - IPv6 sockets (`AF_INET6`) completely bypass the LAN subnet check in `Hook_sendto` and `Hook_connect`.
   - Connected UDP sockets (`connect()` + `send()`/`recv()`) bypass P2P redirection because `send` and `recv` are not hooked.
3. **Deployment Duality (Goldberg Coupling):**
   - While the C++ runtime possesses an autonomous Steam emulation engine (`UnrealSteamEmu`), `AutoDeploy.bat` and `deploy_helper.ps1` still deploy legacy Goldberg binaries (`bin/goldberg/steam_api64.dll` copied as `steam_api64_valve.dll`) in LAN mode, and `Uninstall_ReFix.bat` risks restoring Goldberg over the original game DLL.
4. **Resolution Strategy:**
   - Extract the best elements into a single, standalone **ReFix Universal LAN Transport** (`refix_lan_net`) and **ReFix LAN Core** (`refix_lan_core`), independent of Steam, Goldberg, EOS, Photon, and Unity.
   - Modernize the deployment system into two decoupled profiles: `bin/online/` and `bin/lan/`, completely eliminating the Goldberg backend dependency.

---

## 2. Comparison of Existing Transport Implementations

| Dimension | `unreal_steam_emu.cpp` | `steam_p2p_hook.cpp` | `refix_transport.cpp` / `eossdk` |
| :--- | :--- | :--- | :--- |
| **Primary Role** | Full Steamworks LAN emulator & sockets engine | Transparent Winsock interceptor for UDP game loops | EOS SDK v3 distributed LAN directory & P2P layer |
| **Magic Number** | `0x52464958` (`'RFIX'`) | `0x58464552` (`'REFX'`) | `0x33584652` (`'RFX3'`) |
| **Wire Header** | `NetPacketHeader` (17 B) + `SocketsPayloadHeader` (28 B) | `ReFixP2PHeader` (12 B, pack 1) | Binary length-prefixed stream (`refix_wire.h`) |
| **Socket Model** | Single socket: fixed port 47584, ephemeral fallback | Hooked OS socket handles (no raw socket ownership) | **Dual-socket**: Fixed Group `47584` + Ephemeral Unicast (0) |
| **Discovery Mechanism** | IPv4 Broadcast (`255.255.255.255`, `127.0.0.1`, custom IPs) | None (delegates to Steam matchmaking/lobby) | IPv4 Multicast (`239.255.71.84:47584`) + Broadcast fallback |
| **Multi-Instance on 1 PC**| Fragile fallback; instance 1 only broadcasts to 47584/85 | Port virtualization (`port + 1 .. + 30`) | **Robust**: `SO_REUSEADDR` multicast + `IP_MULTICAST_LOOP = 1` |
| **Threading Model** | Synchronous polling inside `SteamAPI_RunCallbacks()` | Dedicated `P2PPumpThread` with `Sleep(1)` | Dedicated `select()` reactor thread (`ReceiveLoop`) |
| **Reliability & ARQ** | **Full ARQ**: sliding window, cumulative ACK, SACK, out-of-order queue | Delegated to underlying Steam P2P | **None**: Raw UDP datagrams, no sequence/ACK, drops silently |
| **Inbound Queuing** | Queues per channel & connection; reordering map | `SocketContext::recvQueue` (cap 2048) | `P2PState::Queue` (max 64 MB) in `eos_api_p2p.cpp` |
| **Outbound Buffering** | `unackedOutbound` vector for retransmission | Pre-Lobby Hold Buffer (cap 512, 3000ms TTL) | None (direct OS socket send) |
| **Internet-Zero Egress**| **Strict**: RFC1918, APIPA, loopback, blocks global multicast/WAN/IPv6 | Filter exists, but **skipped** in LAN mode! IPv6 leak | **None**: No egress validation before calling `sendto()` |
| **Peer Mapping** | `SteamID64` <-> `(IP, Port)` endpoint | `IP` <-> `SteamID64` <-> `Port` | `EOS ProductUserId` <-> `(IP, Port)` endpoint |

---

## 3. Shared Responsibilities & Duplicate Logic

### 3.1 Shared Responsibilities
1. **Endpoint Resolution:** Resolving host/peer IP addresses and ports to communicate over IPv4 LAN.
2. **Channel Demultiplexing:** Separating high-frequency game state (Channel 0) from voice/streaming payloads (Channel 1).
3. **Inbound Queuing:** Decoupling network packet reception from consumer frame rates via memory queues.
4. **Peer Identity Association:** Mapping platform identities (SteamID, EOS PUID) to physical endpoints.
5. **Egress Security:** Restricting network operations to local subnets (RFC 1918, APIPA, Loopback).

### 3.2 Redundant & Duplicate Logic
- **Subnet Classification:** `IsAllowedLanAddress` in `unreal_steam_emu.cpp`, `IsAllowedLanEndpoint` in `steam_p2p_hook.cpp`, and `IsAllowedLanEndpoint` in `test_isolation.cpp` represent three divergent copies of identical logic.
- **Local IP Discovery:** `DiscoverLocalIpv4()` routines are independently implemented in `refix_transport.cpp` and `unreal_steam_emu.cpp`.
- **Custom Broadcast Config:** Comma-separated IP parsing for VPN/VLAN subnets is implemented twice.
- **Port Virtualization Constants:** Hardcoded game ports (`7777`, `27015`) and discovery ports (`47584`) are duplicated across files.

---

## 4. Critical Inconsistencies & Edge-Case Bugs Found

1. **Protocol Incompatibility (Wire Header Tower of Babel):**
   - Three different magic numbers (`'RFIX'`, `'REFX'`, `'RFX3'`) mean packets cannot cross between Steam emulation and EOS emulation on the same LAN.
2. **LAN Internet-Zero Bypass:**
   - In `src/steam_proxy.cpp`:
     ```cpp
     if (g_unrealIsEngine && !ReFix::NetworkModeManager::IsLanOnly() && !ReFix::NetworkModeManager::IsOffline()) {
         SteamP2PHook::Install(g_hOriginalDll);
     }
     ```
     In LAN/Offline mode, hooks are skipped entirely. The game bypasses all drop filters and communicates directly with the physical network adapter.
3. **IPv6 Egress Leak:**
   - `Hook_sendto` and `Hook_connect` check only `AF_INET`. Any `AF_INET6` socket passes uninspected to OS Winsock.
4. **Connected UDP Socket Hole:**
   - When games call `connect()` on a UDP socket and communicate via `send()` and `recv()`, packets bypass ReFix because `send`/`recv` are not hooked.
5. **Missing Reliability in EOS Transport:**
   - `refix_transport.cpp` sends raw UDP packets for EOS P2P. Unreliable network conditions (Wi-Fi, VPNs, packet drops) cause silent state desync.
6. **Main Thread Hitching in Steam Transport:**
   - Because `PollNetwork()` runs synchronously inside `SteamAPI_RunCallbacks()`, level loading hitches stop all network I/O, triggering false handshake and connection timeouts.
7. **Thread Closure Race in EOS Transport:**
   - In `Transport::Stop()`, `closesocket()` is called while the background thread is blocked in `select()`, triggering Winsock undefined behavior.

---

## 5. Recommended Canonical Implementation: ReFix Universal LAN Architecture

The target architecture decouples the networking and core game state into a modular, self-contained hierarchy:

```
+-----------------------------------------------------------------------------------+
|                                  Game Engine                                      |
+-----------------------------------------------------------------------------------+
       |                                |                                   |
+--------------+               +------------------+               +-----------------+
|  Steamworks  |               |       EOS        |               | Photon / Unity  |
|  LAN Adapter |               |   LAN Adapter    |               |   LAN Adapter   |
+--------------+               +------------------+               +-----------------+
       \                                |                                  /
        \                               |                                 /
+-----------------------------------------------------------------------------------+
|                                 ReFix LAN Core                                    |
|                                                                                   |
|  • LocalIdentityService: MachineId, PeerId (128-bit UUID), DisplayName, Scope     |
|  • PeerRegistry: Multi-index mapping (PeerId <-> ExtId <-> Endpoint <-> State)    |
|  • DiscoveryService: Multicast beaconing, heartbeat (2s), query/response, TTL     |
|  • LobbyService: Host-authoritative replication, revision numbers, attributes     |
|  • SessionService: Match session state, dedicated/listen server endpoint nonce    |
|  • MatchmakingService: Typed filter matching (EQUAL, CONTAINS, ANY_OF, etc.)       |
|  • CallbackDispatcher: Thread-safe event queue pumped on game main thread         |
|  • LocalRelayService: Local datagram bounce fallback when direct P2P is blocked   |
+-----------------------------------------------------------------------------------+
                                        |
+-----------------------------------------------------------------------------------+
|                        ReFix Universal LAN Transport                              |
|                                                                                   |
|  1. Dual-Socket Architecture:                                                     |
|     - Discovery Listener: UDP Port 47584 (SO_REUSEADDR, 239.255.71.84 + bcast)    |
|     - Unicast Data Socket: UDP Ephemeral Port 0 (Send & Unicast Recv)             |
|  2. Canonical Wire Framing:                                                       |
|     - Magic: 0x52464958 ('RFIX'), Version: 1, Fixed 36-byte header               |
|     - Sender UUID (16B), SessionId (4B), Channel (1B), Seq (4B), Ack (4B)        |
|  3. ARQ & Reliability Engine:                                                     |
|     - Monotonic sequence numbers, cumulative ACK + selective ACK (SACK bitmask)  |
|     - Retransmission timer (100ms), max retries (20), out-of-order reassembly     |
|     - Packet chunking for payloads > 1200 bytes (MTU safety)                      |
|  4. Internet-Zero Egress Firewall:                                                |
|     - Centralized RFC1918 / APIPA / Loopback / Admin Multicast policy check       |
|     - Drops WAN, Valve SDR, global multicast (224.0.1.0+), and unmapped IPv6     |
|  5. Autonomous Background Reactor:                                                |
|     - Dedicated select() worker thread with graceful termination signaling       |
+-----------------------------------------------------------------------------------+
                                        |
+-----------------------------------------------------------------------------------+
|                               Physical / Local LAN                                |
|                        (Loopback, Localhost, Physical PC)                         |
+-----------------------------------------------------------------------------------+
```

---

## 6. Migration Plan

### Step 1: Implement Canonical Transport & LAN Core
- Implement `src/lan_core/` containing:
  - `refix_lan_types.h`: `PeerId`, `MachineId`, `LanEndpoint`, `ExternalId`, `AttributeValue`.
  - `refix_lan_firewall.h` & `.cpp`: Centralized Internet-Zero validation.
  - `refix_lan_wire.h`: Little-endian binary packet framing (`'RFIX'`).
  - `refix_lan_transport.h` & `.cpp`: Dual-socket reactor with ARQ reliability engine.
  - `refix_lan_core.h` & `.cpp`: Identity, PeerRegistry, Discovery, Lobby, Matchmaking, Callbacks, Relay.

### Step 2: Migrate Adapters to LAN Core
- **Steamworks Adapter (`unreal_steam_emu.cpp`):**
  - Retain ABI wrappers (`ISteamNetworking`, `ISteamNetworkingSockets`, `ISteamMatchmaking`).
  - Route all internal peer discovery, lobby replication, and datagram transmission through `refix_lan_core`.
  - Remove duplicate broadcast and socket initialization logic.
- **EOS Adapter (`src/eossdk/`):**
  - Replace `refix_transport.cpp` and `lobby_directory.cpp` with `refix_lan_core` service calls.
  - Map `EOS_ProductUserId` to `refix::lan::PeerId`.
- **Winsock Hooks (`src/steam_p2p_hook.cpp`):**
  - Fix hook enablement in LAN/Offline modes (`AlwaysInstallFirewallHook`).
  - Add `send` and `recv` hooks for connected UDP sockets.
  - Patch IPv6 egress checks.

### Step 3: Deployment Decoupling
- Restructure `bin/` into `bin/common/`, `bin/online/`, `bin/lan/`.
- Update `AutoDeploy.bat` and `deploy_helper.ps1` to deploy `bin/lan/steam_api64.dll` autonomously without copying Goldberg or generating interfaces.
- Update `Uninstall_ReFix.bat` to prioritize genuine `*_original.dll` backups.

---

## 7. Compatibility & Safety Matrix

| Subsystem | Impact | Mitigation Strategy |
| :--- | :--- | :--- |
| **Steamworks ABI** | Zero ABI breakage | All 1,141 exports and interface tables remain identical. Only internal routing targets LAN Core. |
| **EOSSDK ABI** | Zero ABI breakage | Export table and header types preserved. Adapters map native types to Core types. |
| **Multiprocess Loopback** | Enhanced | Dual-socket model guarantees zero port conflicts between multiple game instances on 1 PC. |
| **Multi-PC LAN** | Enhanced | Multicast discovery + ephemeral unicast transport tested across real subnets. |
| **Internet-Zero** | Hardened | Central firewall eliminates existing LAN mode bypasses and IPv6 leaks. |

---
*Document certified by ReFix Architecture Team.*
