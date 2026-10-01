#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#pragma pack(push, 8)
struct CallbackMsg_t {
    int32_t m_hSteamUser;
    int32_t m_iCallback;
    uint8_t* m_pubParam;
    int32_t m_cubParam;
};

struct SteamAPICallCompleted_t {
    enum { k_iCallback = 703 };
    uint64_t m_hAsyncCall;
    int32_t m_iCallback;
    uint32_t m_cubParam;
};

struct LobbyMatchList_t {
    enum { k_iCallback = 510 };
    uint32_t m_nLobbiesMatching;
};

struct LobbyCreated_t {
    enum { k_iCallback = 513 };
    int32_t m_eResult; // 1 = k_EResultOK
    uint64_t m_ulSteamIDLobby;
};
#pragma pack(pop)

typedef bool (*fn_SteamAPI_Init_t)();
typedef bool (*fn_SteamAPI_InitSafe_t)();
typedef int (*fn_SteamAPI_InitFlat_t)(char* pOutErr);
typedef int (*fn_SteamInternal_SteamAPI_Init_t)(const char* pszVer, char* pOutErr);
typedef void (*fn_SteamAPI_Shutdown_t)();
typedef void (*fn_SteamAPI_RunCallbacks_t)();
typedef uint32_t (*fn_SteamAPI_GetHSteamPipe_t)();
typedef uint32_t (*fn_SteamAPI_GetHSteamUser_t)();
typedef void* (*fn_SteamInternal_CreateInterface_t)(const char* ver);
typedef uint32_t (*fn_SteamAPI_ISteamUtils_GetAppID_t)(void* self);

typedef void* (*fn_SteamMatchmaking_t)();
typedef void* (*fn_SteamUtils_t)();

typedef uint64_t (*fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t)(void* self);
typedef void (*fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_t)(void* self, int eLobbyDistanceFilter);
typedef void (*fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t)(void* self, const char* pchKeyToMatch, const char* pchValueToMatch, int eComparisonType);
typedef void (*fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_t)(void* self, const char* pchKeyToMatch, int nValueToMatch, int eComparisonType);
typedef uint64_t (*fn_SteamAPI_ISteamMatchmaking_GetLobbyByIndex_t)(void* self, int iLobby);
typedef const char* (*fn_SteamAPI_ISteamMatchmaking_GetLobbyData_t)(void* self, uint64_t steamIDLobby, const char* pchKey);
typedef int (*fn_SteamAPI_ISteamMatchmaking_GetLobbyDataCount_t)(void* self, uint64_t steamIDLobby);
typedef bool (*fn_SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_t)(void* self, uint64_t steamIDLobby, int iLobbyData, char* pchKey, int cchKeyBufferSize, char* pchValue, int cchValueBufferSize);
typedef uint64_t (*fn_SteamAPI_ISteamMatchmaking_CreateLobby_t)(void* self, int eLobbyType, int cMaxMembers);
typedef bool (*fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t)(void* self, uint64_t steamIDLobby, const char* pchKey, const char* pchValue);
typedef void (*fn_SteamAPI_ISteamMatchmaking_LeaveLobby_t)(void* self, uint64_t steamIDLobby);

typedef void (*fn_SteamAPI_ManualDispatch_Init_t)();
typedef void (*fn_SteamAPI_ManualDispatch_RunFrame_t)(uint32_t hSteamPipe);
typedef bool (*fn_SteamAPI_ManualDispatch_GetNextCallback_t)(uint32_t hSteamPipe, CallbackMsg_t* pCallbackMsg);
typedef void (*fn_SteamAPI_ManualDispatch_FreeLastCallback_t)(uint32_t hSteamPipe);
typedef bool (*fn_SteamAPI_ManualDispatch_GetAPICallResult_t)(uint32_t hSteamPipe, uint64_t hSteamAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed);

