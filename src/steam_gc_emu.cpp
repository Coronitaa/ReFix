#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>
#include "include/steam/steam_api.h"
#include "include/steam/isteamgamecoordinator.h"
#include "steam_gc_emu.h"
#include "unreal_steam_emu.h"

extern void ReFixLog(const char* fmt, ...);

namespace {

// GC SDK base messages (gcsystemmsgs.proto, EGCBaseClientMsg), common to every Valve GC game.
constexpr uint32_t kMsgClientWelcome = 4004;
constexpr uint32_t kMsgServerWelcome = 4005;
constexpr uint32_t kMsgClientHello = 4006;
constexpr uint32_t kMsgServerHello = 4007;
constexpr uint32_t kMsgClientConnectionStatus = 4009;
constexpr uint32_t kMsgServerConnectionStatus = 4010;
constexpr uint32_t kProtoFlag = 0x80000000;
constexpr size_t kProtoHeaderSize = 8;   // [msg | kProtoFlag][CMsgProtoBufHeader length]
constexpr uint32_t kSOTypeSteamId = 1;   // CMsgSOIDOwner.type for a cache owned by a SteamID
constexpr int kMaxLoggedSends = 8;

void PutVarint(std::vector<uint8_t>& out, uint64_t value) {
    while (value >= 0x80) { out.push_back((uint8_t)(value | 0x80)); value >>= 7; }
    out.push_back((uint8_t)value);
}

void PutVarintField(std::vector<uint8_t>& out, uint32_t field, uint64_t value) {
    PutVarint(out, (uint64_t)field << 3);
    PutVarint(out, value);
}

void PutFixed64Field(std::vector<uint8_t>& out, uint32_t field, uint64_t value) {
    PutVarint(out, ((uint64_t)field << 3) | 1);
    for (int i = 0; i < 8; i++) out.push_back((uint8_t)(value >> (8 * i)));
}

void PutBytesField(std::vector<uint8_t>& out, uint32_t field, const std::vector<uint8_t>& bytes) {
    PutVarint(out, ((uint64_t)field << 3) | 2);
    PutVarint(out, bytes.size());
    out.insert(out.end(), bytes.begin(), bytes.end());
}

bool GetVarint(const uint8_t*& p, const uint8_t* end, uint64_t& value) {
    value = 0;
    for (int shift = 0; p < end && shift < 64; shift += 7) {
        uint8_t b = *p++;
        value |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

class CGameCoordinatorLan : public ISteamGameCoordinator {
public:
    explicit CGameCoordinatorLan(bool gameServer) : m_gameServer(gameServer) {}

    EGCResults SendMessage_(uint32 unMsgType, const void* pubData, uint32 cubData) override {
        const uint32_t msg = unMsgType & ~kProtoFlag;
        const bool proto = (unMsgType & kProtoFlag) != 0;
        if (m_logged++ < kMaxLoggedSends)
            ReFixLog("[GC] %s sent %u%s (%u bytes)", Role(), msg, proto ? " (proto)" : "", cubData);
        if (!proto || !pubData || cubData < kProtoHeaderSize) return k_EGCResultOK;
        // Hellos repeat until the welcome is read; a pipe nobody polls yet must not pile up welcomes.
        if (HasPending()) return k_EGCResultOK;

        if (!m_gameServer && msg == kMsgClientHello) {
            uint32_t version = ParseBody(pubData, cubData, &ReFixGC::ParseHelloVersion);
            Reply(kMsgClientWelcome, ReFixGC::BuildClientWelcome(version, UnrealSteamEmu::GetLocalSteamID()));
            Reply(kMsgClientConnectionStatus, ReFixGC::BuildConnectionStatus());
        } else if (m_gameServer && msg == kMsgServerHello) {
            Reply(kMsgServerWelcome, {});
            Reply(kMsgServerConnectionStatus, ReFixGC::BuildConnectionStatus());
        }
        return k_EGCResultOK;
    }

    bool IsMessageAvailable(uint32* pcubMsgSize) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (pcubMsgSize) *pcubMsgSize = m_queue.empty() ? 0 : (uint32)m_queue.front().data.size();
        return !m_queue.empty();
    }

    EGCResults RetrieveMessage(uint32* punMsgType, void* pubDest, uint32 cubDest, uint32* pcubMsgSize) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            if (pcubMsgSize) *pcubMsgSize = 0;
            return k_EGCResultNoMessage;
        }
        const Message& front = m_queue.front();
        if (pcubMsgSize) *pcubMsgSize = (uint32)front.data.size();
        if (!pubDest || cubDest < front.data.size()) return k_EGCResultBufferTooSmall;
        memcpy(pubDest, front.data.data(), front.data.size());
        if (punMsgType) *punMsgType = front.type;
        m_queue.pop_front();
        return k_EGCResultOK;
    }

private:
    struct Message {
        uint32_t type;
        std::vector<uint8_t> data;
    };

