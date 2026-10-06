# ReFix Two-PC Physical LAN Verification Harness

## Target Environment
- **PC A (Host)**: IP `192.168.x.A`, Subnet `255.255.255.0`
- **PC B (Client)**: IP `192.168.x.B`, Subnet `255.255.255.0`
- **Physical Link**: Same L2 broadcast domain (Ethernet switch or Wi-Fi AP)
- **Steam Client**: ABSENT (Steam.exe killed, service stopped, no login)
- **Internet**: DISCONNECTED or filtered (WAN egress blocked)
- **Operating System**: Windows 10/11 x64

---

## Deployment & Configuration

### 1. File Distribution
Deploy the compiled artifacts to both machines in identical directory structures:
- `bin/steam_api64.dll` (ReFix proxy)
- `bin/refix_net_test.exe` (Phase 3 loopback/LAN harness)
- `bin/refix.ini`

### 2. Configuration (`refix.ini` on PC A and PC B)
```ini
[Network]
Mode=force_lan

[Proxy]
Offline=1
BlockWanEgress=1
```

---

## Execution Sequence

### Phase 1: Peer Discovery & Broadcast
1. **PC A**: Starts listen socket on UDP port 47584:
   ```cmd
   refix_net_test.exe host
   ```
2. **PC B**: Broadcasts discovery ping / query on UDP 47584 (subnet broadcast `192.168.x.255`):
   ```cmd
   refix_net_test.exe client
   ```
3. **Verification**:
   - PC A receives announce and registers PC B endpoint.
   - Zero packets transmitted to public IP addresses (Internet-zero egress).

### Phase 2: Lobby & Matchmaking
1. PC A creates local simulated lobby:
   - `SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, 4)`
2. PC B queries and receives lobby list:
   - `SteamMatchmaking()->RequestLobbyList()`
   - `SteamMatchmaking()->JoinLobby(lobbyID)`
3. **Verification**:
   - `LobbyEnter_t` and `LobbyDataUpdate_t` dispatched locally on both peers without touching Valve backend.

### Phase 3: P2P Connection Handshake (`ConnectP2P`)
1. PC B calls `ISteamNetworkingSockets::ConnectP2P(HostSteamID, 0, 0, nullptr)`.
2. Initial Handshake packet (msgType 7) transmitted over UDP 47584 to PC A.
3. PC A accepts connection via `AcceptConnection(hConn)`.
4. PC A replies with HandshakeAck (msgType 8).
5. **Verification**:
   - Both nodes transition state: `Connecting` -> `Connected`.
   - Callbacks `SteamNetConnectionStatusChangedCallback_t` fire on both sides.

### Phase 4: Interleaved Data Transmission (Reliable & Unreliable)
1. PC B transmits 500 interleaved messages:
   - 250 Reliable: `REL_<seq>`
   - 250 Unreliable: `UNR_<seq>`
2. PC A receives messages on connection, checking sequence continuity:
   - Zero out-of-order delivery for Reliable packets.
   - Duplicate packets suppressed by receive window.
3. PC A responds with `INTERLEAVED_ACK`.
4. **Verification**:
   - PC B confirms complete receipt of ACK.

### Phase 5: Graceful Termination (`CloseConnection`)
1. PC B closes connection with reason 42:
   - `ISteamNetworkingSockets::CloseConnection(conn, 42, "TestDone", false)`.
2. PC A observes `k_ESteamNetworkingConnectionState_ClosedByPeer`.
3. Snapshot data (userData, debug description) verified on both ends.

### Phase 6: Session Teardown & Reconnect
1. PC B immediately attempts reconnect:
   - `ISteamNetworkingSockets::ConnectP2P(HostSteamID, 0, 0, nullptr)`.
2. PC A allocates a fresh connection handle (`hConn2 != hConn1`).
3. Handshake completes and second session transmits test payload cleanly.
4. **Verification**:
   - No handle leak, no ghost session state, no socket binding failure on re-bind.

---

## Gate Status
- **Automated Multi-test in Loopback**: `PASS` (Suite 1 - Suite 13)
- **Two-PC Physical LAN**: `UNVERIFIED` (Awaiting dual-machine execution on target test bench)