static HMODULE g_hSteam = nullptr;
static fn_SteamAPI_Init_t g_pfn_Init = nullptr;
static fn_SteamAPI_InitSafe_t g_pfn_InitSafe = nullptr;
static fn_SteamAPI_InitFlat_t g_pfn_InitFlat = nullptr;
static fn_SteamInternal_SteamAPI_Init_t g_pfn_InternalInit = nullptr;
static fn_SteamAPI_Shutdown_t g_pfn_Shutdown = nullptr;
static fn_SteamAPI_RunCallbacks_t g_pfn_RunCallbacks = nullptr;
static fn_SteamAPI_GetHSteamPipe_t g_pfn_GetHSteamPipe = nullptr;
static fn_SteamAPI_GetHSteamUser_t g_pfn_GetHSteamUser = nullptr;
static fn_SteamInternal_CreateInterface_t g_pfn_CreateInterface = nullptr;
static fn_SteamAPI_ISteamUtils_GetAppID_t g_pfn_GetAppID = nullptr;
static fn_SteamMatchmaking_t g_pfn_SteamMatchmaking = nullptr;
static fn_SteamUtils_t g_pfn_SteamUtils = nullptr;

static fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t g_pfn_RequestLobbyList = nullptr;
static fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_t g_pfn_AddDistanceFilter = nullptr;
static fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t g_pfn_AddStringFilter = nullptr;
static fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_t g_pfn_AddNumericalFilter = nullptr;
static fn_SteamAPI_ISteamMatchmaking_GetLobbyByIndex_t g_pfn_GetLobbyByIndex = nullptr;
static fn_SteamAPI_ISteamMatchmaking_GetLobbyData_t g_pfn_GetLobbyData = nullptr;
static fn_SteamAPI_ISteamMatchmaking_GetLobbyDataCount_t g_pfn_GetLobbyDataCount = nullptr;
static fn_SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_t g_pfn_GetLobbyDataByIndex = nullptr;
static fn_SteamAPI_ISteamMatchmaking_CreateLobby_t g_pfn_CreateLobby = nullptr;
static fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t g_pfn_SetLobbyData = nullptr;
static fn_SteamAPI_ISteamMatchmaking_LeaveLobby_t g_pfn_LeaveLobby = nullptr;

static fn_SteamAPI_ManualDispatch_Init_t g_pfn_ManualDispatch_Init = nullptr;
static fn_SteamAPI_ManualDispatch_RunFrame_t g_pfn_ManualDispatch_RunFrame = nullptr;
static fn_SteamAPI_ManualDispatch_GetNextCallback_t g_pfn_ManualDispatch_GetNextCallback = nullptr;
static fn_SteamAPI_ManualDispatch_FreeLastCallback_t g_pfn_ManualDispatch_FreeLastCallback = nullptr;
static fn_SteamAPI_ManualDispatch_GetAPICallResult_t g_pfn_ManualDispatch_GetAPICallResult = nullptr;

