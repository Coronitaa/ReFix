#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "steam_link.h"

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

extern void ReFixLog(const char* fmt, ...);

namespace refix::steamlink {
namespace {

// --- Valve flat API, resolved from our private copy of steam_api64.dll -------------------------------------
// Minimal layouts of the SDK structs we read (only the leading fields, which are stable since SDK 1.48).
struct NetIdentity {                 // SteamNetworkingIdentity
    int32_t type;                    // 16 = k_ESteamNetworkingIdentityType_SteamID
    int32_t size;
    union { uint64_t steamId; char raw[128]; };
};
static_assert(sizeof(NetIdentity) == 136, "SteamNetworkingIdentity layout");
struct NetMessage {                  // SteamNetworkingMessage_t (leading fields)
    void* data;
    int32_t size;
    uint32_t conn;
    NetIdentity peer;
};
struct CallbackMsg {                 // CallbackMsg_t
    int32_t user;
    int32_t id;
    uint8_t* param;
    int32_t paramSize;
};
struct FriendGameInfo {              // FriendGameInfo_t
    uint64_t gameId;
    uint32_t ip;
    uint16_t port, queryPort;
    uint64_t lobby;
};

constexpr int kChannel = 47;                       // ISteamNetworkingMessages channel of the tunnel
constexpr int kSendFlags = 1 | 32;                 // UnreliableNoNagle | AutoRestartBrokenSession
constexpr int kCbSessionRequest = 1251;            // SteamNetworkingMessagesSessionRequest_t
constexpr int kFriendFlagImmediate = 4;
constexpr uint8_t kFlagBroadcast = 1;
constexpr uint8_t kFlagToLan = 2;                  // bridge: deliver on your LAN to the IPv4 that follows
constexpr uint8_t kFlagFromLan = 4;                // from a LAN player behind the sender, whose IPv4 follows
constexpr uint16_t kDiscoveryPortFirst = 47584;    // LanCore discovery ports (refix_lan_transport.cpp)
constexpr uint16_t kDiscoveryPortLast = 47599;
constexpr DWORD kSteamStartTimeoutMs = 120000;
constexpr DWORD kPumpIntervalMs = 2;
constexpr DWORD kInitRetryMs = 2000;
constexpr ULONGLONG kFriendRefreshMs = 3000;
constexpr size_t kHeader = 5;                      // srcPort u16, dstPort u16, flags u8
constexpr size_t kMaxDatagram = 65535;

struct Api {
    HMODULE dll = nullptr;
    bool (*Init)() = nullptr;
    void (*Shutdown)() = nullptr;
    void (*DispatchInit)() = nullptr;
    void (*RunFrame)(int32_t) = nullptr;
    bool (*NextCallback)(int32_t, CallbackMsg*) = nullptr;
    void (*FreeCallback)(int32_t) = nullptr;
    int32_t (*Pipe)() = nullptr;
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
    bool (*HasFriend)(void*, uint64_t, int) = nullptr;
    int (*MediumAvatar)(void*, uint64_t) = nullptr;
    void* (*UtilsIface)() = nullptr;
    bool (*ImageSize)(void*, int, uint32_t*, uint32_t*) = nullptr;
    bool (*ImageRgba)(void*, int, uint8_t*, int) = nullptr;
};

Api g_api;
void* g_msgs = nullptr;
void* g_friends = nullptr;
void* g_utils = nullptr;
std::string g_gameName;                            // guarded by g_mu
std::atomic<State> g_state{ State::Off };
std::atomic<bool> g_wanted{ false };
std::atomic<uint32_t> g_appId{ 0 };
std::atomic<bool> g_steamOverlay{ false };
std::atomic<uint64_t> g_selfId{ 0 };
std::mutex g_sinkMu;                               // held while sinks run, so RemoveSink waits for them
std::mutex g_mu;                                   // everything below
std::string g_error;
HANDLE g_thread = nullptr;                         // link thread, guarded by g_enableMu
std::mutex g_enableMu;
std::vector<std::pair<Sink, void*>> g_sinks;       // guarded by g_sinkMu
std::vector<Friend> g_friendList;
std::unordered_map<uint64_t, uint32_t> g_ipOf;     // SteamID -> virtual IP
std::unordered_map<uint32_t, uint64_t> g_idOf;     // virtual IP -> SteamID
uint32_t g_nextHost = 1;
struct Relay { uint64_t gateway; uint32_t lanIp; };
std::unordered_map<uint32_t, Relay> g_relayOf;     // virtual IP -> LAN player behind a friend
std::map<std::pair<uint64_t, uint32_t>, uint32_t> g_relayIp;
std::unordered_map<uint64_t, std::pair<uint32_t, uint16_t>> g_advertised;   // friend -> its own LAN data endpoint

struct Bridge { SOCKET s; uint16_t port; };        // LAN-facing socket of one friend's socket
std::mutex g_bridgeMu;                             // the bridge state below
std::map<std::pair<uint64_t, uint16_t>, Bridge> g_bridges;   // (friend, friend's port) -> socket
std::atomic<uint32_t> g_lanIp{ 0 };
std::atomic<LanRewriter> g_rewriter{ nullptr };
std::atomic<SocketCallback> g_sockCb{ nullptr };

void SetError(const std::string& e) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_error = e;
    ReFixLog("[SteamLink] %s", e.c_str());
}

std::wstring RegString(const wchar_t* key, const wchar_t* value) {
    wchar_t buf[MAX_PATH] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return {};
    return buf;
}

DWORD RegDword(const wchar_t* key, const wchar_t* value) {
    DWORD v = 0, size = sizeof(v);
    return RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_DWORD, nullptr, &v, &size) == ERROR_SUCCESS ? v : 0;
}

