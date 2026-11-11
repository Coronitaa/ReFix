// =============================================================================
// ReFix - Forwarded winhttp.dll Proxy (100% Crash-Proof Dynamic MASM Forwarding)
// Used for generic DLL proxying in games protected by EAC / Anti-Cheat
// where version.dll or dxgi.dll are blocked or not imported.
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <stdint.h>
#include <intrin.h>
#pragma comment(lib, "advapi32.lib")

#define WINHTTP_FORWARD_COUNT 92

extern "C" {
    __declspec(dllexport) FARPROC g_winhttpProcs[WINHTTP_FORWARD_COUNT] = { 0 };
}

static const char* g_winhttpNames[WINHTTP_FORWARD_COUNT] = {
    "DllCanUnloadNow",
    "DllGetClassObject",
    "Private1",
    "SvchostPushServiceGlobals",
    "WinHttpAddRequestHeaders",
    "WinHttpAddRequestHeadersEx",
    "WinHttpAutoProxySvcMain",
    "WinHttpCheckPlatform",
    "WinHttpCloseHandle",
    "WinHttpConnect",
    "WinHttpConnectionDeletePolicyEntries",
    "WinHttpConnectionDeletePolicyEntriesByAppSid",
    "WinHttpConnectionDeleteProxyInfo",
    "WinHttpConnectionFreeNameList",
    "WinHttpConnectionFreeProxyInfo",
    "WinHttpConnectionFreeProxyList",
    "WinHttpConnectionGetNameList",
    "WinHttpConnectionGetProxyInfo",
    "WinHttpConnectionGetProxyList",
    "WinHttpConnectionOnlyConvert",
    "WinHttpConnectionOnlyReceive",
    "WinHttpConnectionOnlySend",
    "WinHttpConnectionSetPolicyEntries",
    "WinHttpConnectionSetProxyInfo",
    "WinHttpConnectionUpdateIfIndexTable",
    "WinHttpCrackUrl",
    "WinHttpCreateProxyList",
    "WinHttpCreateProxyManager",
    "WinHttpCreateProxyResolver",
    "WinHttpCreateProxyResult",
    "WinHttpCreateUiCompatibleProxyString",
    "WinHttpCreateUrl",
    "WinHttpDetectAutoProxyConfigUrl",
    "WinHttpFreeProxyResult",
    "WinHttpFreeProxyResultEx",
    "WinHttpFreeProxySettings",
    "WinHttpFreeProxySettingsEx",
    "WinHttpFreeQueryConnectionGroupResult",
    "WinHttpGetDefaultProxyConfiguration",
    "WinHttpGetIEProxyConfigForCurrentUser",
    "WinHttpGetProxyForUrl",
    "WinHttpGetProxyForUrlEx",
    "WinHttpGetProxyForUrlEx2",
    "WinHttpGetProxyForUrlHvsi",
    "WinHttpGetProxyResult",
    "WinHttpGetProxyResultEx",
    "WinHttpGetProxySettingsEx",
    "WinHttpGetProxySettingsResultEx",
    "WinHttpGetProxySettingsVersion",
    "WinHttpGetTunnelSocket",
    "WinHttpOpen",
    "WinHttpOpenRequest",
    "WinHttpPacJsWorkerMain",
    "WinHttpProbeConnectivity",
    "WinHttpProtocolCompleteUpgrade",
    "WinHttpProtocolReceive",
    "WinHttpProtocolSend",
    "WinHttpQueryAuthSchemes",
    "WinHttpQueryConnectionGroup",
    "WinHttpQueryDataAvailable",
    "WinHttpQueryHeaders",
    "WinHttpQueryHeadersEx",
    "WinHttpQueryOption",
    "WinHttpReadData",
    "WinHttpReadDataEx",
    "WinHttpReadProxySettings",
    "WinHttpReadProxySettingsHvsi",
    "WinHttpReceiveResponse",
    "WinHttpRefreshProxySettings",
    "WinHttpRegisterProxyChangeNotification",
    "WinHttpResetAutoProxy",
    "WinHttpResolverGetProxyForUrl",
    "WinHttpSaveProxyCredentials",
    "WinHttpSendRequest",
    "WinHttpSetCredentials",
    "WinHttpSetDefaultProxyConfiguration",
    "WinHttpSetOption",
    "WinHttpSetProxySettingsPerUser",
    "WinHttpSetSecureLegacyServersAppCompat",
    "WinHttpSetStatusCallback",
    "WinHttpSetTimeouts",
    "WinHttpTimeFromSystemTime",
    "WinHttpTimeToSystemTime",
    "WinHttpUnregisterProxyChangeNotification",
    "WinHttpWebSocketClose",
    "WinHttpWebSocketCompleteUpgrade",
    "WinHttpWebSocketQueryCloseStatus",
    "WinHttpWebSocketReceive",
    "WinHttpWebSocketSend",
    "WinHttpWebSocketShutdown",
    "WinHttpWriteData",
    "WinHttpWriteProxySettings"
};

static HMODULE g_hRealWinHttp = NULL;

static void InitWinHttpProxy() {
    char sysDir[MAX_PATH];
    GetSystemDirectoryA(sysDir, MAX_PATH);
    std::string realDllPath = std::string(sysDir) + "\\winhttp.dll";

    g_hRealWinHttp = LoadLibraryA(realDllPath.c_str());
    if (!g_hRealWinHttp) return;

    for (int i = 0; i < WINHTTP_FORWARD_COUNT; ++i) {
        g_winhttpProcs[i] = GetProcAddress(g_hRealWinHttp, g_winhttpNames[i]);
    }
}

