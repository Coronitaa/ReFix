#include "refix_lan_transport.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

namespace refix::lan {

uint32_t ResolveLocalIpv4(std::string* outReason) {
    // 1. Explicit configuration via environment variable (absolute user priority)
    char envBuf[128] = {0};
    if (GetEnvironmentVariableA("REFIX_LAN_INTERFACE_IP", envBuf, sizeof(envBuf)) > 0 ||
        GetEnvironmentVariableA("REFIX_BIND_IP", envBuf, sizeof(envBuf)) > 0) {
        in_addr parsedAddr{};
        if (inet_pton(AF_INET, envBuf, &parsedAddr) == 1 && parsedAddr.s_addr != INADDR_NONE && parsedAddr.s_addr != 0) {
            uint32_t ip = ntohl(parsedAddr.s_addr);
            std::string reason = std::string("Explicitly configured via environment: ") + envBuf;
            if (outReason) *outReason = reason;
            printf("[LanTransport] Interface selected: %s (Reason: %s)\n", envBuf, reason.c_str());
            return ip;
        }
    }

    // 2. Windows IP Helper API (GetAdaptersAddresses) enumeration
    ULONG outBufLen = 15000;
    std::vector<uint8_t> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    DWORD ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    }

    struct Candidate {
        uint32_t ip = 0;
        int score = 0;
        std::string name;
        std::string desc;
        std::string reason;
    };
    std::vector<Candidate> candidates;

    if (ret == NO_ERROR && pAddresses != nullptr) {
        for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr != nullptr; curr = curr->Next) {
            if (curr->OperStatus != IfOperStatusUp) continue;
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

            std::string name;
            if (curr->FriendlyName) {
                int len = WideCharToMultiByte(CP_UTF8, 0, curr->FriendlyName, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    name.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, curr->FriendlyName, -1, &name[0], len, nullptr, nullptr);
                }
            }
            std::string desc;
            if (curr->Description) {
                int len = WideCharToMultiByte(CP_UTF8, 0, curr->Description, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    desc.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, curr->Description, -1, &desc[0], len, nullptr, nullptr);
                }
            }

            std::string nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });
            std::string descLower = desc;
            std::transform(descLower.begin(), descLower.end(), descLower.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });

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
                std::string reason;

                if (curr->IfType == IF_TYPE_ETHERNET_CSMACD && !isVirtual) {
                    score += 100;
                    reason = "Physical Ethernet";
                } else if (curr->IfType == IF_TYPE_IEEE80211 && !isVirtual) {
                    score += 90;
                    reason = "Physical Wi-Fi";
                } else if (!isVirtual) {
                    score += 50;
                    reason = "Physical Adapter";
                } else {
                    score -= 100;
                    reason = "Virtual Adapter";
                }

                if (hasGateway) {
                    score += 40;
                    reason += " (Gateway Present)";
                }
                if (isRfc1918) {
                    score += 30;
                }
                if (isApipa) {
                    score -= 50;
                    reason += " (APIPA Link-Local)";
                }
                if (isVBoxDefault) {
                    score -= 150;
                    reason += " (VirtualBox Subnet)";
                }

                Candidate cand;
                cand.ip = ip;
                cand.score = score;
                cand.name = name;
                cand.desc = desc;
                cand.reason = reason;
                candidates.push_back(cand);
            }
        }
    }

    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.ip != b.ip) return a.ip < b.ip;
            return a.name < b.name;
        });

        const auto& best = candidates.front();
        char ipStr[32];
        snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
                 (best.ip >> 24) & 0xFF, (best.ip >> 16) & 0xFF,
                 (best.ip >> 8) & 0xFF, best.ip & 0xFF);

        std::string finalReason = best.reason + " [" + best.name + " (" + best.desc + ")] (score=" + std::to_string(best.score) + ")";
        if (outReason) *outReason = finalReason;
        printf("[LanTransport] Interface selected: %s (Reason: %s)\n", ipStr, finalReason.c_str());
        return best.ip;
    }

    // 3. Fallback to gethostname / gethostbyname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* he = gethostbyname(hostname);
        if (he && he->h_addr_list) {
            uint32_t fallbackIp = 0;
            for (int i = 0; he->h_addr_list[i] != nullptr; ++i) {
                auto* in = reinterpret_cast<struct in_addr*>(he->h_addr_list[i]);
                uint32_t ip = ntohl(in->s_addr);
                if ((ip & 0xFF000000) == 0x0A000000 ||
                    (ip & 0xFFF00000) == 0xAC100000 ||
                    (ip & 0xFFFF0000) == 0xC0A80000) {
                    if ((ip & 0xFFFFFF00) == 0xC0A83800) {
                        if (fallbackIp == 0) fallbackIp = ip;
                    } else {
                        if (outReason) *outReason = "RFC1918 private IP from hostname lookup";
                        printf("[LanTransport] Interface selected: %u.%u.%u.%u (Reason: RFC1918 Hostname Fallback)\n",
                               (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
                        return ip;
                    }
                }
            }
            if (fallbackIp != 0) {
                if (outReason) *outReason = "Fallback private IP from hostname lookup";
                return fallbackIp;
            }
        }
    }

    if (outReason) *outReason = "Fallback to localhost loopback (127.0.0.1)";
    printf("[LanTransport] Interface selected: 127.0.0.1 (Reason: Fallback loopback)\n");
    return 0x7F000001; // 127.0.0.1 fallback
}

constexpr uint16_t REFIX_DEFAULT_DISCOVERY_PORT = 47584;
constexpr uint16_t REFIX_FALLBACK_DISCOVERY_PORTS[] = { 47584, 47585, 47586 };

LanTransport::LanTransport() = default;

LanTransport::~LanTransport() {
    Stop();
}

