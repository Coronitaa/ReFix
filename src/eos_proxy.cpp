// =============================================================================
// ReFix - EOSSDK-Win64-Shipping.dll — EOS Auth Emulator v3
// =============================================================================
// Full Epic Online Services authentication emulator.
// Resolves the "Signing in..." loop by:
//   1. Emulating Connect external account mappings (Steam <-> PUID) dynamically.
//   2. Faking Connect External Account Info query results with realistic data.
//   3. Retaining deferred callback queueing via Tick.
// =============================================================================

#include <windows.h>
#include <cstring>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <stdarg.h>

// =============================================================================
// EOS TYPES
// =============================================================================
typedef int32_t EOS_EResult;
#define EOS_Success              0
#define EOS_NoConnection         3
#define EOS_NotFound             14
#define EOS_AlreadyConfigured    30
#define EOS_LimitExceeded        31

typedef void*    EOS_HPlatform;
typedef void*    EOS_ProductUserId;
typedef void*    EOS_EpicAccountId;
typedef void*    EOS_ContinuanceToken;
typedef uint64_t EOS_NotificationId;

// Steam Friends Interface Typedefs
typedef void* (*fn_SteamFriends_t)();
typedef int (*fn_GetFriendCount_t)(void* self, int friendFlags);
typedef uint64_t (*fn_GetFriendByIndex_t)(void* self, int index, int friendFlags);
typedef const char* (*fn_GetFriendPersonaName_t)(void* self, uint64_t steamIDFriend);

static fn_SteamFriends_t g_pfn_SteamFriends = nullptr;
static fn_GetFriendCount_t g_pfn_GetFriendCount = nullptr;
static fn_GetFriendByIndex_t g_pfn_GetFriendByIndex = nullptr;
static fn_GetFriendPersonaName_t g_pfn_GetFriendPersonaName = nullptr;

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

// =============================================================================
// DEBUG LOGGING
// =============================================================================
static void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buf[1024];
    vsprintf_s(buf, sizeof(buf), format, args);
    va_end(args);
    
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string logPath(exePath);
    size_t pos = logPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        logPath = logPath.substr(0, pos + 1) + "ReFix_eos_debug.log";
    } else {
        logPath = "ReFix_eos_debug.log";
    }
    
    FILE* f = nullptr;
    fopen_s(&f, logPath.c_str(), "a");
    if (f) {
        fprintf(f, "[%lu] %s\n", GetTickCount(), buf);
        fclose(f);
    }
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
struct EOS_Connect_ExternalAccountInfo {
    int32_t ApiVersion;
    EOS_ProductUserId ProductUserId;
    const char* DisplayName;
    const char* AccountId;
    int32_t AccountIdType;
    int64_t LastLoginTime;
};

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
    std::string dispName = GetDisplayNameForExternalId(extId);
    
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
    Log("Generic stub called: %s (index %lld)", g_eosNames[index], index);
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
    
    // Refresh SteamId if still using hardcoded fallback
    if (strcmp(g_productUserIdStr, "76561197960287930") == 0) {
        char envBuf[64] = {0};
        if (GetEnvironmentVariableA("REFIX_STEAM_ID", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
            strcpy_s(g_productUserIdStr, sizeof(g_productUserIdStr), envBuf);
            Log("RefreshUserName: Updated SteamId from real Steam account: %s", g_productUserIdStr);
        }
    }
    
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
    return HANDLE_PLATFORM;
}

static void eos_Platform_Tick(void* Handle) {
    FlushCallbacks();
}

