#include "pn_fusion.h"

namespace PnFusion {
namespace {

constexpr uint8_t kVersionWithSerializationVersion = 2; // V1_1_0
constexpr uint8_t kVersionWithUniqueId = 6;             // V1_2_3
constexpr uint8_t kVersionWithPlayerRef = 7;            // V1_3_0
constexpr uint8_t kVersionWithEncryption = 9;           // V1_5_0
constexpr size_t kBufferAlignment = 4;

// Fusion.Protocol.BitStream: bits are filled from the least significant bit of each byte.
class BitWriter {
public:
    void Bits(uint32_t value, int count) {
        for (int i = 0; i < count; i++, m_bit++) {
            if (m_bit % 8 == 0) m_out.push_back('\0');
            if (value >> i & 1) m_out.back() = (char)((uint8_t)m_out.back() | 1 << (m_bit % 8));
        }
    }
    void U8(uint8_t v) { Bits(v, 8); }
    void I32(int32_t v) { Bits((uint32_t)v, 32); }
    void Optional(const OptionalBytes& b) {
        Bits(b.present ? 1 : 0, 1);
        if (!b.present) return;
        Bits((uint32_t)b.bytes.size() & 0xFFFF, 16);
        for (char c : b.bytes) U8((uint8_t)c);
    }
    std::string Finish() {
        while (m_out.size() % kBufferAlignment) m_out.push_back('\0');
        return m_out;
    }

private:
    std::string m_out;
    size_t m_bit = 0;
};

class BitReader {
public:
    explicit BitReader(const std::string& in) : m_in(in) {}
    bool ok = true;
    uint32_t Bits(int count) {
        uint32_t v = 0;
        for (int i = 0; i < count; i++, m_bit++) {
            if (m_bit / 8 >= m_in.size()) { ok = false; return 0; }
            if ((uint8_t)m_in[m_bit / 8] >> (m_bit % 8) & 1) v |= 1u << i;
        }
        return v;
    }
    uint8_t U8() { return (uint8_t)Bits(8); }
    int32_t I32() { return (int32_t)Bits(32); }
    void Optional(OptionalBytes& b) {
        b.present = Bits(1) != 0;
        b.bytes.clear();
        if (!b.present) return;
        uint32_t n = Bits(16);
        for (uint32_t i = 0; i < n && ok; i++) b.bytes.push_back((char)U8());
    }

private:
    const std::string& m_in;
    size_t m_bit = 0;
};

void ReadHeader(BitReader& r, Header& h) {
    h.protocolVersion = r.U8();
    if (h.protocolVersion < kVersionWithSerializationVersion) return;
    h.versionMajor = r.I32();
    h.versionMinor = r.I32();
    h.versionBuild = r.I32();
}

void WriteHeader(BitWriter& w, const Header& h) {
    w.U8(h.protocolVersion);
    if (h.protocolVersion < kVersionWithSerializationVersion) return;
    w.I32(h.versionMajor);
    w.I32(h.versionMinor);
    w.I32(h.versionBuild);
}

} // namespace

int FirstMessageId(const std::string& buffer) {
    return buffer.empty() ? -1 : (uint8_t)buffer[0];
}

bool ParseJoin(const std::string& buffer, Header& header, Join& join) {
    BitReader r(buffer);
    if (r.U8() != MSG_JOIN) return false;
    ReadHeader(r, header);
    join = Join();
    join.type = r.U8();
    join.gameMode = r.U8();
    join.peerMode = r.U8();
    join.joinRequests = r.I32();
    uint8_t v = header.protocolVersion;
    if (v >= kVersionWithUniqueId) r.Optional(join.uniqueId);
    if (v >= kVersionWithPlayerRef) join.playerRef = r.I32();
    if (v >= kVersionWithEncryption) {
        r.Optional(join.encryptionKey);
        r.Optional(join.encryptionKeySecret);
    }
    return r.ok;
}

std::string BuildJoin(const Header& header, const Join& join) {
    BitWriter w;
    w.U8(MSG_JOIN);
    WriteHeader(w, header);
    w.U8(join.type);
    w.U8(join.gameMode);
    w.U8(join.peerMode);
    w.I32(join.joinRequests);
    uint8_t v = header.protocolVersion;
    if (v >= kVersionWithUniqueId) w.Optional(join.uniqueId);
    if (v >= kVersionWithPlayerRef) w.I32(join.playerRef);
    if (v >= kVersionWithEncryption) {
        w.Optional(join.encryptionKey);
        w.Optional(join.encryptionKeySecret);
    }
    return w.Finish();
}

} // namespace PnFusion
