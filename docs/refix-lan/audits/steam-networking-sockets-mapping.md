# Phase 3: SteamNetworkingSockets Mapping Architecture

## 1. Handle Mapping
The SteamNetworkingSockets API relies on connection handles (HSteamNetConnection, an integer) to identify active connections. ReFix needs to maintain a strict mapping between these handles, internal connection state objects, and physical LAN endpoints.

### Mapping Structure
- **Global Handle Table**: A thread-safe data structure (e.g., std::unordered_map<HSteamNetConnection, std::shared_ptr<ReFixConnection>>) maps integer handles to internal connection objects.
- **ReFixConnection**: An internal class representing a single peer-to-peer connection. It contains:
  - HSteamNetConnection m_hConnection: The handle exposed to the game.
  - CSteamID m_peerSteamID: The Steam ID of the remote peer.
  - sockaddr_in m_peerEndpoint: The resolved local IP and port for the LAN peer.
  - EReFixConnectionState m_eState: Current state of the connection.
  - Queue structures for reliable/unreliable message buffering (mimicking Steam's internal queuing).
  - The underlying socket (or reference to a multiplexed socket manager).

## 2. Connection State Machine
The connection must faithfully emulate Steam's ESteamNetworkingConnectionState transitions.

### Internal States (EReFixConnectionState)
1. **STATE_NONE**: Uninitialized or fully torn down connection.
2. **STATE_CONNECTING**: Connection initiated by the local user. Handshake sent to the remote peer, waiting for a response.
3. **STATE_FINDING_ROUTE**: Used if peer IP resolution is currently pending (e.g., querying the Lobby manager for the peer's LAN address).
4. **STATE_CONNECTED**: Handshake verified. Connection is established and ready for payload traffic.
5. **STATE_CLOSED_BY_PEER**: The remote peer gracefully closed the connection.
6. **STATE_PROBLEM_DETECTED_LOCALLY**: A timeout, socket error, or unrecoverable transmission failure occurred.

## 3. Peer Resolution Flow
Steamworks normally obscures IP addresses, routing traffic through Steam's backbone via CSteamID. ReFix routes this locally.

### Resolution Steps (Lobby -> Peer Identity -> LAN endpoint -> ConnectP2P)
1. **Lobby Context**: When a user joins a ReFix LAN lobby, the lobby system tracks all members, storing a CSteamID -> LAN Endpoint (IP:Port) mapping in memory.
2. **Connection Request**: The game calls ISteamNetworkingSockets::ConnectP2P(CSteamID remote_id, ...).
3. **Endpoint Lookup**: The networking layer queries the ReFix lobby manager for emote_id's corresponding LAN endpoint.
   - *If found*: The socket layer retrieves the IP address and port.
   - *If not found*: Send a LAN broadcast or query the local discovery service to resolve the CSteamID on the fly.
4. **Connection Initiation**: A new ReFixConnection is allocated, state set to STATE_CONNECTING, and the local handshake is transmitted to the resolved LAN endpoint.

## 4. Minimal Local Handshake
To ensure traffic validity and emulate connection establishment, a lightweight handshake protocol is required upon initiating a connection.

### Handshake Packet Structure
`cpp
#pragma pack(push, 1)
struct ReFixHandshake {
    uint32_t magic;     // Always 0x52464958 ('RFIX')
    uint16_t version;   // Protocol version (e.g., 0x0001)
    uint64_t steam_id;  // The CSteamID of the sender initiating the connection
};
#pragma pack(pop)
`

### Handshake Flow
1. **Sender**: Sends ReFixHandshake immediately after socket creation/binding to identify itself to the remote LAN IP.
2. **Receiver**: Validates magic and ersion. If valid, compares steam_id against expected peers.
3. **Acceptance**: The receiver accepts the connection (firing OnSteamNetConnectionStatusChanged callback for the game) and replies with an acknowledgement (either a similar handshake or a dedicated ACK packet). Both peers transition to STATE_CONNECTED.
