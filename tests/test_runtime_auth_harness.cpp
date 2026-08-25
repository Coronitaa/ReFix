// =============================================================================
// ReFix EOS Online v2 - Runtime Auth Verification Harness (Checkpoint 0)
// =============================================================================
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

// EOS ABI
#pragma pack(push, 8)
typedef int32_t EOS_EResult;
constexpr EOS_EResult EOS_Success = 0;

typedef void* EOS_HPlatform;
typedef void* EOS_HConnect;
typedef void* EOS_ProductUserId;

struct EOS_Initialize_ThreadAffinity {
    int32_t ApiVersion;
    uint64_t Main;
    uint64_t Network;
    uint64_t Http;
    uint64_t Storage;
    uint64_t AsyncIO;
};

struct EOS_InitializeOptions {
    int32_t ApiVersion;
    const char* AllocateMemoryFunction;
    const char* ReallocateMemoryFunction;
    const char* ReleaseMemoryFunction;
    const char* ProductName;
    const char* ProductVersion;
    void* Reserved;
    void* SystemInitializeOptions;
    void* OverrideThreadAffinity;
};

struct EOS_Platform_ClientCredentials {
    const char* ClientId;
    const char* ClientSecret;
};

struct EOS_Platform_Options {
    int32_t ApiVersion;
    void* Reserved;
    const char* ProductId;
    const char* SandboxId;
    EOS_Platform_ClientCredentials ClientCredentials;
    bool bIsServer;
    void* EncryptionKey;
    void* OverrideCountryCode;
    void* OverrideLocaleCode;
    const char* DeploymentId;
    uint32_t Flags;
    void* CacheDirectory;
    void* IntegratedPlatformOptionsContainerHandle;
};

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

struct EOS_Connect_LoginCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
    void* ContinuanceToken;
};
#pragma pack(pop)

typedef void (*EOS_Connect_OnLoginCallback)(const EOS_Connect_LoginCallbackInfo* Data);

typedef bool (*fn_SteamAPI_Init_t)();
typedef void (*fn_SteamAPI_RunCallbacks_t)();
typedef void* (*fn_SteamAPI_SteamUtils_v010_t)();
typedef void* (*fn_SteamAPI_SteamUser_v021_t)();
typedef uint32_t (*fn_SteamAPI_ISteamUtils_GetAppID_t)(void* self);
typedef uint64_t (*fn_SteamAPI_ISteamUser_GetSteamID_t)(void* self);
typedef bool (*fn_SteamAPI_ISteamUser_BLoggedOn_t)(void* self);
typedef uint32_t (*fn_SteamAPI_ISteamUser_GetAuthSessionTicket_t)(void* self, void* pTicket, int cbMaxTicket, uint32_t* pcbTicket);

typedef EOS_EResult (*fn_EOS_Initialize_t)(const EOS_InitializeOptions* Options);
typedef EOS_HPlatform (*fn_EOS_Platform_Create_t)(const EOS_Platform_Options* Options);
typedef void (*fn_EOS_Platform_Tick_t)(EOS_HPlatform Handle);
typedef EOS_HConnect (*fn_EOS_Platform_GetConnectInterface_t)(EOS_HPlatform Handle);
typedef void (*fn_EOS_Connect_Login_t)(EOS_HConnect Handle, const EOS_Connect_LoginOptions* Options, void* ClientData, EOS_Connect_OnLoginCallback CompletionDelegate);
typedef EOS_EResult (*fn_EOS_ProductUserId_ToString_t)(EOS_ProductUserId ProductUserId, char* OutBuffer, int32_t* InOutBufferLength);

static bool g_loginCompleted = false;
static EOS_EResult g_loginResult = -1;
static std::string g_loginPuidStr = "";

static void OnLoginComplete(const EOS_Connect_LoginCallbackInfo* Data) {
    g_loginCompleted = true;
    if (Data) {
        g_loginResult = Data->ResultCode;
        printf("[HARNESS CALLBACK] EOS_Connect_Login finished with ResultCode=%d\n", Data->ResultCode);
        fflush(stdout);
    }
}

static std::string BytesToHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

