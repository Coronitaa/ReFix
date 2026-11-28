// refix_steamhost(64).exe: the Steam side of ReFix's optional Steam online services (steam_host_proto.h).
// Started by ReFix in the game with: <game port> <token> <game pid> <folder of refix_steamlink DLL>.
// Opens Steam if needed, signs in as Spacewar (480) through Valve's steam_api, and relays the friends list,
// rich presence, avatars and the P2P tunnel (ISteamNetworkingMessages channel 47) to the game over loopback.
// Exits when the game asks, or when the game process is gone, so Steam stops showing Spacewar.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include "../refix_wine.h"

#include "steam_host_proto.h"

#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "../include/steam/steam_api.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

using namespace refix::steamlink::proto;

namespace {

// --- Valve flat API (leading fields of the SDK structs, stable since SDK 1.48) ----------------------------
#pragma pack(push, 1)
struct NetIdentity { int32_t type; int32_t size; union { uint64_t steamId; char raw[128]; }; };
#pragma pack(pop)
static_assert(sizeof(NetIdentity) == 136, "SteamNetworkingIdentity layout");
struct NetMessage { void* data; int32_t size; uint32_t conn; NetIdentity peer; };
static_assert(offsetof(NetMessage, peer) == (sizeof(void*) == 8 ? 16 : 12), "NetMessage peer offset");
struct SessionFailedInfo { NetIdentity remote; char rest[696 - sizeof(NetIdentity)]; };
static_assert(sizeof(SessionFailedInfo) == 696, "SteamNetworkingMessagesSessionFailed_t layout");
struct FriendGameInfo { uint64_t gameId; uint32_t ip; uint16_t port, queryPort; uint64_t lobby; };

constexpr int kChannel = 47;
constexpr int kSendFlags = 1 | 32;                 // UnreliableNoNagle | AutoRestartBrokenSession
constexpr int kCbSessionRequest = 1251;
constexpr int kCbSessionFailed = 1252;
constexpr int kResultOk = 1;
constexpr ULONGLONG kDiagMs = 5000;
constexpr int kFriendFlagImmediate = 4;
constexpr DWORD kSteamStartTimeoutMs = 180000;
constexpr DWORD kInitRetryMs = 2000;
constexpr DWORD kActiveUserGraceMs = 4000;  // Steam running this long without ActiveUser: try SteamAPI_Init anyway
constexpr ULONGLONG kFriendRefreshMs = 3000;
constexpr size_t kFriendBytes = 60000;             // one datagram

struct Api {
    HMODULE dll = nullptr;
    bool (*Init)() = nullptr;
    void (*Shutdown)() = nullptr;
    void (*RunCallbacks)() = nullptr;
    void (*RegisterCallback)(void*, int) = nullptr;
    void (*UnregisterCallback)(void*) = nullptr;
    void* (*Messages)() = nullptr;
    void* (*FriendsIface)() = nullptr;
    void* (*UserIface)() = nullptr;
    int (*SendMsg)(void*, const NetIdentity*, const void*, uint32_t, int, int) = nullptr;
    int (*RecvMsgs)(void*, int, NetMessage**, int) = nullptr;
    bool (*AcceptSession)(void*, const NetIdentity*) = nullptr;
    void (*Release)(NetMessage*) = nullptr;
    uint64_t (*GetSteamID)(void*) = nullptr;
    int (*FriendCount)(void*, int) = nullptr;
    uint64_t (*FriendByIndex)(void*, int, int) = nullptr;
    const char* (*FriendName)(void*, uint64_t) = nullptr;
    int (*FriendState)(void*, uint64_t) = nullptr;
    bool (*FriendGame)(void*, uint64_t, FriendGameInfo*) = nullptr;
    const char* (*FriendRP)(void*, uint64_t, const char*) = nullptr;
    bool (*SetRP)(void*, const char*, const char*) = nullptr;
    bool (*RequestRP)(void*, uint64_t) = nullptr;
    bool (*HasFriend)(void*, uint64_t, int) = nullptr;
    int (*MediumAvatar)(void*, uint64_t) = nullptr;
    void* (*UtilsIface)() = nullptr;
    bool (*ImageSize)(void*, int, uint32_t*, uint32_t*) = nullptr;
    bool (*ImageRgba)(void*, int, uint8_t*, int) = nullptr;
    void* (*AppsIface)() = nullptr;
    bool (*IsSubscribedApp)(void*, uint32_t) = nullptr;
    uint32_t (*GetAuthTicket)(void*, void*, int, uint32_t*, const void*) = nullptr;
    uint32_t (*GetAuthTicketWebApi)(void*, const char*) = nullptr;
    void (*CancelAuthTicket)(void*, uint32_t) = nullptr;
    uint64_t (*RequestEncryptedAppTicket)(void*, void*, int) = nullptr;
    bool (*GetEncryptedAppTicket)(void*, void*, int, uint32_t*) = nullptr;
    void (*RegisterCallResult)(void*, uint64_t) = nullptr;
    void (*UnregisterCallResult)(void*, uint64_t) = nullptr;
    void* (*MatchmakingIface)() = nullptr;
    uint64_t (*RequestLobbyList)(void*) = nullptr;
    void (*AddLobbyStrFilter)(void*, const char*, const char*, int) = nullptr;
    void (*AddLobbyNumFilter)(void*, const char*, int, int) = nullptr;
    void (*AddLobbyNearFilter)(void*, const char*, int) = nullptr;
    void (*AddLobbySlotsFilter)(void*, int) = nullptr;
    void (*AddLobbyDistFilter)(void*, int) = nullptr;
    void (*AddLobbyCountFilter)(void*, int) = nullptr;
    void (*AddLobbyCompatFilter)(void*, uint64_t) = nullptr;
    uint64_t (*GetLobbyByIndex)(void*, int) = nullptr;
    uint64_t (*CreateLobby)(void*, int, int) = nullptr;
    uint64_t (*JoinLobby)(void*, uint64_t) = nullptr;
    void (*LeaveLobby)(void*, uint64_t) = nullptr;
    uint64_t (*GetLobbyOwner)(void*, uint64_t) = nullptr;
    int (*GetNumLobbyMembers)(void*, uint64_t) = nullptr;
    uint64_t (*GetLobbyMemberByIndex)(void*, uint64_t, int) = nullptr;
    int (*GetLobbyDataCount)(void*, uint64_t) = nullptr;
    bool (*GetLobbyDataByIndex)(void*, uint64_t, int, char*, int, char*, int) = nullptr;
    bool (*SetLobbyData)(void*, uint64_t, const char*, const char*) = nullptr;
    bool (*DeleteLobbyData)(void*, uint64_t, const char*) = nullptr;
    bool (*SetLobbyType)(void*, uint64_t, int) = nullptr;
    bool (*SetLobbyJoinable)(void*, uint64_t, bool) = nullptr;
    bool (*SetLobbyMemberLimit)(void*, uint64_t, int) = nullptr;
    int (*GetLobbyMemberLimit)(void*, uint64_t) = nullptr;
    bool (*SetLobbyOwner)(void*, uint64_t, uint64_t) = nullptr;
    void (*SetLobbyGameServer)(void*, uint64_t, uint32_t, uint16_t, uint64_t) = nullptr;
    bool (*GetLobbyGameServer)(void*, uint64_t, uint32_t*, uint16_t*, uint64_t*) = nullptr;
    bool (*RequestLobbyData)(void*, uint64_t) = nullptr;
    void* (*UserStatsIface)() = nullptr;
    bool (*RequestCurrentStats)(void*) = nullptr;
    bool (*GetAchievement)(void*, const char*, bool*) = nullptr;
    bool (*SetAchievement)(void*, const char*) = nullptr;
    bool (*ClearAchievement)(void*, const char*) = nullptr;
    bool (*IndicateProgress)(void*, const char*, uint32_t, uint32_t) = nullptr;
    bool (*GetStatInt32)(void*, const char*, int32_t*) = nullptr;
    bool (*GetStatFloat)(void*, const char*, float*) = nullptr;
    bool (*SetStatInt32)(void*, const char*, int32_t) = nullptr;
    bool (*SetStatFloat)(void*, const char*, float) = nullptr;
    bool (*StoreStats)(void*) = nullptr;
    bool (*ResetAllStats)(void*, bool) = nullptr;
    // Optional (diagnostics and relay warm-up): older Steam API DLLs may lack them.
    void* (*NetUtils)() = nullptr;
    int (*InitRelay)(void*) = nullptr;
    int (*RelayStatus)(void*, void*) = nullptr;
    int (*SessionInfo)(void*, const NetIdentity*, void*, void*) = nullptr;
};

// SteamRelayNetworkStatus_t: availability of the relays, then Steam's own explanation.
struct RelayStatusInfo { int avail; int pinging; int availConfig; int availRelay; char debug[256]; };

// Per friend, what the tunnel did: the two-PC diagnosis reads these lines in ReFix.log.
struct PeerStats { uint32_t sent = 0, failed = 0, received = 0; int lastResult = 0; uint32_t logged = 0; };

Api g_api;
void* g_msgs = nullptr;
void* g_friends = nullptr;
void* g_user = nullptr;
void* g_utils = nullptr;
void* g_apps = nullptr;
void* g_matchmaking = nullptr;
void* g_userStats = nullptr;
bool g_maskMode = false;
bool g_statsReady = false;
ULONGLONG g_statsInitAt = 0;
struct PendingStatOp {
    uint8_t kind = 0;
    std::string name;
    uint32_t cur = 0, max = 0;
    int32_t iVal = 0;
    float fVal = 0.0f;
    bool bVal = false;
};
std::vector<PendingStatOp> g_pendingStatOps;
std::unordered_map<std::string, bool> g_sentUnlocked;
std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> g_sentProgress;
std::unordered_map<std::string, int32_t> g_sentIntStats;
std::unordered_map<std::string, float> g_sentFloatStats;
bool g_hostStatsDirty = false;
struct PendingAuthTicket { uint32_t reqId = 0; bool webApi = false; std::vector<uint8_t> bytes; };
std::map<uint32_t, PendingAuthTicket> g_pendingAuth;
SOCKET g_sock = INVALID_SOCKET;
sockaddr_in g_game{};
HANDLE g_gameProcess = nullptr;
bool g_quit = false;
bool g_startedSteam = false;                       // Steam was closed and this helper opened it
constexpr ULONGLONG kKeepSteamInTrayMs = 20000;    // Steam opens its window a few seconds after the login
uint32_t g_bridgeAppId = 480;
uint32_t g_appId = 0;
std::vector<uint32_t> g_altAppIds;
std::string g_gameName;
uint64_t g_self = 0;
uint32_t g_initialAccountId = 0;

void Post(const Writer& w) {
    if (w.b.size() <= kMaxDatagram) sendto(g_sock, (const char*)w.b.data(), (int)w.b.size(), 0, (const sockaddr*)&g_game, sizeof(g_game));
}

void Log(const char* fmt, ...) {
    char line[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    Post(Writer(kLog).Str(line));
}

std::map<uint64_t, PeerStats> g_peers;
void* g_netUtils = nullptr;

void Report(Step step, const char* error = "") {
    Post(Writer(kState).Put<uint8_t>(step).Put<uint64_t>(g_self).Str(error).Put<uint32_t>(g_bridgeAppId));
}

NetIdentity IdentityOf(uint64_t id) {
    NetIdentity n{};
    n.type = 16;
    n.size = 8;
    n.steamId = id;
    return n;
}

void PublishPresence(bool on) {
    if (!g_friends) return;
    g_api.SetRP(g_friends, "refix", on ? "1" : "");
    g_api.SetRP(g_friends, "refix_app", on ? std::to_string(g_appId).c_str() : "");
    g_api.SetRP(g_friends, "refix_game", on ? g_gameName.substr(0, 120).c_str() : "");
}

void SendAvatar(int image) {
    uint32_t w = 0, h = 0;
    if (!g_utils || image <= 0 || !g_api.ImageSize(g_utils, image, &w, &h) || !w || !h || w > 120 || h > 120) return;
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    if (!g_api.ImageRgba(g_utils, image, rgba.data(), (int)rgba.size())) return;
    Post(Writer(kAvatarData).Put<int32_t>(image).Put<uint16_t>((uint16_t)w).Put<uint16_t>((uint16_t)h).Raw(rgba.data(), rgba.size()));
}

struct LobbyStrFilter { std::string key; std::string val; int cmp = 0; };
struct LobbyNumFilter { std::string key; int val = 0; int cmp = 0; };
struct LobbyNearFilter { std::string key; int val = 0; };
void SendLobbySnapshot(uint64_t lobbyId, uint32_t reqId);
void StartLobbySearchCall(uint32_t reqId, uint64_t hCall);
void StartLobbyCreateCall(uint32_t reqId, uint64_t hCall);
void StartLobbyJoinCall(uint32_t reqId, uint64_t hCall);
void StartEncryptedTicketCall(uint32_t reqId, uint64_t hCall);
void CleanupLobbyCalls();

void ApplyStatOp(const PendingStatOp& op) {
    if (!g_userStats) return;
    auto isUnlockedInSteam = [&](const std::string& name) -> bool {
        auto it = g_sentUnlocked.find(name);
        if (it != g_sentUnlocked.end() && it->second) return true;
        bool achieved = false;
        if (g_api.GetAchievement && g_api.GetAchievement(g_userStats, name.c_str(), &achieved) && achieved) {
            g_sentUnlocked[name] = true;
            return true;
        }
        return false;
    };

    switch (op.kind) {
    case kStatsSetAchievement: {
        if (isUnlockedInSteam(op.name)) break;
        if (g_api.SetAchievement) {
            const bool ok = g_api.SetAchievement(g_userStats, op.name.c_str());
            Log("Mask/Official SetAchievement('%s') -> %d", op.name.c_str(), ok ? 1 : 0);
            if (ok) {
                g_sentUnlocked[op.name] = true;
                g_hostStatsDirty = true;
            }
        }
        break;
    }
    case kStatsClearAchievement: {
        auto it = g_sentUnlocked.find(op.name);
        bool achieved = (it != g_sentUnlocked.end()) ? it->second : false;
        if (it == g_sentUnlocked.end() && g_api.GetAchievement) {
            if (!g_api.GetAchievement(g_userStats, op.name.c_str(), &achieved)) achieved = true;
        }
        if (!achieved) break;
        if (g_api.ClearAchievement && g_api.ClearAchievement(g_userStats, op.name.c_str())) {
            g_sentUnlocked[op.name] = false;
            g_sentProgress.erase(op.name);
            g_hostStatsDirty = true;
        }
        break;
    }
    case kStatsIndicateProgress: {
        if (isUnlockedInSteam(op.name)) break;
        auto pit = g_sentProgress.find(op.name);
        if (pit != g_sentProgress.end() && pit->second.first == op.cur && pit->second.second == op.max) break;
        g_sentProgress[op.name] = { op.cur, op.max };
        if (g_api.IndicateProgress) {
            const bool ok = g_api.IndicateProgress(g_userStats, op.name.c_str(), op.cur, op.max);
            Log("Mask/Official IndicateProgress('%s', %u/%u) -> %d", op.name.c_str(), op.cur, op.max, ok ? 1 : 0);
            if (ok) g_hostStatsDirty = true;
        }
        break;
    }
    case kStatsSetInt: {
        auto iit = g_sentIntStats.find(op.name);
        if (iit != g_sentIntStats.end() && iit->second == op.iVal) break;
        int32_t curVal = 0;
        if (iit == g_sentIntStats.end() && g_api.GetStatInt32 && g_api.GetStatInt32(g_userStats, op.name.c_str(), &curVal) && curVal == op.iVal) {
            g_sentIntStats[op.name] = op.iVal;
            break;
        }
        g_sentIntStats[op.name] = op.iVal;
        if (g_api.SetStatInt32 && g_api.SetStatInt32(g_userStats, op.name.c_str(), op.iVal)) {
            g_hostStatsDirty = true;
        }
        break;
    }
    case kStatsSetFloat: {
        auto fit = g_sentFloatStats.find(op.name);
        if (fit != g_sentFloatStats.end() && fit->second == op.fVal) break;
        float curVal = 0.0f;
        if (fit == g_sentFloatStats.end() && g_api.GetStatFloat && g_api.GetStatFloat(g_userStats, op.name.c_str(), &curVal) && curVal == op.fVal) {
            g_sentFloatStats[op.name] = op.fVal;
            break;
        }
        g_sentFloatStats[op.name] = op.fVal;
        if (g_api.SetStatFloat && g_api.SetStatFloat(g_userStats, op.name.c_str(), op.fVal)) {
            g_hostStatsDirty = true;
        }
        break;
    }
    case kStatsStore:
        if (!g_hostStatsDirty) break;
        if (g_api.StoreStats) {
            const bool ok = g_api.StoreStats(g_userStats);
            Log("Mask/Official StoreStats() -> %d", ok ? 1 : 0);
            if (ok) g_hostStatsDirty = false;
        }
        break;
    case kStatsResetAll:
        g_sentIntStats.clear();
        g_sentFloatStats.clear();
        g_sentProgress.clear();
        if (op.bVal) g_sentUnlocked.clear();
        if (g_api.ResetAllStats && g_api.ResetAllStats(g_userStats, op.bVal)) {
            g_hostStatsDirty = true;
        }
        break;
    default: break;
    }
}

void FlushPendingStatOps() {
    if (!g_statsReady && g_statsInitAt != 0 && GetTickCount64() - g_statsInitAt >= 1500) {
        g_statsReady = true;
    }
    if (!g_statsReady || g_pendingStatOps.empty()) return;
    auto ops = std::move(g_pendingStatOps);
    g_pendingStatOps.clear();
    for (const auto& op : ops) ApplyStatOp(op);
}

void QueueOrApplyStatOp(PendingStatOp op) {
    if (g_statsReady && g_userStats) {
        ApplyStatOp(op);
        return;
    }
    if (op.kind == kStatsStore && !g_pendingStatOps.empty() && g_pendingStatOps.back().kind == kStatsStore) {
        return;   // Coalesce consecutive StoreStats while waiting for UserStatsReceived
    }
    for (const auto& prev : g_pendingStatOps) {
        if (prev.kind != op.kind || prev.name != op.name) continue;
        if (op.kind == kStatsSetAchievement || op.kind == kStatsClearAchievement) return;
        if (op.kind == kStatsIndicateProgress && prev.cur == op.cur && prev.max == op.max) return;
        if (op.kind == kStatsSetInt && prev.iVal == op.iVal) return;
        if (op.kind == kStatsSetFloat && prev.fVal == op.fVal) return;
    }
    if (g_pendingStatOps.size() < 256) g_pendingStatOps.push_back(std::move(op));
}

// Messages from the game; anything from another socket is ignored.
void HandleGame() {
    static std::vector<uint8_t> buf(kMaxDatagram + 16);
    for (;;) {
        sockaddr_in from{};
        int fromLen = sizeof(from);
        const int n = recvfrom(g_sock, (char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fromLen);
        if (n <= 0) return;
        if (from.sin_addr.s_addr != g_game.sin_addr.s_addr || from.sin_port != g_game.sin_port) continue;
        Reader r(buf.data() + 1, (size_t)n - 1);
        switch (buf[0]) {
        case kSend: {
            const uint64_t to = r.Get<uint64_t>();
            if (!r.ok || !r.Left() || !g_msgs) break;
            const NetIdentity id = IdentityOf(to);
            const int result = g_api.SendMsg(g_msgs, &id, r.p, (uint32_t)r.Left(), kSendFlags, kChannel);
            PeerStats& st = g_peers[to];
            ++st.sent;
            if (result != kResultOk) {
                if (!st.failed) Log("send to %llu failed: EResult %d", (unsigned long long)to, result);
                ++st.failed;
                st.lastResult = result;
            }
            break;
        }
        case kPresence:
            g_appId = r.Get<uint32_t>();
            g_gameName = r.Str();
            g_altAppIds.clear();
            while (r.Left() >= 4) {
                g_altAppIds.push_back(r.Get<uint32_t>());
            }
            if (r.ok) PublishPresence(true);
            break;
        case kAvatar: SendAvatar(r.Get<int32_t>()); break;
        case kCheckApp: {
            const uint32_t checkId = r.Get<uint32_t>();
            if (r.ok && checkId != 0 && g_apps && g_api.IsSubscribedApp) {
                const bool owned = g_api.IsSubscribedApp(g_apps, checkId);
                Post(Writer(kAppLicense).Put<uint32_t>(checkId).Put<uint8_t>(owned ? 1 : 0));
            }
            break;
        }
        case kRequestAuthTicket: {
            const uint32_t reqId = r.Get<uint32_t>();
            const bool webApi = r.Get<uint8_t>() != 0;
            const std::string identity = r.Str();
            if (!r.ok || !g_user) {
                Post(Writer(kAuthTicketData).Put<uint32_t>(reqId).Put<uint32_t>(0).Put<int32_t>(2).Put<uint32_t>(0));
                break;
            }
            if (webApi && g_api.GetAuthTicketWebApi) {
                const uint32_t h = g_api.GetAuthTicketWebApi(g_user, identity.empty() ? nullptr : identity.c_str());
                if (h) g_pendingAuth[h] = PendingAuthTicket{ reqId, true, {} };
                else Post(Writer(kAuthTicketData).Put<uint32_t>(reqId).Put<uint32_t>(0).Put<int32_t>(2).Put<uint32_t>(0));
            } else if (!webApi && g_api.GetAuthTicket) {
                std::vector<uint8_t> ticket(1024);
                uint32_t len = 0;
                const uint32_t h = g_api.GetAuthTicket(g_user, ticket.data(), (int)ticket.size(), &len, nullptr);
                if (h && len > 0 && len <= ticket.size()) {
                    ticket.resize(len);
                    g_pendingAuth[h] = PendingAuthTicket{ reqId, false, std::move(ticket) };
                } else {
                    Post(Writer(kAuthTicketData).Put<uint32_t>(reqId).Put<uint32_t>(0).Put<int32_t>(2).Put<uint32_t>(0));
                }
            } else {
                Post(Writer(kAuthTicketData).Put<uint32_t>(reqId).Put<uint32_t>(0).Put<int32_t>(2).Put<uint32_t>(0));
            }
            break;
        }
        case kCancelAuthTicket: {
            const uint32_t h = r.Get<uint32_t>();
            if (r.ok && h && g_user && g_api.CancelAuthTicket) {
                g_api.CancelAuthTicket(g_user, h);
                g_pendingAuth.erase(h);
            }
            break;
        }
        case kRequestEncryptedAppTicket: {
            const uint32_t reqId = r.Get<uint32_t>();
            const uint32_t dataLen = r.Get<uint32_t>();
            if (!r.ok || r.Left() < dataLen || dataLen > 4096 || !g_user || !g_api.RequestEncryptedAppTicket ||
                !g_api.GetEncryptedAppTicket || !g_api.RegisterCallResult) {
                Post(Writer(kEncryptedAppTicketData).Put<uint32_t>(reqId).Put<int32_t>(2).Put<uint32_t>(0));
                break;
            }
            std::vector<uint8_t> incl;
            if (dataLen > 0) incl.assign(r.p, r.p + dataLen);
            const uint64_t hCall = g_api.RequestEncryptedAppTicket(
                g_user, incl.empty() ? nullptr : incl.data(), (int)incl.size());
            if (hCall) StartEncryptedTicketCall(reqId, hCall);
            else Post(Writer(kEncryptedAppTicketData).Put<uint32_t>(reqId).Put<int32_t>(2).Put<uint32_t>(0));
            break;
        }
        case kLobbySearch: {
            const uint32_t reqId = r.Get<uint32_t>();
            const uint32_t slotsAvail = r.Get<uint32_t>();
            const int32_t distFilter = r.Get<int32_t>();
            const uint32_t maxResults = r.Get<uint32_t>();
            const uint64_t compatLobby = r.Get<uint64_t>();
            const uint16_t strN = r.Get<uint16_t>();
            std::vector<LobbyStrFilter> sfs;
            for (uint16_t i = 0; i < strN && r.ok; ++i) {
                LobbyStrFilter f;
                f.key = r.Str16();
                f.val = r.Str16();
                f.cmp = r.Get<int32_t>();
                sfs.push_back(std::move(f));
            }
            const uint16_t numN = r.Get<uint16_t>();
            std::vector<LobbyNumFilter> nfs;
            for (uint16_t i = 0; i < numN && r.ok; ++i) {
                LobbyNumFilter f;
                f.key = r.Str16();
                f.val = r.Get<int32_t>();
                f.cmp = r.Get<int32_t>();
                nfs.push_back(std::move(f));
            }
            const uint16_t nearN = r.Get<uint16_t>();
            std::vector<LobbyNearFilter> nrfs;
            for (uint16_t i = 0; i < nearN && r.ok; ++i) {
                LobbyNearFilter f;
                f.key = r.Str16();
                f.val = r.Get<int32_t>();
                nrfs.push_back(std::move(f));
            }
            if (!r.ok || !g_matchmaking || !g_api.RequestLobbyList || !g_api.RegisterCallResult) {
                Post(Writer(kLobbySearchDone).Put<uint32_t>(reqId).Put<uint8_t>(1).Put<uint16_t>(0));
                break;
            }
            if (slotsAvail > 0 && g_api.AddLobbySlotsFilter) g_api.AddLobbySlotsFilter(g_matchmaking, (int)slotsAvail);
            if (distFilter >= 0 && g_api.AddLobbyDistFilter) g_api.AddLobbyDistFilter(g_matchmaking, distFilter);
            if (maxResults > 0 && g_api.AddLobbyCountFilter) g_api.AddLobbyCountFilter(g_matchmaking, (int)maxResults);
            if (compatLobby != 0 && g_api.AddLobbyCompatFilter) g_api.AddLobbyCompatFilter(g_matchmaking, compatLobby);
            if (g_api.AddLobbyStrFilter) {
                for (const auto& f : sfs) g_api.AddLobbyStrFilter(g_matchmaking, f.key.c_str(), f.val.c_str(), f.cmp);
            }
            if (g_api.AddLobbyNumFilter) {
                for (const auto& f : nfs) g_api.AddLobbyNumFilter(g_matchmaking, f.key.c_str(), f.val, f.cmp);
            }
            if (g_api.AddLobbyNearFilter) {
                for (const auto& f : nrfs) g_api.AddLobbyNearFilter(g_matchmaking, f.key.c_str(), f.val);
            }
            const uint64_t hCall = g_api.RequestLobbyList(g_matchmaking);
            if (hCall) StartLobbySearchCall(reqId, hCall);
            else Post(Writer(kLobbySearchDone).Put<uint32_t>(reqId).Put<uint8_t>(1).Put<uint16_t>(0));
            break;
        }
        case kLobbyCreate: {
            const uint32_t reqId = r.Get<uint32_t>();
            const int32_t eType = r.Get<int32_t>();
            const int32_t maxMembers = r.Get<int32_t>();
            if (!r.ok || !g_matchmaking || !g_api.CreateLobby || !g_api.RegisterCallResult) {
                Post(Writer(kLobbyCreatedRes).Put<uint32_t>(reqId).Put<int32_t>(2).Put<uint64_t>(0));
                break;
            }
            const uint64_t hCall = g_api.CreateLobby(g_matchmaking, eType, maxMembers);
            if (hCall) StartLobbyCreateCall(reqId, hCall);
            else Post(Writer(kLobbyCreatedRes).Put<uint32_t>(reqId).Put<int32_t>(2).Put<uint64_t>(0));
            break;
        }
        case kLobbyJoin: {
            const uint32_t reqId = r.Get<uint32_t>();
            const uint64_t lobbyId = r.Get<uint64_t>();
            if (!r.ok || !lobbyId || !g_matchmaking || !g_api.JoinLobby || !g_api.RegisterCallResult) {
                Post(Writer(kLobbyEnterRes).Put<uint32_t>(reqId).Put<uint64_t>(lobbyId).Put<uint32_t>(0).Put<uint8_t>(0).Put<uint32_t>(2));
                break;
            }
            const uint64_t hCall = g_api.JoinLobby(g_matchmaking, lobbyId);
            if (hCall) StartLobbyJoinCall(reqId, hCall);
            else Post(Writer(kLobbyEnterRes).Put<uint32_t>(reqId).Put<uint64_t>(lobbyId).Put<uint32_t>(0).Put<uint8_t>(0).Put<uint32_t>(2));
            break;
        }
        case kLobbyLeave: {
            const uint64_t lobbyId = r.Get<uint64_t>();
            if (r.ok && lobbyId && g_matchmaking && g_api.LeaveLobby) g_api.LeaveLobby(g_matchmaking, lobbyId);
            break;
        }
        case kLobbySetData: {
            const uint64_t lobbyId = r.Get<uint64_t>();
            const std::string key = r.Str16();
            const uint8_t del = r.Get<uint8_t>();
            const std::string val = del ? std::string() : r.Str16();
            if (r.ok && lobbyId && g_matchmaking) {
                if (del && g_api.DeleteLobbyData) g_api.DeleteLobbyData(g_matchmaking, lobbyId, key.c_str());
                else if (!del && g_api.SetLobbyData) g_api.SetLobbyData(g_matchmaking, lobbyId, key.c_str(), val.c_str());
            }
            break;
        }
        case kLobbySetType: {
            const uint64_t lobbyId = r.Get<uint64_t>();
            const int32_t eType = r.Get<int32_t>();
            if (r.ok && lobbyId && g_matchmaking && g_api.SetLobbyType) g_api.SetLobbyType(g_matchmaking, lobbyId, eType);
            break;
        }
        case kLobbySetJoinable: {
            const uint64_t lobbyId = r.Get<uint64_t>();
            const bool joinable = r.Get<uint8_t>() != 0;
            if (r.ok && lobbyId && g_matchmaking && g_api.SetLobbyJoinable) g_api.SetLobbyJoinable(g_matchmaking, lobbyId, joinable);
            break;
        }
        case kLobbySetLimit: {
            const uint64_t lobbyId = r.Get<uint64_t>();
            const int32_t limit = r.Get<int32_t>();
            if (r.ok && lobbyId && g_matchmaking && g_api.SetLobbyMemberLimit) g_api.SetLobbyMemberLimit(g_matchmaking, lobbyId, limit);
            break;
        }
        case kLobbySetOwner: {
            const uint64_t lobbyId = r.Get<uint64_t>();
            const uint64_t owner = r.Get<uint64_t>();
            if (r.ok && lobbyId && g_matchmaking && g_api.SetLobbyOwner) g_api.SetLobbyOwner(g_matchmaking, lobbyId, owner);
            break;
        }
        case kLobbySetGameServer: {
            const uint64_t lobbyId = r.Get<uint64_t>();
            const uint32_t ip = r.Get<uint32_t>();
            const uint16_t port = r.Get<uint16_t>();
            const uint64_t gsId = r.Get<uint64_t>();
            if (r.ok && lobbyId && g_matchmaking && g_api.SetLobbyGameServer) g_api.SetLobbyGameServer(g_matchmaking, lobbyId, ip, port, gsId);
            break;
        }
        case kLobbyReqData: {
            const uint64_t lobbyId = r.Get<uint64_t>();
            if (r.ok && lobbyId && g_matchmaking && g_api.RequestLobbyData) g_api.RequestLobbyData(g_matchmaking, lobbyId);
            break;
        }
        case kStatsSetAchievement: {
            PendingStatOp op;
            op.kind = kStatsSetAchievement;
            op.name = r.Str16();
            if (r.ok && !op.name.empty()) QueueOrApplyStatOp(std::move(op));
            break;
        }
        case kStatsClearAchievement: {
            PendingStatOp op;
            op.kind = kStatsClearAchievement;
            op.name = r.Str16();
            if (r.ok && !op.name.empty()) QueueOrApplyStatOp(std::move(op));
            break;
        }
        case kStatsIndicateProgress: {
            PendingStatOp op;
            op.kind = kStatsIndicateProgress;
            op.name = r.Str16();
            op.cur = r.Get<uint32_t>();
            op.max = r.Get<uint32_t>();
            if (r.ok && !op.name.empty()) QueueOrApplyStatOp(std::move(op));
            break;
        }
        case kStatsSetInt: {
            PendingStatOp op;
            op.kind = kStatsSetInt;
            op.name = r.Str16();
            op.iVal = r.Get<int32_t>();
            if (r.ok && !op.name.empty()) QueueOrApplyStatOp(std::move(op));
            break;
        }
        case kStatsSetFloat: {
            PendingStatOp op;
            op.kind = kStatsSetFloat;
            op.name = r.Str16();
            op.fVal = r.Get<float>();
            if (r.ok && !op.name.empty()) QueueOrApplyStatOp(std::move(op));
            break;
        }
        case kStatsStore: {
            PendingStatOp op;
            op.kind = kStatsStore;
            QueueOrApplyStatOp(std::move(op));
            break;
        }
        case kStatsResetAll: {
            PendingStatOp op;
            op.kind = kStatsResetAll;
            op.bVal = r.Get<uint8_t>() != 0;
            if (r.ok) QueueOrApplyStatOp(std::move(op));
            break;
        }
        case kQuit: g_quit = true; break;
        default: break;
        }
    }
}

bool GameGone() { return WaitForSingleObject(g_gameProcess, 0) != WAIT_TIMEOUT; }

// Waits on the game socket (so a Quit is seen at once); false when it is time to stop.
bool Wait(DWORD ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(g_sock, &fds);
    timeval tv{ (long)(ms / 1000), (long)(ms % 1000) * 1000 };
    if (select(0, &fds, nullptr, nullptr, &tv) > 0) HandleGame();
    if (GameGone()) g_quit = true;
    return !g_quit;
}

// --- Steam client -------------------------------------------------------------------------------------------
std::wstring RegString(const wchar_t* key, const wchar_t* value) {
    wchar_t buf[MAX_PATH] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return {};
    return buf;
}

DWORD RegDword(const wchar_t* key, const wchar_t* value) {
    DWORD v = 0, size = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_DWORD, nullptr, &v, &size);
    return v;
}

bool ProcessAlive(DWORD pid) {
    HANDLE h = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    if (!h) return false;
    const bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
}

std::wstring Normalized(std::wstring p) {
    for (wchar_t& c : p) c = c == L'/' ? L'\\' : (wchar_t)towlower(c);
    return p;
}

// The Steam client's process, found by its image path. Not the registry's ActiveProcess pid: any second
// steam.exe (a steam:// link, a shortcut) writes its own pid there and exits at once, leaving a dead pid.
DWORD SteamPid() {
    const std::wstring want = Normalized(RegString(L"Software\\Valve\\Steam", L"SteamExe"));
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD found = 0;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, L"steam.exe") != 0) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) continue;
        wchar_t path[MAX_PATH] = {};
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, path, &len) && (want.empty() || Normalized(path) == want)) found = pe.th32ProcessID;
        CloseHandle(h);
    }
    CloseHandle(snap);
    return found;
}

