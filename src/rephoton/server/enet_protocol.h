#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace ReFix::Photon::Server {

    // Photon / ENet Command Types
    namespace ENetCommandType {
        constexpr uint8_t Acknowledge    = 1; // EG_CMD_ACK
        constexpr uint8_t Connect        = 2; // EG_CMD_CONNECT
        constexpr uint8_t VerifyConnect  = 3; // EG_CMD_VERIFY_CONNECT
        constexpr uint8_t Disconnect     = 4; // EG_CMD_DISCONNECT
        constexpr uint8_t Ping           = 5; // EG_CMD_PING
        constexpr uint8_t SendReliable   = 6; // EG_CMD_SEND_RELIABLE
        constexpr uint8_t SendUnreliable = 7; // EG_CMD_SEND_UNRELIABLE
        constexpr uint8_t SendFragment   = 8; // EG_CMD_SEND_FRAGMENT
    }

    #pragma pack(push, 1)
    struct ENetDatagramHeader {
        uint16_t peerId;
        uint8_t flags;
        uint8_t commandCount;
        uint32_t timestamp;
        uint32_t challenge;
    };

    struct ENetCommandHeader {
        uint8_t commandType;
        uint8_t channelId;
        uint8_t commandFlags;
        uint8_t reserved;
        uint32_t commandLength;
        uint32_t reliableSequenceNumber;
    };

    struct ENetConnectPayload {
        uint16_t peerId;
        uint8_t flags;
        uint8_t reserved;
        uint32_t mtu;
        uint32_t windowSize;
        uint32_t channelCount;
        uint32_t incomingBandwidth;
        uint32_t outgoingBandwidth;
        uint32_t packetThrottleInterval;
        uint32_t packetThrottleAcceleration;
        uint32_t packetThrottleDeceleration;
        uint32_t protocolVersion;
    };

    struct ENetAckPayload {
        uint32_t acknowledgedReliableSequenceNumber;
        uint32_t acknowledgedSentTime;
    };
    #pragma pack(pop)

    inline uint16_t ReadBE16(const uint8_t* p) {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }

    inline uint32_t ReadBE32(const uint8_t* p) {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) |
               static_cast<uint32_t>(p[3]);
    }

    inline void WriteBE16(uint8_t* p, uint16_t val) {
        p[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
        p[1] = static_cast<uint8_t>(val & 0xFF);
    }

    inline void WriteBE32(uint8_t* p, uint32_t val) {
        p[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
        p[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
        p[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
        p[3] = static_cast<uint8_t>(val & 0xFF);
    }

} // namespace ReFix::Photon::Server
