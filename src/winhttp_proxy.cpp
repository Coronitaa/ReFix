// ReFix - winhttp.dll 64-bit forwarder proxy
#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <objbase.h>
#include <oleauto.h>
#include <UIAutomation.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <stdint.h>
#include <intrin.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

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

static void WINAPI DummyWinHttpStub() {
    SetLastError(ERROR_NOT_SUPPORTED);
}

static void InitWinHttpProxy() {
    for (int i = 0; i < WINHTTP_FORWARD_COUNT; ++i) {
        g_winhttpProcs[i] = reinterpret_cast<FARPROC>(&DummyWinHttpStub);
    }

    char sysDir[MAX_PATH];
    if (GetSystemDirectoryA(sysDir, MAX_PATH) == 0) return;
    std::string realDllPath = std::string(sysDir) + "\\winhttp.dll";

    g_hRealWinHttp = LoadLibraryA(realDllPath.c_str());
    if (!g_hRealWinHttp) return;

    for (int i = 0; i < WINHTTP_FORWARD_COUNT; ++i) {
        FARPROC proc = GetProcAddress(g_hRealWinHttp, g_winhttpNames[i]);
        if (proc) {
            g_winhttpProcs[i] = proc;
        }
    }
}

static uint64_t GenerateMachineUniqueSteamId() {
    DWORD serialNum = 0;
    char sysDrive[4] = "C:\\";
    char sysDir[MAX_PATH];
    if (GetSystemDirectoryA(sysDir, MAX_PATH) >= 3) {
        sysDrive[0] = sysDir[0];
        sysDrive[1] = ':';
        sysDrive[2] = '\\';
        sysDrive[3] = '\0';
    }
    GetVolumeInformationA(sysDrive, NULL, 0, &serialNum, NULL, NULL, NULL, 0);

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

static std::string DirOf(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(0, pos) : "";
}

static bool IsIniTrue(const char* val) {
    return _stricmp(val, "true") == 0 || strcmp(val, "1") == 0;
}

static bool ClaimBootstrapOnce() {
    char evtName[64];
    sprintf_s(evtName, sizeof(evtName), "Local\\ReFixSteamBoot_%lu", GetCurrentProcessId());
    HANDLE hEvt = CreateEventA(NULL, TRUE, FALSE, evtName);
    if (!hEvt) return true;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hEvt);
        return false;
    }
    return true;
}

static bool IsHelperOrToolProcess(const std::string& exePath) {
    char envBuf[16] = {};
    if (GetEnvironmentVariableA("REFIX_HELPER_PROCESS", envBuf, sizeof(envBuf)) > 0 && envBuf[0] == '1') {
        return true;
    }
    if (GetEnvironmentVariableA("REFIX_NO_BOOTSTRAP", envBuf, sizeof(envBuf)) > 0 && envBuf[0] == '1') {
        return true;
    }
    size_t slash = exePath.find_last_of("\\/");
    std::string name = (slash != std::string::npos) ? exePath.substr(slash + 1) : exePath;
    for (char& c : name) c = (char)::tolower((unsigned char)c);

    return name.find("refix_steamhost") != std::string::npos ||
           name == "refixfusionshared.exe" ||
           name == "refixmod.exe" ||
           name == "gamelaunchhelper.exe" ||
           name == "eosbootstrapper.exe" ||
           name == "unitycrashhandler64.exe" ||
           name == "unitycrashhandler.exe" ||
           name == "crashpad_handler.exe" ||
           name.find("crashhandler") != std::string::npos ||
           name.find("crashpad") != std::string::npos;
}

static bool IsGameLaunchHelper(const std::string& exePath) {
    size_t slash = exePath.find_last_of("\\/");
    std::string name = (slash != std::string::npos) ? exePath.substr(slash + 1) : exePath;
    return _stricmp(name.c_str(), "gamelaunchhelper.exe") == 0;
}

static bool IsWrapperLauncherProcess(const std::string& exePath) {
    return IsHelperOrToolProcess(exePath);
}

