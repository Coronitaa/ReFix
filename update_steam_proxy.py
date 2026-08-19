import os

names_file = r"c:\Users\Valen\Desktop\STEAM_CRACKING\ReFix\src\steam_export_names_generated.txt"
proxy_file = r"c:\Users\Valen\Desktop\STEAM_CRACKING\ReFix\src\steam_proxy.cpp"

with open(names_file, "r") as f:
    lines = [line.strip() for line in f if line.strip()]

print(f"Loaded {len(lines)} export names.")

names_formatted = "\n    ".join(lines)

code = f"""// =============================================================================
// ReFix - steam_api64.dll Proxy (Active Matchmaking & Direct P2P UPnP Helper)
// =============================================================================
// Forwards {len(lines)} exports to steam_api64_valve.dll via ASM jump table (g_steamProcs).
// Intercepts SteamAPI_Init, ISteamMatchmaking (CreateLobby, RequestLobbyList, SetLobbyData),
// and callbacks (GameLobbyJoinRequested_t, GameRichPresenceJoinRequested_t).
//
// Native UPnP Router Port Forwarding & Windows Firewall rule added on startup.
// Registers Direct P2P Connect string in Steam Rich Presence (+connect <PUBLIC_IP>:7777).
// =============================================================================

#pragma comment(lib, "user32.lib")
#include <windows.h>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include "upnp_firewall.h"

#define STEAM_FORWARD_COUNT {len(lines)}

extern "C" {{
    __declspec(dllexport) FARPROC g_steamProcs[STEAM_FORWARD_COUNT] = {{ 0 }};
}}

static const char* g_forwardNames[STEAM_FORWARD_COUNT] = {{
    {names_formatted}
}};

static HMODULE g_hOriginalDll = nullptr;
static bool g_configLoaded = false;
static char g_maskAppId[16] = "480";
static uint32_t g_maskAppIdNum = 480;
static char g_language[32] = "english";
static std::string g_hostPublicIP = "";
static std::string g_hostLocalIP = "";

// Function pointer typedefs
typedef bool(*fn_SteamAPI_Init_t)();
typedef bool(*fn_SteamAPI_RestartAppIfNecessary_t)(unsigned int);
typedef bool(*fn_SteamInternal_GameServer_Init_t)(uint32_t, uint16_t, uint16_t, int, const char*);
typedef bool(*fn_SteamGameServer_InitSafe_t)();

typedef const char* (*fn_GetPersonaName_t)(void* self);
typedef void* (*fn_SteamFriends_v017_t)();

typedef uint64_t(*fn_GetSteamID_t)(void* self);
typedef void* (*fn_SteamUser_v021_t)();

typedef void* (*fn_RequestServerList4_t)(void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse);
typedef void* (*fn_RequestLANServerList_t)(void* self, uint32_t iApp, void* pResponse);
typedef int (*fn_AddFavoriteGame_t)(void* self, uint32_t nAppID, uint32_t nIP, uint16_t nConnPort, uint16_t nQueryPort, uint32_t unFlags, uint32_t rTime32);

typedef void(*fn_SteamAPI_RegisterCallback_t)(void* pCallback, int iCallback);
typedef uint64_t(*fn_SteamAPI_ISteamMatchmaking_CreateLobby_t)(void* self, int eLobbyType, int cMaxMembers);
typedef bool(*fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t)(void* self, uint64_t steamIDLobby, const char* pchKey, const char* pchValue);
typedef void(*fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t)(void* self);
typedef void(*fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t)(void* self, const char* pchKeyToMatch, const char* pchValueToMatch, int eComparisonType);
typedef bool(*fn_SteamAPI_ISteamFriends_SetRichPresence_t)(void* self, const char* pchKey, const char* pchValue);

static fn_SteamAPI_Init_t g_pfn_Init = nullptr;
static fn_SteamAPI_Init_t g_pfn_InitSafe = nullptr;
static fn_SteamAPI_Init_t g_pfn_InitAnon = nullptr;
static fn_SteamAPI_RestartAppIfNecessary_t g_pfn_Restart = nullptr;
static fn_SteamInternal_GameServer_Init_t g_pfn_GSInit = nullptr;
static fn_SteamGameServer_InitSafe_t g_pfn_GSInitSafe = nullptr;

static fn_GetPersonaName_t g_pfn_GetPersonaName = nullptr;
static fn_SteamFriends_v017_t g_pfn_SteamFriends = nullptr;
static fn_GetSteamID_t g_pfn_GetSteamID = nullptr;
static fn_SteamUser_v021_t g_pfn_SteamUser = nullptr;

static fn_RequestServerList4_t g_pfn_RequestFavoritesServerList = nullptr;
static fn_RequestServerList4_t g_pfn_RequestFriendsServerList = nullptr;
static fn_RequestServerList4_t g_pfn_RequestHistoryServerList = nullptr;
static fn_RequestServerList4_t g_pfn_RequestInternetServerList = nullptr;
static fn_RequestLANServerList_t g_pfn_RequestLANServerList = nullptr;
static fn_RequestServerList4_t g_pfn_RequestSpectatorServerList = nullptr;
static fn_AddFavoriteGame_t g_pfn_AddFavoriteGame = nullptr;

static fn_SteamAPI_ISteamMatchmaking_CreateLobby_t g_pfn_CreateLobby = nullptr;
static fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t g_pfn_SetLobbyData = nullptr;
static fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t g_pfn_RequestLobbyList = nullptr;
static fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t g_pfn_AddRequestLobbyListStringFilter = nullptr;
static fn_SteamAPI_RegisterCallback_t g_pfn_RegisterCallback = nullptr;
static fn_SteamAPI_ISteamFriends_SetRichPresence_t g_pfn_SetRichPresence = nullptr;

void ReFixLog(const char* fmt, ...) {{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string logPath(exePath);
    size_t pos = logPath.find_last_of("\\\\/");
    if (pos != std::string::npos) logPath = logPath.substr(0, pos + 1) + "ReFix.log";

    FILE* f = nullptr;
    fopen_s(&f, logPath.c_str(), "a");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [steam_api64] ",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\\n");
    fclose(f);
}}

static std::string GetExeDir() {{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string dir(path);
    size_t pos = dir.find_last_of("\\\\/");
    return (pos != std::string::npos) ? dir.substr(0, pos + 1) : ".\\\\";
}}

static void LoadConfig() {{
    if (g_configLoaded) return;
    g_configLoaded = true;

    std::string ini = GetExeDir() + "ReFix.ini";
    GetPrivateProfileStringA("Steam", "MaskAppId", "480",
        g_maskAppId, sizeof(g_maskAppId), ini.c_str());
    g_maskAppIdNum = (uint32_t)atoi(g_maskAppId);
    if (g_maskAppIdNum == 0) g_maskAppIdNum = 480;
    GetPrivateProfileStringA("Steam", "Language", "english",
        g_language, sizeof(g_language), ini.c_str());
}}

static void ApplySteamEnv() {{
    LoadConfig();
    SetEnvironmentVariableA("SteamAppId", g_maskAppId);
    SetEnvironmentVariableA("SteamGameId", g_maskAppId);
    if (g_language[0]) {{
        SetEnvironmentVariableA("SteamLanguage", g_language);
    }}

    // Auto-configure Network: Firewall Rule + UPnP Mapping + Public IP Lookup
    ReFixNet::AddFirewallRule(7777, L"ReFix Game P2P");
    ReFixNet::MapUPnPPort(7777, L"ReFix P2P Server");
    g_hostLocalIP = ReFixNet::GetLocalIP();
    g_hostPublicIP = ReFixNet::GetPublicIP();

    SetEnvironmentVariableA("REFIX_LOCAL_IP", g_hostLocalIP.c_str());
    SetEnvironmentVariableA("REFIX_PUBLIC_IP", g_hostPublicIP.c_str());

    ReFixLog("Network P2P setup: LocalIP=%s, PublicIP=%s, UPnP/Firewall Port=7777",
             g_hostLocalIP.c_str(), g_hostPublicIP.c_str());
}}

// C++ Interception handlers
static void Intercepted_SteamAPI_RegisterCallback(void* pCallback, int iCallback) {{
    ReFixLog("SteamAPI_RegisterCallback: pCallback=%p, iCallback=%d", pCallback, iCallback);
    if (iCallback == 333) ReFixLog("  -> Intercepted GameLobbyJoinRequested_t (333)");
    else if (iCallback == 337) ReFixLog("  -> Intercepted GameRichPresenceJoinRequested_t (337)");

    if (g_pfn_RegisterCallback) g_pfn_RegisterCallback(pCallback, iCallback);
}}

static bool Intercepted_SetRichPresence(void* self, const char* pchKey, const char* pchValue) {{
    ReFixLog("SteamAPI_ISteamFriends_SetRichPresence: Key='%s', Value='%s'",
             pchKey ? pchKey : "", pchValue ? pchValue : "");

    if (g_pfn_SetRichPresence) {{
        return g_pfn_SetRichPresence(self, pchKey, pchValue);
    }}
    return true;
}}

static uint64_t Intercepted_CreateLobby(void* self, int eLobbyType, int cMaxMembers) {{
    ReFixLog("SteamAPI_ISteamMatchmaking_CreateLobby called (Type=%d, MaxMembers=%d)", eLobbyType, cMaxMembers);
    uint64_t hResult = 0;
    if (g_pfn_CreateLobby) {{
        hResult = g_pfn_CreateLobby(self, eLobbyType, cMaxMembers);
    }}
    ReFixLog("  CreateLobby APICall handle: %llu (attaching game_filter=mechachameleon)", hResult);

    // Register Direct P2P Connect string in Steam Rich Presence for 1-click Steam Invitations
    if (!g_hostPublicIP.empty() && g_pfn_SetRichPresence && g_pfn_SteamFriends) {{
        void* friends = g_pfn_SteamFriends();
        if (friends) {{
            char connectString[128];
            sprintf_s(connectString, sizeof(connectString), "+connect %s:7777", g_hostPublicIP.c_str());
            g_pfn_SetRichPresence(friends, "connect", connectString);
            ReFixLog("  -> Registered Steam Rich Presence connect string: '%s'", connectString);
        }}
    }}

    return hResult;
}}

static bool Intercepted_SetLobbyData(void* self, uint64_t steamIDLobby, const char* pchKey, const char* pchValue) {{
    ReFixLog("SteamAPI_ISteamMatchmaking_SetLobbyData: Lobby=%llu, Key='%s', Value='%s'",
             steamIDLobby, pchKey ? pchKey : "", pchValue ? pchValue : "");
    if (g_pfn_SetLobbyData) {{
        return g_pfn_SetLobbyData(self, steamIDLobby, pchKey, pchValue);
    }}
    return false;
}}

static void Intercepted_RequestLobbyList(void* self) {{
    ReFixLog("SteamAPI_ISteamMatchmaking_RequestLobbyList: Injecting filter game_filter=mechachameleon");
    if (g_pfn_AddRequestLobbyListStringFilter) {{
        g_pfn_AddRequestLobbyListStringFilter(self, "game_filter", "mechachameleon", 0);
    }}
    if (g_pfn_RequestLobbyList) {{
        g_pfn_RequestLobbyList(self);
    }}
}}

static bool EnsureOriginal() {{
    if (g_hOriginalDll) return true;

    std::string path = GetExeDir() + "..\\\\..\\\\..\\\\Engine\\\\Binaries\\\\ThirdParty\\\\Steamworks\\\\Steamv157\\\\Win64\\\\steam_api64_valve.dll";
    g_hOriginalDll = LoadLibraryA(path.c_str());

    if (!g_hOriginalDll) {{
        g_hOriginalDll = LoadLibraryA("steam_api64_valve.dll");
    }}

    if (!g_hOriginalDll) {{
        MessageBoxA(NULL,
            "ReFix Error: Could not find 'steam_api64_valve.dll'.\\n\\n"
            "Please rename original Valve steam_api64.dll to steam_api64_valve.dll.",
            "ReFix - Steam Proxy", MB_ICONERROR | MB_OK);
        return false;
    }}

    g_pfn_Init     = (fn_SteamAPI_Init_t)GetProcAddress(g_hOriginalDll, "SteamAPI_Init");
    g_pfn_InitSafe = (fn_SteamAPI_Init_t)GetProcAddress(g_hOriginalDll, "SteamAPI_InitSafe");
    g_pfn_InitAnon = (fn_SteamAPI_Init_t)GetProcAddress(g_hOriginalDll, "SteamAPI_InitAnonymousUser");
    g_pfn_Restart  = (fn_SteamAPI_RestartAppIfNecessary_t)GetProcAddress(g_hOriginalDll, "SteamAPI_RestartAppIfNecessary");

    g_pfn_GSInit     = (fn_SteamInternal_GameServer_Init_t)GetProcAddress(g_hOriginalDll, "SteamInternal_GameServer_Init");
    g_pfn_GSInitSafe = (fn_SteamGameServer_InitSafe_t)GetProcAddress(g_hOriginalDll, "SteamGameServer_InitSafe");

    g_pfn_GetPersonaName = (fn_GetPersonaName_t)GetProcAddress(g_hOriginalDll, "SteamAPI_ISteamFriends_GetPersonaName");
    g_pfn_SteamFriends   = (fn_SteamFriends_v017_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamFriends_v017");

    g_pfn_GetSteamID = (fn_GetSteamID_t)GetProcAddress(g_hOriginalDll, "SteamAPI_ISteamUser_GetSteamID");
    g_pfn_SteamUser  = (fn_SteamUser_v021_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamUser_v021");

    g_pfn_RegisterCallback = (fn_SteamAPI_RegisterCallback_t)GetProcAddress(g_hOriginalDll, "SteamAPI_RegisterCallback");

    // Populate forwarding table for ASM trampolines
    int resolvedCount = 0;
    for (int i = 0; i < STEAM_FORWARD_COUNT; i++) {{
        g_steamProcs[i] = GetProcAddress(g_hOriginalDll, g_forwardNames[i]);
        if (g_steamProcs[i]) resolvedCount++;
    }}
    ReFixLog("steam_api64 forwarding table populated: %d/%d exports resolved", resolvedCount, STEAM_FORWARD_COUNT);

    // Assign custom C++ function pointers to g_steamProcs table for ASM trampolines
    g_pfn_RequestFavoritesServerList = (fn_RequestServerList4_t)g_steamProcs[418];
    g_pfn_RequestFriendsServerList   = (fn_RequestServerList4_t)g_steamProcs[419];
    g_pfn_RequestHistoryServerList   = (fn_RequestServerList4_t)g_steamProcs[420];
    g_pfn_RequestInternetServerList  = (fn_RequestServerList4_t)g_steamProcs[421];
    g_pfn_RequestLANServerList       = (fn_RequestLANServerList_t)g_steamProcs[422];
    g_pfn_RequestSpectatorServerList = (fn_RequestServerList4_t)g_steamProcs[423];
    g_pfn_AddFavoriteGame            = (fn_AddFavoriteGame_t)g_steamProcs[425];

    g_pfn_SetRichPresence                 = (fn_SteamAPI_ISteamFriends_SetRichPresence_t)g_steamProcs[217];
    g_pfn_AddRequestLobbyListStringFilter = (fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t)g_steamProcs[432];
    g_pfn_CreateLobby                     = (fn_SteamAPI_ISteamMatchmaking_CreateLobby_t)g_steamProcs[433];
    g_pfn_RequestLobbyList                = (fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t)g_steamProcs[453];
    g_pfn_SetLobbyData                    = (fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t)g_steamProcs[456];

    // Override g_steamProcs table slots so ASM trampolines jump directly to our C++ interceptors
    g_steamProcs[217] = (FARPROC)Intercepted_SetRichPresence;
    g_steamProcs[433] = (FARPROC)Intercepted_CreateLobby;
    g_steamProcs[453] = (FARPROC)Intercepted_RequestLobbyList;
    g_steamProcs[456] = (FARPROC)Intercepted_SetLobbyData;
    g_steamProcs[928] = (FARPROC)Intercepted_SteamAPI_RegisterCallback;

    ReFixLog("steam_api64.dll proxy initialized successfully");
    return true;
}}

static void CapturePersonaName() {{
    if (!g_pfn_GetPersonaName || !g_pfn_SteamFriends) return;
    void* friends = g_pfn_SteamFriends();
    if (!friends) return;
    const char* name = g_pfn_GetPersonaName(friends);
    if (name && name[0] != '\\0') {{
        SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", name);
        ReFixLog("CapturePersonaName: %s", name);
    }}
    if (g_pfn_GetSteamID && g_pfn_SteamUser) {{
        void* user = g_pfn_SteamUser();
        if (user) {{
            uint64_t steamId = g_pfn_GetSteamID(user);
            if (steamId != 0) {{
                char idStr[32];
                sprintf_s(idStr, sizeof(idStr), "%llu", steamId);
                SetEnvironmentVariableA("REFIX_STEAM_ID", idStr);
                ReFixLog("CaptureSteamID: %s", idStr);
            }}
        }}
    }}
}}

// Intercepted Exports implemented directly
extern "C" __declspec(dllexport) bool SteamAPI_Init() {{
    ApplySteamEnv();
    ReFixLog("SteamAPI_Init called");
    if (!EnsureOriginal() || !g_pfn_Init) {{
        ReFixLog("SteamAPI_Init: EnsureOriginal failed or no pfn_Init");
        return false;
    }}
    bool result = g_pfn_Init();
    ReFixLog("SteamAPI_Init: result=%d", result);
    if (result) CapturePersonaName();
    return result;
}}

extern "C" __declspec(dllexport) bool SteamAPI_InitSafe() {{
    ApplySteamEnv();
    if (!EnsureOriginal()) return false;
    bool result = false;
    if (g_pfn_InitSafe) result = g_pfn_InitSafe();
    else if (g_pfn_Init) result = g_pfn_Init();
    if (result) CapturePersonaName();
    return result;
}}

extern "C" __declspec(dllexport) bool SteamAPI_InitAnonymousUser() {{
    ApplySteamEnv();
    if (!EnsureOriginal()) return false;
    bool result = false;
    if (g_pfn_InitAnon) result = g_pfn_InitAnon();
    else if (g_pfn_Init) result = g_pfn_Init();
    if (result) CapturePersonaName();
    return result;
}}

extern "C" __declspec(dllexport) bool SteamAPI_RestartAppIfNecessary(unsigned int unOwnAppID) {{
    (void)unOwnAppID;
    return false;
}}

extern "C" __declspec(dllexport) bool SteamInternal_GameServer_Init(
    uint32_t unIP, uint16_t usGamePort, uint16_t usQueryPort,
    int eServerMode, const char* pchVersionString)
{{
    ApplySteamEnv();
    ReFixLog("SteamInternal_GameServer_Init: IP=%u, GamePort=%u, QueryPort=%u, Mode=%d, Ver=%s",
             unIP, usGamePort, usQueryPort, eServerMode, pchVersionString ? pchVersionString : "null");
    if (!EnsureOriginal() || !g_pfn_GSInit) return false;
    return g_pfn_GSInit(unIP, usGamePort, usQueryPort, eServerMode, pchVersionString);
}}

extern "C" __declspec(dllexport) bool SteamGameServer_InitSafe() {{
    ApplySteamEnv();
    ReFixLog("SteamGameServer_InitSafe called");
    if (!EnsureOriginal() || !g_pfn_GSInitSafe) return false;
    return g_pfn_GSInitSafe();
}}

extern "C" __declspec(dllexport) int SteamAPI_ISteamUser_UserHasLicenseForApp(
    void* self, uint64_t steamID, uint32_t appID) {{
    ReFixLog("UserHasLicenseForApp: steamID=%llu, appID=%u -> HasLicense(0)", steamID, appID);
    return 0;
}}

extern "C" __declspec(dllexport) int SteamAPI_ISteamGameServer_UserHasLicenseForApp(
    void* self, uint64_t steamID, uint32_t appID) {{
    ReFixLog("GS_UserHasLicenseForApp: steamID=%llu, appID=%u -> HasLicense(0)", steamID, appID);
    return 0;
}}

static void* Intercept_RequestServerList4(
    fn_RequestServerList4_t pfnOriginal,
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse)
{{
    LoadConfig();
    ReFixLog("RequestServerList: replacing AppID %u -> %u", iApp, g_maskAppIdNum);
    if (!pfnOriginal) return nullptr;
    return pfnOriginal(self, g_maskAppIdNum, ppchFilters, nFilters, pResponse);
}}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestInternetServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {{
    return Intercept_RequestServerList4(g_pfn_RequestInternetServerList, self, iApp, ppchFilters, nFilters, pResponse);
}}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {{
    return Intercept_RequestServerList4(g_pfn_RequestFavoritesServerList, self, iApp, ppchFilters, nFilters, pResponse);
}}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {{
    return Intercept_RequestServerList4(g_pfn_RequestFriendsServerList, self, iApp, ppchFilters, nFilters, pResponse);
}}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {{
    return Intercept_RequestServerList4(g_pfn_RequestHistoryServerList, self, iApp, ppchFilters, nFilters, pResponse);
}}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {{
    return Intercept_RequestServerList4(g_pfn_RequestSpectatorServerList, self, iApp, ppchFilters, nFilters, pResponse);
}}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestLANServerList(
    void* self, uint32_t iApp, void* pResponse) {{
    LoadConfig();
    if (!g_pfn_RequestLANServerList) return nullptr;
    return g_pfn_RequestLANServerList(self, g_maskAppIdNum, pResponse);
}}

extern "C" __declspec(dllexport) int SteamAPI_ISteamMatchmaking_AddFavoriteGame(
    void* self, uint32_t nAppID, uint32_t nIP, uint16_t nConnPort,
    uint16_t nQueryPort, uint32_t unFlags, uint32_t rTime32) {{
    LoadConfig();
    if (!g_pfn_AddFavoriteGame) return 0;
    return g_pfn_AddFavoriteGame(self, g_maskAppIdNum, nIP, nConnPort, nQueryPort, unFlags, rTime32);
}}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {{
    switch (ul_reason_for_call) {{
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            EnsureOriginal();
            break;
        case DLL_PROCESS_DETACH:
            ReFixNet::UnmapUPnPPort(7777);
            if (g_hOriginalDll && !lpReserved) {{
                FreeLibrary(g_hOriginalDll);
                g_hOriginalDll = nullptr;
            }}
            break;
    }}
    return TRUE;
}}
"""

with open(proxy_file, "w") as f:
    f.write(code)

print("Successfully updated src/steam_proxy.cpp with UPnP and Windows Firewall integration!")
