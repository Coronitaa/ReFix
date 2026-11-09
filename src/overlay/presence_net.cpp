#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "presence_net.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <set>

#include "../steam_p2p_hook.h"
#include "../lan_core/refix_lan_core.h"

#if REFIX_STEAM_LINK
#include "../steam_link/steam_link.h"
#endif

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::net {
namespace {

constexpr uint32_t kMagic = 0x50584652;   // "RFXP"
constexpr uint8_t kVersion = 1;
constexpr ULONGLONG kAnnounceMs = 2000;
constexpr ULONGLONG kStaleMs = 7000;
constexpr ULONGLONG kAdapterRefreshMs = 30000;
constexpr ULONGLONG kAvatarRetryMs = 1500;
constexpr ULONGLONG kInviteRetryMs = 400;
constexpr int kInviteTries = 6;
constexpr size_t kChunkBytes = 1000;
constexpr size_t kMaxAvatarBytes = 256 * 1024;
constexpr size_t kMaxConnect = 256;
constexpr int kPollMs = 100;

enum class Msg : uint8_t { Presence = 1, AvatarReq = 2, AvatarChunk = 3, Invite = 4, InviteAck = 5 };

class Writer {
public:
    explicit Writer(Msg m) { U32(kMagic); U8(kVersion); U8((uint8_t)m); }
    void U8(uint8_t v) { buf_.push_back(v); }
    void U16(uint16_t v) { Raw(&v, 2); }
    void U32(uint32_t v) { Raw(&v, 4); }
    void U64(uint64_t v) { Raw(&v, 8); }
    void Str8(const std::string& s) { size_t n = std::min<size_t>(s.size(), 255); U8((uint8_t)n); Raw(s.data(), n); }
    void Str16(const std::string& s) { size_t n = std::min(s.size(), kMaxConnect); U16((uint16_t)n); Raw(s.data(), n); }
    void Raw(const void* p, size_t n) { const uint8_t* b = (const uint8_t*)p; buf_.insert(buf_.end(), b, b + n); }
    const std::vector<uint8_t>& Data() const { return buf_; }
private:
    std::vector<uint8_t> buf_;
};

class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    uint8_t U8() { uint8_t v = 0; Raw(&v, 1); return v; }
    uint16_t U16() { uint16_t v = 0; Raw(&v, 2); return v; }
    uint32_t U32() { uint32_t v = 0; Raw(&v, 4); return v; }
    uint64_t U64() { uint64_t v = 0; Raw(&v, 8); return v; }
    std::string Str(size_t n) { std::string s; if (Ok(n)) { s.assign((const char*)p_ + off_, n); off_ += n; } return s; }
    std::string Str8() { return Str(U8()); }
    std::string Str16() { return Str(U16()); }
    const uint8_t* Rest(size_t& n) { n = ok_ ? n_ - off_ : 0; return p_ + off_; }
    bool Ok(size_t n = 0) { if (off_ + n > n_) ok_ = false; return ok_; }
private:
    void Raw(void* v, size_t n) { if (Ok(n)) { memcpy(v, p_ + off_, n); off_ += n; } }
    const uint8_t* p_;
    size_t n_;
    size_t off_ = 0;
    bool ok_ = true;
};

struct Download {
    uint32_t key = 0;
    uint32_t total = 0;
    std::vector<uint8_t> data;
    std::vector<bool> got;
    size_t received = 0;
    ULONGLONG lastRequest = 0;
    bool complete = false;
};

struct PendingInvite {
    Invite invite;
    uint64_t to = 0;
    int tries = 0;
    ULONGLONG next = 0;
};

std::mutex g_mu;
std::atomic<bool> g_started{false};
SOCKET g_bcast = INVALID_SOCKET;
SOCKET g_ucast = INVALID_SOCKET;
uint16_t g_ucastPort = 0;
Presence g_local;
bool g_localDirty = true;
uint32_t g_avatarKey = 0;
std::vector<uint8_t> g_avatarJpeg;
std::map<uint64_t, RemotePlayer> g_players;
std::map<uint64_t, Download> g_downloads;
std::vector<PendingInvite> g_outbox;
std::vector<Invite> g_inbox;
std::set<std::pair<uint64_t, uint32_t>> g_seenInvites;
std::vector<uint32_t> g_broadcasts;
uint32_t g_nextInviteId = 1;

void SendTo(SOCKET s, uint32_t ip, uint16_t port, const std::vector<uint8_t>& data) {
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(ip);
    to.sin_port = htons(port);
#if REFIX_STEAM_LINK
    if (steamlink::IsVirtual(ip)) {   // a Steam friend: through the ReFix Online tunnel
        steamlink::Send(ip, g_ucastPort, port, data.data(), data.size());
        return;
    }
#endif
    sendto(s, (const char*)data.data(), (int)data.size(), 0, (const sockaddr*)&to, sizeof(to));
}

void RefreshBroadcasts() {
    std::vector<uint32_t> out = { 0xFFFFFFFFu, 0x7F000001u };
    ULONG size = 16 * 1024;
    std::vector<uint8_t> buf(size);
    auto* aa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, aa, &size) == ERROR_BUFFER_OVERFLOW) {
        buf.resize(size);
        aa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    }
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, aa, &size) == NO_ERROR) {
        for (auto* a = aa; a; a = a->Next) {
            if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
                if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
                const uint32_t ip = ntohl(reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr)->sin_addr.s_addr);
                const uint8_t prefix = u->OnLinkPrefixLength;
                if (prefix == 0 || prefix >= 31) continue;
                const uint32_t mask = 0xFFFFFFFFu << (32 - prefix);
                const uint32_t bcast = (ip & mask) | ~mask;
                if (std::find(out.begin(), out.end(), bcast) == out.end()) out.push_back(bcast);
            }
        }
    }
    std::lock_guard<std::mutex> lock(g_mu);
    g_broadcasts = std::move(out);
}

