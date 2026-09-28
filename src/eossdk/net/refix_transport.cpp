#include "refix_transport.h"
#include "../core/refix_config.h"
#include "../core/refix_log.h"

#include <winsock2.h>
#include <ws2tcpip.h>

namespace refix {

std::string Endpoint::ToString() const {
    in_addr a{};
    a.s_addr = Ipv4;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s:%u", inet_ntoa(a), (unsigned)Port);
    return buf;
}

Endpoint Endpoint::Parse(const std::string& hostPort) {
    Endpoint e;
    size_t colon = hostPort.find_last_of(':');
    if (colon == std::string::npos) return e;
    std::string host = hostPort.substr(0, colon);
    e.Ipv4 = inet_addr(host.c_str());
    if (e.Ipv4 == INADDR_NONE) e.Ipv4 = 0;
    e.Port = (uint16_t)std::atoi(hostPort.c_str() + colon + 1);
    return e;
}

Transport& Transport::Get() { static Transport t; return t; }

namespace {

// Best-effort local IPv4, used only so peers on other machines have an address
// to answer to. A failure here degrades to loopback, which still works for two
// instances on one PC.
uint32_t DiscoverLocalIpv4() {
    char host[256] = {0};
    if (gethostname(host, sizeof(host)) != 0) return htonl(INADDR_LOOPBACK);

    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) return htonl(INADDR_LOOPBACK);

    uint32_t best = 0;
    for (addrinfo* p = res; p; p = p->ai_next) {
        auto* sa = (sockaddr_in*)p->ai_addr;
        uint32_t addr = sa->sin_addr.s_addr;
        uint32_t hostOrder = ntohl(addr);
        if ((hostOrder >> 24) == 127) continue;      // loopback
        best = addr;
        break;
    }
    freeaddrinfo(res);
    return best ? best : htonl(INADDR_LOOPBACK);
}

} // namespace

bool Transport::Start() {
    if (m_running) return true;

    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    auto& cfg = Config::Get();
    std::string group = cfg.GetString("Network", "DiscoveryGroup", "239.255.71.84");
    m_groupPort = (uint16_t)cfg.GetInt("Network", "ListenPort", 47584);
    m_groupAddr = inet_addr(group.c_str());
    if (m_groupAddr == INADDR_NONE) m_groupAddr = inet_addr("239.255.71.84");

    // Unicast socket: an ephemeral port, so any number of instances coexist.
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { RFLOG(Net, "Transport: socket() failed (%d)", WSAGetLastError()); return false; }

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = 0;
    if (bind(s, (sockaddr*)&bindAddr, sizeof(bindAddr)) != 0) {
        RFLOG(Net, "Transport: bind() failed (%d)", WSAGetLastError());
        closesocket(s);
        return false;
    }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof(yes));
    // Loop multicast back to this host so instances on one PC find each other.
    unsigned char loop = 1;
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&loop, sizeof(loop));
    unsigned char ttl = (unsigned char)cfg.GetInt("Network", "DiscoveryTtl", 1);
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));

    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    getsockname(s, (sockaddr*)&bound, &boundLen);

    // Group socket: shared port, joined by every instance on the machine.
    SOCKET g = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g != INVALID_SOCKET) {
        setsockopt(g, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
        sockaddr_in ga{};
        ga.sin_family = AF_INET;
        ga.sin_addr.s_addr = htonl(INADDR_ANY);
        ga.sin_port = htons(m_groupPort);
        if (bind(g, (sockaddr*)&ga, sizeof(ga)) != 0) {
            RFLOG(Net, "Transport: discovery bind(%u) failed (%d)", m_groupPort, WSAGetLastError());
            closesocket(g);
            g = INVALID_SOCKET;
        } else {
            ip_mreq mreq{};
            mreq.imr_multiaddr.s_addr = m_groupAddr;
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            if (setsockopt(g, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) != 0)
                RFLOG(Net, "Transport: joining %s failed (%d) - LAN discovery may be limited",
                      group.c_str(), WSAGetLastError());
        }
    }

    // Additional broadcast targets for VPNs and multi-subnet setups.
    std::string extras = cfg.GetString("Network", "CustomBroadcasts", "");
    size_t start = 0;
    while (start < extras.size()) {
        size_t comma = extras.find(',', start);
        std::string one = Trim(extras.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!one.empty()) {
            uint32_t addr = inet_addr(one.c_str());
            if (addr != INADDR_NONE) m_extraBroadcasts.push_back(addr);
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }

    m_socket      = (uintptr_t)s;
    m_groupSocket = (uintptr_t)g;
    m_local.Ipv4  = DiscoverLocalIpv4();
    m_local.Port  = ntohs(bound.sin_port);
    m_running     = true;
    m_thread      = std::thread(&Transport::ReceiveLoop, this);

    RFLOG(Net, "Transport up: local=%s discovery=%s:%u",
          m_local.ToString().c_str(), group.c_str(), (unsigned)m_groupPort);
    return true;
}

void Transport::Stop() {
    if (!m_running) return;
    m_running = false;
    if (m_socket != (uintptr_t)-1)      closesocket((SOCKET)m_socket);
    if (m_groupSocket != (uintptr_t)-1) closesocket((SOCKET)m_groupSocket);
    m_socket = m_groupSocket = (uintptr_t)-1;
    if (m_thread.joinable()) m_thread.join();
}

bool Transport::SendTo(const Endpoint& to, const void* data, size_t len) {
    if (!m_running || !to.Valid()) return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = to.Ipv4;
    a.sin_port = htons(to.Port);
    int sent = sendto((SOCKET)m_socket, (const char*)data, (int)len, 0, (sockaddr*)&a, sizeof(a));
    return sent == (int)len;
}

bool Transport::SendToGroup(const void* data, size_t len) {
    if (!m_running) return false;

    auto blast = [&](uint32_t addr) {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = addr;
        a.sin_port = htons(m_groupPort);
        return sendto((SOCKET)m_socket, (const char*)data, (int)len, 0,
                      (sockaddr*)&a, sizeof(a)) == (int)len;
    };

    // Multicast is the primary channel (it reaches other instances on this
    // machine and other machines on the LAN). Limited broadcast is sent as well
    // because some networks and virtual adapters drop multicast: duplicate
    // announcements are harmless, a missed lobby is not.
    bool any = blast(m_groupAddr);
    any = blast(htonl(INADDR_BROADCAST)) || any;
    for (uint32_t extra : m_extraBroadcasts) any = blast(extra) || any;
    return any;
}

void Transport::ReceiveLoop() {
    std::vector<uint8_t> buf(64 * 1024);
    while (m_running) {
        fd_set rd;
        FD_ZERO(&rd);
        SOCKET s = (SOCKET)m_socket;
        SOCKET g = (SOCKET)m_groupSocket;
        if (s != INVALID_SOCKET) FD_SET(s, &rd);
        if (g != INVALID_SOCKET) FD_SET(g, &rd);

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;
        int ready = select(0, &rd, nullptr, nullptr, &tv);
        if (ready <= 0) continue;

        for (SOCKET sock : { s, g }) {
            if (sock == INVALID_SOCKET || !FD_ISSET(sock, &rd)) continue;
            sockaddr_in from{};
            int fromLen = sizeof(from);
            int n = recvfrom(sock, (char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fromLen);
            if (n <= 0) continue;
            Endpoint ep;
            ep.Ipv4 = from.sin_addr.s_addr;
            ep.Port = ntohs(from.sin_port);
            if (m_handler) m_handler(ep, buf.data(), (size_t)n);
        }
    }
}

} // namespace refix
