#include "provider_factory.h"
#include "../network/network_mode.h"
#include "../unreal_steam_emu.h"
#include <windows.h>
#include <string>
#include <mutex>

extern "C" {
    extern HMODULE g_hOriginalDll;
    bool ReFix_EnsureOriginalDll();
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

    void* GetSteamClient() override {
        return UnrealSteamEmu::GetSteamClient();
    }

    void* GetSteamUser() override {
        return UnrealSteamEmu::GetSteamUser();
    }

    void* GetSteamFriends() override {
        return UnrealSteamEmu::GetSteamFriends();
    }

    void* GetSteamApps() override {
        return UnrealSteamEmu::GetSteamApps();
    }

    void* GetSteamUtils() override {
        return UnrealSteamEmu::GetSteamUtils();
    }

    void* GetSteamMatchmaking() override {
        return UnrealSteamEmu::GetSteamMatchmaking();
    }

    void* GetSteamMatchmakingServers() override {
        return UnrealSteamEmu::GetSteamMatchmakingServers();
    }

    int32_t GetHSteamUser() override {
        return UnrealSteamEmu::GetHSteamUser();
    }

    int32_t GetHSteamPipe() override {
        return UnrealSteamEmu::GetHSteamPipe();
    }

    bool IsSteamRunning() override {
        return UnrealSteamEmu::IsInitialized();
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

    void* GetGameServerNetworkingSockets() override {
        return UnrealSteamEmu::GetSteamNetworkingSockets();
    }

    void* GetGameServerNetworkingMessages() override {
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
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return false;
        typedef bool (*fn_Init_t)();
        fn_Init_t pfn = (fn_Init_t)GetProcAddress(g_hOriginalDll, "SteamAPI_Init");
        if (pfn) return pfn();

        typedef bool (*fn_InitSafe_t)();
        fn_InitSafe_t pfnSafe = (fn_InitSafe_t)GetProcAddress(g_hOriginalDll, "SteamAPI_InitSafe");
        if (pfnSafe) return pfnSafe();

        typedef int (*fn_InitFlat_t)(char*);
        fn_InitFlat_t pfnFlat = (fn_InitFlat_t)GetProcAddress(g_hOriginalDll, "SteamAPI_InitFlat");
        if (pfnFlat) {
            char errMsg[1024] = { 0 };
            return (pfnFlat(errMsg) == 0);
        }

        typedef int (*fn_SteamAPIInit_Internal_t)(const char*, char*);
        fn_SteamAPIInit_Internal_t pfnInternal = (fn_SteamAPIInit_Internal_t)GetProcAddress(g_hOriginalDll, "SteamInternal_SteamAPI_Init");
        if (pfnInternal) {
            char errMsg[1024] = { 0 };
            return (pfnInternal("", errMsg) == 0);
        }

        return false;
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

    void* GetSteamClient() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamClient");
        if (pfn) return pfn();
        return CreateInterface("SteamClient023");
    }

    void* GetSteamUser() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamUser_v021");
        if (!pfn) pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamUser");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamUser021");
    }

    void* GetSteamFriends() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamFriends_v017");
        if (!pfn) pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamFriends");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamFriends017");
    }

    void* GetSteamApps() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamApps_v008");
        if (!pfn) pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamApps");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamApps008");
    }

    void* GetSteamUtils() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamUtils_v010");
        if (!pfn) pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamUtils");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamUtils010");
    }

    void* GetSteamMatchmaking() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamMatchmaking_v009");
        if (!pfn) pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamMatchmaking");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamMatchmaking009");
    }

    void* GetSteamMatchmakingServers() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamMatchmakingServers_v002");
        if (!pfn) pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamMatchmakingServers");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamMatchmakingServers002");
    }

    int32_t GetHSteamUser() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return 0;
        typedef int32_t (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_GetHSteamUser");
        if (!pfn) pfn = (fn_t)GetProcAddress(g_hOriginalDll, "GetHSteamUser");
        return pfn ? pfn() : 0;
    }

    int32_t GetHSteamPipe() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return 0;
        typedef int32_t (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_GetHSteamPipe");
        if (!pfn) pfn = (fn_t)GetProcAddress(g_hOriginalDll, "GetHSteamPipe");
        return pfn ? pfn() : 0;
    }

    bool IsSteamRunning() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return false;
        typedef bool (*fn_t)();
        fn_t pfn = (fn_t)GetProcAddress(g_hOriginalDll, "SteamAPI_IsSteamRunning");
        return pfn ? pfn() : (g_hOriginalDll != nullptr);
    }

    void* GetNetworkingSockets() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_GetSockets_t)();
        fn_GetSockets_t pfn = (fn_GetSockets_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingSockets_SteamAPI_v012");
        if (!pfn) pfn = (fn_GetSockets_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingSockets_SteamAPI_v009");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamNetworkingSockets012");
    }

    void* GetNetworkingUtils() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_GetUtils_t)();
        fn_GetUtils_t pfn = (fn_GetUtils_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004");
        if (!pfn) pfn = (fn_GetUtils_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingUtils_SteamAPI_v003");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamNetworkingUtils004");
    }

    void* GetNetworkingMessages() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_GetMsgs_t)();
        fn_GetMsgs_t pfn = (fn_GetMsgs_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002");
        if (pfn) return pfn();
        return FindOrCreateUserInterface(0, "SteamNetworkingMessages002");
    }

    void* GetGameServerNetworkingSockets() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_GetGSSockets_t)();
        fn_GetGSSockets_t pfn = (fn_GetGSSockets_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012");
        if (!pfn) pfn = (fn_GetGSSockets_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v009");
        if (pfn) return pfn();
        return nullptr;
    }

    void* GetGameServerNetworkingMessages() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return nullptr;
        typedef void* (*fn_GetGSMsgs_t)();
        fn_GetGSMsgs_t pfn = (fn_GetGSMsgs_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002");
        if (pfn) return pfn();
        return nullptr;
    }

    void* FindOrCreateUserInterface(int32_t hUser, const char* pszVersion) override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll || !pszVersion) return nullptr;
        typedef void* (*fn_Find_t)(int32_t, const char*);
        fn_Find_t pfn = (fn_Find_t)GetProcAddress(g_hOriginalDll, "SteamInternal_FindOrCreateUserInterface");
        return pfn ? pfn(hUser, pszVersion) : nullptr;
    }

    void* CreateInterface(const char* pszVersion) override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll || !pszVersion) return nullptr;
        typedef void* (*fn_Create_t)(const char*);
        fn_Create_t pfn = (fn_Create_t)GetProcAddress(g_hOriginalDll, "SteamInternal_CreateInterface");
        if (!pfn) pfn = (fn_Create_t)GetProcAddress(g_hOriginalDll, "CreateInterface");
        return pfn ? pfn(pszVersion) : nullptr;
    }

    void RunCallbacks() override {
        if (!g_hOriginalDll) {
            ReFix_EnsureOriginalDll();
        }
        if (!g_hOriginalDll) return;
        typedef void (*fn_RunCallbacks_t)();
        fn_RunCallbacks_t pfn = (fn_RunCallbacks_t)GetProcAddress(g_hOriginalDll, "SteamAPI_RunCallbacks");
        if (pfn) pfn();
    }
};