static bool IsWine() { return ReFixIsWine(); }
static bool HasProtonSteamEnv() { return ReFixHasProtonSteamEnv(); }

bool SteamRunning() {
    if (SteamPid() != 0) return true;
    if (IsWine() && HasProtonSteamEnv()) return true;
    return false;
}

// SteamAPI_Init checks the registry pid too: point it back at the running client when a stray steam.exe left
// it dead (the value Steam itself keeps there).
bool SteamSignedIn() {
    const DWORD pid = SteamPid();
    const wchar_t* key = L"Software\\Valve\\Steam\\ActiveProcess";
    if (pid != 0) {
        if (RegDword(key, L"pid") != pid && !ProcessAlive(RegDword(key, L"pid")))
            RegSetKeyValueW(HKEY_CURRENT_USER, key, L"pid", REG_DWORD, &pid, sizeof(pid));
        return RegDword(key, L"ActiveUser") != 0;
    }
    if (IsWine()) {
        if (RegDword(key, L"ActiveUser") != 0) return true;
        if (HasProtonSteamEnv()) return true;
    }
    return false;
}

// Steam must outlive the game: started outside the game's job when Windows allows it.
bool RunSteam(const std::wstring& exe, const wchar_t* args) {
    std::wstring cmd = L"\"" + exe + L"\" " + args;
    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWMINNOACTIVE;
    const size_t slash = exe.find_last_of(L"\\/");
    const std::wstring dir = slash == std::wstring::npos ? std::wstring() : exe.substr(0, slash);
    for (DWORD flags : { (DWORD)(CREATE_BREAKAWAY_FROM_JOB | DETACHED_PROCESS), (DWORD)DETACHED_PROCESS }) {
        PROCESS_INFORMATION pi{};
        std::wstring line = cmd;
        if (CreateProcessW(exe.c_str(), &line[0], nullptr, nullptr, FALSE, flags, nullptr,
                           dir.empty() ? nullptr : dir.c_str(), &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }
    }
    return false;
}

// Steam that ReFix opened stays in the notification area: its main window ("Steam"), which it opens after the
// login despite -silent, is closed, which in Steam means "to the tray". Login and update windows have other
// titles and are left alone.
BOOL CALLBACK TrayIfSteamMain(HWND w, LPARAM) {
    wchar_t title[16] = {};
    if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER) || !InternalGetWindowText(w, title, 16) || wcscmp(title, L"Steam") != 0)
        return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return TRUE;
    wchar_t path[MAX_PATH] = {};
    DWORD len = MAX_PATH;
    const bool steam = QueryFullProcessImageNameW(h, 0, path, &len) &&
                       (wcsstr(Normalized(path).c_str(), L"\\steamwebhelper.exe") || wcsstr(Normalized(path).c_str(), L"\\steam.exe"));
    CloseHandle(h);
    if (steam) PostMessageW(w, WM_CLOSE, 0, 0);
    return TRUE;
}

