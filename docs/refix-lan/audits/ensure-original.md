# EnsureOriginal & Initialization Paths Audit

## 1. EnsureOriginal() Analysis

Located in `src/steam_proxy.cpp`, `EnsureOriginal()` is the critical bootstrapper for the 64-bit ReFix proxy. Its responsibilities are:

*   **Discovery**: It generates a broad list of candidate paths and filenames for the real Steam API DLL (e.g., `steam_api64_valve.dll`, `steam_api64_goldberg.dll`, `steam_api64_o.dll`). It recursively checks typical locations, including the executable directory, Unity `*_Data/Plugins`, and Unreal Engine `ThirdParty/Steamworks` directories.
*   **Validation**: It uses `LoadLibraryA` and probes the loaded module using `GetProcAddress` for known exports (`SteamAPI_Init`, `SteamAPI_InitFlat`, `SteamAPI_Shutdown`). It uniquely checks for the presence of `g_steamProcs` to prevent the proxy from accidentally loading another copy of itself in an infinite loop.
*   **Resolution**: Maps function pointers for initialization routines (`g_pfn_Init`, `g_pfn_InitFlat`, `g_pfn_SteamAPIInit_Internal`, etc.) and internal interfaces.
*   **Flat Export Hooking**: It iterates through `STEAM_FORWARD_COUNT` to populate the `g_steamProcs` jump table (used by `steam_fwd.asm`), and explicitly overwrites specific pointers in the table to intercept flat API calls like `SteamAPI_ISteamFriends_SetRichPresence` and `SteamAPI_ISteamMatchmakingServers_RequestInternetServerList` with custom C++ logic.

## 2. Analysis of Initialization Paths

The initialization paths (`SteamAPI_Init`, `SteamAPI_InitSafe`, `SteamAPI_InitFlat`, `SteamAPI_InitAnonymousUser`, `SteamInternal_SteamAPI_Init`) currently suffer from **high code duplication** and **severe behavioral inconsistencies**. 

### Critical Issues Identified:

1.  **Missing Provider Initialization (Fatal Auth Bug)**
    *   `ProviderFactory::GetSteamProvider()->Init()` is **ONLY** called inside `SteamAPI_Init`.
    *   *Impact*: Newer games (Steamworks SDK >= 1.53) heavily use `SteamInternal_SteamAPI_Init` or `SteamAPI_InitFlat` instead of `SteamAPI_Init`. For these games, the EOS/Auth provider is never initialized, breaking cross-play ticket authentication entirely.
2.  **Inconsistent Winsock P2P Hook Logic**
    *   In `SteamAPI_Init` and `SteamInternal_SteamAPI_Init`, the condition to install P2P Winsock hooks is: `if (g_unrealIsEngine && !g_isGoldbergMode)`.
    *   In `SteamAPI_InitFlat`, the condition is: `if (!g_godotIsEngine && !g_isGoldbergMode)`.
    *   *Impact*: Depending on which init function the game happens to invoke, Winsock P2P routing may be incorrectly installed or skipped, leading to connection failures on LAN.
3.  **GameServer Init Incomplete**
    *   `SteamInternal_GameServer_Init` and `SteamGameServer_InitSafe` simply pass through to the original DLL. They do not trigger `InstallVTableHooks()` or capture persona/identity details.
    *   *Impact*: VTable hooks for matchmaking filters or network interceptions might be silently missing on dedicated servers.

## 3. Proposed Centralized `ReFixInitialize` Strategy

To resolve the fragmentation and ensure the proxy acts predictably across all SDK versions, a centralized static function `ReFixInitialize` must be implemented.

### Refactoring Blueprint

```cpp
static bool ReFixInitialize(char* pOutErrMsg) {
    ApplySteamEnv();
    
    // 1. Initialize Auth/EOS Provider globally ONCE
    static bool s_providerInit = false;
    if (!s_providerInit) {
        auto provider = ReFix::ProviderFactory::GetSteamProvider();
        provider->Init();
        ReFixLog("Provider initialized: %s", provider->GetName());
        s_providerInit = true;
    }

    // 2. Load Original DLL
    if (!EnsureOriginal()) {
        if (pOutErrMsg) strncpy_s(pOutErrMsg, 1024, "ReFix: EnsureOriginal failed", _TRUNCATE);
        return false;
    }

    // 3. Fallback Cascade Initialization
    bool result = false;
    if (g_pfn_SteamAPIInit_Internal) {
        char localErr[1024] = { 0 };
        result = (g_pfn_SteamAPIInit_Internal("", localErr) == 0);
        if (!result && pOutErrMsg) strcpy_s(pOutErrMsg, 1024, localErr);
    } else if (g_pfn_InitFlat) {
        char localErr[1024] = { 0 };
        result = (g_pfn_InitFlat(localErr) == 0);
        if (!result && pOutErrMsg) strcpy_s(pOutErrMsg, 1024, localErr);
    } else if (g_pfn_Init) {
        result = g_pfn_Init();
    } else if (g_pfn_InitSafe) {
        result = g_pfn_InitSafe();
    }

    // 4. Uniform Hook Installation
    static bool s_hooksInstalled = false;
    if (result && !s_hooksInstalled) {
        g_steamInitTick = GetTickCount();
        CapturePersonaName();
        TriggerSyntheticRelayCallback();

        // Standardized condition for P2P hooking
        if (g_unrealIsEngine && !g_isGoldbergMode) {
            SteamP2PHook::Install(g_hOriginalDll);
            extern void SteamP2PHook_ForceResolve();
            SteamP2PHook_ForceResolve();
        }

        InstallVTableHooks();
        s_hooksInstalled = true;
    }

    return result;
}
```

**Export Implementations**:
All public-facing initialization exports will shrink to simple wrappers:

```cpp
extern "C" __declspec(dllexport) bool SteamAPI_Init() {
    return ReFixInitialize(nullptr);
}

extern "C" __declspec(dllexport) int SteamInternal_SteamAPI_Init(const char* pszVersion, char* pOutErrMsg) {
    return ReFixInitialize(pOutErrMsg) ? 0 : 1; // 0 = k_ESteamAPIInitResult_OK
}

extern "C" __declspec(dllexport) int SteamAPI_InitFlat(char* pOutErrMsg) {
    return ReFixInitialize(pOutErrMsg) ? 0 : 1;
}
```

This ensures guaranteed provider initialization, identical hook behavior, and substantially reduces code duplication.
