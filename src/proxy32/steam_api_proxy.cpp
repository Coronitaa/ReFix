#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "exports_pragma.h"

// Enum ESteamAPIInitResult
enum ESteamAPIInitResult {
    k_ESteamAPIInitResult_OK = 0,
    k_ESteamAPIInitResult_FailedGeneric = 1,
    k_ESteamAPIInitResult_NoSteamClient = 2,
    k_ESteamAPIInitResult_VersionMismatch = 3,
};

static HMODULE g_hOrig = NULL;

// Dummy VTable for unsupported interfaces
static void* __cdecl DummyFunction() { return nullptr; }
static void* g_dummyVTable[256] = { 0 };
static void* g_pDummyInstance = nullptr;

static void InitDummyInstance() {
    if (g_pDummyInstance) return;
    for (int i = 0; i < 256; i++) {
        g_dummyVTable[i] = (void*)&DummyFunction;
    }
    static void* s_dummyObj = (void*)&g_dummyVTable;
    g_pDummyInstance = (void*)&s_dummyObj;
}

static void LogMsg(const char* fmt, ...) {
    char buf[1024];
    va_list va;
    va_start(va, fmt);
    vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, va);
    va_end(va);
    OutputDebugStringA(buf);

    static char s_logPath[MAX_PATH] = { 0 };
    if (!s_logPath[0]) {
        GetModuleFileNameA(NULL, s_logPath, MAX_PATH);
        char* slash = strrchr(s_logPath, '\\');
        if (!slash) slash = strrchr(s_logPath, '/');
        if (slash) *(slash + 1) = '\0';
        strcat_s(s_logPath, sizeof(s_logPath), "ReFix_x86.log");
    }

    FILE* fp = NULL;
    fopen_s(&fp, s_logPath, "a");
    if (fp) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(fp, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
        fclose(fp);
    }
}

static void EnsureOrigLoaded() {
    if (g_hOrig) return;

    // 1. Try next to this DLL
    HMODULE hSelf = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&EnsureOrigLoaded, &hSelf) && hSelf) {
        char dllPath[MAX_PATH] = { 0 };
        GetModuleFileNameA(hSelf, dllPath, MAX_PATH);
        std::string dllDir(dllPath);
        size_t slash = dllDir.find_last_of("\\/");
        if (slash != std::string::npos) dllDir = dllDir.substr(0, slash + 1);

        std::string candidate = dllDir + "steam_api_o.dll";
        g_hOrig = LoadLibraryA(candidate.c_str());
        if (g_hOrig) {
            LogMsg("Loaded orig DLL next to proxy: %s", candidate.c_str());
            return;
        }
    }

    // 2. Try next to exe
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) exeDir = exeDir.substr(0, lastSlash + 1);

    std::string oPath = exeDir + "steam_api_o.dll";
    g_hOrig = LoadLibraryA(oPath.c_str());
    if (g_hOrig) {
        LogMsg("Loaded orig DLL next to exe: %s", oPath.c_str());
        return;
    }

    // 3. Fallback to standard DLL search
    g_hOrig = LoadLibraryA("steam_api_o.dll");
    if (g_hOrig) {
        LogMsg("Loaded orig DLL from search path: steam_api_o.dll");
    } else {
        LogMsg("ERROR: Failed to load steam_api_o.dll (LastError=%lu)", GetLastError());
    }
}

