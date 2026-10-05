#include "provider_factory.h"
#include "../network/network_mode.h"

namespace ReFix {

class DummySteamProvider : public IReFixSteamProvider {
public:
    DummySteamProvider(const char* name) : m_name(name) {}
    bool Init() override {
        // We will call EnsureOriginal inside the proxy for now
        // This just serves as the selection boundary
        return true; 
    }
    void Shutdown() override {}
    const char* GetName() const override { return m_name; }
private:
    const char* m_name;
};

std::shared_ptr<IReFixSteamProvider> ProviderFactory::s_steamProvider = nullptr;

std::shared_ptr<IReFixSteamProvider> ProviderFactory::GetSteamProvider() {
    if (!s_steamProvider) {
        ReFixNetworkMode mode = NetworkModeManager::GetMode();
        if (mode == ReFixNetworkMode::Online) {
            s_steamProvider = std::make_shared<DummySteamProvider>("OnlineSteamProvider");
        } else {
            s_steamProvider = std::make_shared<DummySteamProvider>("LanSteamProvider");
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
