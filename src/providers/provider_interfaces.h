#pragma once
#include <cstdint>

namespace ReFix {

class IReFixSteamProvider {
public:
    virtual ~IReFixSteamProvider() = default;
    
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual const char* GetName() const = 0;
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
