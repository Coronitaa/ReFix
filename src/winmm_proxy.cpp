// =============================================================================
// ReFix - Forwarded winmm.dll Proxy (100% Crash-Proof Dynamic MASM Forwarding)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <stdint.h>
#pragma comment(lib, "advapi32.lib")

#define WINMM_FORWARD_COUNT 182

extern "C" {
    __declspec(dllexport) FARPROC g_winmmProcs[WINMM_FORWARD_COUNT] = { 0 };
}

static const char* g_winmmNames[WINMM_FORWARD_COUNT] = {
    "CloseDriver",
    "DefDriverProc",
    "DriverCallback",
    "DrvGetModuleHandle",
    "GetDriverModuleHandle",
    "OpenDriver",
    "PlaySound",
    "PlaySoundA",
    "PlaySoundW",
    "SendDriverMessage",
    "WOWAppExit",
    "auxGetDevCapsA",
    "auxGetDevCapsW",
    "auxGetNumDevs",
    "auxGetVolume",
    "auxOutMessage",
    "auxSetVolume",
    "joyConfigChanged",
    "joyGetDevCapsA",
    "joyGetDevCapsW",
    "joyGetNumDevs",
    "joyGetPos",
    "joyGetPosEx",
    "joyGetThreshold",
    "joyReleaseCapture",
    "joySetCapture",
    "joySetThreshold",
    "mciDriverNotify",
    "mciDriverYield",
    "mciExecute",
    "mciFreeCommandResource",
    "mciGetCreatorTask",
    "mciGetDeviceIDA",
    "mciGetDeviceIDFromElementIDA",
    "mciGetDeviceIDFromElementIDW",
    "mciGetDeviceIDW",
    "mciGetDriverData",
    "mciGetErrorStringA",
    "mciGetErrorStringW",
    "mciGetYieldProc",
    "mciLoadCommandResource",
    "mciSendCommandA",
    "mciSendCommandW",
    "mciSendStringA",
    "mciSendStringW",
    "mciSetDriverData",
    "mciSetYieldProc",
    "midiConnect",
    "midiDisconnect",
    "midiInAddBuffer",
    "midiInClose",
    "midiInGetDevCapsA",
    "midiInGetDevCapsW",
    "midiInGetErrorTextA",
    "midiInGetErrorTextW",
    "midiInGetID",
    "midiInGetNumDevs",
    "midiInMessage",
    "midiInOpen",
    "midiInPrepareHeader",
    "midiInReset",
    "midiInStart",
    "midiInStop",
    "midiInUnprepareHeader",
    "midiOutCacheDrumPatches",
    "midiOutCachePatches",
    "midiOutClose",
    "midiOutGetDevCapsA",
    "midiOutGetDevCapsW",
    "midiOutGetErrorTextA",
    "midiOutGetErrorTextW",
    "midiOutGetID",
    "midiOutGetNumDevs",
    "midiOutGetVolume",
    "midiOutLongMsg",
    "midiOutMessage",
    "midiOutOpen",
    "midiOutPrepareHeader",
    "midiOutReset",
    "midiOutSetVolume",
    "midiOutShortMsg",
    "midiOutUnprepareHeader",
    "midiStreamClose",
    "midiStreamOpen",
    "midiStreamOut",
    "midiStreamPause",
    "midiStreamPosition",
    "midiStreamProperty",
    "midiStreamRestart",
    "midiStreamStop",
    "mixerClose",
    "mixerGetControlDetailsA",
    "mixerGetControlDetailsW",
    "mixerGetDevCapsA",
    "mixerGetDevCapsW",
    "mixerGetID",
    "mixerGetLineControlsA",
    "mixerGetLineControlsW",
    "mixerGetLineInfoA",
    "mixerGetLineInfoW",
    "mixerGetNumDevs",
    "mixerMessage",
    "mixerOpen",
    "mixerSetControlDetails",
    "mmDrvInstall",
    "mmGetCurrentTask",
    "mmTaskBlock",
    "mmTaskCreate",
    "mmTaskSignal",
    "mmTaskYield",
    "mmioAdvance",
    "mmioAscend",
    "mmioClose",
    "mmioCreateChunk",
    "mmioDescend",
    "mmioFlush",
    "mmioGetInfo",
    "mmioInstallIOProcA",
    "mmioInstallIOProcW",
    "mmioOpenA",
    "mmioOpenW",
    "mmioRead",
    "mmioRenameA",
    "mmioRenameW",
    "mmioSeek",
    "mmioSendMessage",
    "mmioSetBuffer",
    "mmioSetInfo",
    "mmioStringToFOURCCA",
    "mmioStringToFOURCCW",
    "mmioWrite",
    "mmsystemGetVersion",
    "sndPlaySoundA",
    "sndPlaySoundW",
    "timeBeginPeriod",
    "timeEndPeriod",
    "timeGetDevCaps",
    "timeGetSystemTime",
    "timeGetTime",
    "timeKillEvent",
    "timeSetEvent",
    "waveInAddBuffer",
    "waveInClose",
    "waveInGetDevCapsA",
    "waveInGetDevCapsW",
    "waveInGetErrorTextA",
    "waveInGetErrorTextW",
    "waveInGetID",
    "waveInGetNumDevs",
    "waveInGetPosition",
    "waveInMessage",
    "waveInOpen",
    "waveInPrepareHeader",
    "waveInReset",
    "waveInStart",
    "waveInStop",
    "waveInUnprepareHeader",
    "waveOutBreakLoop",
    "waveOutClose",
    "waveOutGetDevCapsA",
    "waveOutGetDevCapsW",
    "waveOutGetErrorTextA",
    "waveOutGetErrorTextW",
    "waveOutGetID",
    "waveOutGetNumDevs",
    "waveOutGetPitch",
    "waveOutGetPlaybackRate",
    "waveOutGetPosition",
    "waveOutGetVolume",
    "waveOutMessage",
    "waveOutOpen",
    "waveOutPause",
    "waveOutPrepareHeader",
    "waveOutReset",
    "waveOutRestart",
    "waveOutSetPitch",
    "waveOutSetPlaybackRate",
    "waveOutSetVolume",
    "waveOutUnprepareHeader",
    "waveOutWrite"
};

