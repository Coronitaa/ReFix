#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include "steam_link.h"
#include "steam_host_proto.h"
#include "../mods/mod_host.h"
#include "../refix_wine.h"
#include "../overlay/overlay_settings.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

extern void ReFixLog(const char* fmt, ...);

namespace refix::steamlink {
namespace {

// The Steam side runs in refix_steamhost(64).exe (steam_host.cpp): Steam shows a process as playing Spacewar
// until it exits, so turning Steam online off ends that process. This side keeps the tunnel and bridge logic.
constexpr uint8_t kFlagBroadcast = 1;
constexpr uint8_t kFlagToLan = 2;                  // bridge: deliver on your LAN to the IPv4 that follows
constexpr uint8_t kFlagFromLan = 4;                // from a LAN player behind the sender, whose IPv4 follows
constexpr uint16_t kDiscoveryPortFirst = 47584;    // LanCore discovery ports (refix_lan_transport.cpp)
constexpr uint16_t kDiscoveryPortLast = 47599;
constexpr DWORD kPumpIntervalMs = 2;
constexpr DWORD kHelloTimeoutMs = 10000;
constexpr DWORD kQuitWaitMs = 5000;
constexpr DWORD kAvatarRetryMs = 2000;
constexpr size_t kHeader = 5;                      // srcPort u16, dstPort u16, flags u8
constexpr size_t kMaxDatagram = 65535;

SOCKET g_ipc = INVALID_SOCKET;                     // loopback socket to the helper (link thread; sendto is thread-safe)
sockaddr_in g_host{};                              // helper's address, set by its hello
std::atomic<bool> g_hostKnown{ false };
SOCKET g_maskIpc = INVALID_SOCKET;                 // loopback socket to companion --mask helper
sockaddr_in g_maskHost{};
bool g_maskHostKnown = false;
std::atomic<bool> g_maskOnline{ false };
std::atomic<uint32_t> g_maskAppId{ 0 };
std::mutex g_statMu;
std::vector<std::vector<uint8_t>> g_statQueue;
std::atomic<int> g_step{ 0 };                      // proto::Step while Starting
struct Avatar { int w = 0, h = 0; std::vector<uint8_t> rgba; };
std::map<int, Avatar> g_avatars;                   // guarded by g_mu
std::map<int, ULONGLONG> g_avatarAsked;            // guarded by g_mu
std::string g_gameName;                            // guarded by g_mu
std::atomic<State> g_state{ State::Off };
std::atomic<bool> g_wanted{ false };
std::atomic<uint32_t> g_appId{ 0 };
std::atomic<uint32_t> g_bridgeAppId{ 480 };
std::atomic<bool> g_steamOverlay{ false };
std::atomic<uint64_t> g_selfId{ 0 };
std::mutex g_probeMu;
LicenseProbe g_probe;
std::mutex g_appLicMu;
std::condition_variable g_appLicCv;
std::unordered_map<uint32_t, bool> g_appLicMap;

struct AuthWaiter {
    bool done = false;
    int32_t eResult = 0;
    uint32_t hostHandle = 0;
    std::vector<uint8_t> ticket;
};
std::mutex g_authMu;
std::condition_variable g_authCv;
std::unordered_map<uint32_t, AuthWaiter> g_authWaiters;
std::atomic<uint32_t> g_nextAuthReqId{ 1 };

struct LobbyWaiter {
    bool done = false;
    bool failed = false;
    int32_t eResult = 0;
    uint64_t lobbyId = 0;
    uint32_t chatPerms = 0;
    bool locked = false;
    uint32_t enterResp = 2;
    std::vector<uint64_t> order;
    std::unordered_map<uint64_t, OfficialLobbySnapshot> snaps;
};
std::mutex g_lobbyMu;
std::condition_variable g_lobbyCv;
std::unordered_map<uint32_t, LobbyWaiter> g_lobbyWaiters;
std::atomic<LobbyUpdateCb> g_onLobbySnap{ nullptr };
std::atomic<LobbyDataChangedCb> g_onLobbyData{ nullptr };
std::atomic<LobbyChatChangedCb> g_onLobbyChat{ nullptr };
std::atomic<AccountChangedCb> g_accountChangedCb{ nullptr };
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

std::wstring ModuleDir() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&SteamInstalled), &self);
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring dir = path;
    return dir.substr(0, dir.find_last_of(L"\\/") + 1);
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

void ToHost(const proto::Writer& w) {
    if (g_hostKnown && w.b.size() <= proto::kMaxDatagram)
        sendto(g_ipc, (const char*)w.b.data(), (int)w.b.size(), 0, (const sockaddr*)&g_host, sizeof(g_host));
}

void PublishPresence() {
    std::string name;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        name = g_gameName.substr(0, 120);   // rich presence values are short
    }
    auto w = proto::Writer(proto::kPresence).Put<uint32_t>(g_appId.load()).Str(name);
    for (uint32_t alt : refix::mods::GameAppIds()) {
        if (alt != g_appId.load()) w.Put<uint32_t>(alt);
    }
    ToHost(w);
}

void ReadFriends(proto::Reader& r) {
    std::vector<Friend> list;
    const uint16_t n = r.Get<uint16_t>();
    for (uint16_t i = 0; i < n && r.ok; ++i) {
        Friend f;
        f.steamId = r.Get<uint64_t>();
        f.personaState = r.Get<int32_t>();
        f.playing = r.Get<uint8_t>() != 0;
        f.steamGameId = r.Get<uint32_t>();
        f.refix = r.Get<uint8_t>() != 0;
        f.appId = r.Get<uint32_t>();
        f.avatar = r.Get<int32_t>();
        f.name = r.Str();
        f.gameName = r.Str();
        if (r.ok && f.steamId) list.push_back(std::move(f));
    }
    std::lock_guard<std::mutex> lock(g_mu);
    for (Friend& f : list) if (f.refix) f.virtIp = VirtualIpLocked(f.steamId);
    g_friendList = std::move(list);
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
    const uint32_t lanIp = g_lanIp ? g_lanIp.load() : 0x7F000001u;
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
        static std::set<uint64_t> heard;   // the two-PC diagnosis: does anything arrive from this friend at all?
        if (heard.insert(from).second) ReFixLog("[SteamLink] First datagram through the tunnel from %llu", (unsigned long long)from);
    }
    if (!ip) return;
    {
        std::lock_guard<std::mutex> sinkLock(g_sinkMu);
        for (auto& s : g_sinks) s.first(s.second, ip, srcPort, dstPort, data, n);
    }
    // A friend's own LanCore discovery broadcast is repeated on this LAN, so LAN-only players see the friend.
    if ((flags & kFlagBroadcast) && !(flags & kFlagFromLan) && dstPort == 0) BridgeToLan(from, srcPort, 0, 0, data, n, true);
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
    const std::wstring bridgeStr = std::to_wstring(g_bridgeAppId.load() ? g_bridgeAppId.load() : 480);
    SetEnvironmentVariableW(L"SteamAppId", bridgeStr.c_str());
    SetEnvironmentVariableW(L"SteamGameId", bridgeStr.c_str());
    SetEnvironmentVariableW(L"SteamOverlayGameId", bridgeStr.c_str());
    SetEnvironmentVariableW(L"_SteamInjectionPIDs", std::to_wstring(GetCurrentProcessId()).c_str());
    SetEnvironmentVariableW(L"ENABLE_STEAM_OVERLAY", L"1");
    const HMODULE h = LoadLibraryW(dll.c_str());
    SetEnvironmentVariableW(L"SteamAppId", hadApp ? prevApp : nullptr);
    SetEnvironmentVariableW(L"SteamGameId", hadGame ? prevGame : nullptr);
    ReFixLog("[SteamLink] Steam overlay %s (%lu)", h ? "loaded" : "failed to load", h ? 0UL : GetLastError());
}

