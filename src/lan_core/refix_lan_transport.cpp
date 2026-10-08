#include "refix_lan_transport.h"
#include <iostream>
#include <algorithm>

namespace refix::lan {

static uint32_t ResolveLocalIpv4() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* he = gethostbyname(hostname);
        if (he && he->h_addr_list) {
            for (int i = 0; he->h_addr_list[i] != nullptr; ++i) {
                auto* in = reinterpret_cast<struct in_addr*>(he->h_addr_list[i]);
                uint32_t ip = ntohl(in->s_addr);
                // Pick first valid RFC1918 / private address
                if ((ip & 0xFF000000) == 0x0A000000 ||
                    (ip & 0xFFF00000) == 0xAC100000 ||
                    (ip & 0xFFFF0000) == 0xC0A80000) {
                    return ip;
                }
            }
        }
    }
    return 0x7F000001; // 127.0.0.1 fallback
}

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

    // 1. Setup Group Socket (Multicast + Broadcast Listener)
    m_groupSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_groupSocket == INVALID_SOCKET) {
        return false;
    }

    BOOL reuse = TRUE;
    setsockopt(m_groupSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    BOOL bcast = TRUE;
    setsockopt(m_groupSocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&bcast), sizeof(bcast));

    sockaddr_in groupSin{};
    groupSin.sin_family = AF_INET;
    groupSin.sin_addr.s_addr = htonl(INADDR_ANY);
    groupSin.sin_port = htons(m_discoveryPort);

    if (bind(m_groupSocket, reinterpret_cast<const sockaddr*>(&groupSin), sizeof(groupSin)) == SOCKET_ERROR) {
        // If port 47584 is blocked, try binding to 0 for fallback
        groupSin.sin_port = 0;
        if (bind(m_groupSocket, reinterpret_cast<const sockaddr*>(&groupSin), sizeof(groupSin)) == 0) {
            int gSinLen = sizeof(groupSin);
            if (getsockname(m_groupSocket, reinterpret_cast<sockaddr*>(&groupSin), &gSinLen) == 0) {
                m_discoveryPort = ntohs(groupSin.sin_port);
            }
        }
    }

    // Join IPv4 Multicast Group 239.255.71.84
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.71.84");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(m_groupSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq));

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
    m_localDataEndpoint.ipv4 = ResolveLocalIpv4();

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
        type == MsgType::LobbyAnnouncement || type == MsgType::LobbyQuery) {
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

                uint32_t seq = hdr->sequence;

                if (seq == state.expectedSequenceIn) {
                    // In-order packet arrived!
                    state.expectedSequenceIn++;

                    InboundPacket pkt;
                    pkt.senderPeerId = senderPeer;
                    pkt.senderEndpoint = fromEp;
                    pkt.channel = hdr->channel;
                    pkt.isReliable = true;
                    pkt.sequence = seq;
                    pkt.payload.assign(payload, payload + payloadLen);

                    {
                        std::lock_guard<std::mutex> qlock(m_inboundQueueMutex);
                        m_inboundQueue.push_back(pkt);
                    }
                    packetsToNotify.push_back(pkt);

                    // Drain any contiguous buffered packets
                    while (!state.outOfOrderInbound.empty() &&
                           state.outOfOrderInbound.begin()->first == state.expectedSequenceIn) {
                        InboundPacket ooo = std::move(state.outOfOrderInbound.begin()->second);
                        state.outOfOrderInbound.erase(state.outOfOrderInbound.begin());
                        state.expectedSequenceIn++;

                        {
                            std::lock_guard<std::mutex> qlock(m_inboundQueueMutex);
                            m_inboundQueue.push_back(ooo);
                        }
                        packetsToNotify.push_back(ooo);
                    }
                } else if (seq > state.expectedSequenceIn) {
                    // Out of order packet, buffer it
                    InboundPacket pkt;
                    pkt.senderPeerId = senderPeer;
                    pkt.senderEndpoint = fromEp;
                    pkt.channel = hdr->channel;
                    pkt.isReliable = true;
                    pkt.sequence = seq;
                    pkt.payload.assign(payload, payload + payloadLen);
                    state.outOfOrderInbound[seq] = std::move(pkt);
                }
                // If seq < state.expectedSequenceIn, it's a duplicate. We simply re-ACK it below.

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
            }

            if (m_listener) {
                for (const auto& p : packetsToNotify) {
                    m_listener->OnInboundData(p);
                }
            }
            return;
        }

        // Unreliable data packet
        InboundPacket pkt;
        pkt.senderPeerId = senderPeer;
        pkt.senderEndpoint = fromEp;
        pkt.channel = hdr->channel;
        pkt.isReliable = false;
        pkt.sequence = 0;
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

        for (auto& [peerId, state] : m_peerStates) {
            for (auto it = state.unackedOutbound.begin(); it != state.unackedOutbound.end();) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->sendTime).count();
                if (elapsed > 100) { // 100ms retransmission timer
                    it->retries++;
                    if (it->retries > 20) {
                        timedOutPeers.emplace_back(peerId, it->target);
                        it = state.unackedOutbound.erase(it);
                        continue;
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

    uint16_t port = targetPort != 0 ? targetPort : m_discoveryPort;

    std::vector<uint8_t> buffer(sizeof(WireHeader) + len);
    auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(type);
    hdr->flags = 0;
    hdr->SetSenderPeerId(m_localPeerId);
    hdr->sessionId = 0;
    hdr->channel = 0;
    hdr->payloadLen = static_cast<uint16_t>(len);

    if (len > 0 && data) {
        std::memcpy(buffer.data() + sizeof(WireHeader), data, len);
    }

    auto sendToTarget = [&](uint32_t ip) {
        sockaddr_in sin{};
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(ip);
        sin.sin_port = htons(port);
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

    std::vector<uint8_t> buffer(sizeof(WireHeader) + len);
    auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataUnreliable);
    hdr->flags = 0;
    hdr->SetSenderPeerId(m_localPeerId);
    hdr->channel = channel;
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
    if (!LanFirewall::Get().IsAllowedEndpoint(target)) {
        LanFirewall::Get().RecordBlockedEgress();
        return false;
    }

    std::lock_guard<std::mutex> lock(m_stateMutex);
    auto& state = m_peerStates[targetPeer];

    uint32_t seq = state.nextSequenceOut++;

    std::vector<uint8_t> buffer(sizeof(WireHeader) + len);
    auto* hdr = reinterpret_cast<WireHeader*>(buffer.data());
    hdr->magic = REFIX_WIRE_MAGIC;
    hdr->version = REFIX_WIRE_VERSION;
    hdr->msgType = static_cast<uint8_t>(MsgType::DataReliable);
    hdr->flags = FLAG_RELIABLE;
    hdr->SetSenderPeerId(m_localPeerId);
    hdr->channel = channel;
    hdr->sequence = seq;
    hdr->ack = state.expectedSequenceIn - 1;
    hdr->payloadLen = static_cast<uint16_t>(len);

    if (len > 0 && data) {
        std::memcpy(buffer.data() + sizeof(WireHeader), data, len);
    }

    // Record in unacked queue
    OutboundReliable out;
    out.sequence = seq;
    out.channel = channel;
    out.target = target;
    out.targetPeer = targetPeer;
    out.packetBytes = buffer;
    out.sendTime = std::chrono::steady_clock::now();
    out.retries = 0;
    state.unackedOutbound.push_back(out);

    // Initial send
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(target.ipv4);
    sin.sin_port = htons(target.port);

    sendto(m_dataSocket, reinterpret_cast<const char*>(buffer.data()),
           static_cast<int>(buffer.size()), 0,
           reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));

    return true;
}

bool LanTransport::PollInbound(InboundPacket& outPacket) {
    std::lock_guard<std::mutex> lock(m_inboundQueueMutex);
    if (m_inboundQueue.empty()) return false;
    outPacket = std::move(m_inboundQueue.front());
    m_inboundQueue.pop_front();
    return true;
}

void LanTransport::ResetPeerState(const PeerId& peerId) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_peerStates.erase(peerId);
}

} // namespace refix::lan