static void eos_Platform_Release(void* Handle) {
    Log("EOS_Platform_Release called");
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

// --- Auth Interface ---
static EOS_NotificationId eos_Auth_AddNotifyLoginStatusChanged(void* Handle, void* Options, void* ClientData, void* Callback) {
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
static EOS_NotificationId eos_Connect_AddNotifyLoginStatusChanged(void* H, void* O, void* C, void* Cb) { return s_nextNotifId++; }
static EOS_EResult eos_Connect_CopyIdToken(void* H, void* O, void** Out) { return EOS_NotFound; }

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
    if (UserId == FAKE_PRODUCT_USER_ID) return EOS_LS_LoggedIn;
    for (auto& pair : g_puidMap) {
        if (pair.second == UserId) return EOS_LS_LoggedIn;
    }
    return 0; // EOS_LS_NotLoggedIn
}

static uint32_t eos_Connect_GetProductUserExternalAccountCount(void* H, void* O) {
    Log("EOS_Connect_GetProductUserExternalAccountCount -> 2");
    return 2;
}

static EOS_EResult eos_Connect_GetProductUserIdMapping(void* H, void* O, char* Buf, int32_t* Len) { return EOS_NotFound; }
static void eos_Connect_IdToken_Release(void* T) { }

static void eos_Connect_LinkAccount(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Connect_LinkAccount called");
    CB_Connect_Generic info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    info.LocalUserId = FAKE_PRODUCT_USER_ID;
    QueueCallback(Cb, info);
}

struct EOS_Connect_Credentials {
    int32_t ApiVersion;
    const char* Token;
    int32_t Type;
};

struct EOS_Connect_LoginOptions {
    int32_t ApiVersion;
    const EOS_Connect_Credentials* Credentials;
    void* UserLoginInfo;
};

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

struct EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions {
    int32_t ApiVersion;
    void* TargetUserId;
    int32_t AccountIdType;
};

struct EOS_Connect_CopyProductUserExternalAccountByIndexOptions {
    int32_t ApiVersion;
    void* TargetUserId;
    uint32_t ExternalAccountInfoIndex;
};

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
struct EOS_Lobby_AttributeValue {
    int64_t AsInt64;
    double AsDouble;
    int32_t bAsBool;
    const char* AsUtf8;
};

struct EOS_Lobby_AttributeData {
    int32_t ApiVersion;
    const char* Key;
    EOS_Lobby_AttributeValue Value;
    int32_t ValueType;
};

static std::vector<EOS_Lobby_AttributeData> g_capturedSearchParameters;

static void FreeCapturedSearchParameters() {
    for (auto& param : g_capturedSearchParameters) {
        if (param.Key) free((void*)param.Key);
        if (param.ValueType == 4 && param.Value.AsUtf8) { // String type
            free((void*)param.Value.AsUtf8);
        }
    }
    g_capturedSearchParameters.clear();
}

static void CaptureSearchParameter(const EOS_Lobby_AttributeData* data) {
    if (!data || !data->Key) return;
    
    for (auto& param : g_capturedSearchParameters) {
        if (strcmp(param.Key, data->Key) == 0) {
            if (param.ValueType == 4 && param.Value.AsUtf8) free((void*)param.Value.AsUtf8);
            param.ValueType = data->ValueType;
            param.Value = data->Value;
            if (data->ValueType == 4 && data->Value.AsUtf8) {
                param.Value.AsUtf8 = _strdup(data->Value.AsUtf8);
            }
            return;
        }
    }
    
    EOS_Lobby_AttributeData clone = *data;
    clone.Key = _strdup(data->Key);
    if (data->ValueType == 4 && data->Value.AsUtf8) {
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

static void eos_Lobby_CreateLobby(void* H, void* O, void* C, void* Cb) {
    Log("EOS_Lobby_CreateLobby called");
    CB_Lobby_CreateLobby info = {};
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
    Log("EOS_LobbySearch_Find called");
    g_foundLobbies.clear();
    
    void* steamFriends = GetSteamFriendsInterface();
    if (steamFriends && g_pfn_GetFriendCount && g_pfn_GetFriendByIndex) {
        int count = g_pfn_GetFriendCount(steamFriends, 0x04); // k_EFriendFlagImmediate
        Log("  Querying friends for lobbies... Found %d friends", count);
        for (int i = 0; i < count; i++) {
            uint64_t steamId = g_pfn_GetFriendByIndex(steamFriends, i, 0x04);
            if (steamId != 0) {
                char idStr[32];
                sprintf_s(idStr, sizeof(idStr), "%llu", steamId);
                g_foundLobbies.push_back(idStr);
                Log("    Mapped friend %d (SteamID %s) as lobby", i, idStr);
            }
        }
    }
    
    CB_LobbySearch_Find info = {};
    info.ResultCode = EOS_Success;
    info.ClientData = C;
    QueueCallback(Cb, info);
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
    Log("EOS_LobbyDetails_GetAttributeCount called -> %d", (int)g_capturedSearchParameters.size());
    return (uint32_t)g_capturedSearchParameters.size();
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
    if (index >= g_capturedSearchParameters.size()) return EOS_NotFound;
    
    auto* attr = (EOS_Lobby_Attribute*)malloc(sizeof(EOS_Lobby_Attribute));
    if (!attr) return EOS_LimitExceeded;
    
    auto* data = (EOS_Lobby_AttributeData*)malloc(sizeof(EOS_Lobby_AttributeData));
    if (!data) { free(attr); return EOS_LimitExceeded; }
    
    *data = g_capturedSearchParameters[index];
    data->Key = _strdup(g_capturedSearchParameters[index].Key);
    if (data->ValueType == 4 && data->Value.AsUtf8) {
        data->Value.AsUtf8 = _strdup(g_capturedSearchParameters[index].Value.AsUtf8);
    }
    
    attr->ApiVersion = 1;
    attr->Data = data;
    attr->Visibility = 0; // Public
    
    *OutAttribute = attr;
    Log("  Returning attribute at index %d: Key=%s", index, data->Key);
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
    
    int index = -1;
    for (size_t i = 0; i < g_capturedSearchParameters.size(); i++) {
        if (strcmp(g_capturedSearchParameters[i].Key, key) == 0) {
            index = (int)i;
            break;
        }
    }
    
    if (index < 0) {
        // Create dummy attribute to satisfy search filters
        Log("    Key not found in captured parameters. Creating dummy attribute to satisfy filter!");
        auto* attr = (EOS_Lobby_Attribute*)malloc(sizeof(EOS_Lobby_Attribute));
        if (!attr) return EOS_LimitExceeded;
        auto* data = (EOS_Lobby_AttributeData*)malloc(sizeof(EOS_Lobby_AttributeData));
        if (!data) { free(attr); return EOS_LimitExceeded; }
        
        data->ApiVersion = 1;
        data->Key = _strdup(key);
        data->ValueType = 4; // String
        data->Value.AsUtf8 = _strdup("dummy");
        
        attr->ApiVersion = 1;
        attr->Data = data;
        attr->Visibility = 0;
        
        *OutAttribute = attr;
        return EOS_Success;
    }
    
    EOS_LobbyDetails_CopyAttributeByIndexOptions idxOpts = {};
    idxOpts.ApiVersion = 1;
    idxOpts.AttributeIndex = (uint32_t)index;
    return eos_LobbyDetails_CopyAttributeByIndex(H, &idxOpts, OutAttribute);
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
    Log("EOS_LobbyDetails_GetMemberCount called -> 1");
    return 1;
}

static void* eos_LobbyDetails_GetMemberByIndex(void* H, void* O) {
    Log("EOS_LobbyDetails_GetMemberByIndex called -> FAKE_PRODUCT_USER_ID");
    return FAKE_PRODUCT_USER_ID;
}

static void* eos_LobbyDetails_GetLobbyOwner(void* H, void* O) {
    Log("EOS_LobbyOwner_GetLobbyOwner called -> FAKE_PRODUCT_USER_ID");
    return FAKE_PRODUCT_USER_ID;
}



static void* GetSteamFriendsInterface() {
    static void* friendsInterface = nullptr;
    if (friendsInterface) return friendsInterface;
    
    HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
    if (!hSteam) hSteam = LoadLibraryA("steam_api64_valve.dll");
    if (hSteam) {
        g_pfn_SteamFriends = (fn_SteamFriends_t)GetProcAddress(hSteam, "SteamAPI_SteamFriends_v017");
        g_pfn_GetFriendCount = (fn_GetFriendCount_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_GetFriendCount");
        g_pfn_GetFriendByIndex = (fn_GetFriendByIndex_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_GetFriendByIndex");
        g_pfn_GetFriendPersonaName = (fn_GetFriendPersonaName_t)GetProcAddress(hSteam, "SteamAPI_ISteamFriends_GetFriendPersonaName");
        
        if (g_pfn_SteamFriends) friendsInterface = g_pfn_SteamFriends();
    }
    return friendsInterface;
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

// PROC TABLE SETUP
// =============================================================================
static int FindExportIndex(const char* name) {
    for (int i = 0; i < EOS_FORWARD_COUNT; i++) {
        if (g_eosNames[i] && strcmp(g_eosNames[i], name) == 0) return i;
    }
    return -1;
}

static void Override(const char* name, void* proc) {
    int idx = FindExportIndex(name);
    if (idx >= 0) g_eosProcs[idx] = (FARPROC)proc;
}

static void SetupEmulatedFunctions() {
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
    Override("EOS_Lobby_UpdateLobby",                         (void*)eos_Lobby_UpdateLobby);
    Override("EOS_LobbySearch_Find",                          (void*)eos_LobbySearch_Find);
    Override("EOS_LobbySearch_GetSearchResultCount",          (void*)eos_LobbySearch_GetSearchResultCount);
    Override("EOS_LobbySearch_CopySearchResultByIndex",       (void*)eos_LobbySearch_CopySearchResultByIndex);
    Override("EOS_LobbySearch_SetParameter",                  (void*)eos_LobbySearch_SetParameter);
    Override("EOS_LobbySearch_Release",                       (void*)eos_LobbySearch_Release);
    Override("EOS_Lobby_JoinLobby",                           (void*)eos_Lobby_JoinLobby);
    Override("EOS_Lobby_CreateLobbySearch",                   (void*)eos_Lobby_CreateLobbySearch);
    Override("EOS_Lobby_UpdateLobbyModification",              (void*)eos_Lobby_UpdateLobbyModification);
    Override("EOS_LobbyDetails_CopyInfo",                      (void*)eos_LobbyDetails_CopyInfo);
    Override("EOS_LobbyDetails_GetAttributeCount",             (void*)eos_LobbyDetails_GetAttributeCount);
    Override("EOS_LobbyDetails_CopyAttributeByIndex",          (void*)eos_LobbyDetails_CopyAttributeByIndex);
    Override("EOS_LobbyDetails_CopyAttributeByKey",            (void*)eos_LobbyDetails_CopyAttributeByKey);
    Override("EOS_Lobby_Attribute_Release",                    (void*)eos_Lobby_Attribute_Release);
    Override("EOS_LobbyDetails_GetMemberCount",                (void*)eos_LobbyDetails_GetMemberCount);
    Override("EOS_LobbyDetails_Info_Release",                  (void*)eos_LobbyDetails_Info_Release);
    Override("EOS_LobbyDetails_Release",                       (void*)eos_LobbyDetails_Release);
    Override("EOS_LobbyDetails_GetMemberByIndex",              (void*)eos_LobbyDetails_GetMemberByIndex);
    Override("EOS_LobbyDetails_GetLobbyOwner",                 (void*)eos_LobbyDetails_GetLobbyOwner);
}

// =============================================================================
// ReFix marker
// =============================================================================
extern "C" __declspec(dllexport) int ReFix() {
    return 1;
}

// =============================================================================
// CONFIGURATION
// =============================================================================
static void LoadConfig() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string iniPath(exePath);
    size_t pos = iniPath.find_last_of("\\/");
    if (pos != std::string::npos) iniPath = iniPath.substr(0, pos + 1) + "ReFix.ini";
    
    // Read username from INI first
    GetPrivateProfileStringA("User", "Name", "",
        g_userName, sizeof(g_userName), iniPath.c_str());
    
    // If INI Name is empty, try environment variable set by winmm loader
    if (g_userName[0] == '\0') {
        char envBuf[128] = {0};
        if (GetEnvironmentVariableA("REFIX_USERNAME", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
            strcpy_s(g_userName, sizeof(g_userName), envBuf);
        }
    }
    
    // If still empty, try Steam persona name (set by steam_proxy after SteamAPI_Init)
    // Note: This env var may not be set yet at DllMain time, but we re-check later
    if (g_userName[0] == '\0') {
        char envBuf[128] = {0};
        if (GetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
            strcpy_s(g_userName, sizeof(g_userName), envBuf);
        }
    }
    
    // Final fallback
    if (g_userName[0] == '\0') {
        strcpy_s(g_userName, sizeof(g_userName), "ReFix User");
    }
    
    // Read SteamId from INI
    GetPrivateProfileStringA("User", "SteamId", "",
        g_productUserIdStr, sizeof(g_productUserIdStr), iniPath.c_str());
    
    // If INI SteamId is empty, try env var from steam_proxy
    if (g_productUserIdStr[0] == '\0') {
        char envBuf[64] = {0};
        if (GetEnvironmentVariableA("REFIX_STEAM_ID", envBuf, sizeof(envBuf)) > 0 && envBuf[0] != '\0') {
            strcpy_s(g_productUserIdStr, sizeof(g_productUserIdStr), envBuf);
        }
    }
    
    // Final fallback
    if (g_productUserIdStr[0] == '\0') {
        strcpy_s(g_productUserIdStr, sizeof(g_productUserIdStr), "76561197960287930");
    }
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
            
            // Delete old debug log
            {
                char exePath[MAX_PATH];
                GetModuleFileNameA(NULL, exePath, MAX_PATH);
                std::string logPath(exePath);
                size_t pos = logPath.find_last_of("\\/");
                if (pos != std::string::npos) logPath = logPath.substr(0, pos + 1) + "ReFix_eos_debug.log";
                DeleteFileA(logPath.c_str());
            }
            
            Log("=== ReFix EOS Emulator Debug Log ===");
            Log("DllMain: DLL_PROCESS_ATTACH (EOSSDK-Win64-Shipping.dll ReFix v3)");
            Log("DllMain: Config loaded (UserName=%s, SteamID=%s)", g_userName, g_productUserIdStr);
            
            InitExportNames();
            SetupEmulatedFunctions();
            Log("DllMain: Emulation ready");
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

