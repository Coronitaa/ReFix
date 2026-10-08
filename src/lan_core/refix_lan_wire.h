#pragma once

#include "refix_lan_types.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <string_view>

namespace refix::lan {

constexpr uint32_t REFIX_WIRE_MAGIC = 0x52464958; // 'RFIX' in little endian
constexpr uint8_t  REFIX_WIRE_VERSION = 1;
constexpr uint16_t REFIX_MAX_FRAGMENT_PAYLOAD = 1150; // MTU-safe chunk size

// Message Types
enum class MsgType : uint8_t {
    DiscoveryBeacon   = 0x01,
    DiscoveryQuery    = 0x02,
    DiscoveryResponse = 0x03,
    ConnectReq        = 0x04,
    ConnectAck        = 0x05,
    Disconnect        = 0x06,
    DataUnreliable    = 0x07,
    DataReliable      = 0x08,
    DataAck           = 0x09,
    Ping              = 0x0A,
    Pong              = 0x0B,
    RelayWrapper      = 0x0C,
    LobbyAnnouncement = 0x0D,
    LobbyQuery        = 0x0E
};

// Flags
constexpr uint16_t FLAG_RELIABLE      = 0x0001;
constexpr uint16_t FLAG_HAS_ACK       = 0x0002;
constexpr uint16_t FLAG_FRAGMENT      = 0x0004;
constexpr uint16_t FLAG_LAST_FRAGMENT = 0x0008;

#pragma pack(push, 1)
struct WireHeader {
    uint32_t magic;          // 0x52464958
    uint8_t  version;        // 1
    uint8_t  msgType;        // MsgType enum
    uint16_t flags;          // FLAG_*
    uint64_t senderPeerHigh;
    uint64_t senderPeerLow;
    uint32_t sessionId;      // Session or conversation ID
    uint8_t  channel;        // 0: Signaling, 1: Game, 2: Voice, etc.
    uint8_t  fragIndex;      // 0-indexed fragment
    uint8_t  fragTotal;      // Total fragments (1 if unfragmented)
    uint8_t  reserved;       // 0
    uint32_t sequence;       // Sequence number for reliable data
    uint32_t ack;            // Cumulative acknowledged sequence
    uint32_t sackMask;       // 32-bit Selective ACK bitmask
    uint16_t payloadLen;     // Payload byte length following this header

    PeerId GetSenderPeerId() const noexcept {
        PeerId id;
        id.high = senderPeerHigh;
        id.low = senderPeerLow;
        return id;
    }

    void SetSenderPeerId(const PeerId& id) noexcept {
        senderPeerHigh = id.high;
        senderPeerLow = id.low;
    }
};
#pragma pack(pop)

static_assert(sizeof(WireHeader) == 46, "WireHeader must be exactly 46 bytes packed");

// =============================================================================
// Binary Serialization Helpers (Explicit Little-Endian)
// =============================================================================
class ByteWriter {
public:
    ByteWriter() = default;
    explicit ByteWriter(size_t reserveBytes) { m_buffer.reserve(reserveBytes); }

    const std::vector<uint8_t>& Buffer() const noexcept { return m_buffer; }
    const uint8_t* Data() const noexcept { return m_buffer.data(); }
    size_t Size() const noexcept { return m_buffer.size(); }

    void WriteU8(uint8_t v) { m_buffer.push_back(v); }