bool ProcessAlive(DWORD pid) {
    HANDLE h = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    if (!h) return false;
    const bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
}

// Steam is up and signed in (the same registry values steam_api checks before it connects).
bool SteamSignedIn() {
    const wchar_t* key = L"Software\\Valve\\Steam\\ActiveProcess";
    return ProcessAlive(RegDword(key, L"pid")) && RegDword(key, L"ActiveUser") != 0;
}

bool StartSteamAndWait() {
    if (SteamSignedIn()) return true;
    const std::wstring exe = RegString(L"Software\\Valve\\Steam", L"SteamExe");
    if (exe.empty()) {
        SetError("Steam is not installed");
        return false;
    }
    if (!ProcessAlive(RegDword(L"Software\\Valve\\Steam\\ActiveProcess", L"pid"))) {
        ReFixLog("[SteamLink] Starting Steam in the background");
        if ((INT_PTR)ShellExecuteW(nullptr, L"open", exe.c_str(), L"-silent", nullptr, SW_SHOWMINNOACTIVE) <= 32) {
            SetError("Could not start Steam");
            return false;
        }
    }
    for (DWORD waited = 0; waited < kSteamStartTimeoutMs && g_wanted; waited += 500) {
        if (SteamSignedIn()) {
            Sleep(2000);   // steamclient needs a moment after login before it accepts API clients
            return true;
        }
        Sleep(500);
    }
    SetError(g_wanted ? "Steam did not sign in in time" : "Cancelled");
    return false;
}

std::wstring ModuleDir() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&SteamInstalled), &self);
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring dir = path;
    return dir.substr(0, dir.find_last_of(L"\\/") + 1);
}

template <class F> bool Resolve(F& fn, const char* name) {
    fn = reinterpret_cast<F>(GetProcAddress(g_api.dll, name));
    return fn != nullptr;
}

template <class F> bool ResolveAny(F& fn, std::initializer_list<const char*> names) {
    for (const char* n : names) if (Resolve(fn, n)) return true;
    return false;
}

// ReFix ships Valve's DLL as refix_steamlink(64).dll; the game's own genuine copy is the fallback.
#ifdef _WIN64
constexpr const wchar_t* kLinkDll = L"refix_steamlink64.dll";
constexpr const wchar_t* kGameDll = L"steam_api64_original.dll";
#else
constexpr const wchar_t* kLinkDll = L"refix_steamlink.dll";
constexpr const wchar_t* kGameDll = L"steam_api_original.dll";
#endif

