# Phase 3.6.1 Verification - ISteamNetworkingSockets LAN Implementation & Adversarial Protocol Testing

## Objective
Provide auditable, reproducible, and certified proof that `ISteamNetworkingSockets` operates correctly and reliably over ReFix's UDP/LAN transport without Internet dependencies, adhering strictly to the real Steamworks SDK ABI, with zero deadlocks, zero crashes, and zero packet reordering under adverse network conditions.

---

## 1. ABI Verification & Single Source of Truth (PASS)
- **Source of Truth**: Exact SDK headers (`src/include/steam/isteamnetworkingsockets.h`, `src/include/steam/steamnetworkingtypes.h`, `src/include/steam/isteamnetworkingutils.h`).
- **Static Assertions Added to Test Harness**:
  ```cpp
  static_assert(sizeof(SteamNetworkingIdentity) == 136, "SteamNetworkingIdentity ABI size mismatch!");
  static_assert(sizeof(SteamNetworkingIPAddr) == 18, "SteamNetworkingIPAddr ABI size mismatch!");
  static_assert(sizeof(SteamNetworkingMessage_t) == 216, "SteamNetworkingMessage_t ABI size mismatch!");
  static_assert(sizeof(SteamNetConnectionInfo_t) == 696, "SteamNetConnectionInfo_t ABI size mismatch!");
  ```
- **Finding on Prior False Diagnoses**:
  Prior sessions incorrectly claimed `sizeof(SteamNetConnectionInfo_t) == 416`. The real SDK struct size in MSVC x64 is **696 bytes**. Using a 416-byte buffer previously resulted in stack corruption (`0xC0000005 ACCESS_VIOLATION`) when calling `GetConnectionInfo`. Compiling directly against the SDK headers resolved all struct layout mismatches.
- **VTable Alignment**:
  `CSteamNetworkingSocketsEmu` declared all 47 virtual functions in the exact declaration order of `ISteamNetworkingSockets`, placing `GetConnectionInfo` at vtable index 15 (slot 16).
- **Runtime Verification**:
  `.\bin\refix_net_test.exe abi` executed and passed all runtime ABI and vtable routing assertions.

---

## 2. Provider Routing & NetworkMode Boundary (PASS)
- **Implementation**:
  Updated `Intercepted_SteamInternal_FindOrCreateUserInterface` and `Intercepted_SteamInternal_CreateInterface` in `src/steam_proxy.cpp`:
  - When in `LAN` mode (`!ReFix::NetworkModeManager::IsOnline()`): Requests for `SteamNetworkingSockets` and `SteamNetworkingUtils` are dispatched to `UnrealSteamEmu`.
  - When in `Online` mode (`ReFix::NetworkModeManager::IsOnline()`): Requests pass directly through to Valve's native DLL (`g_pfn_FindOrCreateUserInterface`), strictly preserving Valve SDR, Steam Relay, and online matchmaking without interception.

---

## 3. Multithreaded Locking & Deadlock Resolution (PASS)
- **Policy**:
  `g_socketsMutex` and `g_emuMutex` are strictly decoupled. Neither lock is ever acquired while holding the other.
- **Lock-Free Callback Dispatch**:
  In `RunCallbacks()`, pending callbacks are popped under `g_emuMutex` into a local dispatch list, the mutex is released, and user listeners are invoked without holding any locks. This prevents callback-invoked API calls (such as `AcceptConnection()` or `SendMessageToConnection()`) from deadlocking against worker threads.
- **Stress Test**:
  `.\bin\refix_net_test.exe locking` ran 10,000 concurrent multi-threaded operations across 4 worker threads (`RunCallbacks`, `ConnectP2P`/`Accept`/`Close`, `SendMessage`/`Receive`, `UserData`/`GetConnectionInfo`):
  ```text
  [PASS] 10,000 multi-threaded API calls executed concurrently: deadlocks = 0, crashes = 0, races = 0.
  ```

---