static void ApplyEnvironmentOverrides() {
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) exeDir = exeDir.substr(0, lastSlash + 1);

    // AppId
    FILE* fApp = NULL;
    std::string appFile = exeDir + "steam_appid.txt";
    fopen_s(&fApp, appFile.c_str(), "r");
    if (!fApp) {
        appFile = exeDir + "steam_settings\\steam_appid.txt";
        fopen_s(&fApp, appFile.c_str(), "r");
    }
    if (fApp) {
        char buf[64] = { 0 };
        if (fgets(buf, sizeof(buf), fApp)) {
            char* p = buf;
            while (*p && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) p++;
            char* end = p + strlen(p) - 1;
            while (end > p && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t')) { *end = '\0'; end--; }
            if (*p) {
                SetEnvironmentVariableA("SteamAppId", p);
                SetEnvironmentVariableA("SteamGameId", p);
                SetEnvironmentVariableA("STEAM_COMPAT_APP_ID", p);
                LogMsg("Applied SteamAppId from %s: %s", appFile.c_str(), p);
            }
        }
        fclose(fApp);
    }

    // Persona Name
    std::string nameFile = exeDir + "steam_settings\\force_account_name.txt";
    FILE* fName = NULL;
    fopen_s(&fName, nameFile.c_str(), "r");
    if (fName) {
        char buf[128] = { 0 };
        if (fgets(buf, sizeof(buf), fName)) {
            char* p = buf;
            while (*p && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) p++;
            char* end = p + strlen(p) - 1;
            while (end > p && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t')) { *end = '\0'; end--; }
            if (*p) {
                SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", p);
                SetEnvironmentVariableA("REFIX_USER_NAME", p);
                SetEnvironmentVariableA("REFIX_USERNAME", p);
                SetEnvironmentVariableA("SteamPersonaName", p);
                LogMsg("Applied PersonaName from %s: %s", nameFile.c_str(), p);
            }
        }
        fclose(fName);
    }

    // Steam ID
    std::string sidFile = exeDir + "steam_settings\\force_steamid.txt";
    FILE* fSid = NULL;
    fopen_s(&fSid, sidFile.c_str(), "r");
    if (fSid) {
        char buf[64] = { 0 };
        if (fgets(buf, sizeof(buf), fSid)) {
            char* p = buf;
            while (*p && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) p++;
            char* end = p + strlen(p) - 1;
            while (end > p && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t')) { *end = '\0'; end--; }
            if (*p) {
                SetEnvironmentVariableA("REFIX_STEAM_ID", p);
                SetEnvironmentVariableA("REFIX_STEAMID", p);
                SetEnvironmentVariableA("SteamID", p);
                SetEnvironmentVariableA("SteamId", p);
                LogMsg("Applied SteamID from %s: %s", sidFile.c_str(), p);
            }
        }
        fclose(fSid);
    }
}

// Helpers to get interfaces from orig
static void* GetOrigInterface(const char* name) {
    EnsureOrigLoaded();
    if (!g_hOrig) return nullptr;
    typedef void* (__cdecl *fn_Get_t)();
    auto pfn = (fn_Get_t)GetProcAddress(g_hOrig, name);
    return pfn ? pfn() : nullptr;
}

extern "C" {

__declspec(dllexport) bool __cdecl SteamAPI_RestartAppIfNecessary(uint32_t unOwnAppID) {
    LogMsg("SteamAPI_RestartAppIfNecessary called for AppID %u -> returning FALSE (bypass restart)", unOwnAppID);
    return false; // MUST NEVER return true, otherwise the game closes immediately!
}

__declspec(dllexport) bool __cdecl SteamAPI_IsSteamRunning() {
    return true;
}

__declspec(dllexport) bool __cdecl SteamAPI_Init() {
    LogMsg("SteamAPI_Init called");
    ApplyEnvironmentOverrides();
    EnsureOrigLoaded();

    if (g_hOrig) {
        typedef bool (__cdecl *fn_Init_t)();
        auto pfn = (fn_Init_t)GetProcAddress(g_hOrig, "SteamAPI_Init");
        if (pfn) {
            bool ok = pfn();
            LogMsg("Underlying SteamAPI_Init returned %d", (int)ok);
            return ok;
        }
        auto pfnSafe = (fn_Init_t)GetProcAddress(g_hOrig, "SteamAPI_InitSafe");
        if (pfnSafe) {
            bool ok = pfnSafe();
            LogMsg("Underlying SteamAPI_InitSafe returned %d", (int)ok);
            return ok;
        }
    }
    return true;
}

__declspec(dllexport) bool __cdecl SteamAPI_InitSafe() {
    return SteamAPI_Init();
}

__declspec(dllexport) bool __cdecl SteamAPI_InitFlat(char* pOutErrMsg) {
    LogMsg("SteamAPI_InitFlat called");
    ApplyEnvironmentOverrides();
    EnsureOrigLoaded();

    if (g_hOrig) {
        typedef bool (__cdecl *fn_InitFlat_t)(char*);
        auto pfn = (fn_InitFlat_t)GetProcAddress(g_hOrig, "SteamAPI_InitFlat");
        if (pfn) {
            bool ok = pfn(pOutErrMsg);
            LogMsg("Underlying SteamAPI_InitFlat returned %d", (int)ok);
            return ok;
        }
    }
    return SteamAPI_Init();
}

__declspec(dllexport) int __cdecl SteamInternal_SteamAPI_Init(
    const char* pszInternalCheckInterfaceVersions, char* pOutErrMsg)
{
    LogMsg("SteamInternal_SteamAPI_Init called (ver='%s')", pszInternalCheckInterfaceVersions ? pszInternalCheckInterfaceVersions : "null");
    ApplyEnvironmentOverrides();
    EnsureOrigLoaded();

    if (g_hOrig) {
        typedef int (__cdecl *fn_InternalInit_t)(const char*, char*);
        auto pfnInternal = (fn_InternalInit_t)GetProcAddress(g_hOrig, "SteamInternal_SteamAPI_Init");
        if (pfnInternal) {
            int res = pfnInternal(pszInternalCheckInterfaceVersions, pOutErrMsg);
            LogMsg("Underlying SteamInternal_SteamAPI_Init returned %d", res);
            return res;
        }
    }

    bool ok = SteamAPI_Init();
    if (ok) {
        LogMsg("SteamInternal_SteamAPI_Init: succeeded via SteamAPI_Init -> returning k_ESteamAPIInitResult_OK (0)");
        return k_ESteamAPIInitResult_OK; // 0 = OK (ESteamAPIInitResult)
    }

    if (pOutErrMsg) strcpy_s(pOutErrMsg, 1024, "SteamAPI_Init failed");
    return k_ESteamAPIInitResult_FailedGeneric;
}

__declspec(dllexport) int32_t __cdecl GetHSteamPipe() {
    EnsureOrigLoaded();
    typedef int32_t (__cdecl *fn_t)();
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_GetHSteamPipe");
    if (!pfn) pfn = (fn_t)GetProcAddress(g_hOrig, "GetHSteamPipe");
    if (pfn) {
        int32_t r = pfn();
        if (r != 0) {
            static bool s_logged = false;
            if (!s_logged) { s_logged = true; LogMsg("GetHSteamPipe -> %d", r); }
            return r;
        }
    }
    static bool s_loggedFallback = false;
    if (!s_loggedFallback) { s_loggedFallback = true; LogMsg("GetHSteamPipe -> fallback 1"); }
    return 1;
}

__declspec(dllexport) int32_t __cdecl GetHSteamUser() {
    EnsureOrigLoaded();
    typedef int32_t (__cdecl *fn_t)();
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_GetHSteamUser");
    if (!pfn) pfn = (fn_t)GetProcAddress(g_hOrig, "GetHSteamUser");
    if (pfn) {
        int32_t r = pfn();
        if (r != 0) {
            static bool s_logged = false;
            if (!s_logged) { s_logged = true; LogMsg("GetHSteamUser -> %d", r); }
            return r;
        }
    }
    static bool s_loggedFallback = false;
    if (!s_loggedFallback) { s_loggedFallback = true; LogMsg("GetHSteamUser -> fallback 1"); }
    return 1;
}

__declspec(dllexport) int32_t __cdecl SteamAPI_GetHSteamPipe() {
    return GetHSteamPipe();
}

__declspec(dllexport) int32_t __cdecl SteamAPI_GetHSteamUser() {
    return GetHSteamUser();
}

__declspec(dllexport) void* __cdecl SteamClient() {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_Get_t)();
    auto pfn = (fn_Get_t)GetProcAddress(g_hOrig, "SteamClient");
    if (pfn) {
        void* r = pfn();
        if (r) {
            LogMsg("SteamClient() -> %p via SteamClient", r);
            return r;
        }
    }
    auto pfn2 = (fn_Get_t)GetProcAddress(g_hOrig, "SteamAPI_SteamClient_v020");
    if (pfn2) {
        void* r = pfn2();
        if (r) {
            LogMsg("SteamClient() -> %p via SteamAPI_SteamClient_v020", r);
            return r;
        }
    }
    LogMsg("SteamClient() -> fallback nullptr");
    return nullptr;
}

__declspec(dllexport) void* __cdecl SteamInternal_CreateInterface(const char* pszVersion) {
    LogMsg("SteamInternal_CreateInterface('%s')", pszVersion ? pszVersion : "null");
    EnsureOrigLoaded();
    if (!pszVersion) return nullptr;

    if (strstr(pszVersion, "SteamClient") || strstr(pszVersion, "STEAMCLIENT")) {
        return SteamClient();
    }
    if (strstr(pszVersion, "SteamUtils") || strstr(pszVersion, "STEAMUTILS")) {
        void* p = GetOrigInterface("SteamAPI_SteamUtils_v010");
        if (!p) p = GetOrigInterface("SteamAPI_SteamUtils_v009");
        if (!p) p = GetOrigInterface("SteamUtils");
        if (p) return p;
    }
    if (strstr(pszVersion, "SteamFriends") || strstr(pszVersion, "STEAMFRIENDS")) {
        void* p = GetOrigInterface("SteamFriends");
        if (p) return p;
    }
    if (strstr(pszVersion, "SteamApps") || strstr(pszVersion, "STEAMAPPS")) {
        void* p = GetOrigInterface("SteamApps");
        if (p) return p;
    }
    if (strstr(pszVersion, "SteamUserStats") || strstr(pszVersion, "STEAMUSERSTATS")) {
        void* p = GetOrigInterface("SteamUserStats");
        if (p) return p;
    }
    if (strstr(pszVersion, "SteamUser") || strstr(pszVersion, "STEAMUSER")) {
        void* p = GetOrigInterface("SteamUser");
        if (p) return p;
    }
    if (strstr(pszVersion, "SteamMatchMaking") || strstr(pszVersion, "STEAMMATCHMAKING")) {
        void* p = GetOrigInterface("SteamMatchmaking");
        if (p) return p;
    }

    typedef void* (__cdecl *fn_Create_t)(const char*);
    auto pfnOrig = (fn_Create_t)GetProcAddress(g_hOrig, "SteamInternal_CreateInterface");
    if (pfnOrig) {
        void* res = pfnOrig(pszVersion);
        if (res) return res;
    }

    LogMsg("SteamInternal_CreateInterface('%s') fallback to nullptr", pszVersion);
    return nullptr;
}

__declspec(dllexport) void* __cdecl SteamInternal_FindOrCreateUserInterface(uint32_t hSteamUser, const char* pszVersion) {
    EnsureOrigLoaded();
    if (!pszVersion) return nullptr;

    typedef void* (__cdecl *fn_FindOrCreate_t)(uint32_t, const char*);
    auto pfnOrig = (fn_FindOrCreate_t)GetProcAddress(g_hOrig, "SteamInternal_FindOrCreateUserInterface");
    if (pfnOrig) {
        void* res = pfnOrig(hSteamUser, pszVersion);
        if (res) return res;
    }

    if (strstr(pszVersion, "SteamNetworkingUtils")) {
        void* p = GetOrigInterface("SteamAPI_SteamNetworkingUtils_v003");
        if (!p) p = GetOrigInterface("SteamNetworkingUtils");
        if (p) return p;
    }
    if (strstr(pszVersion, "SteamNetworkingSockets")) {
        void* p = GetOrigInterface("SteamAPI_SteamNetworkingSockets_v009");
        if (!p) p = GetOrigInterface("SteamNetworkingSockets");
        if (p) return p;
    }
    if (strstr(pszVersion, "SteamNetworkingMessages")) {
        void* p = GetOrigInterface("SteamAPI_SteamNetworkingMessages_v002");
        if (!p) p = GetOrigInterface("SteamAPI_SteamNetworkingMessages_SteamAPI_v002");
        if (p) return p;
    }

    LogMsg("SteamInternal_FindOrCreateUserInterface(%u, '%s') fallback to nullptr", hSteamUser, pszVersion);
    return nullptr;
}

__declspec(dllexport) void* __cdecl SteamInternal_FindOrCreateGameServerInterface(uint32_t hSteamUser, const char* pszVersion) {
    EnsureOrigLoaded();
    if (!pszVersion) return nullptr;
    typedef void* (__cdecl *fn_FindOrCreate_t)(uint32_t, const char*);
    auto pfnOrig = (fn_FindOrCreate_t)GetProcAddress(g_hOrig, "SteamInternal_FindOrCreateGameServerInterface");
    if (pfnOrig) {
        void* res = pfnOrig(hSteamUser, pszVersion);
        if (res) return res;
    }
    return nullptr;
}

// Flat getters called by CSteamAPIContext::Init
__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamUser(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    LogMsg("SteamAPI_ISteamClient_GetISteamUser(self=%p, user=%u, pipe=%u, ver='%s')", self, hUser, hPipe, ver ? ver : "null");
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamUser");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) {
            LogMsg("  -> orig returned %p", r);
            return r;
        }
        r = pfn(self, hUser, hPipe, "SteamUser021");
        if (r) {
            LogMsg("  -> orig with SteamUser021 returned %p", r);
            return r;
        }
    }
    void* p = GetOrigInterface("SteamUser");
    LogMsg("  -> fallback GetOrigInterface returned %p", p);
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamFriends(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    LogMsg("SteamAPI_ISteamClient_GetISteamFriends(ver='%s')", ver ? ver : "null");
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamFriends");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
        r = pfn(self, hUser, hPipe, "SteamFriends017");
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamFriends");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamUtils(void* self, uint32_t hPipe, const char* ver) {
    LogMsg("SteamAPI_ISteamClient_GetISteamUtils(ver='%s')", ver ? ver : "null");
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamUtils");
    if (pfn) {
        void* r = pfn(self, hPipe, ver);
        if (r) return r;
        r = pfn(self, hPipe, "SteamUtils010");
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamUtils");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamMatchmaking(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamMatchmaking");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamMatchmaking");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamMatchmakingServers(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamMatchmakingServers");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamMatchmakingServers");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamUserStats(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamUserStats");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamUserStats");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamApps(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamApps");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamApps");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamNetworking(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamNetworking");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamNetworking");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamRemoteStorage(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamRemoteStorage");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamRemoteStorage");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamScreenshots(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamScreenshots");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamScreenshots");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamGameSearch(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    return nullptr;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamHTTP(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamHTTP");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamHTTP");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamUGC(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    EnsureOrigLoaded();
    typedef void* (__cdecl *fn_t)(void*, uint32_t, uint32_t, const char*);
    auto pfn = (fn_t)GetProcAddress(g_hOrig, "SteamAPI_ISteamClient_GetISteamUGC");
    if (pfn) {
        void* r = pfn(self, hUser, hPipe, ver);
        if (r) return r;
    }
    void* p = GetOrigInterface("SteamUGC");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamMusic(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamMusic");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamMusicRemote(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamMusicRemote");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamHTMLSurface(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamHTMLSurface");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamInventory(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamInventory");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamVideo(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamVideo");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamParentalSettings(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamParentalSettings");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamInput(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamInput");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamParties(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamParties");
    return p;
}

__declspec(dllexport) void* __cdecl SteamAPI_ISteamClient_GetISteamRemotePlay(void* self, uint32_t hUser, uint32_t hPipe, const char* ver) {
    void* p = GetOrigInterface("SteamRemotePlay");
    return p;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        InitDummyInstance();
        EnsureOrigLoaded();
        LogMsg("ReFix x86 steam_api.dll attached to process");
    }
    return TRUE;
}
