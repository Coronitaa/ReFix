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

#include "steam_host_proto.h"

#include <cstdlib>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

using namespace refix::steamlink::proto;

namespace {

// --- Valve flat API (leading fields of the SDK structs, stable since SDK 1.48) ----------------------------
struct NetIdentity { int32_t type; int32_t size; union { uint64_t steamId; char raw[128]; }; };
static_assert(sizeof(NetIdentity) == 136, "SteamNetworkingIdentity layout");
struct NetMessage { void* data; int32_t size; uint32_t conn; NetIdentity peer; };
struct CallbackMsg { int32_t user; int32_t id; uint8_t* param; int32_t paramSize; };
struct FriendGameInfo { uint64_t gameId; uint32_t ip; uint16_t port, queryPort; uint64_t lobby; };

constexpr int kChannel = 47;
constexpr int kSendFlags = 1 | 32;                 // UnreliableNoNagle | AutoRestartBrokenSession
constexpr int kCbSessionRequest = 1251;
constexpr int kFriendFlagImmediate = 4;
constexpr DWORD kSteamStartTimeoutMs = 180000;
constexpr DWORD kInitRetryMs = 2000;
constexpr ULONGLONG kFriendRefreshMs = 3000;
constexpr size_t kFriendBytes = 60000;             // one datagram

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
SOCKET g_sock = INVALID_SOCKET;
sockaddr_in g_game{};
HANDLE g_gameProcess = nullptr;
bool g_quit = false;
uint32_t g_appId = 0;
std::string g_gameName;
uint64_t g_self = 0;

void Post(const Writer& w) {
    if (w.b.size() <= kMaxDatagram) sendto(g_sock, (const char*)w.b.data(), (int)w.b.size(), 0, (const sockaddr*)&g_game, sizeof(g_game));
}

void Report(Step step, const char* error = "") { Post(Writer(kState).Put<uint8_t>(step).Put<uint64_t>(g_self).Str(error)); }

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
            g_api.SendMsg(g_msgs, &id, r.p, (uint32_t)r.Left(), kSendFlags, kChannel);
            break;
        }
        case kPresence:
            g_appId = r.Get<uint32_t>();
            g_gameName = r.Str();
            if (r.ok) PublishPresence(true);
            break;
        case kAvatar: SendAvatar(r.Get<int32_t>()); break;
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

bool SteamRunning() { return SteamPid() != 0; }

