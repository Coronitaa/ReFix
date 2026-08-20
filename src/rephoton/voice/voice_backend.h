#pragma once

#include "../core/photon_interfaces.h"
#include <string>
#include <vector>

namespace ReFix::Photon::Voice {

    class VoiceBackendStub : public IPhotonVoiceBackend {
    public:
        VoiceBackendStub() = default;
        virtual ~VoiceBackendStub() = default;

        bool InitializeVoice(const std::string& voiceAppId, const std::string& endpoint) override;
        bool SendAudioData(const std::vector<uint8_t>& audioFrame, uint32_t channelId) override;
        void ShutdownVoice() override;
        bool IsVoiceActive() const override { return m_active; }

    private:
        bool m_active = false;
        std::string m_appId;
        std::string m_endpoint;
    };

} // namespace ReFix::Photon::Voice
