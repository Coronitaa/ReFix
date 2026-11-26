#include "pn_quantum.h"
#include <algorithm>
#include <cstring>

namespace PnQuantum {
namespace {

class BitWriter {
public:
    void Bits(uint64_t value, int count) {
        for (int i = 0; i < count; i++, m_bit++) {
            if ((m_bit & 7) == 0) m_out.push_back('\0');
            if ((value >> i) & 1ULL) {
                m_out.back() = (char)((uint8_t)m_out.back() | (1u << (m_bit & 7)));
            }
        }
    }

    void RoundToByte() {
        while ((m_bit & 7) != 0) Bits(0, 1);
    }

    void Bool(bool v) { Bits(v ? 1u : 0u, 1); }
    void U8(uint8_t v) { Bits(v, 8); }
    void U16(uint16_t v) { Bits(v, 16); }
    void I32(int32_t v) { Bits((uint32_t)v, 32); }

    void Double(double d) {
        uint64_t raw = 0;
        std::memcpy(&raw, &d, sizeof(raw));
        for (int i = 0; i < 8; i++) U8((uint8_t)((raw >> (i * 8)) & 0xFF));
    }

    // Quantum strings use 1=null, 0=present followed by 16-bit byte length and UTF-8 bytes.
    void String(const OptionalBytes& s) {
        Bool(!s.present);
        if (!s.present) return;
        U16((uint16_t)s.bytes.size());
        for (char c : s.bytes) U8((uint8_t)c);
    }

    // Quantum byte[] uses 1=present, 0=null followed by 16-bit length and raw bytes.
    void Bytes(const OptionalBytes& b) {
        Bool(b.present);
        if (!b.present) return;
        U16((uint16_t)b.bytes.size());
        for (char c : b.bytes) U8((uint8_t)c);
    }

    void IntArray(const std::vector<int32_t>& arr, bool present) {
        Bool(present);
        if (!present) return;
        U16((uint16_t)arr.size());
        for (int32_t v : arr) I32(v);
    }

    std::string Finish() {
        RoundToByte();
        return std::move(m_out);
    }

private:
    std::string m_out;
    size_t m_bit = 0;
};

class BitReader {
public:
    explicit BitReader(const std::string& in) : m_in(in) {}
    bool ok = true;

    size_t BitsLeft() const {
        return (m_bit <= m_in.size() * 8) ? (m_in.size() * 8 - m_bit) : 0;
    }

    size_t BitPos() const { return m_bit; }

    uint64_t Bits64(int count) {
        if (count <= 0) return 0;
        if (m_bit + (size_t)count > m_in.size() * 8) {
            ok = false;
            return 0;
        }
        uint64_t v = 0;
        for (int i = 0; i < count; i++, m_bit++) {
            uint64_t b = ((uint8_t)m_in[m_bit >> 3] >> (m_bit & 7)) & 1u;
            v |= (b << i);
        }
        return v;
    }

    uint32_t Bits(int count) { return (uint32_t)Bits64(count); }
    bool Bool() { return Bits(1) != 0; }
    uint8_t U8() { return (uint8_t)Bits(8); }
    uint16_t U16() { return (uint16_t)Bits(16); }
    int32_t I32() { return (int32_t)Bits(32); }

    double Double() {
        uint64_t raw = 0;
        for (int i = 0; i < 8; i++) raw |= ((uint64_t)U8()) << (i * 8);
        double d = 0.0;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }

    void RoundToByte() {
        size_t rem = m_bit & 7;
        if (rem != 0) {
            size_t skip = 8 - rem;
            if (m_bit + skip <= m_in.size() * 8) m_bit += skip;
            else m_bit = m_in.size() * 8;
        }
    }

    void String(OptionalBytes& s) {
        bool isNull = Bool();
        s.present = !isNull;
        s.bytes.clear();
        if (!ok || isNull) return;
        uint16_t len = U16();
        if (!ok || BitsLeft() < (size_t)len * 8) {
            ok = false;
            return;
        }
        s.bytes.resize(len);
        for (uint16_t i = 0; i < len; i++) s.bytes[i] = (char)U8();
    }