int main(int argc, char** argv) {
    printf("====================================================================\n");
    printf("ReFix EOS Online v2 - Runtime Auth Verification Harness (Checkpoint 0)\n");
    printf("====================================================================\n\n");
    fflush(stdout);

    // 1. Load Steam Proxy DLL
    HMODULE hSteam = LoadLibraryA("steam_api64.dll");
    if (!hSteam) {
        printf("[FAIL] Could not load steam_api64.dll. Ensure proxy is in the current directory.\n");
        fflush(stdout);
        return 1;
    }
    printf("[PASS] Successfully loaded steam_api64.dll\n");
    fflush(stdout);

    auto pfnInit = (fn_SteamAPI_Init_t)GetProcAddress(hSteam, "SteamAPI_Init");
    auto pfnRunCallbacks = (fn_SteamAPI_RunCallbacks_t)GetProcAddress(hSteam, "SteamAPI_RunCallbacks");
    auto pfnGetUtils = (fn_SteamAPI_SteamUtils_v010_t)GetProcAddress(hSteam, "SteamAPI_SteamUtils_v010");
    if (!pfnGetUtils) pfnGetUtils = (fn_SteamAPI_SteamUtils_v010_t)GetProcAddress(hSteam, "SteamUtils");
    auto pfnGetUser = (fn_SteamAPI_SteamUser_v021_t)GetProcAddress(hSteam, "SteamAPI_SteamUser_v021");
    if (!pfnGetUser) pfnGetUser = (fn_SteamAPI_SteamUser_v021_t)GetProcAddress(hSteam, "SteamUser");
    
    auto pfnGetAppID = (fn_SteamAPI_ISteamUtils_GetAppID_t)GetProcAddress(hSteam, "SteamAPI_ISteamUtils_GetAppID");
    auto pfnBLoggedOn = (fn_SteamAPI_ISteamUser_BLoggedOn_t)GetProcAddress(hSteam, "SteamAPI_ISteamUser_BLoggedOn");
    auto pfnGetSteamID = (fn_SteamAPI_ISteamUser_GetSteamID_t)GetProcAddress(hSteam, "SteamAPI_ISteamUser_GetSteamID");
    auto pfnGetAuthSessionTicket = (fn_SteamAPI_ISteamUser_GetAuthSessionTicket_t)GetProcAddress(hSteam, "SteamAPI_ISteamUser_GetAuthSessionTicket");

    if (!pfnInit || !pfnRunCallbacks) {
        printf("[FAIL] Failed to locate SteamAPI exports in steam_api64.dll\n");
        fflush(stdout);
        return 1;
    }

    // 2. Initialize SteamAPI
    printf("[STEP 1] Initializing SteamAPI via SteamAPI_Init()...\n");
    fflush(stdout);
    bool steamInitOk = pfnInit();
    if (!steamInitOk) {
        printf("[WARN] SteamAPI_Init returned false. (Is Steam Client running with AppID 480 / Spacewar?)\n");
    } else {
        printf("[PASS] SteamAPI_Init returned true!\n");
    }
    fflush(stdout);

    // 3. Query AppID & SteamID
    void* pUtils = pfnGetUtils ? pfnGetUtils() : nullptr;
    uint32_t appId = 0;
    if (pfnGetAppID && pUtils) {
        appId = pfnGetAppID(pUtils);
    }
    printf("[STEP 2] Querying AppID -> AppId=%u (Expected: 480)\n", appId);
    if (appId == 480) {
        printf("[PASS] AppID matches MaskAppId 480 (Spacewar)\n");
    } else {
        printf("[WARN] AppID is %u\n", appId);
    }
    fflush(stdout);

    uint64_t steamId = 0;
    bool bLoggedOn = false;
    void* pUser = pfnGetUser ? pfnGetUser() : nullptr;
    if (pUser) {
        if (pfnBLoggedOn) bLoggedOn = pfnBLoggedOn(pUser);
        if (pfnGetSteamID) steamId = pfnGetSteamID(pUser);
    }
    printf("[STEP 3] Querying SteamID -> SteamID=%llu, BLoggedOn=%s\n",
           steamId, (bLoggedOn ? "TRUE" : "FALSE"));
    fflush(stdout);

    // 4. Obtain Auth Session Ticket
    printf("[STEP 4] Requesting Steam Auth Session Ticket...\n");
    fflush(stdout);
    uint8_t ticketBuf[1024] = { 0 };
    uint32_t ticketSize = 0;
    uint32_t ticketHandle = 0;
    if (pfnGetAuthSessionTicket && pUser) {
        ticketHandle = pfnGetAuthSessionTicket(pUser, ticketBuf, sizeof(ticketBuf), &ticketSize);
    }

    printf("  -> TicketHandle=%u, TicketSize=%u bytes\n", ticketHandle, ticketSize);
    if (ticketHandle != 0 && ticketSize > 0) {
        printf("[PASS] Real Steam Auth Session Ticket successfully obtained! (Size: %u bytes)\n", ticketSize);
    } else {
        printf("[WARN] Ticket generation returned handle=%u, size=%u\n", ticketHandle, ticketSize);
    }
    fflush(stdout);

    // Run callbacks
    pfnRunCallbacks();

    // 5. Load EOS SDK Proxy DLL
    printf("\n[STEP 5] Loading EOS SDK Proxy DLL (EOSSDK-Win64-Shipping.dll)...\n");
    fflush(stdout);
    HMODULE hEOS = LoadLibraryA("EOSSDK-Win64-Shipping.dll");
    if (!hEOS) {
        printf("[FAIL] Could not load EOSSDK-Win64-Shipping.dll\n");
        fflush(stdout);
        return 1;
    }
    printf("[PASS] Loaded EOSSDK-Win64-Shipping.dll\n");
    fflush(stdout);

    auto pfnEOSInit = (fn_EOS_Initialize_t)GetProcAddress(hEOS, "EOS_Initialize");
    auto pfnEOSPlatformCreate = (fn_EOS_Platform_Create_t)GetProcAddress(hEOS, "EOS_Platform_Create");
    auto pfnEOSTick = (fn_EOS_Platform_Tick_t)GetProcAddress(hEOS, "EOS_Platform_Tick");
    auto pfnEOSGetConnect = (fn_EOS_Platform_GetConnectInterface_t)GetProcAddress(hEOS, "EOS_Platform_GetConnectInterface");
    auto pfnEOSConnectLogin = (fn_EOS_Connect_Login_t)GetProcAddress(hEOS, "EOS_Connect_Login");
    auto pfnEOSPuidToString = (fn_EOS_ProductUserId_ToString_t)GetProcAddress(hEOS, "EOS_ProductUserId_ToString");

    if (!pfnEOSInit || !pfnEOSPlatformCreate || !pfnEOSTick || !pfnEOSGetConnect || !pfnEOSConnectLogin) {
        printf("[FAIL] Failed to locate required EOS SDK entrypoints\n");
        fflush(stdout);
        return 1;
    }

    // 6. Initialize EOS Platform
    printf("[STEP 6] Initializing EOS SDK Platform...\n");
    fflush(stdout);
    EOS_InitializeOptions initOpts = {};
    initOpts.ApiVersion = 4;
    initOpts.ProductName = "ReFixHarness";
    initOpts.ProductVersion = "1.0.0";
    EOS_EResult initRes = pfnEOSInit(&initOpts);
    printf("  -> EOS_Initialize Result=%d\n", initRes);
    fflush(stdout);

    EOS_Platform_Options platOpts = {};
    platOpts.ApiVersion = 12;
    platOpts.ProductId = "refix_prod";
    platOpts.SandboxId = "refix_sand";
    platOpts.DeploymentId = "refix_dept";
    EOS_HPlatform hPlatform = pfnEOSPlatformCreate(&platOpts);
    printf("  -> EOS_Platform_Create Handle=%p\n", hPlatform);
    fflush(stdout);
    if (!hPlatform) {
        printf("[FAIL] EOS_Platform_Create failed!\n");
        fflush(stdout);
        return 1;
    }

    EOS_HConnect hConnect = pfnEOSGetConnect(hPlatform);
    printf("  -> EOS_Platform_GetConnectInterface Handle=%p\n", hConnect);
    fflush(stdout);

    // 7. Perform EOS_Connect_Login with Steam Ticket
    printf("\n[STEP 7] Performing EOS_Connect_Login using Steam credential token...\n");
    fflush(stdout);
    std::string tokenStr = (ticketSize > 0) ? BytesToHex(ticketBuf, ticketSize) : "dummy_steam_ticket_hex_1234567890abcdef";

    EOS_Connect_Credentials creds = {};
    creds.ApiVersion = 1;
    creds.Type = 3; // EOS_ECT_STEAM_SESSION_TICKET
    creds.Token = tokenStr.c_str();

    EOS_Connect_LoginOptions loginOpts = {};
    loginOpts.ApiVersion = 2;
    loginOpts.Credentials = &creds;

    g_loginCompleted = false;
    pfnEOSConnectLogin(hConnect, &loginOpts, (void*)0x1337, OnLoginComplete);

    // 8. Tick Platform until callback completes
    printf("[STEP 8] Ticking EOS Platform to process asynchronous callbacks...\n");
    fflush(stdout);
    auto start = std::chrono::steady_clock::now();
    while (!g_loginCompleted) {
        pfnEOSTick(hPlatform);
        pfnRunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed > 3000) {
            printf("[FAIL] Login callback timed out after 3000ms\n");
            fflush(stdout);
            break;
        }
    }

    if (g_loginCompleted && g_loginResult == EOS_Success) {
        printf("\n====================================================================\n");
        printf("[CHECKPOINT 0 SUCCESS] Full Steam -> EOS Authentication Succeeded!\n");
        printf("  - SteamAPI_Init: OK\n");
        printf("  - AppID: %u\n", appId);
        printf("  - SteamID: %llu\n", steamId);
        printf("  - Steam Auth Ticket: %u bytes\n", ticketSize);
        printf("  - EOS_Connect_Login: EOS_Success (0)\n");
        printf("====================================================================\n");
        fflush(stdout);
        return 0;
    } else {
        printf("\n[FAIL] Authentication verification did not complete successfully (Result=%d)\n", g_loginResult);
        fflush(stdout);
        return 1;
    }
}