bool LanTransport::Start(uint16_t discoveryPort, ILanTransportListener* listener) {
    if (m_running.load()) return true;

    m_discoveryPort = discoveryPort;
    m_listener = listener;

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    m_localDataEndpoint.ipv4 = ResolveLocalIpv4();

    // 1. Setup Group Socket (Multicast + Broadcast Listener)
    m_groupSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_groupSocket == INVALID_SOCKET) {
        return false;
    }

    BOOL reuse = TRUE;
    setsockopt(m_groupSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    BOOL bcast = TRUE;
    setsockopt(m_groupSocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&bcast), sizeof(bcast));

    bool boundGroup = false;
    sockaddr_in groupSin{};
    groupSin.sin_family = AF_INET;
    groupSin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (discoveryPort != 0 && discoveryPort != REFIX_DEFAULT_DISCOVERY_PORT) {
        groupSin.sin_port = htons(discoveryPort);
        if (bind(m_groupSocket, reinterpret_cast<const sockaddr*>(&groupSin), sizeof(groupSin)) == 0) {
            boundGroup = true;
            m_discoveryPort = discoveryPort;
        }
    } else {
        for (uint16_t candPort : REFIX_FALLBACK_DISCOVERY_PORTS) {
            groupSin.sin_port = htons(candPort);
            if (bind(m_groupSocket, reinterpret_cast<const sockaddr*>(&groupSin), sizeof(groupSin)) == 0) {
                boundGroup = true;
                m_discoveryPort = candPort;
                break;
            }
        }
    }

    if (!boundGroup) {
        closesocket(m_groupSocket);
        m_groupSocket = INVALID_SOCKET;
        return false;
    }

    // Join IPv4 Multicast Group 239.255.71.84 on selected network interface
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.71.84");
    mreq.imr_interface.s_addr = htonl(m_localDataEndpoint.ipv4);

    m_multicastJoined = false;
    int mcastErr = 0;
    if (setsockopt(m_groupSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == 0) {
        m_multicastJoined = true;
        printf("[LanTransport] Multicast membership established on interface %s for group 239.255.71.84 (port %u)\n",
               m_localDataEndpoint.ToIpString().c_str(), m_discoveryPort);
    } else {
        mcastErr = WSAGetLastError();
        // Fallback attempt to INADDR_ANY
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(m_groupSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == 0) {
            m_multicastJoined = true;
            printf("[LanTransport] Multicast membership established on INADDR_ANY fallback (iface err %d) for group 239.255.71.84 (port %u)\n",
                   mcastErr, m_discoveryPort);
        } else {
            int fallbackErr = WSAGetLastError();
            m_multicastJoined = false;
            printf("[LanTransport] Multicast membership FAILED on interface %s (err %d) and INADDR_ANY (err %d). Continuing in degraded broadcast-only discovery mode on port %u\n",
                   m_localDataEndpoint.ToIpString().c_str(), mcastErr, fallbackErr, m_discoveryPort);
        }
    }

    u_long nonblock = 1;
    ioctlsocket(m_groupSocket, FIONBIO, &nonblock);

    // 2. Setup Data Socket (Unicast Sender/Receiver with Ephemeral Port)
    m_dataSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_dataSocket == INVALID_SOCKET) {
        closesocket(m_groupSocket);
        m_groupSocket = INVALID_SOCKET;
        return false;
    }

    setsockopt(m_dataSocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&bcast), sizeof(bcast));
    int rcvBufSize = 4 * 1024 * 1024;
    setsockopt(m_dataSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBufSize), sizeof(rcvBufSize));

    BOOL loop = TRUE;
    setsockopt(m_dataSocket, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop), sizeof(loop));

    BYTE ttl = 1;
    setsockopt(m_dataSocket, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));

    sockaddr_in dataSin{};
    dataSin.sin_family = AF_INET;
    dataSin.sin_addr.s_addr = htonl(INADDR_ANY);
    dataSin.sin_port = 0; // Request OS ephemeral port!

    if (bind(m_dataSocket, reinterpret_cast<const sockaddr*>(&dataSin), sizeof(dataSin)) == SOCKET_ERROR) {
        closesocket(m_groupSocket);
        closesocket(m_dataSocket);
        m_groupSocket = INVALID_SOCKET;
        m_dataSocket = INVALID_SOCKET;
        return false;
    }

    int sinLen = sizeof(dataSin);
    if (getsockname(m_dataSocket, reinterpret_cast<sockaddr*>(&dataSin), &sinLen) == 0) {
        m_localDataEndpoint.port = ntohs(dataSin.sin_port);
    }

    if (m_multicastJoined) {
        struct in_addr mcastIf{};
        mcastIf.s_addr = htonl(m_localDataEndpoint.ipv4);
        setsockopt(m_dataSocket, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&mcastIf), sizeof(mcastIf));
        setsockopt(m_groupSocket, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&mcastIf), sizeof(mcastIf));
    }

    ioctlsocket(m_dataSocket, FIONBIO, &nonblock);

    // 3. Launch Reactor Thread
    m_running.store(true);
    m_reactorThread = std::thread(&LanTransport::ReactorThreadLoop, this);

    return true;
}

void LanTransport::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    // Wake up reactor thread by sending a local ping datagram to ephemeral port
    if (m_dataSocket != INVALID_SOCKET && m_localDataEndpoint.port != 0) {
        sockaddr_in loopSin{};
        loopSin.sin_family = AF_INET;
        loopSin.sin_addr.s_addr = htonl(0x7F000001);
        loopSin.sin_port = htons(m_localDataEndpoint.port);
        char wake = 0;
        sendto(m_dataSocket, &wake, 1, 0, reinterpret_cast<const sockaddr*>(&loopSin), sizeof(loopSin));
    }

    if (m_reactorThread.joinable()) {
        m_reactorThread.join();
    }

    if (m_groupSocket != INVALID_SOCKET) {
        closesocket(m_groupSocket);
        m_groupSocket = INVALID_SOCKET;
    }

    if (m_dataSocket != INVALID_SOCKET) {
        closesocket(m_dataSocket);
        m_dataSocket = INVALID_SOCKET;
    }
}

