#pragma once
#include <cstdint>

// Local stand-in for Party Animals' ServerlessP2P relay server, bridged over Steam Datagram Relay.
//
// The game's P2PServerProxy (host) / P2PClientProxy (client) talk UDP to <p2pServerIp>:27100 with
//   [0x01][utf-8 JSON]                     control (SERVER_LOGIN, CLIENT_LOGIN, CLIENT_QUERY, PING_PONG)
//   [0x02][ipv4:4][port:2][Bolt payload]   data, header = the far peer's endpoint
// The local backend publishes p2pServerIp = 127.0.0.1, so each PC talks to its own relay. Peers are
// mapped to virtual endpoints (10.80.x.y), and payloads travel as ISteamNetworkingMessages on a
// dedicated channel to the peer's SteamID (room id == host SteamID64).
namespace ReFixPARelay {
    // Starts the relay ([P2P] RelayPort, default 27100; RelayChannel, default 7). Idempotent.
    void Initialize();

    uint16_t Port();
}