bool LoadValve() {
    const std::wstring dir = ModuleDir();
    for (const std::wstring& name : { std::wstring(kLinkDll), std::wstring(kGameDll), L"ReFix\\" + std::wstring(kLinkDll) }) {
        const std::wstring path = dir + name;
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        g_api.dll = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!g_api.dll) continue;
        bool ok = Resolve(g_api.Init, "SteamAPI_Init") && Resolve(g_api.Shutdown, "SteamAPI_Shutdown") &&
                  Resolve(g_api.DispatchInit, "SteamAPI_ManualDispatch_Init") &&
                  Resolve(g_api.RunFrame, "SteamAPI_ManualDispatch_RunFrame") &&
                  Resolve(g_api.NextCallback, "SteamAPI_ManualDispatch_GetNextCallback") &&
                  Resolve(g_api.FreeCallback, "SteamAPI_ManualDispatch_FreeLastCallback") &&
                  Resolve(g_api.Pipe, "SteamAPI_GetHSteamPipe") &&
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
            ReFixLog("[SteamLink] Using %ls", path.c_str());
            return true;
        }
        ReFixLog("[SteamLink] %ls lacks the Steam networking API; skipped", path.c_str());
        FreeLibrary(g_api.dll);
        g_api = Api{};
    }
    SetError("Steam API library not found (refix_steamlink DLL)");
    return false;
}

// SteamAPI_Init reads the AppID from the environment: Spacewar for this call only, so the game's
// own view of SteamAppId (read by the emulator and the EOS proxy) is left untouched.
bool InitAsSpacewar() {
    wchar_t prevApp[32] = {}, prevGame[32] = {};
    const bool hadApp = GetEnvironmentVariableW(L"SteamAppId", prevApp, 32) > 0;
    const bool hadGame = GetEnvironmentVariableW(L"SteamGameId", prevGame, 32) > 0;
    SetEnvironmentVariableW(L"SteamAppId", L"480");
    SetEnvironmentVariableW(L"SteamGameId", L"480");
    g_api.DispatchInit();
    const bool ok = g_api.Init();
    SetEnvironmentVariableW(L"SteamAppId", hadApp ? prevApp : nullptr);
    SetEnvironmentVariableW(L"SteamGameId", hadGame ? prevGame : nullptr);
    if (!ok) {
        SetError("SteamAPI_Init failed (is Steam signed in?)");
        return false;
    }
    g_msgs = g_api.Messages();
    g_friends = g_api.FriendsIface();
    void* user = g_api.UserIface();
    if (!g_msgs || !g_friends || !user) {
        SetError("Steam interfaces unavailable");
        g_api.Shutdown();
        return false;
    }
    g_selfId = g_api.GetSteamID(user);
    g_utils = g_api.UtilsIface();
    return true;
}

NetIdentity IdentityOf(uint64_t steamId) {
    NetIdentity id{};
    id.type = 16;
    id.size = 8;
    id.steamId = steamId;
    return id;
}

uint32_t VirtualIpLocked(uint64_t steamId) {
    auto it = g_ipOf.find(steamId);
    if (it != g_ipOf.end()) return it->second;
    if (g_nextHost >= 0xFFFF) return 0;
    const uint32_t ip = kVirtualNet | g_nextHost++;
    g_ipOf[steamId] = ip;
    g_idOf[ip] = steamId;
    return ip;
}

void PublishPresence(bool on) {
    if (!g_friends) return;
    g_api.SetRP(g_friends, "refix", on ? "1" : "");
    const std::string app = on ? std::to_string(g_appId.load()) : std::string();
    g_api.SetRP(g_friends, "refix_app", app.c_str());
    std::string name;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (on) name = g_gameName.substr(0, 120);   // rich presence values are short
    }
    g_api.SetRP(g_friends, "refix_game", name.c_str());
}