void LanTransport::ReactorThreadLoop() {
    uint8_t rxBuf[65536];

    while (m_running.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        if (m_groupSocket != INVALID_SOCKET) FD_SET(m_groupSocket, &readSet);
        if (m_dataSocket != INVALID_SOCKET)  FD_SET(m_dataSocket, &readSet);

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 25000; // 25ms tick

        int sel = select(0, &readSet, nullptr, nullptr, &tv);
        if (sel < 0) {
            if (!m_running.load()) break;
            Sleep(10);
            continue;
        }
        if (sel > 0) {
            // Drain Group Socket (Discovery)
            if (m_groupSocket != INVALID_SOCKET && FD_ISSET(m_groupSocket, &readSet)) {
                sockaddr_in fromSin{};
                int fromLen = sizeof(fromSin);
                int r = recvfrom(m_groupSocket, reinterpret_cast<char*>(rxBuf), sizeof(rxBuf), 0,
                                 reinterpret_cast<sockaddr*>(&fromSin), &fromLen);
                while (r > 0) {
                    if (LanFirewall::Get().IsAllowedSockaddr(reinterpret_cast<const sockaddr*>(&fromSin))) {
                        LanEndpoint fromEp{ntohl(fromSin.sin_addr.s_addr), ntohs(fromSin.sin_port)};
                        ProcessInboundWirePacket(rxBuf, static_cast<size_t>(r), fromEp);
                    }
                    fromLen = sizeof(fromSin);
                    r = recvfrom(m_groupSocket, reinterpret_cast<char*>(rxBuf), sizeof(rxBuf), 0,
                                 reinterpret_cast<sockaddr*>(&fromSin), &fromLen);
                }
            }

            // Drain Data Socket (Unicast Data & ACKs)
            if (m_dataSocket != INVALID_SOCKET && FD_ISSET(m_dataSocket, &readSet)) {
                sockaddr_in fromSin{};
                int fromLen = sizeof(fromSin);
                int r = recvfrom(m_dataSocket, reinterpret_cast<char*>(rxBuf), sizeof(rxBuf), 0,
                                 reinterpret_cast<sockaddr*>(&fromSin), &fromLen);
                while (r > 0) {
                    if (LanFirewall::Get().IsAllowedSockaddr(reinterpret_cast<const sockaddr*>(&fromSin))) {
                        LanEndpoint fromEp{ntohl(fromSin.sin_addr.s_addr), ntohs(fromSin.sin_port)};
                        ProcessInboundWirePacket(rxBuf, static_cast<size_t>(r), fromEp);
                    }
                    fromLen = sizeof(fromSin);
                    r = recvfrom(m_dataSocket, reinterpret_cast<char*>(rxBuf), sizeof(rxBuf), 0,
                                 reinterpret_cast<sockaddr*>(&fromSin), &fromLen);
                }
            }
        }

        // Periodic Retransmissions & Timeouts
        CheckRetransmissionsAndTimeouts();
    }
}