static bool LoadSteamApi(const char* dllPath) {
    g_hSteam = LoadLibraryA(dllPath);
    if (!g_hSteam) {
        printf("[FAIL] Could not load DLL: %s (Error %lu)\n", dllPath, GetLastError());
        return false;
    }
    printf("[OK] Loaded %s at %p\n", dllPath, g_hSteam);

    #define RESOLVE(type, var, name) \
        var = (type)GetProcAddress(g_hSteam, name);

    RESOLVE(fn_SteamAPI_Init_t, g_pfn_Init, "SteamAPI_Init");
    RESOLVE(fn_SteamAPI_InitSafe_t, g_pfn_InitSafe, "SteamAPI_InitSafe");
    RESOLVE(fn_SteamAPI_InitFlat_t, g_pfn_InitFlat, "SteamAPI_InitFlat");
    RESOLVE(fn_SteamInternal_SteamAPI_Init_t, g_pfn_InternalInit, "SteamInternal_SteamAPI_Init");
    RESOLVE(fn_SteamAPI_Shutdown_t, g_pfn_Shutdown, "SteamAPI_Shutdown");
    RESOLVE(fn_SteamAPI_RunCallbacks_t, g_pfn_RunCallbacks, "SteamAPI_RunCallbacks");
    RESOLVE(fn_SteamAPI_GetHSteamPipe_t, g_pfn_GetHSteamPipe, "SteamAPI_GetHSteamPipe");
    RESOLVE(fn_SteamAPI_GetHSteamUser_t, g_pfn_GetHSteamUser, "SteamAPI_GetHSteamUser");
    RESOLVE(fn_SteamInternal_CreateInterface_t, g_pfn_CreateInterface, "SteamInternal_CreateInterface");
    RESOLVE(fn_SteamAPI_ISteamUtils_GetAppID_t, g_pfn_GetAppID, "SteamAPI_ISteamUtils_GetAppID");
    RESOLVE(fn_SteamMatchmaking_t, g_pfn_SteamMatchmaking, "SteamAPI_SteamMatchmaking_v009");
    RESOLVE(fn_SteamUtils_t, g_pfn_SteamUtils, "SteamAPI_SteamUtils_v010");

    RESOLVE(fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t, g_pfn_RequestLobbyList, "SteamAPI_ISteamMatchmaking_RequestLobbyList");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_t, g_pfn_AddDistanceFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t, g_pfn_AddStringFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_t, g_pfn_AddNumericalFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_GetLobbyByIndex_t, g_pfn_GetLobbyByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyByIndex");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_GetLobbyData_t, g_pfn_GetLobbyData, "SteamAPI_ISteamMatchmaking_GetLobbyData");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_GetLobbyDataCount_t, g_pfn_GetLobbyDataCount, "SteamAPI_ISteamMatchmaking_GetLobbyDataCount");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_t, g_pfn_GetLobbyDataByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_CreateLobby_t, g_pfn_CreateLobby, "SteamAPI_ISteamMatchmaking_CreateLobby");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t, g_pfn_SetLobbyData, "SteamAPI_ISteamMatchmaking_SetLobbyData");
    RESOLVE(fn_SteamAPI_ISteamMatchmaking_LeaveLobby_t, g_pfn_LeaveLobby, "SteamAPI_ISteamMatchmaking_LeaveLobby");

    RESOLVE(fn_SteamAPI_ManualDispatch_Init_t, g_pfn_ManualDispatch_Init, "SteamAPI_ManualDispatch_Init");
    RESOLVE(fn_SteamAPI_ManualDispatch_RunFrame_t, g_pfn_ManualDispatch_RunFrame, "SteamAPI_ManualDispatch_RunFrame");
    RESOLVE(fn_SteamAPI_ManualDispatch_GetNextCallback_t, g_pfn_ManualDispatch_GetNextCallback, "SteamAPI_ManualDispatch_GetNextCallback");
    RESOLVE(fn_SteamAPI_ManualDispatch_FreeLastCallback_t, g_pfn_ManualDispatch_FreeLastCallback, "SteamAPI_ManualDispatch_FreeLastCallback");
    RESOLVE(fn_SteamAPI_ManualDispatch_GetAPICallResult_t, g_pfn_ManualDispatch_GetAPICallResult, "SteamAPI_ManualDispatch_GetAPICallResult");

    #undef RESOLVE
    return (g_pfn_Init != nullptr || g_pfn_InitSafe != nullptr || g_pfn_InitFlat != nullptr || g_pfn_InternalInit != nullptr);
}

static void* GetMatchmakingInterface() {
    if (g_pfn_SteamMatchmaking) {
        void* ptr = g_pfn_SteamMatchmaking();
        if (ptr) return ptr;
    }
    if (g_pfn_CreateInterface) {
        void* ptr = g_pfn_CreateInterface("SteamMatchMaking009");
        if (ptr) return ptr;
    }
    return nullptr;
}

static void* GetUtilsInterface() {
    if (g_pfn_SteamUtils) {
        void* ptr = g_pfn_SteamUtils();
        if (ptr) return ptr;
    }
    if (g_pfn_CreateInterface) {
        void* ptr = g_pfn_CreateInterface("SteamUtils010");
        if (ptr) return ptr;
    }
    return nullptr;
}