void RefreshFriends() {
    std::vector<Friend> list;
    const int n = g_api.FriendCount(g_friends, kFriendFlagImmediate);
    for (int i = 0; i < n; ++i) {
        Friend f;
        f.steamId = g_api.FriendByIndex(g_friends, i, kFriendFlagImmediate);
        if (!f.steamId) continue;
        const char* name = g_api.FriendName(g_friends, f.steamId);
        f.name = name ? name : "";
        f.personaState = g_api.FriendState(g_friends, f.steamId);
        FriendGameInfo game{};
        f.playing = g_api.FriendGame(g_friends, f.steamId, &game) && game.gameId != 0;
        if (f.playing) f.steamGameId = (uint32_t)(game.gameId & 0xFFFFFF);
        const char* rp = g_api.FriendRP(g_friends, f.steamId, "refix");
        f.refix = f.playing && f.steamGameId == 480 && rp && rp[0] == '1';
        if (f.refix) {
            f.appId = strtoul(g_api.FriendRP(g_friends, f.steamId, "refix_app"), nullptr, 10);
            const char* gname = g_api.FriendRP(g_friends, f.steamId, "refix_game");
            f.gameName = gname ? gname : "";
        }
        f.avatar = g_api.MediumAvatar(g_friends, f.steamId);   // -1 while loading, 0 when none
        if (f.avatar < 0) f.avatar = 0;
        list.push_back(std::move(f));
    }
    std::lock_guard<std::mutex> lock(g_mu);
    for (Friend& f : list) if (f.refix) f.virtIp = VirtualIpLocked(f.steamId);
    g_friendList = std::move(list);
}

void HandleCallbacks(int32_t pipe) {
    g_api.RunFrame(pipe);
    CallbackMsg cb{};
    while (g_api.NextCallback(pipe, &cb)) {
        if (cb.id == kCbSessionRequest && cb.param && cb.paramSize >= (int)sizeof(NetIdentity)) {
            const auto* remote = reinterpret_cast<const NetIdentity*>(cb.param);
            // Only Steam friends may open a tunnel to us.
            if (remote->type == 16 && g_api.HasFriend(g_friends, remote->steamId, kFriendFlagImmediate))
                g_api.AcceptSession(g_msgs, remote);
        }
        g_api.FreeCallback(pipe);
    }
}

bool SendRaw(uint64_t steamId, uint16_t srcPort, uint16_t dstPort, uint8_t flags, const void* data, size_t len,
             uint32_t lanIp = 0);

uint32_t RelayIpLocked(uint64_t gateway, uint32_t lanIp) {
    auto it = g_relayIp.find({ gateway, lanIp });
    if (it != g_relayIp.end()) return it->second;
    if (g_nextHost >= 0xFFFF) return 0;
    const uint32_t ip = kVirtualNet | g_nextHost++;
    g_relayIp[{ gateway, lanIp }] = ip;
    g_relayOf[ip] = Relay{ gateway, lanIp };
    return ip;
}

// LAN-facing socket that stands for one socket of a friend on this LAN. Caller holds g_bridgeMu.
const Bridge* BridgeLocked(uint64_t friendId, uint16_t friendPort) {
    auto it = g_bridges.find({ friendId, friendPort });
    if (it != g_bridges.end()) return &it->second;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return nullptr;
    if (auto cb = g_sockCb.load()) cb((uintptr_t)s, true);
    BOOL on = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(on));
    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    int addrLen = sizeof(addr);
    if (bind(s, (const sockaddr*)&addr, sizeof(addr)) != 0 || getsockname(s, (sockaddr*)&addr, &addrLen) != 0) {
        if (auto cb = g_sockCb.load()) cb((uintptr_t)s, false);
        closesocket(s);
        return nullptr;
    }
    const Bridge b{ s, ntohs(addr.sin_port) };
    ReFixLog("[SteamLink] Bridge port %u on the LAN for %llu:%u", b.port, (unsigned long long)friendId, friendPort);
    return &(g_bridges[{ friendId, friendPort }] = b);
}

