#include "pn_enet.h"

namespace {

constexpr uint8_t CT_ACK = 1, CT_CONNECT = 2, CT_VERIFYCONNECT = 3, CT_DISCONNECT = 4, CT_SENDRELIABLE = 6,
                  CT_SENDUNRELIABLE = 7, CT_SENDFRAGMENT = 8, CT_SENDUNSEQUENCED = 11, CT_RELIABLE_UNSEQUENCED = 14,
                  CT_FRAGMENT_UNSEQUENCED = 15, CT_ACK_UNSEQUENCED = 16;
constexpr uint8_t FV_RELIABLE = 1, FV_UNRELIABLE_UNSEQUENCED = 2, FV_RELIABLE_UNSEQUENCED = 3;
constexpr uint8_t kFlagCrc = 204, kFlagEncrypted = 1;
constexpr uint8_t kControlChannel = 255;
constexpr size_t kHeaderSize = 12;          // datagram and command header size
constexpr size_t kFragmentExtra = 20;
constexpr size_t kMtu = 1200;               // client default MTU
constexpr size_t kMaxPayload = 1100;        // per command, leaves room for headers
constexpr size_t kVerifyConnectPayload = 32;
constexpr uint32_t kResendMs = 300;
constexpr int kMaxSends = 40;               // ~12 s of retransmissions before giving up
constexpr uint32_t kTimeoutMs = 15000;

int32_t BE32(const uint8_t* p) { return (int32_t)((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]); }
void PutBE32(std::string& s, int32_t v) {
    s += (char)((uint32_t)v >> 24); s += (char)((uint32_t)v >> 16); s += (char)((uint32_t)v >> 8); s += (char)v;
}
void PutBE16(std::string& s, int16_t v) { s += (char)((uint16_t)v >> 8); s += (char)v; }

size_t ExtraSize(uint8_t type) {
    switch (type) {
        case CT_ACK: case CT_ACK_UNSEQUENCED: return 8;
        case CT_SENDUNRELIABLE: case CT_SENDUNSEQUENCED: return 4;
        case CT_SENDFRAGMENT: case CT_FRAGMENT_UNSEQUENCED: return kFragmentExtra;
        default: return 0;
    }
}

std::string CommandHeader(uint8_t type, uint8_t channel, uint8_t flags, int32_t size, int32_t reliableSeq) {
    std::string h;
    h += (char)type; h += (char)channel; h += (char)flags; h += (char)0;
    PutBE32(h, size);
    PutBE32(h, reliableSeq);
    return h;
}

} // namespace

PnEnetConnection::PnEnetConnection(int16_t peerId, SendFn send, MessageFn onMessage)
    : m_peerId(peerId), m_send(std::move(send)), m_onMessage(std::move(onMessage)) {}

void PnEnetConnection::OnDatagram(const uint8_t* data, size_t size, uint32_t nowMs) {
    if (m_closed || size < kHeaderSize) return;
    uint8_t flags = data[2];
    uint8_t count = data[3];
    int32_t sentTime = BE32(data + 4);
    int32_t challenge = BE32(data + 8);
    if (flags == kFlagEncrypted) return; // datagram encryption is not offered by this emulator
    size_t offset = kHeaderSize + (flags == kFlagCrc ? 4 : 0);
    if (m_connected && challenge != m_challenge) return;
    if (!m_connected) m_challenge = challenge;
    m_lastReceiveMs = nowMs;
    m_nowMs = nowMs;

    for (uint8_t i = 0; i < count && offset + kHeaderSize <= size; i++) {
        const uint8_t* c = data + offset;
        int32_t cmdSize = BE32(c + 4);
        size_t extra = ExtraSize(c[0]);
        if (cmdSize < (int32_t)(kHeaderSize + extra) || offset + (size_t)cmdSize > size) return;
        HandleCommand(c[0], c[1], c[2], BE32(c + 8), c + kHeaderSize, c + kHeaderSize + extra,
                      (size_t)cmdSize - kHeaderSize - extra, sentTime);
        offset += (size_t)cmdSize;
    }
}