static std::mutex s_providerMutex;
std::shared_ptr<IReFixSteamProvider> ProviderFactory::s_steamProvider = nullptr;
std::shared_ptr<IReFixEOSProvider> ProviderFactory::s_eosProvider = nullptr;
std::shared_ptr<IReFixPhotonProvider> ProviderFactory::s_photonProvider = nullptr;
std::shared_ptr<IReFixNetworkProvider> ProviderFactory::s_networkProvider = nullptr;

void ProviderFactory::Reset() {
    std::lock_guard<std::mutex> lock(s_providerMutex);
    if (s_steamProvider) {
        s_steamProvider->Shutdown();
        s_steamProvider = nullptr;
    }
    s_eosProvider = nullptr;
    s_photonProvider = nullptr;
    s_networkProvider = nullptr;
}

std::shared_ptr<IReFixSteamProvider> ProviderFactory::GetSteamProvider() {
    std::lock_guard<std::mutex> lock(s_providerMutex);
    ReFixNetworkMode mode = NetworkModeManager::GetMode();
    if (s_steamProvider) {
        if ((mode == ReFixNetworkMode::Online && strcmp(s_steamProvider->GetName(), "OnlineSteamProvider") != 0) ||
            (mode != ReFixNetworkMode::Online && strcmp(s_steamProvider->GetName(), "LanSteamProvider") != 0)) {
            // Safely shut down active provider before replacing (BLOCKER 9)
            s_steamProvider->Shutdown();
            s_steamProvider = nullptr;
        }
    }
    if (!s_steamProvider) {
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
