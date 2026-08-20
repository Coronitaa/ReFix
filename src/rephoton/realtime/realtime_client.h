#pragma once

#include "../core/photon_interfaces.h"
#include <memory>
#include <functional>

namespace ReFix::Photon::Realtime {

    class RealtimeClient {
    public:
        explicit RealtimeClient(std::shared_ptr<IPhotonBackend> backend);
        ~RealtimeClient();

        bool Connect(const std::string& appId, const std::string& appVersion, const std::string& userId);
        void Disconnect();
        void Update();

        bool CreateRoom(const std::string& roomName, const RoomOptions& options = RoomOptions());
        bool JoinRoom(const std::string& roomName);
        bool LeaveRoom();

        bool SendEvent(uint8_t eventCode, const Protocol::PhotonValue& data, uint8_t targetGroup = Protocol::ReceiverGroup::Others);
        bool SetRoomProperties(const Protocol::PhotonHashtable& props);
        bool SetPlayerProperties(const Protocol::PhotonHashtable& props);

        ConnectionState GetState() const;
        int32_t GetLocalActorNumber() const;
        bool IsMasterClient() const;
        RoomStateInfo GetCurrentRoom() const;

        void OnEventReceived(std::function<void(uint8_t code, int32_t sender, const Protocol::PhotonValue& data)> handler);
        void OnRoomJoined(std::function<void(const std::string& roomName, int32_t actorNr)> handler);
        void OnPlayerEntered(std::function<void(int32_t actorNr)> handler);
        void OnPlayerLeft(std::function<void(int32_t actorNr)> handler);

    private:
        void SetupCallbacks();

        std::shared_ptr<IPhotonBackend> m_backend;
        int32_t m_localActorNr = 0;

        std::function<void(uint8_t, int32_t, const Protocol::PhotonValue&)> m_userEventHandler;
        std::function<void(const std::string&, int32_t)> m_roomJoinedHandler;
        std::function<void(int32_t)> m_playerEnteredHandler;
        std::function<void(int32_t)> m_playerLeftHandler;
    };

} // namespace ReFix::Photon::Realtime