#ifdef _WIN64
constexpr const wchar_t* kHostExe = L"refix_steamhost64.exe";
#else
constexpr const wchar_t* kHostExe = L"refix_steamhost.exe";
#endif

std::wstring BuildCleanHostEnv() {
    std::wstring env;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (const wchar_t* v = block; *v; v += wcslen(v) + 1) {
            if ((!ReFixIsWine() && _wcsnicmp(v, L"Steam", 5) == 0) ||
                (ReFixIsWine() && (_wcsnicmp(v, L"SteamAppId=", 11) == 0 ||
                                  _wcsnicmp(v, L"SteamGameId=", 12) == 0 ||
                                  _wcsnicmp(v, L"SteamOverlayGameId=", 19) == 0)) ||
                _wcsnicmp(v, L"_SteamInjection", 15) == 0 ||
                _wcsnicmp(v, L"ENABLE_STEAM_OVERLAY", 20) == 0) continue;
            env.append(v);
            env.push_back(L'\0');
        }
        FreeEnvironmentStringsW(block);
    }
    env.push_back(L'\0');
    return env;
}

// Preserve Proton's Steam connection environment; the helper sets its own app identity.
HANDLE StartHost(uint16_t port, const std::string& token, uint32_t overrideAppId = 0, bool maskMode = false) {
    const std::wstring dir = ModuleDir();
    std::wstring exe = dir + kHostExe;
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) exe = dir + L"ReFix\\" + kHostExe;
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!maskMode) SetError("refix_steamhost is missing next to the game's steam_api: deploy ReFix again");
        return nullptr;
    }
    std::wstring env = BuildCleanHostEnv();
    const std::wstring dllDir = dir.substr(0, dir.size() - 1);
    const uint32_t bridgeApp = overrideAppId ? overrideAppId : (g_bridgeAppId.load() ? g_bridgeAppId.load() : 480);
    std::wstring cmd = L"\"" + exe + L"\" " + std::to_wstring(port) + L" " + std::wstring(token.begin(), token.end()) + L" " +
                       std::to_wstring(GetCurrentProcessId()) + L" \"" + dllDir + L"\" " + std::to_wstring(bridgeApp);
    if (maskMode) cmd += L" --mask";
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                        env.data(), dllDir.c_str(), &si, &pi)) {
        if (!maskMode) SetError("Could not start refix_steamhost");
        return nullptr;
    }
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

std::string NewToken() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    char buf[40];
    snprintf(buf, sizeof(buf), "%08lx%016llx", GetCurrentProcessId() ^ GetTickCount(), (unsigned long long)t.QuadPart);
    return buf;
}

void OnHostState(proto::Reader& r) {
    const uint8_t step = r.Get<uint8_t>();
    const uint64_t self = r.Get<uint64_t>();
    const std::string error = r.Str();
    if (!r.ok) return;
    const uint32_t actualBridge = r.Left() >= 4 ? r.Get<uint32_t>() : 0;
    if (actualBridge != 0 && actualBridge != g_bridgeAppId.load()) {
        const uint32_t prev = g_bridgeAppId.exchange(actualBridge);
        ReFixLog("[SteamLink] Bridge AppID updated by host: %u -> %u", prev, actualBridge);
        if (actualBridge == 480 && refix::overlay::settings::IsOfficialServersActive()) {
            refix::overlay::settings::SetOfficialServersActive(false);
            ReFixLog("[SteamLink] Disabled Official Servers because Steam fell back to Spacewar (480)");
        }
    }
    if (step == proto::kFailed) {
        SetError(error);
        g_state = State::Failed;
        return;
    }
    if (step != proto::kOnline) {
        static const char* kSteps[] = { "", "opening Steam", "waiting for Steam to sign in", "connecting" };
        if (g_step.exchange(step) != step && step < 4) ReFixLog("[SteamLink] Steam: %s", kSteps[step]);
        return;
    }
    g_selfId = self;
    if (g_state == State::Online) return;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_error.clear();
    }
    g_state = State::Online;
    PublishPresence();
    if (g_steamOverlay && !GetModuleHandleW(sizeof(void*) == 8 ? L"GameOverlayRenderer64.dll" : L"GameOverlayRenderer.dll"))
        LoadSteamOverlay();
    ReFixLog("[SteamLink] Online as %llu (bridge %u), app %u",
             (unsigned long long)self, g_bridgeAppId.load() ? g_bridgeAppId.load() : 480, g_appId.load());
}

