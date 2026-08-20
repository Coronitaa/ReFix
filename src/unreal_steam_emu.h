#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <string>

class CCallbackBase;

// Public API of the Re:Goldberg Unreal Steam Emulation Layer
namespace UnrealSteamEmu {

    // Lifecycle
    bool Initialize();
    inline bool Init() { return Initialize(); }
    bool InitFlat(char* pOutErrMsg);
    int InitInternal(const char* pszInternalCheckInterfaceVersions, char* pOutErrMsg);
    void Shutdown();
    bool IsInitialized();

    // GameServer Lifecycle
    bool GameServer_Init(uint32_t unIP, uint16_t usGamePort, uint16_t usQueryPort, int eServerMode, const char* pchVersionString);
    bool GameServer_InitSafe();

    // Callbacks & CallResults
    void RunCallbacks();
    void GameServer_RunCallbacks();
    void RegisterCallback(class CCallbackBase* pCallback, int iCallback);
    void UnregisterCallback(class CCallbackBase* pCallback);
    void RegisterCallResult(class CCallbackBase* pCallback, uint64_t hAPICall);
    void UnregisterCallResult(class CCallbackBase* pCallback, uint64_t hAPICall);

    uint64_t PostCallResult(int iCallback, const void* pData, size_t dataSize, double delaySeconds = 0.0);
    void PostCallback(int iCallback, const void* pData, size_t dataSize, double delaySeconds = 0.0);

    bool IsAPICallCompleted(uint64_t hAPICall, bool* pbFailed);
    bool GetAPICallResult(uint64_t hAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed);

    // Manual Dispatch
    void ManualDispatch_RunFrame(int32_t hSteamPipe);
    bool ManualDispatch_GetNextCallback(int32_t hSteamPipe, void* pCallbackMsg);
    void ManualDispatch_FreeLastCallback(int32_t hSteamPipe);
    bool ManualDispatch_GetAPICallResult(int32_t hSteamPipe, uint64_t hSteamAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed);

    // Interface Factory & Accessors
    void* GetGenericInterface(const char* pchVersion);
    void* FindOrCreateUserInterface(int32_t hUser, const char* pszVersion);
    void* FindOrCreateGameServerInterface(int32_t hUser, const char* pszVersion);
    void* CreateInterface(const char* ver);
    void* ContextInit(void* pContextInitData);

    int32_t GetHSteamPipe();
    int32_t GetHSteamUser();
    int32_t GameServer_GetHSteamPipe();
    int32_t GameServer_GetHSteamUser();

    uint64_t GetLocalSteamID();
    const char* GetPersonaName();
    uint32_t GetAppID();

    // Direct interface pointers
    void* GetSteamClient();
    void* GetSteamUser();
    void* GetSteamFriends();
    void* GetSteamUtils();
    void* GetSteamMatchmaking();
    void* GetSteamMatchmakingServers();
    void* GetSteamUserStats();
    void* GetSteamApps();
    void* GetSteamNetworking();
    void* GetSteamNetworkingSockets();
    void* GetSteamNetworkingUtils();
    void* GetSteamRemoteStorage();
    void* GetSteamUGC();
    void* GetSteamGameServer();
    void* GetSteamGameServerStats();
    void* GetSteamGameServerNetworking();
    void* GetSteamHTTP();
    void* GetSteamInput();
    void* GetSteamInventory();
    void* GetSteamScreenshots();
    void* GetSteamTimeline();

    // Aliases with ISteam prefix
    inline void* GetISteamClient() { return GetSteamClient(); }
    inline void* GetISteamUser() { return GetSteamUser(); }
    inline void* GetISteamFriends() { return GetSteamFriends(); }
    inline void* GetISteamUtils() { return GetSteamUtils(); }
    inline void* GetISteamMatchmaking() { return GetSteamMatchmaking(); }
    inline void* GetISteamMatchmakingServers() { return GetSteamMatchmakingServers(); }
    inline void* GetISteamUserStats() { return GetSteamUserStats(); }
    inline void* GetISteamApps() { return GetSteamApps(); }
    inline void* GetISteamNetworking() { return GetSteamNetworking(); }
    inline void* GetISteamNetworkingSockets() { return GetSteamNetworkingSockets(); }
    inline void* GetISteamNetworkingUtils() { return GetSteamNetworkingUtils(); }
    inline void* GetISteamRemoteStorage() { return GetSteamRemoteStorage(); }
    inline void* GetISteamUGC() { return GetSteamUGC(); }
    inline void* GetISteamGameServer() { return GetSteamGameServer(); }
    inline void* GetISteamGameServerStats() { return GetSteamGameServerStats(); }
    inline void* GetISteamGameServerNetworking() { return GetSteamGameServerNetworking(); }
    inline void* GetISteamGameServerUtils() { return GetSteamUtils(); }
    inline void* GetISteamHTTP() { return GetSteamHTTP(); }
    inline void* GetISteamInput() { return GetSteamInput(); }
    inline void* GetISteamInventory() { return GetSteamInventory(); }
    inline void* GetISteamScreenshots() { return GetSteamScreenshots(); }
    inline void* GetISteamTimeline() { return GetSteamTimeline(); }

    // Auth ticket helpers
    uint32_t GetAuthSessionTicket(void* pTicket, int cbMaxTicket, uint32_t* pcbTicket, const void* pSteamNetworkingIdentity = nullptr);
    uint32_t GetAuthTicketForWebApi(const char* pchIdentity);

    // Lobby sync notification with EOS
    void NotifyEOSLobby(uint64_t lobbyID);
}