void SendFrom(SOCKET s, uint32_t ip, uint16_t port, const std::vector<uint8_t>& data) {
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(ip);
    to.sin_port = htons(port);
    sendto(s, (const char*)data.data(), (int)data.size(), 0, (const sockaddr*)&to, sizeof(to));
}

// A friend's datagram goes out on this LAN from the friend's bridge socket, with the endpoint it advertises
// (its own unreachable LAN address) replaced by the bridge's. `broadcast`: a LanCore discovery broadcast,
// repeated on the discovery ports; anything else is not repeated.
void BridgeToLan(uint64_t from, uint16_t srcPort, uint32_t ip, uint16_t port, const uint8_t* data, size_t n, bool broadcast) {
    const uint32_t lanIp = g_lanIp;
    if (!lanIp) return;
    std::vector<uint8_t> copy(data, data + n);
    std::lock_guard<std::mutex> lock(g_bridgeMu);
    const Bridge* b = BridgeLocked(from, srcPort);
    if (!b) return;
    const LanRewriter rewrite = g_rewriter;
    const bool rewritten = rewrite && rewrite(copy.data(), copy.size(), lanIp, b->port);
    if (!broadcast) {
        SendFrom(b->s, ip, port, copy);
        return;
    }
    if (!rewritten) return;
    const uint32_t subnet = (lanIp & 0xFFFFFF00u) | 0xFFu;
    for (uint16_t p = kDiscoveryPortFirst; p <= kDiscoveryPortLast; ++p) {
        for (uint32_t target : { 0xFFFFFFFFu, subnet, 0x7F000001u }) SendFrom(b->s, target, p, copy);
    }
}

// LAN players answering a friend's bridge socket: their datagrams go to that friend, tagged with their address.
void PumpBridges() {
    struct Out { uint64_t friendId; uint16_t friendPort; uint32_t ip; uint16_t port; std::vector<uint8_t> data; };
    std::vector<Out> out;
    {
        std::lock_guard<std::mutex> lock(g_bridgeMu);
        static thread_local std::vector<uint8_t> buf(kMaxDatagram);
        for (auto& [key, b] : g_bridges) {
            sockaddr_in from{};
            int fromLen = sizeof(from);
            int n;
            while ((n = recvfrom(b.s, (char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fromLen)) > 0) {
                out.push_back({ key.first, key.second, ntohl(from.sin_addr.s_addr), ntohs(from.sin_port),
                                std::vector<uint8_t>(buf.begin(), buf.begin() + n) });
                fromLen = sizeof(from);
            }
        }
    }
    for (auto& o : out) SendRaw(o.friendId, o.port, o.friendPort, kFlagFromLan, o.data.data(), o.data.size(), o.ip);
}

void CloseBridges() {
    std::lock_guard<std::mutex> lock(g_bridgeMu);
    for (auto& [key, b] : g_bridges) {
        if (auto cb = g_sockCb.load()) cb((uintptr_t)b.s, false);
        closesocket(b.s);
    }
    g_bridges.clear();
}

void Deliver(uint64_t from, const uint8_t* buf, size_t len) {
    if (len < kHeader) return;
    const uint16_t srcPort = (uint16_t)(buf[0] | (buf[1] << 8));
    const uint16_t dstPort = (uint16_t)(buf[2] | (buf[3] << 8));
    const uint8_t flags = buf[4];
    size_t off = kHeader;
    uint32_t lanIp = 0;
    if (flags & (kFlagToLan | kFlagFromLan)) {
        if (len < kHeader + 4) return;
        memcpy(&lanIp, buf + kHeader, 4);
        off += 4;
    }
    const uint8_t* data = buf + off;
    const size_t n = len - off;
    if (flags & kFlagToLan) {
        BridgeToLan(from, srcPort, lanIp, dstPort, data, n, false);
        return;
    }
    uint32_t ip;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        ip = (flags & kFlagFromLan) ? RelayIpLocked(from, lanIp) : VirtualIpLocked(from);
    }
    if (!ip) return;
    {
        std::lock_guard<std::mutex> sinkLock(g_sinkMu);
        for (auto& s : g_sinks) s.first(s.second, ip, srcPort, dstPort, data, n);
    }
    // A friend's own LanCore discovery broadcast is repeated on this LAN, so LAN-only players see the friend.
    if ((flags & kFlagBroadcast) && !(flags & kFlagFromLan) && dstPort == 0) BridgeToLan(from, srcPort, 0, 0, data, n, true);
}