// One datagram from the helper; before its hello (with our token) nothing else is taken.
void HandleHost(const uint8_t* buf, size_t n, const std::string& token, const sockaddr_in& from) {
    if (!n) return;
    proto::Reader r(buf + 1, n - 1);
    if (buf[0] == proto::kHello) {
        if (!g_hostKnown && n - 1 == token.size() && memcmp(buf + 1, token.data(), token.size()) == 0) {
            g_host = from;
            g_hostKnown = true;
        }
        return;
    }
    if (!g_hostKnown || from.sin_port != g_host.sin_port || from.sin_addr.s_addr != g_host.sin_addr.s_addr) return;
    switch (buf[0]) {
    case proto::kState: OnHostState(r); break;
    case proto::kFriends: ReadFriends(r); break;
    case proto::kLog: {
        const std::string line = r.Str();
        if (r.ok) ReFixLog("[SteamHost] %s", line.c_str());
        break;
    }
    case proto::kMessage: {
        const uint64_t sender = r.Get<uint64_t>();
        if (r.ok) Deliver(sender, r.p, r.Left());
        break;
    }
    case proto::kAvatarData: {
        Avatar a;
        const int image = r.Get<int32_t>();
        a.w = r.Get<uint16_t>();
        a.h = r.Get<uint16_t>();
        if (!r.ok || r.Left() != (size_t)a.w * a.h * 4) return;
        a.rgba.assign(r.p, r.p + r.Left());
        std::lock_guard<std::mutex> lock(g_mu);
        g_avatars[image] = std::move(a);
        break;
    }
    case proto::kAccountChanged: {
        const uint64_t newSteamId = r.Get<uint64_t>();
        if (r.ok) {
            ReFixLog("[SteamLink] Active Steam account changed (new=%llu); stopping current session",
                     (unsigned long long)newSteamId);
            g_wanted = false;
            if (auto cb = g_accountChangedCb.load()) {
                std::thread([cb, newSteamId] { cb(newSteamId); }).detach();
            }
        }
        break;
    }
    case proto::kAppLicense: {
        const uint32_t appId = r.Get<uint32_t>();
        const bool owned = r.Get<uint8_t>() != 0;
        if (!r.ok || appId == 0) break;
        {
            std::lock_guard<std::mutex> lock(g_appLicMu);
            g_appLicMap[appId] = owned;
        }
        g_appLicCv.notify_all();
        break;
    }
    case proto::kAuthTicketData: {
        const uint32_t reqId = r.Get<uint32_t>();
        const uint32_t hostHandle = r.Get<uint32_t>();
        const int32_t eResult = r.Get<int32_t>();
        const uint32_t len = r.Get<uint32_t>();
        if (!r.ok || r.Left() < len) break;
        std::vector<uint8_t> bytes;
        if (len) bytes.assign(r.p, r.p + len);
        {
            std::lock_guard<std::mutex> lock(g_authMu);
            auto it = g_authWaiters.find(reqId);
            if (it != g_authWaiters.end()) {
                it->second.eResult = eResult;
                it->second.hostHandle = hostHandle;
                it->second.ticket = std::move(bytes);
                it->second.done = true;
            }
        }
        g_authCv.notify_all();
        break;
    }
    case proto::kEncryptedAppTicketData: {
        const uint32_t reqId = r.Get<uint32_t>();
        const int32_t eResult = r.Get<int32_t>();
        const uint32_t len = r.Get<uint32_t>();
        if (!r.ok || r.Left() < len) break;
        std::vector<uint8_t> bytes;
        if (len) bytes.assign(r.p, r.p + len);
        {
            std::lock_guard<std::mutex> lock(g_authMu);
            auto it = g_authWaiters.find(reqId);
            if (it != g_authWaiters.end()) {
                it->second.eResult = eResult;
                it->second.ticket = std::move(bytes);
                it->second.done = true;
            }
        }
        g_authCv.notify_all();
        break;
    }
    case proto::kLobbySnapshot: {
        const uint32_t reqId = r.Get<uint32_t>();
        OfficialLobbySnapshot snap;
        snap.lobbyId = r.Get<uint64_t>();
        snap.owner = r.Get<uint64_t>();
        snap.maxMembers = r.Get<int32_t>();
        snap.hasGameServer = r.Get<uint8_t>() != 0;
        snap.gsIp = r.Get<uint32_t>();
        snap.gsPort = r.Get<uint16_t>();
        snap.gsSteamId = r.Get<uint64_t>();
        const uint16_t nm = r.Get<uint16_t>();
        for (uint16_t i = 0; i < nm && r.ok; ++i) snap.members.push_back(r.Get<uint64_t>());
        const uint16_t nk = r.Get<uint16_t>();
        for (uint16_t i = 0; i < nk && r.ok; ++i) {
            std::string k = r.Str16();
            std::string v = r.Str16();
            snap.data.emplace_back(std::move(k), std::move(v));
        }
        if (!r.ok || !snap.lobbyId) break;
        if (auto cb = g_onLobbySnap.load()) cb(snap);
        if (reqId != 0) {
            std::lock_guard<std::mutex> lock(g_lobbyMu);
            auto it = g_lobbyWaiters.find(reqId);
            if (it != g_lobbyWaiters.end()) it->second.snaps[snap.lobbyId] = std::move(snap);
        }
        break;
    }
    case proto::kLobbySearchDone: {
        const uint32_t reqId = r.Get<uint32_t>();
        const bool failed = r.Get<uint8_t>() != 0;
        const uint16_t count = r.Get<uint16_t>();
        std::vector<uint64_t> ids;
        for (uint16_t i = 0; i < count && r.ok; ++i) ids.push_back(r.Get<uint64_t>());
        if (!r.ok) break;
        {
            std::lock_guard<std::mutex> lock(g_lobbyMu);
            auto it = g_lobbyWaiters.find(reqId);
            if (it != g_lobbyWaiters.end()) {
                it->second.failed = failed;
                it->second.order = std::move(ids);
                it->second.done = true;
            }
        }
        g_lobbyCv.notify_all();
        break;
    }
    case proto::kLobbyCreatedRes: {
        const uint32_t reqId = r.Get<uint32_t>();
        const int32_t eResult = r.Get<int32_t>();
        const uint64_t lobbyId = r.Get<uint64_t>();
        if (!r.ok) break;
        {
            std::lock_guard<std::mutex> lock(g_lobbyMu);
            auto it = g_lobbyWaiters.find(reqId);
            if (it != g_lobbyWaiters.end()) {
                it->second.eResult = eResult;
                it->second.lobbyId = lobbyId;
                it->second.done = true;
            }
        }
        g_lobbyCv.notify_all();
        break;
    }
    case proto::kLobbyEnterRes: {
        const uint32_t reqId = r.Get<uint32_t>();
        const uint64_t lobbyId = r.Get<uint64_t>();
        const uint32_t chatPerms = r.Get<uint32_t>();
        const bool locked = r.Get<uint8_t>() != 0;
        const uint32_t enterResp = r.Get<uint32_t>();
        if (!r.ok) break;
        {
            std::lock_guard<std::mutex> lock(g_lobbyMu);
            auto it = g_lobbyWaiters.find(reqId);
            if (it != g_lobbyWaiters.end()) {
                it->second.lobbyId = lobbyId;
                it->second.chatPerms = chatPerms;
                it->second.locked = locked;
                it->second.enterResp = enterResp;
                it->second.done = true;
            }
        }
        g_lobbyCv.notify_all();
        break;
    }
    case proto::kLobbyDataChanged: {
        const uint64_t lobbyId = r.Get<uint64_t>();
        const uint64_t memberId = r.Get<uint64_t>();
        const bool success = r.Get<uint8_t>() != 0;
        if (r.ok) {
            if (auto cb = g_onLobbyData.load()) cb(lobbyId, memberId, success);
        }
        break;
    }
    case proto::kLobbyChatChanged: {
        const uint64_t lobbyId = r.Get<uint64_t>();
        const uint64_t userChanged = r.Get<uint64_t>();
        const uint64_t makingChange = r.Get<uint64_t>();
        const uint32_t stateChange = r.Get<uint32_t>();
        if (r.ok) {
            if (auto cb = g_onLobbyChat.load()) cb(lobbyId, userChanged, makingChange, stateChange);
        }
        break;
    }
    default: break;
    }
}

