#include "voice_backend.h"
#include "../diagnostics/photon_diagnostics.h"

namespace ReFix::Photon::Voice {

    bool VoiceBackendStub::InitializeVoice(const std::string& voiceAppId, const std::string& endpoint) {
        m_appId = voiceAppId;
        m_endpoint = endpoint;
        m_active = true;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Voice, "Voice Backend Stub Initialized (Endpoint: %s, AppId: %s)",
                             m_endpoint.c_str(), m_appId.c_str());
        return true;
    }

    bool VoiceBackendStub::SendAudioData(const std::vector<uint8_t>& audioFrame, uint32_t channelId) {
        if (!m_active) return false;
        Diagnostics::LogDebug(Diagnostics::LogChannel::Voice, "Voice: SendAudioData (Channel %u, Size: %zu bytes)",
                              channelId, audioFrame.size());
        return true;
    }

    void VoiceBackendStub::ShutdownVoice() {
        m_active = false;
        Diagnostics::LogInfo(Diagnostics::LogChannel::Voice, "Voice Backend Stub Shutdown");
    }

} // namespace ReFix::Photon::Voice
