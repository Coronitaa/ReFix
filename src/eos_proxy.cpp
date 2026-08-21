// =============================================================================
// ReFix - EOSSDK-Win64-Shipping.dll — EOS Auth Emulator v3
// =============================================================================
// Full Epic Online Services authentication emulator.
// Resolves the "Signing in..." loop by:
//   1. Emulating Connect external account mappings (Steam <-> PUID) dynamically.
//   2. Faking Connect External Account Info query results with realistic data.
//   3. Retaining deferred callback queueing via Tick.
// =============================================================================

#include "upnp_firewall.h"
#include "eos/eos_connect.h"
#include <windows.h>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <stdarg.h>

// Steam P2P peer notification — calls into steam_api64.dll (loaded in same process)
// We use GetProcAddress to avoid a cross-DLL link dependency.
static void ReFix_NotifyLobbyID(uint64_t lobbyID) {
    static bool resolved = false;
    static void (*pfn)(uint64_t) = nullptr;
    if (!resolved) {
        resolved = true;
        HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
        if (hSteam) pfn = (void(*)(uint64_t))GetProcAddress(hSteam, "ReFix_NotifyLobbyID");
    }
    if (pfn) pfn(lobbyID);
}

static void ReFix_NotifyLobbyMemberChange(uint64_t lobbyID) {
    static bool resolved = false;
    static void (*pfn)(uint64_t) = nullptr;
    if (!resolved) {
        resolved = true;
        HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
        if (hSteam) pfn = (void(*)(uint64_t))GetProcAddress(hSteam, "ReFix_NotifyLobbyMemberChange");
    }
    if (pfn) pfn(lobbyID);
}



// =============================================================================
// EOS TYPES
// =============================================================================
typedef int32_t EOS_EResult;
#define EOS_Success              0
#define EOS_InvalidParameters    2
// #define EOS_NoConnection 3 (defined in eos_types.h)
#define EOS_NotFound             14
#define EOS_AlreadyConfigured    30
#define EOS_LimitExceeded        31

typedef void*    EOS_HPlatform;
typedef void*    EOS_EpicAccountId;
typedef void*    EOS_ContinuanceToken;
typedef uint64_t EOS_NotificationId;

// Steam Friends Interface Typedefs
typedef void* (*fn_SteamFriends_t)();
typedef int (*fn_GetFriendCount_t)(void* self, int friendFlags);
typedef uint64_t (*fn_GetFriendByIndex_t)(void* self, int index, int friendFlags);
typedef const char* (*fn_GetFriendPersonaName_t)(void* self, uint64_t steamIDFriend);
typedef const char* (*fn_GetPersonaName_t)(void* self);
typedef const char* (*fn_GetFriendRichPresence_t)(void* self, uint64_t steamIDFriend, const char* pchKey);

static fn_SteamFriends_t g_pfn_SteamFriends = nullptr;
static fn_GetFriendCount_t g_pfn_GetFriendCount = nullptr;
static fn_GetFriendByIndex_t g_pfn_GetFriendByIndex = nullptr;
static fn_GetFriendPersonaName_t g_pfn_GetFriendPersonaName = nullptr;
static fn_GetPersonaName_t g_pfn_GetPersonaName = nullptr;
static fn_GetFriendRichPresence_t g_pfn_GetFriendRichPresence = nullptr;

// Steam Matchmaking Interface Typedefs (for real lobby backend)
typedef void* (*fn_SteamMatchmaking_t)();
typedef uint64_t (*fn_MM_CreateLobby_t)(void* self, int eLobbyType, int cMaxMembers);
typedef uint64_t (*fn_MM_RequestLobbyList_t)(void* self);
typedef uint64_t (*fn_MM_JoinLobby_t)(void* self, uint64_t steamIDLobby);
typedef void (*fn_MM_LeaveLobby_t)(void* self, uint64_t steamIDLobby);
typedef bool (*fn_MM_SetLobbyData_t)(void* self, uint64_t steamIDLobby, const char* pchKey, const char* pchValue);
typedef const char* (*fn_MM_GetLobbyData_t)(void* self, uint64_t steamIDLobby, const char* pchKey);
typedef uint64_t (*fn_MM_GetLobbyByIndex_t)(void* self, int iLobby);
typedef void (*fn_MM_AddStringFilter_t)(void* self, const char* pchKeyToMatch, const char* pchValueToMatch, int eComparisonType);
typedef void (*fn_MM_AddResultCountFilter_t)(void* self, int cMaxResults);
typedef int (*fn_MM_GetNumLobbyMembers_t)(void* self, uint64_t steamIDLobby);
typedef void (*fn_MM_SetLobbyType_t)(void* self, uint64_t steamIDLobby, int eLobbyType);
typedef bool (*fn_MM_SetLobbyJoinable_t)(void* self, uint64_t steamIDLobby, bool bLobbyJoinable);

static fn_SteamMatchmaking_t g_pfn_SteamMatchmaking = nullptr;
static fn_MM_CreateLobby_t g_pfn_MM_CreateLobby = nullptr;
static fn_MM_RequestLobbyList_t g_pfn_MM_RequestLobbyList = nullptr;
static fn_MM_JoinLobby_t g_pfn_MM_JoinLobby = nullptr;
static fn_MM_LeaveLobby_t g_pfn_MM_LeaveLobby = nullptr;
static fn_MM_SetLobbyData_t g_pfn_MM_SetLobbyData = nullptr;
static fn_MM_GetLobbyData_t g_pfn_MM_GetLobbyData = nullptr;
static fn_MM_GetLobbyByIndex_t g_pfn_MM_GetLobbyByIndex = nullptr;
static fn_MM_AddStringFilter_t g_pfn_MM_AddStringFilter = nullptr;
static fn_MM_AddResultCountFilter_t g_pfn_MM_AddResultCountFilter = nullptr;
static fn_MM_GetNumLobbyMembers_t g_pfn_MM_GetNumLobbyMembers = nullptr;
static fn_MM_SetLobbyType_t g_pfn_MM_SetLobbyType = nullptr;
static fn_MM_SetLobbyJoinable_t g_pfn_MM_SetLobbyJoinable = nullptr;

// Steam Matchmaking state
static void* g_steamMatchmakingInterface = nullptr;
static std::atomic<uint64_t> g_currentSteamLobbyId{0}; // Steam lobby ID of the lobby we created/joined
static char g_gameFilter[128] = ""; // configurable lobby filter tag

// Lobby data cache: maps Steam lobby IDs to their host connection info
struct SteamLobbyInfo {
    uint64_t steamLobbyId;
    char hostIP[64];
    char hostPort[16];
    char eosLobbyId[64]; // The EOS lobby ID the game uses
};
static std::vector<SteamLobbyInfo> g_steamLobbies;

typedef bool (*fn_SetRichPresence_t)(void* self, const char* pchKey, const char* pchValue);
static fn_SetRichPresence_t g_pfn_SetRichPresence = nullptr;
static std::string g_activeSessionName = "RefixSession";

// Steam callback runner
typedef void (*fn_SteamAPI_RunCallbacks_t)();
typedef void (*fn_SteamAPI_RegisterCallResult_t)(void* pCallback, uint64_t hAPICall);
static fn_SteamAPI_RunCallbacks_t g_pfn_SteamRunCallbacks = nullptr;
static fn_SteamAPI_RegisterCallResult_t g_pfn_SteamRegisterCallResult = nullptr;

static void* GetSteamMatchmakingInterface();

#define EOS_LS_NotLoggedIn       0
#define EOS_LS_UsingLocalProfile 1
#define EOS_LS_LoggedIn          2

// =============================================================================
// GLOBAL CONFIGURATION & DATA
// =============================================================================
static char g_userName[128] = "ReFix User";
static char g_productUserIdStr[64] = "000102030405060708090a0b0c0d0e0f";
static char g_epicAccountIdStr[64] = "f0e0d0c0b0a009080706050403020100";
static void* GetSteamFriendsInterface();

// Forward declaration (defined below in DEBUG LOGGING section)
static void Log(const char* format, ...);

// Read game_filter from environment (set by winmm_proxy from ReFix.ini)
static void LoadGameFilterConfig() {
    char buf[128] = { 0 };
    if (GetEnvironmentVariableA("REFIX_GAME_FILTER", buf, sizeof(buf)) > 0 && buf[0] != '\0') {
        strncpy_s(g_gameFilter, sizeof(g_gameFilter), buf, _TRUNCATE);
    } else {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string iniPath(exePath);
        size_t pos = iniPath.find_last_of("\\/");
        if (pos != std::string::npos) iniPath = iniPath.substr(0, pos + 1) + "ReFix.ini";

        char iniFilter[128] = { 0 };
        GetPrivateProfileStringA("Network", "GameFilter", "", iniFilter, sizeof(iniFilter), iniPath.c_str());

        char realAppId[64] = { 0 };
        GetPrivateProfileStringA("Steam", "RealAppId", "", realAppId, sizeof(realAppId), iniPath.c_str());

        if (iniFilter[0] != '\0') {
            strncpy_s(g_gameFilter, sizeof(g_gameFilter), iniFilter, _TRUNCATE);
        } else if (realAppId[0] != '\0' && strcmp(realAppId, "0") != 0) {
            sprintf_s(g_gameFilter, sizeof(g_gameFilter), "refix_game_%s", realAppId);
        } else {
            char appId[64] = "480";
            if (GetEnvironmentVariableA("SteamAppId", appId, sizeof(appId)) > 0 && appId[0] != '\0' && strcmp(appId, "0") != 0) {
                sprintf_s(g_gameFilter, sizeof(g_gameFilter), "refix_game_%s", appId);
            } else {
                strcpy_s(g_gameFilter, "refix_game_default");
            }
        }
    }
    Log("Dynamic Game filter for lobby matching: %s", g_gameFilter);
}

// =============================================================================
// DEBUG LOGGING
static bool g_enableEosLog = false;
static bool g_eosLogConfigLoaded = false;

static void LoadEOSLogConfig() {
    if (g_eosLogConfigLoaded) return;
    g_eosLogConfigLoaded = true;

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string iniPath(exePath);
    size_t pos = iniPath.find_last_of("\\/");
    if (pos != std::string::npos) iniPath = iniPath.substr(0, pos + 1) + "ReFix.ini";

    char buf[64];
    GetPrivateProfileStringA("Debug", "EnableLog", "false", buf, sizeof(buf), iniPath.c_str());
    g_enableEosLog = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);
}

static void Log(const char* format, ...) {
    LoadEOSLogConfig();
    va_list args;
    va_start(args, format);
    char buf[1024];
    vsprintf_s(buf, sizeof(buf), format, args);
    va_end(args);
    
    SYSTEMTIME st;
    GetLocalTime(&st);

    HWND hCons = GetConsoleWindow();
    if (hCons) {
        printf("[%02d:%02d:%02d.%03d] [EOSSDK] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
        fflush(stdout);
    }

    if (!g_enableEosLog) return;

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string logPath(exePath);
    size_t pos = logPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        logPath = logPath.substr(0, pos + 1) + "ReFix.log";
    } else {
        logPath = "ReFix.log";
    }
    
    FILE* f = nullptr;
    fopen_s(&f, logPath.c_str(), "a");
    if (f) {
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [EOSSDK] %s\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
        fclose(f);
    }
}

extern "C" __declspec(dllexport) void ReFix_EOS_SetLobbyID(uint64_t lobbyID) {
    if (lobbyID != 0) {
        g_currentSteamLobbyId.store(lobbyID);
        Log("ReFix_EOS_SetLobbyID: Active Steam Lobby ID set to %llu", lobbyID);
    }
}

static char g_cachedPublicIP[64] = "";
static bool g_fetchingPublicIP = false;

static void FetchPublicIPAsync() {
    if (g_fetchingPublicIP || g_cachedPublicIP[0] != '\0') return;
    g_fetchingPublicIP = true;
    std::thread([]() {
        std::string pub = ReFixNet::GetPublicIP();
        if (!pub.empty() && pub != "127.0.0.1" && pub != "0.0.0.0") {
            strncpy_s(g_cachedPublicIP, sizeof(g_cachedPublicIP), pub.c_str(), _TRUNCATE);
            Log("Async Public IP resolved: %s", g_cachedPublicIP);
        }
        g_fetchingPublicIP = false;
    }).detach();
}

static void GetBestLocalIP(char* outIP, size_t outIPSize) {
    outIP[0] = '\0';
    GetEnvironmentVariableA("REFIX_PUBLIC_IP", outIP, (DWORD)outIPSize);
    if (outIP[0] != '\0') return;

    if (g_cachedPublicIP[0] != '\0') {
        strncpy_s(outIP, outIPSize, g_cachedPublicIP, _TRUNCATE);
        return;
    }

    // Trigger async fetch in background thread so main thread NEVER blocks
    FetchPublicIPAsync();

    GetEnvironmentVariableA("REFIX_LOCAL_IP", outIP, (DWORD)outIPSize);
    if (outIP[0] != '\0') return;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s != INVALID_SOCKET) {
        sockaddr_in target;
        memset(&target, 0, sizeof(target));
        target.sin_family = AF_INET;
        target.sin_addr.s_addr = inet_addr("1.1.1.1");
        target.sin_port = htons(53);

        if (connect(s, (sockaddr*)&target, sizeof(target)) != SOCKET_ERROR) {
            sockaddr_in name;
            int namelen = sizeof(name);
            if (getsockname(s, (sockaddr*)&name, &namelen) != SOCKET_ERROR) {
                const char* ipStr = inet_ntoa(name.sin_addr);
                if (ipStr && strcmp(ipStr, "127.0.0.1") != 0 && strcmp(ipStr, "0.0.0.0") != 0) {
                    strncpy_s(outIP, outIPSize, ipStr, _TRUNCATE);
                    closesocket(s);
                    return;
                }
            }
        }
        closesocket(s);
    }

    char hostName[256] = {0};
    if (gethostname(hostName, sizeof(hostName)) == 0) {
        struct hostent* host = gethostbyname(hostName);
        if (host && host->h_addr_list && host->h_addr_list[0]) {
            for (int i = 0; host->h_addr_list[i] != nullptr; ++i) {
                struct in_addr addr;
                memcpy(&addr, host->h_addr_list[i], sizeof(struct in_addr));
                const char* ipStr = inet_ntoa(addr);
                if (ipStr && strncmp(ipStr, "127.", 4) != 0 && strcmp(ipStr, "0.0.0.0") != 0) {
                    strncpy_s(outIP, outIPSize, ipStr, _TRUNCATE);
                    return;
                }
            }
        }
    }

    strcpy_s(outIP, outIPSize, "127.0.0.1");
}

// =============================================================================
// FAKE HANDLES
// =============================================================================
static char s_handles[64] = {};
#define HANDLE_PLATFORM         ((void*)&s_handles[0])
#define HANDLE_AUTH             ((void*)&s_handles[1])
#define HANDLE_CONNECT          ((void*)&s_handles[2])
#define HANDLE_USERINFO         ((void*)&s_handles[3])
#define HANDLE_LOBBY            ((void*)&s_handles[4])
#define HANDLE_P2P              ((void*)&s_handles[5])
#define HANDLE_FRIENDS          ((void*)&s_handles[6])
#define HANDLE_PRESENCE         ((void*)&s_handles[7])
#define HANDLE_UI               ((void*)&s_handles[8])
#define HANDLE_SESSIONS         ((void*)&s_handles[9])
#define HANDLE_STATS            ((void*)&s_handles[10])
#define HANDLE_ACHIEVEMENTS     ((void*)&s_handles[11])
#define HANDLE_LEADERBOARDS     ((void*)&s_handles[12])
#define HANDLE_METRICS          ((void*)&s_handles[13])
#define HANDLE_MODS             ((void*)&s_handles[14])
#define HANDLE_ECOM             ((void*)&s_handles[15])
#define HANDLE_REPORTS          ((void*)&s_handles[16])
#define HANDLE_SANCTIONS        ((void*)&s_handles[17])
#define HANDLE_ANTICHEAT_CLIENT ((void*)&s_handles[18])
#define HANDLE_ANTICHEAT_SERVER ((void*)&s_handles[19])
#define HANDLE_CUSTOM_INVITES   ((void*)&s_handles[20])
#define HANDLE_PLAYER_DATA      ((void*)&s_handles[21])
#define HANDLE_TITLE_STORAGE    ((void*)&s_handles[22])
#define HANDLE_RTC              ((void*)&s_handles[23])
#define HANDLE_RTC_ADMIN        ((void*)&s_handles[24])
#define HANDLE_KWS              ((void*)&s_handles[25])
#define HANDLE_PROGRESSION      ((void*)&s_handles[26])
#define HANDLE_LOBBY_SEARCH     ((void*)&s_handles[27])
#define HANDLE_LOBBY_MODIFICATION ((void*)&s_handles[28])
#define HANDLE_SESSION_MODIFICATION ((void*)&s_handles[29])
#define HANDLE_INTEGRATED_PLATFORM  ((void*)&s_handles[30])

static char s_fakeProductUserId[64]  = "refix_product_user_00001";
static char s_fakeEpicAccountId[64]  = "refix_epic_account_00001";
#define FAKE_PRODUCT_USER_ID    ((EOS_ProductUserId)s_fakeProductUserId)
#define FAKE_EPIC_ACCOUNT_ID    ((EOS_EpicAccountId)s_fakeEpicAccountId)

static EOS_NotificationId s_nextNotifId = 1000;
static int32_t g_lastConnectLoginCredType = 1; // 1 = Steam, 18 = Epic ID Token

// =============================================================================
// DYNAMIC MAPPING MANAGEMENT
// =============================================================================
static std::unordered_map<std::string, void*> g_puidMap;

static void* GetOrCreateProductUserId(const std::string& externalId) {
    if (externalId.empty()) return nullptr;
    
    auto it = g_puidMap.find(externalId);
    if (it != g_puidMap.end()) {
        return it->second;
    }
    
    char* fakePuid = (char*)malloc(64);
    sprintf_s(fakePuid, 64, "refix_puid_%s", externalId.c_str());
    g_puidMap[externalId] = fakePuid;
    Log("GetOrCreateProductUserId: Mapped %s -> %p", externalId.c_str(), fakePuid);
    return fakePuid;
}

static std::string FindExternalId(void* puid) {
    for (auto& pair : g_puidMap) {
        if (pair.second == puid) return pair.first;
    }
    if (puid == FAKE_PRODUCT_USER_ID) return g_productUserIdStr;
    return "";
}


static std::string GetDisplayNameForExternalId(const std::string& externalId) {
    if (externalId == g_productUserIdStr || externalId.empty()) return g_userName;
    
    uint64_t steamId = _strtoui64(externalId.c_str(), nullptr, 10);
    if (steamId != 0) {
        void* steamFriends = GetSteamFriendsInterface();
        if (steamFriends && g_pfn_GetFriendPersonaName) {
            const char* name = g_pfn_GetFriendPersonaName(steamFriends, steamId);
            if (name && name[0] != '\0') {
                return name;
            }
        }
    }
    
    if (externalId.length() >= 4) return "Player_" + externalId.substr(externalId.length() - 4);
    return "Player_" + externalId;
}

// Heuristics to find string and PUID pointers dynamically inside variable-size options structs
static const char* FindStringPointer(const void* optionsStruct, size_t sizeBytes) {
    if (!optionsStruct) return nullptr;
    const uint64_t* ptrs = (const uint64_t*)optionsStruct;
    size_t count = sizeBytes / 8;
    for (size_t i = 0; i < count; i++) {
        uint64_t val = ptrs[i];
        if (val > 0x10000 && !IsBadReadPtr((const void*)val, 1)) {
            const char* str = (const char*)val;
            bool printable = true;
            size_t len = 0;
            while (len < 128) {
                char c = str[len];
                if (c == '\0') break;
                if (c < 32 || c > 126) { printable = false; break; }
                len++;
            }
            if (printable && len > 0) return str;
        }
    }
    return nullptr;
}

static void* FindPuidPointer(const void* optionsStruct, size_t sizeBytes) {
    if (!optionsStruct) return nullptr;
    const void** ptrs = (const void**)optionsStruct;
    size_t count = sizeBytes / 8;
    for (size_t i = 0; i < count; i++) {
        const void* val = ptrs[i];
        for (auto& pair : g_puidMap) {
            if (pair.second == val) return pair.second;
        }
        if (val == FAKE_PRODUCT_USER_ID) return FAKE_PRODUCT_USER_ID;
    }
    return nullptr;
}

// =============================================================================
// CONNECT STRUCTURES
// =============================================================================
static EOS_Connect_ExternalAccountInfo* CreateEpicExternalAccountInfo(void* puid) {
    auto* info = (EOS_Connect_ExternalAccountInfo*)malloc(sizeof(EOS_Connect_ExternalAccountInfo));
    if (!info) return nullptr;
    
    info->ApiVersion = 1;
    info->ProductUserId = puid;
    info->DisplayName = _strdup(g_userName);
    info->AccountId = _strdup(g_epicAccountIdStr);
    info->AccountIdType = 0; // EOS_EAT_EPIC
    info->LastLoginTime = -1;
    
    Log("CreateEpicExternalAccountInfo: Generated details for PUID %p (Name=%s, EpicID=%s)", puid, g_userName, g_epicAccountIdStr);
    return info;
}

static EOS_Connect_ExternalAccountInfo* CreateSteamExternalAccountInfo(void* puid) {
    auto* info = (EOS_Connect_ExternalAccountInfo*)malloc(sizeof(EOS_Connect_ExternalAccountInfo));
    if (!info) return nullptr;
    
    std::string extId = FindExternalId(puid);
    if (extId.empty()) extId = g_productUserIdStr;
    std::string dispName = g_userName;
    if (dispName.empty() || dispName == "Player" || dispName == "ReFix User") {
        dispName = GetDisplayNameForExternalId(extId);
    }
    
    info->ApiVersion = 1;
    info->ProductUserId = puid;
    info->DisplayName = _strdup(dispName.c_str());
    info->AccountId = _strdup(extId.c_str());
    info->AccountIdType = 1; // EOS_EAT_STEAM
    info->LastLoginTime = -1;
    
    Log("CreateSteamExternalAccountInfo: Generated details for PUID %p (Name=%s, SteamID=%s)", puid, dispName.c_str(), extId.c_str());
    return info;
}

static EOS_Connect_ExternalAccountInfo* CreateExternalAccountInfo(void* puid) {
    if (g_lastConnectLoginCredType == 18 || g_lastConnectLoginCredType == 0) {
        return CreateEpicExternalAccountInfo(puid);
    } else {
        return CreateSteamExternalAccountInfo(puid);
    }
}

static void FreeExternalAccountInfo(EOS_Connect_ExternalAccountInfo* info) {
    if (info) {
        if (info->DisplayName) free((void*)info->DisplayName);
        if (info->AccountId) free((void*)info->AccountId);
        free(info);
    }
}

// =============================================================================
// FAKE AUTH TOKENS
// =============================================================================
struct FakeAuthToken {
    int32_t     ApiVersion;
    const char* App;
    const char* ClientId;
    void*       AccountId;
    const char* AccessToken;
    double      ExpiresIn;
    const char* ExpiresAt;
    int32_t     AuthType;
    const char* RefreshToken;
    double      RefreshExpiresIn;
    const char* RefreshExpiresAt;
};

static FakeAuthToken s_fakeAuthToken = {
    3,
    "refix",
    "xyza7891muomRmynIIHaJB9DogUVA",
    nullptr,
    "refix_access_token_aabbccdd00112233445566778899eeff",
    86400.0,
    "2099-12-31T23:59:59.999Z",
    0,
    "refix_refresh_token_ffeeddccbbaa99887766554433221100",
    604800.0,
    "2099-12-31T23:59:59.999Z"
};

struct FakeIdToken {
    int32_t     ApiVersion;
    void*       AccountId;
    const char* JsonWebToken;
};

static FakeIdToken s_fakeIdToken = {
    1,
    nullptr,
    "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJyZWZpeCJ9.fake_signature"
};

struct FakeConnectIdToken {
    int32_t     ApiVersion;
    void*       ProductUserId;
    const char* JsonWebToken;
};

static FakeConnectIdToken s_fakeConnectIdToken = {
    1,
    FAKE_PRODUCT_USER_ID,
    "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJyZWZpeF9wdWlkIn0.fake_signature"
};

// =============================================================================
// DEFERRED CALLBACK QUEUE
// =============================================================================
typedef void (*GenericCallbackFn)(const void* Data);

struct PendingCallback {
    GenericCallbackFn callback;
    uint8_t data[128];
    bool active;
};

#define MAX_PENDING_CALLBACKS 32
static PendingCallback g_pendingCallbacks[MAX_PENDING_CALLBACKS];
static int g_pendingCount = 0;
static CRITICAL_SECTION g_callbackCS;
static bool g_csInitialized = false;

template<typename TCallbackInfo>
static void QueueCallback(void* callbackFn, const TCallbackInfo& info) {
    if (!callbackFn) return;
    EnterCriticalSection(&g_callbackCS);
    if (g_pendingCount < MAX_PENDING_CALLBACKS) {
        PendingCallback& p = g_pendingCallbacks[g_pendingCount++];
        p.callback = (GenericCallbackFn)callbackFn;
        memcpy(p.data, &info, sizeof(TCallbackInfo));
        p.active = true;
    }
    LeaveCriticalSection(&g_callbackCS);
}

static void FlushCallbacks() {
    PendingCallback localQueue[MAX_PENDING_CALLBACKS];
    int count = 0;
    EnterCriticalSection(&g_callbackCS);
    count = g_pendingCount;
    if (count > 0) {
        memcpy(localQueue, g_pendingCallbacks, sizeof(PendingCallback) * count);
        g_pendingCount = 0;
    }
    LeaveCriticalSection(&g_callbackCS);
    
    if (count > 0) {
        Log("FlushCallbacks: Firing %d pending callbacks", count);
    }
    for (int i = 0; i < count; i++) {
        if (localQueue[i].callback) {
            localQueue[i].callback(localQueue[i].data);
        }
    }
}

// =============================================================================
// CALLBACK INFO STRUCTS
// =============================================================================
#pragma pack(push, 8)
struct CB_Auth_LoginStatusChanged {
    void*   ClientData;
    void*   LocalUserId;
    int32_t PrevStatus;
    int32_t CurrentStatus;
};

struct CB_Connect_LoginStatusChanged {
    void*   ClientData;
    void*   LocalUserId;
    int32_t PrevStatus;
    int32_t CurrentStatus;
};

struct CB_Auth_Login {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
    void*       PinGrantInfo;
    void*       ContinuanceToken;
    void*       SelectedAccountId;
};
struct CB_Auth_Logout {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
};
struct CB_Auth_DeletePersistentAuth {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};
struct CB_Auth_QueryIdToken {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
};
struct CB_Auth_VerifyIdToken {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};
struct CB_Auth_VerifyUserAuth {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};
struct CB_Auth_LinkAccount {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
    void*       PinGrantInfo;
    void*       SelectedAccountId;
};
struct CB_Connect_Login {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
    void*       ContinuanceToken;
};
struct CB_Connect_CreateDeviceId {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};
struct CB_Connect_CreateUser {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
};
struct CB_Connect_Generic {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
};
struct CB_Connect_QueryMappings {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
};
struct CB_UserInfo_Query {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
    void*       TargetUserId;
};
#pragma pack(pop)

// =============================================================================
// PROC TABLE
// =============================================================================
#define EOS_FORWARD_COUNT 679
extern "C" {
    __declspec(dllexport) FARPROC g_eosProcs[EOS_FORWARD_COUNT] = { 0 };
}
static const char* g_eosNames[EOS_FORWARD_COUNT];

// =============================================================================
// GENERIC STUB
// =============================================================================
#pragma pack(push, 1)
struct Trampoline {
    uint8_t  mov_rcx_imm[2];   // 48 B9
    uint64_t index;            // index value
    uint8_t  mov_rax_imm[2];   // 48 B8
    uint64_t handler;          // address of CommonStubHandler
    uint8_t  jmp_rax[2];       // FF E0
};
#pragma pack(pop)

static Trampoline* g_trampolines = nullptr;

extern "C" int64_t CommonStubHandler(int64_t index) {
    Log("Generic stub called: %s (index %lld, ptr=%p, tramp=%p)", g_eosNames[index], index, (index < EOS_FORWARD_COUNT ? g_eosProcs[index] : nullptr), (g_trampolines ? &g_trampolines[index] : nullptr));
    if (g_eosNames[index] && strstr(g_eosNames[index], "AddNotify")) {
        return (int64_t)(s_nextNotifId++);
    }
    return 0; // return EOS_Success
}

static void SetupTrampolines() {
    g_trampolines = (Trampoline*)VirtualAlloc(NULL, sizeof(Trampoline) * EOS_FORWARD_COUNT, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_trampolines) return;
    
    for (int i = 0; i < EOS_FORWARD_COUNT; i++) {
        g_trampolines[i].mov_rcx_imm[0] = 0x48;
        g_trampolines[i].mov_rcx_imm[1] = 0xB9;
        g_trampolines[i].index = (uint64_t)i;
        
        g_trampolines[i].mov_rax_imm[0] = 0x48;
        g_trampolines[i].mov_rax_imm[1] = 0xB8;
        g_trampolines[i].handler = (uint64_t)CommonStubHandler;
        
        g_trampolines[i].jmp_rax[0] = 0xFF;
        g_trampolines[i].jmp_rax[1] = 0xE0;
    }
}

// =============================================================================
// EMULATED FUNCTIONS
// =============================================================================
static EOS_EResult eos_Initialize(void* Options) {
    Log("EOS_Initialize called");
    return EOS_Success;
}

static uint64_t GetMachineUniqueHash() {
    static uint64_t s_hash = 0;
    if (s_hash != 0) return s_hash;

    char compName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    DWORD compLen = sizeof(compName);
    GetComputerNameA(compName, &compLen);

    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);

    // FNV-1a 64-bit hash
    uint64_t h = 14695981039346656037ULL;
    for (const char* p = compName; *p; ++p) {
        h ^= (uint8_t)*p;
        h *= 1099511628211ULL;
    }
    h ^= volSerial;
    h *= 1099511628211ULL;

    s_hash = h ? h : 0x123456789ABCDEF0ULL;
    return s_hash;
}

static uint64_t GetMachineUniqueSteamID() {
    char envBuf[64] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_ID", envBuf, sizeof(envBuf)) > 0) {
        uint64_t sid = _strtoui64(envBuf, nullptr, 10);
        if (sid != 0) return sid;
    }
    uint64_t mHash = GetMachineUniqueHash();
    uint32_t accountId = (uint32_t)(mHash & 0x0FFFFFFF);
    if (accountId == 0) accountId = 100001;
    return 76561197960265728ULL + accountId;
}