// Ends other processes running this very executable (same full path), never other games' copies.
static void TerminateOtherInstancesOfSelf(DWORD selfPid) {
    wchar_t selfPath[MAX_PATH] = {};
    if (!GetModuleFileNameW(NULL, selfPath, MAX_PATH)) return;
    const wchar_t* selfName = wcsrchr(selfPath, L'\\');
    selfName = selfName ? selfName + 1 : selfPath;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{ sizeof(pe) };
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (pe.th32ProcessID == selfPid || _wcsicmp(pe.szExeFile, selfName) != 0) continue;
        HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) continue;
        wchar_t path[MAX_PATH] = {};
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, path, &len) && _wcsicmp(path, selfPath) == 0) TerminateProcess(h, 0);
        CloseHandle(h);
    }
    CloseHandle(snap);
}

static bool WindowHasTextMatch(HWND hwnd, const wchar_t* needle) {
    wchar_t buf[512] = {};
    if (GetWindowTextW(hwnd, buf, 512) > 0 && wcsstr(buf, needle) != nullptr) {
        return true;
    }
    struct ChildSearch {
        const wchar_t* needle;
        bool matched;
    } cs{ needle, false };
    EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
        auto* s = reinterpret_cast<ChildSearch*>(lp);
        wchar_t cbuf[512] = {};
        if (GetWindowTextW(child, cbuf, 512) > 0 && wcsstr(cbuf, s->needle) != nullptr) {
            s->matched = true;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&cs));
    return cs.matched;
}

static bool GetProcessExeNameByPid(DWORD pid, wchar_t* outName, size_t maxChars) {
    outName[0] = L'\0';
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    wchar_t path[MAX_PATH] = {};
    DWORD len = MAX_PATH;
    const bool found = QueryFullProcessImageNameW(h, 0, path, &len) != FALSE;
    CloseHandle(h);
    if (found) {
        const wchar_t* name = wcsrchr(path, L'\\');
        wcsncpy_s(outName, maxChars, name ? name + 1 : path, _TRUNCATE);
    }
    return found;
}

static bool ContainsError0x800705b4(const wchar_t* text) {
    if (!text || !text[0]) return false;
    std::wstring lower;
    lower.reserve(256);
    for (size_t i = 0; text[i] && i < 1024; ++i) {
        lower.push_back(towlower(text[i]));
    }
    return lower.find(L"0x800705b4") != std::wstring::npos ||
           lower.find(L"800705b4") != std::wstring::npos ||
           lower.find(L"ya se est") != std::wstring::npos ||
           lower.find(L"already running") != std::wstring::npos ||
           lower.find(L"error al iniciar tu juego") != std::wstring::npos ||
           lower.find(L"something went wrong launching") != std::wstring::npos;
}

static bool UIAutomationHasMsStoreError(IUIAutomation* pAuto, HWND hwnd) {
    if (!pAuto || !hwnd) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(pAuto->ElementFromHandle(hwnd, &pRoot)) || !pRoot) return false;

    bool matched = false;
    BSTR rootName = nullptr;
    if (SUCCEEDED(pRoot->get_CurrentName(&rootName)) && rootName) {
        matched = ContainsError0x800705b4(rootName);
        SysFreeString(rootName);
    }

    if (!matched) {
        IUIAutomationCondition* pTrue = nullptr;
        if (SUCCEEDED(pAuto->CreateTrueCondition(&pTrue)) && pTrue) {
            IUIAutomationElementArray* pArr = nullptr;
            if (SUCCEEDED(pRoot->FindAll(TreeScope_Descendants, pTrue, &pArr)) && pArr) {
                int len = 0;
                pArr->get_Length(&len);
                if (len > 512) len = 512;
                for (int i = 0; i < len && !matched; ++i) {
                    IUIAutomationElement* pChild = nullptr;
                    if (SUCCEEDED(pArr->GetElement(i, &pChild)) && pChild) {
                        BSTR name = nullptr;
                        if (SUCCEEDED(pChild->get_CurrentName(&name)) && name) {
                            matched = ContainsError0x800705b4(name);
                            SysFreeString(name);
                        }
                        pChild->Release();
                    }
                }
                pArr->Release();
            }
            pTrue->Release();
        }
    }

    pRoot->Release();
    return matched;
}

