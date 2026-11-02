#define WIN32_LEAN_AND_MEAN
#include <atomic>
#include <windows.h>
#include <psapi.h>
#include "refix_steam_flat.h"
#include <isteamnetworkingmessages.h>
#include <cstdlib>
#include <mutex>
#include <map>
#include <algorithm>

extern void ReFixLog(const char* fmt, ...);
extern HMODULE g_hOriginalDll;

namespace ReFixSteamFlat {

namespace {

constexpr int kFriendFlagImmediate = 0x04;
constexpr int kSendUnreliableNoNagle = 1;
constexpr int kSendAutoRestartBrokenSession = 32;
constexpr int kSessionRequestCallback = 1251;
constexpr DWORD kResolveRetryMs = 500;

typedef void* (*fn_GetInterface_t)();
typedef uint64_t (*fn_GetSteamID_t)(void* self);
typedef bool (*fn_SetRichPresence_t)(void* self, const char* key, const char* value);
typedef int (*fn_GetFriendCount_t)(void* self, int flags);
typedef uint64_t (*fn_GetFriendByIndex_t)(void* self, int index, int flags);
typedef const char* (*fn_GetFriendRichPresence_t)(void* self, uint64_t friendID, const char* key);
typedef void (*fn_RequestFriendRichPresence_t)(void* self, uint64_t friendID);
typedef int (*fn_SendMessageToUser_t)(void* self, const SteamNetworkingIdentity& id, const void* data, uint32_t size, int flags, int channel);
typedef int (*fn_ReceiveMessagesOnChannel_t)(void* self, int channel, SteamNetworkingMessage_t** out, int max);
typedef bool (*fn_AcceptSessionWithUser_t)(void* self, const SteamNetworkingIdentity& id);
typedef uint64_t (*fn_GetLobbyOwner_t)(void* self, uint64_t lobbyId);
typedef int (*fn_GetSessionConnectionInfo_t)(void* self, const SteamNetworkingIdentity& id, SteamNetConnectionInfo_t* info,
                                             SteamNetConnectionRealTimeStatus_t* status);
typedef void (*fn_InitRelayNetworkAccess_t)(void* self);
typedef bool (*fn_SetGlobalConfigValueInt32_t)(void* self, int value, int32_t v);

struct Api {
    void* user = nullptr;
    void* friends = nullptr;
    void* messages = nullptr;
    void* matchmaking = nullptr;
    void* utils = nullptr;
    fn_GetSteamID_t getSteamID = nullptr;
    fn_SetRichPresence_t setRichPresence = nullptr;
    fn_GetFriendCount_t getFriendCount = nullptr;
    fn_GetFriendByIndex_t getFriendByIndex = nullptr;
    fn_GetFriendRichPresence_t getFriendRichPresence = nullptr;
    fn_RequestFriendRichPresence_t requestFriendRichPresence = nullptr;
    fn_SendMessageToUser_t sendMessageToUser = nullptr;
    fn_ReceiveMessagesOnChannel_t receiveMessagesOnChannel = nullptr;
    fn_AcceptSessionWithUser_t acceptSessionWithUser = nullptr;
    fn_GetLobbyOwner_t getLobbyOwner = nullptr;
    fn_GetSessionConnectionInfo_t getSessionConnectionInfo = nullptr;
    fn_InitRelayNetworkAccess_t initRelayNetworkAccess = nullptr;
    fn_SetGlobalConfigValueInt32_t setGlobalConfigValueInt32 = nullptr;
};

std::mutex g_apiMutex;
Api g_api;

template <typename T>
T Export(HMODULE module, const char* name) {
    return (T)GetProcAddress(module, name);
}

typedef int (*fn_GetHSteamUser_t)();
typedef void* (*fn_FindOrCreateUserInterface_t)(int hSteamUser, const char* version);

// One interface version: the flat accessor exported by the SDK that matches it, and its version string.
struct InterfaceVersion {
    const char* accessor;
    const char* version;
};

// The SteamAPI_ISteamX_* flat thunks dispatch through the vtable layout of the SDK the DLL was built with, so
// the interface object must be that exact version: asking steamclient for an older one (e.g. SteamFriends017
// from a v018 DLL) returns a different vtable and the thunks jump to the wrong slot. Only versions whose
// accessor this DLL exports are used; the internal lookup by version string covers games that initialized
// Steam in a way the flat accessors do not see. Versions are listed newest first.
void* Interface(HMODULE module, const InterfaceVersion* versions) {
    auto getUser = Export<fn_GetHSteamUser_t>(module, "SteamAPI_GetHSteamUser");
    auto find = Export<fn_FindOrCreateUserInterface_t>(module, "SteamInternal_FindOrCreateUserInterface");
    int user = getUser ? getUser() : 0;
    for (const InterfaceVersion* v = versions; v->accessor; v++) {
        auto fn = Export<fn_GetInterface_t>(module, v->accessor);
        if (!fn) continue; // this DLL's flat thunks do not match this version
        if (find && user) {
            __try {
                if (void* ptr = find(user, v->version)) return ptr;
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        __try {
            if (void* ptr = fn()) return ptr;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return nullptr;
}

bool HasSteamUser(HMODULE module) {
    auto getUser = Export<fn_GetHSteamUser_t>(module, "SteamAPI_GetHSteamUser");
    return getUser && getUser() != 0;
}

// Games can ship several steam_api64 copies (e.g. next to the exe and under Plugins); only the one the game
// called SteamAPI_Init on has a user. Prefer ours, otherwise any loaded original that is initialized.
HMODULE InitializedSteamModule() {
    if (g_hOriginalDll && HasSteamUser(g_hOriginalDll)) return g_hOriginalDll;
    HMODULE modules[1024];
    DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) return nullptr;
    for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 1024; i++) {
        if (GetProcAddress(modules[i], "SteamInternal_FindOrCreateUserInterface") && HasSteamUser(modules[i])) return modules[i];
    }
    return nullptr;
}

// Interfaces only become available after SteamAPI_Init, so resolution is retried until it succeeds.
Api ResolveApi() {
    std::lock_guard<std::mutex> lock(g_apiMutex);
    if (g_api.user && g_api.friends && g_api.messages) return g_api;
    static DWORD s_lastAttempt = 0;
    DWORD now = GetTickCount();
    if (s_lastAttempt && now - s_lastAttempt < kResolveRetryMs) return g_api; // module scan is not free
    s_lastAttempt = now;
    HMODULE steam = InitializedSteamModule();
    if (!steam) return g_api; // Steam not initialized yet
    static const InterfaceVersion kUser[] = { { "SteamAPI_SteamUser_v023", "SteamUser023" },
                                              { "SteamAPI_SteamUser_v022", "SteamUser022" },
                                              { "SteamAPI_SteamUser_v021", "SteamUser021" }, { nullptr, nullptr } };
    static const InterfaceVersion kFriends[] = { { "SteamAPI_SteamFriends_v018", "SteamFriends018" },
                                                 { "SteamAPI_SteamFriends_v017", "SteamFriends017" }, { nullptr, nullptr } };
    static const InterfaceVersion kMessages[] = {
        { "SteamAPI_SteamNetworkingMessages_SteamAPI_v002", "SteamNetworkingMessages002" },
        { "SteamAPI_SteamNetworkingMessages_v002", "SteamNetworkingMessages002" }, { nullptr, nullptr } };
    static const InterfaceVersion kMatchmaking[] = { { "SteamAPI_SteamMatchmaking_v009", "SteamMatchMaking009" }, { nullptr, nullptr } };
    static const InterfaceVersion kUtils[] = { { "SteamAPI_SteamNetworkingUtils_SteamAPI_v004", "SteamNetworkingUtils004" },
                                               { "SteamAPI_SteamNetworkingUtils_SteamAPI_v003", "SteamNetworkingUtils003" },
                                               { "SteamAPI_SteamNetworkingUtils_v003", "SteamNetworkingUtils003" }, { nullptr, nullptr } };
    if (!g_api.user) g_api.user = Interface(steam, kUser);
    if (!g_api.utils) g_api.utils = Interface(steam, kUtils);
    if (!g_api.friends) g_api.friends = Interface(steam, kFriends);
    if (!g_api.messages) g_api.messages = Interface(steam, kMessages);
    if (!g_api.matchmaking) g_api.matchmaking = Interface(steam, kMatchmaking);
    g_api.getLobbyOwner = Export<fn_GetLobbyOwner_t>(steam, "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    g_api.getSteamID = Export<fn_GetSteamID_t>(steam, "SteamAPI_ISteamUser_GetSteamID");
    g_api.setRichPresence = Export<fn_SetRichPresence_t>(steam, "SteamAPI_ISteamFriends_SetRichPresence");
    g_api.getFriendCount = Export<fn_GetFriendCount_t>(steam, "SteamAPI_ISteamFriends_GetFriendCount");
    g_api.getFriendByIndex = Export<fn_GetFriendByIndex_t>(steam, "SteamAPI_ISteamFriends_GetFriendByIndex");
    g_api.getFriendRichPresence = Export<fn_GetFriendRichPresence_t>(steam, "SteamAPI_ISteamFriends_GetFriendRichPresence");
    g_api.requestFriendRichPresence = Export<fn_RequestFriendRichPresence_t>(steam, "SteamAPI_ISteamFriends_RequestFriendRichPresence");
    g_api.sendMessageToUser = Export<fn_SendMessageToUser_t>(steam, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser");
    g_api.receiveMessagesOnChannel = Export<fn_ReceiveMessagesOnChannel_t>(steam, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel");
    g_api.acceptSessionWithUser = Export<fn_AcceptSessionWithUser_t>(steam, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser");
    g_api.getSessionConnectionInfo = Export<fn_GetSessionConnectionInfo_t>(steam, "SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo");
    g_api.initRelayNetworkAccess = Export<fn_InitRelayNetworkAccess_t>(steam, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess");
    g_api.setGlobalConfigValueInt32 = Export<fn_SetGlobalConfigValueInt32_t>(steam, "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32");
    static bool s_logged = false;
    static DWORD s_lastMissingLog = 0;
    if (!s_logged && !(g_api.user && g_api.friends && g_api.messages) && GetTickCount() - s_lastMissingLog > 10000) {
        s_lastMissingLog = GetTickCount();
        ReFixLog("[SteamFlat] Waiting for Steam interfaces (user=%p friends=%p messages=%p)", g_api.user, g_api.friends, g_api.messages);
    }
    if (!s_logged && g_api.user && g_api.friends && g_api.messages) {
        s_logged = true;
        ReFixLog("[SteamFlat] Resolved user=%p friends=%p messages=%p setRichPresence=%p send=%p", g_api.user, g_api.friends,
                 g_api.messages, g_api.setRichPresence, g_api.sendMessageToUser);
    }
    return g_api;
}

SteamNetworkingIdentity Identity(uint64_t steamID) {
    SteamNetworkingIdentity id;
    id.Clear();
    id.SetSteamID64(steamID);
    return id;
}

// --- Main-thread-marshaled friend/presence cache ----------------------------------------------------------
// ISteamFriends enumeration (GetFriendByIndex) crashes the Valve DLL when called off the game's main thread
// while the game is also using Steam (reliably during Photon room creation). So every ISteamFriends read
// happens on the game's main thread in ServiceMainThread(); worker threads (the Photon pump, the PA relay)
// only touch these caches. Local rich-presence writes are queued and applied on the main thread too.
namespace {
std::mutex g_cacheMutex;
std::vector<std::string> g_watchedKeys;                              // presence keys worker threads asked about
std::map<std::string, std::vector<FriendPresence>> g_presenceCache;  // key -> matching friends (last refresh)
std::vector<uint64_t> g_friendIdsCache;                              // immediate friend SteamIDs (last refresh)
std::map<std::string, std::string> g_pendingPresence;                // queued local SetRichPresence writes
bool g_pendingPresenceDirty = false;
// Set the first time a ReFix feature needs friend or presence data (Photon emulator, PA relay). Until then
// ServiceMainThread() does nothing, so games that never use those features see no ReFix ISteamFriends calls.
std::atomic<bool> g_friendsWanted{ false };
}

} // namespace

uint64_t LocalSteamID() {
    char env[32] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_ID", env, sizeof(env)) > 0) {
        uint64_t id = strtoull(env, nullptr, 10);
        if (id) return id;
    }
    Api api = ResolveApi();
    return api.user && api.getSteamID ? api.getSteamID(api.user) : 0;
}

bool SetRichPresence(const char* key, const std::string& value) {
    // Queue the write; it is applied on the game's main thread in ServiceMainThread() to avoid touching
    // ISteamFriends from a worker thread.
    g_friendsWanted.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_pendingPresence[key ? key : ""] = value;
    g_pendingPresenceDirty = true;
    return true;
}

std::string FriendRichPresence(uint64_t steamID, const char* key) {
    Api api = ResolveApi();
    if (!api.friends || !api.getFriendRichPresence) return "";
    const char* value = api.getFriendRichPresence(api.friends, steamID, key);
    return value ? value : "";
}

std::vector<FriendPresence> FriendsWithRichPresence(const char* key) {
    // Lock-free of Steam: returns the cache last refreshed on the main thread, and registers the key so the
    // next ServiceMainThread() refresh picks it up.
    std::string k = key ? key : "";
    g_friendsWanted.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    if (std::find(g_watchedKeys.begin(), g_watchedKeys.end(), k) == g_watchedKeys.end()) g_watchedKeys.push_back(k);
    auto it = g_presenceCache.find(k);
    return it != g_presenceCache.end() ? it->second : std::vector<FriendPresence>{};
}

std::vector<uint64_t> FriendSteamIDs() {
    g_friendsWanted.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    return g_friendIdsCache;
}

uint64_t LobbyOwner(uint64_t lobbyId) {
    Api api = ResolveApi();
    return api.matchmaking && api.getLobbyOwner && lobbyId ? api.getLobbyOwner(api.matchmaking, lobbyId) : 0;
}

bool SendMessage(uint64_t steamID, const void* data, uint32_t size, int channel, bool reliable) {
    Api api = ResolveApi();
    if (!api.messages || !api.sendMessageToUser || !steamID) return false;
    SteamNetworkingIdentity id = Identity(steamID);
    int sendFlags = reliable
        ? (9 | kSendAutoRestartBrokenSession) // k_nSteamNetworkingSend_ReliableNoNagle | AutoRestart
        : (kSendUnreliableNoNagle | kSendAutoRestartBrokenSession);
    int result = api.sendMessageToUser(api.messages, id, data, size, sendFlags, channel);
    return result == 1; // k_EResultOK
}

uint64_t ActiveLobbyID() {
    typedef uint64_t (*fn_GetActiveLobbyID_t)();
    static fn_GetActiveLobbyID_t pfn = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        HMODULE hSelf = GetModuleHandleA("steam_api64.dll");
        if (hSelf) pfn = (fn_GetActiveLobbyID_t)GetProcAddress(hSelf, "ReFix_GetActiveLobbyID");
    }
    return pfn ? pfn() : 0;
}

int ReceiveMessages(int channel, int maxMessages, const std::function<void(uint64_t, const uint8_t*, uint32_t)>& onMessage) {
    Api api = ResolveApi();
    if (!api.messages || !api.receiveMessagesOnChannel || maxMessages <= 0) return 0;
    std::vector<SteamNetworkingMessage_t*> msgs((size_t)maxMessages, nullptr);
    int count = api.receiveMessagesOnChannel(api.messages, channel, msgs.data(), maxMessages);
    for (int i = 0; i < count; i++) {
        SteamNetworkingMessage_t* msg = msgs[i];
        if (!msg) continue;
        uint64_t from = msg->m_identityPeer.GetSteamID64();
        if (from && msg->m_pData && msg->m_cbSize > 0) onMessage(from, (const uint8_t*)msg->m_pData, (uint32_t)msg->m_cbSize);
        msg->Release();
    }
    return count;
}

bool ConfigureNetworking(const NetworkingConfig& config) {
    Api api = ResolveApi();
    if (!api.utils || !api.initRelayNetworkAccess || !api.setGlobalConfigValueInt32) return false;
    api.initRelayNetworkAccess(api.utils); // warm SDR (relay config + POP pings) before the first send needs it
    auto set = [&](ESteamNetworkingConfigValue key, int32_t value, const char* name) {
        if (!value) return;
        bool ok = api.setGlobalConfigValueInt32(api.utils, key, value);
        ReFixLog("[SteamFlat] Networking config %s = %d%s", name, value, ok ? "" : " (rejected)");
    };
    set(k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable, config.iceEnable, "P2P_Transport_ICE_Enable");
    set(k_ESteamNetworkingConfig_SendRateMin, config.sendRateMin, "SendRateMin");
    set(k_ESteamNetworkingConfig_SendRateMax, config.sendRateMax, "SendRateMax");
    set(k_ESteamNetworkingConfig_TimeoutInitial, config.timeoutInitialMs, "TimeoutInitial");
    set(k_ESteamNetworkingConfig_TimeoutConnected, config.timeoutConnectedMs, "TimeoutConnected");
    return true;
}

bool SessionInfo(uint64_t steamID, SessionStatus& out) {
    Api api = ResolveApi();
    if (!api.messages || !api.getSessionConnectionInfo || !steamID) return false;
    SteamNetConnectionInfo_t info = {};
    SteamNetConnectionRealTimeStatus_t status = {};
    out.state = api.getSessionConnectionInfo(api.messages, Identity(steamID), &info, &status);
    if (out.state == k_ESteamNetworkingConnectionState_None) return false;
    out.relayed = (info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Relayed) != 0;
    out.pingMs = status.m_nPing;
    out.qualityLocal = status.m_flConnectionQualityLocal;
    out.queueTimeUs = (int64_t)status.m_usecQueueTime;
    out.pendingUnreliable = status.m_cbPendingUnreliable;
    out.pendingReliable = status.m_cbPendingReliable;
    out.sendRate = status.m_nSendRateBytesPerSecond;
    return true;
}

bool AcceptSession(uint64_t steamID) {
    Api api = ResolveApi();
    if (!api.messages || !api.acceptSessionWithUser || !steamID) return false;
    SteamNetworkingIdentity id = Identity(steamID);
    return api.acceptSessionWithUser(api.messages, id);
}

void OnSessionRequestCallback(const void* param, int32_t size) {
    if (!param || size < (int32_t)sizeof(SteamNetworkingIdentity)) return;
    const auto* id = (const SteamNetworkingIdentity*)param; // SteamNetworkingMessagesSessionRequest_t::m_identityRemote
    uint64_t steamID = id->GetSteamID64();
    bool accepted = AcceptSession(steamID);
    ReFixLog("[SteamFlat] Session request (callback %d) from %llu -> %s", kSessionRequestCallback,
             (unsigned long long)steamID, accepted ? "accepted" : "not pending");
}

void AcceptFriendSessions() {
    g_friendsWanted.store(true, std::memory_order_relaxed);
    std::vector<uint64_t> ids;
    { std::lock_guard<std::mutex> lock(g_cacheMutex); ids = g_friendIdsCache; }
    for (uint64_t id : ids) {
        if (AcceptSession(id)) ReFixLog("[SteamFlat] Accepted pending session from friend %llu", (unsigned long long)id);
    }
}

void ServiceMainThread() {
    if (!g_friendsWanted.load(std::memory_order_relaxed)) return;
    // MUST run on the game's main thread (called from the Steam callback pumps). Applies queued rich-presence
    // writes and refreshes the friend/presence caches that worker threads read.
    Api api = ResolveApi();
    if (!api.friends) return;

    std::map<std::string, std::string> writes;
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_pendingPresenceDirty) { writes.swap(g_pendingPresence); g_pendingPresenceDirty = false; }
    }
    if (api.setRichPresence)
        for (const auto& kv : writes)
            api.setRichPresence(api.friends, kv.first.c_str(), kv.second.empty() ? nullptr : kv.second.c_str());

    static DWORD lastRefresh = 0;
    DWORD now = GetTickCount();
    if (lastRefresh && now - lastRefresh < 1000) return; // throttle the enumeration to ~1 Hz
    lastRefresh = now;

    std::vector<std::string> keys;
    { std::lock_guard<std::mutex> lock(g_cacheMutex); keys = g_watchedKeys; }

    std::vector<uint64_t> ids;
    if (api.getFriendCount && api.getFriendByIndex) {
        int count = api.getFriendCount(api.friends, kFriendFlagImmediate);
        for (int i = 0; i < count; i++) ids.push_back(api.getFriendByIndex(api.friends, i, kFriendFlagImmediate));
    }
    std::map<std::string, std::vector<FriendPresence>> fresh;
    for (const auto& k : keys) {
        if (k.empty()) continue;
        for (uint64_t id : ids) {
            const char* val = api.getFriendRichPresence ? api.getFriendRichPresence(api.friends, id, k.c_str()) : nullptr;
            if (val && *val) fresh[k].push_back({ id, std::string(val) });
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_friendIdsCache.swap(ids);
        g_presenceCache.swap(fresh);
    }
    // Automatically accept incoming P2P sessions from all active friends
    for (uint64_t id : g_friendIdsCache) {
        AcceptSession(id);
    }
}

} // namespace ReFixSteamFlat