void LanTransport::ProcessInboundWirePacket(const uint8_t* buf, size_t len, const LanEndpoint& fromEp) {
    if (len < sizeof(WireHeader)) return;

    const auto* hdr = reinterpret_cast<const WireHeader*>(buf);
    if (hdr->magic != REFIX_WIRE_MAGIC || hdr->version != REFIX_WIRE_VERSION) {
        return;
    }

    if (sizeof(WireHeader) + hdr->payloadLen > len) {
        return; // Truncated packet
    }

    PeerId senderPeer = hdr->GetSenderPeerId();
    // Ignore self-announcements
    if (m_localPeerId.IsValid() && senderPeer == m_localPeerId) {
        return;
    }

    const uint8_t* payload = buf + sizeof(WireHeader);
    size_t payloadLen = hdr->payloadLen;
    MsgType type = static_cast<MsgType>(hdr->msgType);

    // 1. Discovery & Lobby Messages
    if (type == MsgType::DiscoveryBeacon || type == MsgType::DiscoveryQuery || type == MsgType::DiscoveryResponse ||
        type == MsgType::LobbyAnnouncement || type == MsgType::LobbyQuery ||
        type == MsgType::LobbyJoinReq || type == MsgType::LobbyJoinResp || type == MsgType::LobbyLeaveReq) {
        if (m_listener) {
            m_listener->OnDiscoveryPacket(fromEp, type, payload, payloadLen);
        }
        return;
    }

    // 2. Data ACKs
    if (type == MsgType::DataAck) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto it = m_peerStates.find(senderPeer);
        if (it != m_peerStates.end()) {
            ProcessReliableAck(it->second, hdr->ack, hdr->sackMask);
        }
        return;
    }

    // 3. Reliable / Unreliable Data Packets
    if (type == MsgType::DataReliable || type == MsgType::DataUnreliable) {
        bool isReliable = (type == MsgType::DataReliable);

        if (isReliable) {
            std::vector<InboundPacket> packetsToNotify;
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                auto& state = m_peerStates[senderPeer];
                state.lastReceivedTime = std::chrono::steady_clock::now();
                state.timedOut = false;

                uint32_t seq = hdr->sequence;
                std::vector<InboundPacket> readyToProcess;

                if (seq == state.expectedSequenceIn) {
                    state.expectedSequenceIn++;

                    InboundPacket pkt;
                    pkt.senderPeerId = senderPeer;
                    pkt.senderEndpoint = fromEp;
                    pkt.channel = hdr->channel;
                    pkt.isReliable = true;
                    pkt.sequence = seq;
                    pkt.flags = hdr->flags;
                    pkt.sessionId = hdr->sessionId;
                    pkt.fragIndex = hdr->fragIndex;
                    pkt.fragTotal = hdr->fragTotal;
                    pkt.payload.assign(payload, payload + payloadLen);

                    readyToProcess.push_back(std::move(pkt));

                    // Drain contiguous buffered out-of-order packets
                    while (!state.outOfOrderInbound.empty() &&
                           state.outOfOrderInbound.begin()->first == state.expectedSequenceIn) {
                        readyToProcess.push_back(std::move(state.outOfOrderInbound.begin()->second));
                        state.outOfOrderInbound.erase(state.outOfOrderInbound.begin());
                        state.expectedSequenceIn++;
                    }
                } else if (seq > state.expectedSequenceIn) {
                    // Out of order packet, buffer it (capped at 64 packets to avoid unbounded memory growth)
                    if (state.outOfOrderInbound.size() < 64) {
                        InboundPacket pkt;
                        pkt.senderPeerId = senderPeer;
                        pkt.senderEndpoint = fromEp;
                        pkt.channel = hdr->channel;
                        pkt.isReliable = true;
                        pkt.sequence = seq;
                        pkt.flags = hdr->flags;
                        pkt.sessionId = hdr->sessionId;
                        pkt.fragIndex = hdr->fragIndex;
                        pkt.fragTotal = hdr->fragTotal;
                        pkt.payload.assign(payload, payload + payloadLen);
                        state.outOfOrderInbound[seq] = std::move(pkt);
                    }
                }

                // Build SACK bitmask
                uint32_t sackMask = 0;
                for (const auto& [bufferedSeq, _] : state.outOfOrderInbound) {
                    if (bufferedSeq > state.expectedSequenceIn) {
                        uint32_t diff = bufferedSeq - state.expectedSequenceIn - 1;
                        if (diff < 32) {
                            sackMask |= (1u << diff);
                        }
                    }
                }

                // Immediately send ACK back
                SendAckPacket(fromEp, hdr->channel, state.expectedSequenceIn - 1, sackMask);

                // Process all consecutive packets (assemble fragments if needed)
                for (auto& rawPkt : readyToProcess) {
                    if ((rawPkt.flags & FLAG_FRAGMENT) == 0) {
                        // Unfragmented complete message
                        {
                            std::lock_guard<std::mutex> qlock(m_inboundQueueMutex);
                            m_inboundQueue.push_back(rawPkt);
                        }
                        packetsToNotify.push_back(std::move(rawPkt));
                    } else {
                        // Fragmented message validation:
                        // 1. Validate fragTotal and fragIndex before allocating memory
                        if (rawPkt.fragTotal == 0 || rawPkt.fragIndex >= rawPkt.fragTotal) {
                            continue;
                        }
                        // 2. Reject payload exceeding protocol limit
                        if (rawPkt.payload.size() > REFIX_MAX_FRAGMENT_PAYLOAD) {
                            continue;
                        }
                        // 3. Reject if total size would exceed REFIX_MAX_MESSAGE_SIZE
                        if (static_cast<size_t>(rawPkt.fragTotal) * REFIX_MAX_FRAGMENT_PAYLOAD > REFIX_MAX_MESSAGE_SIZE + REFIX_MAX_FRAGMENT_PAYLOAD) {
                            continue;
                        }

                        auto now = std::chrono::steady_clock::now();
                        PruneExpiredFragmentsLocked(now);

                        auto fragKey = std::make_pair(rawPkt.senderPeerId, rawPkt.sessionId);
                        auto it = m_fragmentMap.find(fragKey);
                        if (it == m_fragmentMap.end()) {
                            // Check per-peer context count limit (16) and evict oldest if saturated
                            size_t peerContextCount = 0;
                            for (const auto& kv : m_fragmentMap) {
                                if (kv.first.first == rawPkt.senderPeerId) peerContextCount++;
                            }
                            if (peerContextCount >= REFIX_MAX_PEER_REASSEMBLY_CONTEXTS) {
                                EvictOldestReassemblyContextLocked(&rawPkt.senderPeerId);
                            }

                            // Check global context count limit (64) and evict oldest if saturated
                            if (m_fragmentMap.size() >= REFIX_MAX_REASSEMBLY_CONTEXTS) {
                                EvictOldestReassemblyContextLocked(nullptr);
                            }

                            // Check global and per-peer memory budgets
                            size_t chunkLen = rawPkt.payload.size();
                            auto pIt = m_peerReassemblyBytes.find(rawPkt.senderPeerId);
                            size_t currentPeerBytes = (pIt != m_peerReassemblyBytes.end()) ? pIt->second : 0;
                            if (m_globalReassemblyBytes + chunkLen > REFIX_MAX_GLOBAL_REASSEMBLY_MEM ||
                                currentPeerBytes + chunkLen > REFIX_MAX_PEER_REASSEMBLY_MEM) {
                                continue;
                            }

                            FragmentAssembler fa;
                            fa.totalFragments = rawPkt.fragTotal;
                            fa.channel = rawPkt.channel;
                            fa.isReliable = rawPkt.isReliable;
                            fa.sessionId = rawPkt.sessionId;
                            fa.senderPeerId = rawPkt.senderPeerId;
                            fa.allocatedBytes = 0;
                            fa.fragments.resize(rawPkt.fragTotal);
                            fa.received.resize(rawPkt.fragTotal, false);
                            fa.startTime = now;
                            fa.lastActivityTime = now;
                            it = m_fragmentMap.emplace(fragKey, std::move(fa)).first;
                        }

                        auto& fa = it->second;

                        // Verify metadata consistency across all fragments of session
                        if (rawPkt.fragTotal != fa.totalFragments ||
                            rawPkt.channel != fa.channel ||
                            rawPkt.isReliable != fa.isReliable) {
                            // Inconsistent metadata: drop the invalid fragment to prevent corrupting
                            // or aborting the ongoing legitimate reassembly session.
                            continue;
                        }

                        // Duplicate fragment check
                        if (fa.received[rawPkt.fragIndex]) {
                            fa.lastActivityTime = now;
                            continue;
                        }

                        // Check message reconstructed byte limit (256 KB)
                        size_t chunkLen = rawPkt.payload.size();
                        if (fa.allocatedBytes + chunkLen > REFIX_MAX_MESSAGE_SIZE) {
                            // Abort context exceeding message limit
                            ReleaseReassemblyContextLocked(it);
                            continue;
                        }

                        // Check global and per-peer memory budgets
                        auto pIt2 = m_peerReassemblyBytes.find(rawPkt.senderPeerId);
                        size_t currentPeerBytes2 = (pIt2 != m_peerReassemblyBytes.end()) ? pIt2->second : 0;
                        if (m_globalReassemblyBytes + chunkLen > REFIX_MAX_GLOBAL_REASSEMBLY_MEM ||
                            currentPeerBytes2 + chunkLen > REFIX_MAX_PEER_REASSEMBLY_MEM) {
                            continue;
                        }

                        fa.fragments[rawPkt.fragIndex] = std::move(rawPkt.payload);
                        fa.received[rawPkt.fragIndex] = true;
                        fa.allocatedBytes += chunkLen;
                        m_globalReassemblyBytes += chunkLen;
                        m_peerReassemblyBytes[rawPkt.senderPeerId] += chunkLen;
                        fa.lastActivityTime = now;

                        bool allReceived = true;
                        for (bool r : fa.received) {
                            if (!r) { allReceived = false; break; }
                        }

                        if (allReceived) {
                            InboundPacket fullMsg;
                            fullMsg.senderPeerId = rawPkt.senderPeerId;
                            fullMsg.senderEndpoint = rawPkt.senderEndpoint;
                            fullMsg.channel = fa.channel;
                            fullMsg.isReliable = fa.isReliable;
                            fullMsg.sequence = rawPkt.sequence;
                            fullMsg.payload.reserve(fa.allocatedBytes);
                            for (auto& chunk : fa.fragments) {
                                fullMsg.payload.insert(fullMsg.payload.end(), chunk.begin(), chunk.end());
                            }
                            ReleaseReassemblyContextLocked(it);

                            {
                                std::lock_guard<std::mutex> qlock(m_inboundQueueMutex);
                                m_inboundQueue.push_back(fullMsg);
                            }
                            packetsToNotify.push_back(std::move(fullMsg));
                        }
                    }
                }
            }

            if (m_listener) {
                for (const auto& p : packetsToNotify) {
                    m_listener->OnInboundData(p);
                }
            }
            return;
        }

        // Unreliable data packet
        if (payloadLen > REFIX_MAX_UNRELIABLE_PAYLOAD) {
            return;
        }
        InboundPacket pkt;
        pkt.senderPeerId = senderPeer;
        pkt.senderEndpoint = fromEp;
        pkt.channel = hdr->channel;
        pkt.isReliable = false;
        pkt.sequence = 0;
        pkt.flags = 0;
        pkt.sessionId = 0;
        pkt.fragIndex = 0;
        pkt.fragTotal = 1;
        pkt.payload.assign(payload, payload + payloadLen);

        {
            std::lock_guard<std::mutex> qlock(m_inboundQueueMutex);
            m_inboundQueue.push_back(pkt);
        }
        if (m_listener) m_listener->OnInboundData(pkt);
    }
}