bool OpenIpcSock(SOCKET& sock, uint16_t& port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in self{};
    self.sin_family = AF_INET;
    self.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int selfLen = sizeof(self), big = 1 << 20;
    if (sock == INVALID_SOCKET) return false;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&big, sizeof(big));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&big, sizeof(big));
    if (bind(sock, (const sockaddr*)&self, sizeof(self)) != 0 || getsockname(sock, (sockaddr*)&self, &selfLen) != 0) {
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }
    if (auto cb = g_sockCb.load()) cb((uintptr_t)sock, true);   // ReFix's own socket: Winsock hooks skip it
    u_long nonblock = 1;
    ioctlsocket(sock, FIONBIO, &nonblock);
    port = ntohs(self.sin_port);
    return true;
}

void HandleMaskHost(const uint8_t* buf, size_t n, const std::string& token, const sockaddr_in& from, uint32_t maskApp, bool& maskFailed) {
    if (!n) return;
    proto::Reader r(buf + 1, n - 1);
    if (buf[0] == proto::kHello) {
        if (!g_maskHostKnown && n - 1 == token.size() && memcmp(buf + 1, token.data(), token.size()) == 0) {
            g_maskHost = from;
            g_maskHostKnown = true;
        }
        return;
    }
    if (!g_maskHostKnown || from.sin_port != g_maskHost.sin_port || from.sin_addr.s_addr != g_maskHost.sin_addr.s_addr) return;
    switch (buf[0]) {
    case proto::kState: {
        const uint8_t step = r.Get<uint8_t>();
        (void)r.Get<uint64_t>();
        const std::string err = r.Str();
        if (!r.ok) break;
        if (step == proto::kOnline) {
            if (!g_maskOnline.exchange(true)) {
                ReFixLog("[SteamLink] Mask active for AppID %u alongside Spacewar (480)", maskApp);
            }
        } else if (step == proto::kFailed) {
            g_maskOnline = false;
            maskFailed = true;
            ReFixLog("[SteamLink] Mask helper (AppID %u) failed: %s", maskApp, err.c_str());
        }
        break;
    }
    case proto::kLog: {
        const std::string line = r.Str();
        if (r.ok) ReFixLog("[SteamMask] %s", line.c_str());
        break;
    }
    default: break;
    }
}

void StopMaskHelper(HANDLE& maskProc) {
    g_maskOnline = false;
    if (maskProc) {
        if (g_maskHostKnown && g_maskIpc != INVALID_SOCKET) {
            const uint8_t q = proto::kQuit;
            sendto(g_maskIpc, (const char*)&q, 1, 0, (const sockaddr*)&g_maskHost, sizeof(g_maskHost));
        }
        if (WaitForSingleObject(maskProc, 2000) != WAIT_OBJECT_0) TerminateProcess(maskProc, 0);
        CloseHandle(maskProc);
        maskProc = nullptr;
    }
    if (g_maskIpc != INVALID_SOCKET) {
        if (auto cb = g_sockCb.load()) cb((uintptr_t)g_maskIpc, false);
        closesocket(g_maskIpc);
        g_maskIpc = INVALID_SOCKET;
    }
    g_maskHostKnown = false;
}

void FlushStatQueue(uint32_t runningMaskApp) {
    const bool useMask = (runningMaskApp != 0 && g_maskOnline.load() && g_maskHostKnown && g_maskIpc != INVALID_SOCKET);
    const bool usePrimary = (!useMask && g_state == State::Online && g_hostKnown && g_ipc != INVALID_SOCKET &&
                             g_bridgeAppId.load() != 480);
    if (!useMask && !usePrimary) return;

    std::vector<std::vector<uint8_t>> batch;
    {
        std::lock_guard<std::mutex> lock(g_statMu);
        if (g_statQueue.empty()) return;
        batch.swap(g_statQueue);
    }
    for (const auto& pkt : batch) {
        if (pkt.empty() || pkt.size() > proto::kMaxDatagram) continue;
        if (useMask) {
            sendto(g_maskIpc, (const char*)pkt.data(), (int)pkt.size(), 0, (const sockaddr*)&g_maskHost, sizeof(g_maskHost));
        } else {
            sendto(g_ipc, (const char*)pkt.data(), (int)pkt.size(), 0, (const sockaddr*)&g_host, sizeof(g_host));
        }
    }
}