std::vector<uint8_t> BuildPresence() {
    Writer w(Msg::Presence);
    w.U64(g_local.steamId);
    w.U16(g_ucastPort);
    w.U32(g_local.appId);
    w.U8((uint8_t)g_local.avatarKind);
    w.U32(g_local.avatarKey);
    w.U64(g_local.lobbyId);
    w.U8(g_local.joinable ? 1 : 0);
    w.U8(g_local.members);
    w.U8(g_local.maxMembers);
    w.Str8(g_local.name);
    w.Str8(g_local.game);
    w.Str16(g_local.connect);
    return w.Data();
}

void OnPresence(Reader& r, uint32_t fromIp) {
    RemotePlayer p;
    p.steamId = r.U64();
    p.port = r.U16();
    p.appId = r.U32();
    p.avatarKind = (AvatarKind)r.U8();
    p.avatarKey = r.U32();
    p.lobbyId = r.U64();
    p.joinable = (r.U8() & 1) != 0;
    p.members = r.U8();
    p.maxMembers = r.U8();
    p.name = r.Str8();
    p.game = r.Str8();
    p.connect = r.Str16();
    if (!r.Ok() || p.steamId == 0 || p.name.empty()) return;
    p.ip = fromIp;
    p.lastSeen = GetTickCount64();
    std::lock_guard<std::mutex> lock(g_mu);
    if (p.steamId == g_local.steamId && p.port == g_ucastPort) return;
    uint64_t playerKey = p.steamId;
    if (playerKey == g_local.steamId && p.port != g_ucastPort) {
        playerKey += p.port;
        p.steamId = playerKey;
    }
    g_players[playerKey] = std::move(p);
}

void OnAvatarReq(Reader& r, uint32_t fromIp) {
    r.U64();                                  // requester (informational)
    const uint64_t owner = r.U64();
    const uint32_t key = r.U32();
    const uint16_t replyPort = r.U16();
    std::vector<std::vector<uint8_t>> packets;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (!r.Ok() || owner != g_local.steamId || key != g_avatarKey || g_avatarJpeg.empty()) return;
        const size_t count = (g_avatarJpeg.size() + kChunkBytes - 1) / kChunkBytes;
        for (size_t i = 0; i < count; ++i) {
            Writer w(Msg::AvatarChunk);
            w.U64(owner);
            w.U32(key);
            w.U32((uint32_t)g_avatarJpeg.size());
            w.U16((uint16_t)i);
            const size_t off = i * kChunkBytes;
            w.Raw(g_avatarJpeg.data() + off, std::min(kChunkBytes, g_avatarJpeg.size() - off));
            packets.push_back(w.Data());
        }
    }
    for (const auto& pkt : packets) SendTo(g_ucast, fromIp, replyPort, pkt);
}

