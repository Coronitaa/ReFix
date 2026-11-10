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
    // helper -> game
    kHello = 10,       // token
    kState = 11,       // u8 step, u64 self, str error
    kFriends = 12,     // u16 count, then per friend: u64 id, i32 state, u8 playing, u32 gameId, u8 refix,
                       // u32 appId, i32 avatar, str name, str game
    kMessage = 13,     // u64 from, tunnel datagram
    kAvatarData = 14,  // i32 image, u16 w, u16 h, rgba
    kLog = 15,         // str line: the helper's tunnel diagnostics, written to ReFix.log
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
    size_t Left() const { return (size_t)(end - p); }
};

} // namespace refix::steamlink::proto
