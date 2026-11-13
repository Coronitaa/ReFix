#include "eos_registry_redirect.h"
#include "refix_log.h"
#include "minhook/MinHook.h"
#include <string>
#include <unordered_set>
#include <mutex>
#include <wincred.h>

namespace refix {

static std::unordered_set<HKEY> g_epicGamesKeys;
static std::mutex g_epicKeysMutex;

static std::wstring GetProfileIdW() {
    wchar_t buf[256] = { 0 };
    if (GetEnvironmentVariableW(L"REFIX_ACTIVE_PROFILE", buf, 256) > 0 && buf[0]) {
        return buf;
    }
    HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
    if (hSteam) {
        typedef uint64_t (*fn_GetSteamID)();
        fn_GetSteamID pfn = (fn_GetSteamID)GetProcAddress(hSteam, "ReFix_GetLocalSteamID");
        if (pfn) {
            uint64_t sid = pfn();
            if (sid != 0) return std::to_wstring(sid);
        }
    }
    char envBuf[128] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_USER_ID", envBuf, sizeof(envBuf)) > 0 && envBuf[0]) {
        uint64_t sid = _strtoui64(envBuf, nullptr, 10);
        if (sid != 0) return std::to_wstring(sid);
    }
    char slotBuf[16] = { 0 };
    int slot = 0;
    if (GetEnvironmentVariableA("REFIX_MULTI_SLOT", slotBuf, sizeof(slotBuf)) > 0) {
        slot = atoi(slotBuf);
        return L"Guest_Slot" + std::to_wstring(slot);
    }
    if (hSteam) {
        typedef int (*fn_GetSlot)();
        fn_GetSlot pfnSlot = (fn_GetSlot)GetProcAddress(hSteam, "ReFix_GetInstanceSlot");
        if (pfnSlot) {
            slot = pfnSlot();
            return L"Guest_Slot" + std::to_wstring(slot);
        }
    }
    return L"Guest_Slot0";
}

static std::string GetProfileIdA() {
    char buf[256] = { 0 };
    if (GetEnvironmentVariableA("REFIX_ACTIVE_PROFILE", buf, sizeof(buf)) > 0 && buf[0]) {
        return buf;
    }
    HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
    if (hSteam) {
        typedef uint64_t (*fn_GetSteamID)();
        fn_GetSteamID pfn = (fn_GetSteamID)GetProcAddress(hSteam, "ReFix_GetLocalSteamID");
        if (pfn) {
            uint64_t sid = pfn();
            if (sid != 0) return std::to_string(sid);
        }
    }
    char envBuf[128] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_USER_ID", envBuf, sizeof(envBuf)) > 0 && envBuf[0]) {
        uint64_t sid = _strtoui64(envBuf, nullptr, 10);
        if (sid != 0) return std::to_string(sid);
    }
    char slotBuf[16] = { 0 };
    int slot = 0;
    if (GetEnvironmentVariableA("REFIX_MULTI_SLOT", slotBuf, sizeof(slotBuf)) > 0) {
        slot = atoi(slotBuf);
        return "Guest_Slot" + std::to_string(slot);
    }
    if (hSteam) {
        typedef int (*fn_GetSlot)();
        fn_GetSlot pfnSlot = (fn_GetSlot)GetProcAddress(hSteam, "ReFix_GetInstanceSlot");
        if (pfnSlot) {
            slot = pfnSlot();
            return "Guest_Slot" + std::to_string(slot);
        }
    }
    return "Guest_Slot0";
}

static uint64_t GetCurrentSteamId64() {
    HMODULE hSteam = GetModuleHandleA("steam_api64.dll");
    if (hSteam) {
        typedef uint64_t (*fn_GetSteamID)();
        fn_GetSteamID pfn = (fn_GetSteamID)GetProcAddress(hSteam, "ReFix_GetLocalSteamID");
        if (pfn) {
            uint64_t sid = pfn();
            if (sid != 0) return sid;
        }
    }
    char buf[128] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_USER_ID", buf, sizeof(buf)) > 0 && buf[0]) {
        uint64_t sid = _strtoui64(buf, nullptr, 10);
        if (sid != 0) return sid;
    }
    if (GetEnvironmentVariableA("REFIX_ACTIVE_PROFILE", buf, sizeof(buf)) > 0 && buf[0]) {
        uint64_t sid = _strtoui64(buf, nullptr, 10);
        if (sid > 70000000000000000ULL) return sid;
    }
    int slot = 0;
    if (GetEnvironmentVariableA("REFIX_MULTI_SLOT", buf, sizeof(buf)) > 0) {
        slot = atoi(buf);
    } else if (hSteam) {
        typedef int (*fn_GetSlot)();
        fn_GetSlot pfnSlot = (fn_GetSlot)GetProcAddress(hSteam, "ReFix_GetInstanceSlot");
        if (pfnSlot) slot = pfnSlot();
    }
    return 0x0110000100000000ULL | (0x10000ULL + (uint64_t)(slot * 1000 + (GetCurrentProcessId() & 0x3FF)));
}

