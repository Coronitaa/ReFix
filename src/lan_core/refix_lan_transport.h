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

// Deterministic LAN network interface selection
uint32_t ResolveLocalIpv4(std::string* outReason = nullptr);

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
    void SetPeerAdmissionFilter(std::function<bool(const PeerId&)> filter) { m_admissionFilter = std::move(filter); }

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

    /**
     * Send reliable message.
     *
     * Contract:
     * - Returns true if the message was successfully formatted, initially transmitted over the socket,
     *   and enqueued in the local ARQ tracking buffer (unackedOutbound).
     * - Does NOT guarantee immediate delivery to the remote application. Delivery is guaranteed via
     *   ARQ retransmissions upon receiving remote DataAck or until retry exhaustion triggers OnPeerTimeout().
     * - Link ACK (DataAck) acknowledges datagram receipt by the peer transport layer.
     * - API Consumer Acceptance: Messages are delivered via ILanTransportListener::OnInboundData, or
     *   buffered in m_inboundQueue (capped at 512). If the inbound queue is full in polling mode,
     *   backpressure is asserted (datagram is NOT acknowledged), forcing sender ARQ retransmissions
     *   until queue capacity is freed or connection times out.
     */
    bool SendReliable(const PeerId& targetPeer, const LanEndpoint& target, uint8_t channel, const void* data, size_t len);

    // Queue Polling (if caller wants to drain synchronously)
    bool PollInbound(InboundPacket& outPacket);

    // Peer Connection Management
    void ResetPeerState(const PeerId& peerId);

    // Diagnostics & Test Inspection
    size_t GetGlobalReassemblyBytes() const;
    size_t GetReassemblyContextCount() const;
    size_t GetPeerReassemblyBytes(const PeerId& peerId) const;
    size_t GetTrackedPeerReassemblyCount() const;
    size_t GetPeerStateCount() const;
    size_t GetInboundQueueSize() const;
    uint64_t GetInboundQueueDroppedCount() const;
    bool IsMulticastJoined() const { return m_multicastJoined; }
    std::string GetDiscoveryStatus() const;
    void SetSimulateMulticastFailure(bool fail) { m_simulateMulticastFailure = fail; }

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
        bool isRegisteredPeer = false;
        std::map<uint32_t, InboundPacket> outOfOrderInbound;
        std::vector<OutboundReliable> unackedOutbound;
        LanEndpoint lastEndpoint;
        std::chrono::steady_clock::time_point lastReceivedTime; // Maintained for backwards compatibility
        std::chrono::steady_clock::time_point lastDataRecvTime;
        std::chrono::steady_clock::time_point lastAckRecvTime;
        std::chrono::steady_clock::time_point lastSendTime;
        std::chrono::steady_clock::time_point lastActivityTime;
        uint32_t lastMeasuredRttMs = 20;
    };

    struct FragmentAssembler {
        uint8_t totalFragments = 0;
        uint8_t channel = 0;
        bool isReliable = false;
        uint32_t sessionId = 0;
        PeerId senderPeerId;
        size_t allocatedBytes = 0;
        std::vector<std::vector<uint8_t>> fragments;
        std::vector<bool> received;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastActivityTime;
    };

    void ReactorThreadLoop();
    void HandleGroupSocketRead();
    void HandleDataSocketRead();
    void ProcessInboundWirePacket(const uint8_t* buf, size_t len, const LanEndpoint& fromEp);
    void ProcessReliableAck(PeerReliabilityState& state, uint32_t ackSeq, uint32_t sackMask);
    void SendAckPacket(const LanEndpoint& target, uint8_t channel, uint32_t ackSeq, uint32_t sackMask);
    void CheckRetransmissionsAndTimeouts();
    bool BroadcastDiscoveryPacket(MsgType type, const void* data, size_t len, uint16_t targetPort = 0);
    void PruneExpiredFragmentsLocked(std::chrono::steady_clock::time_point now);
    void PruneInactivePeerStatesLocked(std::chrono::steady_clock::time_point now);
    std::unordered_map<PeerId, PeerReliabilityState>::iterator TryAdmitPeerStateLocked(
        const PeerId& peerId, bool isLocalSend, std::chrono::steady_clock::time_point now);
    void EvictOldestReassemblyContextLocked(const PeerId* preferredPeer = nullptr);
    void ReleaseReassemblyContextLocked(std::map<std::pair<PeerId, uint32_t>, FragmentAssembler>::iterator it);
    void ReleaseReassemblyMemoryLocked(const PeerId& peerId, size_t bytes);

    std::atomic<bool> m_running{false};
    bool m_multicastJoined = false;
    bool m_simulateMulticastFailure = false;
    uint16_t m_discoveryPort = 47584;
    PeerId m_localPeerId;
    LanEndpoint m_localDataEndpoint;
    ILanTransportListener* m_listener = nullptr;
    std::function<bool(const PeerId&)> m_admissionFilter;

    SOCKET m_groupSocket = INVALID_SOCKET;
    SOCKET m_dataSocket = INVALID_SOCKET;
    std::thread m_reactorThread;

    mutable std::mutex m_stateMutex;
    std::unordered_map<PeerId, PeerReliabilityState> m_peerStates;
    std::map<std::pair<PeerId, uint32_t>, FragmentAssembler> m_fragmentMap;
    size_t m_globalReassemblyBytes = 0;
    std::unordered_map<PeerId, size_t> m_peerReassemblyBytes;

    mutable std::mutex m_inboundQueueMutex;
    std::deque<InboundPacket> m_inboundQueue;
    std::atomic<uint64_t> m_inboundQueueDroppedPackets{0};
};

} // namespace refix::lan