void PnEnetConnection::HandleCommand(uint8_t type, uint8_t channel, uint8_t flags, int32_t reliableSeq,
                                     const uint8_t* extra, const uint8_t* payload, size_t payloadSize, int32_t sentTime) {
    if (flags == FV_RELIABLE || flags == FV_RELIABLE_UNSEQUENCED) {
        QueueAck(channel, reliableSeq, sentTime, flags == FV_RELIABLE_UNSEQUENCED);
    }
    switch (type) {
        case CT_ACK:
        case CT_ACK_UNSEQUENCED: {
            int32_t acked = BE32(extra);
            for (auto it = m_unacked.begin(); it != m_unacked.end(); ++it) {
                if (it->channel == channel && it->sequence == acked) { m_unacked.erase(it); break; }
            }
            break;
        }
        case CT_CONNECT:
            if (!m_connected) {
                std::string verify(kVerifyConnectPayload, '\0');
                verify[0] = (char)((uint16_t)m_peerId >> 8);
                verify[1] = (char)m_peerId;
                QueueReliable(CT_VERIFYCONNECT, kControlChannel, "", verify);
                m_connected = true;
            }
            break;
        case CT_DISCONNECT:
            m_closed = true;
            break;
        case CT_SENDRELIABLE:
        case CT_SENDFRAGMENT:
            DeliverReliable(channel, reliableSeq, type, extra, payload, payloadSize);
            break;
        case CT_SENDUNRELIABLE:
        case CT_SENDUNSEQUENCED:
        case CT_RELIABLE_UNSEQUENCED:
            m_onMessage(std::string((const char*)payload, payloadSize), type == CT_RELIABLE_UNSEQUENCED, channel);
            break;
        default: // ping, server time, unsequenced fragments: acknowledged above, nothing to deliver
            break;
    }
}

void PnEnetConnection::DeliverReliable(uint8_t channel, int32_t seq, uint8_t type, const uint8_t* extra,
                                       const uint8_t* payload, size_t size) {
    IncomingChannel& ch = m_inChannels[channel];
    if (seq < ch.nextReliable) return; // duplicate (retransmission whose ack was lost)
    std::string item(1, (char)type);
    if (type == CT_SENDFRAGMENT) item.append((const char*)extra, kFragmentExtra);
    item.append((const char*)payload, size);
    ch.pending.emplace(seq, std::move(item));
    for (auto it = ch.pending.find(ch.nextReliable); it != ch.pending.end(); it = ch.pending.find(ch.nextReliable)) {
        std::string next = std::move(it->second);
        ch.pending.erase(it);
        ch.nextReliable++;
        DeliverOrdered(ch, channel, next);
    }
}

void PnEnetConnection::DeliverOrdered(IncomingChannel& ch, uint8_t channel, const std::string& item) {
    if ((uint8_t)item[0] != CT_SENDFRAGMENT) {
        m_onMessage(item.substr(1), true, channel);
        return;
    }
    const uint8_t* f = (const uint8_t*)item.data() + 1;
    int32_t count = BE32(f + 4), number = BE32(f + 8), total = BE32(f + 12), offset = BE32(f + 16);
    size_t chunk = item.size() - 1 - kFragmentExtra;
    if (number == 0) {
        if (total <= 0 || count <= 0) return;
        ch.fragmentBuffer.assign((size_t)total, '\0');
        ch.fragmentsLeft = count;
    }
    if (ch.fragmentsLeft <= 0 || offset < 0 || (size_t)offset + chunk > ch.fragmentBuffer.size()) return;
    ch.fragmentBuffer.replace((size_t)offset, chunk, item, 1 + kFragmentExtra, chunk);
    if (--ch.fragmentsLeft == 0) m_onMessage(ch.fragmentBuffer, true, channel);
}

void PnEnetConnection::QueueAck(uint8_t channel, int32_t seq, int32_t sentTime, bool unsequenced) {
    std::string ack;
    ack += (char)(unsequenced ? CT_ACK_UNSEQUENCED : CT_ACK);
    ack += (char)channel; ack += (char)0; ack += (char)4;
    PutBE32(ack, (int32_t)(kHeaderSize + 8));
    PutBE32(ack, 0);
    PutBE32(ack, seq);
    PutBE32(ack, sentTime);
    m_acks.push_back(ack);
}

