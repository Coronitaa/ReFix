#pragma once
#include "provider_interfaces.h"
#include <memory>

namespace ReFix {

class ProviderFactory {
public:
    static std::shared_ptr<IReFixSteamProvider> GetSteamProvider();
    static std::shared_ptr<IReFixEOSProvider> GetEOSProvider();
    static std::shared_ptr<IReFixPhotonProvider> GetPhotonProvider();
    static std::shared_ptr<IReFixNetworkProvider> GetNetworkProvider();

    static void Reset();

private:
    static std::shared_ptr<IReFixSteamProvider> s_steamProvider;
    static std::shared_ptr<IReFixEOSProvider> s_eosProvider;
    static std::shared_ptr<IReFixPhotonProvider> s_photonProvider;
    static std::shared_ptr<IReFixNetworkProvider> s_networkProvider;
};

} // namespace ReFix
