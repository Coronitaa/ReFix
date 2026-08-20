#include "chat_backend.h"
#include "../diagnostics/photon_diagnostics.h"

namespace ReFix::Photon::Chat {

    bool ChatBackendStub::ConnectChat(const std::string& chatAppId, const std::string& userId) {
        m_appId = chatAppId;
        m_userId = userId;
        m_connected = true;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Chat, "Chat Backend Stub Connected (User: %s, AppId: %s)",
                             m_userId.c_str(), m_appId.c_str());
        return true;
    }

    bool ChatBackendStub::SubscribeChannel(const std::string& channelName) {
        if (!m_connected) return false;
        m_channels.push_back(channelName);
        Diagnostics::LogInfo(Diagnostics::LogChannel::Chat, "Chat: Subscribed to channel '%s'", channelName.c_str());
        return true;
    }

    bool ChatBackendStub::SendChannelMessage(const std::string& channelName, const std::string& message) {
        if (!m_connected) return false;
        Diagnostics::LogDebug(Diagnostics::LogChannel::Chat, "Chat: Message to '%s' -> '%s'", channelName.c_str(), message.c_str());
        return true;
    }

    void ChatBackendStub::DisconnectChat() {
        m_connected = false;
        m_channels.clear();
        Diagnostics::LogInfo(Diagnostics::LogChannel::Chat, "Chat Backend Stub Disconnected");
    }

} // namespace ReFix::Photon::Chat
