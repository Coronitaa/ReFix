#include "provider_factory.h"
#include "../network/network_mode.h"
#include "../unreal_steam_emu.h"
#include <windows.h>
#include <string>

extern "C" {
    extern HMODULE g_hOriginalDll;
}

namespace ReFix {

// ============================================================================
// LAN STEAM PROVIDER (Emulated Goldberg / UnrealSteamEmu)
// ============================================================================
class LanSteamProvider : public IReFixSteamProvider {
public:
    virtual ~LanSteamProvider() = default;

    bool Init() override {
        return UnrealSteamEmu::Initialize();
    }

    void Shutdown() override {
        UnrealSteamEmu::Shutdown();
    }

    const char* GetName() const override {
        return "LanSteamProvider";
    }

    void* GetNetworkingSockets() override {
        return UnrealSteamEmu::GetSteamNetworkingSockets();
    }

    void* GetNetworkingUtils() override {
        return UnrealSteamEmu::GetSteamNetworkingUtils();
    }

    void* GetNetworkingMessages() override {
        return UnrealSteamEmu::GetSteamNetworkingMessages();
    }

    void* FindOrCreateUserInterface(int32_t hUser, const char* pszVersion) override {
        return UnrealSteamEmu::FindOrCreateUserInterface(hUser, pszVersion);
    }

    void* CreateInterface(const char* pszVersion) override {
        return UnrealSteamEmu::CreateInterface(pszVersion);
    }

    void RunCallbacks() override {
        UnrealSteamEmu::RunCallbacks();
    }
};

// ============================================================================
// ONLINE STEAM PROVIDER (Valve steam_api64.dll Passthrough)
// ============================================================================
class OnlineSteamProvider : public IReFixSteamProvider {
public:
    virtual ~OnlineSteamProvider() = default;

    bool Init() override {
        if (!g_hOriginalDll) return false;
        typedef bool (*fn_Init_t)();
        fn_Init_t pfn = (fn_Init_t)GetProcAddress(g_hOriginalDll, "SteamAPI_Init");
        return pfn ? pfn() : false;
    }

    void Shutdown() override {
        if (!g_hOriginalDll) return;
        typedef void (*fn_Shutdown_t)();
        fn_Shutdown_t pfn = (fn_Shutdown_t)GetProcAddress(g_hOriginalDll, "SteamAPI_Shutdown");
        if (pfn) pfn();
    }

    const char* GetName() const override {
        return "OnlineSteamProvider";
    }

    void* GetNetworkingSockets() override {
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_GetSockets_t)();
        fn_GetSockets_t pfn = (fn_GetSockets_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingSockets_SteamAPI_v012");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamNetworkingSockets012");
    }

    void* GetNetworkingUtils() override {
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_GetUtils_t)();
        fn_GetUtils_t pfn = (fn_GetUtils_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamNetworkingUtils004");
    }

    void* GetNetworkingMessages() override {
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_GetMsgs_t)();
        fn_GetMsgs_t pfn = (fn_GetMsgs_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamNetworkingMessages002");
    }

    void* FindOrCreateUserInterface(int32_t hUser, const char* pszVersion) override {
        if (!g_hOriginalDll || !pszVersion) return nullptr;
        typedef void* (*fn_Find_t)(int32_t, const char*);
        fn_Find_t pfn = (fn_Find_t)GetProcAddress(g_hOriginalDll, "SteamInternal_FindOrCreateUserInterface");
        return pfn ? pfn(hUser, pszVersion) : nullptr;
    }

    void* CreateInterface(const char* pszVersion) override {
        if (!g_hOriginalDll || !pszVersion) return nullptr;
        typedef void* (*fn_Create_t)(const char*);
        fn_Create_t pfn = (fn_Create_t)GetProcAddress(g_hOriginalDll, "SteamInternal_CreateInterface");
        if (!pfn) pfn = (fn_Create_t)GetProcAddress(g_hOriginalDll, "CreateInterface");
        return pfn ? pfn(pszVersion) : nullptr;
    }

    void RunCallbacks() override {
        if (!g_hOriginalDll) return;
        typedef void (*fn_RunCallbacks_t)();
        fn_RunCallbacks_t pfn = (fn_RunCallbacks_t)GetProcAddress(g_hOriginalDll, "SteamAPI_RunCallbacks");
        if (pfn) pfn();
    }
};

std::shared_ptr<IReFixSteamProvider> ProviderFactory::s_steamProvider = nullptr;
std::shared_ptr<IReFixEOSProvider> ProviderFactory::s_eosProvider = nullptr;
std::shared_ptr<IReFixPhotonProvider> ProviderFactory::s_photonProvider = nullptr;
std::shared_ptr<IReFixNetworkProvider> ProviderFactory::s_networkProvider = nullptr;

void ProviderFactory::Reset() {
    s_steamProvider = nullptr;
    s_eosProvider = nullptr;
    s_photonProvider = nullptr;
    s_networkProvider = nullptr;
}

std::shared_ptr<IReFixSteamProvider> ProviderFactory::GetSteamProvider() {
    if (!s_steamProvider) {
        ReFixNetworkMode mode = NetworkModeManager::GetMode();
        if (mode == ReFixNetworkMode::Online) {
            s_steamProvider = std::make_shared<OnlineSteamProvider>();
        } else {
            s_steamProvider = std::make_shared<LanSteamProvider>();
        }
    }
    return s_steamProvider;
}

std::shared_ptr<IReFixEOSProvider> ProviderFactory::GetEOSProvider() {
    return nullptr;
}

std::shared_ptr<IReFixPhotonProvider> ProviderFactory::GetPhotonProvider() {
    return nullptr;
}

std::shared_ptr<IReFixNetworkProvider> ProviderFactory::GetNetworkProvider() {
    return nullptr;
}

} // namespace ReFix