static bool TerminateChildOrNamedProcesses(const wchar_t* targetName, DWORD parentPid) {
    bool killed = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{ sizeof(pe) };
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (pe.th32ProcessID == 0 || pe.th32ProcessID == parentPid) continue;
        if (_wcsicmp(pe.szExeFile, targetName) == 0) {
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
            if (h) {
                if (TerminateProcess(h, 0)) killed = true;
                CloseHandle(h);
            }
        }
    }
    CloseHandle(snap);
    return killed;
}

struct MsStoreWatchdogArgs {
    std::string logDir;
    bool isHelper;
};

static bool IsMsStoreGameProcess(const std::string& exePath, const std::string& baseDir) {
    std::string lowerExe = exePath;
    for (char& c : lowerExe) c = (char)::tolower((unsigned char)c);
    if (lowerExe.find("-wingdk-") != std::string::npos || lowerExe.find("\\windowsapps\\") != std::string::npos) {
        return true;
    }
    const char* rels[] = { "", "\\..", "\\..\\..", "\\..\\..\\.." };
    for (const char* rel : rels) {
        std::string dir = baseDir + rel;
        if (GetFileAttributesA((dir + "\\MicrosoftGame.config").c_str()) != INVALID_FILE_ATTRIBUTES ||
            GetFileAttributesA((dir + "\\gamelaunchhelper.exe").c_str()) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }
    return false;
}

static DWORD WINAPI MsStoreLaunchWatchdogThread(LPVOID param) {
    MsStoreWatchdogArgs* args = reinterpret_cast<MsStoreWatchdogArgs*>(param);
    std::string logPath = (args && !args->logDir.empty()) ? (args->logDir + "\\ReFix.log") : "";
    bool isHelper = args ? args->isHelper : true;
    delete args;

    const DWORD selfPid = GetCurrentProcessId();
    // A launcher of this same game left from an earlier attempt keeps the package "running" (0x800705b4).
    // Steam helpers are not touched: each one exits with its own game, and another game's helper is not ours.
    if (isHelper) TerminateOtherInstancesOfSelf(selfPid);

    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IUIAutomation* pAuto = nullptr;
    CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, reinterpret_cast<void**>(&pAuto));

    struct ScanCtx {
        IUIAutomation* pAuto;
        HWND foundHwnd;
        DWORD foundPid;
    };

    for (int i = 0; i < 240; ++i) {
        Sleep(250);
        ScanCtx ctx{ pAuto, nullptr, 0 };
        EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
            if (!IsWindowVisible(hwnd)) return TRUE;
            auto* c = reinterpret_cast<ScanCtx*>(lp);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            wchar_t procName[MAX_PATH] = {};
            GetProcessExeNameByPid(pid, procName, MAX_PATH);
            bool isGsUi = (_wcsicmp(procName, L"GamingServicesUI.exe") == 0 ||
                           _wcsicmp(procName, L"ApplicationFrameHost.exe") == 0 ||
                           _wcsicmp(procName, L"XboxPcApp.exe") == 0 ||
                           _wcsicmp(procName, L"XboxPcAppFT.exe") == 0 ||
                           _wcsicmp(procName, L"XboxAppServices.exe") == 0);

            if (WindowHasTextMatch(hwnd, L"0x800705b4") || WindowHasTextMatch(hwnd, L"800705b4") ||
                (isGsUi && UIAutomationHasMsStoreError(c->pAuto, hwnd))) {
                c->foundHwnd = hwnd;
                c->foundPid = pid;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

        if (!ctx.foundHwnd) continue;

        // Terminating GamingServicesUI.exe lets GameLaunchHelper.exe unblock and boot the game
        // without aborting the launch sequence.
        bool killedUi = TerminateChildOrNamedProcesses(L"GamingServicesUI.exe", selfPid);
        wchar_t ownerName[MAX_PATH] = {};
        if (ctx.foundPid && GetProcessExeNameByPid(ctx.foundPid, ownerName, MAX_PATH)) {
            if ((_wcsicmp(ownerName, L"GamingServicesUI.exe") == 0 ||
                 _wcsicmp(ownerName, L"XboxAppServices.exe") == 0) &&
                ctx.foundPid != selfPid) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, ctx.foundPid);
                if (h) {
                    TerminateProcess(h, 0);
                    CloseHandle(h);
                    killedUi = true;
                }
            }
        }
        PostMessageW(ctx.foundHwnd, WM_CLOSE, 0, 0);
        PostMessageW(ctx.foundHwnd, WM_SYSCOMMAND, SC_CLOSE, 0);

        if (!logPath.empty()) {
            FILE* f = fopen(logPath.c_str(), "a");
            if (f) {
                fprintf(f, "[MsStoreWatchdog] Cleared 0x800705b4 dialog (pid=%lu, killedUi=%d, owner=%ls)\n",
                        ctx.foundPid, killedUi ? 1 : 0, ownerName);
                fclose(f);
            }
        }
        Sleep(500);
    }

    if (pAuto) pAuto->Release();
    if (SUCCEEDED(hrCom)) CoUninitialize();
    return 0;
}