static void InitWinmmProxy() {
    char sysDir[MAX_PATH];
    GetSystemDirectoryA(sysDir, MAX_PATH);
    std::string realWinmmPath = std::string(sysDir) + "\\winmm.dll";
    HMODULE hRealWinmm = LoadLibraryA(realWinmmPath.c_str());
    if (!hRealWinmm) {
        hRealWinmm = LoadLibraryA("C:\\Windows\\System32\\winmm.dll");
    }

    if (hRealWinmm) {
        for (int i = 0; i < WINMM_FORWARD_COUNT; i++) {
            if (g_winmmNames[i]) {
                g_winmmProcs[i] = GetProcAddress(hRealWinmm, g_winmmNames[i]);
            }
        }
    }
}

#pragma comment(lib, "advapi32.lib")

static bool InjectSteamOverlayWinmm(const char* appIdStr) {
#ifdef _WIN64
    const char* overlayDllName = "GameOverlayRenderer64.dll";
#else
    const char* overlayDllName = "GameOverlayRenderer.dll";
#endif

    if (GetModuleHandleA(overlayDllName) != NULL) return true;

    char steamPath[MAX_PATH] = { 0 };
    DWORD pathSize = sizeof(steamPath);
    HKEY hKey = NULL;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "SteamPath", NULL, NULL, (LPBYTE)steamPath, &pathSize);
        RegCloseKey(hKey);
    }
    if (steamPath[0] == '\0') {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            pathSize = sizeof(steamPath);
            RegQueryValueExA(hKey, "InstallPath", NULL, NULL, (LPBYTE)steamPath, &pathSize);
            RegCloseKey(hKey);
        }
    }
    if (steamPath[0] == '\0') return false;

    std::string sPath(steamPath);
    for (size_t i = 0; i < sPath.size(); i++) {
        if (sPath[i] == '/') sPath[i] = '\\';
    }

    if (!appIdStr || appIdStr[0] == '\0') appIdStr = "480";

    char pidStr[32];
    sprintf_s(pidStr, sizeof(pidStr), "%lu", GetCurrentProcessId());

    SetEnvironmentVariableA("SteamPath", sPath.c_str());
    SetEnvironmentVariableA("SteamAppId", appIdStr);
    SetEnvironmentVariableA("SteamGameId", appIdStr);
    SetEnvironmentVariableA("SteamOverlayGameId", appIdStr);
    SetEnvironmentVariableA("_SteamInjectionPIDs", pidStr);
    SetEnvironmentVariableA("ENABLE_STEAM_OVERLAY", "1");

    char eventName[128];
    sprintf_s(eventName, sizeof(eventName), "SteamOverlayRunning_%s", appIdStr);
    HANDLE hEvt = CreateEventA(NULL, TRUE, TRUE, eventName);
    (void)hEvt;

    std::string fullOverlayPath = sPath + "\\" + overlayDllName;
    HMODULE hOverlay = LoadLibraryA(fullOverlayPath.c_str());
    return (hOverlay != NULL);
}