void LinkThread() {
    uint16_t port = 0;
    if (!OpenIpcSock(g_ipc, port)) {
        SetError("Could not open the Steam helper socket");
        g_state = State::Failed;
        return;
    }
    const std::string token = NewToken();
    HANDLE host = StartHost(port, token);
    const ULONGLONG started = GetTickCount64();
    HANDLE maskProc = nullptr;
    uint32_t runningMaskApp = 0;
    std::string maskToken;
    bool maskFailed = false;
    uint32_t failedMaskApp = 0;
    static std::vector<uint8_t> buf(proto::kMaxDatagram + 16);
    while (host && g_wanted) {
        const uint32_t bridgeNow = g_bridgeAppId.load() ? g_bridgeAppId.load() : 480;
        const uint32_t rawMask = g_maskAppId.load();
        const uint32_t desiredMask = (g_state == State::Online && bridgeNow == 480 && rawMask != 0 && rawMask != 480) ? rawMask : 0;
        if (desiredMask != failedMaskApp) failedMaskApp = 0;
        const uint32_t effectiveMask = (desiredMask == failedMaskApp) ? 0 : desiredMask;

        if (effectiveMask != runningMaskApp) {
            StopMaskHelper(maskProc);
            runningMaskApp = 0;
            maskFailed = false;
            if (effectiveMask != 0) {
                uint16_t maskPort = 0;
                if (OpenIpcSock(g_maskIpc, maskPort)) {
                    maskToken = NewToken();
                    maskProc = StartHost(maskPort, maskToken, effectiveMask, true);
                    if (maskProc) {
                        runningMaskApp = effectiveMask;
                    } else {
                        StopMaskHelper(maskProc);
                        failedMaskApp = effectiveMask;
                    }
                }
            }
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(g_ipc, &fds);
        if (g_maskIpc != INVALID_SOCKET) FD_SET(g_maskIpc, &fds);
        timeval tv{ 0, (long)kPumpIntervalMs * 1000 };
        if (select(0, &fds, nullptr, nullptr, &tv) > 0) {
            for (;;) {
                sockaddr_in from{};
                int fromLen = sizeof(from);
                const int n = recvfrom(g_ipc, (char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fromLen);
                if (n <= 0) break;
                HandleHost(buf.data(), (size_t)n, token, from);
            }
            if (g_maskIpc != INVALID_SOCKET) {
                for (;;) {
                    sockaddr_in from{};
                    int fromLen = sizeof(from);
                    const int n = recvfrom(g_maskIpc, (char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fromLen);
                    if (n <= 0) break;
                    HandleMaskHost(buf.data(), (size_t)n, maskToken, from, runningMaskApp, maskFailed);
                }
            }
        }
        if (maskProc && (maskFailed || WaitForSingleObject(maskProc, 0) != WAIT_TIMEOUT)) {
            failedMaskApp = runningMaskApp;
            StopMaskHelper(maskProc);
            runningMaskApp = 0;
        }
        FlushStatQueue(runningMaskApp);
        PumpBridges();
        if (g_state == State::Failed) break;
        if (WaitForSingleObject(host, 0) != WAIT_TIMEOUT) {
            SetError("The Steam helper stopped");
            g_state = State::Failed;
            break;
        }
        if (!g_hostKnown && GetTickCount64() - started > kHelloTimeoutMs) {
            SetError("The Steam helper did not answer");
            g_state = State::Failed;
            break;
        }
    }
    StopMaskHelper(maskProc);
    {
        std::lock_guard<std::mutex> lock(g_statMu);
        g_statQueue.clear();
    }
    // Off or failed: the helper signs out of Steam and exits, so Steam stops showing Spacewar.
    if (host) {
        ToHost(proto::Writer(proto::kQuit));
        if (WaitForSingleObject(host, kQuitWaitMs) != WAIT_OBJECT_0) TerminateProcess(host, 0);
        CloseHandle(host);
    }
    CloseBridges();
    if (auto cb = g_sockCb.load()) cb((uintptr_t)g_ipc, false);
    closesocket(g_ipc);
    g_ipc = INVALID_SOCKET;
    g_hostKnown = false;
    g_step = 0;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_friendList.clear();
        g_avatars.clear();
        g_avatarAsked.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_authMu);
        for (auto& [id, w] : g_authWaiters) w.done = true;
    }
    g_authCv.notify_all();
    if (!host) g_state = State::Failed;
    if (!g_wanted) g_state = State::Off;
    else if (g_state != State::Failed) g_state = State::Failed;
    ReFixLog("[SteamLink] %s", g_state == State::Off ? "Off" : "Stopped (failed)");
}

bool SendRaw(uint64_t steamId, uint16_t srcPort, uint16_t dstPort, uint8_t flags, const void* data, size_t len,
             uint32_t lanIp) {
    const size_t head = kHeader + ((flags & (kFlagToLan | kFlagFromLan)) ? 4 : 0);
    if (g_state != State::Online || len + head > kMaxDatagram) return false;
    proto::Writer w(proto::kSend);
    w.Put<uint64_t>(steamId).Put<uint16_t>(srcPort).Put<uint16_t>(dstPort).Put<uint8_t>(flags);
    if (head > kHeader) w.Raw(&lanIp, 4);
    if (len) w.Raw(data, len);
    if (w.b.size() > proto::kMaxDatagram) return false;
    ToHost(w);
    return true;
}

} // namespace

bool SteamInstalled() {
    if (ReFixIsWine() && ReFixHasProtonSteamEnv()) return true;
    const std::wstring exe = RegString(L"Software\\Valve\\Steam", L"SteamExe");
    return !exe.empty() && GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void SetLocalGame(uint32_t appId) {
    g_appId = appId;
    if (g_state == State::Online) PublishPresence();
}

void SetBridgeAppId(uint32_t appId) {
    g_bridgeAppId = appId ? appId : 480;
}

uint32_t BridgeAppId() {
    const uint32_t b = g_bridgeAppId.load();
    return b ? b : 480;
}

void SetMaskAppId(uint32_t appId) {
    g_maskAppId = (appId != 0 && appId != 480) ? appId : 0;
}

uint32_t MaskAppId() {
    return g_maskAppId.load();
}

bool MaskOnline() {
    return g_maskOnline.load();
}

uint32_t ActiveRegistryAccountId() {
    DWORD v = 0, size = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", L"ActiveUser",
                 RRF_RT_REG_DWORD, nullptr, &v, &size);
    return (uint32_t)v;
}

void SetAccountChangedCallback(AccountChangedCb cb) {
    g_accountChangedCb = cb;
}

