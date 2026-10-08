#pragma once

#include "refix_lan_types.h"
#include <vector>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace refix::lan {

class LanFirewall {
public:
    static LanFirewall& Get();

    // Core validation: host byte order IPv4
    bool IsAllowedIpv4(uint32_t ipHostOrder) const noexcept;

    // Endpoint validation
    bool IsAllowedEndpoint(const LanEndpoint& ep) const noexcept;

#ifdef _WIN32
    // Winsock sockaddr validation
    bool IsAllowedSockaddr(const struct sockaddr* sa) const noexcept;
#endif

    // Management of custom allowed LAN / VPN subnets
    void AddAllowedSubnet(uint32_t subnetHostOrder, uint32_t maskHostOrder);
    void AddAllowedIp(uint32_t ipHostOrder);
    void ParseAndAddSubnets(std::string_view commaSeparatedList);
    void ClearCustomSubnets();

    // Telemetry & metrics
    uint64_t GetBlockedEgressCount() const noexcept { return m_blockedEgressCount.load(); }
    void RecordBlockedEgress() noexcept { m_blockedEgressCount.fetch_add(1, std::memory_order_relaxed); }

private:
    LanFirewall();
    ~LanFirewall() = default;

    struct SubnetEntry {
        uint32_t subnet;
        uint32_t mask;
    };

    mutable std::mutex m_mutex;
    std::vector<SubnetEntry> m_customSubnets;
    std::atomic<uint64_t> m_blockedEgressCount{0};
};

} // namespace refix::lan