void LanTransport::ProcessReliableAck(PeerReliabilityState& state, uint32_t ackSeq, uint32_t sackMask) {
    auto& unacked = state.unackedOutbound;
    auto now = std::chrono::steady_clock::now();

    // Remove all acknowledged sequences <= ackSeq
    auto it = std::remove_if(unacked.begin(), unacked.end(), [&](const OutboundReliable& r) {
        if (r.sequence <= ackSeq) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - r.sendTime).count();
            if (elapsed > 0) {
                state.lastMeasuredRttMs = static_cast<uint32_t>(elapsed);
            }
            return true;
        }
        // SACK mask check
        uint32_t diff = r.sequence - ackSeq - 1;
        if (diff < 32 && ((sackMask & (1u << diff)) != 0)) {
            return true;
        }
        return false;
    });
    unacked.erase(it, unacked.end());
}

void LanTransport::SendAckPacket(const LanEndpoint& target, uint8_t channel, uint32_t ackSeq, uint32_t sackMask) {
    if (!LanFirewall::Get().IsAllowedEndpoint(target)) return;

    WireHeader hdr{};
    hdr.magic = REFIX_WIRE_MAGIC;
    hdr.version = REFIX_WIRE_VERSION;
    hdr.msgType = static_cast<uint8_t>(MsgType::DataAck);
    hdr.flags = FLAG_HAS_ACK;
    hdr.SetSenderPeerId(m_localPeerId);
    hdr.channel = channel;
    hdr.ack = ackSeq;
    hdr.sackMask = sackMask;
    hdr.payloadLen = 0;

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);

    sendto(m_dataSocket, reinterpret_cast<const char*>(&hdr), sizeof(hdr), 0,
           reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
}