static std::string FindSteamApiInPlugins(const std::string& iniDir, const char* dllName, const char* archSubdir) {
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((iniDir + "\\*_Data").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return "";
    std::string found;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string cand = iniDir + "\\" + fd.cFileName + "\\Plugins\\" + archSubdir + "\\" + dllName;
            if (GetFileAttributesA(cand.c_str()) != INVALID_FILE_ATTRIBUTES) {
                found = cand;
                break;
            }
            std::string candRoot = iniDir + "\\" + fd.cFileName + "\\Plugins\\" + dllName;
            if (GetFileAttributesA(candRoot.c_str()) != INVALID_FILE_ATTRIBUTES) {
                found = candRoot;
                break;
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return found;
}

static void LoadExtraDLLs(const std::string& iniPath) {
    if (!ClaimBootstrapOnce()) return;

    char autoSteam[32] = "false";
    GetPrivateProfileStringA("Paths", "AutoLoadSteamApi", "false", autoSteam, sizeof(autoSteam), iniPath.c_str());
#ifdef _WIN64
    const char* steamDllName = "steam_api64.dll";
    const char* archSubdir = "x86_64";
#else
    const char* steamDllName = "steam_api.dll";
    const char* archSubdir = "x86";
#endif
    if (IsIniTrue(autoSteam) && GetModuleHandleA(steamDllName) == NULL) {
        std::string iniDir = DirOf(iniPath);
        std::string steamPath = iniDir.empty() ? steamDllName : (iniDir + "\\" + steamDllName);
        HMODULE hSteam = LoadLibraryA(steamPath.c_str());
        if (!hSteam && !iniDir.empty()) {
            std::string pluginPath = FindSteamApiInPlugins(iniDir, steamDllName, archSubdir);
            if (!pluginPath.empty()) {
                hSteam = LoadLibraryA(pluginPath.c_str());
            }
        }
        if (!hSteam) {
            hSteam = LoadLibraryA(steamDllName);
        }
        if (hSteam) {
            // Games without a native steam_api64.dll import never call SteamAPI_Init();
            // invoke it once so LanCore, SteamLink, PhotonP2P, and the overlay start.
            typedef bool (*fn_SteamAPI_Init)();
            fn_SteamAPI_Init pInit = (fn_SteamAPI_Init)GetProcAddress(hSteam, "SteamAPI_Init");
            if (pInit) pInit();
        }
    }

    char buf[1024];
    GetPrivateProfileStringA("Paths", "ExtraDLLs", "", buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') {
        std::stringstream ss(buf);
        std::string dllPath;
        while (std::getline(ss, dllPath, ',')) {
            size_t first = dllPath.find_first_not_of(" \t\r\n");
            size_t last = dllPath.find_last_not_of(" \t\r\n");
            if (first != std::string::npos && last != std::string::npos && last >= first) {
                dllPath = dllPath.substr(first, (last - first + 1));
                if (!dllPath.empty()) {
                    LoadLibraryA(dllPath.c_str());
                }
            }
        }
    }
}

static DWORD WINAPI ExtraDllsThread(LPVOID param) {
    char* pIni = reinterpret_cast<char*>(param);
    if (pIni) {
        LoadExtraDLLs(std::string(pIni));
        free(pIni);
    }
    return 0;
}

static void LaunchMsStoreWatchdog(const std::string& logDir, bool enableLog, bool isHelper) {
    auto* args = new MsStoreWatchdogArgs{ enableLog ? logDir : "", isHelper };
    HANDLE hWatch = CreateThread(NULL, 0, MsStoreLaunchWatchdogThread, args, 0, NULL);
    if (hWatch) {
        CloseHandle(hWatch);
    } else {
        delete args;
    }
}

static void LoadAndApplyConfig() {
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (IsHelperOrToolProcess(exePath)) {
        if (IsGameLaunchHelper(exePath)) {
            char logFlag[16] = "false";
            std::string helperDir = DirOf(exePath);
            std::string helperIni = helperDir + "\\ReFix.ini";
            GetPrivateProfileStringA("Debug", "EnableLog", "false", logFlag, sizeof(logFlag), helperIni.c_str());
            LaunchMsStoreWatchdog(helperDir, IsIniTrue(logFlag), true);
        }
        return;
    }
    std::string baseDir = DirOf(exePath);

    std::string candidates[] = {
        baseDir + "\\ReFix.ini",
        baseDir + "\\..\\ReFix.ini",
        baseDir + "\\..\\..\\ReFix.ini",
        baseDir + "\\..\\..\\..\\ReFix.ini"
    };
    std::string iniPath;
    bool foundIni = false;
    for (const auto& c : candidates) {
        if (GetFileAttributesA(c.c_str()) != INVALID_FILE_ATTRIBUTES) {
            iniPath = c;
            foundIni = true;
            break;
        }
    }
    if (!foundIni) return;

    if (IsMsStoreGameProcess(exePath, baseDir)) {
        char logFlag[16] = "false";
        GetPrivateProfileStringA("Debug", "EnableLog", "false", logFlag, sizeof(logFlag), iniPath.c_str());
        LaunchMsStoreWatchdog(DirOf(iniPath), IsIniTrue(logFlag), false);
    }

    char mode[64] = "";
    GetPrivateProfileStringA("Online", "Mode", "", mode, sizeof(mode), iniPath.c_str());
    if (mode[0] == '\0') {
        GetPrivateProfileStringA("Steam", "Mode", "valve", mode, sizeof(mode), iniPath.c_str());
    }

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

    // Respect [User] IdentityMode=auto and active ReFix Overlay so we do not mask profile identities
    // with machine-generated SteamIDs or default PersonaNames before steam_api initializes.
    char identityMode[32] = "auto";
    GetPrivateProfileStringA("User", "IdentityMode", "auto", identityMode, sizeof(identityMode), iniPath.c_str());
    char overlayEnabledBuf[32] = "true";
    GetPrivateProfileStringA("Overlay", "Enabled", "true", overlayEnabledBuf, sizeof(overlayEnabledBuf), iniPath.c_str());
    char envProfileId[64] = "";
    const bool hasProfileEnv = (GetEnvironmentVariableA("REFIX_PROFILE_ID", envProfileId, sizeof(envProfileId)) > 0 && envProfileId[0] != '\0');
    const bool fixedIdentity = (_stricmp(identityMode, "fixed") == 0);
    const bool overlayEnabled = (_stricmp(overlayEnabledBuf, "true") == 0 || strcmp(overlayEnabledBuf, "1") == 0);

    if (!hasProfileEnv && (fixedIdentity || !overlayEnabled)) {
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
    }

    char extraBuf[1024] = "";
    char autoSteamBuf[32] = "false";
    GetPrivateProfileStringA("Paths", "ExtraDLLs", "", extraBuf, sizeof(extraBuf), iniPath.c_str());
    GetPrivateProfileStringA("Paths", "AutoLoadSteamApi", "false", autoSteamBuf, sizeof(autoSteamBuf), iniPath.c_str());
    if (extraBuf[0] != '\0' || IsIniTrue(autoSteamBuf)) {
        char* pCopy = _strdup(iniPath.c_str());
        HANDLE hThread = CreateThread(NULL, 0, ExtraDllsThread, pCopy, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        } else {
            free(pCopy);
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