void OnAvatarChunk(Reader& r) {
    const uint64_t owner = r.U64();
    const uint32_t key = r.U32();
    const uint32_t total = r.U32();
    const uint16_t index = r.U16();
    size_t n = 0;
    const uint8_t* bytes = r.Rest(n);
    if (!r.Ok() || total == 0 || total > kMaxAvatarBytes || n == 0) return;
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_downloads.find(owner);
    if (it == g_downloads.end() || it->second.key != key || it->second.complete) return;
    Download& d = it->second;
    const size_t count = (total + kChunkBytes - 1) / kChunkBytes;
    if (d.total != total) {
        d.total = total;
        d.data.assign(total, 0);
        d.got.assign(count, false);
        d.received = 0;
    }
    const size_t off = (size_t)index * kChunkBytes;
    if (index >= count || d.got[index] || off + n > total) return;
    memcpy(d.data.data() + off, bytes, n);
    d.got[index] = true;
    if (++d.received == count) d.complete = Crc32(d.data.data(), d.data.size()) == key;
    if (d.received == count && !d.complete) d.total = 0;   // corrupted: start over
}

void OnInvite(Reader& r, uint32_t fromIp) {
    Invite inv;
    inv.id = r.U32();
    inv.from = r.U64();
    const uint64_t to = r.U64();
    inv.appId = r.U32();
    inv.lobbyId = r.U64();
    inv.connect = r.Str16();
    const uint16_t replyPort = r.U16();
    if (!r.Ok()) return;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (to != g_local.steamId) return;
        if (g_seenInvites.insert({ inv.from, inv.id }).second) g_inbox.push_back(inv);
    }
    Writer ack(Msg::InviteAck);
    ack.U32(inv.id);
    ack.U64(to);
    SendTo(g_ucast, fromIp, replyPort, ack.Data());
}

void OnInviteAck(Reader& r) {
    const uint32_t id = r.U32();
    const uint64_t from = r.U64();
    if (!r.Ok()) return;
    std::lock_guard<std::mutex> lock(g_mu);
    g_outbox.erase(std::remove_if(g_outbox.begin(), g_outbox.end(),
                                  [&](const PendingInvite& p) { return p.invite.id == id && p.to == from; }),
                   g_outbox.end());
}

void Handle(const uint8_t* buf, size_t n, uint32_t ip) {
    if (n < 6) return;
    Reader r(buf, n);
    if (r.U32() != kMagic || r.U8() != kVersion) return;
    switch ((Msg)r.U8()) {
    case Msg::Presence: OnPresence(r, ip); break;
    case Msg::AvatarReq: OnAvatarReq(r, ip); break;
    case Msg::AvatarChunk: OnAvatarChunk(r); break;
    case Msg::Invite: OnInvite(r, ip); break;
    case Msg::InviteAck: OnInviteAck(r); break;
    default: break;
    }
}