void LanTransport::CheckRetransmissionsAndTimeouts() {
    std::vector<std::pair<PeerId, LanEndpoint>> timedOutPeers;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto now = std::chrono::steady_clock::now();

        // Expire incomplete fragment assemblers using unified 10s sliding inactivity window
        PruneExpiredFragmentsLocked(now);

        for (auto& [peerId, state] : m_peerStates) {
            if (state.timedOut) continue;

            for (auto it = state.unackedOutbound.begin(); it != state.unackedOutbound.end();) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->sendTime).count();
                if (elapsed > 100) { // 100ms retransmission timer
                    it->retries++;
                    if (it->retries > 20) {
                        timedOutPeers.emplace_back(peerId, it->target);
                        state.unackedOutbound.clear();
                        state.timedOut = true;
                        break;
                    }
                    it->sendTime = now;
                    sockaddr_in sin{};
                    sin.sin_family = AF_INET;
                    sin.sin_addr.s_addr = htonl(it->target.ipv4);
                    sin.sin_port = htons(it->target.port);

                    sendto(m_dataSocket, reinterpret_cast<const char*>(it->packetBytes.data()),
                           static_cast<int>(it->packetBytes.size()), 0,
                           reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
                    ++it;
                } else {
                    ++it;
                }
            }
        }
    }

    if (m_listener) {
        for (const auto& [pId, target] : timedOutPeers) {
            m_listener->OnPeerTimeout(pId, target);
        }
    }
}

bool LanTransport::BroadcastDiscoveryPacket(MsgType type, const void* data, size_t len, uint16_t targetPort) {
    if (m_dataSocket == INVALID_SOCKET) return false;

    std::vector<uint8_t> buffer(sizeof(WireHeader) + len);
    auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(type);
    hdr->flags = 0;
    hdr->SetSenderPeerId(m_localPeerId);
    hdr->sessionId = 0;
    hdr->channel = 0;
    hdr->fragIndex = 0;
    hdr->fragTotal = 1;
    hdr->reserved = 0;
    hdr->sequence = 0;
    hdr->ack = 0;
    hdr->sackMask = 0;
    hdr->payloadLen = static_cast<uint16_t>(len);

    if (len > 0 && data) {
        std::memcpy(buffer.data() + sizeof(WireHeader), data, len);
    }

    auto sendToPort = [&](uint16_t p) {
        auto sendToTarget = [&](uint32_t ip) {
            sockaddr_in sin{};
            sin.sin_family = AF_INET;
            sin.sin_addr.s_addr = htonl(ip);
            sin.sin_port = htons(p);
            sendto(m_dataSocket, reinterpret_cast<const char*>(buffer.data()),
                   static_cast<int>(buffer.size()), 0,
                   reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
        };
        // 1. Multicast Group (239.255.71.84)
        sendToTarget(0xEFff4754);
        // 2. Limited Broadcast (255.255.255.255)
        sendToTarget(0xFFFFFFFF);
        // 3. Localhost (127.0.0.1)
        sendToTarget(0x7F000001);
    };

    if (targetPort != 0) {
        sendToPort(targetPort);
    } else {
        sendToPort(m_discoveryPort);
        for (uint16_t altPort : REFIX_FALLBACK_DISCOVERY_PORTS) {
            if (altPort != m_discoveryPort) {
                sendToPort(altPort);
            }
        }
    }

    return true;
}

bool LanTransport::BroadcastDiscovery(const void* data, size_t len, uint16_t targetPort) {
    return BroadcastDiscoveryPacket(MsgType::DiscoveryBeacon, data, len, targetPort);
}

bool LanTransport::BroadcastDiscoveryQuery(const void* data, size_t len, uint16_t targetPort) {
    return BroadcastDiscoveryPacket(MsgType::DiscoveryQuery, data, len, targetPort);
}

bool LanTransport::BroadcastLobbyAnnouncement(const void* data, size_t len, uint16_t targetPort) {
    return BroadcastDiscoveryPacket(MsgType::LobbyAnnouncement, data, len, targetPort);
}

bool LanTransport::BroadcastLobbyQuery(const void* data, size_t len, uint16_t targetPort) {
    return BroadcastDiscoveryPacket(MsgType::LobbyQuery, data, len, targetPort);
}

bool LanTransport::SendDiscoveryPacket(MsgType type, const LanEndpoint& target, const void* data, size_t len) {
    if (!LanFirewall::Get().IsAllowedEndpoint(target)) return false;

    std::vector<uint8_t> buffer(sizeof(WireHeader) + len);
    auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(type);
    hdr->flags = 0;
    hdr->SetSenderPeerId(m_localPeerId);
    hdr->channel = 0;
    hdr->payloadLen = static_cast<uint16_t>(len);

    if (len > 0 && data) {
        std::memcpy(buffer.data() + sizeof(WireHeader), data, len);
    }

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);

    int sent = sendto(m_dataSocket, reinterpret_cast<const char*>(buffer.data()),
                      static_cast<int>(buffer.size()), 0,
                      reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    return sent > 0;
}

bool LanTransport::SendDiscoveryResponse(const LanEndpoint& target, const void* data, size_t len) {
    if (!LanFirewall::Get().IsAllowedEndpoint(target)) return false;

    std::vector<uint8_t> buffer(sizeof(WireHeader) + len);
    auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DiscoveryResponse);
    hdr->flags = 0;
    hdr->SetSenderPeerId(m_localPeerId);
    hdr->channel = 0;
    hdr->payloadLen = static_cast<uint16_t>(len);

    if (len > 0 && data) {
        std::memcpy(buffer.data() + sizeof(WireHeader), data, len);
    }

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);

    int sent = sendto(m_dataSocket, reinterpret_cast<const char*>(buffer.data()),
                      static_cast<int>(buffer.size()), 0,
                      reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    return sent > 0;
}

