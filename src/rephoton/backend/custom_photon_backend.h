#pragma once

#include "../core/photon_interfaces.h"
#include "../core/photon_config.h"
#include "../realtime/room_state.h"
#include <memory>
#include <map>
#include <functional>
#include <mutex>

namespace ReFix::Photon::Backend {

    class CustomPhotonBackend : public IPhotonBackend,
                                public IPhotonAuthentication,
                                public IPhotonRegionProvider,
                                public IPhotonRoomService {
    public:
        explicit CustomPhotonBackend(const CustomPhotonConfig& config, std::shared_ptr<IPhotonTransport> transport = nullptr);
        virtual ~CustomPhotonBackend();

        // IPhotonBackend
        bool Initialize() override;
        void Shutdown() override;

        bool Connect(const std::string& endpoint, uint16_t port) override;
        void Disconnect() override;
        ConnectionState GetState() const override { return m_state; }

        void Update() override;

        IPhotonAuthentication* GetAuth() override { return this; }
        IPhotonRegionProvider* GetRegionProvider() override { return this; }
        IPhotonRoomService* GetRoomService() override { return this; }

        BackendMode GetMode() const override { return BackendMode::CustomPhoton; }
        const char* GetName() const override { return "CustomPhoton"; }

        void OnOperationResponse(std::function<void(const Protocol::OperationResponse&)> handler) override { m_opResponseHandler = handler; }
        void OnEvent(std::function<void(const Protocol::EventData&)> handler) override { m_eventHandler = handler; }

        // IPhotonAuthentication
        bool Authenticate(const std::string& appId, const std::string& appVersion,
                          const std::string& userId, const std::string& token = "") override;
        bool IsAuthenticated() const override { return m_authenticated; }
        std::string GetUserId() const override { return m_userId; }
        std::string GetToken() const override { return m_token; }

        // IPhotonRegionProvider
        std::vector<RegionInfo> GetAvailableRegions() override;
        bool SelectRegion(const std::string& regionCode) override;
        RegionInfo GetCurrentRegion() const override { return m_currentRegion; }
        void PingAllRegions(std::function<void(const std::vector<RegionInfo>&)> onComplete) override;

        // IPhotonRoomService
        bool CreateRoom(const std::string& roomName, const RoomOptions& options) override;
        bool JoinRoom(const std::string& roomName) override;
        bool JoinRandomRoom(const Protocol::PhotonHashtable& expectedProperties = {}) override;
        bool LeaveRoom() override;

        bool SetCustomProperties(const Protocol::PhotonHashtable& properties) override;
        bool SetActorProperties(int32_t actorNr, const Protocol::PhotonHashtable& properties) override;

        bool RaiseEvent(uint8_t eventCode, const Protocol::PhotonValue& eventContent,
                        uint8_t receiverGroup = Protocol::ReceiverGroup::Others,
                        uint8_t cachingOption = Protocol::EventCaching::DoNotCache) override;

        bool IsInRoom() const override { return m_inRoom; }
        RoomStateInfo GetCurrentRoom() const override;

    private:
        CustomPhotonConfig m_config;
        std::shared_ptr<IPhotonTransport> m_transport;
        ConnectionState m_state = ConnectionState::Disconnected;

        bool m_authenticated = false;
        std::string m_appId;
        std::string m_appVersion;
        std::string m_userId;
        std::string m_token;

        RegionInfo m_currentRegion;
        std::vector<RegionInfo> m_availableRegions;

        bool m_inRoom = false;
        int32_t m_localActorNr = 0;
        std::shared_ptr<Realtime::RoomState> m_room;

        std::function<void(const Protocol::OperationResponse&)> m_opResponseHandler;
        std::function<void(const Protocol::EventData&)> m_eventHandler;

        mutable std::mutex m_stateMutex;
    };

} // namespace ReFix::Photon::Backend