void Receive(SOCKET s) {
    uint8_t buf[2048];
    sockaddr_in from{};
    int fromLen = sizeof(from);
    const int n = recvfrom(s, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
    if (n > 0) Handle(buf, (size_t)n, ntohl(from.sin_addr.s_addr));
}

#if REFIX_STEAM_LINK
// Datagrams from Steam friends arrive on the link thread; the net thread handles them like socket input.
std::mutex g_tunnelMu;
std::vector<std::pair<uint32_t, std::vector<uint8_t>>> g_tunnelIn;

void TunnelSink(void*, uint32_t fromIp, uint16_t, uint16_t dstPort, const uint8_t* data, size_t len) {
    if (dstPort != kPresencePort && dstPort != g_ucastPort) return;
    std::lock_guard<std::mutex> lock(g_tunnelMu);
    if (g_tunnelIn.size() < 1024) g_tunnelIn.emplace_back(fromIp, std::vector<uint8_t>(data, data + len));
}

void DrainTunnel() {
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> batch;
    {
        std::lock_guard<std::mutex> lock(g_tunnelMu);
        batch.swap(g_tunnelIn);
    }
    for (auto& [ip, data] : batch) Handle(data.data(), data.size(), ip);
}
#endif

void Housekeeping(ULONGLONG now, ULONGLONG& lastAnnounce, ULONGLONG& lastAdapters) {
    if (now - lastAdapters >= kAdapterRefreshMs) {
        RefreshBroadcasts();
        lastAdapters = now;
    }
    std::vector<uint8_t> presence;
    std::vector<uint32_t> targets;
    std::vector<std::pair<RemotePlayer, std::vector<uint8_t>>> resend;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_local.steamId != 0 && (g_localDirty || now - lastAnnounce >= kAnnounceMs)) {
            presence = BuildPresence();
            targets = g_broadcasts;
            g_localDirty = false;
            lastAnnounce = now;
        }
        for (auto it = g_players.begin(); it != g_players.end();) {
            it = now - it->second.lastSeen > kStaleMs ? g_players.erase(it) : std::next(it);
        }
        for (auto& p : g_outbox) {
            if (now < p.next) continue;
            auto target = g_players.find(p.to);
            if (target == g_players.end()) continue;
            Writer w(Msg::Invite);
            w.U32(p.invite.id);
            w.U64(p.invite.from);
            w.U64(p.to);
            w.U32(p.invite.appId);
            w.U64(p.invite.lobbyId);
            w.Str16(p.invite.connect);
            w.U16(g_ucastPort);
            resend.push_back({ target->second, w.Data() });
            ++p.tries;
            p.next = now + kInviteRetryMs;
        }
        g_outbox.erase(std::remove_if(g_outbox.begin(), g_outbox.end(), [](const PendingInvite& p) { return p.tries >= kInviteTries; }),
                       g_outbox.end());
    }
    for (uint32_t ip : targets) {
        SendTo(g_bcast, ip, kPresencePort, presence);
        if (ip == 0x7F000001u || ip == 0xFFFFFFFFu) {
            for (uint16_t alt = 47691; alt <= 47698; ++alt) {
                SendTo(g_bcast, ip, alt, presence);
            }
        }
    }
#if REFIX_STEAM_LINK
    if (!presence.empty()) steamlink::Broadcast(g_ucastPort, kPresencePort, presence.data(), presence.size());
#endif
    for (const auto& [player, pkt] : resend) SendTo(g_ucast, player.ip, player.port, pkt);
}

DWORD WINAPI NetThread(LPVOID) {
    ULONGLONG lastAnnounce = 0, lastAdapters = 0;
    for (;;) {
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(g_bcast, &rd);
        FD_SET(g_ucast, &rd);
        timeval tv{ 0, kPollMs * 1000 };
        if (select(0, &rd, nullptr, nullptr, &tv) > 0) {
            if (FD_ISSET(g_bcast, &rd)) Receive(g_bcast);
            if (FD_ISSET(g_ucast, &rd)) Receive(g_ucast);
        }
#if REFIX_STEAM_LINK
        DrainTunnel();
#endif
        Housekeeping(GetTickCount64(), lastAnnounce, lastAdapters);
    }
}

SOCKET OpenUdp(uint16_t port, bool shared) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return s;
    SteamP2PHook::RegisterInternalSocket((uintptr_t)s);
    BOOL on = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(on));
    // Only allow address reuse on ephemeral ports (port == 0).
    // For fixed listener ports (kPresencePort 47690), each local instance must bind a unique
    // port (47690, 47691, etc.) so Windows UDP delivers inbound packets to all instances reliably.
    if (shared && port == 0) setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, (const sockaddr*)&addr, sizeof(addr)) != 0) {
        SteamP2PHook::UnregisterInternalSocket((uintptr_t)s);
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

} // namespace

uint32_t Crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

bool Start() {
    bool expected = false;
    if (!g_started.compare_exchange_strong(expected, true)) return true;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    g_bcast = OpenUdp(kPresencePort, true);
    if (g_bcast == INVALID_SOCKET) {
        for (uint16_t alt = 47691; alt <= 47698; ++alt) {
            g_bcast = OpenUdp(alt, true);
            if (g_bcast != INVALID_SOCKET) break;
        }
    }
    g_ucast = OpenUdp(0, false);
    if (g_bcast == INVALID_SOCKET || g_ucast == INVALID_SOCKET) {
        ReFixLog("[OverlayNet] Could not open the presence sockets (error %d)", WSAGetLastError());
        return false;
    }
    sockaddr_in local{};
    int len = sizeof(local);
    getsockname(g_ucast, (sockaddr*)&local, &len);
    g_ucastPort = ntohs(local.sin_port);
    RefreshBroadcasts();
#if REFIX_STEAM_LINK
    steamlink::AddSink(&TunnelSink, nullptr);
#endif
    HANDLE t = CreateThread(nullptr, 0, NetThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
    ReFixLog("[OverlayNet] Presence on UDP %u, unicast port %u", kPresencePort, g_ucastPort);
    return t != nullptr;
}

void SetLocal(const Presence& p) {
    std::lock_guard<std::mutex> lock(g_mu);
    const bool changed = p.name != g_local.name || p.lobbyId != g_local.lobbyId || p.joinable != g_local.joinable ||
                         p.avatarKey != g_local.avatarKey || p.avatarKind != g_local.avatarKind ||
                         p.members != g_local.members || p.connect != g_local.connect || p.game != g_local.game;
    g_local = p;
    if (changed) g_localDirty = true;
}

void SetLocalAvatar(uint32_t key, std::vector<uint8_t> jpeg) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_avatarKey = key;
    g_avatarJpeg = std::move(jpeg);
}

