#pragma once

#include "../core/photon_interfaces.h"
#include <string>
#include <vector>

namespace ReFix::Photon::Chat {

    class ChatBackendStub : public IPhotonChatBackend {
    public:
        ChatBackendStub() = default;
        virtual ~ChatBackendStub() = default;

        bool ConnectChat(const std::string& chatAppId, const std::string& userId) override;
        bool SubscribeChannel(const std::string& channelName) override;
        bool SendChannelMessage(const std::string& channelName, const std::string& message) override;
        void DisconnectChat() override;
        bool IsChatConnected() const override { return m_connected; }

    private:
        bool m_connected = false;
        std::string m_appId;
        std::string m_userId;
        std::vector<std::string> m_channels;
    };

} // namespace ReFix::Photon::Chat