// Re-check Steam persona name and SteamId (set by steam_proxy after SteamAPI_Init)
// Called lazily since EOS DllMain runs before SteamAPI_Init
static bool g_userNameRefreshed = false;
static void RefreshUserName() {
    if (g_userNameRefreshed) return;
    
    bool refreshed = false;
    
    // Refresh username if still using fallback
    if (strcmp(g_userName, "ReFix User") == 0) {
        char envBuf[128] = {0};
        if (GetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
            strcpy_s(g_userName, sizeof(g_userName), envBuf);
            Log("RefreshUserName: Updated username from Steam persona name: %s", g_userName);
            refreshed = true;
        }
    } else {
        refreshed = true; // User set a custom name
    }
    
    // Refresh SteamId and PUID with unique Steam ID per PC
    uint64_t uniqueSteamId = GetMachineUniqueSteamID();
    sprintf_s(g_productUserIdStr, sizeof(g_productUserIdStr), "%llu", uniqueSteamId);
    sprintf_s(s_fakeProductUserId, sizeof(s_fakeProductUserId), "%llu", uniqueSteamId);
    sprintf_s(s_fakeEpicAccountId, sizeof(s_fakeEpicAccountId), "epic_%llu", uniqueSteamId);
    Log("RefreshUserName: Unique SteamId & PUID for PC: %s", g_productUserIdStr);
    
    g_userNameRefreshed = refreshed;
}

static EOS_EResult eos_Shutdown() {
    Log("EOS_Shutdown called");
    return EOS_Success;
}

static void* eos_Platform_Create(void* Options) {
    Log("EOS_Platform_Create called");
    // Re-check Steam persona name (may have been set by steam_proxy after SteamAPI_Init)
    RefreshUserName();
    ReFixNet::AutoOpenPorts();
    return HANDLE_PLATFORM;
}

static void eos_Platform_Tick(void* Handle) {
    FlushCallbacks();
    ReFixEOS::CallbackManager::Get().FlushCallbacks();
}

static void eos_Platform_Release(void* Handle) {
    Log("EOS_Platform_Release called");
}

static EOS_EResult eos_Platform_CheckForLauncherAndRestart(void* Handle) {
    Log("EOS_Platform_CheckForLauncherAndRestart called -> EOS_Success");
    return EOS_Success;
}

static int32_t eos_Platform_GetApplicationStatus(void* Handle) {
    return 3; // EOS_AS_Foreground
}

static EOS_EResult eos_Platform_SetApplicationStatus(void* Handle, int32_t Status) {
    return EOS_Success;
}

static int32_t eos_Platform_GetNetworkStatus(void* Handle) {
    return 2; // EOS_NS_Online
}

static EOS_EResult eos_Platform_SetNetworkStatus(void* Handle, int32_t Status) {
    return EOS_Success;
}

static EOS_EResult eos_Platform_GetActiveCountryCode(void* Handle, void* LocalUserId, char* OutBuffer, int32_t* InOutBufferLength) {
    if (OutBuffer && InOutBufferLength && *InOutBufferLength >= 3) {
        strcpy_s(OutBuffer, *InOutBufferLength, "US");
        *InOutBufferLength = 2;
    }
    return EOS_Success;
}

static EOS_EResult eos_Platform_GetActiveLocaleCode(void* Handle, void* LocalUserId, char* OutBuffer, int32_t* InOutBufferLength) {
    if (OutBuffer && InOutBufferLength && *InOutBufferLength >= 6) {
        strcpy_s(OutBuffer, *InOutBufferLength, "en-US");
        *InOutBufferLength = 5;
    }
    return EOS_Success;
}

static EOS_EResult eos_Platform_GetOverrideCountryCode(void* Handle, char* OutBuffer, int32_t* InOutBufferLength) {
    if (OutBuffer && InOutBufferLength && *InOutBufferLength >= 3) {
        strcpy_s(OutBuffer, *InOutBufferLength, "US");
        *InOutBufferLength = 2;
    }
    return EOS_Success;
}

static EOS_EResult eos_Platform_GetOverrideLocaleCode(void* Handle, char* OutBuffer, int32_t* InOutBufferLength) {
    if (OutBuffer && InOutBufferLength && *InOutBufferLength >= 6) {
        strcpy_s(OutBuffer, *InOutBufferLength, "en-US");
        *InOutBufferLength = 5;
    }
    return EOS_Success;
}

static int32_t eos_Platform_GetDesktopCrossplayStatus(void* Handle, void* Options, void* OutDesktopCrossplayStatusInfo) {
    return EOS_Success;
}

static EOS_EResult eos_ByteArray_ToString(const uint8_t* ByteArray, const uint32_t Length, char* OutBuffer, uint32_t* InOutBufferLength) {
    Log("EOS_ByteArray_ToString called (Length=%u)", Length);
    if (!ByteArray || !InOutBufferLength) return EOS_InvalidParameters;
    uint32_t needed = Length * 2 + 1;
    if (!OutBuffer || *InOutBufferLength < needed) {
        *InOutBufferLength = needed;
        return EOS_LimitExceeded;
    }
    static const char hexDigits[] = "0123456789abcdef";
    for (uint32_t i = 0; i < Length; ++i) {
        OutBuffer[i * 2]     = hexDigits[(ByteArray[i] >> 4) & 0x0F];
        OutBuffer[i * 2 + 1] = hexDigits[ByteArray[i] & 0x0F];
    }
    OutBuffer[Length * 2] = '\0';
    *InOutBufferLength = needed;
    return EOS_Success;
}

static EOS_EResult eos_ByteArray_FromString(const char* HexString, uint8_t* OutBuffer, uint32_t* InOutBufferLength) {
    Log("EOS_ByteArray_FromString called");
    if (!HexString || !InOutBufferLength) return EOS_InvalidParameters;
    size_t len = strlen(HexString);
    if (len % 2 != 0) return EOS_InvalidParameters;
    uint32_t needed = (uint32_t)(len / 2);
    if (!OutBuffer || *InOutBufferLength < needed) {
        *InOutBufferLength = needed;
        return EOS_LimitExceeded;
    }
    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < needed; ++i) {
        int h = hexVal(HexString[i * 2]);
        int l = hexVal(HexString[i * 2 + 1]);
        if (h < 0 || l < 0) return EOS_InvalidParameters;
        OutBuffer[i] = (uint8_t)((h << 4) | l);
    }
    *InOutBufferLength = needed;
    return EOS_Success;
}

extern "C" __declspec(dllexport) EOS_EResult EOS_ByteArray_ToString(const uint8_t* ByteArray, const uint32_t Length, char* OutBuffer, uint32_t* InOutBufferLength) {
    return eos_ByteArray_ToString(ByteArray, Length, OutBuffer, InOutBufferLength);
}

extern "C" __declspec(dllexport) EOS_EResult EOS_ByteArray_FromString(const char* HexString, uint8_t* OutBuffer, uint32_t* InOutBufferLength) {
    return eos_ByteArray_FromString(HexString, OutBuffer, InOutBufferLength);
}

static void* eos_GetAuthInterface(void* h)              { return HANDLE_AUTH; }
static void* eos_GetConnectInterface(void* h)            { return HANDLE_CONNECT; }
static void* eos_GetLobbyInterface(void* h)              { return HANDLE_LOBBY; }
static void* eos_GetP2PInterface(void* h)                { return HANDLE_P2P; }
static void* eos_GetUserInfoInterface(void* h)           { return HANDLE_USERINFO; }
static void* eos_GetFriendsInterface(void* h)            { return HANDLE_FRIENDS; }
static void* eos_GetPresenceInterface(void* h)           { return HANDLE_PRESENCE; }
static void* eos_GetUIInterface(void* h)                 { return HANDLE_UI; }
static void* eos_GetSessionsInterface(void* h)           { return HANDLE_SESSIONS; }
static void* eos_GetStatsInterface(void* h)              { return HANDLE_STATS; }
static void* eos_GetAchievementsInterface(void* h)       { return HANDLE_ACHIEVEMENTS; }
static void* eos_GetLeaderboardsInterface(void* h)       { return HANDLE_LEADERBOARDS; }
static void* eos_GetMetricsInterface(void* h)            { return HANDLE_METRICS; }
static void* eos_GetModsInterface(void* h)               { return HANDLE_MODS; }
static void* eos_GetEcomInterface(void* h)               { return HANDLE_ECOM; }
static void* eos_GetReportsInterface(void* h)            { return HANDLE_REPORTS; }
static void* eos_GetSanctionsInterface(void* h)          { return HANDLE_SANCTIONS; }
static void* eos_GetAntiCheatClientInterface(void* h)    { return HANDLE_ANTICHEAT_CLIENT; }
static void* eos_GetAntiCheatServerInterface(void* h)    { return HANDLE_ANTICHEAT_SERVER; }
static void* eos_GetCustomInvitesInterface(void* h)      { return HANDLE_CUSTOM_INVITES; }
static void* eos_GetPlayerDataStorageInterface(void* h)  { return HANDLE_PLAYER_DATA; }
static void* eos_GetTitleStorageInterface(void* h)       { return HANDLE_TITLE_STORAGE; }
static void* eos_GetRTCInterface(void* h)                { return HANDLE_RTC; }
static void* eos_GetRTCAdminInterface(void* h)           { return HANDLE_RTC_ADMIN; }
static void* eos_GetKWSInterface(void* h)                { return HANDLE_KWS; }
static void* eos_GetProgressionSnapshotInterface(void* h){ return HANDLE_PROGRESSION; }
static void* eos_GetIntegratedPlatformInterface(void* h) { return HANDLE_INTEGRATED_PLATFORM; }

// --- IntegratedPlatform Interface ---
static EOS_NotificationId eos_IntegratedPlatform_AddNotifyUserLoginStatusChanged(void* H, void* O, void* C, void* Cb) {
    return s_nextNotifId++;
}
static void eos_IntegratedPlatform_RemoveNotifyUserLoginStatusChanged(void* H, EOS_NotificationId Id) { }
static EOS_EResult eos_IntegratedPlatform_SetUserLoginStatus(void* H, void* O) {
    return EOS_Success;
}
static EOS_EResult eos_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer(void* O, void** OutContainer) {
    if (OutContainer) *OutContainer = (void*)0x12345678;
    return EOS_Success;
}
static EOS_EResult eos_IntegratedPlatformOptionsContainer_Add(void* H, void* O) {
    return EOS_Success;
}
static void eos_IntegratedPlatformOptionsContainer_Release(void* H) { }

// --- Auth Interface ---
static EOS_NotificationId eos_Auth_AddNotifyLoginStatusChanged(void* Handle, void* Options, void* ClientData, void* Callback) {
    Log("EOS_Auth_AddNotifyLoginStatusChanged registered (id=%llu)", s_nextNotifId);
    return s_nextNotifId++;
}

static EOS_EResult eos_Auth_CopyIdToken(void* Handle, void* Options, FakeIdToken** OutToken) {
    Log("EOS_Auth_CopyIdToken called");
    if (OutToken) {
        s_fakeIdToken.AccountId = FAKE_EPIC_ACCOUNT_ID;
        *OutToken = &s_fakeIdToken;
    }
    return EOS_Success;
}

static EOS_EResult eos_Auth_CopyUserAuthToken(void* Handle, void* Options, void* AccountId, FakeAuthToken** OutToken) {
    Log("EOS_Auth_CopyUserAuthToken called");
    if (OutToken) {
        s_fakeAuthToken.AccountId = FAKE_EPIC_ACCOUNT_ID;
        *OutToken = &s_fakeAuthToken;
    }
    return EOS_Success;
}

static void eos_Auth_DeletePersistentAuth(void* Handle, void* Options, void* ClientData, void* Callback) {
    Log("EOS_Auth_DeletePersistentAuth called");
    CB_Auth_DeletePersistentAuth info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = ClientData;
    QueueCallback(Callback, info);
}

static void* eos_Auth_GetLoggedInAccountByIndex(void* Handle, int32_t Index) {
    return (Index == 0) ? FAKE_EPIC_ACCOUNT_ID : nullptr;
}

static int32_t eos_Auth_GetLoggedInAccountsCount(void* Handle) {
    return 1;
}

static int32_t eos_Auth_GetLoginStatus(void* Handle, void* LocalUserId) {
    Log("EOS_Auth_GetLoginStatus (LocalUserId=%p) -> EOS_LS_LoggedIn", LocalUserId);
    return EOS_LS_LoggedIn;
}

static void* eos_Auth_GetMergedAccountByIndex(void* Handle, void* LocalUserId, uint32_t Index) { return nullptr; }
static uint32_t eos_Auth_GetMergedAccountsCount(void* Handle, void* LocalUserId) { return 0; }

static EOS_EResult eos_Auth_GetSelectedAccountId(void* Handle, void* LocalUserId, void** OutSelectedAccountId) {
    if (OutSelectedAccountId) *OutSelectedAccountId = FAKE_EPIC_ACCOUNT_ID;
    return EOS_Success;
}

static void eos_Auth_IdToken_Release(void* Token) { }

static void eos_Auth_LinkAccount(void* Handle, void* Options, void* ClientData, void* Callback) {
    Log("EOS_Auth_LinkAccount called");
    CB_Auth_LinkAccount info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = ClientData;
    info.LocalUserId = FAKE_EPIC_ACCOUNT_ID;
    info.PinGrantInfo = nullptr;
    info.SelectedAccountId = FAKE_EPIC_ACCOUNT_ID;
    QueueCallback(Callback, info);
}

static void eos_Auth_Login(void* Handle, void* Options, void* ClientData, void* Callback) {
    Log("EOS_Auth_Login called");
    CB_Auth_Login info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = ClientData;
    info.LocalUserId = FAKE_EPIC_ACCOUNT_ID;
    info.PinGrantInfo = nullptr;
    info.ContinuanceToken = nullptr;
    info.SelectedAccountId = FAKE_EPIC_ACCOUNT_ID;
    QueueCallback(Callback, info);
}

static void eos_Auth_Logout(void* Handle, void* Options, void* ClientData, void* Callback) {
    Log("EOS_Auth_Logout called");
    CB_Auth_Logout info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = ClientData;
    info.LocalUserId = FAKE_EPIC_ACCOUNT_ID;
    QueueCallback(Callback, info);
}

static void eos_Auth_QueryIdToken(void* Handle, void* Options, void* ClientData, void* Callback) {
    Log("EOS_Auth_QueryIdToken called");
    CB_Auth_QueryIdToken info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = ClientData;
    info.LocalUserId = FAKE_EPIC_ACCOUNT_ID;
    QueueCallback(Callback, info);
}

static void eos_Auth_RemoveNotifyLoginStatusChanged(void* Handle, EOS_NotificationId Id) { }
static void eos_Auth_Token_Release(void* Token) { }

static void eos_Auth_VerifyIdToken(void* Handle, void* Options, void* ClientData, void* Callback) {
    Log("EOS_Auth_VerifyIdToken called");
    CB_Auth_VerifyIdToken info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = ClientData;
    QueueCallback(Callback, info);
}

static void eos_Auth_VerifyUserAuth(void* Handle, void* Options, void* ClientData, void* Callback) {
    Log("EOS_Auth_VerifyUserAuth called");
    CB_Auth_VerifyUserAuth info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = ClientData;
    QueueCallback(Callback, info);
}

// --- Connect Interface ---
static EOS_NotificationId eos_Connect_AddNotifyAuthExpiration(void* H, void* O, void* C, void* Cb) { return s_nextNotifId++; }
static EOS_NotificationId eos_Connect_AddNotifyLoginStatusChanged(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_AddNotifyLoginStatusChanged registered (id=%llu)", s_nextNotifId);
    return s_nextNotifId++;
}

static EOS_EResult eos_Connect_CopyIdToken(void* H, void* O, FakeConnectIdToken** Out) {
    Log("EOS_Connect_CopyIdToken called");
    if (Out) {
        s_fakeConnectIdToken.ProductUserId = FAKE_PRODUCT_USER_ID;
        *Out = &s_fakeConnectIdToken;
    }
    return EOS_Success;
}

static void eos_Connect_CreateDeviceId(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_CreateDeviceId called");
    CB_Connect_CreateDeviceId info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
}

static void eos_Connect_CreateUser(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_CreateUser called");
    CB_Connect_CreateUser info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    QueueCallback(Cb, info);
}

static void eos_Connect_DeleteDeviceId(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_DeleteDeviceId called");
    CB_Connect_CreateDeviceId info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
}

static void eos_Connect_ExternalAccountInfo_Release(void* Info) {
    Log("EOS_Connect_ExternalAccountInfo_Release called. Info=%p", Info);
    FreeExternalAccountInfo((EOS_Connect_ExternalAccountInfo*)Info);
}

static void* eos_Connect_GetExternalAccountMapping(void* H, void* O) {
    const char* accountIdStr = FindStringPointer(O, 32);
    void* result = nullptr;
    if (accountIdStr) {
        result = GetOrCreateProductUserId(accountIdStr);
    } else {
        result = FAKE_PRODUCT_USER_ID;
    }
    Log("EOS_Connect_GetExternalAccountMapping: Query PUID for AccountId '%s' -> %p", accountIdStr ? accountIdStr : "null", result);
    return result;
}

static void* eos_Connect_GetLoggedInUserByIndex(void* H, int32_t Idx) {
    return (Idx == 0) ? FAKE_PRODUCT_USER_ID : nullptr;
}

static int32_t eos_Connect_GetLoggedInUsersCount(void* H) { return 1; }
static int32_t eos_Connect_GetLoginStatus(void* H, void* UserId) {
    Log("EOS_Connect_GetLoginStatus (UserId=%p) -> EOS_LS_LoggedIn", UserId);
    return EOS_LS_LoggedIn;
}

static uint32_t eos_Connect_GetProductUserExternalAccountCount(void* H, void* O) {
    Log("EOS_Connect_GetProductUserExternalAccountCount -> 2");
    return 2;
}

static EOS_EResult eos_Connect_GetProductUserIdMapping(void* H, void* O, char* OutBuffer, int32_t* InOutBufferLength) {
    Log("EOS_Connect_GetProductUserIdMapping called");
    if (!InOutBufferLength) return EOS_InvalidParameters;
    const char* puidStr = g_productUserIdStr;
    if (!puidStr || puidStr[0] == '\0') puidStr = "000102030405060708090a0b0c0d0e0f";
    int32_t requiredLen = (int32_t)strlen(puidStr) + 1;
    if (!OutBuffer || *InOutBufferLength < requiredLen) {
        *InOutBufferLength = requiredLen;
        return EOS_LimitExceeded;
    }
    strcpy_s(OutBuffer, *InOutBufferLength, puidStr);
    *InOutBufferLength = requiredLen - 1;
    return EOS_Success;
}
static void eos_Connect_IdToken_Release(void* T) { }

static void eos_Connect_LinkAccount(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_LinkAccount called");
    CB_Connect_Generic info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    QueueCallback(Cb, info);
}

// EOS_Connect_Credentials defined in eos_connect.h

// EOS_Connect_LoginOptions defined in eos_connect.h

static void eos_Connect_Login(void* H, void* O, void* C, void* Cb) {
    // Re-check Steam persona name (guaranteed to be available by now)
    RefreshUserName();
    
    auto* opts = (EOS_Connect_LoginOptions*)O;
    int32_t credType = -1;
    const char* credToken = "null";
    if (opts && opts->Credentials) {
        credType = opts->Credentials->Type;
        if (opts->Credentials->Token) credToken = opts->Credentials->Token;
    }
    Log("EOS_Connect_Login called. CredType=%d, Token=%s", credType, credToken);
    g_lastConnectLoginCredType = credType;
    
    CB_Connect_Login info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    info.ContinuanceToken = nullptr;
    QueueCallback(Cb, info);
}

static void eos_Connect_Logout(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_Logout called");
    CB_Connect_Generic info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    QueueCallback(Cb, info);
}

static void eos_Connect_QueryExternalAccountMappings(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_QueryExternalAccountMappings called");
    CB_Connect_QueryMappings info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    QueueCallback(Cb, info);
}

static void eos_Connect_QueryProductUserIdMappings(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_QueryProductUserIdMappings called");
    CB_Connect_QueryMappings info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    QueueCallback(Cb, info);
}

static void eos_Connect_RemoveNotifyAuthExpiration(void* H, EOS_NotificationId Id) { }
static void eos_Connect_RemoveNotifyLoginStatusChanged(void* H, EOS_NotificationId Id) { }

static void eos_Connect_TransferDeviceIdAccount(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_TransferDeviceIdAccount called");
    CB_Connect_Generic info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    QueueCallback(Cb, info);
}

static void eos_Connect_UnlinkAccount(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_UnlinkAccount called");
    CB_Connect_Generic info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    QueueCallback(Cb, info);
}

static void eos_Connect_VerifyIdToken(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_VerifyIdToken called");
    CB_Auth_VerifyIdToken info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
}

// Connect Copy functions
static EOS_EResult eos_Connect_CopyProductUserExternalAccountByAccountId(void* H, void* O, EOS_Connect_ExternalAccountInfo** Out) {
    const char* accountIdStr = FindStringPointer(O, 32);
    Log("EOS_Connect_CopyProductUserExternalAccountByAccountId called. Options=%p, AccountId=%s", O, accountIdStr ? accountIdStr : "null");
    if (!Out || !O) return 2;
    void* puid = nullptr;
    if (accountIdStr) {
        puid = GetOrCreateProductUserId(accountIdStr);
    } else {
        puid = FAKE_PRODUCT_USER_ID;
    }
    *Out = CreateExternalAccountInfo(puid);
    Log("  Returning %d, OutInfo=%p", (*Out) ? EOS_Success : EOS_LimitExceeded, *Out);
    return (*Out) ? EOS_Success : EOS_LimitExceeded;
}

static EOS_EResult eos_Connect_CopyProductUserExternalAccountByAccountType(void* H, void* O, EOS_Connect_ExternalAccountInfo** Out) {
    if (!Out || !O) return 2;
    auto* opts = (EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions*)O;
    Log("EOS_Connect_CopyProductUserExternalAccountByAccountType called. PUID=%p, ReqType=%d", opts->TargetUserId, opts->AccountIdType);
    
    void* puid = opts->TargetUserId;
    if (!puid) puid = FAKE_PRODUCT_USER_ID;
    
    if (opts->AccountIdType == 0) {
        *Out = CreateEpicExternalAccountInfo(puid);
    } else {
        *Out = CreateSteamExternalAccountInfo(puid);
    }
    Log("  Returning %d, OutInfo=%p", (*Out) ? EOS_Success : EOS_LimitExceeded, *Out);
    return (*Out) ? EOS_Success : EOS_LimitExceeded;
}

static EOS_EResult eos_Connect_CopyProductUserExternalAccountByIndex(void* H, void* O, EOS_Connect_ExternalAccountInfo** Out) {
    if (!Out || !O) return 2;
    auto* opts = (EOS_Connect_CopyProductUserExternalAccountByIndexOptions*)O;
    Log("EOS_Connect_CopyProductUserExternalAccountByIndex called. PUID=%p, Index=%d", opts->TargetUserId, opts->ExternalAccountInfoIndex);
    
    void* puid = opts->TargetUserId;
    if (!puid) puid = FAKE_PRODUCT_USER_ID;
    
    if (opts->ExternalAccountInfoIndex == 0) {
        *Out = CreateEpicExternalAccountInfo(puid);
    } else if (opts->ExternalAccountInfoIndex == 1) {
        *Out = CreateSteamExternalAccountInfo(puid);
    } else {
        *Out = nullptr;
    }
    Log("  Returning %d, OutInfo=%p", (*Out) ? EOS_Success : EOS_LimitExceeded, *Out);
    return (*Out) ? EOS_Success : EOS_LimitExceeded;
}

static EOS_EResult eos_Connect_CopyProductUserInfo(void* H, void* O, EOS_Connect_ExternalAccountInfo** Out) {
    void* puid = FindPuidPointer(O, 32);
    Log("EOS_Connect_CopyProductUserInfo called. Options=%p, PUID=%p", O, puid);
    if (!Out || !O) return 2;
    if (!puid) puid = FAKE_PRODUCT_USER_ID;
    
    // Always return Epic account info as the primary user info to satisfy OnlineSubsystem requirements
    *Out = CreateEpicExternalAccountInfo(puid);
    Log("  Returning %d, OutInfo=%p", (*Out) ? EOS_Success : EOS_LimitExceeded, *Out);
    return (*Out) ? EOS_Success : EOS_LimitExceeded;
}

// --- ID Validation & ToString ---
static int32_t eos_ProductUserId_IsValid(void* Id) { return (Id != nullptr) ? 1 : 0; }
static int32_t eos_EpicAccountId_IsValid(void* Id) { return (Id != nullptr) ? 1 : 0; }

static EOS_EResult eos_ProductUserId_ToString(void* Id, char* Buf, int32_t* Len) {
    if (!Buf || !Len) return 2;
    std::string extId = FindExternalId(Id);
    if (extId.empty()) extId = g_productUserIdStr;
    int32_t needed = (int32_t)extId.length() + 1;
    if (*Len < needed) { *Len = needed; return EOS_LimitExceeded; }
    strcpy_s(Buf, *Len, extId.c_str());
    *Len = needed;
    return EOS_Success;
}

static EOS_EResult eos_EpicAccountId_ToString(void* Id, char* Buf, int32_t* Len) {
    if (!Buf || !Len) return 2;
    std::string extId = FindExternalId(Id);
    if (extId.empty()) extId = g_epicAccountIdStr;
    int32_t needed = (int32_t)extId.length() + 1;
    if (*Len < needed) { *Len = needed; return EOS_LimitExceeded; }
    strcpy_s(Buf, *Len, extId.c_str());
    *Len = needed;
    return EOS_Success;
}

static EOS_ProductUserId eos_ProductUserId_FromString(const char* Str) {
    return GetOrCreateProductUserId(Str ? Str : "");
}

static EOS_EpicAccountId eos_EpicAccountId_FromString(const char* Str) {
    return GetOrCreateProductUserId(Str ? Str : "");
}

// --- Helpers ---
static const char* eos_EResult_ToString(EOS_EResult R) {
    switch (R) {
        case 0:  return "EOS_Success";
        case 14: return "EOS_NotFound";
        case 24: return "EOS_InvalidUser";
        default: return "EOS_UnknownResult";
    }
}

static int32_t eos_EResult_IsOperationComplete(EOS_EResult R) { return 1; }
static EOS_EResult eos_Logging_SetCallback(void* Cb) { return EOS_Success; }
static EOS_EResult eos_Logging_SetLogLevel(int32_t Cat, int32_t Lvl) { return EOS_Success; }

// --- UserInfo ---
struct EOS_UserInfo_QueryUserInfoOptions_v {
    int32_t ApiVersion;
    int32_t _pad0;
    void* LocalUserId;
    void* TargetUserId;
};

static void eos_UserInfo_QueryUserInfo(void* H, void* O, void* C, void* Cb) {
    Log("EOS_UserInfo_QueryUserInfo called");
    if (!O) return;
    void* targetUserId = ((EOS_UserInfo_QueryUserInfoOptions_v*)O)->TargetUserId;
    CB_UserInfo_Query info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = ((EOS_UserInfo_QueryUserInfoOptions_v*)O)->LocalUserId;
    info.TargetUserId = targetUserId;
    QueueCallback(Cb, info);
}

struct EOS_UserInfo {
    int32_t ApiVersion;
    EOS_EpicAccountId UserId;
    const char* DisplayName;
    const char* Country;
    const char* PreferredLanguage;
    const char* DisplayNameSanitized;
    const char* Nickname;
};

struct EOS_UserInfo_BestDisplayName {
    int32_t ApiVersion;
    EOS_EpicAccountId UserId;
    const char* DisplayName;
    const char* DisplayNameSanitized;
    const char* Nickname;
    int32_t PlatformType;
};

struct EOS_UserInfo_CopyUserInfoOptions {
    int32_t ApiVersion;
    int32_t _pad0;
    void* LocalUserId;
    void* TargetUserId;
};

static EOS_EResult eos_UserInfo_CopyUserInfo(void* H, void* O, EOS_UserInfo** Out) {
    Log("EOS_UserInfo_CopyUserInfo called");
    if (!Out || !O) return 2;
    
    void* targetUserId = ((EOS_UserInfo_CopyUserInfoOptions*)O)->TargetUserId;
    std::string extId = FindExternalId(targetUserId);
    if (extId.empty()) extId = g_productUserIdStr;
    std::string dispName = GetDisplayNameForExternalId(extId);
    
    auto* info = (EOS_UserInfo*)malloc(sizeof(EOS_UserInfo));
    if (!info) return EOS_LimitExceeded;
    
    info->ApiVersion = 1;
    info->UserId = targetUserId;
    info->DisplayName = _strdup(dispName.c_str());
    info->Country = _strdup("US");
    info->PreferredLanguage = _strdup("en");
    info->DisplayNameSanitized = _strdup(dispName.c_str());
    info->Nickname = _strdup(dispName.c_str());
    
    *Out = info;
    Log("  Returning EOS_Success, User=%p (Name=%s, SteamID=%s)", targetUserId, dispName.c_str(), extId.c_str());
    return EOS_Success;
}

struct EOS_UserInfo_CopyBestDisplayNameOptions {
    int32_t ApiVersion;
    int32_t _pad0;
    void* LocalUserId;
    void* TargetUserId;
};

static EOS_EResult eos_UserInfo_CopyBestDisplayName(void* H, void* O, EOS_UserInfo_BestDisplayName** Out) {
    Log("EOS_UserInfo_CopyBestDisplayName called");
    if (!Out || !O) return 2;
    
    void* targetUserId = ((EOS_UserInfo_CopyBestDisplayNameOptions*)O)->TargetUserId;
    std::string extId = FindExternalId(targetUserId);
    if (extId.empty()) extId = g_productUserIdStr;
    std::string dispName = GetDisplayNameForExternalId(extId);
    
    auto* info = (EOS_UserInfo_BestDisplayName*)malloc(sizeof(EOS_UserInfo_BestDisplayName));
    if (!info) return EOS_LimitExceeded;
    
    info->ApiVersion = 1;
    info->UserId = targetUserId;
    info->DisplayName = _strdup(dispName.c_str());
    info->DisplayNameSanitized = _strdup(dispName.c_str());
    info->Nickname = _strdup(dispName.c_str());
    info->PlatformType = (g_lastConnectLoginCredType == 18) ? 0 : 1;
    
    *Out = info;
    Log("  Returning EOS_Success, BestNameUser=%p (Name=%s, SteamID=%s)", targetUserId, dispName.c_str(), extId.c_str());
    return EOS_Success;
}

static void eos_UserInfo_Release(void* I) {
    Log("EOS_UserInfo_Release called. Info=%p", I);
    auto* info = (EOS_UserInfo*)I;
    if (info) {
        if (info->DisplayName) free((void*)info->DisplayName);
        if (info->Country) free((void*)info->Country);
        if (info->PreferredLanguage) free((void*)info->PreferredLanguage);
        if (info->DisplayNameSanitized) free((void*)info->DisplayNameSanitized);
        if (info->Nickname) free((void*)info->Nickname);
        free(info);
    }
}

static void eos_UserInfo_BestDisplayName_Release(void* I) {
    Log("EOS_UserInfo_BestDisplayName_Release called. Info=%p", I);
    auto* info = (EOS_UserInfo_BestDisplayName*)I;
    if (info) {
        if (info->DisplayName) free((void*)info->DisplayName);
        if (info->DisplayNameSanitized) free((void*)info->DisplayNameSanitized);
        if (info->Nickname) free((void*)info->Nickname);
        free(info);
    }
}

// =============================================================================
// PLAYER DATA STORAGE
// =============================================================================
struct CB_PlayerDataStorage_QueryFileList {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
    uint32_t    FileCount;
};

