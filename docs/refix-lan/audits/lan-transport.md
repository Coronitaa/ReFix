# LAN Transport Audit Report (`unreal_steam_emu.cpp`)

## Overview
The transport uses a custom UDP protocol for P2P messaging and LAN lobby discovery. It binds a UDP socket to `g_listenPort` (default 47584) with `SO_BROADCAST` and `SO_REUSEADDR`.

## 1. Multicast Discovery & Broadcasts
- **Mechanism:** Discovery works via UDP broadcasts to `255.255.255.255` and custom user-provided IPs (`g_customBroadcasts`). It does not use true IGMP multicast.
- **Listen Socket:** `PollNetwork()` runs on the main execution thread inside `RunCallbacks()` and `GameServer_RunCallbacks()` to poll the socket using non-blocking `recvfrom`.

## 2. UDP Unicast Fallback
- **Handling:** `SendMessageToUser` checks the `g_peers` map for the remote `CSteamID`. If the peer's IP and port are cached (from previous discovery pings or packets), the packet is sent directly via unicast `sendto()`. If the peer is unknown, it falls back to a global UDP broadcast.

## 3. Packet Format
Traffic begins with `NetPacketHeader` followed by payload data:
```cpp
struct NetPacketHeader {
    uint32_t magic;      // 0x52464958 'RFIX'
    uint8_t  msgType;    // 1: Ping, 2: LobbyAnnounce, 3: LobbyQuery, 4: LobbyJoin, 5: P2P
    uint64_t senderID;   // The Local CSteamID
    uint32_t appID;
    uint32_t payloadLen;
};
```
For P2P (`msgType == 5`), the payload contains an `int32` channel ID followed by raw message data.

## 4. Peer Identity
- Peers are identified via their 64-bit `CSteamID`.
- `SteamNetworkingIdentity` correctly resolves these IDs (`GetSteamID64()`).
- The `g_peers` map tracks `DiscoveredPeer` (Steam ID, IPv4 address, port, and `lastSeen` timestamp).

## 5. Connection State & Timeouts
- Connections are fully stateless.
- Peers and lobbies track a `lastSeen` monotonic timestamp upon receiving packets. However, there is no active timeout cleanup or "disconnect" logic in `PollNetwork`.
- `ISteamNetworkingSockets` returns dummy connection data (`k_ESteamNetworkingConnectionState_Connected` and 5ms ping).

## 6. Threading & Queues
- **Threading:** No background socket threads exist. Polling is done synchronously inside `RunCallbacks`, guarded by `g_emuMutex`.
- **Queues:** Incoming messages map to channels via `std::map<int, std::queue<P2PPacket>> g_p2pIncoming;`.

## Implementing `SendMessageToConnection` & `ReceiveMessagesOnConnection`
To implement the `ISteamNetworkingSockets` connection-based API on top of this backend:
1. **Connection Manager:** Maintain a registry (e.g. `std::map<HSteamNetConnection, ConnectionData>`) mapping `hConn` handles to remote `CSteamID`s and virtual ports.
2. **Connecting:** `ConnectP2P` should allocate an `hConn`, bind it to the target identity, register it, and queue a `SteamNetConnectionStatusChangedCallback_t` to fake the handshake.
3. **Sending:** `SendMessageToConnection` maps `hConn` to the remote `CSteamID`. It then relies on the existing Unicast/Broadcast routing (`SendMessageToUser` logic). To distinguish connections, either add `hConn` to the payload, dedicate a channel mapping, or introduce a new `msgType` (e.g., 6 for Sockets P2P).
4. **Receiving:** `PollNetwork` can route incoming connection packets to a per-connection queue (`g_connIncoming[hConn]`). `ReceiveMessagesOnConnection` simply drains this queue, formatting them into `SteamNetworkingMessage_t` structures like `ReceiveMessagesOnChannel` does.