bool ProbeLicensesSync(uint32_t currentGameAppId, uint32_t customAppId, LicenseProbe& out) {
    out = LicenseProbe{};
    out.currentGameAppId = currentGameAppId;
    out.customAppId = customAppId;

    const std::wstring dir = ModuleDir();
    std::wstring exe = dir + kHostExe;
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) exe = dir + L"ReFix\\" + kHostExe;
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        out.done = true;
        return false;
    }

    std::wstring csv = L"480";
    if (currentGameAppId != 0 && currentGameAppId != 480) csv += L"," + std::to_wstring(currentGameAppId);
    if (customAppId != 0 && customAppId != 480 && customAppId != currentGameAppId) csv += L"," + std::to_wstring(customAppId);

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        out.done = true;
        return false;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    std::wstring env = BuildCleanHostEnv();
    std::wstring cmd = L"\"" + exe + L"\" --probe " + csv;
    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi{};
    const std::wstring dllDir = dir.substr(0, dir.size() - 1);
    const BOOL ok = CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                                   env.data(), dllDir.c_str(), &si, &pi);
    CloseHandle(hWrite);
    if (!ok) {
        CloseHandle(hRead);
        out.done = true;
        return false;
    }

    std::string line;
    char buf[256];
    DWORD nr = 0;
    while (ReadFile(hRead, buf, sizeof(buf), &nr, nullptr) && nr > 0) {
        line.append(buf, nr);
    }
    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(hRead);

    out.done = true;
    std::string firstLine, secondLine;
    {
        const size_t nl = line.find('\n');
        firstLine = (nl == std::string::npos) ? line : line.substr(0, nl);
        if (nl != std::string::npos) secondLine = line.substr(nl + 1);
    }
    auto trimCrLf = [](std::string& s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    };
    trimCrLf(firstLine);
    trimCrLf(secondLine);
    std::istringstream iss(firstLine);
    std::string status;
    iss >> status;
    if (status == "OK") {
        uint64_t sid = 0;
        iss >> sid;
        out.steamSignedIn = (sid != 0);
        out.steamId = sid;
        std::string tok;
        while (iss >> tok) {
            const size_t colon = tok.find(':');
            if (colon == std::string::npos) continue;
            const uint32_t id = (uint32_t)strtoul(tok.substr(0, colon).c_str(), nullptr, 10);
            const bool owned = (tok.substr(colon + 1) == "1");
            if (id != 0 && id == currentGameAppId) out.currentGameOwned = owned;
            if (id != 0 && id == customAppId) out.customAppOwned = owned;
        }
        if (secondLine.rfind("NAME ", 0) == 0) {
            out.steamName = secondLine.substr(5);
        }
    }
    if (!out.steamSignedIn && g_state.load() == State::Online && g_selfId.load() != 0) {
        out.steamSignedIn = true;
        out.steamId = g_selfId.load();
    }
    if (g_state.load() == State::Online) {
        std::vector<uint32_t> ask;
        if (currentGameAppId != 0 && currentGameAppId != 480) ask.push_back(currentGameAppId);
        if (customAppId != 0 && customAppId != 480 && customAppId != currentGameAppId) ask.push_back(customAppId);
        if (!ask.empty()) {
            {
                std::lock_guard<std::mutex> lock(g_appLicMu);
                for (uint32_t id : ask) g_appLicMap.erase(id);
            }
            for (uint32_t id : ask) {
                ToHost(proto::Writer(proto::kCheckApp).Put<uint32_t>(id));
            }
            std::unique_lock<std::mutex> lock(g_appLicMu);
            g_appLicCv.wait_for(lock, std::chrono::milliseconds(1500), [&] {
                for (uint32_t id : ask) {
                    if (g_appLicMap.find(id) == g_appLicMap.end()) return false;
                }
                return true;
            });
            if (currentGameAppId != 0 && currentGameAppId != 480) {
                auto it = g_appLicMap.find(currentGameAppId);
                if (it != g_appLicMap.end()) out.currentGameOwned = it->second;
            }
            if (customAppId != 0 && customAppId != 480) {
                auto it = g_appLicMap.find(customAppId);
                if (it != g_appLicMap.end()) out.customAppOwned = it->second;
            }
        }
    }
    return out.steamSignedIn;
}

void ProbeLicenses(uint32_t currentGameAppId, uint32_t customAppId) {
    {
        std::lock_guard<std::mutex> lock(g_probeMu);
        g_probe.probing = true;
        g_probe.currentGameAppId = currentGameAppId;
        g_probe.customAppId = customAppId;
    }
    std::thread([currentGameAppId, customAppId] {
        LicenseProbe res;
        ProbeLicensesSync(currentGameAppId, customAppId, res);
        res.probing = false;
        std::lock_guard<std::mutex> lock(g_probeMu);
        g_probe = res;
    }).detach();
}

LicenseProbe GetLicenseProbe() {
    std::lock_guard<std::mutex> lock(g_probeMu);
    return g_probe;
}

void SetLocalGameName(const std::string& utf8Name) {
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_gameName == utf8Name) return;
        g_gameName = utf8Name;
    }
    if (g_state == State::Online) PublishPresence();
}

bool AvatarRgba(int image, int& width, int& height, std::vector<uint8_t>& rgba) {
    if (g_state != State::Online || image <= 0) return false;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto it = g_avatars.find(image);
        if (it != g_avatars.end()) {
            width = it->second.w;
            height = it->second.h;
            rgba = it->second.rgba;
            return true;
        }
        ULONGLONG& asked = g_avatarAsked[image];
        const ULONGLONG now = GetTickCount64();
        if (asked && now - asked < kAvatarRetryMs) return false;   // Steam may still be downloading it
        asked = now;
    }
    ToHost(proto::Writer(proto::kAvatar).Put<int32_t>(image));
    return false;
}

Step CurrentStep() {
    switch (g_step.load()) {
    case proto::kOpeningSteam: return Step::OpeningSteam;
    case proto::kSigningIn: return Step::SigningIn;
    case proto::kConnecting: return Step::Connecting;
    default: return Step::None;
    }
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

void ForwardFromLan(uint32_t ip, uint16_t port, const void* wire, size_t len, uint16_t dstPort) {
    if (g_state != State::Online || IsVirtual(ip) || !wire || !len) return;
    // Deduplicate identical discovery/lobby datagrams received simultaneously across multiple ports/interfaces.
    uint64_t h = 14695981039346656037ULL ^ ((uint64_t)dstPort << 16) ^ (uint64_t)len;
    const auto* bytes = static_cast<const uint8_t*>(wire);
    for (size_t i = 0; i < len; ++i) {
        h ^= bytes[i];
        h *= 1099511628211ULL;
    }
    const ULONGLONG now = GetTickCount64();
    std::vector<uint64_t> peers;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        struct RecentFwd { uint64_t hash = 0; ULONGLONG at = 0; };
        static RecentFwd s_recent[16];
        static size_t s_cursor = 0;
        for (const RecentFwd& rf : s_recent) {
            if (rf.hash == h && now >= rf.at && (now - rf.at) < 40) return;
        }
        s_recent[s_cursor++ & 15] = RecentFwd{ h, now };
        for (const Friend& f : g_friendList) if (f.refix) peers.push_back(f.steamId);
    }
    for (uint64_t id : peers) SendRaw(id, port, dstPort, kFlagBroadcast | kFlagFromLan, wire, len, ip);
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

uint32_t VirtualIp(uint64_t steamId) {
    if (!steamId) return 0;
    std::lock_guard<std::mutex> lock(g_mu);
    return VirtualIpLocked(steamId);
}

uint64_t SteamIdOf(uint32_t virtIp) {
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_idOf.find(virtIp);
    if (it != g_idOf.end()) return it->second;
    auto r = g_relayOf.find(virtIp);
    if (r != g_relayOf.end()) return r->second.gateway;
    return 0;
}

bool RequestRealAuthTicket(bool webApi, const char* identity, std::vector<uint8_t>& outTicket, uint32_t& outHostHandle) {
    outTicket.clear();
    outHostHandle = 0;
    // If steamlink is still starting right at boot, wait briefly for Online state.
    const ULONGLONG startWait = GetTickCount64();
    while (g_state == State::Starting && GetTickCount64() - startWait < 3500) {
        Sleep(20);
    }
    if (g_state != State::Online || !g_hostKnown) return false;
    const uint32_t reqId = g_nextAuthReqId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_authMu);
        g_authWaiters[reqId] = AuthWaiter{};
    }
    ToHost(proto::Writer(proto::kRequestAuthTicket)
               .Put<uint32_t>(reqId)
               .Put<uint8_t>(webApi ? 1 : 0)
               .Str(identity ? identity : ""));
    bool ok = false;
    {
        std::unique_lock<std::mutex> lock(g_authMu);
        g_authCv.wait_for(lock, std::chrono::milliseconds(3000), [&] {
            auto it = g_authWaiters.find(reqId);
            return it == g_authWaiters.end() || it->second.done || g_state != State::Online;
        });
        auto it = g_authWaiters.find(reqId);
        if (it != g_authWaiters.end()) {
            if (it->second.done && it->second.eResult == 1 && !it->second.ticket.empty()) {
                outTicket = std::move(it->second.ticket);
                outHostHandle = it->second.hostHandle;
                ok = true;
            }
            g_authWaiters.erase(it);
        }
    }
    return ok;
}