std::vector<RemotePlayer> Players() {
    std::lock_guard<std::mutex> lock(g_mu);
    std::vector<RemotePlayer> out;
    out.reserve(g_players.size() + 8);
    for (const auto& kv : g_players) out.push_back(kv.second);

    // Merge peers discovered by ILanCore
    auto allPeers = refix::lan::ILanCore::Get().Peers().GetAllPeers();
    for (const auto& p : allPeers) {
        uint64_t sid = p.externalId.numericId;
        if (sid == 0 || sid == g_local.steamId) continue; // P1.a: forbid peerId.low as SteamID fallback
        if (g_players.count(sid) == 0) {
            RemotePlayer rp;
            rp.steamId = sid;
            rp.name = p.displayName;
            rp.game = !g_local.game.empty() ? g_local.game : "LAN Game";
            rp.appId = g_local.appId != 0 ? g_local.appId : 480;
            rp.joinable = true;
            rp.ip = p.endpoint.ipv4;
            rp.port = p.endpoint.port != 0 ? p.endpoint.port : 47585;
            rp.lastSeen = GetTickCount64();
            out.push_back(rp);
        }
    }
    return out;
}

bool SendInvite(uint64_t to, uint64_t lobbyId, const std::string& connect) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_players.count(to)) {
        auto pid = refix::lan::ILanCore::Get().Peers().FindBySteamId(to);
        if (pid) {
            auto pinfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(*pid);
            if (pinfo) {
                RemotePlayer rp;
                rp.steamId = to;
                rp.name = pinfo->displayName;
                rp.game = !g_local.game.empty() ? g_local.game : "LAN Game";
                rp.appId = g_local.appId != 0 ? g_local.appId : 480;
                rp.joinable = true;
                rp.ip = pinfo->endpoint.ipv4;
                rp.port = pinfo->endpoint.port != 0 ? pinfo->endpoint.port : 47585;
                rp.lastSeen = GetTickCount64();
                g_players[to] = rp;
            }
        }
    }
    if (!g_players.count(to)) return false;
    PendingInvite p;
    p.invite.id = g_nextInviteId++;
    p.invite.from = g_local.steamId;
    p.invite.appId = g_local.appId;
    p.invite.lobbyId = lobbyId;
    p.invite.connect = connect.substr(0, kMaxConnect);
    p.to = to;
    g_outbox.push_back(std::move(p));
    return true;
}

std::vector<Invite> TakeInvites() {
    std::lock_guard<std::mutex> lock(g_mu);
    std::vector<Invite> out;
    out.swap(g_inbox);
    return out;
}

bool FetchAvatar(uint64_t steamId, uint32_t key, std::vector<uint8_t>& jpeg) {
    std::vector<uint8_t> request;
    RemotePlayer owner;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        Download& d = g_downloads[steamId];
        if (d.key != key) d = Download{ key };
        if (d.complete) { jpeg = d.data; return true; }
        auto it = g_players.find(steamId);
        const ULONGLONG now = GetTickCount64();
        if (it == g_players.end() || now - d.lastRequest < kAvatarRetryMs) return false;
        d.lastRequest = now;
        owner = it->second;
        Writer w(Msg::AvatarReq);
        w.U64(g_local.steamId);
        w.U64(steamId);
        w.U32(key);
        w.U16(g_ucastPort);
        request = w.Data();
    }
    SendTo(g_ucast, owner.ip, owner.port, request);
    return false;
}

} // namespace refix::overlay::net
