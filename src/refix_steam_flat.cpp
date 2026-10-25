#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include "refix_steam_flat.h"
#include <isteamnetworkingmessages.h>
#include <cstdlib>
#include <mutex>

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

struct Api {
    void* user = nullptr;
    void* friends = nullptr;
    void* messages = nullptr;
    void* matchmaking = nullptr;
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
};

std::mutex g_apiMutex;
Api g_api;

template <typename T>
T Export(HMODULE module, const char* name) {
    return (T)GetProcAddress(module, name);
}

typedef int (*fn_GetHSteamUser_t)();
typedef void* (*fn_FindOrCreateUserInterface_t)(int hSteamUser, const char* version);

// Flat accessor first; then the internal lookup with the interface version string, which also works when
// the game initialized Steam in a way the versioned flat accessors do not see.
void* Interface(HMODULE module, const char* const* accessors, const char* version) {
    for (; *accessors; accessors++) {
        auto fn = Export<fn_GetInterface_t>(module, *accessors);
        if (void* ptr = fn ? fn() : nullptr) return ptr;
    }
    auto getUser = Export<fn_GetHSteamUser_t>(module, "SteamAPI_GetHSteamUser");
    auto find = Export<fn_FindOrCreateUserInterface_t>(module, "SteamInternal_FindOrCreateUserInterface");
    int user = getUser ? getUser() : 0;
    return find && user ? find(user, version) : nullptr;
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
    static const char* kUser[] = { "SteamAPI_SteamUser_v023", "SteamAPI_SteamUser_v022", "SteamAPI_SteamUser_v021", nullptr };
    static const char* kFriends[] = { "SteamAPI_SteamFriends_v017", nullptr };
    static const char* kMessages[] = { "SteamAPI_SteamNetworkingMessages_SteamAPI_v002", "SteamAPI_SteamNetworkingMessages_v002", nullptr };
    if (!g_api.user) g_api.user = Interface(steam, kUser, "SteamUser023");
    if (!g_api.friends) g_api.friends = Interface(steam, kFriends, "SteamFriends017");
    if (!g_api.messages) g_api.messages = Interface(steam, kMessages, "SteamNetworkingMessages002");
    static const char* kMatchmaking[] = { "SteamAPI_SteamMatchmaking_v009", nullptr };
    if (!g_api.matchmaking) g_api.matchmaking = Interface(steam, kMatchmaking, "SteamMatchMaking009");
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

std::vector<uint64_t> ImmediateFriends(const Api& api) {
    std::vector<uint64_t> out;
    if (!api.friends || !api.getFriendCount || !api.getFriendByIndex) return out;
    int count = api.getFriendCount(api.friends, kFriendFlagImmediate);
    for (int i = 0; i < count; i++) out.push_back(api.getFriendByIndex(api.friends, i, kFriendFlagImmediate));
    return out;
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
    Api api = ResolveApi();
    if (!api.friends || !api.setRichPresence) return false;
    return api.setRichPresence(api.friends, key, value.empty() ? nullptr : value.c_str());
}

std::string FriendRichPresence(uint64_t steamID, const char* key) {
    Api api = ResolveApi();
    if (!api.friends || !api.getFriendRichPresence) return "";
    const char* value = api.getFriendRichPresence(api.friends, steamID, key);
    return value ? value : "";
}

std::vector<FriendPresence> FriendsWithRichPresence(const char* key) {
    std::vector<FriendPresence> out;
    Api api = ResolveApi();
    for (uint64_t id : ImmediateFriends(api)) {
        if (api.requestFriendRichPresence) api.requestFriendRichPresence(api.friends, id);
        std::string value = FriendRichPresence(id, key);
        if (!value.empty()) out.push_back({ id, value });
    }
    return out;
}

uint64_t LobbyOwner(uint64_t lobbyId) {
    Api api = ResolveApi();
    return api.matchmaking && api.getLobbyOwner && lobbyId ? api.getLobbyOwner(api.matchmaking, lobbyId) : 0;
}

bool SendMessage(uint64_t steamID, const void* data, uint32_t size, int channel) {
    Api api = ResolveApi();
    if (!api.messages || !api.sendMessageToUser || !steamID) return false;
    SteamNetworkingIdentity id = Identity(steamID);
    int result = api.sendMessageToUser(api.messages, id, data, size, kSendUnreliableNoNagle | kSendAutoRestartBrokenSession, channel);
    return result == 1; // k_EResultOK
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
    Api api = ResolveApi();
    for (uint64_t id : ImmediateFriends(api)) {
        if (AcceptSession(id)) ReFixLog("[SteamFlat] Accepted pending session from friend %llu", (unsigned long long)id);
    }
}

} // namespace ReFixSteamFlat