static void eos_PlayerDataStorage_QueryFileList(void* H, void* O, void* C, void* Cb) {
    Log("EOS_PlayerDataStorage_QueryFileList called");
    void* puid = FindPuidPointer(O, 32);
    if (!puid) puid = FAKE_PRODUCT_USER_ID;
    
    CB_PlayerDataStorage_QueryFileList info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = puid;
    info.FileCount = 0;
    QueueCallback(Cb, info);
}

static uint32_t eos_PlayerDataStorage_GetFileMetadataCount(void* H, void* O) {
    Log("EOS_PlayerDataStorage_GetFileMetadataCount -> 0");
    return 0;
}

static EOS_EResult eos_PlayerDataStorage_CopyFileMetadataAtIndex(void* H, void* O, void** Out) {
    Log("EOS_PlayerDataStorage_CopyFileMetadataAtIndex -> NotFound");
    return EOS_NotFound;
}

static EOS_EResult eos_PlayerDataStorage_CopyFileMetadataByFilename(void* H, void* O, void** Out) {
    Log("EOS_PlayerDataStorage_CopyFileMetadataByFilename -> NotFound");
    return EOS_NotFound;
}

static void eos_PlayerDataStorage_FileMetadata_Release(void* Info) {
    Log("EOS_PlayerDataStorage_FileMetadata_Release called");
}

struct EOS_PlayerDataStorage_ReadFileOptions {
    int32_t ApiVersion;
    void* LocalUserId;
    const char* Filename;
    uint32_t ReadChunkLengthBytes;
    void* ReadFileDataCallback;
    void* FileTransferProgressCallback;
};

struct CB_PlayerDataStorage_ReadFile {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
    const char* Filename;
};

static void* eos_PlayerDataStorage_ReadFile(void* H, const EOS_PlayerDataStorage_ReadFileOptions* O, void* C, void* Cb) {
    Log("EOS_PlayerDataStorage_ReadFile called. Filename=%s", O ? O->Filename : "nullptr");
    
    void* puid = O ? O->LocalUserId : nullptr;
    if (!puid) puid = FAKE_PRODUCT_USER_ID;
    
    CB_PlayerDataStorage_ReadFile info = {};
    info.ResultCode = EOS_NotFound; // 14
    info.ClientData = C;
    info.LocalUserId = puid;
    info.Filename = O ? O->Filename : "";
    
    QueueCallback(Cb, info);
    
    return (void*)0x22222222;
}

struct EOS_PlayerDataStorage_WriteFileOptions {
    int32_t ApiVersion;
    void* LocalUserId;
    const char* Filename;
    uint32_t ChunkLengthBytes;
    void* WriteFileDataCallback;
    void* FileTransferProgressCallback;
};

struct CB_PlayerDataStorage_WriteFile {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
    const char* Filename;
};

static void* eos_PlayerDataStorage_WriteFile(void* H, const EOS_PlayerDataStorage_WriteFileOptions* O, void* C, void* Cb) {
    Log("EOS_PlayerDataStorage_WriteFile called. Filename=%s", O ? O->Filename : "nullptr");
    
    void* puid = O ? O->LocalUserId : nullptr;
    if (!puid) puid = FAKE_PRODUCT_USER_ID;
    
    CB_PlayerDataStorage_WriteFile info = {};
    info.ResultCode = EOS_Success; // 0
    info.ClientData = C;
    info.LocalUserId = puid;
    info.Filename = O ? O->Filename : "";
    
    QueueCallback(Cb, info);
    
    return (void*)0x33333333;
}

struct EOS_PlayerDataStorage_DuplicateFileOptions {
    int32_t ApiVersion;
    void* LocalUserId;
    const char* SourceFilename;
    const char* DestinationFilename;
};

struct CB_PlayerDataStorage_DuplicateFile {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
};

static void eos_PlayerDataStorage_DuplicateFile(void* H, const EOS_PlayerDataStorage_DuplicateFileOptions* O, void* C, void* Cb) {
    Log("EOS_PlayerDataStorage_DuplicateFile called. Source=%s, Dest=%s", 
        O ? O->SourceFilename : "nullptr", O ? O->DestinationFilename : "nullptr");
        
    void* puid = O ? O->LocalUserId : nullptr;
    if (!puid) puid = FAKE_PRODUCT_USER_ID;
    
    if (O && O->SourceFilename && O->DestinationFilename) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string dirPath(exePath);
        size_t pos = dirPath.find_last_of("\\/");
        if (pos != std::string::npos) dirPath = dirPath.substr(0, pos + 1) + "ReFix_Saves\\";
        
        std::string srcName(O->SourceFilename);
        for (char& c : srcName) { if (c == '/' || c == '\\') c = '_'; }
        std::string dstName(O->DestinationFilename);
        for (char& c : dstName) { if (c == '/' || c == '\\') c = '_'; }
        
        std::string srcPath = dirPath + srcName;
        std::string dstPath = dirPath + dstName;
        
        Log("  Duplicating file locally from %s to %s", srcPath.c_str(), dstPath.c_str());
        CopyFileA(srcPath.c_str(), dstPath.c_str(), FALSE);
    }
    
    CB_PlayerDataStorage_DuplicateFile info = {};
    info.ResultCode = EOS_Success; // 0
    info.ClientData = C;
    info.LocalUserId = puid;
    
    QueueCallback(Cb, info);
}

struct EOS_PlayerDataStorage_DeleteFileOptions {
    int32_t ApiVersion;
    void* LocalUserId;
    const char* Filename;
};

struct CB_PlayerDataStorage_DeleteFile {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
    const char* Filename;
};

static void eos_PlayerDataStorage_DeleteFile(void* H, const EOS_PlayerDataStorage_DeleteFileOptions* O, void* C, void* Cb) {
    Log("EOS_PlayerDataStorage_DeleteFile called. Filename=%s", O ? O->Filename : "nullptr");
    
    void* puid = O ? O->LocalUserId : nullptr;
    if (!puid) puid = FAKE_PRODUCT_USER_ID;
    
    if (O && O->Filename) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string dirPath(exePath);
        size_t pos = dirPath.find_last_of("\\/");
        if (pos != std::string::npos) dirPath = dirPath.substr(0, pos + 1) + "ReFix_Saves\\";
        
        std::string name(O->Filename);
        for (char& c : name) { if (c == '/' || c == '\\') c = '_'; }
        std::string path = dirPath + name;
        
        Log("  Deleting file locally: %s", path.c_str());
        DeleteFileA(path.c_str());
    }
    
    CB_PlayerDataStorage_DeleteFile info = {};
    info.ResultCode = EOS_Success; // 0
    info.ClientData = C;
    info.LocalUserId = puid;
    info.Filename = O ? O->Filename : "";
    
    QueueCallback(Cb, info);
}

// =============================================================================
// LOBBY
// =============================================================================
struct CB_Lobby_CreateLobby {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    const char* LobbyId;
};

// Search Parameter Capturing & Dynamic Lying for Matchmaking
union EOS_Lobby_AttributeValue {
    int64_t AsInt64;
    double AsDouble;
    int32_t bAsBool;
    const char* AsUtf8;
};

struct EOS_Lobby_AttributeData {
    int32_t ApiVersion;
    const char* Key;
    EOS_Lobby_AttributeValue Value;
    int32_t ValueType; // 0=Bool, 1=Int64, 2=Double, 3=String (EOS_AT_STRING)
};

static std::vector<EOS_Lobby_AttributeData> g_capturedSearchParameters;

static void FreeCapturedSearchParameters() {
    for (auto& param : g_capturedSearchParameters) {
        if (param.Key) free((void*)param.Key);
        if ((param.ValueType == 3 || param.ValueType == 4) && param.Value.AsUtf8) { // String type
            free((void*)param.Value.AsUtf8);
        }
    }
    g_capturedSearchParameters.clear();
}

static void CaptureSearchParameter(const EOS_Lobby_AttributeData* data) {
    if (!data || !data->Key) return;
    
    for (auto& param : g_capturedSearchParameters) {
        if (strcmp(param.Key, data->Key) == 0) {
            if ((param.ValueType == 3 || param.ValueType == 4) && param.Value.AsUtf8) free((void*)param.Value.AsUtf8);
            param.ValueType = data->ValueType;
            param.Value = data->Value;
            if ((data->ValueType == 3 || data->ValueType == 4) && data->Value.AsUtf8) {
                param.Value.AsUtf8 = _strdup(data->Value.AsUtf8);
            }
            return;
        }
    }
    
    EOS_Lobby_AttributeData clone = *data;
    clone.Key = _strdup(data->Key);
    if ((data->ValueType == 3 || data->ValueType == 4) && data->Value.AsUtf8) {
        clone.Value.AsUtf8 = _strdup(data->Value.AsUtf8);
    }
    g_capturedSearchParameters.push_back(clone);
}

struct EOS_LobbySearch_SetParameterOptions {
    int32_t ApiVersion;
    const EOS_Lobby_AttributeData* Parameter;
    int32_t ComparisonOp;
};

static EOS_EResult eos_LobbySearch_SetParameter(void* H, void* O) {
    Log("EOS_LobbySearch_SetParameter called");
    if (O) {
        auto* opts = (EOS_LobbySearch_SetParameterOptions*)O;
        CaptureSearchParameter(opts->Parameter);
        if (opts->Parameter && opts->Parameter->Key) {
            Log("  Captured parameter key: %s (Type=%d)", opts->Parameter->Key, opts->Parameter->ValueType);
        }
    }
    return EOS_Success;
}

static void eos_LobbySearch_Release(void* H) {
    Log("EOS_LobbySearch_Release called");
    FreeCapturedSearchParameters();
}

static std::vector<std::string> g_foundLobbies;

static void CreateAndTagRealSteamLobbyAsync() {
    static std::atomic<bool> s_creatingLobby{false};
    bool expected = false;
    if (!s_creatingLobby.compare_exchange_strong(expected, true)) return;
    if (g_currentSteamLobbyId.load() != 0) { s_creatingLobby.store(false); return; }

    std::thread([]() {
        void* mm = GetSteamMatchmakingInterface();
        if (!mm || !g_pfn_MM_CreateLobby) {
            Log("CreateAndTagRealSteamLobbyAsync: Steam Matchmaking interface not available");
            s_creatingLobby.store(false);
            return;
        }

        Log("CreateAndTagRealSteamLobbyAsync: Creating real Valve Steam lobby in background...");
        uint64_t hAPICall = g_pfn_MM_CreateLobby(mm, 2 /*k_ELobbyTypePublic*/, 4);
        Log("  CreateLobby SteamAPICall_t handle: %llu", hAPICall);

        // --- Capture the lobby ID via LobbyCreated_t callback ---
        // The Steamworks SDK fires LobbyCreated_t (iCallback=513) when CreateLobby completes.
        // Since we're in an emulated environment (Goldberg/GSE), the callback is dispatched
        // synchronously during RunCallbacks(). We use a CCallResult-style struct to poll.

        // LobbyCreated_t struct layout:
        //   EResult m_eResult;      // offset 0, 4 bytes
        //   uint64  m_ulSteamIDLobby; // offset 8, 8 bytes (aligned)
        struct LobbyCreated_t {
            int32_t m_eResult;
            int32_t _pad;
            uint64_t m_ulSteamIDLobby;
        };

        // Also try using RegisterCallResult if available
        // This is a lightweight CCallResult-compatible struct for Goldberg GSE
        struct CallbackPoll {
            void* vtable[4];    // CCallbackBase vtable (dummy)
            uint8_t flags;
            int32_t iCallback;
            LobbyCreated_t result;
            bool fired;
        };

        uint64_t steamLobbyId = 0;

        // Strategy: pump RunCallbacks and check if our lobby is now available.
        // Goldberg emulator processes CreateLobby synchronously in most cases,
        // so the lobby should appear in the internal state after a few RunCallbacks calls.
        if (g_pfn_SteamRunCallbacks) {
            // First approach: run callbacks to process the creation
            for (int attempt = 0; attempt < 50; attempt++) {
                Sleep(50);
                g_pfn_SteamRunCallbacks();

                // After CreateLobby completes, the lobby appears in the matchmaking list.
                // We need to do a fresh RequestLobbyList to find it.
                // But first, check if Goldberg already set our lobby ID via the callback
                // by checking g_currentSteamLobbyId (which may be set by other code paths).
                if (g_currentSteamLobbyId.load() != 0) {
                    steamLobbyId = g_currentSteamLobbyId.load();
                    Log("  Lobby ID captured from external notification: %llu", steamLobbyId);
                    break;
                }
            }

            // If we still don't have a lobby ID, do a lobby list request to find our own lobby
            if (steamLobbyId == 0 && g_pfn_MM_RequestLobbyList && g_pfn_MM_GetLobbyByIndex) {
                Log("  Lobby ID not captured via callback, searching via RequestLobbyList...");
                
                // Request ALL lobbies with our game filter
                if (g_pfn_MM_AddStringFilter) {
                    g_pfn_MM_AddStringFilter(mm, "game_filter", g_gameFilter, 0 /*k_ELobbyComparisonEqual*/);
                }
                g_pfn_MM_RequestLobbyList(mm);
                
                // Wait for results
                for (int i = 0; i < 30; i++) {
                    Sleep(30);
                    g_pfn_SteamRunCallbacks();
                    uint64_t id = g_pfn_MM_GetLobbyByIndex(mm, 0);
                    if (id != 0) {
                        steamLobbyId = id;
                        Log("  Found lobby via RequestLobbyList fallback: %llu", steamLobbyId);
                        break;
                    }
                }
            }

            // Last resort: if Goldberg assigned the lobby synchronously, 
            // try requesting with no filter and take the first result
            if (steamLobbyId == 0 && g_pfn_MM_RequestLobbyList && g_pfn_MM_GetLobbyByIndex) {
                Log("  Last resort: requesting unfiltered lobby list...");
                g_pfn_MM_RequestLobbyList(mm);
                for (int i = 0; i < 20; i++) {
                    Sleep(50);
                    g_pfn_SteamRunCallbacks();
                    uint64_t id = g_pfn_MM_GetLobbyByIndex(mm, 0);
                    if (id != 0) {
                        steamLobbyId = id;
                        Log("  Found lobby via unfiltered list: %llu", steamLobbyId);
                        break;
                    }
                }
            }
        }

        char pubIP[64] = "127.0.0.1";
        GetBestLocalIP(pubIP, sizeof(pubIP));

        if (steamLobbyId == 0) {
            char envSteamId[32] = "";
            GetEnvironmentVariableA("REFIX_STEAM_ID", envSteamId, sizeof(envSteamId));
            uint64_t userSteamID = envSteamId[0] != '\0' ? _strtoui64(envSteamId, nullptr, 10) : 76561198362393833ULL;
            steamLobbyId = 0x0110000100000000ULL | (userSteamID & 0xFFFFFFFFULL);
            Log("WARNING: CreateAndTagRealSteamLobbyAsync: Steam API returned 0, assigned Synthetic Lobby ID=%llu", steamLobbyId);
        } else {
            Log("CreateAndTagRealSteamLobbyAsync: Steam Lobby ID=%llu (HostIP=%s:7777)", steamLobbyId, pubIP);
        }

        g_currentSteamLobbyId.store(steamLobbyId);

        if (g_pfn_MM_SetLobbyData) {
            g_pfn_MM_SetLobbyData(mm, steamLobbyId, "game_filter", g_gameFilter);
            g_pfn_MM_SetLobbyData(mm, steamLobbyId, "HOST_IP", pubIP);
            g_pfn_MM_SetLobbyData(mm, steamLobbyId, "SERVER_IP", pubIP);
            g_pfn_MM_SetLobbyData(mm, steamLobbyId, "HOST_PORT", "7777");
            g_pfn_MM_SetLobbyData(mm, steamLobbyId, "SERVER_PORT", "7777");
            g_pfn_MM_SetLobbyData(mm, steamLobbyId, "eos_lobby_id", g_productUserIdStr);
            g_pfn_MM_SetLobbyData(mm, steamLobbyId, "session_name", g_activeSessionName.c_str());

            if (g_pfn_MM_SetLobbyType) g_pfn_MM_SetLobbyType(mm, steamLobbyId, 2 /*k_ELobbyTypePublic*/);
            if (g_pfn_MM_SetLobbyJoinable) g_pfn_MM_SetLobbyJoinable(mm, steamLobbyId, true);

            Log("  Successfully tagged Steam lobby %llu with game_filter=%s, HOST_IP=%s:7777",
                steamLobbyId, g_gameFilter, pubIP);
        }

        // Notify P2P hook so Winsock sendto is redirected through Steam relay
        ReFix_NotifyLobbyID(steamLobbyId);

        void* steamFriends = GetSteamFriendsInterface();
        if (steamFriends && g_pfn_SetRichPresence) {
            char connectStr[128];
            if (steamLobbyId != 0) {
                sprintf_s(connectStr, sizeof(connectStr), "+connect_lobby %llu", steamLobbyId);
            } else {
                sprintf_s(connectStr, sizeof(connectStr), "+connect %s:7777", pubIP);
            }
            g_pfn_SetRichPresence(steamFriends, "connect", connectStr);
            Log("  Set Steam Rich Presence connect string: '%s'", connectStr);
        }

        s_creatingLobby.store(false);
    }).detach();
}

static void eos_Lobby_CreateLobby(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_CreateLobby called");
    CreateAndTagRealSteamLobbyAsync();
    
    CB_Lobby_CreateLobby info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LobbyId = _strdup(g_productUserIdStr);
    QueueCallback(Cb, info);
}

struct CB_Lobby_DestroyLobby {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    const char* LobbyId;
};

static void eos_Lobby_DestroyLobby(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_DestroyLobby called");
    
    // Leave the real Steam lobby if we have one
    if (g_currentSteamLobbyId.load() != 0) {
        void* mm = GetSteamMatchmakingInterface();
        if (mm && g_pfn_MM_LeaveLobby) {
            Log("  Leaving Steam lobby %llu", g_currentSteamLobbyId.load());
            g_pfn_MM_LeaveLobby(mm, g_currentSteamLobbyId.load());
        }
        g_currentSteamLobbyId.store(0);
    }
    
    CB_Lobby_DestroyLobby info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LobbyId = _strdup(g_productUserIdStr);
    QueueCallback(Cb, info);
}

struct CB_Lobby_UpdateLobby {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    const char* LobbyId;
};

static void eos_Lobby_UpdateLobby(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_UpdateLobby called");
    CB_Lobby_UpdateLobby info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LobbyId = _strdup(g_productUserIdStr);
    QueueCallback(Cb, info);
}

struct CB_LobbySearch_Find {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};

static void eos_LobbySearch_Find(void* H, void* O, void* C, void* Cb) {
    Log("EOS_LobbySearch_Find called (Async non-blocking)");
    g_foundLobbies.clear();
    g_steamLobbies.clear();
    
    std::thread([C, Cb]() {
        void* mm = GetSteamMatchmakingInterface();
        if (mm && g_pfn_MM_RequestLobbyList && g_pfn_MM_AddStringFilter && g_pfn_MM_GetLobbyByIndex) {
            Log("  [Background Thread] Using REAL Steam Matchmaking for lobby search...");
            
            // Add filter: only show lobbies for our game
            g_pfn_MM_AddStringFilter(mm, "game_filter", g_gameFilter, 0 /*k_ELobbyComparisonEqual*/);
            
            // Limit results
            if (g_pfn_MM_AddResultCountFilter) {
                g_pfn_MM_AddResultCountFilter(mm, 20);
            }
            
            // Request the lobby list (async)
            uint64_t hCall = g_pfn_MM_RequestLobbyList(mm);
            Log("  [Background Thread] Steam RequestLobbyList APICall: %llu", hCall);
            
            // Non-blocking poll in background thread so main thread NEVER freezes
            if (g_pfn_SteamRunCallbacks) {
                for (int i = 0; i < 15; i++) {
                    Sleep(20);
                    g_pfn_SteamRunCallbacks();
                    if (g_pfn_MM_GetLobbyByIndex(mm, 0) != 0) {
                        break; // Stop waiting as soon as results arrive!
                    }
                }
            }
            
            // Now try to enumerate the results
            for (int i = 0; i < 20; i++) {
                uint64_t lobbyId = g_pfn_MM_GetLobbyByIndex(mm, i);
                if (lobbyId == 0) break;
                
                // Skip our own lobby
                if (lobbyId == g_currentSteamLobbyId.load()) {
                    Log("    [Background Thread] Skipping our own lobby %llu", lobbyId);
                    continue;
                }
                
                // Read lobby metadata
                SteamLobbyInfo lobbyInfo = {};
                lobbyInfo.steamLobbyId = lobbyId;
                lobbyInfo.hostIP[0] = '\0';  // Don't pre-fill with local IP!
                strcpy_s(lobbyInfo.hostPort, "7777");
                
                if (g_pfn_MM_GetLobbyData) {
                    const char* hostIP = g_pfn_MM_GetLobbyData(mm, lobbyId, "HOST_IP");
                    // Fallback to SERVER_IP if HOST_IP is not set
                    if (!hostIP || hostIP[0] == '\0') {
                        hostIP = g_pfn_MM_GetLobbyData(mm, lobbyId, "SERVER_IP");
                    }
                    const char* hostPort = g_pfn_MM_GetLobbyData(mm, lobbyId, "HOST_PORT");
                    if (!hostPort || hostPort[0] == '\0') {
                        hostPort = g_pfn_MM_GetLobbyData(mm, lobbyId, "SERVER_PORT");
                    }
                    const char* eosId = g_pfn_MM_GetLobbyData(mm, lobbyId, "eos_lobby_id");
                    
                    if (hostIP && hostIP[0] != '\0') strncpy_s(lobbyInfo.hostIP, hostIP, _TRUNCATE);
                    if (hostPort && hostPort[0] != '\0') strncpy_s(lobbyInfo.hostPort, hostPort, _TRUNCATE);
                    if (eosId && eosId[0] != '\0') strncpy_s(lobbyInfo.eosLobbyId, eosId, _TRUNCATE);
                    else sprintf_s(lobbyInfo.eosLobbyId, "%llu", lobbyId);
                }
                
                g_steamLobbies.push_back(lobbyInfo);
                
                char idStr[32];
                sprintf_s(idStr, sizeof(idStr), "%llu", lobbyId);
                g_foundLobbies.push_back(idStr);
                Log("    [Background Thread] Found Steam lobby %d: ID=%llu, HOST_IP=%s:%s",
                    i, lobbyId, lobbyInfo.hostIP, lobbyInfo.hostPort);
            }

            // ALSO query online Steam friends for active lobbies in Rich Presence
            void* steamFriends = GetSteamFriendsInterface();
            if (steamFriends && g_pfn_GetFriendCount && g_pfn_GetFriendByIndex && g_pfn_GetFriendRichPresence) {
                int friendCount = g_pfn_GetFriendCount(steamFriends, 0x04 /*k_EFriendFlagImmediate*/);
                Log("  [Background Thread] Checking %d friends for active game lobbies...", friendCount);
                for (int i = 0; i < friendCount; i++) {
                    uint64_t friendId = g_pfn_GetFriendByIndex(steamFriends, i, 0x04);
                    if (!friendId || friendId == g_currentSteamLobbyId.load()) continue;

                    const char* connectStr = g_pfn_GetFriendRichPresence(steamFriends, friendId, "connect");
                    if (connectStr && connectStr[0] != '\0') {
                        Log("    Friend %llu Rich Presence connect string: '%s'", friendId, connectStr);
                        uint64_t friendLobbyId = 0;
                        if (sscanf_s(connectStr, "+connect_lobby %llu", &friendLobbyId) == 1 && friendLobbyId != 0) {
                            bool exists = false;
                            for (const auto& existing : g_steamLobbies) {
                                if (existing.steamLobbyId == friendLobbyId) { exists = true; break; }
                            }
                            if (!exists) {
                                SteamLobbyInfo friendLobby = {};
                                friendLobby.steamLobbyId = friendLobbyId;
                                sprintf_s(friendLobby.eosLobbyId, "%llu", friendLobbyId);
                                strcpy_s(friendLobby.hostPort, "7777");

                                if (g_pfn_MM_GetLobbyData && mm) {
                                    const char* hostIP = g_pfn_MM_GetLobbyData(mm, friendLobbyId, "HOST_IP");
                                    if (!hostIP || hostIP[0] == '\0') hostIP = g_pfn_MM_GetLobbyData(mm, friendLobbyId, "SERVER_IP");
                                    if (hostIP && hostIP[0] != '\0') strncpy_s(friendLobby.hostIP, hostIP, _TRUNCATE);
                                }

                                g_steamLobbies.push_back(friendLobby);
                                char idStr[32];
                                sprintf_s(idStr, sizeof(idStr), "%llu", friendLobbyId);
                                g_foundLobbies.push_back(idStr);
                                Log("    [Background Thread] Added friend lobby via Rich Presence: ID=%llu, HOST_IP=%s",
                                    friendLobbyId, friendLobby.hostIP);
                            }
                        }
                    }
                }
            }
            
            Log("  [Background Thread] Steam lobby search completed: %d lobbies found", (int)g_steamLobbies.size());
        } else {
            Log("  [Background Thread] Steam Matchmaking not available yet for lobby search");
        }
        
        CB_LobbySearch_Find info = {};
        info.ResultCode = EOS_Success;
        info.ClientData = C;
        QueueCallback(Cb, info);
    }).detach();
}

static uint32_t eos_LobbySearch_GetSearchResultCount(void* H, void* O) {
    Log("EOS_LobbySearch_GetSearchResultCount called -> %d", (int)g_foundLobbies.size());
    return (uint32_t)g_foundLobbies.size();
}

struct EOS_LobbySearch_CopySearchResultByIndexOptions {
    int32_t ApiVersion;
    uint32_t LobbyIndex;
};

static EOS_EResult eos_LobbySearch_CopySearchResultByIndex(void* H, void* O, void** OutLobbyDetailsHandle) {
    Log("EOS_LobbySearch_CopySearchResultByIndex called");
    if (!O || !OutLobbyDetailsHandle) return EOS_NotFound;
    uint32_t index = ((EOS_LobbySearch_CopySearchResultByIndexOptions*)O)->LobbyIndex;
    if (index >= g_foundLobbies.size()) return EOS_NotFound;
    
    *OutLobbyDetailsHandle = (void*)(uintptr_t)(0x1000 + index);
    Log("  Returning handle %p for lobby index %d", *OutLobbyDetailsHandle, index);
    return EOS_Success;
}

struct CB_Lobby_JoinLobby {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    const char* LobbyId;
};

static void eos_Lobby_JoinLobby(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_JoinLobby called");
    
    // O is EOS_Lobby_JoinLobbyOptions
    // In SDK, the option has EOS_HLobbyDetails LobbyDetailsHandle at offset 16 (on 64-bit)
    struct EOS_Lobby_JoinLobbyOptions {
        int32_t ApiVersion;
        int32_t _pad0;
        void* LocalUserId;
        void* LobbyDetailsHandle;
    };
    
    void* detailsHandle = nullptr;
    if (O) detailsHandle = ((EOS_Lobby_JoinLobbyOptions*)O)->LobbyDetailsHandle;
    
    std::string lobbyId = "refix_fake_lobby";
    uintptr_t val = (uintptr_t)detailsHandle;
    if (val >= 0x1000 && val < 0x1000 + g_foundLobbies.size()) {
        lobbyId = g_foundLobbies[val - 0x1000];
        
        // Also join the real Steam lobby if available
        uint32_t lobbyIdx = (uint32_t)(val - 0x1000);
        if (lobbyIdx < g_steamLobbies.size()) {
            uint64_t steamLobbyId = g_steamLobbies[lobbyIdx].steamLobbyId;
            Log("  Joining REAL Steam lobby %llu asynchronously", steamLobbyId);
            std::thread([steamLobbyId]() {
                void* mm = GetSteamMatchmakingInterface();
                if (mm && g_pfn_MM_JoinLobby) {
                    g_pfn_MM_JoinLobby(mm, steamLobbyId);
                    if (g_pfn_SteamRunCallbacks) g_pfn_SteamRunCallbacks();
                }
            }).detach();
        }
    }
    
    CB_Lobby_JoinLobby info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LobbyId = _strdup(lobbyId.c_str());
    Log("  Joining lobby %s", lobbyId.c_str());
    QueueCallback(Cb, info);
}

static EOS_EResult eos_Lobby_CreateLobbySearch(void* H, void* O, void** OutLobbySearchHandle) {
    Log("EOS_Lobby_CreateLobbySearch called");
    if (OutLobbySearchHandle) {
        *OutLobbySearchHandle = HANDLE_LOBBY_SEARCH;
    }
    return EOS_Success;
}

static EOS_EResult eos_Lobby_UpdateLobbyModification(void* H, void* O, void** OutLobbyModificationHandle) {
    Log("EOS_Lobby_UpdateLobbyModification called");
    if (OutLobbyModificationHandle) {
        *OutLobbyModificationHandle = HANDLE_LOBBY_MODIFICATION;
    }
    return EOS_Success;
}

static EOS_EResult eos_Lobby_CreateLobbyModification(void* H, void* O, void** OutLobbyModificationHandle) {
    Log("EOS_Lobby_CreateLobbyModification called");
    if (OutLobbyModificationHandle) {
        *OutLobbyModificationHandle = HANDLE_LOBBY_MODIFICATION;
    }
    return EOS_Success;
}

static EOS_EResult eos_LobbyModification_SetPermissionLevel(void* H, void* O) {
    Log("EOS_LobbyModification_SetPermissionLevel called");
    return EOS_Success;
}

static EOS_EResult eos_LobbyModification_SetMaxMembers(void* H, void* O) {
    Log("EOS_LobbyModification_SetMaxMembers called");
    return EOS_Success;
}

static EOS_EResult eos_LobbyModification_SetBucketId(void* H, void* O) {
    Log("EOS_LobbyModification_SetBucketId called");
    return EOS_Success;
}

static EOS_EResult eos_LobbyModification_SetInvitesAllowed(void* H, void* O) {
    Log("EOS_LobbyModification_SetInvitesAllowed called");
    return EOS_Success;
}

static EOS_EResult eos_LobbyModification_AddAttribute(void* H, void* O) {
    Log("EOS_LobbyModification_AddAttribute called");
    return EOS_Success;
}

static EOS_EResult eos_LobbyModification_AddMemberAttribute(void* H, void* O) {
    Log("EOS_LobbyModification_AddMemberAttribute called");
    return EOS_Success;
}

static EOS_EResult eos_LobbyModification_RemoveAttribute(void* H, void* O) {
    Log("EOS_LobbyModification_RemoveAttribute called");
    return EOS_Success;
}

static EOS_EResult eos_LobbyModification_RemoveMemberAttribute(void* H, void* O) {
    Log("EOS_LobbyModification_RemoveMemberAttribute called");
    return EOS_Success;
}

static void eos_LobbyModification_Release(void* H) {
    Log("EOS_LobbyModification_Release called");
}

static EOS_EResult eos_Lobby_CopyLobbyDetailsHandle(void* H, void* O, void** OutLobbyDetailsHandle) {
    Log("EOS_Lobby_CopyLobbyDetailsHandle called");
    if (OutLobbyDetailsHandle) {
        *OutLobbyDetailsHandle = (void*)(uintptr_t)0x1000;
    }
    return EOS_Success;
}

