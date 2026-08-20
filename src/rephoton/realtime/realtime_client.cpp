#include "realtime_client.h"
#include "../diagnostics/photon_diagnostics.h"

namespace ReFix::Photon::Realtime {

    RealtimeClient::RealtimeClient(std::shared_ptr<IPhotonBackend> backend)
        : m_backend(backend) {
        SetupCallbacks();
    }

    RealtimeClient::~RealtimeClient() {
        Disconnect();
    }

    void RealtimeClient::SetupCallbacks() {
        if (!m_backend) return;

        m_backend->OnOperationResponse([this](const Protocol::OperationResponse& resp) {
            if (resp.opCode == Protocol::OpCode::CreateGame || resp.opCode == Protocol::OpCode::JoinGame) {
                if (resp.returnCode == Protocol::ErrorCode::Ok) {
                    m_localActorNr = resp.GetParam(Protocol::ParameterCode::ActorNr).AsInt(1);
                    std::string gameId = resp.GetParam(Protocol::ParameterCode::GameId).AsString("Room");
                    if (m_roomJoinedHandler) {
                        m_roomJoinedHandler(gameId, m_localActorNr);
                    }
                }
            }
        });

        m_backend->OnEvent([this](const Protocol::EventData& evt) {
            if (evt.code == Protocol::EventCode::Join) {
                int32_t joinedActor = evt.GetParam(Protocol::ParameterCode::ActorNr).AsInt(evt.senderActorNumber);
                if (m_playerEnteredHandler) m_playerEnteredHandler(joinedActor);
            } else if (evt.code == Protocol::EventCode::Leave) {
                int32_t leftActor = evt.GetParam(Protocol::ParameterCode::ActorNr).AsInt(evt.senderActorNumber);
                if (m_playerLeftHandler) m_playerLeftHandler(leftActor);
            } else {
                // User custom game events
                if (m_userEventHandler) {
                    Protocol::PhotonValue data = evt.GetParam(Protocol::ParameterCode::Data);
                    m_userEventHandler(evt.code, evt.senderActorNumber, data);
                }
            }
        });
    }

    bool RealtimeClient::Connect(const std::string& appId, const std::string& appVersion, const std::string& userId) {
        if (!m_backend) return false;
        m_backend->Initialize();
        m_backend->Connect("", 0);
        return m_backend->GetAuth()->Authenticate(appId, appVersion, userId);
    }

    void RealtimeClient::Disconnect() {
        if (m_backend) {
            m_backend->Disconnect();
        }
        m_localActorNr = 0;
    }

    void RealtimeClient::Update() {
        if (m_backend) {
            m_backend->Update();
        }
    }

    bool RealtimeClient::CreateRoom(const std::string& roomName, const RoomOptions& options) {
        if (!m_backend) return false;
        return m_backend->GetRoomService()->CreateRoom(roomName, options);
    }

    bool RealtimeClient::JoinRoom(const std::string& roomName) {
        if (!m_backend) return false;
        return m_backend->GetRoomService()->JoinRoom(roomName);
    }

    bool RealtimeClient::LeaveRoom() {
        if (!m_backend) return false;
        return m_backend->GetRoomService()->LeaveRoom();
    }

    bool RealtimeClient::SendEvent(uint8_t eventCode, const Protocol::PhotonValue& data, uint8_t targetGroup) {
        if (!m_backend) return false;
        return m_backend->GetRoomService()->RaiseEvent(eventCode, data, targetGroup);
    }

    bool RealtimeClient::SetRoomProperties(const Protocol::PhotonHashtable& props) {
        if (!m_backend) return false;
        return m_backend->GetRoomService()->SetCustomProperties(props);
    }

    bool RealtimeClient::SetPlayerProperties(const Protocol::PhotonHashtable& props) {
        if (!m_backend) return false;
        return m_backend->GetRoomService()->SetActorProperties(m_localActorNr, props);
    }

    ConnectionState RealtimeClient::GetState() const {
        return m_backend ? m_backend->GetState() : ConnectionState::Disconnected;
    }

    int32_t RealtimeClient::GetLocalActorNumber() const {
        return m_localActorNr;
    }

    bool RealtimeClient::IsMasterClient() const {
        if (!m_backend) return false;
        auto room = m_backend->GetRoomService()->GetCurrentRoom();
        return room.masterClientId == m_localActorNr;
    }

    RoomStateInfo RealtimeClient::GetCurrentRoom() const {
        if (m_backend) {
            return m_backend->GetRoomService()->GetCurrentRoom();
        }
        return RoomStateInfo();
    }

    void RealtimeClient::OnEventReceived(std::function<void(uint8_t, int32_t, const Protocol::PhotonValue&)> handler) {
        m_userEventHandler = handler;
    }

    void RealtimeClient::OnRoomJoined(std::function<void(const std::string&, int32_t)> handler) {
        m_roomJoinedHandler = handler;
    }

    void RealtimeClient::OnPlayerEntered(std::function<void(int32_t)> handler) {
        m_playerEnteredHandler = handler;
    }

    void RealtimeClient::OnPlayerLeft(std::function<void(int32_t)> handler) {
        m_playerLeftHandler = handler;
    }

} // namespace ReFix::Photon::Realtime
