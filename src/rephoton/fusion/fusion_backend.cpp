#include "fusion_backend.h"
#include "../diagnostics/photon_diagnostics.h"

namespace ReFix::Photon::Fusion {

    bool FusionBackendStub::InitializeFusion(const std::string& fusionAppId, bool isSharedMode) {
        m_appId = fusionAppId;
        m_isSharedMode = isSharedMode;
        m_active = true;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Fusion, "Fusion Stub Initialized (Mode: %s, AppId: %s)",
                             isSharedMode ? "SharedMode" : "ServerMode", m_appId.c_str());
        return true;
    }

    bool FusionBackendStub::SendTickState(uint32_t tick, const std::vector<uint8_t>& stateData) {
        if (!m_active) return false;
        Diagnostics::LogDebug(Diagnostics::LogChannel::Fusion, "Fusion: SendTickState (Tick %u, Size: %zu bytes)",
                              tick, stateData.size());
        return true;
    }

    void FusionBackendStub::ShutdownFusion() {
        m_active = false;
        Diagnostics::LogInfo(Diagnostics::LogChannel::Fusion, "Fusion Stub Shutdown");
    }

} // namespace ReFix::Photon::Fusion
