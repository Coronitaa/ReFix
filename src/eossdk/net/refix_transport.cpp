#include "refix_transport.h"
#include "../core/refix_config.h"
#include "../core/refix_log.h"
#include "../../lan_core/refix_lan_firewall.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <vector>
#include <algorithm>

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

// Best-effort local IPv4 with adapter scoring to penalize virtual adapters (e.g. VirtualBox)
uint32_t DiscoverLocalIpv4() {
    ULONG outBufLen = 15000;
    std::vector<uint8_t> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    DWORD ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    }

    struct Candidate {
        uint32_t ip; // host byte order
        int score;
    };
    std::vector<Candidate> candidates;

    if (ret == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr != nullptr; curr = curr->Next) {
            if (curr->OperStatus != IfOperStatusUp) continue;
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

            std::string nameLower;
            if (curr->FriendlyName) {
                int len = WideCharToMultiByte(CP_UTF8, 0, curr->FriendlyName, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    nameLower.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, curr->FriendlyName, -1, &nameLower[0], len, nullptr, nullptr);
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });
                }
            }
            std::string descLower;
            if (curr->Description) {
                int len = WideCharToMultiByte(CP_UTF8, 0, curr->Description, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    descLower.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, curr->Description, -1, &descLower[0], len, nullptr, nullptr);
                    std::transform(descLower.begin(), descLower.end(), descLower.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });
                }
            }

            bool isVirtual = false;
            const char* vKeywords[] = {
                "virtual", "vbox", "vmware", "hyper-v", "wsl", "docker",
                "tap", "tun", "tailscale", "zerotier", "wireguard", "vpn", "host-only"
            };
            for (const char* kw : vKeywords) {
                if (nameLower.find(kw) != std::string::npos || descLower.find(kw) != std::string::npos) {
                    isVirtual = true;
                    break;
                }
            }

            bool hasGateway = (curr->FirstGatewayAddress != nullptr);

            for (PIP_ADAPTER_UNICAST_ADDRESS u = curr->FirstUnicastAddress; u != nullptr; u = u->Next) {
                if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
                auto* sin = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
                uint32_t ip = ntohl(sin->sin_addr.s_addr);

                if ((ip & 0xFF000000) == 0x7F000000) continue; // Loopback

                bool isRfc1918 = ((ip & 0xFF000000) == 0x0A000000) ||   // 10.0.0.0/8
                                 ((ip & 0xFFF00000) == 0xAC100000) ||   // 172.16.0.0/12
                                 ((ip & 0xFFFF0000) == 0xC0A80000);     // 192.168.0.0/16
                bool isApipa = ((ip & 0xFFFF0000) == 0xA9FE0000);       // 169.254.0.0/16
                bool isVBoxDefault = ((ip & 0xFFFFFF00) == 0xC0A83800); // 192.168.56.0/24

                int score = 0;
                if (curr->IfType == IF_TYPE_ETHERNET_CSMACD && !isVirtual) {
                    score += 100;
                } else if (curr->IfType == IF_TYPE_IEEE80211 && !isVirtual) {
                    score += 90;
                } else if (!isVirtual) {
                    score += 50;
                } else {
                    score -= 100;
                }

                if (hasGateway) score += 40;
                if (isRfc1918) score += 30;
                if (isApipa) score -= 50;
                if (isVBoxDefault) score -= 150;

                candidates.push_back({ip, score});
            }
        }
    }

    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score > b.score;
        });
        return htonl(candidates.front().ip);
    }

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
        bool boundGroup = (bind(g, (sockaddr*)&ga, sizeof(ga)) == 0);
        if (!boundGroup) {
            for (uint16_t altPort = 47585; altPort <= 47589; ++altPort) {
                ga.sin_port = htons(altPort);
                if (bind(g, (sockaddr*)&ga, sizeof(ga)) == 0) {
                    m_groupPort = altPort;
                    boundGroup = true;
                    RFLOG(Net, "Transport: discovery bound to fallback port %u", altPort);
                    break;
                }
            }
        }
        if (!boundGroup) {
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
    // Wake up ReceiveLoop thread gracefully
    if (m_socket != (uintptr_t)-1 && m_local.Port != 0) {
        sockaddr_in loopSin{};
        loopSin.sin_family = AF_INET;
        loopSin.sin_addr.s_addr = htonl(0x7F000001);
        loopSin.sin_port = htons(m_local.Port);
        char wake = 0;
        sendto((SOCKET)m_socket, &wake, 1, 0, (sockaddr*)&loopSin, sizeof(loopSin));
    }
    if (m_thread.joinable()) m_thread.join();
    if (m_socket != (uintptr_t)-1)      closesocket((SOCKET)m_socket);
    if (m_groupSocket != (uintptr_t)-1) closesocket((SOCKET)m_groupSocket);
    m_socket = m_groupSocket = (uintptr_t)-1;
}

bool Transport::SendTo(const Endpoint& to, const void* data, size_t len) {
    if (!m_running || !to.Valid()) return false;
    uint32_t ipHost = ntohl(to.Ipv4);
    if (!refix::lan::LanFirewall::Get().IsAllowedIpv4(ipHost)) {
        refix::lan::LanFirewall::Get().RecordBlockedEgress();
        return false;
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = to.Ipv4;
    a.sin_port = htons(to.Port);
    int sent = sendto((SOCKET)m_socket, (const char*)data, (int)len, 0, (sockaddr*)&a, sizeof(a));
    return sent == (int)len;
}

bool Transport::SendToGroup(const void* data, size_t len) {
    if (!m_running) return false;

    auto blastPort = [&](uint32_t addr, uint16_t port) {
        if (!refix::lan::LanFirewall::Get().IsAllowedIpv4(ntohl(addr))) {
            refix::lan::LanFirewall::Get().RecordBlockedEgress();
            return false;
        }
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = addr;
        a.sin_port = htons(port);
        return sendto((SOCKET)m_socket, (const char*)data, (int)len, 0,
                      (sockaddr*)&a, sizeof(a)) == (int)len;
    };

    auto blast = [&](uint32_t addr) {
        bool res = blastPort(addr, m_groupPort);
        for (uint16_t alt = 47584; alt <= 47589; ++alt) {
            if (alt != m_groupPort) {
                blastPort(addr, alt);
            }
        }
        return res;
    };

    // Multicast is the primary channel (it reaches other instances on this
    // machine and other machines on the LAN). Limited broadcast is sent as well
    // because some networks and virtual adapters drop multicast: duplicate
    // announcements are harmless, a missed lobby is not.
    bool any = blast(m_groupAddr);
    any = blast(htonl(INADDR_BROADCAST)) || any;
    any = blast(htonl(INADDR_LOOPBACK)) || any;
    if (m_local.Ipv4 != 0 && m_local.Ipv4 != htonl(INADDR_LOOPBACK)) {
        blast(m_local.Ipv4);
        uint32_t hostIp = ntohl(m_local.Ipv4);
        uint32_t subnetBcast = htonl((hostIp & 0xFFFFFF00) | 0x000000FF);
        blast(subnetBcast);
    }
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
