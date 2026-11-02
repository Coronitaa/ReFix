#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

// Server side of Photon's UDP transport (an eNet derivative, see ExitGames.Client.Photon.EnetPeer).
//
// Datagram: [peerId:i16][flags:u8 (0 plain, 204 crc)][commandCount:u8][sentTime:i32][challenge:i32] commands...
// Command:  [type][channel][flags][reserved][size:i32][reliableSeq:i32] (+ type specific) payload
// All integers big-endian. One connection object per remote client socket.
class PnEnetConnection {
public:
    using SendFn = std::function<void(const std::string& datagram)>;
    // message = [0xF3][msgType]...; reliable/channel describe how the client sent it.
    using MessageFn = std::function<void(const std::string& message, bool reliable, uint8_t channel)>;

    PnEnetConnection(int16_t peerId, SendFn send, MessageFn onMessage);

    void OnDatagram(const uint8_t* data, size_t size, uint32_t nowMs);
    void SendPayload(const std::string& message, bool reliable, uint8_t channel);
    void Disconnect();
    // Flushes acks and queued commands, retransmits unacknowledged reliable commands.
    void Tick(uint32_t nowMs);

    bool IsConnected() const { return m_connected && !m_closed; }
    bool IsClosed() const { return m_closed; }
    uint32_t LastReceiveMs() const { return m_lastReceiveMs; }

private:
    struct Outgoing {
        std::string bytes;      // full serialized command
        uint8_t channel;
        int32_t sequence;
        uint32_t lastSentMs = 0;
        int sendCount = 0;
    };
    struct IncomingChannel {
        int32_t nextReliable = 1;                // next reliable sequence to deliver
        std::map<int32_t, std::string> pending;  // out-of-order reliable commands (type byte + payload)
        std::string fragmentBuffer;              // reassembly of the fragmented message in progress
        int32_t fragmentsLeft = 0;
    };

    // Unsequenced fragments (unreliable messages over the MTU) of one message, keyed by channel and start sequence.
    struct UnsequencedMessage {
        std::string buffer;
        std::set<int32_t> received;  // fragment numbers already copied
        int32_t count = 0;
        uint32_t startedMs = 0;
    };
    std::map<uint64_t, UnsequencedMessage> m_unsequencedFragments;
    std::set<uint64_t> m_completedUnsequenced;  // recent keys, so late duplicates are not delivered twice
    void DeliverUnsequencedFragment(uint8_t channel, const uint8_t* extra, const uint8_t* payload, size_t size);

    void HandleCommand(uint8_t type, uint8_t channel, uint8_t flags, int32_t reliableSeq, const uint8_t* extra,
                       const uint8_t* payload, size_t payloadSize, int32_t sentTime);
    void DeliverReliable(uint8_t channel, int32_t seq, uint8_t type, const uint8_t* extra, const uint8_t* payload, size_t size);
    void DeliverOrdered(IncomingChannel& ch, uint8_t channel, const std::string& item);
    void QueueAck(uint8_t channel, int32_t seq, int32_t sentTime, bool unsequenced);
    void QueueReliable(uint8_t type, uint8_t channel, const std::string& extra, const std::string& payload);
    void QueueUnsequenced(uint8_t channel, const std::string& payload);
    void SendCommands(const std::vector<std::string>& commands, uint32_t nowMs);

    int16_t m_peerId;
    SendFn m_send;
    MessageFn m_onMessage;
    bool m_connected = false;
    bool m_closed = false;
    int32_t m_challenge = 0;
    uint32_t m_lastReceiveMs = 0;
    uint32_t m_nowMs = 0;

    std::vector<std::string> m_acks;
    std::deque<std::string> m_unreliableQueue;
    std::vector<Outgoing> m_unacked;
    std::map<uint8_t, int32_t> m_outReliableSeq;
    int32_t m_unsequencedGroup = 0;
    std::map<uint8_t, IncomingChannel> m_inChannels;
};
