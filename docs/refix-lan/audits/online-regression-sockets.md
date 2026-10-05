# Audit Report: Online Regression Prevention for ISteamNetworkingSockets (Phase 3)

## Objective
Identify the exact boundaries where GetSteamProvider()->GetISteamNetworkingSockets() should be returned, and document how to keep this proxying strictly behind IsLanOnly() and IsOffline() to prevent accidental interception of the real Online provider (steam_api64_valve.dll or steam_api_o.dll).

## Findings & Boundaries

### 1. Proxy32 (src\proxy32\steam_api_proxy.cpp)
**Boundary Methods:**
- SteamInternal_FindOrCreateUserInterface(uint32_t, const char*)
- SteamInternal_CreateInterface(const char*)

**Current Behavior:** 
It currently delegates "SteamNetworkingSockets" to GetOrigInterface. 

**Phase 3 Implementation:**
We must wrap the custom provider's interface injection in a NetworkModeManager check.
`cpp
if (strstr(pszVersion, "SteamNetworkingSockets")) {
    if (ReFix::NetworkModeManager::IsLanOnly() || ReFix::NetworkModeManager::IsOffline()) {
        return ReFix::ProviderFactory::GetSteamProvider()->GetISteamNetworkingSockets();
    }
    // Fall back to original for Online
    void* p = GetOrigInterface("SteamAPI_SteamNetworkingSockets_v009");
    if (!p) p = GetOrigInterface("SteamNetworkingSockets");
    if (IsValidInterfacePtr(p)) return p;
}
`

### 2. Proxy64 (src\steam_proxy.cpp)
**Boundary Methods:**
- Intercepted_SteamInternal_FindOrCreateUserInterface(uint32_t, const char*)
- Intercepted_SteamInternal_CreateInterface(const char*)

**Current Behavior:**
These methods passively pass through to g_pfn_FindOrCreateUserInterface and g_pfn_SteamInternal_CreateInterface and only apply hooks.

**Phase 3 Implementation:**
We must intercept the creation request directly, but **only** if we are operating locally:
`cpp
if (pszVersion && strstr(pszVersion, "SteamNetworkingSockets")) {
    if (ReFix::NetworkModeManager::IsLanOnly() || ReFix::NetworkModeManager::IsOffline()) {
        return ReFix::ProviderFactory::GetSteamProvider()->GetISteamNetworkingSockets();
    }
}
// Fall back to g_pfn_...
`

### 3. Provider Interfaces (src\providers\provider_interfaces.h)
The IReFixSteamProvider class currently lacks the interface accessor. We must add:
`cpp
virtual void* GetISteamNetworkingSockets() = 0;
`
to allow the proxy boundaries to correctly fetch the mocked implementation.

### 4. Unreal Steam Emulator (src\unreal_steam_emu.cpp)
**Boundary Method:**
- UnrealSteamEmu::GetGenericInterface(const char*)

**Current Behavior:**
It unconditionally returns &g_steamNetworkingSocketsInstance without checking the networking mode.
While UnrealDetect_IsReGoldbergActive() attempts to gate the initialization of the emulator based on config (checking if Mode is goldberg), it is safer to explicitly guard the interface return within GetGenericInterface if mixed environments (e.g. Steam + EOS hybrid setups) might still trigger this path in Online mode.

## Conclusion
By isolating the SteamNetworkingSockets override behind ReFix::NetworkModeManager::IsLanOnly() || ReFix::NetworkModeManager::IsOffline() explicitly at the factory boundaries in steam_proxy.cpp and steam_api_proxy.cpp, the proxy guarantees that authentic Online steam_api64_valve.dll requests are perfectly unmodified and correctly bound.
