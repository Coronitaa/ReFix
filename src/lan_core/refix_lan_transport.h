#pragma once

#include "refix_lan_types.h"
#include "refix_lan_wire.h"
#include "refix_lan_firewall.h"

#include <memory>
#include <functional>
#include <vector>
#include <deque>
#include <unordered_map>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace refix::lan {

struct InboundPacket {
    PeerId senderPeerId;
    LanEndpoint senderEndpoint;
    uint8_t channel = 0;
    bool isReliable = false;
    uint32_t sequence = 0;
    uint16_t flags = 0;
    uint32_t sessionId = 0;
    uint8_t fragIndex = 0;
    uint8_t fragTotal = 1;
    std::vector<uint8_t> payload;
};

class ILanTransportListener {
public:
    virtual ~ILanTransportListener() = default;

    virtual void OnDiscoveryPacket(const LanEndpoint& sender, MsgType type, const uint8_t* data, size_t len) = 0;
    virtual void OnInboundData(const InboundPacket& packet) = 0;
    virtual void OnPeerTimeout(const PeerId& peerId, const LanEndpoint& endpoint) = 0;
};

class LanTransport {
public:
    LanTransport();
    ~LanTransport();

    // Lifecycle
    bool Start(uint16_t discoveryPort = 47584, ILanTransportListener* listener = nullptr);
    void Stop();
    bool IsRunning() const noexcept { return m_running.load(); }

    void SetListener(ILanTransportListener* listener) { m_listener = listener; }
    void SetLocalPeerId(const PeerId& id) { m_localPeerId = id; }
    PeerId GetLocalPeerId() const { return m_localPeerId; }

    LanEndpoint GetLocalDataEndpoint() const { return m_localDataEndpoint; }
    uint16_t GetDiscoveryPort() const { return m_discoveryPort; }

    // Transmission APIs
    bool BroadcastDiscovery(const void* data, size_t len, uint16_t targetPort = 0);
    bool BroadcastDiscoveryQuery(const void* data, size_t len, uint16_t targetPort = 0);
    bool BroadcastLobbyAnnouncement(const void* data, size_t len, uint16_t targetPort = 0);
    bool BroadcastLobbyQuery(const void* data, size_t len, uint16_t targetPort = 0);
    bool SendDiscoveryResponse(const LanEndpoint& target, const void* data, size_t len);
    bool SendDiscoveryPacket(MsgType type, const LanEndpoint& target, const void* data, size_t len);

    bool SendUnreliable(const LanEndpoint& target, uint8_t channel, const void* data, size_t len);
    bool SendReliable(const PeerId& targetPeer, const LanEndpoint& target, uint8_t channel, const void* data, size_t len);

    // Queue Polling (if caller wants to drain synchronously)
    bool PollInbound(InboundPacket& outPacket);

    // Peer Connection Management
    void ResetPeerState(const PeerId& peerId);

private:
    struct OutboundReliable {
        uint32_t sequence = 0;
        uint8_t channel = 0;
        LanEndpoint target;
        PeerId targetPeer;
        std::vector<uint8_t> packetBytes; // Formatted wire packet
        std::chrono::steady_clock::time_point sendTime;
        int retries = 0;
    };

    struct PeerReliabilityState {
        uint32_t nextSequenceOut = 1;
        uint32_t nextMessageIdOut = 1;
        uint32_t expectedSequenceIn = 1;
        bool timedOut = false;
        std::map<uint32_t, InboundPacket> outOfOrderInbound;
        std::vector<OutboundReliable> unackedOutbound;
        std::chrono::steady_clock::time_point lastReceivedTime;
        uint32_t lastMeasuredRttMs = 20;
    };

    struct FragmentAssembler {
        uint8_t totalFragments = 0;
        uint8_t channel = 0;
        bool isReliable = false;
        std::vector<std::vector<uint8_t>> fragments;
        std::vector<bool> received;
        std::chrono::steady_clock::time_point startTime;
    };

    void ReactorThreadLoop();
    void HandleGroupSocketRead();
    void HandleDataSocketRead();
    void ProcessInboundWirePacket(const uint8_t* buf, size_t len, const LanEndpoint& fromEp);
    void ProcessReliableAck(PeerReliabilityState& state, uint32_t ackSeq, uint32_t sackMask);
    void SendAckPacket(const LanEndpoint& target, uint8_t channel, uint32_t ackSeq, uint32_t sackMask);
    void CheckRetransmissionsAndTimeouts();
    bool BroadcastDiscoveryPacket(MsgType type, const void* data, size_t len, uint16_t targetPort = 0);

    std::atomic<bool> m_running{false};
    uint16_t m_discoveryPort = 47584;
    PeerId m_localPeerId;
    LanEndpoint m_localDataEndpoint;
    ILanTransportListener* m_listener = nullptr;

    SOCKET m_groupSocket = INVALID_SOCKET;
    SOCKET m_dataSocket = INVALID_SOCKET;
    std::thread m_reactorThread;

    std::mutex m_stateMutex;
    std::unordered_map<PeerId, PeerReliabilityState> m_peerStates;
    std::map<std::pair<PeerId, uint32_t>, FragmentAssembler> m_fragmentMap;

    std::mutex m_inboundQueueMutex;
    std::deque<InboundPacket> m_inboundQueue;
};

} // namespace refix::lan
