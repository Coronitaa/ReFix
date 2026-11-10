#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "steam_link.h"
#include "steam_host_proto.h"

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
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
std::atomic<int> g_step{ 0 };                      // proto::Step while Starting
struct Avatar { int w = 0, h = 0; std::vector<uint8_t> rgba; };
std::map<int, Avatar> g_avatars;                   // guarded by g_mu
std::map<int, ULONGLONG> g_avatarAsked;            // guarded by g_mu
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
    ToHost(proto::Writer(proto::kPresence).Put<uint32_t>(g_appId.load()).Str(name));
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

#ifdef _WIN64
constexpr const wchar_t* kHostExe = L"refix_steamhost64.exe";
#else
constexpr const wchar_t* kHostExe = L"refix_steamhost.exe";
#endif

// The helper inherits none of the game's Steam* variables (it sets Spacewar itself).
HANDLE StartHost(uint16_t port, const std::string& token) {
    const std::wstring dir = ModuleDir();
    std::wstring exe = dir + kHostExe;
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) exe = dir + L"ReFix\\" + kHostExe;
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        SetError("refix_steamhost is missing next to the game's steam_api: deploy ReFix again");
        return nullptr;
    }
    std::wstring env;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (const wchar_t* v = block; *v; v += wcslen(v) + 1) {
            if (_wcsnicmp(v, L"Steam", 5) == 0 || _wcsnicmp(v, L"_SteamInjection", 15) == 0 ||
                _wcsnicmp(v, L"ENABLE_STEAM_OVERLAY", 20) == 0) continue;
            env.append(v);
            env.push_back(L'\0');
        }
        FreeEnvironmentStringsW(block);
    }
    env.push_back(L'\0');
    const std::wstring dllDir = dir.substr(0, dir.size() - 1);
    std::wstring cmd = L"\"" + exe + L"\" " + std::to_wstring(port) + L" " + std::wstring(token.begin(), token.end()) + L" " +
                       std::to_wstring(GetCurrentProcessId()) + L" \"" + dllDir + L"\"";
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                        env.data(), dllDir.c_str(), &si, &pi)) {
        SetError("Could not start refix_steamhost");
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
    ReFixLog("[SteamLink] Online as %llu (Spacewar), app %u", (unsigned long long)self, g_appId.load());
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
    default: break;
    }
}

bool OpenIpc(uint16_t& port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    g_ipc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in self{};
    self.sin_family = AF_INET;
    self.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int selfLen = sizeof(self), big = 1 << 20;
    if (g_ipc == INVALID_SOCKET) return false;
    setsockopt(g_ipc, SOL_SOCKET, SO_RCVBUF, (const char*)&big, sizeof(big));
    setsockopt(g_ipc, SOL_SOCKET, SO_SNDBUF, (const char*)&big, sizeof(big));
    if (bind(g_ipc, (const sockaddr*)&self, sizeof(self)) != 0 || getsockname(g_ipc, (sockaddr*)&self, &selfLen) != 0) {
        closesocket(g_ipc);
        g_ipc = INVALID_SOCKET;
        return false;
    }
    if (auto cb = g_sockCb.load()) cb((uintptr_t)g_ipc, true);   // ReFix's own socket: Winsock hooks skip it
    u_long nonblock = 1;
    ioctlsocket(g_ipc, FIONBIO, &nonblock);
    port = ntohs(self.sin_port);
    return true;
}

void LinkThread() {
    uint16_t port = 0;
    if (!OpenIpc(port)) {
        SetError("Could not open the Steam helper socket");
        g_state = State::Failed;
        return;
    }
    const std::string token = NewToken();
    HANDLE host = StartHost(port, token);
    const ULONGLONG started = GetTickCount64();
    static std::vector<uint8_t> buf(proto::kMaxDatagram + 16);
    while (host && g_wanted) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(g_ipc, &fds);
        timeval tv{ 0, (long)kPumpIntervalMs * 1000 };
        if (select(0, &fds, nullptr, nullptr, &tv) > 0) {
            for (;;) {
                sockaddr_in from{};
                int fromLen = sizeof(from);
                const int n = recvfrom(g_ipc, (char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fromLen);
                if (n <= 0) break;
                HandleHost(buf.data(), (size_t)n, token, from);
            }
        }
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
