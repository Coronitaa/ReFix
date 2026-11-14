#pragma once
#include <windows.h>

// True when running under Wine / Proton (Linux, Steam Play): ntdll exports wine_get_version there.
inline bool ReFixIsWine() {
    static const bool isWine = [] {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
    }();
    return isWine;
}

// Set by Steam when it starts a game through Proton. Not SteamAppId / SteamGameId: ReFix sets those itself.
inline bool ReFixHasProtonSteamEnv() {
    char buf[8];
    return GetEnvironmentVariableA("STEAM_COMPAT_CLIENT_INSTALL_PATH", buf, sizeof(buf)) > 0 ||
           GetEnvironmentVariableA("STEAM_COMPAT_DATA_PATH", buf, sizeof(buf)) > 0 ||
           GetEnvironmentVariableA("SteamEnv", buf, sizeof(buf)) > 0;
}