// SteamAPI_Init checks the registry pid too: point it back at the running client when a stray steam.exe left
// it dead (the value Steam itself keeps there).
bool SteamSignedIn() {
    const DWORD pid = SteamPid();
    if (!pid) return false;
    const wchar_t* key = L"Software\\Valve\\Steam\\ActiveProcess";
    if (RegDword(key, L"pid") != pid && !ProcessAlive(RegDword(key, L"pid")))
        RegSetKeyValueW(HKEY_CURRENT_USER, key, L"pid", REG_DWORD, &pid, sizeof(pid));
    return RegDword(key, L"ActiveUser") != 0;
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

bool StartSteamAndWait() {
    if (SteamSignedIn()) return true;
    const std::wstring exe = RegString(L"Software\\Valve\\Steam", L"SteamExe");
    if (exe.empty()) { Report(kFailed, "Steam is not installed"); return false; }
    if (!SteamRunning()) {
        Report(kOpeningSteam);
        if (!RunSteam(exe, L"-silent")) { Report(kFailed, "Could not start Steam"); return false; }
    }
    // Never another steam.exe while one runs: it would only rewrite ActiveProcess (see SteamPid).
    for (DWORD waited = 0; waited < kSteamStartTimeoutMs; waited += 500) {
        if (SteamSignedIn()) {
            Report(kConnecting);
            return Wait(2000);   // steamclient needs a moment after login before it accepts API clients
        }
        if (SteamRunning()) Report(kSigningIn);
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
        if (!g_api.dll) continue;
        const bool ok = Resolve(g_api.Init, "SteamAPI_Init") && Resolve(g_api.Shutdown, "SteamAPI_Shutdown") &&
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
        if (ok) return true;
        FreeLibrary(g_api.dll);
        g_api = Api{};
    }
    Report(kFailed, "Steam API library not found (refix_steamlink DLL)");
    return false;
}

bool InitAsSpacewar() {
    SetEnvironmentVariableW(L"SteamAppId", L"480");
    SetEnvironmentVariableW(L"SteamGameId", L"480");
    g_api.DispatchInit();
    if (!g_api.Init()) return false;
    g_msgs = g_api.Messages();
    g_friends = g_api.FriendsIface();
    void* user = g_api.UserIface();
    if (!g_msgs || !g_friends || !user) {
        g_api.Shutdown();
        return false;
    }
    g_self = g_api.GetSteamID(user);
    g_utils = g_api.UtilsIface();
    return true;
}

// Steam that has just signed in may still refuse API clients for a moment: retry until it accepts us.
bool InitWhenReady() {
    for (DWORD waited = 0;; waited += kInitRetryMs) {
        if (SteamSignedIn() && InitAsSpacewar()) return true;
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
    for (int i = 0; i < n && w.b.size() < kFriendBytes; ++i) {
        const uint64_t id = g_api.FriendByIndex(g_friends, i, kFriendFlagImmediate);
        if (!id) continue;
        const char* name = g_api.FriendName(g_friends, id);
        FriendGameInfo game{};
        const bool playing = g_api.FriendGame(g_friends, id, &game) && game.gameId != 0;
        const uint32_t gameId = playing ? (uint32_t)(game.gameId & 0xFFFFFF) : 0;
        const char* rp = g_api.FriendRP(g_friends, id, "refix");
        const bool refix = playing && gameId == 480 && rp && rp[0] == '1';
        const uint32_t appId = refix ? strtoul(g_api.FriendRP(g_friends, id, "refix_app"), nullptr, 10) : 0;
        const char* gname = refix ? g_api.FriendRP(g_friends, id, "refix_game") : "";
        const int avatar = g_api.MediumAvatar(g_friends, id);   // -1 while loading, 0 when none
        w.Put<uint64_t>(id).Put<int32_t>(g_api.FriendState(g_friends, id)).Put<uint8_t>(playing).Put<uint32_t>(gameId)
         .Put<uint8_t>(refix).Put<uint32_t>(appId).Put<int32_t>(avatar < 0 ? 0 : avatar).Str(name ? name : "").Str(gname ? gname : "");
        ++count;
    }
    memcpy(&w.b[1], &count, sizeof(count));
    Post(w);
}

void HandleCallbacks(int32_t pipe) {
    g_api.RunFrame(pipe);
    CallbackMsg cb{};
    while (g_api.NextCallback(pipe, &cb)) {
        if (cb.id == kCbSessionRequest && cb.param && cb.paramSize >= (int)sizeof(NetIdentity)) {
            const auto* remote = reinterpret_cast<const NetIdentity*>(cb.param);
            if (remote->type == 16 && g_api.HasFriend(g_friends, remote->steamId, kFriendFlagImmediate))   // friends only
                g_api.AcceptSession(g_msgs, remote);
        }
        g_api.FreeCallback(pipe);
    }
}

void ForwardMessages() {
    NetMessage* msgs[32];
    int n;
    while ((n = g_api.RecvMsgs(g_msgs, kChannel, msgs, 32)) > 0) {
        for (int i = 0; i < n; ++i) {
            if (msgs[i]->peer.type == 16 && msgs[i]->size > 0 && (size_t)msgs[i]->size < kMaxDatagram - 16)
                Post(Writer(kMessage).Put<uint64_t>(msgs[i]->peer.steamId).Raw(msgs[i]->data, (size_t)msgs[i]->size));
            g_api.Release(msgs[i]);
        }
    }
}

int Run(int argc, wchar_t** argv) {
    if (argc < 5) return 2;
    const u_short gamePort = (u_short)_wtoi(argv[1]);
    const std::string token(argv[2], argv[2] + wcslen(argv[2]));
    g_gameProcess = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)_wtoi(argv[3]));
    const std::wstring dir = argv[4];
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

    if (!StartSteamAndWait() || !LoadValve(dir) || !InitWhenReady()) {
        while (Wait(500)) {}   // stay until the game reads the failure and lets us go
        return 1;
    }
    const int32_t pipe = g_api.Pipe();
    Report(kOnline);
    PublishPresence(true);
    SendFriends();
    ULONGLONG lastFriends = GetTickCount64();
    while (Wait(2)) {
        HandleCallbacks(pipe);
        ForwardMessages();
        if (GetTickCount64() - lastFriends >= kFriendRefreshMs) {
            lastFriends = GetTickCount64();
            SendFriends();
        }
    }
    PublishPresence(false);
    HandleCallbacks(pipe);
    g_api.Shutdown();
    return 0;   // process exit: Steam stops showing Spacewar
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const int rc = argv ? Run(argc, argv) : 2;
    if (argv) LocalFree(argv);
    return rc;
}