void CancelRealAuthTicket(uint32_t hostHandle) {
    if (hostHandle && g_state == State::Online && g_hostKnown) {
        ToHost(proto::Writer(proto::kCancelAuthTicket).Put<uint32_t>(hostHandle));
    }
}

bool RequestRealEncryptedAppTicket(const void* dataToInclude, int cbDataToInclude,
                                   std::vector<uint8_t>& outTicket, int32_t& outResult) {
    outTicket.clear();
    outResult = 2;
    const ULONGLONG startWait = GetTickCount64();
    while (g_state == State::Starting && GetTickCount64() - startWait < 3500) {
        Sleep(20);
    }
    if (g_state != State::Online || !g_hostKnown) return false;
    const uint32_t len = (dataToInclude && cbDataToInclude > 0) ? (uint32_t)(std::min)(cbDataToInclude, 4096) : 0;
    const uint32_t reqId = g_nextAuthReqId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_authMu);
        g_authWaiters[reqId] = AuthWaiter{};
    }
    proto::Writer w(proto::kRequestEncryptedAppTicket);
    w.Put<uint32_t>(reqId).Put<uint32_t>(len);
    if (len > 0) w.Raw(dataToInclude, len);
    ToHost(w);
    bool ok = false;
    {
        std::unique_lock<std::mutex> lock(g_authMu);
        g_authCv.wait_for(lock, std::chrono::milliseconds(4000), [&] {
            auto it = g_authWaiters.find(reqId);
            return it == g_authWaiters.end() || it->second.done || g_state != State::Online;
        });
        auto it = g_authWaiters.find(reqId);
        if (it != g_authWaiters.end()) {
            if (it->second.done) {
                outResult = it->second.eResult;
                if (outResult == 1 && !it->second.ticket.empty()) {
                    outTicket = std::move(it->second.ticket);
                    ok = true;
                }
            }
            g_authWaiters.erase(it);
        }
    }
    return ok;
}

void SetOfficialLobbyCallbacks(LobbyUpdateCb onSnap, LobbyDataChangedCb onData, LobbyChatChangedCb onChat) {
    g_onLobbySnap.store(onSnap);
    g_onLobbyData.store(onData);
    g_onLobbyChat.store(onChat);
}

static bool WaitSteamLinkOnline() {
    const ULONGLONG start = GetTickCount64();
    while (g_state == State::Starting && GetTickCount64() - start < 4000) {
        Sleep(20);
    }
    return g_state == State::Online && g_hostKnown;
}

bool OfficialSearchLobbies(const OfficialLobbySearchQuery& q, std::vector<OfficialLobbySnapshot>& outLobbies) {
    outLobbies.clear();
    if (!WaitSteamLinkOnline()) return false;
    const uint32_t reqId = g_nextAuthReqId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_lobbyMu);
        g_lobbyWaiters[reqId] = LobbyWaiter{};
    }
    proto::Writer w(proto::kLobbySearch);
    w.Put<uint32_t>(reqId).Put<uint32_t>(q.slotsAvailable).Put<int32_t>(q.distanceFilter)
     .Put<uint32_t>(q.maxResults).Put<uint64_t>(q.compatibleLobby);
    w.Put<uint16_t>((uint16_t)q.stringFilters.size());
    for (const auto& f : q.stringFilters) w.Str16(f.key).Str16(f.val).Put<int32_t>(f.cmp);
    w.Put<uint16_t>((uint16_t)q.numFilters.size());
    for (const auto& f : q.numFilters) w.Str16(f.key).Put<int32_t>(f.val).Put<int32_t>(f.cmp);
    w.Put<uint16_t>((uint16_t)q.nearFilters.size());
    for (const auto& f : q.nearFilters) w.Str16(f.key).Put<int32_t>(f.val);
    ToHost(w);

    bool ok = false;
    std::unique_lock<std::mutex> lock(g_lobbyMu);
    g_lobbyCv.wait_for(lock, std::chrono::milliseconds(8000), [&] {
        auto it = g_lobbyWaiters.find(reqId);
        return it == g_lobbyWaiters.end() || it->second.done || g_state != State::Online;
    });
    auto it = g_lobbyWaiters.find(reqId);
    if (it != g_lobbyWaiters.end()) {
        if (it->second.done && !it->second.failed) {
            ok = true;
            for (uint64_t id : it->second.order) {
                auto sit = it->second.snaps.find(id);
                if (sit != it->second.snaps.end()) {
                    outLobbies.push_back(std::move(sit->second));
                } else {
                    OfficialLobbySnapshot blank;
                    blank.lobbyId = id;
                    outLobbies.push_back(std::move(blank));
                }
            }
        }
        g_lobbyWaiters.erase(it);
    }
    return ok;
}

bool OfficialCreateLobby(int eLobbyType, int cMaxMembers, int32_t& outResult, uint64_t& outLobbyId, OfficialLobbySnapshot& outSnap) {
    outResult = 2;
    outLobbyId = 0;
    outSnap = OfficialLobbySnapshot{};
    if (!WaitSteamLinkOnline()) return false;
    const uint32_t reqId = g_nextAuthReqId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_lobbyMu);
        g_lobbyWaiters[reqId] = LobbyWaiter{};
    }
    ToHost(proto::Writer(proto::kLobbyCreate).Put<uint32_t>(reqId).Put<int32_t>(eLobbyType).Put<int32_t>(cMaxMembers));
    bool ok = false;
    std::unique_lock<std::mutex> lock(g_lobbyMu);
    g_lobbyCv.wait_for(lock, std::chrono::milliseconds(8000), [&] {
        auto it = g_lobbyWaiters.find(reqId);
        return it == g_lobbyWaiters.end() || it->second.done || g_state != State::Online;
    });
    auto it = g_lobbyWaiters.find(reqId);
    if (it != g_lobbyWaiters.end()) {
        if (it->second.done) {
            outResult = it->second.eResult;
            outLobbyId = it->second.lobbyId;
            auto sit = it->second.snaps.find(outLobbyId);
            if (sit != it->second.snaps.end()) outSnap = std::move(sit->second);
            ok = (outResult == 1 && outLobbyId != 0);
        }
        g_lobbyWaiters.erase(it);
    }
    return ok;
}