void PnEnetConnection::QueueReliable(uint8_t type, uint8_t channel, const std::string& extra, const std::string& payload) {
    int32_t seq = ++m_outReliableSeq[channel];
    Outgoing out;
    out.bytes = CommandHeader(type, channel, FV_RELIABLE, (int32_t)(kHeaderSize + extra.size() + payload.size()), seq) + extra + payload;
    out.channel = channel;
    out.sequence = seq;
    m_unacked.push_back(std::move(out));
}

void PnEnetConnection::QueueUnsequenced(uint8_t channel, const std::string& payload) {
    std::string cmd = CommandHeader(CT_SENDUNSEQUENCED, channel, FV_UNRELIABLE_UNSEQUENCED,
                                    (int32_t)(kHeaderSize + 4 + payload.size()), 0);
    PutBE32(cmd, ++m_unsequencedGroup);
    m_unreliableQueue.push_back(cmd + payload);
}

void PnEnetConnection::SendPayload(const std::string& message, bool reliable, uint8_t channel) {
    if (m_closed) return;
    if (!reliable && message.size() <= kMaxPayload) {
        QueueUnsequenced(channel, message);
        return;
    }
    if (message.size() <= kMaxPayload) {
        QueueReliable(CT_SENDRELIABLE, channel, "", message);
        return;
    }
    int32_t count = (int32_t)((message.size() + kMaxPayload - 1) / kMaxPayload);
    int32_t start = m_outReliableSeq[channel] + 1;
    for (int32_t i = 0; i < count; i++) {
        size_t offset = (size_t)i * kMaxPayload;
        std::string extra;
        PutBE32(extra, start);
        PutBE32(extra, count);
        PutBE32(extra, i);
        PutBE32(extra, (int32_t)message.size());
        PutBE32(extra, (int32_t)offset);
        QueueReliable(CT_SENDFRAGMENT, channel, extra, message.substr(offset, kMaxPayload));
    }
}

void PnEnetConnection::Disconnect() {
    if (m_closed) return;
    m_unreliableQueue.push_back(CommandHeader(CT_DISCONNECT, kControlChannel, 0, (int32_t)kHeaderSize, 0));
    Tick(m_nowMs);
    m_closed = true;
}

void PnEnetConnection::Tick(uint32_t nowMs) {
    m_nowMs = nowMs;
    if (m_closed) return;
    if (m_connected && nowMs - m_lastReceiveMs > kTimeoutMs) {
        m_closed = true;
        return;
    }
    std::vector<std::string> commands;
    commands.swap(m_acks);
    while (!m_unreliableQueue.empty()) {
        commands.push_back(std::move(m_unreliableQueue.front()));
        m_unreliableQueue.pop_front();
    }
    for (auto& out : m_unacked) {
        if (out.sendCount > 0 && nowMs - out.lastSentMs < kResendMs) continue;
        if (++out.sendCount > kMaxSends) {
            m_closed = true;
            return;
        }
        out.lastSentMs = nowMs;
        commands.push_back(out.bytes);
    }
    if (!commands.empty()) SendCommands(commands, nowMs);
}

void PnEnetConnection::SendCommands(const std::vector<std::string>& commands, uint32_t nowMs) {
    std::string body;
    uint8_t count = 0;
    auto flush = [&]() {
        if (!count) return;
        std::string datagram;
        PutBE16(datagram, m_peerId);
        datagram += (char)0;
        datagram += (char)count;
        PutBE32(datagram, (int32_t)nowMs);
        PutBE32(datagram, m_challenge);
        m_send(datagram + body);
        body.clear();
        count = 0;
    };
    for (const auto& cmd : commands) {
        if (count && (kHeaderSize + body.size() + cmd.size() > kMtu || count == 255)) flush();
        body += cmd;
        count++;
    }
    flush();
}