bool LanTransport::SendUnreliable(const LanEndpoint& target, uint8_t channel, const void* data, size_t len) {
    if (!LanFirewall::Get().IsAllowedEndpoint(target)) {
        LanFirewall::Get().RecordBlockedEgress();
        return false;
    }

    // Contract: Unreliable messages are strictly single-datagram MTU-safe.
    // Payloads exceeding REFIX_MAX_UNRELIABLE_PAYLOAD are unequivocally rejected.
    if (len > REFIX_MAX_UNRELIABLE_PAYLOAD) {
        return false;
    }

    if (len > 0 && data == nullptr) {
        return false;
    }

    std::vector<uint8_t> buffer(sizeof(WireHeader) + len);
    auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataUnreliable);
    hdr->flags = 0;
    hdr->SetSenderPeerId(m_localPeerId);
    hdr->sessionId = 0;
    hdr->channel = channel;
    hdr->fragIndex = 0;
    hdr->fragTotal = 1;
    hdr->reserved = 0;
    hdr->sequence = 0;
    hdr->ack = 0;
    hdr->sackMask = 0;
    hdr->payloadLen = static_cast<uint16_t>(len);

    if (len > 0 && data) {
        std::memcpy(buffer.data() + sizeof(WireHeader), data, len);
    }

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);

    int sent = sendto(m_dataSocket, reinterpret_cast<const char*>(buffer.data()),
                      static_cast<int>(buffer.size()), 0,
                      reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    return sent > 0;
}