void SendSteamToTray() { EnumWindows(TrayIfSteamMain, 0); }

bool StartSteamAndWait() {
    if (SteamSignedIn()) return true;
    if (IsWine()) {
        // Fast-fail non-Steam Wine environments (Lutris, Heroic, Bottles)
        // Never wait 30 seconds or attempt to run steam.exe -silent when native Steam is not running!
        if (!HasProtonSteamEnv() && RegDword(L"Software\\Valve\\Steam\\ActiveProcess", L"ActiveUser") == 0) {
            Report(kFailed, "Steam is not running on the host system");
            return false;
        }
        for (DWORD waited = 0; waited < 5000; waited += 500) {
            if (SteamSignedIn()) {
                Report(kConnecting);
                return Wait(1000);
            }
            if (!Wait(500)) return false;
        }
        return SteamSignedIn();
    }
    const std::wstring exe = RegString(L"Software\\Valve\\Steam", L"SteamExe");
    if (exe.empty()) { Report(kFailed, "Steam is not installed"); return false; }
    if (!SteamRunning()) {
        Report(kOpeningSteam);
        if (!RunSteam(exe, L"-silent")) { Report(kFailed, "Could not start Steam"); return false; }
        g_startedSteam = true;
    }
    // Never another steam.exe while one runs: it would only rewrite ActiveProcess (see SteamPid).
    DWORD runningFor = 0;
    for (DWORD waited = 0; waited < kSteamStartTimeoutMs; waited += 500) {
        if (SteamSignedIn()) {
            Report(kConnecting);
            return Wait(2000);   // steamclient needs a moment after login before it accepts API clients
        }
        if (SteamRunning()) {
            Report(kSigningIn);
            // Current Steam clients can leave ActiveProcess\ActiveUser at 0 while signed in: after a few seconds
            // let SteamAPI_Init decide (InitWhenReady keeps retrying it until Steam accepts or the timeout).
            if ((runningFor += 500) >= kActiveUserGraceMs) {
                Report(kConnecting);
                return true;
            }
        }
        if (!Wait(500)) return false;
    }
    Report(kFailed, "Steam did not sign in in time");
    return false;
}