void ReceiveAll() {
    NetMessage* msgs[32];
    int n;
    while ((n = g_api.RecvMsgs(g_msgs, kChannel, msgs, 32)) > 0) {
        for (int i = 0; i < n; ++i) {
            if (msgs[i]->peer.type == 16 && msgs[i]->size > 0)
                Deliver(msgs[i]->peer.steamId, static_cast<const uint8_t*>(msgs[i]->data), (size_t)msgs[i]->size);
            g_api.Release(msgs[i]);
        }
    }
}

// Steam's own overlay, only when ReFix.ini asks for it (the ReFix overlay replaces it by default). Same environment
// GameOverlayRenderer expects from a Steam launch, as Spacewar; SteamAppId/SteamGameId go back afterwards.
void LoadSteamOverlay() {
    const std::wstring steam = RegString(L"Software\\Valve\\Steam", L"SteamPath");
    if (steam.empty()) return;
#ifdef _WIN64
    const std::wstring dll = steam + L"\\GameOverlayRenderer64.dll";
#else
    const std::wstring dll = steam + L"\\GameOverlayRenderer.dll";
#endif
    wchar_t prevApp[32] = {}, prevGame[32] = {};
    const bool hadApp = GetEnvironmentVariableW(L"SteamAppId", prevApp, 32) > 0;
    const bool hadGame = GetEnvironmentVariableW(L"SteamGameId", prevGame, 32) > 0;
    SetEnvironmentVariableW(L"SteamAppId", L"480");
    SetEnvironmentVariableW(L"SteamGameId", L"480");
    SetEnvironmentVariableW(L"SteamOverlayGameId", L"480");
    SetEnvironmentVariableW(L"_SteamInjectionPIDs", std::to_wstring(GetCurrentProcessId()).c_str());
    SetEnvironmentVariableW(L"ENABLE_STEAM_OVERLAY", L"1");
    const HMODULE h = LoadLibraryW(dll.c_str());
    SetEnvironmentVariableW(L"SteamAppId", hadApp ? prevApp : nullptr);
    SetEnvironmentVariableW(L"SteamGameId", hadGame ? prevGame : nullptr);
    ReFixLog("[SteamLink] Steam overlay %s (%lu)", h ? "loaded" : "failed to load", h ? 0UL : GetLastError());
}

// Steam that ReFix has just opened still shows the previous session in the registry for a moment and refuses API
// clients until the login finishes: retry until it accepts us.
bool InitWhenReady() {
    for (DWORD waited = 0; g_wanted; waited += kInitRetryMs) {
        if (SteamSignedIn() && InitAsSpacewar()) return true;
        if (waited >= kSteamStartTimeoutMs) return false;
        Sleep(kInitRetryMs);
    }
    return false;
}