static bool WaitForAPICall(uint32_t pipe, uint64_t hCall, int expectedCallback, void* outBuf, int bufSize, int timeoutSec) {
    auto start = std::chrono::steady_clock::now();
    bool completed = false;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= timeoutSec) {
            printf("[TIMEOUT] Timed out waiting for APICall %llu after %d seconds\n", hCall, timeoutSec);
            return false;
        }

        MSG winMsg;
        while (PeekMessageA(&winMsg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&winMsg);
            DispatchMessageA(&winMsg);
        }

        if (g_pfn_ManualDispatch_RunFrame) {
            g_pfn_ManualDispatch_RunFrame(pipe);
        } else if (g_pfn_RunCallbacks) {
            g_pfn_RunCallbacks();
        }

        if (g_pfn_ManualDispatch_GetNextCallback) {
            CallbackMsg_t msg{};
            while (g_pfn_ManualDispatch_GetNextCallback(pipe, &msg)) {
                printf("[CALLBACK] iCallback=%d, cubParam=%d\n", msg.m_iCallback, msg.m_cubParam);
                if (msg.m_iCallback == 703) { // SteamAPICallCompleted_t
                    auto* pComp = (SteamAPICallCompleted_t*)msg.m_pubParam;
                    if (pComp) {
                        printf("[CALLBACK 703] SteamAPICallCompleted: hAsyncCall=%llu, iCallback=%d, cubParam=%u\n",
                               pComp->m_hAsyncCall, pComp->m_iCallback, pComp->m_cubParam);
                        if (pComp->m_hAsyncCall == hCall) {
                            completed = true;
                        }
                    }
                } else if (msg.m_iCallback == expectedCallback) {
                    printf("[CALLBACK %d] Direct expected callback received\n", msg.m_iCallback);
                    if (outBuf && msg.m_cubParam <= bufSize) {
                        memcpy(outBuf, msg.m_pubParam, msg.m_cubParam);
                    }
                    completed = true;
                }
                if (g_pfn_ManualDispatch_FreeLastCallback) {
                    g_pfn_ManualDispatch_FreeLastCallback(pipe);
                }
            }
        }

        if (completed) {
            if (g_pfn_ManualDispatch_GetAPICallResult) {
                bool bFailed = false;
                bool ok = g_pfn_ManualDispatch_GetAPICallResult(pipe, hCall, outBuf, bufSize, expectedCallback, &bFailed);
                printf("[APICallResult] hCall=%llu, ok=%d, bFailed=%d\n", hCall, ok ? 1 : 0, bFailed ? 1 : 0);
                return ok && !bFailed;
            }
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    const char* dllPath = (argc > 1) ? argv[1] : "steam_api64.dll";
    printf("====================================================================\n");
    printf("ReFix Steamworks Matchmaking Test\n");
    printf("Target DLL: %s\n", dllPath);
    printf("====================================================================\n");

    if (!LoadSteamApi(dllPath)) {
        return 1;
    }

    bool initOk = false;
    char errMsg[1024] = { 0 };
    if (g_pfn_InternalInit) {
        initOk = (g_pfn_InternalInit("", errMsg) == 0);
        printf("[INIT] SteamInternal_SteamAPI_Init returned %d (msg='%s')\n", initOk ? 0 : 1, errMsg);
    } else if (g_pfn_InitFlat) {
        initOk = (g_pfn_InitFlat(errMsg) == 0);
        printf("[INIT] SteamAPI_InitFlat returned %d (msg='%s')\n", initOk ? 0 : 1, errMsg);
    } else if (g_pfn_InitSafe) {
        initOk = g_pfn_InitSafe();
        printf("[INIT] SteamAPI_InitSafe returned %d\n", initOk ? 1 : 0);
    } else if (g_pfn_Init) {
        initOk = g_pfn_Init();
        printf("[INIT] SteamAPI_Init returned %d\n", initOk ? 1 : 0);
    } else {
        printf("[FAIL] No supported SteamAPI init export found!\n");
        return 1;
    }

    if (!initOk) {
        printf("[FAIL] SteamAPI init failed! Is Steam client running?\n");
        return 1;
    }
    printf("[OK] SteamAPI initialized successfully!\n");

    if (g_pfn_ManualDispatch_Init) {
        g_pfn_ManualDispatch_Init();
        printf("[OK] SteamAPI_ManualDispatch_Init called after init\n");
    }

    uint32_t pipe = g_pfn_GetHSteamPipe ? g_pfn_GetHSteamPipe() : 0;
    uint32_t user = g_pfn_GetHSteamUser ? g_pfn_GetHSteamUser() : 0;
    printf("[INFO] HSteamPipe=%u, HSteamUser=%u\n", pipe, user);

    void* pUtils = GetUtilsInterface();
    if (pUtils && g_pfn_GetAppID) {
        uint32_t appId = g_pfn_GetAppID(pUtils);
        printf("[INFO] SteamUtils()->GetAppID() = %u\n", appId);
    }

    void* pMatchmaking = GetMatchmakingInterface();
    if (!pMatchmaking) {
        printf("[FAIL] Could not get ISteamMatchmaking interface\n");
        g_pfn_Shutdown();
        return 1;
    }
    printf("[OK] ISteamMatchmaking pointer: %p\n", pMatchmaking);

    // -----------------------------------------------------------------------
    // Test A: RequestLobbyList WITHOUT Distance Filter
    // -----------------------------------------------------------------------
    printf("\n--- [TEST A] RequestLobbyList WITHOUT Distance Filter ---\n");
    uint64_t hCallA = g_pfn_RequestLobbyList(pMatchmaking);
    printf("-> APICallHandle: %llu\n", hCallA);

    LobbyMatchList_t matchResultA{};
    if (WaitForAPICall(pipe, hCallA, 510, &matchResultA, sizeof(matchResultA), 10)) {
        printf("[TEST A RESULT] Lobbies matching: %u\n", matchResultA.m_nLobbiesMatching);
        for (uint32_t i = 0; i < matchResultA.m_nLobbiesMatching && i < 10; ++i) {
            uint64_t lobbyId = g_pfn_GetLobbyByIndex ? g_pfn_GetLobbyByIndex(pMatchmaking, (int)i) : 0;
            const char* name = g_pfn_GetLobbyData ? g_pfn_GetLobbyData(pMatchmaking, lobbyId, "name") : "";
            printf("  Lobby[%u] ID=%llu, name='%s'\n", i, lobbyId, name ? name : "");
        }
    } else {
        printf("[TEST A FAIL] APICall failed or timed out\n");
    }

    // -----------------------------------------------------------------------
    // Test B: RequestLobbyList WITH Distance Filter = Worldwide (3)
    // -----------------------------------------------------------------------
    printf("\n--- [TEST B] RequestLobbyList WITH Distance Filter = Worldwide (3) ---\n");
    if (g_pfn_AddDistanceFilter) {
        g_pfn_AddDistanceFilter(pMatchmaking, 3 /* Worldwide */);
        printf("-> Set DistanceFilter = Worldwide (3)\n");
    }
    uint64_t hCallB = g_pfn_RequestLobbyList(pMatchmaking);
    printf("-> APICallHandle: %llu\n", hCallB);

    LobbyMatchList_t matchResultB{};
    if (WaitForAPICall(pipe, hCallB, 510, &matchResultB, sizeof(matchResultB), 10)) {
        printf("[TEST B RESULT] Lobbies matching: %u\n", matchResultB.m_nLobbiesMatching);
        for (uint32_t i = 0; i < matchResultB.m_nLobbiesMatching && i < 10; ++i) {
            uint64_t lobbyId = g_pfn_GetLobbyByIndex ? g_pfn_GetLobbyByIndex(pMatchmaking, (int)i) : 0;
            const char* name = g_pfn_GetLobbyData ? g_pfn_GetLobbyData(pMatchmaking, lobbyId, "name") : "";
            printf("  Lobby[%u] ID=%llu, name='%s'\n", i, lobbyId, name ? name : "");
        }
    } else {
        printf("[TEST B FAIL] APICall failed or timed out\n");
    }

    // -----------------------------------------------------------------------
    // Test C: RequestLobbyList WITH Distance Filter = Default (1)
    // -----------------------------------------------------------------------
    printf("\n--- [TEST C] RequestLobbyList WITH Distance Filter = Default (1) ---\n");
    if (g_pfn_AddDistanceFilter) {
        g_pfn_AddDistanceFilter(pMatchmaking, 1 /* Default */);
        printf("-> Set DistanceFilter = Default (1)\n");
    }
    uint64_t hCallC = g_pfn_RequestLobbyList(pMatchmaking);
    printf("-> APICallHandle: %llu\n", hCallC);

    LobbyMatchList_t matchResultC{};
    if (WaitForAPICall(pipe, hCallC, 510, &matchResultC, sizeof(matchResultC), 10)) {
        printf("[TEST C RESULT] Lobbies matching: %u\n", matchResultC.m_nLobbiesMatching);
    } else {
        printf("[TEST C FAIL] APICall failed or timed out\n");
    }

    // -----------------------------------------------------------------------
    // Test D: Create a Lobby, then Query
    // -----------------------------------------------------------------------
    printf("\n--- [TEST D] Create Lobby, Set Data, and Verify Discovery ---\n");
    if (g_pfn_CreateLobby) {
        uint64_t hCallCreate = g_pfn_CreateLobby(pMatchmaking, 2 /* k_ELobbyTypePublic */, 4);
        printf("-> CreateLobby APICallHandle: %llu\n", hCallCreate);
        LobbyCreated_t createdResult{};
        if (WaitForAPICall(pipe, hCallCreate, 513, &createdResult, sizeof(createdResult), 10)) {
            printf("[CREATE RESULT] eResult=%d, LobbyID=%llu\n", createdResult.m_eResult, createdResult.m_ulSteamIDLobby);
            if (createdResult.m_eResult == 1 && createdResult.m_ulSteamIDLobby != 0) {
                uint64_t myLobby = createdResult.m_ulSteamIDLobby;
                if (g_pfn_SetLobbyData) {
                    g_pfn_SetLobbyData(pMatchmaking, myLobby, "name", "ReFix_Discovery_Test");
                    g_pfn_SetLobbyData(pMatchmaking, myLobby, "game_filter", "3722330");
                    printf("-> Set lobby metadata name='ReFix_Discovery_Test', game_filter='3722330'\n");
                }

                // Query again without filter
                printf("-> Querying lobbies to see if created lobby is discovered...\n");
                uint64_t hCallD = g_pfn_RequestLobbyList(pMatchmaking);
                LobbyMatchList_t matchResultD{};
                if (WaitForAPICall(pipe, hCallD, 510, &matchResultD, sizeof(matchResultD), 10)) {
                    printf("[TEST D DISCOVERY RESULT] Lobbies matching: %u\n", matchResultD.m_nLobbiesMatching);
                    bool foundSelf = false;
                    for (uint32_t i = 0; i < matchResultD.m_nLobbiesMatching; ++i) {
                        uint64_t lobbyId = g_pfn_GetLobbyByIndex ? g_pfn_GetLobbyByIndex(pMatchmaking, (int)i) : 0;
                        if (lobbyId == myLobby) {
                            foundSelf = true;
                            printf("  -> FOUND SELF LOBBY [%u] ID=%llu!\n", i, lobbyId);
                        }
                    }
                    if (!foundSelf) {
                        printf("  -> Note: Created lobby %llu was not in the returned list (Steam backend behavior)\n", myLobby);
                    }
                }

                // Leave lobby
                if (g_pfn_LeaveLobby) {
                    g_pfn_LeaveLobby(pMatchmaking, myLobby);
                    printf("-> Left lobby %llu\n", myLobby);
                }
            }
        }
    }

    g_pfn_Shutdown();
    printf("\n[OK] SteamAPI_Shutdown called. Test completed.\n");
    return 0;
}
