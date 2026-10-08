#include "refix_lan_firewall.h"
#include <cstdio>
#include <cstring>

namespace refix::lan {

LanFirewall& LanFirewall::Get() {
    static LanFirewall s_instance;
    return s_instance;
}

LanFirewall::LanFirewall() {
    // Default construction
}

bool LanFirewall::IsAllowedIpv4(uint32_t ip) const noexcept {
    // 1. Loopback (127.0.0.0/8)
    if ((ip & 0xFF000000) == 0x7F000000) {
        return true;
    }

    // 2. Limited broadcast (255.255.255.255)
    if (ip == 0xFFFFFFFF) {
        return true;
    }

    // 3. RFC 1918 Private Ranges
    // 10.0.0.0/8
    if ((ip & 0xFF000000) == 0x0A000000) {
        return true;
    }
    // 172.16.0.0/12 (172.16.0.0 - 172.31.255.255)
    if ((ip & 0xFFF00000) == 0xAC100000) {
        return true;
    }
    // 192.168.0.0/16
    if ((ip & 0xFFFF0000) == 0xC0A80000) {
        return true;
    }

    // 4. Link-Local / APIPA (169.254.0.0/16)
    if ((ip & 0xFFFF0000) == 0xA9FE0000) {
        return true;
    }

    // 5. Multicast (224.0.0.0/4: 0xE0000000 - 0xEFFFFFFF)
    // Strictly restrict to Link-Local Control (224.0.0.0/24) and Administratively Scoped (239.0.0.0/8).
    // Globally routable multicast (224.0.1.0 - 238.255.255.255) is BLOCKED.
    if ((ip & 0xFFFFFF00) == 0xE0000000) {
        return true; // 224.0.0.0/24 Link-Local (TTL=1)
    }
    if ((ip & 0xFF000000) == 0xEF000000) {
        return true; // 239.0.0.0/8 Administratively Scoped (RFC 2365)
    }

    // 6. Explicitly configured custom subnets (e.g. VLAN / VPN subnets)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : m_customSubnets) {
            if ((ip & entry.mask) == (entry.subnet & entry.mask)) {
                return true;
            }
        }
    }

    // All other destinations (Internet WAN, Valve SDR 162.254.x.x, 8.8.8.8, 1.1.1.1, etc.) BLOCKED
    return false;
}

bool LanFirewall::IsAllowedEndpoint(const LanEndpoint& ep) const noexcept {
    return IsAllowedIpv4(ep.ipv4);
}

#ifdef _WIN32
bool LanFirewall::IsAllowedSockaddr(const struct sockaddr* sa) const noexcept {
    if (!sa) return false;

    if (sa->sa_family == AF_INET) {
        const auto* sin = reinterpret_cast<const sockaddr_in*>(sa);
        uint32_t ip = ntohl(sin->sin_addr.s_addr);
        return IsAllowedIpv4(ip);
    }

    // IPv6: For strict LAN mode, reject unmapped public IPv6
    if (sa->sa_family == AF_INET6) {
        const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(sa);
        const uint8_t* b = sin6->sin6_addr.s6_addr;

        // Loopback ::1
        bool isLoopback = true;
        for (int i = 0; i < 15; ++i) {
            if (b[i] != 0) { isLoopback = false; break; }
        }
        if (isLoopback && b[15] == 1) return true;

        // Link-local fe80::/10
        if (b[0] == 0xfe && (b[1] & 0xc0) == 0x80) return true;

        // Unique local fc00::/7 (fc00:: - fdff::)
        if ((b[0] & 0xfe) == 0xfc) return true;

        // Node-local or link-local multicast ff01:: / ff02::
        if (b[0] == 0xff && (b[1] == 0x01 || b[1] == 0x02)) return true;

        // IPv4-mapped IPv6 (::ffff:x.x.x.x)
        bool isMapped = true;
        for (int i = 0; i < 10; ++i) {
            if (b[i] != 0) { isMapped = false; break; }
        }
        if (isMapped && b[10] == 0xff && b[11] == 0xff) {
            uint32_t ip4 = (static_cast<uint32_t>(b[12]) << 24) |
                           (static_cast<uint32_t>(b[13]) << 16) |
                           (static_cast<uint32_t>(b[14]) << 8)  |
                           (static_cast<uint32_t>(b[15]));
            return IsAllowedIpv4(ip4);
        }

        // All other IPv6 destinations blocked
        return false;
    }

    return false;
}
#endif

void LanFirewall::AddAllowedSubnet(uint32_t subnet, uint32_t mask) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_customSubnets.push_back({subnet, mask});
}

void LanFirewall::AddAllowedIp(uint32_t ip) {
    AddAllowedSubnet(ip, 0xFFFFFFFF);
}

void LanFirewall::ParseAndAddSubnets(std::string_view list) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t start = 0;
    while (start < list.size()) {
        size_t end = list.find(',', start);
        if (end == std::string_view::npos) end = list.size();

        std::string token(list.substr(start, end - start));
        // Trim whitespace
        while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) token.erase(token.begin());
        while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) token.pop_back();

        if (!token.empty()) {
            unsigned int a, b, c, d, prefix = 32;
            size_t slash = token.find('/');
            if (slash != std::string::npos) {
                prefix = std::stoul(token.substr(slash + 1));
                token = token.substr(0, slash);
            }
            if (sscanf(token.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
                uint32_t ip = ((a & 0xFF) << 24) | ((b & 0xFF) << 16) | ((c & 0xFF) << 8) | (d & 0xFF);
                uint32_t mask = (prefix == 0) ? 0 : (prefix >= 32 ? 0xFFFFFFFF : (~0u << (32 - prefix)));
                m_customSubnets.push_back({ip, mask});
            }
        }
        start = end + 1;
    }
}

void LanFirewall::ClearCustomSubnets() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_customSubnets.clear();
}

} // namespace refix::lan