void LinkThread() {
    if (!StartSteamAndWait() || !g_wanted || (!g_api.dll && !LoadValve()) || !InitWhenReady()) {
        g_state = g_wanted ? State::Failed : State::Off;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_error.clear();   // failed attempts while Steam was still signing in
    }
    const int32_t pipe = g_api.Pipe();
    PublishPresence(true);
    RefreshFriends();
    g_state = State::Online;
    if (g_steamOverlay && !GetModuleHandleW(sizeof(void*) == 8 ? L"GameOverlayRenderer64.dll" : L"GameOverlayRenderer.dll"))
        LoadSteamOverlay();
    ReFixLog("[SteamLink] Online as %llu (Spacewar), app %u", (unsigned long long)g_selfId.load(), g_appId.load());
    ULONGLONG lastFriends = GetTickCount64();
    while (g_wanted) {
        HandleCallbacks(pipe);
        ReceiveAll();
        PumpBridges();
        const ULONGLONG now = GetTickCount64();
        if (now - lastFriends >= kFriendRefreshMs) {
            lastFriends = now;
            RefreshFriends();
        }
        Sleep(kPumpIntervalMs);
    }
    PublishPresence(false);
    CloseBridges();
    HandleCallbacks(pipe);
    g_api.Shutdown();   // Steam stops showing Spacewar; Enable() can init again later
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_friendList.clear();
    }
    g_state = State::Off;
    ReFixLog("[SteamLink] Off");
}

bool SendRaw(uint64_t steamId, uint16_t srcPort, uint16_t dstPort, uint8_t flags, const void* data, size_t len,
             uint32_t lanIp) {
    const size_t head = kHeader + ((flags & (kFlagToLan | kFlagFromLan)) ? 4 : 0);
    if (g_state != State::Online || len + head > kMaxDatagram) return false;
    thread_local std::vector<uint8_t> buf;
    buf.resize(len + head);
    buf[0] = (uint8_t)srcPort;
    buf[1] = (uint8_t)(srcPort >> 8);
    buf[2] = (uint8_t)dstPort;
    buf[3] = (uint8_t)(dstPort >> 8);
    buf[4] = flags;
    if (head > kHeader) memcpy(buf.data() + kHeader, &lanIp, 4);
    if (len) memcpy(buf.data() + head, data, len);
    const NetIdentity id = IdentityOf(steamId);
    return g_api.SendMsg(g_msgs, &id, buf.data(), (uint32_t)buf.size(), kSendFlags, kChannel) == 1;  // k_EResultOK
}

} // namespace

bool SteamInstalled() {
    const std::wstring exe = RegString(L"Software\\Valve\\Steam", L"SteamExe");
    return !exe.empty() && GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void SetLocalGame(uint32_t appId) { g_appId = appId; }

void SetLocalGameName(const std::string& utf8Name) {
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_gameName == utf8Name) return;
        g_gameName = utf8Name;
    }
    if (g_state == State::Online) PublishPresence(true);
}

bool AvatarRgba(int image, int& width, int& height, std::vector<uint8_t>& rgba) {
    if (g_state != State::Online || image <= 0 || !g_utils) return false;
    uint32_t w = 0, h = 0;
    if (!g_api.ImageSize(g_utils, image, &w, &h) || w == 0 || h == 0 || w > 512 || h > 512) return false;
    rgba.resize((size_t)w * h * 4);
    if (!g_api.ImageRgba(g_utils, image, rgba.data(), (int)rgba.size())) return false;
    width = (int)w;
    height = (int)h;
    return true;
}

void SetSteamOverlay(bool on) { g_steamOverlay = on; }

void Enable() {
    std::lock_guard<std::mutex> enableLock(g_enableMu);
    if (g_wanted) return;
    if (g_thread) {   // a previous Disable() may still be shutting Steam down
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_error.clear();
    }
    g_wanted = true;
    g_state = State::Starting;
    g_thread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD { LinkThread(); return 0; }, nullptr, 0, nullptr);
    if (!g_thread) {
        g_wanted = false;
        g_state = State::Failed;
        SetError("Could not start the Steam link thread");
    }
}

// The link thread notices within one pump tick (or 500 ms while waiting for Steam) and shuts Steam down
// itself, so the UI thread never blocks; Enable() joins it before starting again.
void Disable() { g_wanted = false; }

State GetState() { return g_state; }

std::string LastError() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_error;
}

uint64_t LocalSteamId() { return g_selfId; }