template <class F> bool Resolve(F& fn, const char* name) {
    fn = reinterpret_cast<F>(GetProcAddress(g_api.dll, name));
    return fn != nullptr;
}

template <class F> bool ResolveAny(F& fn, std::initializer_list<const char*> names) {
    for (const char* n : names) if (Resolve(fn, n)) return true;
    return false;
}

bool LoadValve(const std::wstring& dir) {
#ifdef _WIN64
    const wchar_t* names[] = { L"refix_steamlink64.dll", L"steam_api64_original.dll", L"ReFix\\refix_steamlink64.dll" };
#else
    const wchar_t* names[] = { L"refix_steamlink.dll", L"steam_api_original.dll", L"ReFix\\refix_steamlink.dll" };
#endif
    for (const wchar_t* name : names) {
        const std::wstring path = dir + L"\\" + name;
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        g_api.dll = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!g_api.dll) {
            Log("Valve Steam API DLL load failed (error %lu)", GetLastError());
            continue;
        }
        const bool ok = Resolve(g_api.Init, "SteamAPI_Init") && Resolve(g_api.Shutdown, "SteamAPI_Shutdown") &&
            Resolve(g_api.RunCallbacks, "SteamAPI_RunCallbacks") &&
            Resolve(g_api.RegisterCallback, "SteamAPI_RegisterCallback") &&
            Resolve(g_api.UnregisterCallback, "SteamAPI_UnregisterCallback") &&
            Resolve(g_api.Messages, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002") &&
            ResolveAny(g_api.FriendsIface, { "SteamAPI_SteamFriends_v017", "SteamAPI_SteamFriends_v018" }) &&
            ResolveAny(g_api.UserIface, { "SteamAPI_SteamUser_v023", "SteamAPI_SteamUser_v022",
                                          "SteamAPI_SteamUser_v021", "SteamAPI_SteamUser_v020" }) &&
            Resolve(g_api.SendMsg, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser") &&
            Resolve(g_api.RecvMsgs, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel") &&
            Resolve(g_api.AcceptSession, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser") &&
            Resolve(g_api.Release, "SteamAPI_SteamNetworkingMessage_t_Release") &&
            Resolve(g_api.GetSteamID, "SteamAPI_ISteamUser_GetSteamID") &&
            Resolve(g_api.FriendCount, "SteamAPI_ISteamFriends_GetFriendCount") &&
            Resolve(g_api.FriendByIndex, "SteamAPI_ISteamFriends_GetFriendByIndex") &&
            Resolve(g_api.FriendName, "SteamAPI_ISteamFriends_GetFriendPersonaName") &&
            Resolve(g_api.FriendState, "SteamAPI_ISteamFriends_GetFriendPersonaState") &&
            Resolve(g_api.FriendGame, "SteamAPI_ISteamFriends_GetFriendGamePlayed") &&
            Resolve(g_api.FriendRP, "SteamAPI_ISteamFriends_GetFriendRichPresence") &&
            Resolve(g_api.SetRP, "SteamAPI_ISteamFriends_SetRichPresence") &&
            Resolve(g_api.HasFriend, "SteamAPI_ISteamFriends_HasFriend") &&
            Resolve(g_api.MediumAvatar, "SteamAPI_ISteamFriends_GetMediumFriendAvatar") &&
            Resolve(g_api.UtilsIface, "SteamAPI_SteamUtils_v010") &&
            Resolve(g_api.ImageSize, "SteamAPI_ISteamUtils_GetImageSize") &&
            Resolve(g_api.ImageRgba, "SteamAPI_ISteamUtils_GetImageRGBA");
        if (ok) {
            Log("Valve Steam API DLL loaded");
            Resolve(g_api.RequestRP, "SteamAPI_ISteamFriends_RequestFriendRichPresence");
            ResolveAny(g_api.NetUtils, { "SteamAPI_SteamNetworkingUtils_SteamAPI_v004", "SteamAPI_SteamNetworkingUtils_SteamAPI_v003" });
            Resolve(g_api.InitRelay, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess");
            Resolve(g_api.RelayStatus, "SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus");
            Resolve(g_api.SessionInfo, "SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo");
            ResolveAny(g_api.AppsIface, { "SteamAPI_SteamApps_v008", "SteamAPI_SteamApps_v009" });
            Resolve(g_api.IsSubscribedApp, "SteamAPI_ISteamApps_BIsSubscribedApp");
            Resolve(g_api.GetAuthTicket, "SteamAPI_ISteamUser_GetAuthSessionTicket");
            Resolve(g_api.GetAuthTicketWebApi, "SteamAPI_ISteamUser_GetAuthTicketForWebApi");
            Resolve(g_api.CancelAuthTicket, "SteamAPI_ISteamUser_CancelAuthTicket");
            Resolve(g_api.RequestEncryptedAppTicket, "SteamAPI_ISteamUser_RequestEncryptedAppTicket");
            Resolve(g_api.GetEncryptedAppTicket, "SteamAPI_ISteamUser_GetEncryptedAppTicket");
            Resolve(g_api.RegisterCallResult, "SteamAPI_RegisterCallResult");
            Resolve(g_api.UnregisterCallResult, "SteamAPI_UnregisterCallResult");
            ResolveAny(g_api.MatchmakingIface, { "SteamAPI_SteamMatchmaking_v009", "SteamAPI_SteamMatchmaking_v008" });
            Resolve(g_api.RequestLobbyList, "SteamAPI_ISteamMatchmaking_RequestLobbyList");
            Resolve(g_api.AddLobbyStrFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
            Resolve(g_api.AddLobbyNumFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter");
            Resolve(g_api.AddLobbyNearFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter");
            Resolve(g_api.AddLobbySlotsFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable");
            Resolve(g_api.AddLobbyDistFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter");
            Resolve(g_api.AddLobbyCountFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter");
            Resolve(g_api.AddLobbyCompatFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter");
            Resolve(g_api.GetLobbyByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyByIndex");
            Resolve(g_api.CreateLobby, "SteamAPI_ISteamMatchmaking_CreateLobby");
            Resolve(g_api.JoinLobby, "SteamAPI_ISteamMatchmaking_JoinLobby");
            Resolve(g_api.LeaveLobby, "SteamAPI_ISteamMatchmaking_LeaveLobby");
            Resolve(g_api.GetLobbyOwner, "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
            Resolve(g_api.GetNumLobbyMembers, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
            Resolve(g_api.GetLobbyMemberByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
            Resolve(g_api.GetLobbyDataCount, "SteamAPI_ISteamMatchmaking_GetLobbyDataCount");
            Resolve(g_api.GetLobbyDataByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex");
            Resolve(g_api.SetLobbyData, "SteamAPI_ISteamMatchmaking_SetLobbyData");
            Resolve(g_api.DeleteLobbyData, "SteamAPI_ISteamMatchmaking_DeleteLobbyData");
            Resolve(g_api.SetLobbyType, "SteamAPI_ISteamMatchmaking_SetLobbyType");
            Resolve(g_api.SetLobbyJoinable, "SteamAPI_ISteamMatchmaking_SetLobbyJoinable");
            Resolve(g_api.SetLobbyMemberLimit, "SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit");
            Resolve(g_api.GetLobbyMemberLimit, "SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit");
            Resolve(g_api.SetLobbyOwner, "SteamAPI_ISteamMatchmaking_SetLobbyOwner");
            Resolve(g_api.SetLobbyGameServer, "SteamAPI_ISteamMatchmaking_SetLobbyGameServer");
            Resolve(g_api.GetLobbyGameServer, "SteamAPI_ISteamMatchmaking_GetLobbyGameServer");
            Resolve(g_api.RequestLobbyData, "SteamAPI_ISteamMatchmaking_RequestLobbyData");
            ResolveAny(g_api.UserStatsIface, { "SteamAPI_SteamUserStats_v013", "SteamAPI_SteamUserStats_v012", "SteamAPI_SteamUserStats_v011" });
            Resolve(g_api.RequestCurrentStats, "SteamAPI_ISteamUserStats_RequestCurrentStats");
            Resolve(g_api.GetAchievement, "SteamAPI_ISteamUserStats_GetAchievement");
            Resolve(g_api.SetAchievement, "SteamAPI_ISteamUserStats_SetAchievement");
            Resolve(g_api.ClearAchievement, "SteamAPI_ISteamUserStats_ClearAchievement");
            Resolve(g_api.IndicateProgress, "SteamAPI_ISteamUserStats_IndicateAchievementProgress");
            Resolve(g_api.GetStatInt32, "SteamAPI_ISteamUserStats_GetStatInt32");
            Resolve(g_api.GetStatFloat, "SteamAPI_ISteamUserStats_GetStatFloat");
            Resolve(g_api.SetStatInt32, "SteamAPI_ISteamUserStats_SetStatInt32");
            Resolve(g_api.SetStatFloat, "SteamAPI_ISteamUserStats_SetStatFloat");
            Resolve(g_api.StoreStats, "SteamAPI_ISteamUserStats_StoreStats");
            Resolve(g_api.ResetAllStats, "SteamAPI_ISteamUserStats_ResetAllStats");
            return true;
        }
        FreeLibrary(g_api.dll);
        Log("Valve Steam API DLL is missing required exports");
        g_api = Api{};
    }
    Report(kFailed, "Steam API library not found (refix_steamlink DLL)");
    return false;
}

bool InitAsBridgeApp() {
    const std::wstring appStr = std::to_wstring(g_bridgeAppId ? g_bridgeAppId : 480);
    SetEnvironmentVariableW(L"SteamAppId", appStr.c_str());
    SetEnvironmentVariableW(L"SteamGameId", appStr.c_str());
    if (!g_api.Init()) return false;
    g_msgs = g_api.Messages();
    g_friends = g_api.FriendsIface();
    void* user = g_api.UserIface();
    if (!g_msgs || !g_friends || !user) {
        g_api.Shutdown();
        return false;
    }
    g_user = user;
    g_self = g_api.GetSteamID(user);
    g_initialAccountId = (uint32_t)(g_self & 0xFFFFFFFFULL);
    g_utils = g_api.UtilsIface();
    g_apps = g_api.AppsIface ? g_api.AppsIface() : nullptr;
    g_matchmaking = g_api.MatchmakingIface ? g_api.MatchmakingIface() : nullptr;
    g_userStats = g_api.UserStatsIface ? g_api.UserStatsIface() : nullptr;
    g_statsInitAt = GetTickCount64();
    if (g_userStats && g_api.RequestCurrentStats) g_api.RequestCurrentStats(g_userStats);
    return true;
}

// Steam that has just signed in may still refuse API clients for a moment: retry until it accepts us.
bool InitWhenReady() {
    for (DWORD waited = 0;; waited += kInitRetryMs) {
        if ((SteamSignedIn() || SteamRunning()) && InitAsBridgeApp()) return true;
        if (g_bridgeAppId != 480 && SteamSignedIn() && waited >= 4000) {
            if (g_maskMode) {
                Report(kFailed, "Mask AppID rejected by Steam");
                return false;
            }
            Log("Bridge AppID %u rejected by Steam; falling back to Spacewar (480)", g_bridgeAppId);
            g_bridgeAppId = 480;
            if (InitAsBridgeApp()) return true;
        }
        if (!waited) Log("Steam API initialization failed; retrying (Wine=%d, Proton environment=%d)", IsWine(), HasProtonSteamEnv());
        if (waited >= kSteamStartTimeoutMs) {
            Report(kFailed, "SteamAPI_Init failed (is Steam signed in?)");
            return false;
        }
        if (!Wait(kInitRetryMs)) return false;
    }
}

void SendFriends() {
    Writer w(kFriends);
    w.Put<uint16_t>(0);
    uint16_t count = 0;
    const int n = g_api.FriendCount(g_friends, kFriendFlagImmediate);
    const ULONGLONG now = GetTickCount64();
    static std::map<uint64_t, ULONGLONG> s_lastRpReq;
    for (int i = 0; i < n && w.b.size() < kFriendBytes; ++i) {
        const uint64_t id = g_api.FriendByIndex(g_friends, i, kFriendFlagImmediate);
        if (!id) continue;
        const char* name = g_api.FriendName(g_friends, id);
        FriendGameInfo game{};
        const bool playing = g_api.FriendGame(g_friends, id, &game) && game.gameId != 0;
        const uint32_t gameId = playing ? (uint32_t)(game.gameId & 0xFFFFFF) : 0;
        const char* rp = g_api.FriendRP(g_friends, id, "refix");
        const char* rpApp = g_api.FriendRP(g_friends, id, "refix_app");
        uint32_t appId = (rpApp && rpApp[0]) ? strtoul(rpApp, nullptr, 10) : 0;
        if (!appId && gameId != 480) appId = gameId;

        if (playing && g_api.RequestRP) {
            auto& lastReq = s_lastRpReq[id];
            const uint32_t reqInterval = (!appId || !rp) ? 3000 : 30000;
            if (now - lastReq >= reqInterval) {
                lastReq = now;
                g_api.RequestRP(g_friends, id);
            }
        }

        const bool sameApp = (appId != 0 && (g_appId == 0 || appId == g_appId ||
            std::find(g_altAppIds.begin(), g_altAppIds.end(), appId) != g_altAppIds.end()));
        const bool hasRefixRP = (rp && rp[0] == '1');
        const bool refix = playing && hasRefixRP;
        const char* gname = (rp && rp[0] == '1') ? g_api.FriendRP(g_friends, id, "refix_game") : "";
        const int avatar = g_api.MediumAvatar(g_friends, id);   // -1 while loading, 0 when none
        w.Put<uint64_t>(id).Put<int32_t>(g_api.FriendState(g_friends, id)).Put<uint8_t>(playing).Put<uint32_t>(gameId)
         .Put<uint8_t>(refix).Put<uint32_t>(appId).Put<int32_t>(avatar < 0 ? 0 : avatar).Str(name ? name : "").Str(gname ? gname : "");
        ++count;
    }
    memcpy(&w.b[1], &count, sizeof(count));
    Post(w);
}

// Peer-to-peer goes through Valve's relays: start reaching them now instead of on the first message.
void StartRelay() {
    g_netUtils = g_api.NetUtils ? g_api.NetUtils() : nullptr;
    if (g_netUtils && g_api.InitRelay) g_api.InitRelay(g_netUtils);
    else Log("relay network warm-up not available in this Steam API DLL");
}

// Every few seconds: the relays' state when it changes, and each friend's tunnel counters when they move.
void Diagnose() {
    static int lastAvail = 0x7fffffff, lastConfig = 0, lastRelay = 0;
    if (g_netUtils && g_api.RelayStatus) {
        RelayStatusInfo s{};
        const int avail = g_api.RelayStatus(g_netUtils, &s);
        s.debug[sizeof(s.debug) - 1] = 0;
        if (avail != lastAvail || s.availConfig != lastConfig || s.availRelay != lastRelay) {   // 100 = ready
            lastAvail = avail;
            lastConfig = s.availConfig;
            lastRelay = s.availRelay;
            Log("relay network: %d (config %d, relays %d) %s", avail, s.availConfig, s.availRelay, s.debug);
        }
    }
    for (auto& [id, st] : g_peers) {
        const uint32_t total = st.sent + st.received;
        if (total == st.logged) continue;
        st.logged = total;
        int state = -1;
        if (g_api.SessionInfo) {
            const NetIdentity peer = IdentityOf(id);
            static uint8_t info[2048], status[2048];   // larger than SteamNetConnectionInfo_t / RealTimeStatus_t
            state = g_api.SessionInfo(g_msgs, &peer, info, status);
        }
        // state: 0 none, 1 connecting, 2 finding route, 3 connected, 4 closed by peer, 5 problem detected locally
        Log("tunnel %llu: sent %u (failed %u, last EResult %d), received %u, session state %d", (unsigned long long)id,
            st.sent, st.failed, st.lastResult, st.received, state);
    }
}

void OnSessionRequest(void* param) {
    const auto* remote = static_cast<const NetIdentity*>(param);
    const bool isFriend = remote->type == 16 && g_api.HasFriend(g_friends, remote->steamId, kFriendFlagImmediate);
    const bool accepted = isFriend && g_api.AcceptSession(g_msgs, remote);   // friends only
    Log("session request from %llu: %s", (unsigned long long)remote->steamId,
        accepted ? "accepted" : isFriend ? "accept failed" : "ignored (not a friend)");
}

void OnSessionFailed(void* param) {   // SteamNetworkingMessagesSessionFailed_t starts with the remote identity
    Log("session with %llu failed", (unsigned long long)static_cast<const NetIdentity*>(param)->steamId);
}

std::atomic<bool> g_friendsDirty{false};

void OnFriendRichPresenceUpdate(void*) {
    g_friendsDirty = true;
}

void OnAvatarImageLoaded(void*) {
    g_friendsDirty = true;
}

void OnPersonaStateChange(void*) {
    g_friendsDirty = true;
}

struct SessionTicketCb { uint32_t h; int eResult; };
struct WebApiTicketCb { uint32_t h; int eResult; int cub; uint8_t data[2560]; };

void OnAuthTicketResp(void* param) {
    const auto* cb = static_cast<const SessionTicketCb*>(param);
    if (!cb) return;
    auto it = g_pendingAuth.find(cb->h);
    if (it == g_pendingAuth.end()) return;
    Writer w(kAuthTicketData);
    w.Put<uint32_t>(it->second.reqId).Put<uint32_t>(cb->h).Put<int32_t>(cb->eResult)
     .Put<uint32_t>((uint32_t)it->second.bytes.size());
    if (!it->second.bytes.empty()) w.Raw(it->second.bytes.data(), it->second.bytes.size());
    Post(w);
    g_pendingAuth.erase(it);
}

void OnWebApiTicketResp(void* param) {
    const auto* cb = static_cast<const WebApiTicketCb*>(param);
    if (!cb) return;
    auto it = g_pendingAuth.find(cb->h);
    if (it == g_pendingAuth.end()) return;
    const uint32_t len = (cb->cub > 0 && cb->cub <= 2560) ? (uint32_t)cb->cub : 0;
    Writer w(kAuthTicketData);
    w.Put<uint32_t>(it->second.reqId).Put<uint32_t>(cb->h).Put<int32_t>(cb->eResult).Put<uint32_t>(len);
    if (len) w.Raw(cb->data, len);
    Post(w);
    g_pendingAuth.erase(it);
}

// Valve's CCallbackBase (steam_api_common.h), declared the same way so MSVC gives it the same vtable.
// Callbacks go through SteamAPI_RunCallbacks like in any game: with manual dispatch the relay network never got
// its configuration (it waited for results only the regular dispatcher delivers), so P2P sessions never connected.
class CallbackBase {
public:
    virtual void Run(void* param) = 0;
    virtual void Run(void* param, bool ioFailure, uint64_t call) = 0;
    virtual int GetCallbackSizeBytes() = 0;
protected:
    uint8_t flags_ = 0;
    int id_ = 0;
};

class HostCallback : public CallbackBase {
public:
    HostCallback(int id, int size, void (*fn)(void*)) : size_(size), fn_(fn) { id_ = id; }
    void Run(void* param) override { fn_(param); }
    void Run(void* param, bool, uint64_t) override { fn_(param); }
    int GetCallbackSizeBytes() override { return size_; }
    int Id() const { return id_; }
private:
    int size_;
    void (*fn_)(void*);
};

void OnUserStatsReceived(void* param) {
    (void)param;
    g_statsReady = true;
    FlushPendingStatOps();
}

HostCallback g_sessionRequest(kCbSessionRequest, (int)sizeof(NetIdentity), &OnSessionRequest);
HostCallback g_statsReceivedCb(1101, 24, &OnUserStatsReceived);
HostCallback g_sessionFailed(kCbSessionFailed, (int)sizeof(SessionFailedInfo), &OnSessionFailed);
HostCallback g_rpUpdate(336, 16, &OnFriendRichPresenceUpdate);
HostCallback g_avatarLoaded(334, 24, &OnAvatarImageLoaded);
HostCallback g_personaState(304, 16, &OnPersonaStateChange);
HostCallback g_authTicketCb(163, (int)sizeof(SessionTicketCb), &OnAuthTicketResp);
HostCallback g_webApiTicketCb(168, (int)sizeof(WebApiTicketCb), &OnWebApiTicketResp);

#pragma pack(push, 8)
struct HostLobbyMatchList { uint32_t count; };
struct HostLobbyCreated { int32_t eResult; uint64_t lobbyId; };
struct HostLobbyEnter { uint64_t lobbyId; uint32_t chatPerms; bool locked; uint32_t enterResp; };
struct HostEncryptedTicket { int32_t eResult; };
struct HostLobbyDataUpdate { uint64_t lobbyId; uint64_t memberId; uint8_t success; };
struct HostLobbyChatUpdate { uint64_t lobbyId; uint64_t userChanged; uint64_t makingChange; uint32_t stateChange; };
#pragma pack(pop)

void SendLobbySnapshot(uint64_t lobbyId, uint32_t reqId) {
    if (!g_matchmaking || !lobbyId) return;
    const uint64_t owner = g_api.GetLobbyOwner ? g_api.GetLobbyOwner(g_matchmaking, lobbyId) : 0;
    const int32_t maxMembers = g_api.GetLobbyMemberLimit ? g_api.GetLobbyMemberLimit(g_matchmaking, lobbyId) : 0;
    uint32_t gsIp = 0;
    uint16_t gsPort = 0;
    uint64_t gsId = 0;
    const bool hasGs = g_api.GetLobbyGameServer && g_api.GetLobbyGameServer(g_matchmaking, lobbyId, &gsIp, &gsPort, &gsId);

    std::vector<uint64_t> members;
    if (g_api.GetNumLobbyMembers && g_api.GetLobbyMemberByIndex) {
        const int nm = g_api.GetNumLobbyMembers(g_matchmaking, lobbyId);
        for (int i = 0; i < nm && i < 256; ++i) {
            const uint64_t mid = g_api.GetLobbyMemberByIndex(g_matchmaking, lobbyId, i);
            if (mid) members.push_back(mid);
        }
    }

    std::vector<std::pair<std::string, std::string>> kvs;
    if (g_api.GetLobbyDataCount && g_api.GetLobbyDataByIndex) {
        const int nk = g_api.GetLobbyDataCount(g_matchmaking, lobbyId);
        char kbuf[512], vbuf[4096];
        for (int i = 0; i < nk && i < 256; ++i) {
            kbuf[0] = 0; vbuf[0] = 0;
            if (g_api.GetLobbyDataByIndex(g_matchmaking, lobbyId, i, kbuf, sizeof(kbuf), vbuf, sizeof(vbuf))) {
                kvs.emplace_back(kbuf, vbuf);
            }
        }
    }

    Writer w(kLobbySnapshot);
    w.Put<uint32_t>(reqId).Put<uint64_t>(lobbyId).Put<uint64_t>(owner).Put<int32_t>(maxMembers)
     .Put<uint8_t>(hasGs ? 1 : 0).Put<uint32_t>(gsIp).Put<uint16_t>(gsPort).Put<uint64_t>(gsId);
    w.Put<uint16_t>((uint16_t)members.size());
    for (uint64_t m : members) w.Put<uint64_t>(m);
    w.Put<uint16_t>((uint16_t)kvs.size());
    for (const auto& kv : kvs) {
        if (w.b.size() + kv.first.size() + kv.second.size() + 16 >= kMaxDatagram) break;
        w.Str16(kv.first).Str16(kv.second);
    }
    Post(w);
}

void OnLobbyDataUpdate(void* param) {
    const auto* cb = static_cast<const HostLobbyDataUpdate*>(param);
    if (!cb || !cb->lobbyId) return;
    if (cb->success) SendLobbySnapshot(cb->lobbyId, 0);
    Post(Writer(kLobbyDataChanged).Put<uint64_t>(cb->lobbyId).Put<uint64_t>(cb->memberId).Put<uint8_t>(cb->success));
}

void OnLobbyChatUpdate(void* param) {
    const auto* cb = static_cast<const HostLobbyChatUpdate*>(param);
    if (!cb || !cb->lobbyId) return;
    SendLobbySnapshot(cb->lobbyId, 0);
    Post(Writer(kLobbyChatChanged).Put<uint64_t>(cb->lobbyId).Put<uint64_t>(cb->userChanged)
             .Put<uint64_t>(cb->makingChange).Put<uint32_t>(cb->stateChange));
}

HostCallback g_lobbyDataCb(505, (int)sizeof(HostLobbyDataUpdate), &OnLobbyDataUpdate);
HostCallback g_lobbyChatCb(506, (int)sizeof(HostLobbyChatUpdate), &OnLobbyChatUpdate);

class LobbyCallSlot : public CallbackBase {
public:
    enum class Kind { Search, Create, Join, EncryptedTicket };
    Kind kind;
    uint32_t reqId = 0;
    uint64_t hCall = 0;
    bool done = false;
    LobbyCallSlot(Kind k, uint32_t r, uint64_t c, int cbId, int sz) : kind(k), reqId(r), hCall(c), size_(sz) { id_ = cbId; }
    void Run(void* param) override { Run(param, false, hCall); }
    void Run(void* param, bool ioFailure, uint64_t) override {
        done = true;
        if (kind == Kind::Search) {
            const auto* cb = static_cast<const HostLobbyMatchList*>(param);
            std::vector<uint64_t> ids;
            if (!ioFailure && cb && g_matchmaking && g_api.GetLobbyByIndex) {
                for (uint32_t i = 0; i < cb->count && i < 200; ++i) {
                    const uint64_t lId = g_api.GetLobbyByIndex(g_matchmaking, (int)i);
                    if (lId) {
                        SendLobbySnapshot(lId, reqId);
                        ids.push_back(lId);
                    }
                }
            }
            Writer w(kLobbySearchDone);
            w.Put<uint32_t>(reqId).Put<uint8_t>(ioFailure ? 1 : 0).Put<uint16_t>((uint16_t)ids.size());
            for (uint64_t id : ids) w.Put<uint64_t>(id);
            Post(w);
        } else if (kind == Kind::Create) {
            const auto* cb = static_cast<const HostLobbyCreated*>(param);
            const int32_t res = (!ioFailure && cb) ? cb->eResult : 2;
            const uint64_t lId = (!ioFailure && cb) ? cb->lobbyId : 0;
            if (res == 1 && lId) SendLobbySnapshot(lId, reqId);
            Post(Writer(kLobbyCreatedRes).Put<uint32_t>(reqId).Put<int32_t>(res).Put<uint64_t>(lId));
        } else if (kind == Kind::Join) {
            const auto* cb = static_cast<const HostLobbyEnter*>(param);
            if (!ioFailure && cb && cb->lobbyId) {
                if (cb->enterResp == 1) SendLobbySnapshot(cb->lobbyId, reqId);
                Post(Writer(kLobbyEnterRes).Put<uint32_t>(reqId).Put<uint64_t>(cb->lobbyId)
                         .Put<uint32_t>(cb->chatPerms).Put<uint8_t>(cb->locked ? 1 : 0).Put<uint32_t>(cb->enterResp));
            } else {
                Post(Writer(kLobbyEnterRes).Put<uint32_t>(reqId).Put<uint64_t>(0).Put<uint32_t>(0).Put<uint8_t>(0).Put<uint32_t>(2));
            }
        } else if (kind == Kind::EncryptedTicket) {
            const auto* cb = static_cast<const HostEncryptedTicket*>(param);
            const int32_t res = (!ioFailure && cb) ? cb->eResult : 2;
            std::vector<uint8_t> buf(2048);
            uint32_t len = 0;
            if (res == 1 && g_user && g_api.GetEncryptedAppTicket &&
                g_api.GetEncryptedAppTicket(g_user, buf.data(), (int)buf.size(), &len) && len > 0 && len <= buf.size()) {
                Writer w(kEncryptedAppTicketData);
                w.Put<uint32_t>(reqId).Put<int32_t>(res).Put<uint32_t>(len).Raw(buf.data(), len);
                Post(w);
            } else {
                Post(Writer(kEncryptedAppTicketData).Put<uint32_t>(reqId).Put<int32_t>(res == 1 ? 2 : res).Put<uint32_t>(0));
            }
        }
    }
    int GetCallbackSizeBytes() override { return size_; }
private:
    int size_;
};

std::vector<std::unique_ptr<LobbyCallSlot>> g_lobbyCallSlots;

void StartLobbySearchCall(uint32_t reqId, uint64_t hCall) {
    auto slot = std::make_unique<LobbyCallSlot>(LobbyCallSlot::Kind::Search, reqId, hCall, 510, (int)sizeof(HostLobbyMatchList));
    g_api.RegisterCallResult(slot.get(), hCall);
    g_lobbyCallSlots.push_back(std::move(slot));
}
void StartLobbyCreateCall(uint32_t reqId, uint64_t hCall) {
    auto slot = std::make_unique<LobbyCallSlot>(LobbyCallSlot::Kind::Create, reqId, hCall, 513, (int)sizeof(HostLobbyCreated));
    g_api.RegisterCallResult(slot.get(), hCall);
    g_lobbyCallSlots.push_back(std::move(slot));
}
void StartLobbyJoinCall(uint32_t reqId, uint64_t hCall) {
    auto slot = std::make_unique<LobbyCallSlot>(LobbyCallSlot::Kind::Join, reqId, hCall, 504, (int)sizeof(HostLobbyEnter));
    g_api.RegisterCallResult(slot.get(), hCall);
    g_lobbyCallSlots.push_back(std::move(slot));
}
void StartEncryptedTicketCall(uint32_t reqId, uint64_t hCall) {
    auto slot = std::make_unique<LobbyCallSlot>(LobbyCallSlot::Kind::EncryptedTicket, reqId, hCall, 154, (int)sizeof(HostEncryptedTicket));
    g_api.RegisterCallResult(slot.get(), hCall);
    g_lobbyCallSlots.push_back(std::move(slot));
}
void CleanupLobbyCalls() {
    g_lobbyCallSlots.erase(
        std::remove_if(g_lobbyCallSlots.begin(), g_lobbyCallSlots.end(), [](const std::unique_ptr<LobbyCallSlot>& s) { return s->done; }),
        g_lobbyCallSlots.end());
}

void ForwardMessages() {
    NetMessage* msgs[32];
    int n;
    while ((n = g_api.RecvMsgs(g_msgs, kChannel, msgs, 32)) > 0) {
        for (int i = 0; i < n; ++i) {
            if (msgs[i]->peer.type == 16) {
                ++g_peers[msgs[i]->peer.steamId].received;
                if (msgs[i]->size > 0 && (size_t)msgs[i]->size < kMaxDatagram - 16)
                    Post(Writer(kMessage).Put<uint64_t>(msgs[i]->peer.steamId).Raw(msgs[i]->data, (size_t)msgs[i]->size));
            } else {
                Log("RecvMsg peer type mismatch: type=%d (expected 16), size=%d", msgs[i]->peer.type, msgs[i]->size);
            }
            g_api.Release(msgs[i]);
        }
    }
}

void WriteStdout(const std::string& line) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!hOut || hOut == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hOut, line.data(), (DWORD)line.size(), &written, nullptr);
}

// Silent license & active-account probe via steamclient(64).dll IPC without invoking SteamAPI_Init
// (so Steam never flashes "Playing Spacewar" when inspecting licenses in Settings).
int RunProbe(const wchar_t* csvAppIds) {
    const bool regSignedIn = SteamSignedIn();
    if (!SteamRunning() && !regSignedIn) {
        WriteStdout("NO_STEAM\n");
        return 0;
    }
    const DWORD activeUser = RegDword(L"Software\\Valve\\Steam\\ActiveProcess", L"ActiveUser");
    auto emitFallbackSignedIn = [&](uint64_t sid) -> int {
        std::string out = "OK " + std::to_string(sid);
        if (csvAppIds && *csvAppIds) {
            const wchar_t* p = csvAppIds;
            while (*p) {
                while (*p == L',' || *p == L' ') ++p;
                if (!*p) break;
                uint32_t appId = (uint32_t)_wtoi(p);
                while (*p && *p != L',') ++p;
                if (appId != 0) out += " " + std::to_string(appId) + ":0";
            }
        }
        out += "\n";
        const std::wstring autoLogin = RegString(L"Software\\Valve\\Steam", L"AutoLoginUser");
        if (!autoLogin.empty()) {
            int u8 = WideCharToMultiByte(CP_UTF8, 0, autoLogin.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (u8 > 1) {
                std::string cleanName(u8 - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, autoLogin.c_str(), -1, &cleanName[0], u8, nullptr, nullptr);
                for (char& c : cleanName) {
                    if (c == '\r' || c == '\n') c = ' ';
                }
                out += "NAME " + cleanName + "\n";
            }
        }
        WriteStdout(out);
        return 0;
    };
    std::wstring steamPath = RegString(L"Software\\Valve\\Steam", L"SteamPath");
    for (wchar_t& c : steamPath) {
        if (c == L'/') c = L'\\';
    }
    while (!steamPath.empty() && steamPath.back() == L'\\') steamPath.pop_back();

#ifdef _WIN64
    std::wstring regClientDll = RegString(L"Software\\Valve\\Steam\\ActiveProcess", L"SteamClientDll64");
    const std::wstring defaultClientDll = steamPath.empty() ? std::wstring() : (steamPath + L"\\steamclient64.dll");
#else
    std::wstring regClientDll = RegString(L"Software\\Valve\\Steam\\ActiveProcess", L"SteamClientDll");
    const std::wstring defaultClientDll = steamPath.empty() ? std::wstring() : (steamPath + L"\\steamclient.dll");
#endif
    for (wchar_t& c : regClientDll) {
        if (c == L'/') c = L'\\';
    }
    const std::wstring dll = (!regClientDll.empty() && GetFileAttributesW(regClientDll.c_str()) != INVALID_FILE_ATTRIBUTES)
        ? regClientDll
        : defaultClientDll;
    if (dll.empty()) {
        if (activeUser != 0) return emitFallbackSignedIn(76561197960265728ULL + (uint64_t)activeUser);
        WriteStdout("NO_CLIENT\n");
        return 0;
    }
    std::wstring dllFolder = steamPath;
    if (dllFolder.empty()) {
        const size_t sl = dll.find_last_of(L'\\');
        if (sl != std::wstring::npos) dllFolder = dll.substr(0, sl);
    }
    if (!dllFolder.empty()) SetDllDirectoryW(dllFolder.c_str());
    HMODULE h = LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h && !defaultClientDll.empty() && defaultClientDll != dll) {
        h = LoadLibraryExW(defaultClientDll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    SetDllDirectoryW(nullptr);
    if (!h) {
        if (activeUser != 0) return emitFallbackSignedIn(76561197960265728ULL + (uint64_t)activeUser);
        WriteStdout("NO_CLIENT\n");
        return 0;
    }
    typedef void* (*fn_CreateInterface)(const char*, int*);
    auto pCreate = (fn_CreateInterface)GetProcAddress(h, "CreateInterface");
    int rc = 0;
    ISteamClient* client = nullptr;
    if (pCreate) {
        for (const char* ver : { "SteamClient021", "SteamClient020", "SteamClient019", "SteamClient018", "SteamClient017" }) {
            client = static_cast<ISteamClient*>(pCreate(ver, &rc));
            if (client) break;
        }
    }
    if (!client) {
        FreeLibrary(h);
        if (activeUser != 0) return emitFallbackSignedIn(76561197960265728ULL + (uint64_t)activeUser);
        WriteStdout("NO_CLIENT\n");
        return 0;
    }
    HSteamPipe pipe = 0;
    HSteamUser user = 0;
    for (int attempt = 0; attempt < 6; ++attempt) {
        pipe = client->CreateSteamPipe();
        user = pipe ? client->ConnectToGlobalUser(pipe) : 0;
        if (pipe && user) break;
        if (pipe) {
            client->BReleaseSteamPipe(pipe);
            pipe = 0;
        }
        Sleep(100);
    }
    if (!pipe || !user) {
        FreeLibrary(h);
        if (regSignedIn || activeUser != 0) {
            return emitFallbackSignedIn(76561197960265728ULL + (uint64_t)activeUser);
        }
        WriteStdout("NOT_SIGNED_IN\n");
        return 0;
    }
    ISteamUser* steamUser = nullptr;
    for (const char* uv : { "SteamUser023", "SteamUser022", "SteamUser021", "SteamUser020" }) {
        steamUser = client->GetISteamUser(user, pipe, uv);
        if (steamUser) break;
    }
    uint64_t steamId = steamUser ? steamUser->GetSteamID().ConvertToUint64() : 0;
    if (steamId == 0 && activeUser != 0) {
        steamId = 76561197960265728ULL + (uint64_t)activeUser;
    }
    ISteamFriends* friends = nullptr;
    for (const char* fv : { "SteamFriends018", "SteamFriends017", "SteamFriends016", "SteamFriends015" }) {
        friends = client->GetISteamFriends(user, pipe, fv);
        if (friends) break;
    }
    const char* personaName = friends ? friends->GetPersonaName() : nullptr;
    ISteamApps* apps = client->GetISteamApps(user, pipe, "STEAMAPPS_INTERFACE_VERSION008");
    std::string out = "OK " + std::to_string(steamId);
    if (csvAppIds && *csvAppIds && apps) {
        const wchar_t* p = csvAppIds;
        while (*p) {
            while (*p == L',' || *p == L' ') ++p;
            if (!*p) break;
            uint32_t appId = (uint32_t)_wtoi(p);
            while (*p && *p != L',') ++p;
            if (appId != 0) {
                const bool owned = apps->BIsSubscribedApp(appId);
                out += " " + std::to_string(appId) + ":" + (owned ? "1" : "0");
            }
        }
    }
    out += "\n";
    if (personaName && *personaName) {
        std::string cleanName(personaName);
        for (char& c : cleanName) {
            if (c == '\r' || c == '\n') c = ' ';
        }
        out += "NAME " + cleanName + "\n";
    }
    client->ReleaseUser(pipe, user);
    client->BReleaseSteamPipe(pipe);
    FreeLibrary(h);
    WriteStdout(out);
    return 0;
}

int Run(int argc, wchar_t** argv) {
    if (argc >= 3 && wcscmp(argv[1], L"--probe") == 0) {
        return RunProbe(argv[2]);
    }
    if (argc < 5) return 2;
    const u_short gamePort = (u_short)_wtoi(argv[1]);
    std::string token;
    if (int tlen = WideCharToMultiByte(CP_UTF8, 0, argv[2], -1, nullptr, 0, nullptr, nullptr)) {
        token.resize(tlen - 1);
        WideCharToMultiByte(CP_UTF8, 0, argv[2], -1, &token[0], tlen, nullptr, nullptr);
    }
    g_gameProcess = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)_wtoi(argv[3]));
    const std::wstring dir = argv[4];
    if (argc >= 6) {
        uint32_t b = (uint32_t)_wtoi(argv[5]);
        if (b != 0) g_bridgeAppId = b;
    }
    for (int i = 6; i < argc; ++i) {
        if (wcscmp(argv[i], L"--mask") == 0) g_maskMode = true;
    }
    if (!gamePort || !g_gameProcess) return 3;

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in self{};
    self.sin_family = AF_INET;
    self.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int big = 1 << 20;
    setsockopt(g_sock, SOL_SOCKET, SO_RCVBUF, (const char*)&big, sizeof(big));
    setsockopt(g_sock, SOL_SOCKET, SO_SNDBUF, (const char*)&big, sizeof(big));
    if (g_sock == INVALID_SOCKET || bind(g_sock, (const sockaddr*)&self, sizeof(self)) != 0) return 4;
    u_long nonblock = 1;
    ioctlsocket(g_sock, FIONBIO, &nonblock);
    g_game.sin_family = AF_INET;
    g_game.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    g_game.sin_port = htons(gamePort);
    Post(Writer(kHello).Raw(token.data(), token.size()));
    Log("%s started (bridge=%u, Wine=%d, Proton environment=%d)",
        g_maskMode ? "Steam mask helper" : "Steam helper", g_bridgeAppId, IsWine(), HasProtonSteamEnv());

    if (!StartSteamAndWait() || !LoadValve(dir) || !InitWhenReady()) {
        while (Wait(500)) {}   // stay until the game reads the failure and lets us go
        return 1;
    }
    g_api.RegisterCallback(&g_statsReceivedCb, g_statsReceivedCb.Id());
    if (g_maskMode) {
        Report(kOnline);
        while (Wait(20)) {
            g_api.RunCallbacks();
            FlushPendingStatOps();
        }
        FlushPendingStatOps();
        g_api.RunCallbacks();
        g_api.UnregisterCallback(&g_statsReceivedCb);
        g_api.Shutdown();
        return 0;
    }
    g_api.RegisterCallback(&g_sessionRequest, g_sessionRequest.Id());
    g_api.RegisterCallback(&g_sessionFailed, g_sessionFailed.Id());
    g_api.RegisterCallback(&g_rpUpdate, g_rpUpdate.Id());
    g_api.RegisterCallback(&g_avatarLoaded, g_avatarLoaded.Id());
    g_api.RegisterCallback(&g_personaState, g_personaState.Id());
    g_api.RegisterCallback(&g_authTicketCb, g_authTicketCb.Id());
    g_api.RegisterCallback(&g_webApiTicketCb, g_webApiTicketCb.Id());
    g_api.RegisterCallback(&g_lobbyDataCb, g_lobbyDataCb.Id());
    g_api.RegisterCallback(&g_lobbyChatCb, g_lobbyChatCb.Id());
    StartRelay();
    Report(kOnline);
    PublishPresence(true);
    SendFriends();
    HANDLE rpSlotMutex = nullptr;
    const DWORD rpAccId = g_initialAccountId ? g_initialAccountId : RegDword(L"Software\\Valve\\Steam\\ActiveProcess", L"ActiveUser");
    if (rpAccId != 0) {
        for (int slot = 0; slot < 16 && !rpSlotMutex; ++slot) {
            wchar_t mName[96];
            swprintf_s(mName, L"Local\\ReFix_SteamHostRP_%u_%d", (unsigned)rpAccId, slot);
            HANDLE m = CreateMutexW(nullptr, FALSE, mName);
            if (m && GetLastError() != ERROR_ALREADY_EXISTS) {
                rpSlotMutex = m;
            } else if (m) {
                CloseHandle(m);
            }
        }
    }
    ULONGLONG lastFriends = GetTickCount64(), lastHide = 0, lastDiag = GetTickCount64(), lastAccCheck = GetTickCount64();
    ULONGLONG zeroAccountSince = 0;
    const ULONGLONG hideUntil = g_startedSteam ? GetTickCount64() + kKeepSteamInTrayMs : 0;
    while (Wait(2)) {
        g_api.RunCallbacks();
        FlushPendingStatOps();
        CleanupLobbyCalls();
        ForwardMessages();
        const ULONGLONG now = GetTickCount64();
        if (now < hideUntil && now - lastHide >= 500) {
            lastHide = now;
            SendSteamToTray();
        }
        if (now - lastDiag >= kDiagMs) {
            lastDiag = now;
            Diagnose();
        }
        if (!IsWine() && g_initialAccountId != 0 && now - lastAccCheck >= 1500) {
            lastAccCheck = now;
            const DWORD curAcc = RegDword(L"Software\\Valve\\Steam\\ActiveProcess", L"ActiveUser");
            if (curAcc != 0 && curAcc != g_initialAccountId) {
                Log("Active Steam account changed (%u -> %u)", g_initialAccountId, curAcc);
                Post(Writer(kAccountChanged).Put<uint64_t>(76561197960265728ULL + (uint64_t)curAcc));
                g_quit = true;
            } else if (curAcc == 0) {
                if (!zeroAccountSince) zeroAccountSince = now;
                if (!SteamRunning() || (now - zeroAccountSince >= 3500)) {
                    Log("Active Steam account signed out");
                    Post(Writer(kAccountChanged).Put<uint64_t>(0));
                    g_quit = true;
                }
            } else {
                zeroAccountSince = 0;
            }
        }
        const bool dirty = g_friendsDirty.load();
        if ((dirty && now - lastFriends >= 1000) || (now - lastFriends >= kFriendRefreshMs)) {
            g_friendsDirty = false;
            lastFriends = now;
            SendFriends();
        }
    }
    bool clearPresence = true;
    if (rpSlotMutex) {
        CloseHandle(rpSlotMutex);
        rpSlotMutex = nullptr;
    }
    if (rpAccId != 0) {
        for (int slot = 0; slot < 16; ++slot) {
            wchar_t mName[96];
            swprintf_s(mName, L"Local\\ReFix_SteamHostRP_%u_%d", (unsigned)rpAccId, slot);
            if (HANDLE other = OpenMutexW(SYNCHRONIZE, FALSE, mName)) {
                CloseHandle(other);
                clearPresence = false;
                break;
            }
        }
    }
    if (clearPresence) PublishPresence(false);
    FlushPendingStatOps();
    g_api.RunCallbacks();
    g_api.UnregisterCallback(&g_statsReceivedCb);
    g_api.UnregisterCallback(&g_sessionRequest);
    g_api.UnregisterCallback(&g_sessionFailed);
    g_api.UnregisterCallback(&g_rpUpdate);
    g_api.UnregisterCallback(&g_avatarLoaded);
    g_api.UnregisterCallback(&g_personaState);
    for (const auto& slot : g_lobbyCallSlots) {
        if (!slot->done && g_api.UnregisterCallResult) g_api.UnregisterCallResult(slot.get(), slot->hCall);
    }
    g_lobbyCallSlots.clear();
    g_api.UnregisterCallback(&g_lobbyChatCb);
    g_api.UnregisterCallback(&g_lobbyDataCb);
    g_api.UnregisterCallback(&g_authTicketCb);
    g_api.UnregisterCallback(&g_webApiTicketCb);
    g_api.Shutdown();
    return 0;   // process exit: Steam stops showing Spacewar
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetEnvironmentVariableW(L"REFIX_HELPER_PROCESS", L"1");
    SetEnvironmentVariableW(L"REFIX_NO_BOOTSTRAP", L"1");
    SetEnvironmentVariableW(L"REFIX_NO_OVERLAY", L"1");
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const int rc = argv ? Run(argc, argv) : 2;
    if (argv) LocalFree(argv);
    return rc;
}