static bool RedirectRegistryPathW(HKEY hKey, const wchar_t* subKey, HKEY& outHKey, std::wstring& outRedirected) {
    if (!subKey) return false;
    std::wstring s(subKey);

    // Prevent double-redirection if path is already redirected
    if (s.find(L"ReFix\\Profiles") != std::wstring::npos || s.find(L"ReFix/Profiles") != std::wstring::npos) {
        return false;
    }

    size_t pos = s.find(L"Epic Games\\EOS");
    if (pos == std::wstring::npos) pos = s.find(L"Epic Games/EOS");
    if (pos != std::wstring::npos) {
        std::wstring prof = GetProfileIdW();
        std::wstring prefix = s.substr(0, pos);
        std::wstring suffix = s.substr(pos);
        outHKey = hKey;
        outRedirected = prefix + L"ReFix\\Profiles\\" + prof + L"\\" + suffix;
        return true;
    }

    bool isEpicParent = false;
    {
        std::lock_guard<std::mutex> lg(g_epicKeysMutex);
        if (g_epicGamesKeys.count(hKey) > 0) isEpicParent = true;
    }
    if (isEpicParent && (s == L"EOS" || s.rfind(L"EOS\\", 0) == 0 || s.rfind(L"EOS/", 0) == 0)) {
        std::wstring prof = GetProfileIdW();
        outHKey = HKEY_CURRENT_USER;
        outRedirected = L"Software\\ReFix\\Profiles\\" + prof + L"\\Epic Games\\" + s;
        return true;
    }

    return false;
}

static bool RedirectRegistryPathA(HKEY hKey, const char* subKey, HKEY& outHKey, std::string& outRedirected) {
    if (!subKey) return false;
    std::string s(subKey);

    // Prevent double-redirection if path is already redirected
    if (s.find("ReFix\\Profiles") != std::string::npos || s.find("ReFix/Profiles") != std::string::npos) {
        return false;
    }

    size_t pos = s.find("Epic Games\\EOS");
    if (pos == std::string::npos) pos = s.find("Epic Games/EOS");
    if (pos != std::string::npos) {
        std::string prof = GetProfileIdA();
        std::string prefix = s.substr(0, pos);
        std::string suffix = s.substr(pos);
        outHKey = hKey;
        outRedirected = prefix + "ReFix\\Profiles\\" + prof + "\\" + suffix;
        return true;
    }

    bool isEpicParent = false;
    {
        std::lock_guard<std::mutex> lg(g_epicKeysMutex);
        if (g_epicGamesKeys.count(hKey) > 0) isEpicParent = true;
    }
    if (isEpicParent && (s == "EOS" || s.rfind("EOS\\", 0) == 0 || s.rfind("EOS/", 0) == 0)) {
        std::string prof = GetProfileIdA();
        outHKey = HKEY_CURRENT_USER;
        outRedirected = "Software\\ReFix\\Profiles\\" + prof + "\\Epic Games\\" + s;
        return true;
    }

    return false;
}

static void TrackEpicKeyW(const wchar_t* subKey, HKEY openedKey) {
    if (!subKey || !openedKey) return;
    std::wstring s(subKey);
    if (s == L"Software\\Epic Games" || s == L"Epic Games" || s == L"Software/Epic Games" || s == L"Epic Games/") {
        std::lock_guard<std::mutex> lg(g_epicKeysMutex);
        g_epicGamesKeys.insert(openedKey);
    }
}

static void TrackEpicKeyA(const char* subKey, HKEY openedKey) {
    if (!subKey || !openedKey) return;
    std::string s(subKey);
    if (s == "Software\\Epic Games" || s == "Epic Games" || s == "Software/Epic Games" || s == "Epic Games/") {
        std::lock_guard<std::mutex> lg(g_epicKeysMutex);
        g_epicGamesKeys.insert(openedKey);
    }
}