bool Send(uint32_t virtIp, uint16_t srcPort, uint16_t dstPort, const void* data, size_t len) {
    uint64_t id = 0;
    Relay relay{};
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto it = g_idOf.find(virtIp);
        if (it != g_idOf.end()) {
            id = it->second;
        } else {
            auto r = g_relayOf.find(virtIp);
            if (r == g_relayOf.end()) return false;
            relay = r->second;
        }
    }
    if (id) return SendRaw(id, srcPort, dstPort, 0, data, len);
    return SendRaw(relay.gateway, srcPort, dstPort, kFlagToLan, data, len, relay.lanIp);   // a LAN player behind a friend
}

void SetLanSide(uint32_t lanIp, LanRewriter rewriter) {
    g_lanIp = lanIp;
    g_rewriter = rewriter;
}

void SetSocketCallback(SocketCallback cb) { g_sockCb = cb; }

void ForwardFromLan(uint32_t ip, uint16_t port, const void* wire, size_t len) {
    if (g_state != State::Online || IsVirtual(ip)) return;
    std::vector<uint64_t> peers;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        for (const Friend& f : g_friendList) if (f.refix) peers.push_back(f.steamId);
    }
    for (uint64_t id : peers) SendRaw(id, port, 0, kFlagBroadcast | kFlagFromLan, wire, len, ip);
}

bool BridgeEndpoint(uint32_t virtIp, uint16_t port, uint32_t& lanIp, uint16_t& lanPort) {
    if (g_state != State::Online || !g_lanIp) return false;
    uint64_t id;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto it = g_idOf.find(virtIp);
        if (it == g_idOf.end()) return false;
        id = it->second;
    }
    std::lock_guard<std::mutex> lock(g_bridgeMu);
    const Bridge* b = BridgeLocked(id, port);
    if (!b) return false;
    lanIp = g_lanIp;
    lanPort = b->port;
    return true;
}

bool IsBridgePort(uint16_t port) {
    std::lock_guard<std::mutex> lock(g_bridgeMu);
    for (const auto& [key, b] : g_bridges) if (b.port == port) return true;
    return false;
}

uint32_t RelayAddress(uint32_t viaVirtIp, uint32_t lanIp, uint16_t lanPort) {
    std::lock_guard<std::mutex> lock(g_mu);
    uint64_t gateway = 0;
    if (auto it = g_idOf.find(viaVirtIp); it != g_idOf.end()) gateway = it->second;
    else if (auto r = g_relayOf.find(viaVirtIp); r != g_relayOf.end()) gateway = r->second.gateway;
    if (!gateway) return 0;
    auto adv = g_advertised.find(gateway);
    if (adv != g_advertised.end() && adv->second == std::make_pair(lanIp, lanPort)) return VirtualIpLocked(gateway);
    return RelayIpLocked(gateway, lanIp);
}

void NoteAdvertised(uint32_t virtIp, uint32_t lanIp, uint16_t lanPort) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (auto it = g_idOf.find(virtIp); it != g_idOf.end()) g_advertised[it->second] = { lanIp, lanPort };
}

void Broadcast(uint16_t srcPort, uint16_t dstPort, const void* data, size_t len) {
    if (g_state != State::Online) return;
    std::vector<uint64_t> peers;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        for (const Friend& f : g_friendList) if (f.refix) peers.push_back(f.steamId);
    }
    for (uint64_t id : peers) SendRaw(id, srcPort, dstPort, kFlagBroadcast, data, len);
}

void AddSink(Sink sink, void* ctx) {
    std::lock_guard<std::mutex> lock(g_sinkMu);
    g_sinks.emplace_back(sink, ctx);
}

void RemoveSink(Sink sink, void* ctx) {
    std::lock_guard<std::mutex> lock(g_sinkMu);
    for (auto it = g_sinks.begin(); it != g_sinks.end(); ++it) {
        if (it->first == sink && it->second == ctx) {
            g_sinks.erase(it);
            return;
        }
    }
}

std::vector<Friend> Friends() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_state == State::Online ? g_friendList : std::vector<Friend>();
}

uint64_t SteamIdOf(uint32_t virtIp) {
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_idOf.find(virtIp);
    return it == g_idOf.end() ? 0 : it->second;
}

} // namespace refix::steamlink
