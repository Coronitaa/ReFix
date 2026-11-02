// Identity probe for tests/test_identity_mode.ps1.
// Loads the ReFix LAN proxy that sits next to this exe (so ReFix.ini is resolved from the probe's own
// folder, exactly like a deployed game), initializes Steam and prints "STEAMID=<id>" and "NAME=<name>".
#include <windows.h>
#include <cstdint>
#include <cstdio>

typedef bool (*fn_SteamAPI_Init)();
typedef void (*fn_SteamAPI_Shutdown)();
typedef void* (*fn_SteamUser)();
typedef uint64_t (*fn_GetSteamID)(void* self);
typedef void* (*fn_SteamFriends)();
typedef const char* (*fn_GetPersonaName)(void* self);

int main() {
    // The probe must see only what ReFix.ini says: drop identity inherited from the parent shell.
    const char* inherited[] = { "REFIX_STEAM_ID", "REFIX_STEAMID", "SteamID", "SteamId",
                                "REFIX_STEAM_PERSONA_NAME", "REFIX_USER_NAME", "REFIX_USERNAME", "SteamPersonaName" };
    for (const char* name : inherited) SetEnvironmentVariableA(name, nullptr);

    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char* slash = strrchr(exePath, '\\');
    if (slash) *(slash + 1) = '\0';
    char dllPath[MAX_PATH] = { 0 };
    snprintf(dllPath, sizeof(dllPath), "%ssteam_api64.dll", exePath);

    HMODULE h = LoadLibraryA(dllPath);
    if (!h) { printf("ERROR=LoadLibrary %lu\n", GetLastError()); return 2; }

    auto init = (fn_SteamAPI_Init)GetProcAddress(h, "SteamAPI_Init");
    auto shutdown = (fn_SteamAPI_Shutdown)GetProcAddress(h, "SteamAPI_Shutdown");
    auto user = (fn_SteamUser)GetProcAddress(h, "SteamAPI_SteamUser_v023");
    auto getId = (fn_GetSteamID)GetProcAddress(h, "SteamAPI_ISteamUser_GetSteamID");
    auto friends = (fn_SteamFriends)GetProcAddress(h, "SteamAPI_SteamFriends_v017");
    auto getName = (fn_GetPersonaName)GetProcAddress(h, "SteamAPI_ISteamFriends_GetPersonaName");
    if (!init || !shutdown || !user || !getId || !friends || !getName) { printf("ERROR=missing exports\n"); return 3; }

    if (!init()) { printf("ERROR=SteamAPI_Init failed\n"); return 4; }
    printf("STEAMID=%llu\n", (unsigned long long)getId(user()));
    printf("NAME=%s\n", getName(friends()));
    fflush(stdout);
    shutdown();
    return 0;
}
