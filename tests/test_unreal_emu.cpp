#include <windows.h>
#include <cstdio>
#include <cstdint>

typedef bool (*fn_SteamAPI_Init)();
typedef void (*fn_SteamAPI_Shutdown)();
typedef void (*fn_SteamAPI_RunCallbacks)();
typedef void* (*fn_SteamInternal_FindOrCreateUserInterface)(uint32_t, const char*);
typedef void* (*fn_SteamInternal_ContextInit)(void*);
typedef uint32_t (*fn_SteamAPI_ISteamUser_GetAuthTicketForWebApi)(void*, const char*);

int main() {
    printf("=== Re:Goldberg Unreal Steam Emulation Integration Test ===\n");
    fflush(stdout);

    HMODULE hSteam = LoadLibraryA("steam_api64.dll");
    if (!hSteam) {
        printf("[FAIL] Could not load steam_api64.dll (Error=%lu)\n", GetLastError());
        fflush(stdout);
        return 101;
    }
    printf("[PASS] Loaded steam_api64.dll\n");
    fflush(stdout);

    auto pInit = (fn_SteamAPI_Init)GetProcAddress(hSteam, "SteamAPI_Init");
    auto pShutdown = (fn_SteamAPI_Shutdown)GetProcAddress(hSteam, "SteamAPI_Shutdown");
    auto pRunCallbacks = (fn_SteamAPI_RunCallbacks)GetProcAddress(hSteam, "SteamAPI_RunCallbacks");
    auto pFindUserIface = (fn_SteamInternal_FindOrCreateUserInterface)GetProcAddress(hSteam, "SteamInternal_FindOrCreateUserInterface");
    auto pContextInit = (fn_SteamInternal_ContextInit)GetProcAddress(hSteam, "SteamInternal_ContextInit");
    auto pGetWebTicket = (fn_SteamAPI_ISteamUser_GetAuthTicketForWebApi)GetProcAddress(hSteam, "SteamAPI_ISteamUser_GetAuthTicketForWebApi");

    if (!pInit || !pFindUserIface || !pContextInit) {
        printf("[FAIL] Required Steam exports missing: pInit=%p, pFindUserIface=%p, pContextInit=%p\n", pInit, pFindUserIface, pContextInit);
        fflush(stdout);
        return 102;
    }
    printf("[PASS] Resolved key Steam exports\n");
    fflush(stdout);

    // Test 1: Initialize
    bool initOk = pInit();
    printf("[TEST 1] SteamAPI_Init() -> %s\n", initOk ? "SUCCESS" : "FAILED");
    fflush(stdout);
    if (!initOk) return 103;

    // Test 2: FindOrCreateUserInterface
    void* pUser = pFindUserIface(0, "SteamUser021");
    printf("[TEST 2a] SteamInternal_FindOrCreateUserInterface(0, 'SteamUser021') -> %p\n", pUser);
    fflush(stdout);
    if (!pUser) return 104;

    void* pFriends = pFindUserIface(0, "SteamFriends017");
    printf("[TEST 2b] SteamInternal_FindOrCreateUserInterface(0, 'SteamFriends017') -> %p\n", pFriends);
    fflush(stdout);
    if (!pFriends) return 105;

    void* pMatchmaking = pFindUserIface(0, "SteamMatchMaking009");
    printf("[TEST 2c] SteamInternal_FindOrCreateUserInterface(0, 'SteamMatchMaking009') -> %p\n", pMatchmaking);
    fflush(stdout);
    if (!pMatchmaking) return 106;

    void* pSockets = pFindUserIface(0, "SteamNetworkingSockets012");
    printf("[TEST 2d] SteamInternal_FindOrCreateUserInterface(0, 'SteamNetworkingSockets012') -> %p\n", pSockets);
    fflush(stdout);
    if (!pSockets) return 107;

    // Test 3: WebAPI Auth Ticket (RedpointEOS requirement)
    uint32_t ticketHandle = pGetWebTicket ? pGetWebTicket(pUser, "epiconlineservices") : 0;
    printf("[TEST 3] SteamAPI_ISteamUser_GetAuthTicketForWebApi('epiconlineservices') -> Handle=%u\n", ticketHandle);
    fflush(stdout);
    if (ticketHandle == 0) return 108;

    // Test 4: RunCallbacks
    if (pRunCallbacks) {
        pRunCallbacks();
        printf("[TEST 4] SteamAPI_RunCallbacks() -> OK\n");
        fflush(stdout);
    }

    // Test 5: Shutdown
    if (pShutdown) {
        pShutdown();
        printf("[TEST 5] SteamAPI_Shutdown() -> OK\n");
        fflush(stdout);
    }

    FreeLibrary(hSteam);
    printf("=== ALL TESTS PASSED SUCCESSFULLY! ===\n");
    fflush(stdout);
    return 0;
}
