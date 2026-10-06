#pragma once
#include <cstdint>

namespace ReFix {

class IReFixSteamProvider {
public:
    virtual ~IReFixSteamProvider() = default;
    
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual const char* GetName() const = 0;

    virtual void* GetSteamClient() = 0;
    virtual void* GetSteamUser() = 0;
    virtual void* GetSteamFriends() = 0;
    virtual void* GetSteamApps() = 0;
    virtual void* GetSteamUtils() = 0;
    virtual void* GetSteamMatchmaking() = 0;
    virtual void* GetSteamMatchmakingServers() = 0;
    virtual int32_t GetHSteamUser() = 0;
    virtual int32_t GetHSteamPipe() = 0;
    virtual bool IsSteamRunning() = 0;

    virtual void* GetNetworkingSockets() = 0;
    virtual void* GetNetworkingUtils() = 0;
    virtual void* GetNetworkingMessages() = 0;
    virtual void* GetGameServerNetworkingSockets() = 0;
    virtual void* GetGameServerNetworkingMessages() = 0;
    virtual void* FindOrCreateUserInterface(int32_t hUser, const char* pszVersion) = 0;
    virtual void* CreateInterface(const char* pszVersion) = 0;
    virtual void RunCallbacks() = 0;
};

class IReFixNetworkProvider {
public:
    virtual ~IReFixNetworkProvider() = default;
    virtual void Update() = 0;
};

class IReFixEOSProvider {
public:
    virtual ~IReFixEOSProvider() = default;
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
};

class IReFixPhotonProvider {
public:
    virtual ~IReFixPhotonProvider() = default;
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
};

} // namespace ReFix
