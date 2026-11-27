// Loopback protocol between ReFix (in the game) and refix_steamhost(64).exe, the helper process that talks to
// Steam as Spacewar. Steam keeps showing a process as "playing" until it exits, even after SteamAPI_Shutdown, so
// the Steam side lives in a process ReFix can end: turning Steam online off ends it, and Steam shows nothing.
// One UDP datagram per message on 127.0.0.1: a type byte, then little-endian fields.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace refix::steamlink::proto {

enum Type : uint8_t {
    // game -> helper
    kSend = 1,         // u64 to, tunnel datagram
    kPresence = 2,     // u32 appId, str game name
    kAvatar = 3,       // i32 image
    kQuit = 4,
    kCheckApp = 5,     // u32 appId
    kRequestAuthTicket = 6, // u32 reqId, u8 webApi, str identity
    kCancelAuthTicket = 7,  // u32 hostHandle
    kRequestEncryptedAppTicket = 8, // u32 reqId, u32 dataLen, raw data
    // helper -> game
    kHello = 10,       // token
    kState = 11,       // u8 step, u64 self, str error[, u32 actualBridgeAppId]
    kFriends = 12,     // u16 count, then per friend: u64 id, i32 state, u8 playing, u32 gameId, u8 refix,
                       // u32 appId, i32 avatar, str name, str game
    kMessage = 13,     // u64 from, tunnel datagram
    kAvatarData = 14,  // i32 image, u16 w, u16 h, rgba
    kLog = 15,         // str line: the helper's tunnel diagnostics, written to ReFix.log
    kAccountChanged = 16, // u64 newSteamId (0 = signed out, else switched account)
    kAppLicense = 17,  // u32 appId, u8 owned
    kAuthTicketData = 18, // u32 reqId, u32 hostHandle, i32 eResult, u32 ticketLen, raw ticketBytes
    kEncryptedAppTicketData = 19, // u32 reqId, i32 eResult, u32 ticketLen, raw ticketBytes

    // Matchmaking proxy (game -> helper)
    kLobbySearch = 20,
    kLobbyCreate = 21,
    kLobbyJoin = 22,
    kLobbyLeave = 23,
    kLobbySetData = 24,
    kLobbySetType = 25,
    kLobbySetJoinable = 26,
    kLobbySetLimit = 27,
    kLobbySetOwner = 28,
    kLobbySetGameServer = 29,
    kLobbyReqData = 30,

    // Achievements & Stats forwarding (game -> active game-AppID host)
    kStatsSetAchievement = 31,      // str16 apiName
    kStatsClearAchievement = 32,    // str16 apiName
    kStatsIndicateProgress = 33,    // str16 apiName, u32 cur, u32 max
    kStatsSetInt = 34,              // str16 apiName, i32 val
    kStatsSetFloat = 35,            // str16 apiName, float val
    kStatsStore = 36,               // empty
    kStatsResetAll = 37,            // u8 achievementsToo

    // Matchmaking proxy (helper -> game)
    kLobbySnapshot = 40,
    kLobbySearchDone = 41,
    kLobbyCreatedRes = 42,
    kLobbyEnterRes = 43,
    kLobbyDataChanged = 44,
    kLobbyChatChanged = 45,
};

enum Step : uint8_t { kOpeningSteam = 1, kSigningIn = 2, kConnecting = 3, kOnline = 4, kFailed = 5 };

constexpr size_t kMaxDatagram = 65000;

struct Writer {
    std::vector<uint8_t> b;
    explicit Writer(Type t) { b.push_back(t); }
    template <class T> Writer& Put(T v) {
        const size_t at = b.size();
        b.resize(at + sizeof(T));
        memcpy(&b[at], &v, sizeof(T));
        return *this;
    }
    Writer& Str(const std::string& s) {   // u8 length: names and errors are short
        const size_t n = s.size() < 255 ? s.size() : 255;
        b.push_back((uint8_t)n);
        b.insert(b.end(), s.begin(), s.begin() + n);
        return *this;
    }
    Writer& Str16(const std::string& s) {
        const size_t n = s.size() < 65535 ? s.size() : 65535;
        Put<uint16_t>((uint16_t)n);
        b.insert(b.end(), s.begin(), s.begin() + n);
        return *this;
    }
    Writer& Raw(const void* p, size_t n) {
        b.insert(b.end(), (const uint8_t*)p, (const uint8_t*)p + n);
        return *this;
    }
};

struct Reader {
    const uint8_t* p;
    const uint8_t* end;
    bool ok = true;
    Reader(const uint8_t* data, size_t n) : p(data), end(data + n) {}
    template <class T> T Get() {
        T v{};
        if (end - p < (ptrdiff_t)sizeof(T)) { ok = false; return v; }
        memcpy(&v, p, sizeof(T));
        p += sizeof(T);
        return v;
    }
    std::string Str() {
        const uint8_t n = Get<uint8_t>();
        if (!ok || end - p < n) { ok = false; return {}; }
        std::string s((const char*)p, n);
        p += n;
        return s;
    }
    std::string Str16() {
        const uint16_t n = Get<uint16_t>();
        if (!ok || end - p < n) { ok = false; return {}; }
        std::string s((const char*)p, n);
        p += n;
        return s;
    }
    size_t Left() const { return (size_t)(end - p); }
};

} // namespace refix::steamlink::proto