    const char* Role() const { return m_gameServer ? "server" : "client"; }

    bool HasPending() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_queue.empty();
    }

    template <typename Parser>
    static uint32_t ParseBody(const void* data, uint32 size, Parser parse) {
        const uint8_t* bytes = (const uint8_t*)data;
        uint32_t headerLen = 0;
        memcpy(&headerLen, bytes + 4, sizeof(headerLen));
        if (kProtoHeaderSize + (size_t)headerLen > size) return 0;
        return parse(bytes + kProtoHeaderSize + headerLen, size - kProtoHeaderSize - headerLen);
    }

    // Queues [msg | proto][empty CMsgProtoBufHeader][body] and raises GCMessageAvailable_t on this pipe.
    void Reply(uint32_t msg, const std::vector<uint8_t>& body) {
        Message out{ msg | kProtoFlag, {} };
        const uint32_t header[2] = { msg | kProtoFlag, 0 };
        out.data.resize(sizeof(header));
        memcpy(out.data.data(), header, sizeof(header));
        out.data.insert(out.data.end(), body.begin(), body.end());

        GCMessageAvailable_t available = {};
        available.m_nMessageSize = (uint32)out.data.size();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(std::move(out));
        }
        if (m_gameServer)
            UnrealSteamEmu::PostGameServerCallback(GCMessageAvailable_t::k_iCallback, &available, sizeof(available));
        else
            UnrealSteamEmu::PostCallback(GCMessageAvailable_t::k_iCallback, &available, sizeof(available));
        ReFixLog("[GC] %s <- %u (%u bytes)", Role(), msg, available.m_nMessageSize);
    }

    const bool m_gameServer;
    std::atomic<int> m_logged{ 0 };
    std::mutex m_mutex;
    std::deque<Message> m_queue;
};

} // namespace

namespace ReFixGC {

ISteamGameCoordinator* Get(bool gameServer) {
    static CGameCoordinatorLan client(false);
    static CGameCoordinatorLan server(true);
    return gameServer ? static_cast<ISteamGameCoordinator*>(&server) : &client;
}

// CMsgClientWelcome: version (1) echoed from the hello, plus an empty SO cache for the local user
// (outofdate_subscribed_caches, 3) so inventory UIs see a loaded, empty cache instead of waiting for one.
std::vector<uint8_t> BuildClientWelcome(uint32_t version, uint64_t steamId) {
    std::vector<uint8_t> owner;                       // CMsgSOIDOwner
    PutVarintField(owner, 1, kSOTypeSteamId);
    PutVarintField(owner, 2, steamId);
    std::vector<uint8_t> cache;                       // CMsgSOCacheSubscribed
    PutFixed64Field(cache, 1, steamId);
    PutFixed64Field(cache, 3, 1);                     // version
    PutBytesField(cache, 4, owner);
    std::vector<uint8_t> welcome;
    if (version) PutVarintField(welcome, 1, version);
    PutBytesField(welcome, 3, cache);
    return welcome;
}

// CMsgConnectionStatus { status = GCConnectionStatus_HAVE_SESSION (0) }
std::vector<uint8_t> BuildConnectionStatus() {
    std::vector<uint8_t> status;
    PutVarintField(status, 1, 0);
    return status;
}

uint32_t ParseHelloVersion(const uint8_t* body, size_t size) {
    const uint8_t* p = body;
    const uint8_t* end = body + size;
    while (p < end) {
        uint64_t key = 0, value = 0;
        if (!GetVarint(p, end, key)) return 0;
        const uint32_t field = (uint32_t)(key >> 3);
        switch (key & 7) {
        case 0:
            if (!GetVarint(p, end, value)) return 0;
            if (field == 1) return (uint32_t)value;
            break;
        case 1: if (end - p < 8) return 0; p += 8; break;
        case 5: if (end - p < 4) return 0; p += 4; break;
        case 2:
            if (!GetVarint(p, end, value) || (uint64_t)(end - p) < value) return 0;
            p += value;
            break;
        default: return 0;
        }
    }
    return 0;
}

} // namespace ReFixGC
