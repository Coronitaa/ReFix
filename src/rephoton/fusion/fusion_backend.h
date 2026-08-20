#pragma once

#include "../core/photon_interfaces.h"
#include <string>
#include <vector>

namespace ReFix::Photon::Fusion {

    class FusionBackendStub : public IFusionBackend {
    public:
        FusionBackendStub() = default;
        virtual ~FusionBackendStub() = default;

        bool InitializeFusion(const std::string& fusionAppId, bool isSharedMode) override;
        bool SendTickState(uint32_t tick, const std::vector<uint8_t>& stateData) override;
        void ShutdownFusion() override;
        bool IsFusionActive() const override { return m_active; }

    private:
        bool m_active = false;
        bool m_isSharedMode = true;
        std::string m_appId;
    };

} // namespace ReFix::Photon::Fusion