    void Bytes(OptionalBytes& b) {
        b.present = Bool();
        b.bytes.clear();
        if (!ok || !b.present) return;
        uint16_t len = U16();
        if (!ok || BitsLeft() < (size_t)len * 8) {
            ok = false;
            return;
        }
        b.bytes.resize(len);
        for (uint16_t i = 0; i < len; i++) b.bytes[i] = (char)U8();
    }

private:
    const std::string& m_in;
    size_t m_bit = 0;
};

void WriteSessionConfig(BitWriter& w, const SessionConfigData& cfg) {
    // Nested reference object uses 1=null, 0=present.
    w.Bool(!cfg.present);
    if (!cfg.present) return;
    w.I32(cfg.i0);
    w.I32(cfg.playerCount);
    w.Bool(cfg.b0 != 0);
    w.Bool(cfg.b1 != 0);
    w.Bool(cfg.b2 != 0);
    w.Bool(cfg.b3 != 0);
    w.Bool(cfg.b4 != 0);
    w.I32(cfg.i2);
    w.I32(cfg.i3);
    w.I32(cfg.i4);
    w.I32(cfg.updateFps);
    for (int i = 0; i < 13; i++) w.I32(cfg.i6[i]);
    w.Bool(cfg.b5 != 0);
    w.Bool(cfg.b6 != 0);
    w.I32(cfg.i19);
}

bool ReadSessionConfig(BitReader& r, SessionConfigData& cfg) {
    bool isNull = r.Bool();
    cfg.present = !isNull;
    if (!r.ok) return false;
    if (isNull) return true;
    cfg.i0 = r.I32();
    cfg.playerCount = r.I32();
    cfg.b0 = r.Bool() ? 1 : 0;
    cfg.b1 = r.Bool() ? 1 : 0;
    cfg.b2 = r.Bool() ? 1 : 0;
    cfg.b3 = r.Bool() ? 1 : 0;
    cfg.b4 = r.Bool() ? 1 : 0;
    cfg.i2 = r.I32();
    cfg.i3 = r.I32();
    cfg.i4 = r.I32();
    cfg.updateFps = r.I32();
    for (int i = 0; i < 13; i++) cfg.i6[i] = r.I32();
    cfg.b5 = r.Bool() ? 1 : 0;
    cfg.b6 = r.Bool() ? 1 : 0;
    cfg.i19 = r.I32();
    return r.ok;
}

} // namespace

int ParseProtocolVersion(const std::string& ver) {
    if (ver.empty()) return 7;
    int major = 0, minor = 0;
    size_t dot1 = ver.find('.');
    if (dot1 != std::string::npos) {
        major = std::atoi(ver.substr(0, dot1).c_str());
        size_t dot2 = ver.find('.', dot1 + 1);
        minor = std::atoi(ver.substr(dot1 + 1, dot2 == std::string::npos ? dot2 : dot2 - dot1 - 1).c_str());
    } else {
        major = std::atoi(ver.c_str());
    }
    if (major >= 3 || (major == 2 && minor >= 2)) return 7;
    if (major == 2 && minor >= 1) return 6;
    return 5;
}

bool DetectStartRequest(const std::string& buffer, std::string& versionOut) {
    if (buffer.size() < 8 || (uint8_t)buffer[0] != MSG_START_REQUEST) return false;
    BitReader r(buffer);
    if (r.U8() != MSG_START_REQUEST) return false;
    OptionalBytes id, ver;
    r.String(id);
    r.String(ver);
    if (!r.ok || !ver.present || ver.bytes.empty()) return false;
    int dots = 0;
    for (char c : ver.bytes) {
        if (c == '.') dots++;
        else if (c < '0' || c > '9') return false;
    }
    if (dots < 2) return false;
    versionOut = ver.bytes;
    return true;
}

bool ParseProtocolPacket(const std::string& buffer, int protoHint, std::vector<ParsedMessage>& out) {
    out.clear();
    if (buffer.empty()) return false;
    BitReader r(buffer);
    while (r.BitsLeft() >= 8) {
        size_t startByte = r.BitPos() >> 3;
        uint8_t id = r.U8();
        if (!r.ok) return !out.empty();
        ParsedMessage m;
        m.msgId = id;
        switch (id) {
            case MSG_START_REQUEST: {
                r.String(m.startReq.id);
                r.String(m.startReq.protocolVersion);
                m.startReq.playerSlots = r.I32();
                m.startReq.initialTick = r.I32();
                m.startReq.playerCount = r.I32();
                if (!r.ok) return false;
                int v = m.startReq.protocolVersion.present
                    ? ParseProtocolVersion(m.startReq.protocolVersion.bytes)
                    : protoHint;
                if (v >= 7 && r.BitsLeft() >= 1) {
                    m.startReq.rejoin = r.Bool();
                }
                break;
            }
            case MSG_SESSION_CONFIG: {
                m.sessionCfg.requested = r.Bool();
                if (!ReadSessionConfig(r, m.sessionCfg.config)) return false;
                break;
            }
            case MSG_RUNTIME_CONFIG: {
                m.runtimeCfg.requested = r.Bool();
                r.Bytes(m.runtimeCfg.config);
                if (!r.ok) return false;
                break;
            }
            case MSG_RTT_UPDATE: {
                m.rtt = r.I32();
                if (!r.ok) return false;
                break;
            }
            case MSG_COMMAND:
            case MSG_SET_PLAYER_DATA: {
                m.player = r.I32();
                r.Bytes(m.payload);
                if (!r.ok) return false;
                break;
            }
            case MSG_FRAME_SNAPSHOT_REQUEST: {
                m.snapshotReq.referenceTick = r.I32();
                if (!r.ok) return false;
                break;
            }
            case MSG_FRAME_SNAPSHOT: {
                m.frameSnapshot.tick = r.I32();
                m.frameSnapshot.totalSize = r.I32();
                m.frameSnapshot.last = r.Bool();
                r.Bytes(m.frameSnapshot.data);
                if (!r.ok) return false;
                break;
            }
            case MSG_TICK_CHECKSUM: {
                // Unknown tail inside Event 101 packet if non-RTT; stop cleanly so caller still sees RTT.
                return !out.empty();
            }
            default: {
                // Unknown control message; return messages parsed so far if any.
                return !out.empty();
            }
        }
        size_t endByte = (r.BitPos() + 7) >> 3;
        if (endByte <= buffer.size() && endByte > startByte) {
            m.rawSlice = buffer.substr(startByte, endByte - startByte);
        }
        out.push_back(std::move(m));
    }
    return !out.empty();
}

bool ParseClientInputs(const std::string& buffer, std::vector<ClientInput>& out) {
    out.clear();
    if (buffer.size() < 6) return false;
    BitReader r(buffer);
    // Shortest input record is player(8) + tick(32) + data_present(1) + rpc_present(1) + flags(4) = 46 bits.
    while (r.BitsLeft() >= 46) {
        ClientInput in;
        in.player = r.U8();
        in.tick = r.I32();
        r.Bytes(in.data);
        if (!r.ok) break;
        // 1 bit follows Data in SendLocalInputs (0 = no RPC); if 1, read 16-bit RPC bytes.
        bool hasRpc = r.Bool();
        if (!r.ok) break;
        if (hasRpc) {
            uint16_t rpcLen = r.U16();
            if (!r.ok || r.BitsLeft() < (size_t)rpcLen * 8 + 4) break;
            in.rpc.present = true;
            in.rpc.bytes.resize(rpcLen);
            for (uint16_t i = 0; i < rpcLen; i++) in.rpc.bytes[i] = (char)r.U8();
        }
        in.flags = (uint8_t)r.Bits(4);
        if (!r.ok) break;
        out.push_back(std::move(in));
    }
    return !out.empty();
}

std::string BuildJoined(int protoVer, const Joined& msg) {
    BitWriter w;
    w.U8(MSG_JOINED);
    if (protoVer < 6) {
        w.Bool(msg.status == JOIN_STATUS_CONFIRMED || msg.status == JOIN_STATUS_WAIT_FOR_SNAPSHOT);
    } else {
        w.I32(msg.status);
    }
    w.IntArray(msg.playerSlots, true);
    return w.Finish();
}

std::string BuildSessionConfigRequest() {
    BitWriter w;
    w.U8(MSG_SESSION_CONFIG);
    w.Bool(true);
    SessionConfigData empty;
    empty.present = false;
    WriteSessionConfig(w, empty);
    return w.Finish();
}

std::string BuildRuntimeConfigRequest() {
    BitWriter w;
    w.U8(MSG_RUNTIME_CONFIG);
    w.Bool(true);
    OptionalBytes empty;
    w.Bytes(empty);
    return w.Finish();
}

std::string BuildSimulationStart(const SimulationStart& msg) {
    BitWriter w;
    w.U8(MSG_SIMULATION_START);
    w.Bool(msg.reconnect);
    w.Double(msg.serverTime);
    w.Bytes(msg.runtimeConfig);
    WriteSessionConfig(w, msg.sessionConfig);
    return w.Finish();
}

std::string BuildRttUpdate(int32_t rtt) {
    BitWriter w;
    w.U8(MSG_RTT_UPDATE);
    w.I32(rtt);
    return w.Finish();
}

std::string BuildFrameSnapshotRequest(int32_t referenceTick) {
    BitWriter w;
    w.U8(MSG_FRAME_SNAPSHOT_REQUEST);
    w.I32(referenceTick);
    return w.Finish();
}

std::string BuildFrameSnapshot(const FrameSnapshotMsg& msg) {
    BitWriter w;
    w.U8(MSG_FRAME_SNAPSHOT);
    w.I32(msg.tick);
    w.I32(msg.totalSize);
    w.Bool(msg.last);
    w.Bytes(msg.data);
    return w.Finish();
}

std::string BuildServerTickPacket(int32_t rttMs, int32_t playerCount, double serverTimeSec,
                                  const std::vector<ServerTickEntry>& ticks) {
    int pCount = std::max(1, std::min(64, playerCount));
    BitWriter w;
    // Header layout verified from 0x1819CF330 (DequeueNetworkRecvQueue packet decoder):
    //   [int32 rtt/seq][double serverTime][double timeScale][uint8 playerCount][1-bit hasFixedSize]
    w.I32(rttMs);
    w.Double(serverTimeSec);
    w.Double(1.0);
    w.U8((uint8_t)pCount);
    w.Bool(false); // dynamic per-input length (uses BitStream.Serialize(ref byte[], ref int))

    for (const ServerTickEntry& t : ticks) {
        // Per-tick header layout verified from 0x1819CECC0..0x1819CEEDE:
        //   [RoundToByte][int32 tick][1-bit verified][playerCount-bit playerMask]
        w.RoundToByte();
        w.I32(t.tick);
        w.Bool(t.verified);
        if (pCount <= 32) {
            w.Bits(t.playerMask, pCount);
        } else {
            w.Bits(t.playerMask & 0xFFFFFFFFULL, 32);
            w.Bits(t.playerMask >> 32, pCount - 32);
        }
        for (int p = 0; p < pCount; p++) {
            if (((t.playerMask >> p) & 1ULL) == 0) continue;
            auto it = t.inputs.find((uint8_t)p);
            if (it == t.inputs.end()) continue;
            const ClientInput& in = it->second;
            // Input.Serialize layout verified from 0x1819CF4F0..0x1819CF70A:
            //   [1-bit isRepeated=0][Bytes data][Bytes rpc][4-bit flags]
            w.Bool(false);
            w.Bytes(in.data);
            w.Bytes(in.rpc);
            w.Bits(in.flags & 0x0Fu, 4);
        }
    }
    return w.Finish();
}

} // namespace PnQuantum