    void WriteU16(uint16_t v) {
        m_buffer.push_back(static_cast<uint8_t>(v & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    void WriteU32(uint32_t v) {
        m_buffer.push_back(static_cast<uint8_t>(v & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    void WriteU64(uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            m_buffer.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }

    void WriteI64(int64_t v) {
        WriteU64(static_cast<uint64_t>(v));
    }

    void WriteDouble(double v) {
        uint64_t u;
        std::memcpy(&u, &v, sizeof(u));
        WriteU64(u);
    }

    void WriteBool(bool b) {
        WriteU8(b ? 1 : 0);
    }

    void WriteString(std::string_view s) {
        WriteU32(static_cast<uint32_t>(s.size()));
        if (!s.empty()) {
            const auto* ptr = reinterpret_cast<const uint8_t*>(s.data());
            m_buffer.insert(m_buffer.end(), ptr, ptr + s.size());
        }
    }

    void WriteBytes(const void* data, size_t len) {
        WriteU32(static_cast<uint32_t>(len));
        if (len > 0 && data) {
            const auto* ptr = reinterpret_cast<const uint8_t*>(data);
            m_buffer.insert(m_buffer.end(), ptr, ptr + len);
        }
    }

    void WritePeerId(const PeerId& id) {
        WriteU64(id.high);
        WriteU64(id.low);
    }

    void WriteEndpoint(const LanEndpoint& ep) {
        WriteU32(ep.ipv4);
        WriteU16(ep.port);
    }

    void WriteAttribute(const AttributeValue& attr) {
        WriteU8(static_cast<uint8_t>(attr.type));
        switch (attr.type) {
            case AttributeType::String:  WriteString(attr.asString); break;
            case AttributeType::Int64:   WriteI64(attr.asInt64); break;
            case AttributeType::Double:  WriteDouble(attr.asDouble); break;
            case AttributeType::Boolean: WriteBool(attr.asBool); break;
        }
    }

private:
    std::vector<uint8_t> m_buffer;
};

class ByteReader {
public:
    ByteReader(const void* data, size_t len)
        : m_data(reinterpret_cast<const uint8_t*>(data)), m_len(len), m_pos(0) {}

    size_t Remaining() const noexcept { return m_pos < m_len ? m_len - m_pos : 0; }
    bool HasError() const noexcept { return m_error; }

    uint8_t ReadU8() {
        if (Remaining() < 1) { m_error = true; return 0; }
        return m_data[m_pos++];
    }

    uint16_t ReadU16() {
        if (Remaining() < 2) { m_error = true; return 0; }
        uint16_t v = static_cast<uint16_t>(m_data[m_pos]) |
                    (static_cast<uint16_t>(m_data[m_pos + 1]) << 8);
        m_pos += 2;
        return v;
    }

    uint32_t ReadU32() {
        if (Remaining() < 4) { m_error = true; return 0; }
        uint32_t v = static_cast<uint32_t>(m_data[m_pos]) |
                    (static_cast<uint32_t>(m_data[m_pos + 1]) << 8) |
                    (static_cast<uint32_t>(m_data[m_pos + 2]) << 16) |
                    (static_cast<uint32_t>(m_data[m_pos + 3]) << 24);
        m_pos += 4;
        return v;
    }

    uint64_t ReadU64() {
        if (Remaining() < 8) { m_error = true; return 0; }
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= (static_cast<uint64_t>(m_data[m_pos + i]) << (i * 8));
        }
        m_pos += 8;
        return v;
    }

    int64_t ReadI64() {
        return static_cast<int64_t>(ReadU64());
    }

    double ReadDouble() {
        uint64_t u = ReadU64();
        double d = 0.0;
        std::memcpy(&d, &u, sizeof(d));
        return d;
    }

    bool ReadBool() {
        return ReadU8() != 0;
    }

    std::string ReadString() {
        uint32_t slen = ReadU32();
        if (m_error || Remaining() < slen) { m_error = true; return {}; }
        std::string s(reinterpret_cast<const char*>(m_data + m_pos), slen);
        m_pos += slen;
        return s;
    }

    std::vector<uint8_t> ReadBytes() {
        uint32_t blen = ReadU32();
        if (m_error || Remaining() < blen) { m_error = true; return {}; }
        std::vector<uint8_t> b(m_data + m_pos, m_data + m_pos + blen);
        m_pos += blen;
        return b;
    }

    PeerId ReadPeerId() {
        PeerId id;
        id.high = ReadU64();
        id.low = ReadU64();
        return id;
    }

    LanEndpoint ReadEndpoint() {
        LanEndpoint ep;
        ep.ipv4 = ReadU32();
        ep.port = ReadU16();
        return ep;
    }

    AttributeValue ReadAttribute() {
        AttributeValue attr;
        attr.type = static_cast<AttributeType>(ReadU8());
        switch (attr.type) {
            case AttributeType::String:  attr.asString = ReadString(); break;
            case AttributeType::Int64:   attr.asInt64 = ReadI64(); break;
            case AttributeType::Double:  attr.asDouble = ReadDouble(); break;
            case AttributeType::Boolean: attr.asBool = ReadBool(); break;
            default: m_error = true; break;
        }
        return attr;
    }

private:
    const uint8_t* m_data;
    size_t m_len;
    size_t m_pos;
    bool m_error = false;
};

} // namespace refix::lan