static EOS_EResult eos_Lobby_CopyLobbyDetailsHandleByInviteId(void* H, void* O, void** OutLobbyDetailsHandle) {
    Log("EOS_Lobby_CopyLobbyDetailsHandleByInviteId called");
    if (OutLobbyDetailsHandle) {
        *OutLobbyDetailsHandle = (void*)(uintptr_t)0x1000;
    }
    return EOS_Success;
}

static EOS_EResult eos_Lobby_CopyLobbyDetailsHandleByUiEventId(void* H, void* O, void** OutLobbyDetailsHandle) {
    Log("EOS_Lobby_CopyLobbyDetailsHandleByUiEventId called");
    if (OutLobbyDetailsHandle) {
        *OutLobbyDetailsHandle = (void*)(uintptr_t)0x1000;
    }
    return EOS_Success;
}
struct EOS_LobbyDetails_Info {
    int32_t ApiVersion;
    const char* LobbyId;
    void* LobbyOwnerUserId;
    int32_t PermissionLevel;
    uint32_t AvailableSlots;
    uint32_t MaxMembers;
    int32_t bAllowInvites;
    const char* BucketId;
    int32_t bAllowHostMigration;
    int32_t bRTCRoomEnabled;
    int32_t bAllowJoinById;
    int32_t bRejoinAfterKickRequiresInvite;
    int32_t bPresenceEnabled;
    const uint32_t* AllowedPlatformIds;
    uint32_t AllowedPlatformIdsCount;
};

static EOS_EResult eos_LobbyDetails_CopyInfo(void* H, void* O, EOS_LobbyDetails_Info** OutLobbyDetailsInfo) {
    Log("EOS_LobbyDetails_CopyInfo called for handle %p", H);
    if (!OutLobbyDetailsInfo) return EOS_LimitExceeded;
    
    std::string lobbyId = "refix_fake_lobby";
    void* lobbyOwner = FAKE_PRODUCT_USER_ID;
    
    uintptr_t val = (uintptr_t)H;
    if (val >= 0x1000 && val < 0x1000 + g_foundLobbies.size()) {
        uint32_t idx = (uint32_t)(val - 0x1000);
        lobbyId = g_foundLobbies[idx];
        lobbyOwner = GetOrCreateProductUserId(lobbyId);
    } else {
        // Fallback or local user's lobby details
        lobbyId = g_productUserIdStr;
        lobbyOwner = FAKE_PRODUCT_USER_ID;
    }
    
    EOS_LobbyDetails_Info* info = (EOS_LobbyDetails_Info*)malloc(sizeof(EOS_LobbyDetails_Info));
    if (!info) return EOS_LimitExceeded;
    
    memset(info, 0, sizeof(EOS_LobbyDetails_Info));
    info->ApiVersion = 1;
    info->LobbyId = _strdup(lobbyId.c_str());
    info->LobbyOwnerUserId = lobbyOwner;
    info->PermissionLevel = 0; // Public
    info->AvailableSlots = 3;
    info->MaxMembers = 4;
    info->bAllowInvites = 1;
    info->BucketId = "refix_bucket";
    info->bAllowHostMigration = 0;
    info->bRTCRoomEnabled = 0;
    info->bAllowJoinById = 1;
    info->bRejoinAfterKickRequiresInvite = 0;
    info->bPresenceEnabled = 1;
    info->AllowedPlatformIds = nullptr;
    info->AllowedPlatformIdsCount = 0;
    
    *OutLobbyDetailsInfo = info;
    Log("  Created lobby info: ID=%s, Owner=%p", lobbyId.c_str(), lobbyOwner);
    return EOS_Success;
}

static void eos_LobbyDetails_Info_Release(EOS_LobbyDetails_Info* Info) {
    Log("EOS_LobbyDetails_Info_Release called");
    if (Info) {
        if (Info->LobbyId) free((void*)Info->LobbyId);
        free(Info);
    }
}

static void eos_LobbyDetails_Release(void* H) {
    Log("EOS_LobbyDetails_Release called");
}

static uint32_t eos_LobbyDetails_GetAttributeCount(void* H, void* O) {
    uint32_t count = (uint32_t)(g_capturedSearchParameters.size() + 4);
    Log("EOS_LobbyDetails_GetAttributeCount called -> %u (custom P2P attributes included)", count);
    return count;
}

struct EOS_LobbyDetails_CopyAttributeByIndexOptions {
    int32_t ApiVersion;
    uint32_t AttributeIndex;
};

struct EOS_Lobby_Attribute {
    int32_t ApiVersion;
    EOS_Lobby_AttributeData* Data;
    int32_t Visibility;
};

static EOS_EResult eos_LobbyDetails_CopyAttributeByIndex(void* H, void* O, EOS_Lobby_Attribute** OutAttribute) {
    Log("EOS_LobbyDetails_CopyAttributeByIndex called");
    if (!O || !OutAttribute) return EOS_NotFound;
    uint32_t index = ((EOS_LobbyDetails_CopyAttributeByIndexOptions*)O)->AttributeIndex;

    // Determine the IP/Port to return based on whether this is a real Steam lobby
    char lobbyIP[64] = "127.0.0.1";
    char lobbyPort[16] = "7777";
    
    // Check if the handle maps to a cached Steam lobby
    uintptr_t handleVal = (uintptr_t)H;
    if (handleVal >= 0x1000 && handleVal < 0x1000 + g_steamLobbies.size()) {
        uint32_t lobbyIdx = (uint32_t)(handleVal - 0x1000);
        strncpy_s(lobbyIP, g_steamLobbies[lobbyIdx].hostIP, _TRUNCATE);
        strncpy_s(lobbyPort, g_steamLobbies[lobbyIdx].hostPort, _TRUNCATE);
        Log("  Using real Steam lobby data: IP=%s, Port=%s", lobbyIP, lobbyPort);
    } else {
        // Local/host lobby - use our own public/LAN IP
        GetBestLocalIP(lobbyIP, sizeof(lobbyIP));
    }

    const char* defaultKeys[4] = { "SERVER_IP", "SERVER_PORT", "HOST_IP", "HOST_PORT" };
    const char* defaultVals[4] = { lobbyIP, lobbyPort, lobbyIP, lobbyPort };

    const char* keyName = "";
    const char* valStr = "";

    if (index < g_capturedSearchParameters.size()) {
        keyName = g_capturedSearchParameters[index].Key;
        if (g_capturedSearchParameters[index].ValueType == 4 && g_capturedSearchParameters[index].Value.AsUtf8) {
            valStr = g_capturedSearchParameters[index].Value.AsUtf8;
        } else {
            valStr = "dummy";
        }
    } else {
        uint32_t defIdx = index - (uint32_t)g_capturedSearchParameters.size();
        if (defIdx < 4) {
            keyName = defaultKeys[defIdx];
            valStr = defaultVals[defIdx];
        } else {
            return EOS_NotFound;
        }
    }

    auto* attr = (EOS_Lobby_Attribute*)malloc(sizeof(EOS_Lobby_Attribute));
    if (!attr) return EOS_LimitExceeded;
    
    auto* data = (EOS_Lobby_AttributeData*)malloc(sizeof(EOS_Lobby_AttributeData));
    if (!data) { free(attr); return EOS_LimitExceeded; }
    
    data->ApiVersion = 1;
    data->Key = _strdup(keyName);
    data->ValueType = 3; // String (EOS_AT_STRING)
    data->Value.AsUtf8 = _strdup(valStr);
    
    attr->ApiVersion = 1;
    attr->Data = data;
    attr->Visibility = 0; // Public
    
    *OutAttribute = attr;
    Log("  Returning attribute at index %d: Key=%s, Value=%s", index, keyName, valStr);
    return EOS_Success;
}

struct EOS_LobbyDetails_CopyAttributeByKeyOptions {
    int32_t ApiVersion;
    const char* AttrKey;
};

static EOS_EResult eos_LobbyDetails_CopyAttributeByKey(void* H, void* O, EOS_Lobby_Attribute** OutAttribute) {
    Log("EOS_LobbyDetails_CopyAttributeByKey called");
    if (!O || !OutAttribute || !((EOS_LobbyDetails_CopyAttributeByKeyOptions*)O)->AttrKey) return EOS_NotFound;
    const char* key = ((EOS_LobbyDetails_CopyAttributeByKeyOptions*)O)->AttrKey;
    Log("  Searching for key: %s", key);
    
    char lobbyIP[64] = "127.0.0.1";
    char lobbyPort[16] = "7777";
    
    uintptr_t handleVal = (uintptr_t)H;
    if (handleVal >= 0x1000 && handleVal < 0x1000 + g_steamLobbies.size()) {
        uint32_t lobbyIdx = (uint32_t)(handleVal - 0x1000);
        strncpy_s(lobbyIP, g_steamLobbies[lobbyIdx].hostIP, _TRUNCATE);
        strncpy_s(lobbyPort, g_steamLobbies[lobbyIdx].hostPort, _TRUNCATE);
        Log("  [CopyAttributeByKey] Found lobby handle %p -> HostIP=%s:%s", H, lobbyIP, lobbyPort);
    } else {
        GetBestLocalIP(lobbyIP, sizeof(lobbyIP));
    }

    int index = -1;
    for (size_t i = 0; i < g_capturedSearchParameters.size(); i++) {
        if (strcmp(g_capturedSearchParameters[i].Key, key) == 0) {
            index = (int)i;
            break;
        }
    }
    
    if (index >= 0) {
        EOS_LobbyDetails_CopyAttributeByIndexOptions idxOpts = {};
        idxOpts.ApiVersion = 1;
        idxOpts.AttributeIndex = (uint32_t)index;
        return eos_LobbyDetails_CopyAttributeByIndex(H, &idxOpts, OutAttribute);
    }

    char connectStr[128];
    sprintf_s(connectStr, sizeof(connectStr), "%s:%s", lobbyIP, lobbyPort);

    const char* valStr = connectStr;
    if (_stricmp(key, "HOST_PORT") == 0 || _stricmp(key, "SERVER_PORT") == 0) valStr = lobbyPort;
    else if (_stricmp(key, "HOST_IP") == 0 || _stricmp(key, "SERVER_IP") == 0) valStr = lobbyIP;

    Log("    Key '%s' resolved for RedpointEOS -> '%s'", key, valStr);
    auto* attr = (EOS_Lobby_Attribute*)malloc(sizeof(EOS_Lobby_Attribute));
    if (!attr) return EOS_LimitExceeded;
    auto* data = (EOS_Lobby_AttributeData*)malloc(sizeof(EOS_Lobby_AttributeData));
    if (!data) { free(attr); return EOS_LimitExceeded; }
    
    data->ApiVersion = 1;
    data->Key = _strdup(key);
    data->ValueType = 3; // String (EOS_AT_STRING)
    data->Value.AsUtf8 = _strdup(valStr);
    
    attr->ApiVersion = 1;
    attr->Data = data;
    attr->Visibility = 0;
    
    *OutAttribute = attr;
    return EOS_Success;
}

static void eos_Lobby_Attribute_Release(EOS_Lobby_Attribute* Attribute) {
    Log("EOS_Lobby_Attribute_Release called");
    if (Attribute) {
        if (Attribute->Data) {
            if (Attribute->Data->Key) free((void*)Attribute->Data->Key);
            if (Attribute->Data->ValueType == 4 && Attribute->Data->Value.AsUtf8) {
                free((void*)Attribute->Data->Value.AsUtf8);
            }
            free(Attribute->Data);
        }
        free(Attribute);
    }
}

static uint32_t eos_LobbyDetails_GetMemberCount(void* H, void* O) {
    uintptr_t handleVal = (uintptr_t)H;
    if (handleVal >= 0x1000 && handleVal < 0x1000 + g_steamLobbies.size()) {
        uint32_t lobbyIdx = (uint32_t)(handleVal - 0x1000);
        uint64_t steamLobbyId = g_steamLobbies[lobbyIdx].steamLobbyId;
        void* mm = GetSteamMatchmakingInterface();
        if (mm && g_pfn_MM_GetNumLobbyMembers && steamLobbyId != 0) {
            int numMembers = g_pfn_MM_GetNumLobbyMembers(mm, steamLobbyId);
            if (numMembers > 0) {
                Log("EOS_LobbyDetails_GetMemberCount handle %p -> Steam Lobby %llu MemberCount=%d", H, steamLobbyId, numMembers);
                return (uint32_t)numMembers;
            }
        }
    }
    Log("EOS_LobbyDetails_GetMemberCount called -> 2");
    return 2;
}

static uint64_t g_nextNotifId = 1000;