static uint64_t GenerateMachineUniqueSteamId() {
    char rawString[512] = { 0 };
    HKEY hKey = NULL;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        DWORD dwSize = sizeof(rawString);
        RegQueryValueExA(hKey, "MachineGuid", NULL, NULL, (LPBYTE)rawString, &dwSize);
        RegCloseKey(hKey);
    }
    if (rawString[0] == '\0') {
        char compName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD compSize = sizeof(compName);
        GetComputerNameA(compName, &compSize);
        char userName[256] = { 0 };
        DWORD userSize = sizeof(userName);
        GetUserNameA(userName, &userSize);
        sprintf_s(rawString, sizeof(rawString), "%s_%s", compName, userName);
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    uint32_t accountId = 0;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            CryptHashData(hHash, (BYTE*)rawString, (DWORD)strlen(rawString), 0);
            BYTE md5Bytes[16] = { 0 };
            DWORD md5Len = sizeof(md5Bytes);
            if (CryptGetHashParam(hHash, HP_HASHVAL, md5Bytes, &md5Len, 0)) {
                memcpy(&accountId, md5Bytes, sizeof(uint32_t));
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    if (accountId == 0) {
        accountId = 123456789;
    }
    accountId = (accountId & 0x1FFFFFFF) + 100000000;
    uint64_t baseSteamId = 76561197960265728ULL;
    return baseSteamId + (uint64_t)accountId;
}

static void WriteTextFileIfChanged(const std::string& path, const std::string& content) {
    char existing[2048] = { 0 };
    FILE* f = NULL;
    if (fopen_s(&f, path.c_str(), "rb") == 0 && f) {
        size_t r = fread(existing, 1, sizeof(existing) - 1, f);
        fclose(f);
        if (r == content.size() && memcmp(existing, content.c_str(), r) == 0) {
            return;
        }
    }
    if (fopen_s(&f, path.c_str(), "wb") == 0 && f) {
        fwrite(content.c_str(), 1, content.size(), f);
        fclose(f);
    }
}

static void SyncGoldbergFolder(const std::string& folderPath,
                               const std::string& playerName,
                               const std::string& steamIdStr,
                               const std::string& listenPort,
                               const std::string& customBroadcasts,
                               const std::string& language,
                               const std::string& realAppId,
                               const std::string& dlcs) {
    std::string settingsDir = folderPath + "\\steam_settings";
    CreateDirectoryA(settingsDir.c_str(), NULL);

    WriteTextFileIfChanged(settingsDir + "\\force_account_name.txt", playerName);
    WriteTextFileIfChanged(settingsDir + "\\force_steamid.txt", steamIdStr);
    WriteTextFileIfChanged(settingsDir + "\\force_listen_port.txt", listenPort);
    WriteTextFileIfChanged(settingsDir + "\\force_language.txt", language);
    if (!customBroadcasts.empty()) {
        WriteTextFileIfChanged(settingsDir + "\\custom_broadcasts.txt", customBroadcasts);
    }
    // Delete offline.txt and disable_lan_only.txt so Goldberg runs in online/LAN mode
    DeleteFileA((settingsDir + "\\offline.txt").c_str());
    DeleteFileA((settingsDir + "\\disable_lan_only.txt").c_str());
    WriteTextFileIfChanged(settingsDir + "\\steam_appid.txt", realAppId);
    WriteTextFileIfChanged(folderPath + "\\steam_appid.txt", realAppId);

    std::string userIni = "[user::general]\naccount_name=" + playerName + "\naccount_steamid=" + steamIdStr + "\nlanguage=" + language + "\n";
    WriteTextFileIfChanged(settingsDir + "\\configs.user.ini", userIni);

    std::string mainIni = "[main::general]\nlisten_port=" + listenPort + "\n";
    WriteTextFileIfChanged(settingsDir + "\\configs.main.ini", mainIni);

    std::string appIni = "[app::general]\nappid=" + realAppId + "\n";
    if (!dlcs.empty()) {
        appIni += "\n[app::dlcs]\nunlock_all=1\n";
    }
    WriteTextFileIfChanged(settingsDir + "\\configs.app.ini", appIni);
}

static bool g_enableConsoleAllowed = false;
static bool g_enableServerBrowserAllowed = false;
static bool g_enableLogAllowed = false;

static void LoadAndApplyConfig() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string iniPath(exePath);
    std::string baseDir = "";
    size_t pos = iniPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        baseDir = iniPath.substr(0, pos);
        iniPath = baseDir + "\\ReFix.ini";
    }

    char buf[1024];

    // --- Mode Config ---
    char mode[64] = "valve";
    GetPrivateProfileStringA("Online", "Mode", "valve", mode, sizeof(mode), iniPath.c_str());

    // --- Debug & UI Config ---
    GetPrivateProfileStringA("Debug", "EnableConsole", "false", buf, sizeof(buf), iniPath.c_str());
    g_enableConsoleAllowed = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);

    GetPrivateProfileStringA("Debug", "EnableServerBrowser", "false", buf, sizeof(buf), iniPath.c_str());
    g_enableServerBrowserAllowed = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);

    GetPrivateProfileStringA("Debug", "EnableLog", "false", buf, sizeof(buf), iniPath.c_str());
    g_enableLogAllowed = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);

    // --- Steam Config ---
    char maskAppId[64] = "480";
    char realAppId[64] = "0";
    char overlayAppId[64] = "480";
    char enableOverlayStr[64] = "true";
    GetPrivateProfileStringA("Steam", "MaskAppId", "480", maskAppId, sizeof(maskAppId), iniPath.c_str());
    GetPrivateProfileStringA("Steam", "RealAppId", "0", realAppId, sizeof(realAppId), iniPath.c_str());
    GetPrivateProfileStringA("Overlay", "EnableOverlay", "true", enableOverlayStr, sizeof(enableOverlayStr), iniPath.c_str());
    GetPrivateProfileStringA("Overlay", "OverlayAppId", maskAppId, overlayAppId, sizeof(overlayAppId), iniPath.c_str());

    bool enableOverlay = (_stricmp(enableOverlayStr, "true") == 0 || strcmp(enableOverlayStr, "1") == 0);

    if (_stricmp(mode, "valve") == 0 || _stricmp(mode, "steam") == 0) {
        // In Valve Spacewar mode, mask with 480
        SetEnvironmentVariableA("SteamAppId", maskAppId);
        SetEnvironmentVariableA("SteamGameId", maskAppId);
        SetEnvironmentVariableA("SteamOverlayGameId", overlayAppId);
        SetEnvironmentVariableA("STEAM_COMPAT_APP_ID", maskAppId);
    } else {
        // In Goldberg LAN mode, use real AppId
        const char* targetAppId = (realAppId[0] != '\0' && strcmp(realAppId, "0") != 0) ? realAppId : maskAppId;
        SetEnvironmentVariableA("SteamAppId", targetAppId);
        SetEnvironmentVariableA("SteamGameId", targetAppId);
        SetEnvironmentVariableA("SteamOverlayGameId", targetAppId);
        SetEnvironmentVariableA("STEAM_COMPAT_APP_ID", targetAppId);
    }

    if (realAppId[0] != '\0') SetEnvironmentVariableA("REFIX_REAL_APPID", realAppId);

    char language[64] = "english";
    GetPrivateProfileStringA("Steam", "Language", "english", language, sizeof(language), iniPath.c_str());
    SetEnvironmentVariableA("SteamLanguage", language);

    char dlcs[1024] = "";
    GetPrivateProfileStringA("Steam", "DLCs", "", dlcs, sizeof(dlcs), iniPath.c_str());
    if (dlcs[0] != '\0') SetEnvironmentVariableA("REFIX_DLC_LIST", dlcs);

    // Perform early Steam Overlay injection in Valve mode
    if (enableOverlay && (_stricmp(mode, "valve") == 0 || _stricmp(mode, "steam") == 0)) {
        InjectSteamOverlayWinmm(overlayAppId);
    }

    // --- EOS Config ---
    GetPrivateProfileStringA("EOS", "DeviceIdAuth", "true", buf, sizeof(buf), iniPath.c_str());
    if (std::string(buf) == "true") {
        SetEnvironmentVariableA("EOS_LOGIN_CREDENTIAL_TYPE", "DeviceID");
        SetEnvironmentVariableA("EOS_DEVICE_ID_AUTH", "true");
    }

    GetPrivateProfileStringA("EOS", "EpicAppId", "", buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') SetEnvironmentVariableA("EOS_APP_ID", buf);

    // --- User Config (Auto-Generate Unique SteamID per PC) ---
    char playerName[256] = "Player";
    GetPrivateProfileStringA("User", "Name", "Player", playerName, sizeof(playerName), iniPath.c_str());
    SetEnvironmentVariableA("REFIX_USERNAME", playerName);

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

    // --- Network / Lobby Config ---
    char listenPort[32] = "47584";
    GetPrivateProfileStringA("Network", "ListenPort", "47584", listenPort, sizeof(listenPort), iniPath.c_str());

    char customBroadcasts[512] = "";
    GetPrivateProfileStringA("Network", "CustomBroadcasts", "", customBroadcasts, sizeof(customBroadcasts), iniPath.c_str());

    GetPrivateProfileStringA("Network", "GameFilter", "", buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') {
        SetEnvironmentVariableA("REFIX_GAME_FILTER", buf);
    } else {
        std::string dynAppId = (realAppId[0] != '\0' && strcmp(realAppId, "0") != 0) ? realAppId : maskAppId;
        std::string dynFilter = "refix_game_" + dynAppId;
        SetEnvironmentVariableA("REFIX_GAME_FILTER", dynFilter.c_str());
    }

    GetPrivateProfileStringA("Network", "PublicIP", "", buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') SetEnvironmentVariableA("REFIX_PUBLIC_IP", buf);

    GetPrivateProfileStringA("Network", "LocalIP", "", buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') SetEnvironmentVariableA("REFIX_LOCAL_IP", buf);

    // --- LIVE GOLDBERG SYNCHRONIZATION ---
    // If running in Goldberg LAN mode, dynamically synchronize ReFix.ini values to steam_settings/
    if (_stricmp(mode, "goldberg") == 0 || _stricmp(mode, "offline") == 0 || _stricmp(mode, "lan") == 0) {
        std::string finalAppId = (realAppId[0] != '\0' && strcmp(realAppId, "0") != 0) ? realAppId : maskAppId;

        // Sync root folder
        SyncGoldbergFolder(baseDir, playerName, finalSteamId, listenPort, customBroadcasts, language, finalAppId, dlcs);

        // Scan for Unity/Unreal plugin subdirectories
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA((baseDir + "\\*_Data").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    std::string dataPluginDir = baseDir + "\\" + fd.cFileName + "\\Plugins\\x86_64";
                    if (GetFileAttributesA(dataPluginDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        SyncGoldbergFolder(dataPluginDir, playerName, finalSteamId, listenPort, customBroadcasts, language, finalAppId, dlcs);
                    }
                    std::string dataPluginDir32 = baseDir + "\\" + fd.cFileName + "\\Plugins\\x86";
                    if (GetFileAttributesA(dataPluginDir32.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        SyncGoldbergFolder(dataPluginDir32, playerName, finalSteamId, listenPort, customBroadcasts, language, finalAppId, dlcs);
                    }
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }

        // Unreal Engine Steamworks locations
        std::string ueSteam = baseDir + "\\..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv157\\Win64";
        if (GetFileAttributesA(ueSteam.c_str()) != INVALID_FILE_ATTRIBUTES) {
            SyncGoldbergFolder(ueSteam, playerName, finalSteamId, listenPort, customBroadcasts, language, finalAppId, dlcs);
        }
        std::string ueSteam153 = baseDir + "\\..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv153\\Win64";
        if (GetFileAttributesA(ueSteam153.c_str()) != INVALID_FILE_ATTRIBUTES) {
            SyncGoldbergFolder(ueSteam153, playerName, finalSteamId, listenPort, customBroadcasts, language, finalAppId, dlcs);
        }
    }

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

#include "server_browser_gui.h"

// =============================================================================
// REFIX IN-GAME DEBUG CONSOLE & STANDALONE GUI SERVER BROWSER
// =============================================================================
static bool g_ConsoleAllocated = false;
static bool g_ConsoleVisible = false;
static DWORD g_LastToggleTime = 0;

static void ToggleConsoleWindow() {
    if (!g_ConsoleAllocated) {
        if (AllocConsole()) {
            FILE* fDummy;
            freopen_s(&fDummy, "CONOUT$", "w", stdout);
            freopen_s(&fDummy, "CONOUT$", "w", stderr);
            freopen_s(&fDummy, "CONIN$", "r", stdin);

            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) {
                DWORD dwMode = 0;
                GetConsoleMode(hOut, &dwMode);
                SetConsoleMode(hOut, dwMode | 0x0004);
            }

            SetConsoleTitleA("[ReFix Online Debug Console] - F1 / INSERT to Show/Hide");
            printf("====================================================================\n");
            printf("           ReFix Online Debug Console Initialized\n");
            printf("====================================================================\n");
            printf("[INFO] Process ID: %lu\n", GetCurrentProcessId());
            printf("[INFO] Press F2 or HOME to open Standalone Server Browser GUI\n");
            printf("[INFO] Press F1 or INSERT to Hide/Show this Console Window\n");
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
            bool keyF2Pressed     = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
            bool keyHomePressed   = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
            bool keyInsertPressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
            bool keyF1Pressed     = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;

            if (g_enableServerBrowserAllowed && (keyF2Pressed || keyHomePressed)) {
                g_LastToggleTime = now;
                ShowServerBrowserGUI();
            } else if (g_enableConsoleAllowed && (keyInsertPressed || keyF1Pressed)) {
                g_LastToggleTime = now;
                ToggleConsoleWindow();
            }
        }
    }
    return 0;
}

static void StartConsoleHotkeyMonitor() {
    CreateThread(NULL, 0, ConsoleHotkeyThread, NULL, 0, NULL);
}

extern "C" __declspec(dllexport) int ReFix() {
    return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            InitWinmmProxy();
            LoadAndApplyConfig();
            StartConsoleHotkeyMonitor();
            break;
    }
    return TRUE;
}