// ------ RegCreateKeyExW -----------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegCreateKeyExW)(
    HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass,
    DWORD dwOptions, REGSAM samDesired, const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult, LPDWORD lpdwDisposition);
static fn_RegCreateKeyExW g_orig_RegCreateKeyExW = nullptr;

static LSTATUS APIENTRY Hook_RegCreateKeyExW(
    HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass,
    DWORD dwOptions, REGSAM samDesired, const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult, LPDWORD lpdwDisposition)
{
    HKEY effectiveKey = hKey;
    std::wstring redir;
    if (RedirectRegistryPathW(hKey, lpSubKey, effectiveKey, redir)) {
        RFLOG(Core, "[RegistryRedirect] RegCreateKeyExW: '%ls' -> '%ls'", lpSubKey, redir.c_str());
        LSTATUS st = g_orig_RegCreateKeyExW(effectiveKey, redir.c_str(), Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
        return st;
    }
    LSTATUS st = g_orig_RegCreateKeyExW(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
    if (st == ERROR_SUCCESS && phkResult) {
        TrackEpicKeyW(lpSubKey, *phkResult);
    }
    return st;
}

// ------ RegCreateKeyExA -----------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegCreateKeyExA)(
    HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass,
    DWORD dwOptions, REGSAM samDesired, const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult, LPDWORD lpdwDisposition);
static fn_RegCreateKeyExA g_orig_RegCreateKeyExA = nullptr;

static LSTATUS APIENTRY Hook_RegCreateKeyExA(
    HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass,
    DWORD dwOptions, REGSAM samDesired, const LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult, LPDWORD lpdwDisposition)
{
    HKEY effectiveKey = hKey;
    std::string redir;
    if (RedirectRegistryPathA(hKey, lpSubKey, effectiveKey, redir)) {
        RFLOG(Core, "[RegistryRedirect] RegCreateKeyExA: '%s' -> '%s'", lpSubKey, redir.c_str());
        LSTATUS st = g_orig_RegCreateKeyExA(effectiveKey, redir.c_str(), Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
        return st;
    }
    LSTATUS st = g_orig_RegCreateKeyExA(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
    if (st == ERROR_SUCCESS && phkResult) {
        TrackEpicKeyA(lpSubKey, *phkResult);
    }
    return st;
}

// ------ RegOpenKeyExW -------------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegOpenKeyExW)(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
static fn_RegOpenKeyExW g_orig_RegOpenKeyExW = nullptr;

static LSTATUS APIENTRY Hook_RegOpenKeyExW(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
    HKEY effectiveKey = hKey;
    std::wstring redir;
    if (RedirectRegistryPathW(hKey, lpSubKey, effectiveKey, redir)) {
        RFLOG(Core, "[RegistryRedirect] RegOpenKeyExW: '%ls' -> '%ls'", lpSubKey, redir.c_str());
        LSTATUS st = g_orig_RegOpenKeyExW(effectiveKey, redir.c_str(), ulOptions, samDesired, phkResult);
        return st;
    }
    LSTATUS st = g_orig_RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
    if (st == ERROR_SUCCESS && phkResult) {
        TrackEpicKeyW(lpSubKey, *phkResult);
    }
    return st;
}

// ------ RegOpenKeyExA -------------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegOpenKeyExA)(
    HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
static fn_RegOpenKeyExA g_orig_RegOpenKeyExA = nullptr;

static LSTATUS APIENTRY Hook_RegOpenKeyExA(
    HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
    HKEY effectiveKey = hKey;
    std::string redir;
    if (RedirectRegistryPathA(hKey, lpSubKey, effectiveKey, redir)) {
        RFLOG(Core, "[RegistryRedirect] RegOpenKeyExA: '%s' -> '%s'", lpSubKey, redir.c_str());
        LSTATUS st = g_orig_RegOpenKeyExA(effectiveKey, redir.c_str(), ulOptions, samDesired, phkResult);
        return st;
    }
    LSTATUS st = g_orig_RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
    if (st == ERROR_SUCCESS && phkResult) {
        TrackEpicKeyA(lpSubKey, *phkResult);
    }
    return st;
}

// ------ RegDeleteKeyW -------------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegDeleteKeyW)(HKEY hKey, LPCWSTR lpSubKey);
static fn_RegDeleteKeyW g_orig_RegDeleteKeyW = nullptr;

static LSTATUS APIENTRY Hook_RegDeleteKeyW(HKEY hKey, LPCWSTR lpSubKey) {
    HKEY effectiveKey = hKey;
    std::wstring redir;
    if (RedirectRegistryPathW(hKey, lpSubKey, effectiveKey, redir)) {
        return g_orig_RegDeleteKeyW(effectiveKey, redir.c_str());
    }
    return g_orig_RegDeleteKeyW(hKey, lpSubKey);
}

// ------ RegDeleteKeyA -------------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegDeleteKeyA)(HKEY hKey, LPCSTR lpSubKey);
static fn_RegDeleteKeyA g_orig_RegDeleteKeyA = nullptr;

static LSTATUS APIENTRY Hook_RegDeleteKeyA(HKEY hKey, LPCSTR lpSubKey) {
    HKEY effectiveKey = hKey;
    std::string redir;
    if (RedirectRegistryPathA(hKey, lpSubKey, effectiveKey, redir)) {
        return g_orig_RegDeleteKeyA(effectiveKey, redir.c_str());
    }
    return g_orig_RegDeleteKeyA(hKey, lpSubKey);
}

// ------ RegDeleteKeyExW -----------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegDeleteKeyExW)(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved);
static fn_RegDeleteKeyExW g_orig_RegDeleteKeyExW = nullptr;

static LSTATUS APIENTRY Hook_RegDeleteKeyExW(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved) {
    HKEY effectiveKey = hKey;
    std::wstring redir;
    if (RedirectRegistryPathW(hKey, lpSubKey, effectiveKey, redir)) {
        return g_orig_RegDeleteKeyExW(effectiveKey, redir.c_str(), samDesired, Reserved);
    }
    return g_orig_RegDeleteKeyExW(hKey, lpSubKey, samDesired, Reserved);
}

// ------ RegDeleteKeyExA -----------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegDeleteKeyExA)(HKEY hKey, LPCSTR lpSubKey, REGSAM samDesired, DWORD Reserved);
static fn_RegDeleteKeyExA g_orig_RegDeleteKeyExA = nullptr;

static LSTATUS APIENTRY Hook_RegDeleteKeyExA(HKEY hKey, LPCSTR lpSubKey, REGSAM samDesired, DWORD Reserved) {
    HKEY effectiveKey = hKey;
    std::string redir;
    if (RedirectRegistryPathA(hKey, lpSubKey, effectiveKey, redir)) {
        return g_orig_RegDeleteKeyExA(effectiveKey, redir.c_str(), samDesired, Reserved);
    }
    return g_orig_RegDeleteKeyExA(hKey, lpSubKey, samDesired, Reserved);
}

// ------ RegCloseKey ---------------------------------------------------------
typedef LSTATUS(APIENTRY* fn_RegCloseKey)(HKEY hKey);
static fn_RegCloseKey g_orig_RegCloseKey = nullptr;

static LSTATUS APIENTRY Hook_RegCloseKey(HKEY hKey) {
    {
        std::lock_guard<std::mutex> lg(g_epicKeysMutex);
        g_epicGamesKeys.erase(hKey);
    }
    return g_orig_RegCloseKey(hKey);
}

// ------ Windows Credential Manager Hooks (EOS DeviceId isolation) -----------
typedef BOOL(WINAPI* fn_CredReadW)(LPCWSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALW* Credential);
static fn_CredReadW g_orig_CredReadW = nullptr;

typedef BOOL(WINAPI* fn_CredWriteW)(PCREDENTIALW Credential, DWORD Flags);
static fn_CredWriteW g_orig_CredWriteW = nullptr;

typedef BOOL(WINAPI* fn_CredDeleteW)(LPCWSTR TargetName, DWORD Type, DWORD Flags);
static fn_CredDeleteW g_orig_CredDeleteW = nullptr;

static bool RedirectCredTargetW(LPCWSTR targetName, std::wstring& out) {
    if (!targetName) return false;
    std::wstring s(targetName);
    std::wstring lower = s;
    for (auto& c : lower) c = towlower(c);
    if (lower.find(L"eos") == std::wstring::npos && lower.find(L"epic") == std::wstring::npos) {
        return false;
    }
    std::wstring suffix = L"_" + std::to_wstring(GetCurrentSteamId64());
    if (s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix) {
        return false;
    }
    out = s + suffix;
    return true;
}

static BOOL WINAPI Hook_CredReadW(LPCWSTR TargetName, DWORD Type, DWORD Flags, PCREDENTIALW* Credential) {
    std::wstring redir;
    if (RedirectCredTargetW(TargetName, redir)) {
        return g_orig_CredReadW(redir.c_str(), Type, Flags, Credential);
    }
    return g_orig_CredReadW(TargetName, Type, Flags, Credential);
}

static BOOL WINAPI Hook_CredWriteW(PCREDENTIALW Credential, DWORD Flags) {
    if (!Credential) return g_orig_CredWriteW(Credential, Flags);
    std::wstring redir;
    if (RedirectCredTargetW(Credential->TargetName, redir)) {
        CREDENTIALW copy = *Credential;
        copy.TargetName = const_cast<LPWSTR>(redir.c_str());
        return g_orig_CredWriteW(&copy, Flags);
    }
    return g_orig_CredWriteW(Credential, Flags);
}

static BOOL WINAPI Hook_CredDeleteW(LPCWSTR TargetName, DWORD Type, DWORD Flags) {
    std::wstring redir;
    if (RedirectCredTargetW(TargetName, redir)) {
        return g_orig_CredDeleteW(redir.c_str(), Type, Flags);
    }
    return g_orig_CredDeleteW(TargetName, Type, Flags);
}

void InstallRegistryRedirectHooks() {
    static bool s_installed = false;
    if (s_installed) return;
    s_installed = true;

    MH_Initialize();

    HMODULE hAdvapi = GetModuleHandleW(L"advapi32.dll");
    if (!hAdvapi) hAdvapi = LoadLibraryW(L"advapi32.dll");
    if (!hAdvapi) {
        RFLOG(Core, "[RegistryRedirect] advapi32.dll not found");
        return;
    }

    auto hook = [](HMODULE mod, const char* name, void* hookFn, void** origFn) {
        void* target = (void*)GetProcAddress(mod, name);
        if (target) {
            MH_CreateHook(target, hookFn, origFn);
            MH_EnableHook(target);
        }
    };

    hook(hAdvapi, "RegCreateKeyExW", (void*)&Hook_RegCreateKeyExW, (void**)&g_orig_RegCreateKeyExW);
    hook(hAdvapi, "RegCreateKeyExA", (void*)&Hook_RegCreateKeyExA, (void**)&g_orig_RegCreateKeyExA);
    hook(hAdvapi, "RegOpenKeyExW",   (void*)&Hook_RegOpenKeyExW,   (void**)&g_orig_RegOpenKeyExW);
    hook(hAdvapi, "RegOpenKeyExA",   (void*)&Hook_RegOpenKeyExA,   (void**)&g_orig_RegOpenKeyExA);
    hook(hAdvapi, "RegDeleteKeyW",   (void*)&Hook_RegDeleteKeyW,   (void**)&g_orig_RegDeleteKeyW);
    hook(hAdvapi, "RegDeleteKeyA",   (void*)&Hook_RegDeleteKeyA,   (void**)&g_orig_RegDeleteKeyA);
    hook(hAdvapi, "RegDeleteKeyExW", (void*)&Hook_RegDeleteKeyExW, (void**)&g_orig_RegDeleteKeyExW);
    hook(hAdvapi, "RegDeleteKeyExA", (void*)&Hook_RegDeleteKeyExA, (void**)&g_orig_RegDeleteKeyExA);
    hook(hAdvapi, "RegCloseKey",     (void*)&Hook_RegCloseKey,     (void**)&g_orig_RegCloseKey);
    hook(hAdvapi, "CredReadW",       (void*)&Hook_CredReadW,       (void**)&g_orig_CredReadW);
    hook(hAdvapi, "CredWriteW",      (void*)&Hook_CredWriteW,      (void**)&g_orig_CredWriteW);
    hook(hAdvapi, "CredDeleteW",     (void*)&Hook_CredDeleteW,     (void**)&g_orig_CredDeleteW);

    RFLOG(Core, "[RegistryRedirect] EOS DeviceId registry and Credential Manager redirection hooks installed (Profile: %s)", GetProfileIdA().c_str());
}

} // namespace refix