static EOS_NotificationId eos_Lobby_AddNotifyJoinLobbyAccepted(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_AddNotifyJoinLobbyAccepted called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Lobby_RemoveNotifyJoinLobbyAccepted(void* H, EOS_NotificationId InId) {
    Log("EOS_Lobby_RemoveNotifyJoinLobbyAccepted called (id=%llu)", InId);
}

static EOS_NotificationId eos_Lobby_AddNotifyLeaveLobbyRequested(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_AddNotifyLeaveLobbyRequested called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Lobby_RemoveNotifyLeaveLobbyRequested(void* H, EOS_NotificationId InId) {
    Log("EOS_Lobby_RemoveNotifyLeaveLobbyRequested called (id=%llu)", InId);
}

static EOS_NotificationId eos_Lobby_AddNotifyLobbyInviteAccepted(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_AddNotifyLobbyInviteAccepted called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Lobby_RemoveNotifyLobbyInviteAccepted(void* H, EOS_NotificationId InId) {
    Log("EOS_Lobby_RemoveNotifyLobbyInviteAccepted called (id=%llu)", InId);
}

static EOS_NotificationId eos_Lobby_AddNotifyLobbyInviteReceived(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_AddNotifyLobbyInviteReceived called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Lobby_RemoveNotifyLobbyInviteReceived(void* H, EOS_NotificationId InId) {
    Log("EOS_Lobby_RemoveNotifyLobbyInviteReceived called (id=%llu)", InId);
}

static EOS_NotificationId eos_Lobby_AddNotifyLobbyInviteRejected(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_AddNotifyLobbyInviteRejected called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Lobby_RemoveNotifyLobbyInviteRejected(void* H, EOS_NotificationId InId) {
    Log("EOS_Lobby_RemoveNotifyLobbyInviteRejected called (id=%llu)", InId);
}

static EOS_NotificationId eos_Lobby_AddNotifyLobbyMemberStatusReceived(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_AddNotifyLobbyMemberStatusReceived called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Lobby_RemoveNotifyLobbyMemberStatusReceived(void* H, EOS_NotificationId InId) {
    Log("EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived called (id=%llu)", InId);
}

static EOS_NotificationId eos_Lobby_AddNotifyLobbyMemberUpdateReceived(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_AddNotifyLobbyMemberUpdateReceived called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Lobby_RemoveNotifyLobbyMemberUpdateReceived(void* H, EOS_NotificationId InId) {
    Log("EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived called (id=%llu)", InId);
}

static EOS_NotificationId eos_Lobby_AddNotifyLobbyUpdateReceived(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_AddNotifyLobbyUpdateReceived called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Lobby_RemoveNotifyLobbyUpdateReceived(void* H, EOS_NotificationId InId) {
    Log("EOS_Lobby_RemoveNotifyLobbyUpdateReceived called (id=%llu)", InId);
}

struct CB_Lobby_SendInvite {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    const char* LobbyId;
};

static void eos_Lobby_SendInvite(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_SendInvite called");
    CB_Lobby_SendInvite info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LobbyId = _strdup(g_productUserIdStr);
    QueueCallback(Cb, info);
}

static void* eos_LobbyDetails_GetMemberByIndex(void* H, void* O) {
    Log("EOS_LobbyDetails_GetMemberByIndex called -> FAKE_PRODUCT_USER_ID");
    return FAKE_PRODUCT_USER_ID;
}

static uint32_t eos_LobbyDetails_GetMemberAttributeCount(void* H, void* O) {
    Log("EOS_LobbyDetails_GetMemberAttributeCount called -> 0");
    return 0;
}

static EOS_EResult eos_LobbyDetails_CopyMemberAttributeByIndex(void* H, void* O, EOS_Lobby_Attribute** OutAttribute) {
    Log("EOS_LobbyDetails_CopyMemberAttributeByIndex called -> EOS_NotFound");
    return EOS_NotFound;
}

static EOS_EResult eos_LobbyDetails_CopyMemberAttributeByKey(void* H, void* O, EOS_Lobby_Attribute** OutAttribute) {
    Log("EOS_LobbyDetails_CopyMemberAttributeByKey called -> EOS_NotFound");
    return EOS_NotFound;
}

static void* eos_LobbyDetails_GetLobbyOwner(void* H, void* O) {
    Log("EOS_LobbyOwner_GetLobbyOwner called -> FAKE_PRODUCT_USER_ID");
    return FAKE_PRODUCT_USER_ID;
}



static HMODULE ResolveSteamApiDll() {
    HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
    if (hSteam) return hSteam;
    hSteam = LoadLibraryA("steam_api64.dll");
    if (hSteam) return hSteam;
    hSteam = LoadLibraryA("steam_api64_valve.dll");
    if (hSteam) return hSteam;

    // Search relative to the executable directory (supports nested Unreal Engine structures)
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) > 0) {
        std::string dir(exePath);
        size_t p = dir.find_last_of("\\/");
        if (p != std::string::npos) {
            std::string root = dir.substr(0, p);
            std::string candidate = root + "\\steam_api64.dll";
            hSteam = LoadLibraryA(candidate.c_str());
            if (hSteam) return hSteam;
            
            candidate = root + "\\..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv157\\Win64\\steam_api64.dll";
            hSteam = LoadLibraryA(candidate.c_str());
            if (hSteam) return hSteam;

            candidate = root + "\\..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv153\\Win64\\steam_api64.dll";
            hSteam = LoadLibraryA(candidate.c_str());
            if (hSteam) return hSteam;
        }
    }
    return nullptr;
}

static void* GetSteamFriendsInterface() {
    static void* friendsInterface = nullptr;
    if (friendsInterface) return friendsInterface;
    
    HMODULE hSteam = ResolveSteamApiDll();
    if (hSteam) {
        g_pfn_SteamFriends = (fn_SteamFriends_t)GetProcAddress(hSteam, "SteamAPI_SteamFriends_v017");
        if (!g_pfn_SteamFriends) g_pfn_SteamFriends = (fn_SteamFriends_t)GetProcAddress(hSteam, "SteamAPI_SteamFriends_v016");
        if (!g_pfn_SteamFriends) g_pfn_SteamFriends = (fn_SteamFriends_t)GetProcAddress(hSteam, "SteamFriends");
        
        g_pfn_GetFriendCount = (fn_GetFriendCount_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_GetFriendCount");
        g_pfn_GetFriendByIndex = (fn_GetFriendByIndex_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_GetFriendByIndex");
        g_pfn_GetFriendPersonaName = (fn_GetFriendPersonaName_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_GetFriendPersonaName");
        g_pfn_GetPersonaName = (fn_GetPersonaName_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_GetPersonaName");
        g_pfn_SetRichPresence = (fn_SetRichPresence_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_SetRichPresence");
        g_pfn_GetFriendRichPresence = (fn_GetFriendRichPresence_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_GetFriendRichPresence");
        
        if (g_pfn_SteamFriends) {
            friendsInterface = g_pfn_SteamFriends();
            if (!friendsInterface) {
                typedef bool (*fn_Init_t)();
                auto pfnInit = (fn_Init_t)GetProcAddress(hSteam, "SteamAPI_Init");
                if (!pfnInit) pfnInit = (fn_Init_t)GetProcAddress(hSteam, "SteamAPI_InitSafe");
                if (pfnInit) { pfnInit(); friendsInterface = g_pfn_SteamFriends(); }
            }
        }
    }
    return friendsInterface;
}

static void* GetSteamMatchmakingInterface() {
    if (g_steamMatchmakingInterface) return g_steamMatchmakingInterface;
    
    HMODULE hSteam = ResolveSteamApiDll();
    if (!hSteam) {
        Log("[ReGoldberg][Unreal][EOS][WARN] GetSteamMatchmakingInterface: Could not find steam_api64.dll or steam_api64_valve.dll");
        return nullptr;
    }
    
    g_pfn_SteamMatchmaking = (fn_SteamMatchmaking_t)GetProcAddress(hSteam, "SteamAPI_SteamMatchmaking_v009");
    if (!g_pfn_SteamMatchmaking) g_pfn_SteamMatchmaking = (fn_SteamMatchmaking_t)GetProcAddress(hSteam, "SteamAPI_SteamMatchmaking_v008");
    if (!g_pfn_SteamMatchmaking) g_pfn_SteamMatchmaking = (fn_SteamMatchmaking_t)GetProcAddress(hSteam, "SteamMatchmaking");
    if (!g_pfn_SteamMatchmaking) {
        Log("[ReGoldberg][Unreal][EOS][WARN] GetSteamMatchmakingInterface: SteamMatchmaking getter not found");
        return nullptr;
    }
    
    g_steamMatchmakingInterface = g_pfn_SteamMatchmaking();
    if (!g_steamMatchmakingInterface) {
        typedef bool (*fn_Init_t)();
        auto pfnInit = (fn_Init_t)GetProcAddress(hSteam, "SteamAPI_Init");
        if (!pfnInit) pfnInit = (fn_Init_t)GetProcAddress(hSteam, "SteamAPI_InitSafe");
        if (pfnInit) {
            pfnInit();
            g_steamMatchmakingInterface = g_pfn_SteamMatchmaking();
        }
    }
    if (!g_steamMatchmakingInterface) {
        Log("[ReGoldberg][Unreal][EOS][WARN] GetSteamMatchmakingInterface: Interface returned null (SteamAPI initialization pending)");
        return nullptr;
    }
    
    // Load all matchmaking function pointers
    g_pfn_MM_CreateLobby      = (fn_MM_CreateLobby_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_CreateLobby");
    g_pfn_MM_RequestLobbyList = (fn_MM_RequestLobbyList_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_RequestLobbyList");
    g_pfn_MM_JoinLobby        = (fn_MM_JoinLobby_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_JoinLobby");
    g_pfn_MM_LeaveLobby       = (fn_MM_LeaveLobby_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_LeaveLobby");
    g_pfn_MM_SetLobbyData     = (fn_MM_SetLobbyData_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_SetLobbyData");
    g_pfn_MM_GetLobbyData     = (fn_MM_GetLobbyData_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_GetLobbyData");
    g_pfn_MM_GetLobbyByIndex  = (fn_MM_GetLobbyByIndex_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_GetLobbyByIndex");
    g_pfn_MM_AddStringFilter  = (fn_MM_AddStringFilter_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
    g_pfn_MM_AddResultCountFilter = (fn_MM_AddResultCountFilter_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter");
    g_pfn_MM_GetNumLobbyMembers = (fn_MM_GetNumLobbyMembers_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
    g_pfn_MM_SetLobbyType     = (fn_MM_SetLobbyType_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_SetLobbyType");
    g_pfn_MM_SetLobbyJoinable = (fn_MM_SetLobbyJoinable_t)GetProcAddress(hSteam, "SteamAPI_ISteamMatchmaking_SetLobbyJoinable");
    
    // Also load callback runner
    g_pfn_SteamRunCallbacks = (fn_SteamAPI_RunCallbacks_t)GetProcAddress(hSteam, "SteamAPI_RunCallbacks");
    g_pfn_SteamRegisterCallResult = (fn_SteamAPI_RegisterCallResult_t)GetProcAddress(hSteam, "SteamAPI_RegisterCallResult");
    
    Log("[ReGoldberg][Unreal][EOS][INFO] GetSteamMatchmakingInterface: Successfully loaded ISteamMatchmaking %p", g_steamMatchmakingInterface);
    return g_steamMatchmakingInterface;
}

struct CB_Friends_QueryFriends {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    void*       LocalUserId;
};

static void eos_Friends_QueryFriends(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Friends_QueryFriends called");
    CB_Friends_QueryFriends info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_EPIC_ACCOUNT_ID;
    QueueCallback(Cb, info);
}

static int32_t eos_Friends_GetFriendsCount(void* H, void* O) {
    Log("EOS_Friends_GetFriendsCount called");
    void* steamFriends = GetSteamFriendsInterface();
    if (steamFriends && g_pfn_GetFriendCount) {
        int count = g_pfn_GetFriendCount(steamFriends, 0x04); // k_EFriendFlagImmediate
        Log("  Found %d Steam friends", count);
        return count;
    }
    return 0;
}

struct EOS_Friends_GetFriendAtIndexOptions {
    int32_t ApiVersion;
    void* LocalUserId;
    int32_t Index;
};

static void* eos_Friends_GetFriendAtIndex(void* H, void* O) {
    Log("EOS_Friends_GetFriendAtIndex called");
    if (!O) return nullptr;
    int32_t index = ((EOS_Friends_GetFriendAtIndexOptions*)O)->Index;
    
    void* steamFriends = GetSteamFriendsInterface();
    if (steamFriends && g_pfn_GetFriendByIndex) {
        uint64_t steamId = g_pfn_GetFriendByIndex(steamFriends, index, 0x04);
        if (steamId != 0) {
            char idStr[32];
            sprintf_s(idStr, sizeof(idStr), "%llu", steamId);
            void* fakeEpicId = GetOrCreateProductUserId(idStr);
            Log("  Friend %d -> SteamID %s (pointer %p)", index, idStr, fakeEpicId);
            return fakeEpicId;
        }
    }
    return nullptr;
}

// =============================================================================
// SESSIONS (Unreal Engine RedpointEOS Session Matchmaking)
// =============================================================================
static bool g_hasActiveSession = false;

struct CB_Sessions_UpdateSession {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    const char* SessionName;
};

static void eos_Sessions_UpdateSession(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_UpdateSession called for SessionName='%s'", g_activeSessionName.c_str());
    g_hasActiveSession = true;
    CreateAndTagRealSteamLobbyAsync();
    
    CB_Sessions_UpdateSession info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.SessionName = g_activeSessionName.c_str();
    QueueCallback(Cb, info);
}

struct EOS_Sessions_CreateSessionModificationOptions {
    int32_t ApiVersion;
    const char* SessionName;
};

static EOS_EResult eos_Sessions_CreateSessionModification(void* H, void* O, void** OutSessionModificationHandle) {
    if (O) {
        auto* opts = (EOS_Sessions_CreateSessionModificationOptions*)O;
        if (opts->SessionName && opts->SessionName[0] != '\0') {
            g_activeSessionName = opts->SessionName;
            Log("EOS_Sessions_CreateSessionModification for SessionName='%s'", g_activeSessionName.c_str());
        }
    } else {
        Log("EOS_Sessions_CreateSessionModification called");
    }
    if (OutSessionModificationHandle) *OutSessionModificationHandle = (void*)0x2000;
    return EOS_Success;
}

struct CB_Sessions_DestroySession {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};

static void eos_Sessions_DestroySession(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_DestroySession called");
    g_hasActiveSession = false;
    CB_Sessions_DestroySession info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
}

static EOS_EResult eos_Sessions_CreateSessionSearch(void* H, void* O, void** OutSessionSearchHandle) {
    Log("EOS_Sessions_CreateSessionSearch called");
    if (OutSessionSearchHandle) *OutSessionSearchHandle = (void*)0x2001;
    return EOS_Success;
}

struct CB_SessionSearch_Find {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};

static void eos_SessionSearch_Find(void* H, void* O, void* C, void* Cb) {
    Log("EOS_SessionSearch_Find called");
    eos_LobbySearch_Find(H, O, C, Cb);
}

static uint32_t eos_SessionSearch_GetSearchResultCount(void* H, void* O) {
    uint32_t count = (uint32_t)g_foundLobbies.size();
    if (count == 0 && g_hasActiveSession) count = 1;
    Log("EOS_SessionSearch_GetSearchResultCount called -> %u", count);
    return count;
}

struct EOS_SessionSearch_CopySearchResultByIndexOptions {
    int32_t ApiVersion;
    uint32_t SessionIndex;
};

static EOS_EResult eos_SessionSearch_CopySearchResultByIndex(void* H, void* O, void** OutSessionDetailsHandle) {
    Log("EOS_SessionSearch_CopySearchResultByIndex called");
    if (!O || !OutSessionDetailsHandle) return EOS_NotFound;
    uint32_t index = ((EOS_SessionSearch_CopySearchResultByIndexOptions*)O)->SessionIndex;
    
    if (index < (uint32_t)g_foundLobbies.size()) {
        // Remote lobby from Steam search — use 0x3000+ range (distinct from 0x2002 = local session)
        *OutSessionDetailsHandle = (void*)(uintptr_t)(0x3000 + index);
        Log("  Returning session handle %p for REMOTE search result index %u", *OutSessionDetailsHandle, index);
    } else if (index == 0 && g_hasActiveSession) {
        // Fallback: no remote lobbies found but we have our own active session
        *OutSessionDetailsHandle = (void*)0x2002;
        Log("  Returning LOCAL active session handle 0x2002 for index 0");
    } else {
        Log("  ERROR: search result index %u out of range (foundLobbies=%u)", index, (uint32_t)g_foundLobbies.size());
        return EOS_NotFound;
    }
    return EOS_Success;
}

// =============================================================================
// P2P REAL IMPLEMENTATION via ISteamNetworking
// Routes EOS P2P calls through Valve's Steam relay for NAT traversal
// =============================================================================

// ISteamNetworking vtable indices (ISteamNetworking006)
// Verified order: SendP2PPacket=0, IsP2PPacketAvailable=1, ReadP2PPacket=2,
//                 AcceptP2PSessionWithUser=3, CloseP2PSessionWithUser=4
#define ISTEAMNETWORKING_SENDP2PPACKET_IDX      0
#define ISTEAMNETWORKING_ISP2PPACKETAVAIL_IDX   1
#define ISTEAMNETWORKING_READP2PPACKET_IDX      2
#define ISTEAMNETWORKING_ACCEPTP2PSESSION_IDX   3
#define ISTEAMNETWORKING_CLOSEP2PSESSION_IDX    4

typedef bool (*fn_ISN_SendP2PPacket_t)(void* self, uint64_t steamIDRemote, const void* pubData, uint32_t cubData, int eP2PSendType, int nChannel);
typedef bool (*fn_ISN_IsP2PPacketAvail_t)(void* self, uint32_t* pcubMsgSize, int nChannel);
typedef bool (*fn_ISN_ReadP2PPacket_t)(void* self, void* pubDest, uint32_t cubDest, uint32_t* pcubMsgSize, uint64_t* psteamIDRemote, int nChannel);
typedef bool (*fn_ISN_AcceptP2PSession_t)(void* self, uint64_t steamIDRemote);
typedef bool (*fn_ISN_CloseP2PSession_t)(void* self, uint64_t steamIDRemote);

// ISteamNetworking getter — loaded from steam_api64.dll in-process
static void* GetSteamNetworkingInterface_EOS() {
    static void* s_iface = nullptr;
    if (s_iface) return s_iface;

    HMODULE hSteam = ResolveSteamApiDll();
    if (!hSteam) return nullptr;

    // Try versioned getter first
    typedef void* (*fn_t)();
    const char* names[] = {
        "SteamAPI_SteamNetworking_v006",
        "SteamAPI_ISteamNetworking_v006",
        "SteamNetworking"
    };
    for (auto name : names) {
        auto fn = (fn_t)GetProcAddress(hSteam, name);
        if (fn) { s_iface = fn(); break; }
    }
    if (!s_iface) {
        typedef bool (*fn_Init_t)();
        auto pfnInit = (fn_Init_t)GetProcAddress(hSteam, "SteamAPI_Init");
        if (!pfnInit) pfnInit = (fn_Init_t)GetProcAddress(hSteam, "SteamAPI_InitSafe");
        if (pfnInit) {
            pfnInit();
            for (auto name : names) {
                auto fn = (fn_t)GetProcAddress(hSteam, name);
                if (fn) { s_iface = fn(); break; }
            }
        }
    }
    if (s_iface)
        Log("[ReGoldberg][Unreal][EOS][INFO] ISteamNetworking resolved: %p", s_iface);
    else
        Log("[ReGoldberg][Unreal][EOS][WARN] ISteamNetworking not found in steam_api64.dll");
    return s_iface;
}

// Inline vtable call helpers
static inline fn_ISN_SendP2PPacket_t GetSN_Send() {
    void* iface = GetSteamNetworkingInterface_EOS();
    if (!iface) return nullptr;
    return ((fn_ISN_SendP2PPacket_t**)iface)[0][ISTEAMNETWORKING_SENDP2PPACKET_IDX];
}
static inline fn_ISN_IsP2PPacketAvail_t GetSN_IsAvail() {
    void* iface = GetSteamNetworkingInterface_EOS();
    if (!iface) return nullptr;
    return ((fn_ISN_IsP2PPacketAvail_t**)iface)[0][ISTEAMNETWORKING_ISP2PPACKETAVAIL_IDX];
}
static inline fn_ISN_ReadP2PPacket_t GetSN_Read() {
    void* iface = GetSteamNetworkingInterface_EOS();
    if (!iface) return nullptr;
    return ((fn_ISN_ReadP2PPacket_t**)iface)[0][ISTEAMNETWORKING_READP2PPACKET_IDX];
}
static inline fn_ISN_AcceptP2PSession_t GetSN_Accept() {
    void* iface = GetSteamNetworkingInterface_EOS();
    if (!iface) return nullptr;
    return ((fn_ISN_AcceptP2PSession_t**)iface)[0][ISTEAMNETWORKING_ACCEPTP2PSESSION_IDX];
}

// EOS_P2P_SendPacketOptions — we only need ApiVersion, RemoteUserId (SteamID64), and packet data.
// Layout based on EOS SDK 1.x (ApiVersion=1):
//   [0]  int32_t  ApiVersion
//   [4]  int32_t  pad
//   [8]  void*    LocalUserId
//   [16] void*    RemoteUserId   <-- SteamID64 cast as pointer
//   [24] struct*  SocketId       <-- ptr to struct { int32 ApiVersion; char Name[33]; }
//   [32] uint8_t  Channel
//   [33] uint8_t  bAllowDelayedDelivery
//   [34] uint8_t  Reliability    (0=UnreliableUnordered,1=Reliable,2=ReliableOrdered)
//   [36] uint32_t DataLengthBytes
//   [40] void*    Data
#pragma pack(push, 1)
struct EOS_P2P_SendPacketOptions_Layout {
    int32_t  ApiVersion;
    int32_t  _pad;
    void*    LocalUserId;
    void*    RemoteUserId;
    void*    SocketId;
    uint8_t  Channel;
    uint8_t  bAllowDelayedDelivery;
    uint8_t  Reliability;
    uint8_t  _pad2;
    uint32_t DataLengthBytes;
    void*    Data;
};
struct EOS_P2P_ReceivePacketOptions_Layout {
    int32_t  ApiVersion;
    int32_t  _pad;
    void*    LocalUserId;
    uint32_t MaxDataSizeBytes;
    uint8_t  RequestedChannel;  // 255 = any
};
struct EOS_P2P_GetNextReceivedPacketSizeOptions_Layout {
    int32_t ApiVersion;
    int32_t _pad;
    void*   LocalUserId;
    uint8_t RequestedChannel; // 255 = any
};
#pragma pack(pop)

static EOS_EResult eos_P2P_SendPacket(void* H, void* O) {
    if (!O) return EOS_Success;
    auto* opts = (EOS_P2P_SendPacketOptions_Layout*)O;

    // RemoteUserId is stored as a pointer; the Steam ID is embedded in the pointed-to string
    // or, in our emulation, it IS the SteamID64 cast to a pointer (set by GetOrCreateProductUserId)
    uint64_t remoteSteamID = 0;
    if (opts->RemoteUserId) {
        // Try to resolve via g_puidMap: look up SteamID string from PUID
        std::string extId = FindExternalId(opts->RemoteUserId);
        if (!extId.empty()) {
            remoteSteamID = _strtoui64(extId.c_str(), nullptr, 10);
        }
    }

    if (remoteSteamID == 0 || !opts->Data || opts->DataLengthBytes == 0) {
        // No valid peer — silently drop (avoids spam log when lobby not joined yet)
        return EOS_Success;
    }

    void* iface = GetSteamNetworkingInterface_EOS();
    auto pfnSend = GetSN_Send();
    if (!iface || !pfnSend) {
        Log("[P2P] SendPacket: ISteamNetworking not available, dropping packet");
        return EOS_Success;
    }

    // Accept their session first (no-op if already accepted)
    auto pfnAccept = GetSN_Accept();
    if (pfnAccept) pfnAccept(iface, remoteSteamID);

    // Reliability: 0=unreliable, 1/2=reliable
    int eType = (opts->Reliability >= 1) ? 2 /* k_EP2PSendReliable */ : 0 /* k_EP2PSendUnreliable */;

    bool ok = pfnSend(iface, remoteSteamID, opts->Data, opts->DataLengthBytes, eType, (int)opts->Channel);
    if (!ok) {
        Log("[P2P] SendP2PPacket to SteamID=%llu FAILED (channel=%d, size=%u)", remoteSteamID, opts->Channel, opts->DataLengthBytes);
    }
    return EOS_Success;
}

static EOS_EResult eos_P2P_GetNextReceivedPacketSize(void* H, void* O, uint32_t* OutPacketSizeBytes) {
    if (OutPacketSizeBytes) *OutPacketSizeBytes = 0;

    // Don't poll Steam networking if no lobby is active yet — avoids crash on early ticks
    if (!g_currentSteamLobbyId.load()) return EOS_NotFound;

    void* iface = GetSteamNetworkingInterface_EOS();
    auto pfnAvail = GetSN_IsAvail();
    if (!iface || !pfnAvail) return EOS_NotFound;

    uint32_t msgSize = 0;
    // Check channel 0 (main game channel); UE typically uses channel 0
    if (!pfnAvail(iface, &msgSize, 0)) return EOS_NotFound;

    if (OutPacketSizeBytes) *OutPacketSizeBytes = msgSize;
    return EOS_Success;
}

// EOS_P2P_ReceivePacketOptions/Out layout (EOS SDK 1.x)
// Out params are separate out-pointers passed by the game
static EOS_EResult eos_P2P_ReceivePacket(void* H, void* O,
    void* OutPeerId,        // EOS_ProductUserId* — peer who sent this
    void* OutSocketId,      // EOS_P2P_SocketId*  — socket name
    uint8_t* OutChannel,    // uint8_t*
    void* OutData,          // void*  (pre-allocated by game)
    uint32_t* OutBytesWritten)
{
    if (OutBytesWritten) *OutBytesWritten = 0;

    void* iface = GetSteamNetworkingInterface_EOS();
    auto pfnRead = GetSN_Read();
    if (!iface || !pfnRead) return EOS_NotFound;

    uint32_t bytesRead = 0;
    uint64_t remoteSteamID = 0;
    uint32_t maxSize = 65536;

    // Query available size first
    auto pfnAvail = GetSN_IsAvail();
    if (pfnAvail) {
        uint32_t avail = 0;
        if (!pfnAvail(iface, &avail, 0) || avail == 0) return EOS_NotFound;
        maxSize = avail;
    }

    // Use a static buffer if game provides nullptr (shouldn't happen but safety net)
    static uint8_t s_recvBuf[65536];
    void* dest = OutData ? OutData : s_recvBuf;

    bool ok = pfnRead(iface, dest, maxSize, &bytesRead, &remoteSteamID, 0);
    if (!ok || bytesRead == 0) return EOS_NotFound;

    if (OutBytesWritten) *OutBytesWritten = bytesRead;
    if (OutChannel) *OutChannel = 0;

    // Map SteamID back to EOS ProductUserId
    if (OutPeerId) {
        char steamIdStr[32];
        sprintf_s(steamIdStr, "%llu", remoteSteamID);
        void* puid = GetOrCreateProductUserId(steamIdStr);
        *(void**)OutPeerId = puid;
    }

    return EOS_Success;
}

static uint64_t eos_P2P_AddNotifyPeerConnectionRequest(void* H, void* O, void* C, void* Cb) {
    Log("EOS_P2P_AddNotifyPeerConnectionRequest called");
    return 1;
}

static void eos_P2P_RemoveNotifyPeerConnectionRequest(void* H, uint64_t Id) {}

static uint64_t eos_P2P_AddNotifyPeerConnectionClosed(void* H, void* O, void* C, void* Cb) {
    Log("EOS_P2P_AddNotifyPeerConnectionClosed called");
    return 1;
}

static void eos_P2P_RemoveNotifyPeerConnectionClosed(void* H, uint64_t Id) {}

static EOS_EResult eos_P2P_AcceptConnection(void* H, void* O) {
    Log("EOS_P2P_AcceptConnection called");
    // Also accept at the Steam level so ReadP2PPacket works
    if (O) {
        // Try to extract RemoteUserId from Options (offset 16, same layout as SendPacket)
        uint64_t* fields = (uint64_t*)O;
        if (fields[2]) { // RemoteUserId at offset 16
            std::string extId = FindExternalId((void*)fields[2]);
            if (!extId.empty()) {
                uint64_t steamID = _strtoui64(extId.c_str(), nullptr, 10);
                void* iface = GetSteamNetworkingInterface_EOS();
                auto pfnAccept = GetSN_Accept();
                if (iface && pfnAccept && steamID)
                    pfnAccept(iface, steamID);
            }
        }
    }
    return EOS_Success;
}

static EOS_EResult eos_P2P_CloseConnection(void* H, void* O) {
    Log("EOS_P2P_CloseConnection called");
    return EOS_Success;
}

static EOS_EResult eos_P2P_CloseConnections(void* H, void* O) {
    Log("EOS_P2P_CloseConnections called");
    return EOS_Success;
}

static EOS_EResult eos_P2P_SetPacketQueueSize(void* H, void* O) {
    return EOS_Success;
}

static EOS_EResult eos_P2P_SetPortRange(void* H, void* O) {
    return EOS_Success;
}

static EOS_EResult eos_P2P_SetRelayControl(void* H, void* O) {
    return EOS_Success;
}

// =============================================================================
// SESSION HANDLE IP RESOLUTION HELPER
// =============================================================================
// Resolves the correct host IP for a session details handle.
// For search result handles (0x3000+), returns the REMOTE host's IP from g_steamLobbies.
// For local session handles (0x2002 etc), returns the local player's public/local IP.
static void ResolveHostIPForSessionHandle(void* H, char* outIP, size_t outIPSize) {
    outIP[0] = '\0';
    uintptr_t hVal = (uintptr_t)H;
    if (hVal >= 0x3000 && (hVal - 0x3000) < (uintptr_t)g_steamLobbies.size()) {
        uint32_t idx = (uint32_t)(hVal - 0x3000);
        strncpy_s(outIP, outIPSize, g_steamLobbies[idx].hostIP, _TRUNCATE);
        if (outIP[0] != '\0') {
            Log("  ResolveHostIP: handle %p -> REMOTE host IP '%s' (steamLobby idx %u)", H, outIP, idx);
            return;
        }
    }
    // Fallback: local player's IP
    GetBestLocalIP(outIP, outIPSize);
    Log("  ResolveHostIP: handle %p -> LOCAL IP '%s'", H, outIP);
}

// =============================================================================
// SESSIONS MODIFICATION & DETAILS IMPLEMENTATIONS
// =============================================================================
static EOS_EResult eos_SessionModification_SetBucketId(void* H, void* O) {
    Log("EOS_SessionModification_SetBucketId called");
    return EOS_Success;
}

static EOS_EResult eos_SessionModification_SetHostAddress(void* H, void* O) {
    Log("EOS_SessionModification_SetHostAddress called");
    return EOS_Success;
}

static EOS_EResult eos_SessionModification_SetPermissionLevel(void* H, void* O) {
    Log("EOS_SessionModification_SetPermissionLevel called");
    return EOS_Success;
}

static EOS_EResult eos_SessionModification_SetMaxPlayers(void* H, void* O) {
    Log("EOS_SessionModification_SetMaxPlayers called");
    return EOS_Success;
}

static EOS_EResult eos_SessionModification_SetJoinInProgressAllowed(void* H, void* O) {
    Log("EOS_SessionModification_SetJoinInProgressAllowed called");
    return EOS_Success;
}

static EOS_EResult eos_SessionModification_SetInvitesAllowed(void* H, void* O) {
    Log("EOS_SessionModification_SetInvitesAllowed called");
    return EOS_Success;
}

static EOS_EResult eos_SessionModification_AddAttribute(void* H, void* O) {
    Log("EOS_SessionModification_AddAttribute called");
    return EOS_Success;
}

static EOS_EResult eos_SessionModification_RemoveAttribute(void* H, void* O) {
    Log("EOS_SessionModification_RemoveAttribute called");
    return EOS_Success;
}

static void eos_SessionModification_Release(void* H) {
    Log("EOS_SessionModification_Release called");
}

struct EOS_SessionDetails_Settings {
    int32_t ApiVersion;
    const char* BucketId;
    uint32_t NumPublicConnections;
    int32_t PermissionLevel;
    int32_t bAllowJoinInProgress;
    int32_t bSanctionsEnabled;
    int32_t bAllowInvites;
    int32_t bPresenceEnabled;
    int32_t bAllowJoinViaPresence;
    int32_t bAllowJoinViaPresenceFriendsOnly;
    int32_t bInvitesDisabled;
    const char* SchemaName;
    const uint32_t* AllowedPlatformIds;
    uint32_t AllowedPlatformIdsCount;
};

struct EOS_SessionDetails_Info {
    int32_t ApiVersion;
    const char* SessionId;
    const char* HostAddress;
    uint32_t NumOpenPublicConnections;
    EOS_SessionDetails_Settings* Settings;
    void* OwnerUserId;
    const char* OwnerDeviceId;
};

static EOS_EResult eos_SessionDetails_CopyInfo(void* H, void* O, EOS_SessionDetails_Info** OutSessionDetailsInfo) {
    Log("EOS_SessionDetails_CopyInfo called for handle %p", H);
    if (!OutSessionDetailsInfo) return EOS_LimitExceeded;
    
    // Resolve host IP: remote lobby IP for search results, local IP for own session
    char pubIP[64] = "";
    ResolveHostIPForSessionHandle(H, pubIP, sizeof(pubIP));

    char connectStr[128];
    sprintf_s(connectStr, sizeof(connectStr), "%s:7777", pubIP);

    void* sessionOwner = FAKE_PRODUCT_USER_ID;
    uintptr_t val = (uintptr_t)H;
    if (val >= 0x3000 && val < 0x3000 + g_foundLobbies.size()) {
        uint32_t idx = (uint32_t)(val - 0x3000);
        sessionOwner = GetOrCreateProductUserId(g_foundLobbies[idx]);
    }

    auto* settings = (EOS_SessionDetails_Settings*)malloc(sizeof(EOS_SessionDetails_Settings));
    if (!settings) return EOS_LimitExceeded;
    memset(settings, 0, sizeof(EOS_SessionDetails_Settings));
    settings->ApiVersion = 3;
    settings->BucketId = _strdup("refix_bucket");
    settings->NumPublicConnections = 4;
    settings->PermissionLevel = 0; // Public
    settings->bAllowJoinInProgress = 1;
    settings->bSanctionsEnabled = 0;
    settings->bAllowInvites = 1;
    settings->bPresenceEnabled = 1;
    settings->bAllowJoinViaPresence = 1;
    settings->bAllowJoinViaPresenceFriendsOnly = 0;
    settings->bInvitesDisabled = 0;
    settings->SchemaName = nullptr;
    settings->AllowedPlatformIds = nullptr;
    settings->AllowedPlatformIdsCount = 0;

    auto* info = (EOS_SessionDetails_Info*)malloc(sizeof(EOS_SessionDetails_Info));
    if (!info) { free(settings); return EOS_LimitExceeded; }
    memset(info, 0, sizeof(EOS_SessionDetails_Info));
    
    info->ApiVersion = 2;
    info->SessionId = _strdup("RefixSession_001");
    info->HostAddress = _strdup(connectStr);
    info->NumOpenPublicConnections = 4;
    info->Settings = settings;
    info->OwnerUserId = sessionOwner;
    info->OwnerDeviceId = nullptr;
    
    *OutSessionDetailsInfo = info;
    Log("  Returning SessionDetails_Info (HostAddress=%s, Settings=%p, Owner=%p)", connectStr, settings, sessionOwner);
    return EOS_Success;
}

static void eos_SessionDetails_Info_Release(EOS_SessionDetails_Info* Info) {
    Log("EOS_SessionDetails_Info_Release called for %p", Info);
    if (Info) {
        if (Info->SessionId) free((void*)Info->SessionId);
        if (Info->HostAddress) free((void*)Info->HostAddress);
        if (Info->Settings) {
            if (Info->Settings->BucketId) free((void*)Info->Settings->BucketId);
            free(Info->Settings);
        }
        free(Info);
    }
}

static void eos_SessionDetails_Release(void* H) {
    Log("EOS_SessionDetails_Release called");
}

union EOS_SessionDetails_AttributeValue {
    int64_t AsInt64;
    double AsDouble;
    int32_t bAsBool;
    const char* AsUtf8;
};

struct EOS_SessionDetails_AttributeData {
    int32_t ApiVersion;
    const char* Key;
    EOS_SessionDetails_AttributeValue Value;
    int32_t ValueType;
};

struct EOS_SessionDetails_Attribute {
    int32_t ApiVersion;
    EOS_SessionDetails_AttributeData* Data;
    int32_t AdvertisementType;
};

static EOS_SessionDetails_AttributeData* CreateSessionAttributeData(const char* key, const char* resolvedIP = nullptr) {
    auto* data = (EOS_SessionDetails_AttributeData*)malloc(sizeof(EOS_SessionDetails_AttributeData));
    if (!data) return nullptr;
    memset(data, 0, sizeof(EOS_SessionDetails_AttributeData));
    data->ApiVersion = 1;
    data->Key = _strdup(key ? key : "");

    char pubIP[64] = "127.0.0.1";
    if (resolvedIP && resolvedIP[0] != '\0') {
        strncpy_s(pubIP, sizeof(pubIP), resolvedIP, _TRUNCATE);
    } else {
        GetBestLocalIP(pubIP, sizeof(pubIP));
    }

    if (key && (_stricmp(key, "SERVER_PORT") == 0 || _stricmp(key, "HOST_PORT") == 0)) {
        data->ValueType = 1; // Int64
        data->Value.AsInt64 = 7777;
    }
    else if (key && (_strnicmp(key, "__EOS_b", 7) == 0 || _stricmp(key, "bUsesPresence") == 0)) {
        data->ValueType = 0; // Boolean
        data->Value.bAsBool = 1;
    }
    else if (key && (_strnicmp(key, "__EOS_num", 9) == 0 || _stricmp(key, "NumPublicConnections") == 0)) {
        data->ValueType = 1; // Int64
        data->Value.AsInt64 = 4;
    }
    else {
        data->ValueType = 3; // String
        data->Value.AsUtf8 = _strdup(pubIP);
    }
    return data;
}

static const char* g_sessionAttrKeys[10] = {
    "__EOS_bUsesPresence",
    "__EOS_bIsDedicatedServer",
    "__EOS_bAllowJoinInProgress",
    "__EOS_bSanctionsEnabled",
    "__EOS_numPublicConnections",
    "__EOS_numPrivateConnections",
    "SERVER_IP",
    "SERVER_PORT",
    "HOST_IP",
    "HOST_PORT"
};

static uint32_t eos_SessionDetails_GetAttributeCount(void* H, void* O) {
    Log("EOS_SessionDetails_GetAttributeCount called -> 10");
    return 10;
}

static uint32_t eos_SessionDetails_GetSessionAttributeCount(void* H, void* O) {
    Log("EOS_SessionDetails_GetSessionAttributeCount called -> 10");
    return 10;
}

static EOS_EResult eos_SessionDetails_CopyAttributeByIndex(void* H, void* O, EOS_SessionDetails_Attribute** OutAttribute) {
    Log("EOS_SessionDetails_CopyAttributeByIndex called");
    if (!O || !OutAttribute) return EOS_NotFound;
    uint32_t index = ((EOS_LobbyDetails_CopyAttributeByIndexOptions*)O)->AttributeIndex;
    if (index >= 10) return EOS_NotFound;

    auto* attr = (EOS_SessionDetails_Attribute*)malloc(sizeof(EOS_SessionDetails_Attribute));
    if (!attr) return EOS_LimitExceeded;
    memset(attr, 0, sizeof(EOS_SessionDetails_Attribute));
    
    // Resolve host IP from session details handle for remote lobbies
    char resolvedIP[64] = "";
    ResolveHostIPForSessionHandle(H, resolvedIP, sizeof(resolvedIP));
    
    attr->ApiVersion = 1;
    attr->Data = CreateSessionAttributeData(g_sessionAttrKeys[index], resolvedIP);
    attr->AdvertisementType = 0;
    
    *OutAttribute = attr;
    Log("  SessionDetails attribute %d: %s", index, g_sessionAttrKeys[index]);
    return EOS_Success;
}

static EOS_EResult eos_SessionDetails_CopyAttributeByKey(void* H, void* O, EOS_SessionDetails_Attribute** OutAttribute) {
    Log("EOS_SessionDetails_CopyAttributeByKey called");
    if (!OutAttribute) return EOS_NotFound;
    *OutAttribute = nullptr;
    if (!O) return EOS_NotFound;

    const char* key = ((EOS_LobbyDetails_CopyAttributeByKeyOptions*)O)->AttrKey;
    if (!key) return EOS_NotFound;

    auto* attr = (EOS_SessionDetails_Attribute*)malloc(sizeof(EOS_SessionDetails_Attribute));
    if (!attr) return EOS_LimitExceeded;
    memset(attr, 0, sizeof(EOS_SessionDetails_Attribute));
    
    // Resolve host IP from session details handle for remote lobbies
    char resolvedIP[64] = "";
    ResolveHostIPForSessionHandle(H, resolvedIP, sizeof(resolvedIP));
    
    attr->ApiVersion = 1;
    attr->Data = CreateSessionAttributeData(key, resolvedIP);
    attr->AdvertisementType = 0;
    
    *OutAttribute = attr;
    Log("  SessionDetails attribute by key '%s' (type %d)", key, attr->Data ? attr->Data->ValueType : -1);
    return EOS_Success;
}

static EOS_EResult eos_SessionDetails_CopySessionAttributeByKey(void* H, void* O, EOS_SessionDetails_Attribute** OutAttribute) {
    Log("EOS_SessionDetails_CopySessionAttributeByKey called");
    return eos_SessionDetails_CopyAttributeByKey(H, O, OutAttribute);
}


static EOS_EResult eos_SessionDetails_CopySessionAttributeByIndex(void* H, void* O, EOS_SessionDetails_Attribute** OutAttribute) {
    Log("EOS_SessionDetails_CopySessionAttributeByIndex called");
    return eos_SessionDetails_CopyAttributeByIndex(H, O, OutAttribute);
}

static void eos_SessionDetails_Attribute_Release(EOS_SessionDetails_Attribute* Attribute) {
    Log("EOS_SessionDetails_Attribute_Release called for attr=%p", Attribute);
    if (Attribute) {
        Log("  Attribute->Data=%p", Attribute->Data);
        if (Attribute->Data) {
            Log("  Attribute->Data->Key=%p (%s)", Attribute->Data->Key, Attribute->Data->Key ? Attribute->Data->Key : "nullptr");
            if (Attribute->Data->Key) {
                free((void*)Attribute->Data->Key);
                Attribute->Data->Key = nullptr;
            }
            if (Attribute->Data->ValueType == 3 && Attribute->Data->Value.AsUtf8) {
                Log("  Attribute->Data->Value.AsUtf8=%p", Attribute->Data->Value.AsUtf8);
                free((void*)Attribute->Data->Value.AsUtf8);
                Attribute->Data->Value.AsUtf8 = nullptr;
            }
            free(Attribute->Data);
            Attribute->Data = nullptr;
        }
        free(Attribute);
    }
    Log("EOS_SessionDetails_Attribute_Release finished");
}

static EOS_EResult eos_SessionSearch_SetParameter(void* H, void* O) {
    Log("EOS_SessionSearch_SetParameter called");
    return EOS_Success;
}

static EOS_EResult eos_SessionSearch_SetTargetUserId(void* H, void* O) {
    Log("EOS_SessionSearch_SetTargetUserId called");
    return EOS_Success;
}

static EOS_EResult eos_SessionSearch_SetSessionId(void* H, void* O) {
    Log("EOS_SessionSearch_SetSessionId called");
    return EOS_Success;
}

static EOS_EResult eos_SessionSearch_SetMaxResults(void* H, void* O) {
    Log("EOS_SessionSearch_SetMaxResults called");
    return EOS_Success;
}

static void eos_SessionSearch_Release(void* H) {
    Log("EOS_SessionSearch_Release called");
}

static EOS_EResult eos_Sessions_CopyActiveSessionHandle(void* H, void* O, void** OutSessionHandle) {
    Log("EOS_Sessions_CopyActiveSessionHandle called");
    if (OutSessionHandle) *OutSessionHandle = (void*)0x2002;
    return EOS_Success;
}

struct CB_Sessions_JoinSession {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};

struct EOS_Sessions_JoinSessionOptions {
    int32_t ApiVersion;
    int32_t _pad0;
    const char* SessionName;
    void* SessionHandle;
    void* LocalUserId;
    int32_t bPresenceEnabled;
};

static void eos_Sessions_JoinSession(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_JoinSession called");
    
    // If joining a remote session (handle 0x3000+), also join the real Steam lobby
    if (O) {
        auto* opts = (EOS_Sessions_JoinSessionOptions*)O;
        void* sessionHandle = opts->SessionHandle;
        uintptr_t hVal = (uintptr_t)sessionHandle;
        
        if (hVal >= 0x3000 && (hVal - 0x3000) < (uintptr_t)g_steamLobbies.size()) {
            uint32_t idx = (uint32_t)(hVal - 0x3000);
            uint64_t steamLobbyId = g_steamLobbies[idx].steamLobbyId;
            Log("  Joining REAL Steam lobby %llu for remote session (index %u, host=%s)",
                steamLobbyId, idx, g_steamLobbies[idx].hostIP);
            
            std::thread([steamLobbyId]() {
                void* mm = GetSteamMatchmakingInterface();
                if (mm && g_pfn_MM_JoinLobby) {
                    g_pfn_MM_JoinLobby(mm, steamLobbyId);
                    if (g_pfn_SteamRunCallbacks) {
                        for (int i = 0; i < 15; i++) {
                            Sleep(20);
                            g_pfn_SteamRunCallbacks();
                        }
                    }
                    Log("  Steam JoinLobby completed for lobby %llu", steamLobbyId);
                    // Notify P2P hook: register host and other members as P2P peers
                    ReFix_NotifyLobbyID(steamLobbyId);
                } else {
                    Log("  ERROR: Cannot join Steam lobby - matchmaking interface or JoinLobby not available");
                }
            }).detach();
        }
    }
    
    CB_Sessions_JoinSession info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
}

static EOS_EResult eos_Sessions_UpdateSessionModification(void* H, void* O, void** OutSessionModificationHandle) {
    Log("EOS_Sessions_UpdateSessionModification called");
    if (OutSessionModificationHandle) {
        *OutSessionModificationHandle = HANDLE_SESSION_MODIFICATION;
    }
    return EOS_Success;
}

static EOS_NotificationId eos_Sessions_AddNotifyJoinSessionAccepted(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_AddNotifyJoinSessionAccepted called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Sessions_RemoveNotifyJoinSessionAccepted(void* H, EOS_NotificationId InId) {
    Log("EOS_Sessions_RemoveNotifyJoinSessionAccepted called (id=%llu)", InId);
}

static EOS_NotificationId eos_Sessions_AddNotifySessionInviteAccepted(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_AddNotifySessionInviteAccepted called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Sessions_RemoveNotifySessionInviteAccepted(void* H, EOS_NotificationId InId) {
    Log("EOS_Sessions_RemoveNotifySessionInviteAccepted called (id=%llu)", InId);
}

static EOS_NotificationId eos_Sessions_AddNotifySessionInviteReceived(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_AddNotifySessionInviteReceived called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Sessions_RemoveNotifySessionInviteReceived(void* H, EOS_NotificationId InId) {
    Log("EOS_Sessions_RemoveNotifySessionInviteReceived called (id=%llu)", InId);
}

static EOS_NotificationId eos_Sessions_AddNotifyLeaveSessionRequested(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_AddNotifyLeaveSessionRequested called");
    return (EOS_NotificationId)(++g_nextNotifId);
}

static void eos_Sessions_RemoveNotifyLeaveSessionRequested(void* H, EOS_NotificationId InId) {
    Log("EOS_Sessions_RemoveNotifyLeaveSessionRequested called (id=%llu)", InId);
}

struct CB_Sessions_SendInvite {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};

static void eos_Sessions_SendInvite(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_SendInvite called");
    CB_Sessions_SendInvite info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
}

struct CB_Sessions_StartSession {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};

static void eos_Sessions_StartSession(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_StartSession called");
    CB_Sessions_StartSession info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
}

struct CB_Sessions_EndSession {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
};

static void eos_Sessions_EndSession(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_EndSession called");
    CB_Sessions_EndSession info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
}

static void* s_dummyPlayerIds[1] = { (void*)FAKE_PRODUCT_USER_ID };

struct CB_Sessions_RegisterPlayers {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    const void** RegisteredPlayerIds;
    uint32_t    RegisteredPlayerIdsCount;
};

static void eos_Sessions_RegisterPlayers(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_RegisterPlayers called");
    CB_Sessions_RegisterPlayers info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.RegisteredPlayerIds = (const void**)s_dummyPlayerIds;
    info.RegisteredPlayerIdsCount = 1;
    QueueCallback(Cb, info);
}

struct CB_Sessions_UnregisterPlayers {
    EOS_EResult ResultCode;
    int32_t     _pad0;
    void*       ClientData;
    const void** UnregisteredPlayerIds;
    uint32_t    UnregisteredPlayerIdsCount;
};

static void eos_Sessions_UnregisterPlayers(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Sessions_UnregisterPlayers called");
    CB_Sessions_UnregisterPlayers info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.UnregisteredPlayerIds = (const void**)s_dummyPlayerIds;
    info.UnregisteredPlayerIdsCount = 1;
    QueueCallback(Cb, info);
}



static void InitExportNames();

static int FindExportIndex(const char* name) {
    for (int i = 0; i < EOS_FORWARD_COUNT; i++) {
        if (g_eosNames[i] && strcmp(g_eosNames[i], name) == 0) return i;
    }
    return -1;
}

static void Override(const char* name, void* proc) {
    int idx = FindExportIndex(name);
    if (idx >= 0) {
        g_eosProcs[idx] = (FARPROC)proc;
        Log("Override: %s -> idx=%d, ptr=%p", name, idx, proc);
    } else {
        Log("Override FAILED: %s not found in g_eosNames", name);
    }
}

static void SetupEmulatedFunctions() {
    InitExportNames();
    SetupTrampolines();
    for (int i = 0; i < EOS_FORWARD_COUNT; i++) {
        if (g_trampolines) {
            g_eosProcs[i] = (FARPROC)&g_trampolines[i];
        } else {
            g_eosProcs[i] = nullptr;
        }
    }

    Override("EOS_Initialize",  (void*)eos_Initialize);
    Override("EOS_Shutdown",    (void*)eos_Shutdown);
    Override("EOS_Platform_Create",  (void*)eos_Platform_Create);
    Override("EOS_Platform_Tick",    (void*)eos_Platform_Tick);
    Override("EOS_Platform_Release", (void*)eos_Platform_Release);
    Override("EOS_Platform_CheckForLauncherAndRestart", (void*)eos_Platform_CheckForLauncherAndRestart);
    Override("EOS_Platform_GetApplicationStatus",       (void*)eos_Platform_GetApplicationStatus);
    Override("EOS_Platform_SetApplicationStatus",       (void*)eos_Platform_SetApplicationStatus);
    Override("EOS_Platform_GetNetworkStatus",           (void*)eos_Platform_GetNetworkStatus);
    Override("EOS_Platform_SetNetworkStatus",           (void*)eos_Platform_SetNetworkStatus);
    Override("EOS_Platform_GetActiveCountryCode",       (void*)eos_Platform_GetActiveCountryCode);
    Override("EOS_Platform_GetActiveLocaleCode",        (void*)eos_Platform_GetActiveLocaleCode);
    Override("EOS_Platform_GetOverrideCountryCode",     (void*)eos_Platform_GetOverrideCountryCode);
    Override("EOS_Platform_GetOverrideLocaleCode",      (void*)eos_Platform_GetOverrideLocaleCode);
    Override("EOS_Platform_GetDesktopCrossplayStatus",  (void*)eos_Platform_GetDesktopCrossplayStatus);
    Override("EOS_Platform_GetIntegratedPlatformInterface", (void*)eos_GetIntegratedPlatformInterface);

    // IntegratedPlatform
    Override("EOS_IntegratedPlatform_AddNotifyUserLoginStatusChanged", (void*)eos_IntegratedPlatform_AddNotifyUserLoginStatusChanged);
    Override("EOS_IntegratedPlatform_RemoveNotifyUserLoginStatusChanged", (void*)eos_IntegratedPlatform_RemoveNotifyUserLoginStatusChanged);
    Override("EOS_IntegratedPlatform_SetUserLoginStatus", (void*)eos_IntegratedPlatform_SetUserLoginStatus);
    Override("EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer", (void*)eos_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer);
    Override("EOS_IntegratedPlatformOptionsContainer_Add", (void*)eos_IntegratedPlatformOptionsContainer_Add);
    Override("EOS_IntegratedPlatformOptionsContainer_Release", (void*)eos_IntegratedPlatformOptionsContainer_Release);

    // Platform Get*Interface
    Override("EOS_Platform_GetAuthInterface",               (void*)eos_GetAuthInterface);
    Override("EOS_Platform_GetConnectInterface",            (void*)eos_GetConnectInterface);
    Override("EOS_Platform_GetLobbyInterface",              (void*)eos_GetLobbyInterface);
    Override("EOS_Platform_GetP2PInterface",                (void*)eos_GetP2PInterface);
    Override("EOS_Platform_GetUserInfoInterface",           (void*)eos_GetUserInfoInterface);
    Override("EOS_Platform_GetFriendsInterface",            (void*)eos_GetFriendsInterface);
    Override("EOS_Platform_GetPresenceInterface",           (void*)eos_GetPresenceInterface);
    Override("EOS_Platform_GetUIInterface",                 (void*)eos_GetUIInterface);
    Override("EOS_Platform_GetSessionsInterface",           (void*)eos_GetSessionsInterface);
    Override("EOS_Platform_GetStatsInterface",              (void*)eos_GetStatsInterface);
    Override("EOS_Platform_GetAchievementsInterface",       (void*)eos_GetAchievementsInterface);
    Override("EOS_Platform_GetLeaderboardsInterface",       (void*)eos_GetLeaderboardsInterface);
    Override("EOS_Platform_GetMetricsInterface",            (void*)eos_GetMetricsInterface);
    Override("EOS_Platform_GetModsInterface",               (void*)eos_GetModsInterface);
    Override("EOS_Platform_GetEcomInterface",               (void*)eos_GetEcomInterface);
    Override("EOS_Platform_GetReportsInterface",            (void*)eos_GetReportsInterface);
    Override("EOS_Platform_GetSanctionsInterface",          (void*)eos_GetSanctionsInterface);
    Override("EOS_Platform_GetAntiCheatClientInterface",    (void*)eos_GetAntiCheatClientInterface);
    Override("EOS_Platform_GetAntiCheatServerInterface",    (void*)eos_GetAntiCheatServerInterface);
    Override("EOS_Platform_GetCustomInvitesInterface",      (void*)eos_GetCustomInvitesInterface);
    Override("EOS_Platform_GetPlayerDataStorageInterface",  (void*)eos_GetPlayerDataStorageInterface);
    Override("EOS_Platform_GetTitleStorageInterface",       (void*)eos_GetTitleStorageInterface);
    Override("EOS_Platform_GetRTCInterface",                (void*)eos_GetRTCInterface);
    Override("EOS_Platform_GetRTCAdminInterface",           (void*)eos_GetRTCAdminInterface);
    Override("EOS_Platform_GetKWSInterface",                (void*)eos_GetKWSInterface);
    Override("EOS_Platform_GetProgressionSnapshotInterface",(void*)eos_GetProgressionSnapshotInterface);

    // Auth
    Override("EOS_Auth_AddNotifyLoginStatusChanged",  (void*)eos_Auth_AddNotifyLoginStatusChanged);
    Override("EOS_Auth_CopyIdToken",                  (void*)eos_Auth_CopyIdToken);
    Override("EOS_Auth_CopyUserAuthToken",            (void*)eos_Auth_CopyUserAuthToken);
    Override("EOS_Auth_DeletePersistentAuth",         (void*)eos_Auth_DeletePersistentAuth);
    Override("EOS_Auth_GetLoggedInAccountByIndex",    (void*)eos_Auth_GetLoggedInAccountByIndex);
    Override("EOS_Auth_GetLoggedInAccountsCount",     (void*)eos_Auth_GetLoggedInAccountsCount);
    Override("EOS_Auth_GetLoginStatus",               (void*)eos_Auth_GetLoginStatus);
    Override("EOS_Auth_GetMergedAccountByIndex",      (void*)eos_Auth_GetMergedAccountByIndex);
    Override("EOS_Auth_GetMergedAccountsCount",       (void*)eos_Auth_GetMergedAccountsCount);
    Override("EOS_Auth_GetSelectedAccountId",         (void*)eos_Auth_GetSelectedAccountId);
    Override("EOS_Auth_IdToken_Release",              (void*)eos_Auth_IdToken_Release);
    Override("EOS_Auth_LinkAccount",                  (void*)eos_Auth_LinkAccount);
    Override("EOS_Auth_Login",                        (void*)eos_Auth_Login);
    Override("EOS_Auth_Logout",                       (void*)eos_Auth_Logout);
    Override("EOS_Auth_QueryIdToken",                 (void*)eos_Auth_QueryIdToken);
    Override("EOS_Auth_RemoveNotifyLoginStatusChanged",(void*)eos_Auth_RemoveNotifyLoginStatusChanged);
    Override("EOS_Auth_Token_Release",                (void*)eos_Auth_Token_Release);
    Override("EOS_Auth_VerifyIdToken",                (void*)eos_Auth_VerifyIdToken);
    Override("EOS_Auth_VerifyUserAuth",               (void*)eos_Auth_VerifyUserAuth);

    // Helpers
    Override("EOS_ByteArray_ToString",                (void*)eos_ByteArray_ToString);
    Override("EOS_ByteArray_FromString",              (void*)eos_ByteArray_FromString);

    char backendMode[32] = { 0 };
    GetPrivateProfileStringA("EOS", "Backend", "online-v2", backendMode, sizeof(backendMode), ".\\ReFix.ini");
    bool isOnlineV2 = (_stricmp(backendMode, "legacy") != 0);

    if (isOnlineV2) {
        Log("[EOS] Backend configured as online-v2: Routing Connect and Identity to modular ReFix EOS v2");
        Override("EOS_Connect_Login",                     (void*)EOS_Connect_Login);
        Override("EOS_Connect_CreateUser",                (void*)EOS_Connect_CreateUser);
        Override("EOS_Connect_LinkAccount",               (void*)EOS_Connect_LinkAccount);
        Override("EOS_Connect_CreateDeviceId",            (void*)EOS_Connect_CreateDeviceId);
        Override("EOS_Connect_DeleteDeviceId",            (void*)EOS_Connect_DeleteDeviceId);
        Override("EOS_Connect_Logout",                    (void*)EOS_Connect_Logout);
        Override("EOS_Connect_QueryExternalAccountMappings", (void*)EOS_Connect_QueryExternalAccountMappings);
        Override("EOS_Connect_GetExternalAccountMapping",  (void*)EOS_Connect_GetExternalAccountMapping);
        Override("EOS_Connect_QueryProductUserIdMappings", (void*)EOS_Connect_QueryProductUserIdMappings);
        Override("EOS_Connect_GetProductUserIdMapping",   (void*)EOS_Connect_GetProductUserIdMapping);
        Override("EOS_Connect_GetProductUserExternalAccountCount", (void*)EOS_Connect_GetProductUserExternalAccountCount);
        Override("EOS_Connect_CopyProductUserInfo",       (void*)EOS_Connect_CopyProductUserInfo);
        Override("EOS_Connect_CopyProductUserExternalAccountByIndex", (void*)EOS_Connect_CopyProductUserExternalAccountByIndex);
        Override("EOS_Connect_CopyProductUserExternalAccountByAccountType", (void*)EOS_Connect_CopyProductUserExternalAccountByAccountType);
        Override("EOS_Connect_CopyProductUserExternalAccountByAccountId", (void*)EOS_Connect_CopyProductUserExternalAccountByAccountId);
        Override("EOS_Connect_ExternalAccountInfo_Release", (void*)EOS_Connect_ExternalAccountInfo_Release);
        Override("EOS_Connect_GetLoggedInUserByIndex",    (void*)EOS_Connect_GetLoggedInUserByIndex);
        Override("EOS_Connect_GetLoggedInUsersCount",     (void*)EOS_Connect_GetLoggedInUsersCount);
        Override("EOS_Connect_GetLoginStatus",            (void*)EOS_Connect_GetLoginStatus);
        Override("EOS_Connect_AddNotifyLoginStatusChanged", (void*)EOS_Connect_AddNotifyLoginStatusChanged);
        Override("EOS_Connect_RemoveNotifyLoginStatusChanged", (void*)EOS_Connect_RemoveNotifyLoginStatusChanged);
        Override("EOS_Connect_AddNotifyAuthExpiration",    (void*)EOS_Connect_AddNotifyAuthExpiration);
        Override("EOS_Connect_RemoveNotifyAuthExpiration", (void*)EOS_Connect_RemoveNotifyAuthExpiration);

        Override("EOS_ProductUserId_IsValid",   (void*)EOS_ProductUserId_IsValid);
        Override("EOS_EpicAccountId_IsValid",   (void*)EOS_EpicAccountId_IsValid);
        Override("EOS_ProductUserId_ToString",  (void*)EOS_ProductUserId_ToString);
        Override("EOS_EpicAccountId_ToString",  (void*)EOS_EpicAccountId_ToString);
        Override("EOS_ProductUserId_FromString", (void*)EOS_ProductUserId_FromString);
        Override("EOS_EpicAccountId_FromString", (void*)EOS_EpicAccountId_FromString);
    }

    // Connect
    Override("EOS_Connect_AddNotifyAuthExpiration",   (void*)eos_Connect_AddNotifyAuthExpiration);
    Override("EOS_Connect_AddNotifyLoginStatusChanged",(void*)eos_Connect_AddNotifyLoginStatusChanged);
    Override("EOS_Connect_CopyIdToken",              (void*)eos_Connect_CopyIdToken);
    Override("EOS_Connect_CopyProductUserExternalAccountByAccountId",   (void*)eos_Connect_CopyProductUserExternalAccountByAccountId);
    Override("EOS_Connect_CopyProductUserExternalAccountByAccountType", (void*)eos_Connect_CopyProductUserExternalAccountByAccountType);
    Override("EOS_Connect_CopyProductUserExternalAccountByIndex",       (void*)eos_Connect_CopyProductUserExternalAccountByIndex);
    Override("EOS_Connect_CopyProductUserInfo",      (void*)eos_Connect_CopyProductUserInfo);
    Override("EOS_Connect_CreateDeviceId",           (void*)eos_Connect_CreateDeviceId);
    Override("EOS_Connect_CreateUser",               (void*)eos_Connect_CreateUser);
    Override("EOS_Connect_DeleteDeviceId",           (void*)eos_Connect_DeleteDeviceId);
    Override("EOS_Connect_ExternalAccountInfo_Release",(void*)eos_Connect_ExternalAccountInfo_Release);
    Override("EOS_Connect_GetExternalAccountMapping", (void*)eos_Connect_GetExternalAccountMapping);
    Override("EOS_Connect_GetLoggedInUserByIndex",   (void*)eos_Connect_GetLoggedInUserByIndex);
    Override("EOS_Connect_GetLoggedInUsersCount",    (void*)eos_Connect_GetLoggedInUsersCount);
    Override("EOS_Connect_GetLoginStatus",           (void*)eos_Connect_GetLoginStatus);
    Override("EOS_Connect_GetProductUserExternalAccountCount",(void*)eos_Connect_GetProductUserExternalAccountCount);
    Override("EOS_Connect_GetProductUserIdMapping",  (void*)eos_Connect_GetProductUserIdMapping);
    Override("EOS_Connect_IdToken_Release",          (void*)eos_Connect_IdToken_Release);
    Override("EOS_Connect_LinkAccount",              (void*)eos_Connect_LinkAccount);
    Override("EOS_Connect_Login",                    (void*)eos_Connect_Login);
    Override("EOS_Connect_Logout",                   (void*)eos_Connect_Logout);
    Override("EOS_Connect_QueryExternalAccountMappings",(void*)eos_Connect_QueryExternalAccountMappings);
    Override("EOS_Connect_QueryProductUserIdMappings",(void*)eos_Connect_QueryProductUserIdMappings);
    Override("EOS_Connect_RemoveNotifyAuthExpiration",(void*)eos_Connect_RemoveNotifyAuthExpiration);
    Override("EOS_Connect_RemoveNotifyLoginStatusChanged",(void*)eos_Connect_RemoveNotifyLoginStatusChanged);
    Override("EOS_Connect_TransferDeviceIdAccount",  (void*)eos_Connect_TransferDeviceIdAccount);
    Override("EOS_Connect_UnlinkAccount",            (void*)eos_Connect_UnlinkAccount);
    Override("EOS_Connect_VerifyIdToken",            (void*)eos_Connect_VerifyIdToken);

    // ID Validation
    Override("EOS_ProductUserId_IsValid",  (void*)eos_ProductUserId_IsValid);
    Override("EOS_EpicAccountId_IsValid",  (void*)eos_EpicAccountId_IsValid);
    Override("EOS_ProductUserId_ToString", (void*)eos_ProductUserId_ToString);
    Override("EOS_EpicAccountId_ToString", (void*)eos_EpicAccountId_ToString);
    Override("EOS_ProductUserId_FromString",(void*)eos_ProductUserId_FromString);
    Override("EOS_EpicAccountId_FromString",(void*)eos_EpicAccountId_FromString);

    // Helpers
    Override("EOS_EResult_ToString",           (void*)eos_EResult_ToString);
    Override("EOS_EResult_IsOperationComplete", (void*)eos_EResult_IsOperationComplete);
    Override("EOS_Logging_SetCallback",        (void*)eos_Logging_SetCallback);
    Override("EOS_Logging_SetLogLevel",        (void*)eos_Logging_SetLogLevel);

    // UserInfo
    Override("EOS_UserInfo_QueryUserInfo",           (void*)eos_UserInfo_QueryUserInfo);
    Override("EOS_UserInfo_CopyUserInfo",            (void*)eos_UserInfo_CopyUserInfo);
    Override("EOS_UserInfo_CopyBestDisplayName",     (void*)eos_UserInfo_CopyBestDisplayName);
    Override("EOS_UserInfo_Release",                 (void*)eos_UserInfo_Release);
    Override("EOS_UserInfo_BestDisplayName_Release",  (void*)eos_UserInfo_BestDisplayName_Release);

    // Friends
    Override("EOS_Friends_QueryFriends",             (void*)eos_Friends_QueryFriends);
    Override("EOS_Friends_GetFriendsCount",          (void*)eos_Friends_GetFriendsCount);
    Override("EOS_Friends_GetFriendAtIndex",          (void*)eos_Friends_GetFriendAtIndex);

    // PlayerDataStorage
    Override("EOS_PlayerDataStorage_QueryFileList",           (void*)eos_PlayerDataStorage_QueryFileList);
    Override("EOS_PlayerDataStorage_GetFileMetadataCount",    (void*)eos_PlayerDataStorage_GetFileMetadataCount);
    Override("EOS_PlayerDataStorage_CopyFileMetadataAtIndex", (void*)eos_PlayerDataStorage_CopyFileMetadataAtIndex);
    Override("EOS_PlayerDataStorage_CopyFileMetadataByFilename", (void*)eos_PlayerDataStorage_CopyFileMetadataByFilename);
    Override("EOS_PlayerDataStorage_FileMetadata_Release",     (void*)eos_PlayerDataStorage_FileMetadata_Release);
    Override("EOS_PlayerDataStorage_ReadFile",                 (void*)eos_PlayerDataStorage_ReadFile);
    Override("EOS_PlayerDataStorage_WriteFile",                (void*)eos_PlayerDataStorage_WriteFile);
    Override("EOS_PlayerDataStorage_DuplicateFile",            (void*)eos_PlayerDataStorage_DuplicateFile);
    Override("EOS_PlayerDataStorage_DeleteFile",               (void*)eos_PlayerDataStorage_DeleteFile);

    // Lobby
    Override("EOS_Lobby_CreateLobby",                         (void*)eos_Lobby_CreateLobby);
    Override("EOS_Lobby_DestroyLobby",                        (void*)eos_Lobby_DestroyLobby);
    Override("EOS_Lobby_UpdateLobby",                         (void*)eos_Lobby_UpdateLobby);
    Override("EOS_LobbySearch_Find",                          (void*)eos_LobbySearch_Find);
    Override("EOS_LobbySearch_GetSearchResultCount",          (void*)eos_LobbySearch_GetSearchResultCount);
    Override("EOS_LobbySearch_CopySearchResultByIndex",       (void*)eos_LobbySearch_CopySearchResultByIndex);
    Override("EOS_LobbySearch_SetParameter",                  (void*)eos_LobbySearch_SetParameter);
    Override("EOS_LobbySearch_Release",                       (void*)eos_LobbySearch_Release);
    Override("EOS_Lobby_JoinLobby",                           (void*)eos_Lobby_JoinLobby);
    Override("EOS_Lobby_CreateLobbySearch",                   (void*)eos_Lobby_CreateLobbySearch);
    Override("EOS_Lobby_CreateLobbyModification",              (void*)eos_Lobby_CreateLobbyModification);
    Override("EOS_Lobby_UpdateLobbyModification",              (void*)eos_Lobby_UpdateLobbyModification);
    Override("EOS_LobbyModification_SetPermissionLevel",       (void*)eos_LobbyModification_SetPermissionLevel);
    Override("EOS_LobbyModification_SetMaxMembers",           (void*)eos_LobbyModification_SetMaxMembers);
    Override("EOS_LobbyModification_SetBucketId",               (void*)eos_LobbyModification_SetBucketId);
    Override("EOS_LobbyModification_SetInvitesAllowed",       (void*)eos_LobbyModification_SetInvitesAllowed);
    Override("EOS_LobbyModification_AddAttribute",            (void*)eos_LobbyModification_AddAttribute);
    Override("EOS_LobbyModification_AddMemberAttribute",      (void*)eos_LobbyModification_AddMemberAttribute);
    Override("EOS_LobbyModification_RemoveAttribute",         (void*)eos_LobbyModification_RemoveAttribute);
    Override("EOS_LobbyModification_RemoveMemberAttribute",   (void*)eos_LobbyModification_RemoveMemberAttribute);
    Override("EOS_LobbyModification_Release",                 (void*)eos_LobbyModification_Release);
    Override("EOS_Lobby_CopyLobbyDetailsHandle",              (void*)eos_Lobby_CopyLobbyDetailsHandle);
    Override("EOS_Lobby_CopyLobbyDetailsHandleByInviteId",    (void*)eos_Lobby_CopyLobbyDetailsHandleByInviteId);
    Override("EOS_Lobby_CopyLobbyDetailsHandleByUiEventId",   (void*)eos_Lobby_CopyLobbyDetailsHandleByUiEventId);
    Override("EOS_LobbyDetails_CopyInfo",                      (void*)eos_LobbyDetails_CopyInfo);
    Override("EOS_LobbyDetails_GetAttributeCount",             (void*)eos_LobbyDetails_GetAttributeCount);
    Override("EOS_LobbyDetails_CopyAttributeByIndex",          (void*)eos_LobbyDetails_CopyAttributeByIndex);
    Override("EOS_LobbyDetails_CopyAttributeByKey",            (void*)eos_LobbyDetails_CopyAttributeByKey);
    Override("EOS_Lobby_Attribute_Release",                    (void*)eos_Lobby_Attribute_Release);
    Override("EOS_LobbyDetails_GetMemberCount",                (void*)eos_LobbyDetails_GetMemberCount);
    Override("EOS_LobbyDetails_GetMemberAttributeCount",       (void*)eos_LobbyDetails_GetMemberAttributeCount);
    Override("EOS_LobbyDetails_CopyMemberAttributeByIndex",    (void*)eos_LobbyDetails_CopyMemberAttributeByIndex);
    Override("EOS_LobbyDetails_CopyMemberAttributeByKey",      (void*)eos_LobbyDetails_CopyMemberAttributeByKey);
    Override("EOS_LobbyDetails_Info_Release",                  (void*)eos_LobbyDetails_Info_Release);
    Override("EOS_LobbyDetails_Release",                       (void*)eos_LobbyDetails_Release);
    Override("EOS_LobbyDetails_GetMemberByIndex",              (void*)eos_LobbyDetails_GetMemberByIndex);
    Override("EOS_LobbyDetails_GetLobbyOwner",                 (void*)eos_LobbyDetails_GetLobbyOwner);
    Override("EOS_Lobby_AddNotifyJoinLobbyAccepted",          (void*)eos_Lobby_AddNotifyJoinLobbyAccepted);
    Override("EOS_Lobby_RemoveNotifyJoinLobbyAccepted",       (void*)eos_Lobby_RemoveNotifyJoinLobbyAccepted);
    Override("EOS_Lobby_AddNotifyLeaveLobbyRequested",        (void*)eos_Lobby_AddNotifyLeaveLobbyRequested);
    Override("EOS_Lobby_RemoveNotifyLeaveLobbyRequested",     (void*)eos_Lobby_RemoveNotifyLeaveLobbyRequested);
    Override("EOS_Lobby_AddNotifyLobbyInviteAccepted",       (void*)eos_Lobby_AddNotifyLobbyInviteAccepted);
    Override("EOS_Lobby_RemoveNotifyLobbyInviteAccepted",    (void*)eos_Lobby_RemoveNotifyLobbyInviteAccepted);
    Override("EOS_Lobby_AddNotifyLobbyInviteReceived",       (void*)eos_Lobby_AddNotifyLobbyInviteReceived);
    Override("EOS_Lobby_RemoveNotifyLobbyInviteReceived",    (void*)eos_Lobby_RemoveNotifyLobbyInviteReceived);
    Override("EOS_Lobby_AddNotifyLobbyInviteRejected",       (void*)eos_Lobby_AddNotifyLobbyInviteRejected);
    Override("EOS_Lobby_RemoveNotifyLobbyInviteRejected",    (void*)eos_Lobby_RemoveNotifyLobbyInviteRejected);
    Override("EOS_Lobby_AddNotifyLobbyMemberStatusReceived", (void*)eos_Lobby_AddNotifyLobbyMemberStatusReceived);
    Override("EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived",(void*)eos_Lobby_RemoveNotifyLobbyMemberStatusReceived);
    Override("EOS_Lobby_AddNotifyLobbyMemberUpdateReceived", (void*)eos_Lobby_AddNotifyLobbyMemberUpdateReceived);
    Override("EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived",(void*)eos_Lobby_RemoveNotifyLobbyMemberUpdateReceived);
    Override("EOS_Lobby_AddNotifyLobbyUpdateReceived",       (void*)eos_Lobby_AddNotifyLobbyUpdateReceived);
    Override("EOS_Lobby_RemoveNotifyLobbyUpdateReceived",    (void*)eos_Lobby_RemoveNotifyLobbyUpdateReceived);
    Override("EOS_Lobby_SendInvite",                          (void*)eos_Lobby_SendInvite);

    // Sessions (RedpointEOS)
    Override("EOS_Sessions_CreateSessionModification",        (void*)eos_Sessions_CreateSessionModification);
    Override("EOS_Sessions_UpdateSessionModification",        (void*)eos_Sessions_UpdateSessionModification);
    Override("EOS_Sessions_UpdateSession",                    (void*)eos_Sessions_UpdateSession);
    Override("EOS_Sessions_CreateSessionSearch",              (void*)eos_Sessions_CreateSessionSearch);
    Override("EOS_SessionSearch_Find",                        (void*)eos_SessionSearch_Find);
    Override("EOS_SessionSearch_GetSearchResultCount",        (void*)eos_SessionSearch_GetSearchResultCount);
    Override("EOS_SessionSearch_CopySearchResultByIndex",     (void*)eos_SessionSearch_CopySearchResultByIndex);
    Override("EOS_Sessions_JoinSession",                      (void*)eos_Sessions_JoinSession);
    Override("EOS_Sessions_DestroySession",                   (void*)eos_Sessions_DestroySession);
    Override("EOS_Sessions_AddNotifyJoinSessionAccepted",     (void*)eos_Sessions_AddNotifyJoinSessionAccepted);
    Override("EOS_Sessions_RemoveNotifyJoinSessionAccepted",  (void*)eos_Sessions_RemoveNotifyJoinSessionAccepted);
    Override("EOS_Sessions_AddNotifySessionInviteAccepted",   (void*)eos_Sessions_AddNotifySessionInviteAccepted);
    Override("EOS_Sessions_RemoveNotifySessionInviteAccepted",(void*)eos_Sessions_RemoveNotifySessionInviteAccepted);
    Override("EOS_Sessions_AddNotifySessionInviteReceived",   (void*)eos_Sessions_AddNotifySessionInviteReceived);
    Override("EOS_Sessions_RemoveNotifySessionInviteReceived",(void*)eos_Sessions_RemoveNotifySessionInviteReceived);
    Override("EOS_Sessions_AddNotifyLeaveSessionRequested",   (void*)eos_Sessions_AddNotifyLeaveSessionRequested);
    Override("EOS_Sessions_RemoveNotifyLeaveSessionRequested",(void*)eos_Sessions_RemoveNotifyLeaveSessionRequested);
    Override("EOS_Sessions_SendInvite",                      (void*)eos_Sessions_SendInvite);
    Override("EOS_Sessions_StartSession",                     (void*)eos_Sessions_StartSession);
    Override("EOS_Sessions_EndSession",                       (void*)eos_Sessions_EndSession);
    Override("EOS_Sessions_RegisterPlayers",                 (void*)eos_Sessions_RegisterPlayers);
    Override("EOS_Sessions_UnregisterPlayers",               (void*)eos_Sessions_UnregisterPlayers);

    // P2P (Non-blocking Stubs to prevent UE Infinite Loop Freeze)
    Override("EOS_P2P_GetNextReceivedPacketSize",             (void*)eos_P2P_GetNextReceivedPacketSize);
    Override("EOS_P2P_ReceivePacket",                        (void*)eos_P2P_ReceivePacket);
    Override("EOS_P2P_SendPacket",                           (void*)eos_P2P_SendPacket);
    Override("EOS_P2P_AddNotifyPeerConnectionRequest",        (void*)eos_P2P_AddNotifyPeerConnectionRequest);
    Override("EOS_P2P_RemoveNotifyPeerConnectionRequest",     (void*)eos_P2P_RemoveNotifyPeerConnectionRequest);
    Override("EOS_P2P_AddNotifyPeerConnectionClosed",         (void*)eos_P2P_AddNotifyPeerConnectionClosed);
    Override("EOS_P2P_RemoveNotifyPeerConnectionClosed",      (void*)eos_P2P_RemoveNotifyPeerConnectionClosed);
    Override("EOS_P2P_AcceptConnection",                     (void*)eos_P2P_AcceptConnection);
    Override("EOS_P2P_CloseConnection",                      (void*)eos_P2P_CloseConnection);
    Override("EOS_P2P_CloseConnections",                     (void*)eos_P2P_CloseConnections);
    Override("EOS_P2P_SetPacketQueueSize",                   (void*)eos_P2P_SetPacketQueueSize);
    Override("EOS_P2P_SetPortRange",                         (void*)eos_P2P_SetPortRange);
    Override("EOS_P2P_SetRelayControl",                      (void*)eos_P2P_SetRelayControl);
    Override("EOS_Sessions_CopyActiveSessionHandle",          (void*)eos_Sessions_CopyActiveSessionHandle);

    Override("EOS_SessionModification_SetBucketId",           (void*)eos_SessionModification_SetBucketId);
    Override("EOS_SessionModification_SetHostAddress",        (void*)eos_SessionModification_SetHostAddress);
    Override("EOS_SessionModification_SetPermissionLevel",   (void*)eos_SessionModification_SetPermissionLevel);
    Override("EOS_SessionModification_SetMaxPlayers",        (void*)eos_SessionModification_SetMaxPlayers);
    Override("EOS_SessionModification_SetJoinInProgressAllowed",(void*)eos_SessionModification_SetJoinInProgressAllowed);
    Override("EOS_SessionModification_SetInvitesAllowed",   (void*)eos_SessionModification_SetInvitesAllowed);
    Override("EOS_SessionModification_AddAttribute",        (void*)eos_SessionModification_AddAttribute);
    Override("EOS_SessionModification_RemoveAttribute",     (void*)eos_SessionModification_RemoveAttribute);
    Override("EOS_SessionModification_Release",             (void*)eos_SessionModification_Release);

    Override("EOS_SessionDetails_CopyInfo",                   (void*)eos_SessionDetails_CopyInfo);
    Override("EOS_SessionDetails_Info_Release",              (void*)eos_SessionDetails_Info_Release);
    Override("EOS_SessionDetails_Release",                   (void*)eos_SessionDetails_Release);
    Override("EOS_SessionDetails_GetAttributeCount",         (void*)eos_SessionDetails_GetAttributeCount);
    Override("EOS_SessionDetails_CopyAttributeByIndex",      (void*)eos_SessionDetails_CopyAttributeByIndex);
    Override("EOS_SessionDetails_CopyAttributeByKey",        (void*)eos_SessionDetails_CopyAttributeByKey);
    Override("EOS_SessionDetails_CopySessionAttributeByKey",  (void*)eos_SessionDetails_CopySessionAttributeByKey);
    Override("EOS_SessionDetails_GetSessionAttributeCount",   (void*)eos_SessionDetails_GetSessionAttributeCount);
    Override("EOS_SessionDetails_CopySessionAttributeByIndex", (void*)eos_SessionDetails_CopySessionAttributeByIndex);
    Override("EOS_SessionDetails_Attribute_Release",        (void*)eos_SessionDetails_Attribute_Release);

    Override("EOS_SessionSearch_SetParameter",              (void*)eos_SessionSearch_SetParameter);
    Override("EOS_SessionSearch_SetTargetUserId",           (void*)eos_SessionSearch_SetTargetUserId);
    Override("EOS_SessionSearch_SetSessionId",              (void*)eos_SessionSearch_SetSessionId);
    Override("EOS_SessionSearch_SetMaxResults",             (void*)eos_SessionSearch_SetMaxResults);
    Override("EOS_SessionSearch_Release",                   (void*)eos_SessionSearch_Release);
}

// =============================================================================
// ReFix marker
// =============================================================================
extern "C" __declspec(dllexport) int ReFix() {
    return 1;
}

// ===============================================================// CONFIGURATION
// =============================================================================
static void LoadConfig() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string iniPath(exePath);
    size_t pos = iniPath.find_last_of("\\/");
    if (pos != std::string::npos) iniPath = iniPath.substr(0, pos + 1) + "ReFix.ini";
    
    // Priority 1: Check environment variable REFIX_USERNAME
    char envBuf[128] = {0};
    if (GetEnvironmentVariableA("REFIX_USERNAME", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
        strcpy_s(g_userName, sizeof(g_userName), envBuf);
    }
    
    // Priority 2: Check ReFix.ini [User] Name
    if (g_userName[0] == '\0' || strcmp(g_userName, "ReFix User") == 0) {
        char iniBuf[128] = {0};
        GetPrivateProfileStringA("User", "Name", "", iniBuf, sizeof(iniBuf), iniPath.c_str());
        if (iniBuf[0] != '\0') {
            strcpy_s(g_userName, sizeof(g_userName), iniBuf);
        }
    }

    // Priority 3: Default fallback "Player" (no Windows OS account name like Valen!)
    if (g_userName[0] == '\0' || strcmp(g_userName, "ReFix User") == 0) {
        strcpy_s(g_userName, sizeof(g_userName), "Player");
    }

    // Read SteamId from INI
    GetPrivateProfileStringA("User", "SteamId", "",
        g_productUserIdStr, sizeof(g_productUserIdStr), iniPath.c_str());
    
    // If INI SteamId is empty, try env var from steam_proxy
    if (g_productUserIdStr[0] == '\0') {
        char envId[64] = {0};
        if (GetEnvironmentVariableA("REFIX_STEAM_ID", envId, sizeof(envId)) > 0 && envId[0] != '\0') {
            strcpy_s(g_productUserIdStr, sizeof(g_productUserIdStr), envId);
        }
    }
    
    // Final fallback
    if (g_productUserIdStr[0] == '\0') {
        strcpy_s(g_productUserIdStr, sizeof(g_productUserIdStr), "76561197960287930");
    }
}

// =============================================================================
// REFIX IN-GAME DEBUG CONSOLE TOGGLE (VK_INSERT / VK_F1)
// =============================================================================
static bool g_ConsoleAllocated = false;
static bool g_ConsoleVisible = false;
static DWORD g_LastToggleTime = 0;

static void ToggleConsoleWindow() {
    if (!g_ConsoleAllocated) {
        if (AllocConsole()) {
            FILE* fp;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            freopen_s(&fp, "CONIN$", "r", stdin);

            SetConsoleTitleA("ReFix EOS Debug Console");
            printf("====================================================================\n");
            printf("           ReFix Universal Multiplatform EOS Emulator (LAN)         \n");
            printf("====================================================================\n");
            printf("[INFO] Process ID: %lu\n", GetCurrentProcessId());
            printf("[INFO] Press INSERT or F1 to Show/Hide this Console Window\n");
            printf("====================================================================\n\n");
            fflush(stdout);

            g_ConsoleAllocated = true;
            g_ConsoleVisible = true;
        }
    } else {
        HWND hConsole = GetConsoleWindow();
        if (hConsole) {
            g_ConsoleVisible = !g_ConsoleVisible;
            ShowWindow(hConsole, g_ConsoleVisible ? SW_SHOW : SW_HIDE);
            if (g_ConsoleVisible) {
                SetForegroundWindow(hConsole);
            }
        }
    }
}

static DWORD WINAPI ConsoleHotkeyThread(LPVOID lpParam) {
    while (true) {
        Sleep(50);
        DWORD now = GetTickCount();
        if (now - g_LastToggleTime > 300) {
            bool keyInsertPressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
            bool keyF1Pressed     = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
            if (keyInsertPressed || keyF1Pressed) {
                g_LastToggleTime = now;
                ToggleConsoleWindow();
            }
        }
    }
    return 0;
}

static bool g_hotkeyStarted = false;
static void StartConsoleHotkeyMonitor() {
    if (g_hotkeyStarted) return;
    g_hotkeyStarted = true;
    CreateThread(NULL, 0, ConsoleHotkeyThread, NULL, 0, NULL);
}

// =============================================================================
// DLL ENTRY
// =============================================================================
static void InitExportNames();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            InitializeCriticalSection(&g_callbackCS);
            g_csInitialized = true;
            LoadConfig();
            LoadGameFilterConfig();
            InitExportNames();
            SetupEmulatedFunctions();
            break;
        case DLL_PROCESS_DETACH:
            if (g_csInitialized && !lpReserved) {
                DeleteCriticalSection(&g_callbackCS);
                g_csInitialized = false;
            }
            break;
    }
    return TRUE;
}

// AUTO-GENERATED: InitExportNames
static void InitExportNames() {
    g_eosNames[0] = "EOS_Achievements_AddNotifyAchievementsUnlocked";
    g_eosNames[1] = "EOS_Achievements_AddNotifyAchievementsUnlockedV2";
    g_eosNames[2] = "EOS_Achievements_CopyAchievementDefinitionByAchievementId";
    g_eosNames[3] = "EOS_Achievements_CopyAchievementDefinitionByIndex";
    g_eosNames[4] = "EOS_Achievements_CopyAchievementDefinitionV2ByAchievementId";
    g_eosNames[5] = "EOS_Achievements_CopyAchievementDefinitionV2ByIndex";
    g_eosNames[6] = "EOS_Achievements_CopyPlayerAchievementByAchievementId";
    g_eosNames[7] = "EOS_Achievements_CopyPlayerAchievementByIndex";
    g_eosNames[8] = "EOS_Achievements_CopyUnlockedAchievementByAchievementId";
    g_eosNames[9] = "EOS_Achievements_CopyUnlockedAchievementByIndex";
    g_eosNames[10] = "EOS_Achievements_DefinitionV2_Release";
    g_eosNames[11] = "EOS_Achievements_Definition_Release";
    g_eosNames[12] = "EOS_Achievements_GetAchievementDefinitionCount";
    g_eosNames[13] = "EOS_Achievements_GetPlayerAchievementCount";
    g_eosNames[14] = "EOS_Achievements_GetUnlockedAchievementCount";
    g_eosNames[15] = "EOS_Achievements_PlayerAchievement_Release";
    g_eosNames[16] = "EOS_Achievements_QueryDefinitions";
    g_eosNames[17] = "EOS_Achievements_QueryPlayerAchievements";
    g_eosNames[18] = "EOS_Achievements_RemoveNotifyAchievementsUnlocked";
    g_eosNames[19] = "EOS_Achievements_UnlockAchievements";
    g_eosNames[20] = "EOS_Achievements_UnlockedAchievement_Release";
    g_eosNames[21] = "EOS_ActiveSession_CopyInfo";
    g_eosNames[22] = "EOS_ActiveSession_GetRegisteredPlayerByIndex";
    g_eosNames[23] = "EOS_ActiveSession_GetRegisteredPlayerCount";
    g_eosNames[24] = "EOS_ActiveSession_Info_Release";
    g_eosNames[25] = "EOS_ActiveSession_Release";
    g_eosNames[26] = "EOS_AntiCheatClient_AddExternalIntegrityCatalog";
    g_eosNames[27] = "EOS_AntiCheatClient_AddNotifyClientIntegrityViolated";
    g_eosNames[28] = "EOS_AntiCheatClient_AddNotifyMessageToPeer";
    g_eosNames[29] = "EOS_AntiCheatClient_AddNotifyMessageToServer";
    g_eosNames[30] = "EOS_AntiCheatClient_AddNotifyPeerActionRequired";
    g_eosNames[31] = "EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged";
    g_eosNames[32] = "EOS_AntiCheatClient_BeginSession";
    g_eosNames[33] = "EOS_AntiCheatClient_EndSession";
    g_eosNames[34] = "EOS_AntiCheatClient_GetModuleBuildId";
    g_eosNames[35] = "EOS_AntiCheatClient_GetProtectMessageOutputLength";
    g_eosNames[36] = "EOS_AntiCheatClient_PollStatus";
    g_eosNames[37] = "EOS_AntiCheatClient_ProtectMessage";
    g_eosNames[38] = "EOS_AntiCheatClient_ReceiveMessageFromPeer";
    g_eosNames[39] = "EOS_AntiCheatClient_ReceiveMessageFromServer";
    g_eosNames[40] = "EOS_AntiCheatClient_RegisterPeer";
    g_eosNames[41] = "EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated";
    g_eosNames[42] = "EOS_AntiCheatClient_RemoveNotifyMessageToPeer";
    g_eosNames[43] = "EOS_AntiCheatClient_RemoveNotifyMessageToServer";
    g_eosNames[44] = "EOS_AntiCheatClient_RemoveNotifyPeerActionRequired";
    g_eosNames[45] = "EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged";
    g_eosNames[46] = "EOS_AntiCheatClient_Reserved01";
    g_eosNames[47] = "EOS_AntiCheatClient_Reserved02";
    g_eosNames[48] = "EOS_AntiCheatClient_UnprotectMessage";
    g_eosNames[49] = "EOS_AntiCheatClient_UnregisterPeer";
    g_eosNames[50] = "EOS_AntiCheatServer_AddNotifyClientActionRequired";
    g_eosNames[51] = "EOS_AntiCheatServer_AddNotifyClientAuthStatusChanged";
    g_eosNames[52] = "EOS_AntiCheatServer_AddNotifyMessageToClient";
    g_eosNames[53] = "EOS_AntiCheatServer_BeginSession";
    g_eosNames[54] = "EOS_AntiCheatServer_EndSession";
    g_eosNames[55] = "EOS_AntiCheatServer_GetProtectMessageOutputLength";
    g_eosNames[56] = "EOS_AntiCheatServer_LogEvent";
    g_eosNames[57] = "EOS_AntiCheatServer_LogGameRoundEnd";
    g_eosNames[58] = "EOS_AntiCheatServer_LogGameRoundStart";
    g_eosNames[59] = "EOS_AntiCheatServer_LogPlayerDespawn";
    g_eosNames[60] = "EOS_AntiCheatServer_LogPlayerRevive";
    g_eosNames[61] = "EOS_AntiCheatServer_LogPlayerSpawn";
    g_eosNames[62] = "EOS_AntiCheatServer_LogPlayerTakeDamage";
    g_eosNames[63] = "EOS_AntiCheatServer_LogPlayerTick";
    g_eosNames[64] = "EOS_AntiCheatServer_LogPlayerUseAbility";
    g_eosNames[65] = "EOS_AntiCheatServer_LogPlayerUseWeapon";
    g_eosNames[66] = "EOS_AntiCheatServer_ProtectMessage";
    g_eosNames[67] = "EOS_AntiCheatServer_ReceiveMessageFromClient";
    g_eosNames[68] = "EOS_AntiCheatServer_RegisterClient";
    g_eosNames[69] = "EOS_AntiCheatServer_RegisterEvent";
    g_eosNames[70] = "EOS_AntiCheatServer_RemoveNotifyClientActionRequired";
    g_eosNames[71] = "EOS_AntiCheatServer_RemoveNotifyClientAuthStatusChanged";
    g_eosNames[72] = "EOS_AntiCheatServer_RemoveNotifyMessageToClient";
    g_eosNames[73] = "EOS_AntiCheatServer_SetClientDetails";
    g_eosNames[74] = "EOS_AntiCheatServer_SetClientNetworkState";
    g_eosNames[75] = "EOS_AntiCheatServer_SetGameSessionId";
    g_eosNames[76] = "EOS_AntiCheatServer_UnprotectMessage";
    g_eosNames[77] = "EOS_AntiCheatServer_UnregisterClient";
    g_eosNames[78] = "EOS_Audio_CreateNewInputStream";
    g_eosNames[79] = "EOS_Audio_CreateNewOutputStream";
    g_eosNames[80] = "EOS_Audio_DestroyInputStream";
    g_eosNames[81] = "EOS_Audio_DestroyOutputStream";
    g_eosNames[82] = "EOS_Audio_EnableCommunicationsModeOutputDevices";
    g_eosNames[83] = "EOS_Audio_GetInputDeviceInfo";
    g_eosNames[84] = "EOS_Audio_GetInputStreamInfo";
    g_eosNames[85] = "EOS_Audio_GetOutputDeviceInfo";
    g_eosNames[86] = "EOS_Audio_GetOutputStreamInfo";
    g_eosNames[87] = "EOS_Audio_IsInputStreamDeviceDisconnected";
    g_eosNames[88] = "EOS_Audio_IsInputStreamSilent";
    g_eosNames[89] = "EOS_Audio_QueryInputDevices";
    g_eosNames[90] = "EOS_Audio_QueryOutputDevices";
    g_eosNames[91] = "EOS_Audio_RegisterUser";
    g_eosNames[92] = "EOS_Audio_RemoveNotifyDevicesChanged";
    g_eosNames[93] = "EOS_Audio_SetFeatureEnabledForInputStream";
    g_eosNames[94] = "EOS_Audio_SetNotifyDevicesChanged";
    g_eosNames[95] = "EOS_Audio_StartInputStream";
    g_eosNames[96] = "EOS_Audio_StartOutputStream";
    g_eosNames[97] = "EOS_Audio_StopInputStream";
    g_eosNames[98] = "EOS_Audio_StopOutputStream";
    g_eosNames[99] = "EOS_Audio_UnregisterUser";
    g_eosNames[100] = "EOS_Auth_AddNotifyLoginStatusChanged";
    g_eosNames[101] = "EOS_Auth_CopyIdToken";
    g_eosNames[102] = "EOS_Auth_CopyUserAuthToken";
    g_eosNames[103] = "EOS_Auth_DeletePersistentAuth";
    g_eosNames[104] = "EOS_Auth_GetLoggedInAccountByIndex";
    g_eosNames[105] = "EOS_Auth_GetLoggedInAccountsCount";
    g_eosNames[106] = "EOS_Auth_GetLoginStatus";
    g_eosNames[107] = "EOS_Auth_GetMergedAccountByIndex";
    g_eosNames[108] = "EOS_Auth_GetMergedAccountsCount";
    g_eosNames[109] = "EOS_Auth_GetSelectedAccountId";
    g_eosNames[110] = "EOS_Auth_IdToken_Release";
    g_eosNames[111] = "EOS_Auth_LinkAccount";
    g_eosNames[112] = "EOS_Auth_Login";
    g_eosNames[113] = "EOS_Auth_Logout";
    g_eosNames[114] = "EOS_Auth_QueryIdToken";
    g_eosNames[115] = "EOS_Auth_RemoveNotifyLoginStatusChanged";
    g_eosNames[116] = "EOS_Auth_Token_Release";
    g_eosNames[117] = "EOS_Auth_VerifyIdToken";
    g_eosNames[118] = "EOS_Auth_VerifyUserAuth";
    g_eosNames[119] = "EOS_BeginScopeEvent";
    g_eosNames[120] = "EOS_BroadcastAudio_CreateNewInputStream";
    g_eosNames[121] = "EOS_BroadcastAudio_CreateNewOutputStream";
    g_eosNames[122] = "EOS_BroadcastAudio_DestroyInputStream";
    g_eosNames[123] = "EOS_BroadcastAudio_DestroyOutputStream";
    g_eosNames[124] = "EOS_BroadcastAudio_GetCurrentGainLevel";
    g_eosNames[125] = "EOS_BroadcastAudio_GetCurrentMicAmplitude";
    g_eosNames[126] = "EOS_BroadcastAudio_GetInputStreamInfo";
    g_eosNames[127] = "EOS_BroadcastAudio_GetOutputStreamInfo";
    g_eosNames[128] = "EOS_BroadcastAudio_PushPacketToOutputStream";
    g_eosNames[129] = "EOS_BroadcastAudio_SetEncoderSettings";
    g_eosNames[130] = "EOS_BroadcastAudio_SetMicProcessingSettings";
    g_eosNames[131] = "EOS_BroadcastAudio_StartInputStream";
    g_eosNames[132] = "EOS_BroadcastAudio_StartOutputStream";
    g_eosNames[133] = "EOS_BroadcastAudio_StopInputStream";
    g_eosNames[134] = "EOS_BroadcastAudio_StopOutputStream";
    g_eosNames[135] = "EOS_ByteArray_ToString";
    g_eosNames[136] = "EOS_Connect_AddNotifyAuthExpiration";
    g_eosNames[137] = "EOS_Connect_AddNotifyLoginStatusChanged";
    g_eosNames[138] = "EOS_Connect_CopyIdToken";
    g_eosNames[139] = "EOS_Connect_CopyProductUserExternalAccountByAccountId";
    g_eosNames[140] = "EOS_Connect_CopyProductUserExternalAccountByAccountType";
    g_eosNames[141] = "EOS_Connect_CopyProductUserExternalAccountByIndex";
    g_eosNames[142] = "EOS_Connect_CopyProductUserInfo";
    g_eosNames[143] = "EOS_Connect_CreateDeviceId";
    g_eosNames[144] = "EOS_Connect_CreateUser";
    g_eosNames[145] = "EOS_Connect_DeleteDeviceId";
    g_eosNames[146] = "EOS_Connect_ExternalAccountInfo_Release";
    g_eosNames[147] = "EOS_Connect_GetExternalAccountMapping";
    g_eosNames[148] = "EOS_Connect_GetLoggedInUserByIndex";
    g_eosNames[149] = "EOS_Connect_GetLoggedInUsersCount";
    g_eosNames[150] = "EOS_Connect_GetLoginStatus";
    g_eosNames[151] = "EOS_Connect_GetProductUserExternalAccountCount";
    g_eosNames[152] = "EOS_Connect_GetProductUserIdMapping";
    g_eosNames[153] = "EOS_Connect_IdToken_Release";
    g_eosNames[154] = "EOS_Connect_LinkAccount";
    g_eosNames[155] = "EOS_Connect_Login";
    g_eosNames[156] = "EOS_Connect_Logout";
    g_eosNames[157] = "EOS_Connect_QueryExternalAccountMappings";
    g_eosNames[158] = "EOS_Connect_QueryProductUserIdMappings";
    g_eosNames[159] = "EOS_Connect_RemoveNotifyAuthExpiration";
    g_eosNames[160] = "EOS_Connect_RemoveNotifyLoginStatusChanged";
    g_eosNames[161] = "EOS_Connect_TransferDeviceIdAccount";
    g_eosNames[162] = "EOS_Connect_UnlinkAccount";
    g_eosNames[163] = "EOS_Connect_VerifyIdToken";
    g_eosNames[164] = "EOS_ContinuanceToken_ToString";
    g_eosNames[165] = "EOS_CustomInvites_AcceptRequestToJoin";
    g_eosNames[166] = "EOS_CustomInvites_AddNotifyCustomInviteAccepted";
    g_eosNames[167] = "EOS_CustomInvites_AddNotifyCustomInviteReceived";
    g_eosNames[168] = "EOS_CustomInvites_AddNotifyCustomInviteRejected";
    g_eosNames[169] = "EOS_CustomInvites_AddNotifyRequestToJoinAccepted";
    g_eosNames[170] = "EOS_CustomInvites_AddNotifyRequestToJoinReceived";
    g_eosNames[171] = "EOS_CustomInvites_AddNotifyRequestToJoinRejected";
    g_eosNames[172] = "EOS_CustomInvites_AddNotifyRequestToJoinResponseReceived";
    g_eosNames[173] = "EOS_CustomInvites_AddNotifySendCustomNativeInviteRequested";
    g_eosNames[174] = "EOS_CustomInvites_FinalizeInvite";
    g_eosNames[175] = "EOS_CustomInvites_RejectRequestToJoin";
    g_eosNames[176] = "EOS_CustomInvites_RemoveNotifyCustomInviteAccepted";
    g_eosNames[177] = "EOS_CustomInvites_RemoveNotifyCustomInviteReceived";
    g_eosNames[178] = "EOS_CustomInvites_RemoveNotifyCustomInviteRejected";
    g_eosNames[179] = "EOS_CustomInvites_RemoveNotifyRequestToJoinAccepted";
    g_eosNames[180] = "EOS_CustomInvites_RemoveNotifyRequestToJoinReceived";
    g_eosNames[181] = "EOS_CustomInvites_RemoveNotifyRequestToJoinRejected";
    g_eosNames[182] = "EOS_CustomInvites_RemoveNotifyRequestToJoinResponseReceived";
    g_eosNames[183] = "EOS_CustomInvites_RemoveNotifySendCustomNativeInviteRequested";
    g_eosNames[184] = "EOS_CustomInvites_SendCustomInvite";
    g_eosNames[185] = "EOS_CustomInvites_SendRequestToJoin";
    g_eosNames[186] = "EOS_CustomInvites_SetCustomInvite";
    g_eosNames[187] = "EOS_EApplicationStatus_ToString";
    g_eosNames[188] = "EOS_ENetworkStatus_ToString";
    g_eosNames[189] = "EOS_EResult_IsOperationComplete";
    g_eosNames[190] = "EOS_EResult_ToString";
    g_eosNames[191] = "EOS_Ecom_CatalogItem_Release";
    g_eosNames[192] = "EOS_Ecom_CatalogOffer_Release";
    g_eosNames[193] = "EOS_Ecom_CatalogRelease_Release";
    g_eosNames[194] = "EOS_Ecom_Checkout";
    g_eosNames[195] = "EOS_Ecom_CopyEntitlementById";
    g_eosNames[196] = "EOS_Ecom_CopyEntitlementByIndex";
    g_eosNames[197] = "EOS_Ecom_CopyEntitlementByNameAndIndex";
    g_eosNames[198] = "EOS_Ecom_CopyItemById";
    g_eosNames[199] = "EOS_Ecom_CopyItemImageInfoByIndex";
    g_eosNames[200] = "EOS_Ecom_CopyItemReleaseByIndex";
    g_eosNames[201] = "EOS_Ecom_CopyLastRedeemEntitlementsResultByIndex";
    g_eosNames[202] = "EOS_Ecom_CopyLastRedeemedEntitlementByIndex";
    g_eosNames[203] = "EOS_Ecom_CopyOfferById";
    g_eosNames[204] = "EOS_Ecom_CopyOfferByIndex";
    g_eosNames[205] = "EOS_Ecom_CopyOfferImageInfoByIndex";
    g_eosNames[206] = "EOS_Ecom_CopyOfferItemByIndex";
    g_eosNames[207] = "EOS_Ecom_CopyTransactionById";
    g_eosNames[208] = "EOS_Ecom_CopyTransactionByIndex";
    g_eosNames[209] = "EOS_Ecom_Entitlement_Release";
    g_eosNames[210] = "EOS_Ecom_GetEntitlementsByNameCount";
    g_eosNames[211] = "EOS_Ecom_GetEntitlementsCount";
    g_eosNames[212] = "EOS_Ecom_GetItemImageInfoCount";
    g_eosNames[213] = "EOS_Ecom_GetItemReleaseCount";
    g_eosNames[214] = "EOS_Ecom_GetLastRedeemEntitlementsResultCount";
    g_eosNames[215] = "EOS_Ecom_GetLastRedeemedEntitlementsCount";
    g_eosNames[216] = "EOS_Ecom_GetOfferCount";
    g_eosNames[217] = "EOS_Ecom_GetOfferImageInfoCount";
    g_eosNames[218] = "EOS_Ecom_GetOfferItemCount";
    g_eosNames[219] = "EOS_Ecom_GetTransactionCount";
    g_eosNames[220] = "EOS_Ecom_KeyImageInfo_Release";
    g_eosNames[221] = "EOS_Ecom_QueryEntitlementToken";
    g_eosNames[222] = "EOS_Ecom_QueryEntitlements";
    g_eosNames[223] = "EOS_Ecom_QueryOffers";
    g_eosNames[224] = "EOS_Ecom_QueryOwnership";
    g_eosNames[225] = "EOS_Ecom_QueryOwnershipBySandboxIds";
    g_eosNames[226] = "EOS_Ecom_QueryOwnershipToken";
    g_eosNames[227] = "EOS_Ecom_RedeemEntitlements";
    g_eosNames[228] = "EOS_Ecom_Transaction_CopyEntitlementByIndex";
    g_eosNames[229] = "EOS_Ecom_Transaction_GetEntitlementsCount";
    g_eosNames[230] = "EOS_Ecom_Transaction_GetTransactionId";
    g_eosNames[231] = "EOS_Ecom_Transaction_Release";
    g_eosNames[232] = "EOS_EndScopeEvent";
    g_eosNames[233] = "EOS_EpicAccountId_FromString";
    g_eosNames[234] = "EOS_EpicAccountId_IsValid";
    g_eosNames[235] = "EOS_EpicAccountId_ToString";
    g_eosNames[236] = "EOS_Friends_AcceptInvite";
    g_eosNames[237] = "EOS_Friends_AddNotifyBlockedUsersUpdate";
    g_eosNames[238] = "EOS_Friends_AddNotifyFriendsUpdate";
    g_eosNames[239] = "EOS_Friends_GetBlockedUserAtIndex";
    g_eosNames[240] = "EOS_Friends_GetBlockedUsersCount";
    g_eosNames[241] = "EOS_Friends_GetFriendAtIndex";
    g_eosNames[242] = "EOS_Friends_GetFriendsCount";
    g_eosNames[243] = "EOS_Friends_GetStatus";
    g_eosNames[244] = "EOS_Friends_QueryFriends";
    g_eosNames[245] = "EOS_Friends_RejectInvite";
    g_eosNames[246] = "EOS_Friends_RemoveNotifyBlockedUsersUpdate";
    g_eosNames[247] = "EOS_Friends_RemoveNotifyFriendsUpdate";
    g_eosNames[248] = "EOS_Friends_SendInvite";
    g_eosNames[249] = "EOS_GetVersion";
    g_eosNames[250] = "EOS_Initialize";
    g_eosNames[251] = "EOS_IntegratedPlatformOptionsContainer_Add";
    g_eosNames[252] = "EOS_IntegratedPlatformOptionsContainer_Release";
    g_eosNames[253] = "EOS_IntegratedPlatform_AddNotifyUserLoginStatusChanged";
    g_eosNames[254] = "EOS_IntegratedPlatform_ClearUserPreLogoutCallback";
    g_eosNames[255] = "EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer";
    g_eosNames[256] = "EOS_IntegratedPlatform_FinalizeDeferredUserLogout";
    g_eosNames[257] = "EOS_IntegratedPlatform_RemoveNotifyUserLoginStatusChanged";
    g_eosNames[258] = "EOS_IntegratedPlatform_SetUserLoginStatus";
    g_eosNames[259] = "EOS_IntegratedPlatform_SetUserPreLogoutCallback";
    g_eosNames[260] = "EOS_KWS_AddNotifyPermissionsUpdateReceived";
    g_eosNames[261] = "EOS_KWS_CopyPermissionByIndex";
    g_eosNames[262] = "EOS_KWS_CreateUser";
    g_eosNames[263] = "EOS_KWS_GetPermissionByKey";
    g_eosNames[264] = "EOS_KWS_GetPermissionsCount";
    g_eosNames[265] = "EOS_KWS_PermissionStatus_Release";
    g_eosNames[266] = "EOS_KWS_QueryAgeGate";
    g_eosNames[267] = "EOS_KWS_QueryPermissions";
    g_eosNames[268] = "EOS_KWS_RemoveNotifyPermissionsUpdateReceived";
    g_eosNames[269] = "EOS_KWS_RequestPermissions";
    g_eosNames[270] = "EOS_KWS_UpdateParentEmail";
    g_eosNames[271] = "EOS_Leaderboards_CopyLeaderboardDefinitionByIndex";
    g_eosNames[272] = "EOS_Leaderboards_CopyLeaderboardDefinitionByLeaderboardId";
    g_eosNames[273] = "EOS_Leaderboards_CopyLeaderboardRecordByIndex";
    g_eosNames[274] = "EOS_Leaderboards_CopyLeaderboardRecordByUserId";
    g_eosNames[275] = "EOS_Leaderboards_CopyLeaderboardUserScoreByIndex";
    g_eosNames[276] = "EOS_Leaderboards_CopyLeaderboardUserScoreByUserId";
    g_eosNames[277] = "EOS_Leaderboards_Definition_Release";
    g_eosNames[278] = "EOS_Leaderboards_GetLeaderboardDefinitionCount";
    g_eosNames[279] = "EOS_Leaderboards_GetLeaderboardRecordCount";
    g_eosNames[280] = "EOS_Leaderboards_GetLeaderboardUserScoreCount";
    g_eosNames[281] = "EOS_Leaderboards_LeaderboardDefinition_Release";
    g_eosNames[282] = "EOS_Leaderboards_LeaderboardRecord_Release";
    g_eosNames[283] = "EOS_Leaderboards_LeaderboardUserScore_Release";
    g_eosNames[284] = "EOS_Leaderboards_QueryLeaderboardDefinitions";
    g_eosNames[285] = "EOS_Leaderboards_QueryLeaderboardRanks";
    g_eosNames[286] = "EOS_Leaderboards_QueryLeaderboardUserScores";
    g_eosNames[287] = "EOS_LobbyDetails_CopyAttributeByIndex";
    g_eosNames[288] = "EOS_LobbyDetails_CopyAttributeByKey";
    g_eosNames[289] = "EOS_LobbyDetails_CopyInfo";
    g_eosNames[290] = "EOS_LobbyDetails_CopyMemberAttributeByIndex";
    g_eosNames[291] = "EOS_LobbyDetails_CopyMemberAttributeByKey";
    g_eosNames[292] = "EOS_LobbyDetails_CopyMemberInfo";
    g_eosNames[293] = "EOS_LobbyDetails_GetAttributeCount";
    g_eosNames[294] = "EOS_LobbyDetails_GetLobbyOwner";
    g_eosNames[295] = "EOS_LobbyDetails_GetMemberAttributeCount";
    g_eosNames[296] = "EOS_LobbyDetails_GetMemberByIndex";
    g_eosNames[297] = "EOS_LobbyDetails_GetMemberCount";
    g_eosNames[298] = "EOS_LobbyDetails_Info_Release";
    g_eosNames[299] = "EOS_LobbyDetails_MemberInfo_Release";
    g_eosNames[300] = "EOS_LobbyDetails_Release";
    g_eosNames[301] = "EOS_LobbyModification_AddAttribute";
    g_eosNames[302] = "EOS_LobbyModification_AddMemberAttribute";
    g_eosNames[303] = "EOS_LobbyModification_Release";
    g_eosNames[304] = "EOS_LobbyModification_RemoveAttribute";
    g_eosNames[305] = "EOS_LobbyModification_RemoveMemberAttribute";
    g_eosNames[306] = "EOS_LobbyModification_SetAllowedPlatformIds";
    g_eosNames[307] = "EOS_LobbyModification_SetBucketId";
    g_eosNames[308] = "EOS_LobbyModification_SetInvitesAllowed";
    g_eosNames[309] = "EOS_LobbyModification_SetMaxMembers";
    g_eosNames[310] = "EOS_LobbyModification_SetPermissionLevel";
    g_eosNames[311] = "EOS_LobbySearch_CopySearchResultByIndex";
    g_eosNames[312] = "EOS_LobbySearch_Find";
    g_eosNames[313] = "EOS_LobbySearch_GetSearchResultCount";
    g_eosNames[314] = "EOS_LobbySearch_Release";
    g_eosNames[315] = "EOS_LobbySearch_RemoveParameter";
    g_eosNames[316] = "EOS_LobbySearch_SetLobbyId";
    g_eosNames[317] = "EOS_LobbySearch_SetMaxResults";
    g_eosNames[318] = "EOS_LobbySearch_SetParameter";
    g_eosNames[319] = "EOS_LobbySearch_SetTargetUserId";
    g_eosNames[320] = "EOS_Lobby_AddNotifyJoinLobbyAccepted";
    g_eosNames[321] = "EOS_Lobby_AddNotifyLeaveLobbyRequested";
    g_eosNames[322] = "EOS_Lobby_AddNotifyLobbyInviteAccepted";
    g_eosNames[323] = "EOS_Lobby_AddNotifyLobbyInviteReceived";
    g_eosNames[324] = "EOS_Lobby_AddNotifyLobbyInviteRejected";
    g_eosNames[325] = "EOS_Lobby_AddNotifyLobbyMemberStatusReceived";
    g_eosNames[326] = "EOS_Lobby_AddNotifyLobbyMemberUpdateReceived";
    g_eosNames[327] = "EOS_Lobby_AddNotifyLobbyUpdateReceived";
    g_eosNames[328] = "EOS_Lobby_AddNotifyRTCRoomConnectionChanged";
    g_eosNames[329] = "EOS_Lobby_AddNotifySendLobbyNativeInviteRequested";
    g_eosNames[330] = "EOS_Lobby_Attribute_Release";
    g_eosNames[331] = "EOS_Lobby_CopyLobbyDetailsHandle";
    g_eosNames[332] = "EOS_Lobby_CopyLobbyDetailsHandleByInviteId";
    g_eosNames[333] = "EOS_Lobby_CopyLobbyDetailsHandleByUiEventId";
    g_eosNames[334] = "EOS_Lobby_CreateLobby";
    g_eosNames[335] = "EOS_Lobby_CreateLobbySearch";
    g_eosNames[336] = "EOS_Lobby_DestroyLobby";
    g_eosNames[337] = "EOS_Lobby_GetConnectString";
    g_eosNames[338] = "EOS_Lobby_GetInviteCount";
    g_eosNames[339] = "EOS_Lobby_GetInviteIdByIndex";
    g_eosNames[340] = "EOS_Lobby_GetRTCRoomName";
    g_eosNames[341] = "EOS_Lobby_HardMuteMember";
    g_eosNames[342] = "EOS_Lobby_IsRTCRoomConnected";
    g_eosNames[343] = "EOS_Lobby_JoinLobby";
    g_eosNames[344] = "EOS_Lobby_JoinLobbyById";
    g_eosNames[345] = "EOS_Lobby_JoinRTCRoom";
    g_eosNames[346] = "EOS_Lobby_KickMember";
    g_eosNames[347] = "EOS_Lobby_LeaveLobby";
    g_eosNames[348] = "EOS_Lobby_LeaveRTCRoom";
    g_eosNames[349] = "EOS_Lobby_ParseConnectString";
    g_eosNames[350] = "EOS_Lobby_PromoteMember";
    g_eosNames[351] = "EOS_Lobby_QueryInvites";
    g_eosNames[352] = "EOS_Lobby_RejectInvite";
    g_eosNames[353] = "EOS_Lobby_RemoveNotifyJoinLobbyAccepted";
    g_eosNames[354] = "EOS_Lobby_RemoveNotifyLeaveLobbyRequested";
    g_eosNames[355] = "EOS_Lobby_RemoveNotifyLobbyInviteAccepted";
    g_eosNames[356] = "EOS_Lobby_RemoveNotifyLobbyInviteReceived";
    g_eosNames[357] = "EOS_Lobby_RemoveNotifyLobbyInviteRejected";
    g_eosNames[358] = "EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived";
    g_eosNames[359] = "EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived";
    g_eosNames[360] = "EOS_Lobby_RemoveNotifyLobbyUpdateReceived";
    g_eosNames[361] = "EOS_Lobby_RemoveNotifyRTCRoomConnectionChanged";
    g_eosNames[362] = "EOS_Lobby_RemoveNotifySendLobbyNativeInviteRequested";
    g_eosNames[363] = "EOS_Lobby_SendInvite";
    g_eosNames[364] = "EOS_Lobby_UpdateLobby";
    g_eosNames[365] = "EOS_Lobby_UpdateLobbyModification";
    g_eosNames[366] = "EOS_Logging_SetCallback";
    g_eosNames[367] = "EOS_Logging_SetLogLevel";
    g_eosNames[368] = "EOS_Mercury_Initialize";
    g_eosNames[369] = "EOS_Mercury_Shutdown";
    g_eosNames[370] = "EOS_Mercury_Tick";
    g_eosNames[371] = "EOS_Metrics_BeginPlayerSession";
    g_eosNames[372] = "EOS_Metrics_EndPlayerSession";
    g_eosNames[373] = "EOS_Mods_CopyModInfo";
    g_eosNames[374] = "EOS_Mods_EnumerateMods";
    g_eosNames[375] = "EOS_Mods_InstallMod";
    g_eosNames[376] = "EOS_Mods_ModInfo_Release";
    g_eosNames[377] = "EOS_Mods_UninstallMod";
    g_eosNames[378] = "EOS_Mods_UpdateMod";
    g_eosNames[379] = "EOS_P2P_AcceptConnection";
    g_eosNames[380] = "EOS_P2P_AddNotifyIncomingPacketQueueFull";
    g_eosNames[381] = "EOS_P2P_AddNotifyPeerConnectionClosed";
    g_eosNames[382] = "EOS_P2P_AddNotifyPeerConnectionEstablished";
    g_eosNames[383] = "EOS_P2P_AddNotifyPeerConnectionInterrupted";
    g_eosNames[384] = "EOS_P2P_AddNotifyPeerConnectionRequest";
    g_eosNames[385] = "EOS_P2P_ClearPacketQueue";
    g_eosNames[386] = "EOS_P2P_CloseConnection";
    g_eosNames[387] = "EOS_P2P_CloseConnections";
    g_eosNames[388] = "EOS_P2P_GetNATType";
    g_eosNames[389] = "EOS_P2P_GetNextReceivedPacketSize";
    g_eosNames[390] = "EOS_P2P_GetPacketQueueInfo";
    g_eosNames[391] = "EOS_P2P_GetPortRange";
    g_eosNames[392] = "EOS_P2P_GetRelayControl";
    g_eosNames[393] = "EOS_P2P_QueryNATType";
    g_eosNames[394] = "EOS_P2P_ReceivePacket";
    g_eosNames[395] = "EOS_P2P_RemoveNotifyIncomingPacketQueueFull";
    g_eosNames[396] = "EOS_P2P_RemoveNotifyPeerConnectionClosed";
    g_eosNames[397] = "EOS_P2P_RemoveNotifyPeerConnectionEstablished";
    g_eosNames[398] = "EOS_P2P_RemoveNotifyPeerConnectionInterrupted";
    g_eosNames[399] = "EOS_P2P_RemoveNotifyPeerConnectionRequest";
    g_eosNames[400] = "EOS_P2P_SendPacket";
    g_eosNames[401] = "EOS_P2P_SetPacketQueueSize";
    g_eosNames[402] = "EOS_P2P_SetPortRange";
    g_eosNames[403] = "EOS_P2P_SetRelayControl";
    g_eosNames[404] = "EOS_Platform_CheckForLauncherAndRestart";
    g_eosNames[405] = "EOS_Platform_Create";
    g_eosNames[406] = "EOS_Platform_GetAchievementsInterface";
    g_eosNames[407] = "EOS_Platform_GetActiveCountryCode";
    g_eosNames[408] = "EOS_Platform_GetActiveLocaleCode";
    g_eosNames[409] = "EOS_Platform_GetAntiCheatClientInterface";
    g_eosNames[410] = "EOS_Platform_GetAntiCheatServerInterface";
    g_eosNames[411] = "EOS_Platform_GetApplicationStatus";
    g_eosNames[412] = "EOS_Platform_GetAuthInterface";
    g_eosNames[413] = "EOS_Platform_GetConnectInterface";
    g_eosNames[414] = "EOS_Platform_GetCustomInvitesInterface";
    g_eosNames[415] = "EOS_Platform_GetDesktopCrossplayStatus";
    g_eosNames[416] = "EOS_Platform_GetEcomInterface";
    g_eosNames[417] = "EOS_Platform_GetFriendsInterface";
    g_eosNames[418] = "EOS_Platform_GetIntegratedPlatformInterface";
    g_eosNames[419] = "EOS_Platform_GetKWSInterface";
    g_eosNames[420] = "EOS_Platform_GetLeaderboardsInterface";
    g_eosNames[421] = "EOS_Platform_GetLobbyInterface";
    g_eosNames[422] = "EOS_Platform_GetMetricsInterface";
    g_eosNames[423] = "EOS_Platform_GetModsInterface";
    g_eosNames[424] = "EOS_Platform_GetNetworkStatus";
    g_eosNames[425] = "EOS_Platform_GetOverrideCountryCode";
    g_eosNames[426] = "EOS_Platform_GetOverrideLocaleCode";
    g_eosNames[427] = "EOS_Platform_GetP2PInterface";
    g_eosNames[428] = "EOS_Platform_GetPlayerDataStorageInterface";
    g_eosNames[429] = "EOS_Platform_GetPresenceInterface";
    g_eosNames[430] = "EOS_Platform_GetProgressionSnapshotInterface";
    g_eosNames[431] = "EOS_Platform_GetRTCAdminInterface";
    g_eosNames[432] = "EOS_Platform_GetRTCInterface";
    g_eosNames[433] = "EOS_Platform_GetReportsInterface";
    g_eosNames[434] = "EOS_Platform_GetSanctionsInterface";
    g_eosNames[435] = "EOS_Platform_GetSessionsInterface";
    g_eosNames[436] = "EOS_Platform_GetStatsInterface";
    g_eosNames[437] = "EOS_Platform_GetTitleStorageInterface";
    g_eosNames[438] = "EOS_Platform_GetUIInterface";
    g_eosNames[439] = "EOS_Platform_GetUserInfoInterface";
    g_eosNames[440] = "EOS_Platform_Release";
    g_eosNames[441] = "EOS_Platform_SetApplicationStatus";
    g_eosNames[442] = "EOS_Platform_SetNetworkStatus";
    g_eosNames[443] = "EOS_Platform_SetOverrideCountryCode";
    g_eosNames[444] = "EOS_Platform_SetOverrideLocaleCode";
    g_eosNames[445] = "EOS_Platform_Tick";
    g_eosNames[446] = "EOS_PlayerDataStorageFileTransferRequest_CancelRequest";
    g_eosNames[447] = "EOS_PlayerDataStorageFileTransferRequest_GetFileRequestState";
    g_eosNames[448] = "EOS_PlayerDataStorageFileTransferRequest_GetFilename";
    g_eosNames[449] = "EOS_PlayerDataStorageFileTransferRequest_Release";
    g_eosNames[450] = "EOS_PlayerDataStorage_CopyFileMetadataAtIndex";
    g_eosNames[451] = "EOS_PlayerDataStorage_CopyFileMetadataByFilename";
    g_eosNames[452] = "EOS_PlayerDataStorage_DeleteCache";
    g_eosNames[453] = "EOS_PlayerDataStorage_DeleteFile";
    g_eosNames[454] = "EOS_PlayerDataStorage_DuplicateFile";
    g_eosNames[455] = "EOS_PlayerDataStorage_FileMetadata_Release";
    g_eosNames[456] = "EOS_PlayerDataStorage_GetFileMetadataCount";
    g_eosNames[457] = "EOS_PlayerDataStorage_QueryFile";
    g_eosNames[458] = "EOS_PlayerDataStorage_QueryFileList";
    g_eosNames[459] = "EOS_PlayerDataStorage_ReadFile";
    g_eosNames[460] = "EOS_PlayerDataStorage_WriteFile";
    g_eosNames[461] = "EOS_PresenceModification_DeleteData";
    g_eosNames[462] = "EOS_PresenceModification_Release";
    g_eosNames[463] = "EOS_PresenceModification_SetData";
    g_eosNames[464] = "EOS_PresenceModification_SetJoinInfo";
    g_eosNames[465] = "EOS_PresenceModification_SetRawRichText";
    g_eosNames[466] = "EOS_PresenceModification_SetStatus";
    g_eosNames[467] = "EOS_PresenceModification_SetTemplateData";
    g_eosNames[468] = "EOS_PresenceModification_SetTemplateId";
    g_eosNames[469] = "EOS_Presence_AddNotifyJoinGameAccepted";
    g_eosNames[470] = "EOS_Presence_AddNotifyOnPresenceChanged";
    g_eosNames[471] = "EOS_Presence_CopyPresence";
    g_eosNames[472] = "EOS_Presence_CreatePresenceModification";
    g_eosNames[473] = "EOS_Presence_GetJoinInfo";
    g_eosNames[474] = "EOS_Presence_HasPresence";
    g_eosNames[475] = "EOS_Presence_Info_Release";
    g_eosNames[476] = "EOS_Presence_QueryPresence";
    g_eosNames[477] = "EOS_Presence_RemoveNotifyJoinGameAccepted";
    g_eosNames[478] = "EOS_Presence_RemoveNotifyOnPresenceChanged";
    g_eosNames[479] = "EOS_Presence_SetPresence";
    g_eosNames[480] = "EOS_ProductUserId_FromString";
    g_eosNames[481] = "EOS_ProductUserId_IsValid";
    g_eosNames[482] = "EOS_ProductUserId_ToString";
    g_eosNames[483] = "EOS_ProgressionSnapshot_AddProgression";
    g_eosNames[484] = "EOS_ProgressionSnapshot_BeginSnapshot";
    g_eosNames[485] = "EOS_ProgressionSnapshot_DeleteSnapshot";
    g_eosNames[486] = "EOS_ProgressionSnapshot_EndSnapshot";
    g_eosNames[487] = "EOS_ProgressionSnapshot_SubmitSnapshot";
    g_eosNames[488] = "EOS_RTCAdmin_CopyUserTokenByIndex";
    g_eosNames[489] = "EOS_RTCAdmin_CopyUserTokenByUserId";
    g_eosNames[490] = "EOS_RTCAdmin_Kick";
    g_eosNames[491] = "EOS_RTCAdmin_QueryJoinRoomToken";
    g_eosNames[492] = "EOS_RTCAdmin_SetParticipantHardMute";
    g_eosNames[493] = "EOS_RTCAdmin_UserToken_Release";
    g_eosNames[494] = "EOS_RTCAudio_AddNotifyAudioBeforeRender";
    g_eosNames[495] = "EOS_RTCAudio_AddNotifyAudioBeforeSend";
    g_eosNames[496] = "EOS_RTCAudio_AddNotifyAudioDevicesChanged";
    g_eosNames[497] = "EOS_RTCAudio_AddNotifyAudioInputState";
    g_eosNames[498] = "EOS_RTCAudio_AddNotifyAudioOutputState";
    g_eosNames[499] = "EOS_RTCAudio_AddNotifyParticipantUpdated";
    g_eosNames[500] = "EOS_RTCAudio_CopyInputDeviceInformationByIndex";
    g_eosNames[501] = "EOS_RTCAudio_CopyOutputDeviceInformationByIndex";
    g_eosNames[502] = "EOS_RTCAudio_GetAudioInputDeviceByIndex";
    g_eosNames[503] = "EOS_RTCAudio_GetAudioInputDevicesCount";
    g_eosNames[504] = "EOS_RTCAudio_GetAudioOutputDeviceByIndex";
    g_eosNames[505] = "EOS_RTCAudio_GetAudioOutputDevicesCount";
    g_eosNames[506] = "EOS_RTCAudio_GetInputDevicesCount";
    g_eosNames[507] = "EOS_RTCAudio_GetOutputDevicesCount";
    g_eosNames[508] = "EOS_RTCAudio_InputDeviceInformation_Release";
    g_eosNames[509] = "EOS_RTCAudio_OutputDeviceInformation_Release";
    g_eosNames[510] = "EOS_RTCAudio_QueryInputDevicesInformation";
    g_eosNames[511] = "EOS_RTCAudio_QueryOutputDevicesInformation";
    g_eosNames[512] = "EOS_RTCAudio_RegisterPlatformAudioUser";
    g_eosNames[513] = "EOS_RTCAudio_RegisterPlatformUser";
    g_eosNames[514] = "EOS_RTCAudio_RemoveNotifyAudioBeforeRender";
    g_eosNames[515] = "EOS_RTCAudio_RemoveNotifyAudioBeforeSend";
    g_eosNames[516] = "EOS_RTCAudio_RemoveNotifyAudioDevicesChanged";
    g_eosNames[517] = "EOS_RTCAudio_RemoveNotifyAudioInputState";
    g_eosNames[518] = "EOS_RTCAudio_RemoveNotifyAudioOutputState";
    g_eosNames[519] = "EOS_RTCAudio_RemoveNotifyParticipantUpdated";
    g_eosNames[520] = "EOS_RTCAudio_SendAudio";
    g_eosNames[521] = "EOS_RTCAudio_SetAudioInputSettings";
    g_eosNames[522] = "EOS_RTCAudio_SetAudioOutputSettings";
    g_eosNames[523] = "EOS_RTCAudio_SetInputDeviceSettings";
    g_eosNames[524] = "EOS_RTCAudio_SetOutputDeviceSettings";
    g_eosNames[525] = "EOS_RTCAudio_UnregisterPlatformAudioUser";
    g_eosNames[526] = "EOS_RTCAudio_UnregisterPlatformUser";
    g_eosNames[527] = "EOS_RTCAudio_UpdateParticipantVolume";
    g_eosNames[528] = "EOS_RTCAudio_UpdateReceiving";
    g_eosNames[529] = "EOS_RTCAudio_UpdateReceivingVolume";
    g_eosNames[530] = "EOS_RTCAudio_UpdateSending";
    g_eosNames[531] = "EOS_RTCAudio_UpdateSendingVolume";
    g_eosNames[532] = "EOS_RTCData_AddNotifyDataReceived";
    g_eosNames[533] = "EOS_RTCData_AddNotifyParticipantUpdated";
    g_eosNames[534] = "EOS_RTCData_RemoveNotifyDataReceived";
    g_eosNames[535] = "EOS_RTCData_RemoveNotifyParticipantUpdated";
    g_eosNames[536] = "EOS_RTCData_SendData";
    g_eosNames[537] = "EOS_RTCData_UpdateReceiving";
    g_eosNames[538] = "EOS_RTCData_UpdateSending";
    g_eosNames[539] = "EOS_RTC_AddNotifyDisconnected";
    g_eosNames[540] = "EOS_RTC_AddNotifyParticipantStatusChanged";
    g_eosNames[541] = "EOS_RTC_AddNotifyRoomBeforeJoin";
    g_eosNames[542] = "EOS_RTC_AddNotifyRoomStatisticsUpdated";
    g_eosNames[543] = "EOS_RTC_BlockParticipant";
    g_eosNames[544] = "EOS_RTC_GetAudioInterface";
    g_eosNames[545] = "EOS_RTC_GetDataInterface";
    g_eosNames[546] = "EOS_RTC_JoinRoom";
    g_eosNames[547] = "EOS_RTC_LeaveRoom";
    g_eosNames[548] = "EOS_RTC_RemoveNotifyDisconnected";
    g_eosNames[549] = "EOS_RTC_RemoveNotifyParticipantStatusChanged";
    g_eosNames[550] = "EOS_RTC_RemoveNotifyRoomBeforeJoin";
    g_eosNames[551] = "EOS_RTC_RemoveNotifyRoomStatisticsUpdated";
    g_eosNames[552] = "EOS_RTC_SetRoomSetting";
    g_eosNames[553] = "EOS_RTC_SetSetting";
    g_eosNames[554] = "EOS_Reports_SendPlayerBehaviorReport";
    g_eosNames[555] = "EOS_Sanctions_CopyPlayerSanctionByIndex";
    g_eosNames[556] = "EOS_Sanctions_CreatePlayerSanctionAppeal";
    g_eosNames[557] = "EOS_Sanctions_GetPlayerSanctionCount";
    g_eosNames[558] = "EOS_Sanctions_PlayerSanction_Release";
    g_eosNames[559] = "EOS_Sanctions_QueryActivePlayerSanctions";
    g_eosNames[560] = "EOS_SessionDetails_Attribute_Release";
    g_eosNames[561] = "EOS_SessionDetails_CopyInfo";
    g_eosNames[562] = "EOS_SessionDetails_CopySessionAttributeByIndex";
    g_eosNames[563] = "EOS_SessionDetails_CopySessionAttributeByKey";
    g_eosNames[564] = "EOS_SessionDetails_GetSessionAttributeCount";
    g_eosNames[565] = "EOS_SessionDetails_Info_Release";
    g_eosNames[566] = "EOS_SessionDetails_Release";
    g_eosNames[567] = "EOS_SessionModification_AddAttribute";
    g_eosNames[568] = "EOS_SessionModification_Release";
    g_eosNames[569] = "EOS_SessionModification_RemoveAttribute";
    g_eosNames[570] = "EOS_SessionModification_SetAllowedPlatformIds";
    g_eosNames[571] = "EOS_SessionModification_SetBucketId";
    g_eosNames[572] = "EOS_SessionModification_SetHostAddress";
    g_eosNames[573] = "EOS_SessionModification_SetInvitesAllowed";
    g_eosNames[574] = "EOS_SessionModification_SetJoinInProgressAllowed";
    g_eosNames[575] = "EOS_SessionModification_SetMaxPlayers";
    g_eosNames[576] = "EOS_SessionModification_SetPermissionLevel";
    g_eosNames[577] = "EOS_SessionSearch_CopySearchResultByIndex";
    g_eosNames[578] = "EOS_SessionSearch_Find";
    g_eosNames[579] = "EOS_SessionSearch_GetSearchResultCount";
    g_eosNames[580] = "EOS_SessionSearch_Release";
    g_eosNames[581] = "EOS_SessionSearch_RemoveParameter";
    g_eosNames[582] = "EOS_SessionSearch_SetMaxResults";
    g_eosNames[583] = "EOS_SessionSearch_SetParameter";
    g_eosNames[584] = "EOS_SessionSearch_SetSessionId";
    g_eosNames[585] = "EOS_SessionSearch_SetTargetUserId";
    g_eosNames[586] = "EOS_Sessions_AddNotifyJoinSessionAccepted";
    g_eosNames[587] = "EOS_Sessions_AddNotifyLeaveSessionRequested";
    g_eosNames[588] = "EOS_Sessions_AddNotifySendSessionNativeInviteRequested";
    g_eosNames[589] = "EOS_Sessions_AddNotifySessionInviteAccepted";
    g_eosNames[590] = "EOS_Sessions_AddNotifySessionInviteReceived";
    g_eosNames[591] = "EOS_Sessions_AddNotifySessionInviteRejected";
    g_eosNames[592] = "EOS_Sessions_CopyActiveSessionHandle";
    g_eosNames[593] = "EOS_Sessions_CopySessionHandleByInviteId";
    g_eosNames[594] = "EOS_Sessions_CopySessionHandleByUiEventId";
    g_eosNames[595] = "EOS_Sessions_CopySessionHandleForPresence";
    g_eosNames[596] = "EOS_Sessions_CreateSessionModification";
    g_eosNames[597] = "EOS_Sessions_CreateSessionSearch";
    g_eosNames[598] = "EOS_Sessions_DestroySession";
    g_eosNames[599] = "EOS_Sessions_DumpSessionState";
    g_eosNames[600] = "EOS_Sessions_EndSession";
    g_eosNames[601] = "EOS_Sessions_GetInviteCount";
    g_eosNames[602] = "EOS_Sessions_GetInviteIdByIndex";
    g_eosNames[603] = "EOS_Sessions_IsUserInSession";
    g_eosNames[604] = "EOS_Sessions_JoinSession";
    g_eosNames[605] = "EOS_Sessions_QueryInvites";
    g_eosNames[606] = "EOS_Sessions_RegisterPlayers";
    g_eosNames[607] = "EOS_Sessions_RejectInvite";
    g_eosNames[608] = "EOS_Sessions_RemoveNotifyJoinSessionAccepted";
    g_eosNames[609] = "EOS_Sessions_RemoveNotifyLeaveSessionRequested";
    g_eosNames[610] = "EOS_Sessions_RemoveNotifySendSessionNativeInviteRequested";
    g_eosNames[611] = "EOS_Sessions_RemoveNotifySessionInviteAccepted";
    g_eosNames[612] = "EOS_Sessions_RemoveNotifySessionInviteReceived";
    g_eosNames[613] = "EOS_Sessions_RemoveNotifySessionInviteRejected";
    g_eosNames[614] = "EOS_Sessions_SendInvite";
    g_eosNames[615] = "EOS_Sessions_StartSession";
    g_eosNames[616] = "EOS_Sessions_UnregisterPlayers";
    g_eosNames[617] = "EOS_Sessions_UpdateSession";
    g_eosNames[618] = "EOS_Sessions_UpdateSessionModification";
    g_eosNames[619] = "EOS_Shutdown";
    g_eosNames[620] = "EOS_Stats_CopyStatByIndex";
    g_eosNames[621] = "EOS_Stats_CopyStatByName";
    g_eosNames[622] = "EOS_Stats_GetStatsCount";
    g_eosNames[623] = "EOS_Stats_IngestStat";
    g_eosNames[624] = "EOS_Stats_QueryStats";
    g_eosNames[625] = "EOS_Stats_Stat_Release";
    g_eosNames[626] = "EOS_TitleStorageFileTransferRequest_CancelRequest";
    g_eosNames[627] = "EOS_TitleStorageFileTransferRequest_GetFileRequestState";
    g_eosNames[628] = "EOS_TitleStorageFileTransferRequest_GetFilename";
    g_eosNames[629] = "EOS_TitleStorageFileTransferRequest_Release";
    g_eosNames[630] = "EOS_TitleStorage_CopyFileMetadataAtIndex";
    g_eosNames[631] = "EOS_TitleStorage_CopyFileMetadataByFilename";
    g_eosNames[632] = "EOS_TitleStorage_DeleteCache";
    g_eosNames[633] = "EOS_TitleStorage_FileMetadata_Release";
    g_eosNames[634] = "EOS_TitleStorage_GetFileMetadataCount";
    g_eosNames[635] = "EOS_TitleStorage_QueryFile";
    g_eosNames[636] = "EOS_TitleStorage_QueryFileList";
    g_eosNames[637] = "EOS_TitleStorage_ReadFile";
    g_eosNames[638] = "EOS_UI_AcknowledgeEventId";
    g_eosNames[639] = "EOS_UI_AddNotifyDisplaySettingsUpdated";
    g_eosNames[640] = "EOS_UI_AddNotifyMemoryMonitor";
    g_eosNames[641] = "EOS_UI_AddNotifyOnScreenKeyboardRequested";
    g_eosNames[642] = "EOS_UI_ConfigureOnScreenKeyboard";
    g_eosNames[643] = "EOS_UI_GetFriendsExclusiveInput";
    g_eosNames[644] = "EOS_UI_GetFriendsVisible";
    g_eosNames[645] = "EOS_UI_GetNotificationLocationPreference";
    g_eosNames[646] = "EOS_UI_GetToggleFriendsButton";
    g_eosNames[647] = "EOS_UI_GetToggleFriendsKey";
    g_eosNames[648] = "EOS_UI_HideFriends";
    g_eosNames[649] = "EOS_UI_IsSocialOverlayPaused";
    g_eosNames[650] = "EOS_UI_IsValidButtonCombination";
    g_eosNames[651] = "EOS_UI_IsValidKeyCombination";
    g_eosNames[652] = "EOS_UI_PauseSocialOverlay";
    g_eosNames[653] = "EOS_UI_PrePresent";
    g_eosNames[654] = "EOS_UI_RemoveNotifyDisplaySettingsUpdated";
    g_eosNames[655] = "EOS_UI_RemoveNotifyMemoryMonitor";
    g_eosNames[656] = "EOS_UI_RemoveNotifyOnScreenKeyboardRequested";
    g_eosNames[657] = "EOS_UI_ReportInputState";
    g_eosNames[658] = "EOS_UI_SetDisplayPreference";
    g_eosNames[659] = "EOS_UI_SetToggleFriendsButton";
    g_eosNames[660] = "EOS_UI_SetToggleFriendsKey";
    g_eosNames[661] = "EOS_UI_ShowBlockPlayer";
    g_eosNames[662] = "EOS_UI_ShowFriends";
    g_eosNames[663] = "EOS_UI_ShowNativeProfile";
    g_eosNames[664] = "EOS_UI_ShowReportPlayer";
    g_eosNames[665] = "EOS_UserInfo_BestDisplayName_Release";
    g_eosNames[666] = "EOS_UserInfo_CopyBestDisplayName";
    g_eosNames[667] = "EOS_UserInfo_CopyBestDisplayNameWithPlatform";
    g_eosNames[668] = "EOS_UserInfo_CopyExternalUserInfoByAccountId";
    g_eosNames[669] = "EOS_UserInfo_CopyExternalUserInfoByAccountType";
    g_eosNames[670] = "EOS_UserInfo_CopyExternalUserInfoByIndex";
    g_eosNames[671] = "EOS_UserInfo_CopyUserInfo";
    g_eosNames[672] = "EOS_UserInfo_ExternalUserInfo_Release";
    g_eosNames[673] = "EOS_UserInfo_GetExternalUserInfoCount";
    g_eosNames[674] = "EOS_UserInfo_GetLocalPlatformType";
    g_eosNames[675] = "EOS_UserInfo_QueryUserInfo";
    g_eosNames[676] = "EOS_UserInfo_QueryUserInfoByDisplayName";
    g_eosNames[677] = "EOS_UserInfo_QueryUserInfoByExternalAccount";
    g_eosNames[678] = "EOS_UserInfo_Release";
}