bool LanTransport::SendReliable(const PeerId& targetPeer, const LanEndpoint& target, uint8_t channel, const void* data, size_t len) {
    if (!targetPeer.IsValid()) {
        return false;
    }

    if (!LanFirewall::Get().IsAllowedEndpoint(target)) {
        LanFirewall::Get().RecordBlockedEgress();
        return false;
    }

    if (len > REFIX_MAX_MESSAGE_SIZE) {
        return false;
    }

    if (len > 0 && data == nullptr) {
        return false;
    }

    if (m_dataSocket == INVALID_SOCKET) {
        return false;
    }

    size_t totalFrags = (len == 0) ? 1 : ((len + REFIX_MAX_FRAGMENT_PAYLOAD - 1) / REFIX_MAX_FRAGMENT_PAYLOAD);
    if (totalFrags == 0 || totalFrags > 255) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_stateMutex);
    auto& state = m_peerStates[targetPeer];

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);

    if (len <= REFIX_MAX_FRAGMENT_PAYLOAD) {
        uint32_t seq = state.nextSequenceOut++;

        std::vector<uint8_t> buffer(sizeof(WireHeader) + len);
        auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
        hdr->magic = REFIX_WIRE_MAGIC;
        hdr->version = REFIX_WIRE_VERSION;
        hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
        hdr->flags = FLAG_RELIABLE;
        hdr->SetSenderPeerId(m_localPeerId);
        hdr->sessionId = 0;
        hdr->channel = channel;
        hdr->fragIndex = 0;
        hdr->fragTotal = 1;
        hdr->reserved = 0;
        hdr->sequence = seq;
        hdr->ack = state.expectedSequenceIn - 1;
        hdr->sackMask = 0;
        hdr->payloadLen = static_cast<uint16_t>(len);

        if (len > 0 && data) {
            std::memcpy(buffer.data() + sizeof(WireHeader), data, len);
        }

        OutboundReliable out;
        out.sequence = seq;
        out.channel = channel;
        out.target = target;
        out.targetPeer = targetPeer;
        out.packetBytes = buffer;
        out.sendTime = std::chrono::steady_clock::now();
        out.retries = 0;
        state.unackedOutbound.push_back(out);

        int sent = sendto(m_dataSocket, reinterpret_cast<const char*>(buffer.data()),
                          static_cast<int>(buffer.size()), 0,
                          reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
        if (sent == SOCKET_ERROR) {
            state.unackedOutbound.pop_back();
            state.nextSequenceOut--;
            return false;
        }
        return true;
    }

    // Application-level fragmentation for payloads > REFIX_MAX_FRAGMENT_PAYLOAD
    uint8_t fragTotal = static_cast<uint8_t>(totalFrags);
    uint32_t msgId = state.nextMessageIdOut++;

    const auto* srcPtr = reinterpret_cast<const uint8_t*>(data);
    size_t bytesRemaining = len;
    size_t offset = 0;
    uint32_t startSeq = state.nextSequenceOut;

    std::vector<OutboundReliable> preparedFrags;
    preparedFrags.reserve(fragTotal);

    for (uint8_t i = 0; i < fragTotal; ++i) {
        size_t chunkLen = (std::min)(static_cast<size_t>(REFIX_MAX_FRAGMENT_PAYLOAD), bytesRemaining);
        uint32_t seq = state.nextSequenceOut++;

        std::vector<uint8_t> buffer(sizeof(WireHeader) + chunkLen);
        auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
        hdr->magic = REFIX_WIRE_MAGIC;
        hdr->version = REFIX_WIRE_VERSION;
        hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
        hdr->flags = FLAG_RELIABLE | FLAG_FRAGMENT;
        if (i == fragTotal - 1) {
            hdr->flags |= FLAG_LAST_FRAGMENT;
        }
        hdr->SetSenderPeerId(m_localPeerId);
        hdr->sessionId = msgId;
        hdr->channel = channel;
        hdr->fragIndex = i;
        hdr->fragTotal = fragTotal;
        hdr->reserved = 0;
        hdr->sequence = seq;
        hdr->ack = state.expectedSequenceIn - 1;
        hdr->sackMask = 0;
        hdr->payloadLen = static_cast<uint16_t>(chunkLen);

        if (chunkLen > 0 && srcPtr) {
            std::memcpy(buffer.data() + sizeof(WireHeader), srcPtr + offset, chunkLen);
        }

        OutboundReliable out;
        out.sequence = seq;
        out.channel = channel;
        out.target = target;
        out.targetPeer = targetPeer;
        out.packetBytes = std::move(buffer);
        out.sendTime = std::chrono::steady_clock::now();
        out.retries = 0;
        preparedFrags.push_back(std::move(out));

        offset += chunkLen;
        bytesRemaining -= chunkLen;
    }

    // Attempt initial send of fragment 0. If socket error occurs immediately, rollback without leaking retransmissions.
    int sent0 = sendto(m_dataSocket, reinterpret_cast<const char*>(preparedFrags[0].packetBytes.data()),
                       static_cast<int>(preparedFrags[0].packetBytes.size()), 0,
                       reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    if (sent0 == SOCKET_ERROR) {
        state.nextSequenceOut = startSeq;
        state.nextMessageIdOut--;
        return false;
    }

    // Enqueue all fragments into ARQ unackedOutbound and transmit remaining fragments
    state.unackedOutbound.push_back(preparedFrags[0]);
    for (size_t i = 1; i < preparedFrags.size(); ++i) {
        state.unackedOutbound.push_back(preparedFrags[i]);
        sendto(m_dataSocket, reinterpret_cast<const char*>(preparedFrags[i].packetBytes.data()),
               static_cast<int>(preparedFrags[i].packetBytes.size()), 0,
               reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    }

    return true;
}

bool LanTransport::PollInbound(InboundPacket& outPacket) {
    std::lock_guard<std::mutex> lock(m_inboundQueueMutex);
    if (m_inboundQueue.empty()) return false;
    outPacket = std::move(m_inboundQueue.front());
    m_inboundQueue.pop_front();
    return true;
}

void LanTransport::ReleaseReassemblyMemoryLocked(const PeerId& peerId, size_t bytes) {
    if (m_globalReassemblyBytes >= bytes) {
        m_globalReassemblyBytes -= bytes;
    } else {
        m_globalReassemblyBytes = 0; // Defensively prevent underflow
    }

    auto it = m_peerReassemblyBytes.find(peerId);
    if (it != m_peerReassemblyBytes.end()) {
        if (it->second <= bytes) {
            m_peerReassemblyBytes.erase(it);
        } else {
            it->second -= bytes;
        }
    }
}

void LanTransport::ReleaseReassemblyContextLocked(std::map<std::pair<PeerId, uint32_t>, FragmentAssembler>::iterator it) {
    if (it == m_fragmentMap.end()) return;
    PeerId peerId = it->second.senderPeerId;
    size_t allocated = it->second.allocatedBytes;
    m_fragmentMap.erase(it);

    ReleaseReassemblyMemoryLocked(peerId, allocated);

    // If this peer has no remaining reassembly contexts, prune any 0-byte accounting entry
    bool hasRemainingContext = false;
    for (const auto& kv : m_fragmentMap) {
        if (kv.first.first == peerId) {
            hasRemainingContext = true;
            break;
        }
    }
    if (!hasRemainingContext) {
        m_peerReassemblyBytes.erase(peerId);
    }
}

void LanTransport::PruneExpiredFragmentsLocked(std::chrono::steady_clock::time_point now) {
    for (auto it = m_fragmentMap.begin(); it != m_fragmentMap.end(); ) {
        if (now - it->second.lastActivityTime > std::chrono::seconds(REFIX_REASSEMBLY_TIMEOUT_SEC)) {
            auto toErase = it++;
            ReleaseReassemblyContextLocked(toErase);
        } else {
            ++it;
        }
    }
}

void LanTransport::EvictOldestReassemblyContextLocked(const PeerId* preferredPeer) {
    auto oldestIt = m_fragmentMap.end();
    std::chrono::steady_clock::time_point oldestTime = (std::chrono::steady_clock::time_point::max)();

    for (auto it = m_fragmentMap.begin(); it != m_fragmentMap.end(); ++it) {
        if (preferredPeer && it->first.first != *preferredPeer) continue;
        if (it->second.lastActivityTime < oldestTime) {
            oldestTime = it->second.lastActivityTime;
            oldestIt = it;
        }
    }

    if (oldestIt != m_fragmentMap.end()) {
        ReleaseReassemblyContextLocked(oldestIt);
    }
}

size_t LanTransport::GetGlobalReassemblyBytes() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_globalReassemblyBytes;
}

size_t LanTransport::GetReassemblyContextCount() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_fragmentMap.size();
}

size_t LanTransport::GetPeerReassemblyBytes(const PeerId& peerId) const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    auto it = m_peerReassemblyBytes.find(peerId);
    return (it != m_peerReassemblyBytes.end()) ? it->second : 0;
}

size_t LanTransport::GetTrackedPeerReassemblyCount() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_peerReassemblyBytes.size();
}

std::string LanTransport::GetDiscoveryStatus() const {
    if (m_multicastJoined) {
        return "Multicast Active (239.255.71.84:" + std::to_string(m_discoveryPort) + ")";
    }
    return "Degraded Broadcast-Only (Port " + std::to_string(m_discoveryPort) + ")";
}

void LanTransport::ResetPeerState(const PeerId& peerId) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_peerStates.erase(peerId);

    for (auto it = m_fragmentMap.begin(); it != m_fragmentMap.end(); ) {
        if (it->first.first == peerId) {
            auto toErase = it++;
            ReleaseReassemblyContextLocked(toErase);
        } else {
            ++it;
        }
    }
    m_peerReassemblyBytes.erase(peerId);
}

} // namespace refix::lan