static uint64_t GenerateMachineUniqueSteamId() {
    DWORD serialNum = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &serialNum, NULL, NULL, NULL, 0);

    char compName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    DWORD compNameLen = sizeof(compName);
    GetComputerNameA(compName, &compNameLen);

    uint64_t hash = 14695981039346656037ULL;
    for (char* p = compName; *p; ++p) {
        hash ^= (uint8_t)(*p);
        hash *= 1099511628211ULL;
    }
    hash ^= (uint64_t)serialNum;
    hash *= 1099511628211ULL;

    uint32_t accountId = (uint32_t)(hash & 0xFFFFFFFF);
    if (accountId == 0) accountId = 1;
    return 0x0110000100000000ULL | (uint64_t)accountId;
}

static void LoadAndApplyConfig() {
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string baseDir(exePath);
    size_t lastSlash = baseDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        baseDir = baseDir.substr(0, lastSlash);
    }

    std::string iniPath = baseDir + "\\ReFix.ini";
    DWORD attrib = GetFileAttributesA(iniPath.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        iniPath = baseDir + "\\..\\ReFix.ini";
        attrib = GetFileAttributesA(iniPath.c_str());
        if (attrib == INVALID_FILE_ATTRIBUTES) return;
    }

    char buf[1024];

    // --- Steam Config ---
    char mode[64] = "valve";
    GetPrivateProfileStringA("Steam", "Mode", "valve", mode, sizeof(mode), iniPath.c_str());

    char maskAppId[64] = "480";
    char realAppId[64] = "0";
    GetPrivateProfileStringA("Steam", "MaskAppId", "480", maskAppId, sizeof(maskAppId), iniPath.c_str());
    GetPrivateProfileStringA("Steam", "RealAppId", "0", realAppId, sizeof(realAppId), iniPath.c_str());

    if (_stricmp(mode, "valve") == 0 || _stricmp(mode, "steam") == 0) {
        SetEnvironmentVariableA("SteamAppId", maskAppId);
        SetEnvironmentVariableA("SteamGameId", maskAppId);
        SetEnvironmentVariableA("STEAM_COMPAT_APP_ID", maskAppId);
    } else {
        const char* targetAppId = (realAppId[0] != '\0' && strcmp(realAppId, "0") != 0) ? realAppId : maskAppId;
        SetEnvironmentVariableA("SteamAppId", targetAppId);
        SetEnvironmentVariableA("SteamGameId", targetAppId);
        SetEnvironmentVariableA("STEAM_COMPAT_APP_ID", targetAppId);
    }

    if (realAppId[0] != '\0') SetEnvironmentVariableA("REFIX_REAL_APPID", realAppId);

    char language[64] = "english";
    GetPrivateProfileStringA("Steam", "Language", "english", language, sizeof(language), iniPath.c_str());
    SetEnvironmentVariableA("SteamLanguage", language);

    // --- User Config ---
    char playerName[256] = "Player";
    GetPrivateProfileStringA("User", "Name", "", playerName, sizeof(playerName), iniPath.c_str());
    if (playerName[0] == '\0') {
        GetPrivateProfileStringA("User", "PersonaName", "Player", playerName, sizeof(playerName), iniPath.c_str());
    }
    SetEnvironmentVariableA("REFIX_USERNAME", playerName);
    SetEnvironmentVariableA("REFIX_USER_NAME", playerName);
    SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", playerName);
    SetEnvironmentVariableA("SteamPersonaName", playerName);

    char autoSteamIdStr[64] = "true";
    GetPrivateProfileStringA("User", "AutoGenerateSteamId", "true", autoSteamIdStr, sizeof(autoSteamIdStr), iniPath.c_str());
    bool autoSteamId = (_stricmp(autoSteamIdStr, "true") == 0 || strcmp(autoSteamIdStr, "1") == 0);

    char steamIdBuf[64] = "";
    GetPrivateProfileStringA("User", "SteamId", "", steamIdBuf, sizeof(steamIdBuf), iniPath.c_str());
    std::string finalSteamId = steamIdBuf;

    if (autoSteamId || finalSteamId.empty() || finalSteamId == "0") {
        uint64_t uniqueSid = GenerateMachineUniqueSteamId();
        char sidStr[64];
        sprintf_s(sidStr, sizeof(sidStr), "%llu", uniqueSid);
        finalSteamId = sidStr;
    }
    SetEnvironmentVariableA("REFIX_STEAMID", finalSteamId.c_str());
    SetEnvironmentVariableA("REFIX_STEAM_ID", finalSteamId.c_str());
    SetEnvironmentVariableA("SteamID", finalSteamId.c_str());

    // --- Extra DLLs to Load ---
    GetPrivateProfileStringA("Paths", "ExtraDLLs", "", buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') {
        std::stringstream ss(buf);
        std::string dllPath;
        while (std::getline(ss, dllPath, ',')) {
            size_t first = dllPath.find_first_not_of(" \t\r\n");
            size_t last = dllPath.find_last_of(" \t\r\n");
            if (first != std::string::npos && last != std::string::npos) {
                dllPath = dllPath.substr(first, (last - first + 1));
                if (!dllPath.empty()) {
                    LoadLibraryA(dllPath.c_str());
                }
            }
        }
    }
}

extern "C" __declspec(dllexport) int ReFix() {
    return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            InitWinHttpProxy();
            LoadAndApplyConfig();
            break;
    }
    return TRUE;
}