bool OfficialJoinLobby(uint64_t lobbyId, uint32_t& outChatPerms, bool& outLocked, uint32_t& outEnterResp, OfficialLobbySnapshot& outSnap) {
    outChatPerms = 0;
    outLocked = false;
    outEnterResp = 2;
    outSnap = OfficialLobbySnapshot{};
    if (!lobbyId || !WaitSteamLinkOnline()) return false;
    const uint32_t reqId = g_nextAuthReqId.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_lobbyMu);
        g_lobbyWaiters[reqId] = LobbyWaiter{};
    }
    ToHost(proto::Writer(proto::kLobbyJoin).Put<uint32_t>(reqId).Put<uint64_t>(lobbyId));
    bool ok = false;
    std::unique_lock<std::mutex> lock(g_lobbyMu);
    g_lobbyCv.wait_for(lock, std::chrono::milliseconds(8000), [&] {
        auto it = g_lobbyWaiters.find(reqId);
        return it == g_lobbyWaiters.end() || it->second.done || g_state != State::Online;
    });
    auto it = g_lobbyWaiters.find(reqId);
    if (it != g_lobbyWaiters.end()) {
        if (it->second.done) {
            outChatPerms = it->second.chatPerms;
            outLocked = it->second.locked;
            outEnterResp = it->second.enterResp;
            auto sit = it->second.snaps.find(lobbyId);
            if (sit != it->second.snaps.end()) outSnap = std::move(sit->second);
            ok = true;
        }
        g_lobbyWaiters.erase(it);
    }
    return ok;
}

void OfficialLeaveLobby(uint64_t lobbyId) {
    if (lobbyId && g_state == State::Online && g_hostKnown)
        ToHost(proto::Writer(proto::kLobbyLeave).Put<uint64_t>(lobbyId));
}

void OfficialSetLobbyData(uint64_t lobbyId, const char* key, const char* val) {
    if (lobbyId && key && g_state == State::Online && g_hostKnown) {
        ToHost(proto::Writer(proto::kLobbySetData).Put<uint64_t>(lobbyId).Str16(key)
                   .Put<uint8_t>(val ? 0 : 1).Str16(val ? val : ""));
    }
}

void OfficialSetLobbyType(uint64_t lobbyId, int eType) {
    if (lobbyId && g_state == State::Online && g_hostKnown)
        ToHost(proto::Writer(proto::kLobbySetType).Put<uint64_t>(lobbyId).Put<int32_t>(eType));
}

void OfficialSetLobbyJoinable(uint64_t lobbyId, bool joinable) {
    if (lobbyId && g_state == State::Online && g_hostKnown)
        ToHost(proto::Writer(proto::kLobbySetJoinable).Put<uint64_t>(lobbyId).Put<uint8_t>(joinable ? 1 : 0));
}

void OfficialSetLobbyMemberLimit(uint64_t lobbyId, int cMax) {
    if (lobbyId && g_state == State::Online && g_hostKnown)
        ToHost(proto::Writer(proto::kLobbySetLimit).Put<uint64_t>(lobbyId).Put<int32_t>(cMax));
}

void OfficialSetLobbyOwner(uint64_t lobbyId, uint64_t owner) {
    if (lobbyId && g_state == State::Online && g_hostKnown)
        ToHost(proto::Writer(proto::kLobbySetOwner).Put<uint64_t>(lobbyId).Put<uint64_t>(owner));
}

void OfficialSetLobbyGameServer(uint64_t lobbyId, uint32_t ip, uint16_t port, uint64_t gsId) {
    if (lobbyId && g_state == State::Online && g_hostKnown)
        ToHost(proto::Writer(proto::kLobbySetGameServer).Put<uint64_t>(lobbyId).Put<uint32_t>(ip).Put<uint16_t>(port).Put<uint64_t>(gsId));
}

void OfficialRequestLobbyData(uint64_t lobbyId) {
    if (lobbyId && g_state == State::Online && g_hostKnown)
        ToHost(proto::Writer(proto::kLobbyReqData).Put<uint64_t>(lobbyId));
}

static void EnqueueStatOp(const proto::Writer& w) {
    if (!g_wanted.load()) return;
    if (g_maskAppId.load() == 0 && !refix::overlay::settings::IsOfficialServersActive()) return;
    std::lock_guard<std::mutex> lock(g_statMu);
    if (g_statQueue.size() < 512) {
        g_statQueue.push_back(w.b);
    }
}

void ForwardStatSetAchievement(const char* name) {
    if (!name || !name[0]) return;
    EnqueueStatOp(proto::Writer(proto::kStatsSetAchievement).Str16(name));
}

void ForwardStatClearAchievement(const char* name) {
    if (!name || !name[0]) return;
    EnqueueStatOp(proto::Writer(proto::kStatsClearAchievement).Str16(name));
}

void ForwardStatIndicateProgress(const char* name, uint32_t cur, uint32_t max) {
    if (!name || !name[0] || max == 0) return;
    EnqueueStatOp(proto::Writer(proto::kStatsIndicateProgress).Str16(name).Put<uint32_t>(cur).Put<uint32_t>(max));
}

void ForwardStatSetInt(const char* name, int32_t value) {
    if (!name || !name[0]) return;
    EnqueueStatOp(proto::Writer(proto::kStatsSetInt).Str16(name).Put<int32_t>(value));
}

void ForwardStatSetFloat(const char* name, float value) {
    if (!name || !name[0]) return;
    EnqueueStatOp(proto::Writer(proto::kStatsSetFloat).Str16(name).Put<float>(value));
}

void ForwardStatStore() {
    EnqueueStatOp(proto::Writer(proto::kStatsStore));
}

void ForwardStatResetAll(bool achievementsToo) {
    EnqueueStatOp(proto::Writer(proto::kStatsResetAll).Put<uint8_t>(achievementsToo ? 1 : 0));
}

} // namespace refix::steamlink
