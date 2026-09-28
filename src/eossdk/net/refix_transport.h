// ReFix EOS v3 - datagram transport.
//
// One UDP socket per process serves everything: peer discovery, lobby state
// replication and game P2P traffic. Discovery rides an IPv4 multicast group,
// which is what makes the same code work in the two situations that matter:
//
//   * two copies of the game on one PC - multicast loops back locally, so the
//     instances see each other with no configuration and no shared port;
//   * two PCs on a LAN - the same datagrams reach the other machine.
//
// Everything else is plain unicast to the endpoint a peer announced.
#pragma once
#include "../core/refix_common.h"
#include <atomic>
#include <thread>
#include <functional>

namespace refix {

struct Endpoint {
    uint32_t Ipv4 = 0;      // network byte order
    uint16_t Port = 0;      // host byte order

    bool Valid() const { return Ipv4 != 0 && Port != 0; }
    bool operator==(const Endpoint& o) const { return Ipv4 == o.Ipv4 && Port == o.Port; }
    bool operator<(const Endpoint& o) const {
        return Ipv4 != o.Ipv4 ? Ipv4 < o.Ipv4 : Port < o.Port;
    }
    std::string ToString() const;
    static Endpoint Parse(const std::string& hostPort);
};

class Transport {
public:
    static Transport& Get();

    using Handler = std::function<void(const Endpoint& from, const uint8_t* data, size_t len)>;

    // Starts the socket and the receive thread. Safe to call repeatedly.
    bool Start();
    void Stop();
    bool Running() const { return m_running; }

    void SetHandler(Handler h) { m_handler = std::move(h); }

    bool SendTo(const Endpoint& to, const void* data, size_t len);
    bool SendToGroup(const void* data, size_t len);

    // The endpoint peers should use to reach us. The address is the LAN address
    // when one could be determined, otherwise loopback - either way the port is
    // the one we are actually bound to.
    Endpoint LocalEndpoint() const { return m_local; }

    uint16_t GroupPort() const { return m_groupPort; }

private:
    Transport() = default;
    void ReceiveLoop();

    std::atomic<bool> m_running{false};
    uintptr_t         m_socket = (uintptr_t)-1;   // SOCKET
    uintptr_t         m_groupSocket = (uintptr_t)-1;
    std::thread       m_thread;
    Handler           m_handler;
    Endpoint          m_local;
    uint32_t          m_groupAddr = 0;
    uint16_t          m_groupPort = 0;
    std::vector<uint32_t> m_extraBroadcasts;
};

} // namespace refix