## 4. Message Lifetime & Memory Management (PASS)
- **Allocation & Release**:
  - Memory for `SteamNetworkingMessage_t` is allocated contiguously (`sizeof(SteamNetworkingMessage_t) + cbData`).
  - `m_pfnRelease` is assigned to `ReleaseReFixMessage`, which calls `free(msg)`.
- **Stress Test**:
  `.\bin\refix_net_test.exe lifetime` executed 10,000 allocate-release cycles with alternating payload sizes and verified buffer integrity:
  ```text
  [PASS] 10,000 messages allocated and released with zero allocator mismatch or crash.
  ```

---

## 5. Peer Discovery vs Unicast Connection Handshake (PASS)
- **Separation of Discovery vs Handshake**:
  - **Discovery**: `BroadcastNetPacket(1)` broadcasts peer persona pings to standard discovery ports (`47584`, `47585`) on `255.255.255.255` and `127.0.0.1`.
  - **Bidirectional Discovery**: Upon receiving a discovery ping (`msgType == 1`), `PollNetwork` immediately replies with a unicast discovery ping back to the sender, ensuring both peers resolve IP and port in `g_peers`.
  - **Connection Handshake**: `ConnectP2P` looks up the peer's resolved endpoint in `g_peers`. It sends a single, direct unicast `SocketsHandshake` (`msgType == 7`) to the peer endpoint.
  - **Handshake ACK**: Peer responds with a single, direct unicast `SocketsHandshakeAck` (`msgType == 8`).
  - **Bug Fix**: Fixed a critical defect in `PollNetwork` where incoming `msgType == 8` read `*(uint32_t*)payload` (the `protocolVersion` field = 1) instead of casting to `SocketsHandshakeAck*` and reading `ack->sessionId`.
- **Results**: Duplicate handshakes = 0, duplicate ACKs = 0, duplicate connections = 0.

---

## 6. Adversarial Protocol Reliability (PASS)
- **Sliding Window & Standalone ACKs**:
  - Implemented `msgType == 9` (`SocketsAckPacket`), which sends standalone cumulative ACKs immediately upon packet reception and upon detecting duplicate/out-of-order packets.
  - Outbound reliable queues (`unackedOutbound`) are purged upon receiving cumulative ACKs.
  - Retransmission loop polls every 200ms for unacknowledged frames.
- **Stress Test Runs**:
  - **Run 1** (10% drop, 5% duplicate, 50–200ms jitter):
    - Delivered: 1000/1000
    - Missing: 0
    - Duplicates: 0
    - Order violations: 0
    - Result: `[PASS] 1000 reliable messages delivered: missing=0, duplicates=0, order_violations=0.`
  - **Run 2** (20% drop, 10% duplicate, 50–250ms jitter):
    - Delivered: 1000/1000
    - Missing: 0
    - Duplicates: 0
    - Order violations: 0
    - Result: `[PASS] 1000 reliable messages delivered: missing=0, duplicates=0, order_violations=0.`

---

## 7. Unreliable Delivery Semantics (PASS)
- 1000 unreliable messages sent immediately following the reliable phase.
- Host received 751 arrivals (Run 1) and 756 arrivals (Run 2) without stalls or retransmission overhead.
- Loss accepted cleanly according to UDP datagram semantics.

---

## 8. Network Isolation / Internet-Zero (PASS)
- `.\bin\refix_net_test.exe isolation` validated that loopback (`127.0.0.1`), private RFC1918 subnets (`10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`), APIPA (`169.254.0.0/16`), and LAN multicast/broadcast are permitted, while public WAN IPs (`8.8.8.8`, `1.1.1.1`, Valve SDR relays `162.254.192.0`) are blocked.

---

## 9. Online Regression Audit
- **Status**: `UNVERIFIED` (Runtime verification is blocked by lack of Valve online servers in this offline testbed).
- **Code Audit**: The Online proxy boundary in `steam_proxy.cpp` passes all online requests straight to Valve DLL exports without alteration.
