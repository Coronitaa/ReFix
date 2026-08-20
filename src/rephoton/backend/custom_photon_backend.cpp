#include "custom_photon_backend.h"
#include "../diagnostics/photon_diagnostics.h"
#include "../transport/udp_transport.h"

namespace ReFix::Photon::Backend {

    CustomPhotonBackend::CustomPhotonBackend(const CustomPhotonConfig& config, std::shared_ptr<IPhotonTransport> transport)
        : m_config(config), m_transport(transport) {
        if (!m_transport) {
            m_transport = std::make_shared<Transport::UdpTransport>();
        }

        m_availableRegions = {
            { PhotonRegion::SouthAmerica, "sa", "South America (Custom)", m_config.serverAddress, m_config.serverPort, 20, true },
            { PhotonRegion::NorthAmerica, "us", "North America (Custom)", m_config.serverAddress, m_config.serverPort, 50, true },
            { PhotonRegion::Europe, "eu", "Europe (Custom)", m_config.serverAddress, m_config.serverPort, 110, true },
            { PhotonRegion::Asia, "asia", "Asia (Custom)", m_config.serverAddress, m_config.serverPort, 200, true }
        };

        SelectRegion(m_config.region);
    }

    CustomPhotonBackend::~CustomPhotonBackend() {
        Shutdown();
    }

    bool CustomPhotonBackend::Initialize() {
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Initializing CustomPhoton Backend (Server: %s:%u, Region: %s, Protocol: %s)",
                             m_config.serverAddress.c_str(), m_config.serverPort, m_config.region.c_str(),
                             PhotonTransportToString(m_config.protocol));
        m_state = ConnectionState::Disconnected;
        return true;
    }

    void CustomPhotonBackend::Shutdown() {
        Disconnect();
        if (m_transport) {
            m_transport->Disconnect();
        }
        m_state = ConnectionState::Disconnected;
    }

    bool CustomPhotonBackend::Connect(const std::string& endpoint, uint16_t port) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_state = ConnectionState::ConnectingToNameServer;

        std::string targetHost = endpoint.empty() ? m_config.serverAddress : endpoint;
        uint16_t targetPort = (port == 0) ? m_config.serverPort : port;

        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "CustomPhoton connecting to %s:%u...", targetHost.c_str(), targetPort);

        bool ok = m_transport->Connect(targetHost, targetPort);
        if (ok) {
            m_state = ConnectionState::ConnectedToNameServer;
            Diagnostics::LogInfo(Diagnostics::LogChannel::General, "CustomPhoton connected to %s:%u", targetHost.c_str(), targetPort);
        } else {
            m_state = ConnectionState::Disconnected;
            Diagnostics::LogError(Diagnostics::LogChannel::General, "CustomPhoton failed to connect to %s:%u", targetHost.c_str(), targetPort);
        }
        return ok;
    }

    void CustomPhotonBackend::Disconnect() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_inRoom && m_room) {
            m_room->RemoveActor(m_localActorNr);
            m_inRoom = false;
            m_localActorNr = 0;
        }

        m_authenticated = false;
        m_state = ConnectionState::Disconnected;
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Disconnected from CustomPhoton backend");
    }

    void CustomPhotonBackend::Update() {
        if (!m_transport) return;

        std::vector<uint8_t> packet;
        while (m_transport->PollPacket(packet)) {
            if (packet.empty()) continue;
            size_t offset = 0;
            uint8_t msgType = packet[0];

            if (msgType == static_cast<uint8_t>(Protocol::MessageType::OperationResponse)) {
                Protocol::OperationResponse resp;
                if (Protocol::OperationResponse::Deserialize(packet, offset, resp)) {
                    if (m_opResponseHandler) m_opResponseHandler(resp);
                }
            } else if (msgType == static_cast<uint8_t>(Protocol::MessageType::Event)) {
                Protocol::EventData evt;
                if (Protocol::EventData::Deserialize(packet, offset, evt)) {
                    if (m_eventHandler) m_eventHandler(evt);
                }
            }
        }
    }

    bool CustomPhotonBackend::Authenticate(const std::string& appId, const std::string& appVersion,
                                           const std::string& userId, const std::string& token) {
        m_appId = m_config.realtimeAppId.empty() ? appId : m_config.realtimeAppId;
        m_appVersion = m_config.serverVersion.empty() ? appVersion : m_config.serverVersion;
        m_userId = userId.empty() ? "CustomUser_" + std::to_string(GetTickCount()) : userId;
        m_token = token;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Auth, "CustomPhoton Authenticate (User: '%s', AppId: %s, Version: %s)",
                             m_userId.c_str(), m_appId.c_str(), m_appVersion.c_str());

        m_authenticated = true;
        m_state = ConnectionState::ConnectedToMasterServer;

        if (m_opResponseHandler) {
            Protocol::OperationResponse resp(Protocol::OpCode::Authenticate, Protocol::ErrorCode::Ok);
            resp.SetParam(Protocol::ParameterCode::Address, Protocol::PhotonValue(m_config.serverAddress + ":" + std::to_string(m_config.serverPort)));
            resp.SetParam(Protocol::ParameterCode::UserId, Protocol::PhotonValue(m_userId));
            m_opResponseHandler(resp);
        }

        return true;
    }

    std::vector<RegionInfo> CustomPhotonBackend::GetAvailableRegions() {
        return m_availableRegions;
    }

    bool CustomPhotonBackend::SelectRegion(const std::string& regionCode) {
        for (const auto& reg : m_availableRegions) {
            if (reg.code == regionCode) {
                m_currentRegion = reg;
                return true;
            }
        }
        return false;
    }

    void CustomPhotonBackend::PingAllRegions(std::function<void(const std::vector<RegionInfo>&)> onComplete) {
        if (onComplete) onComplete(m_availableRegions);
    }

    bool CustomPhotonBackend::CreateRoom(const std::string& roomName, const RoomOptions& options) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!m_room) {
            m_room = std::make_shared<Realtime::RoomState>(roomName, options);
        }

        m_localActorNr = m_room->AddActor(m_userId, m_userId);
        m_inRoom = true;
        m_state = ConnectionState::InRoom;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "CustomPhoton created room '%s' (ActorNr: %d)", roomName.c_str(), m_localActorNr);

        if (m_opResponseHandler) {
            Protocol::OperationResponse resp(Protocol::OpCode::CreateGame, Protocol::ErrorCode::Ok);
            resp.SetParam(Protocol::ParameterCode::GameId, Protocol::PhotonValue(roomName));
            resp.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(m_localActorNr));
            m_opResponseHandler(resp);
        }
        return true;
    }

    bool CustomPhotonBackend::JoinRoom(const std::string& roomName) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!m_room) {
            RoomOptions opts;
            m_room = std::make_shared<Realtime::RoomState>(roomName, opts);
        }

        m_localActorNr = m_room->AddActor(m_userId, m_userId);
        m_inRoom = true;
        m_state = ConnectionState::InRoom;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "CustomPhoton joined room '%s' (ActorNr: %d)", roomName.c_str(), m_localActorNr);

        if (m_opResponseHandler) {
            Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::Ok);
            resp.SetParam(Protocol::ParameterCode::GameId, Protocol::PhotonValue(roomName));
            resp.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(m_localActorNr));
            m_opResponseHandler(resp);
        }
        return true;
    }

    bool CustomPhotonBackend::JoinRandomRoom(const Protocol::PhotonHashtable& expectedProperties) {
        return JoinRoom("CustomRandomRoom");
    }

    bool CustomPhotonBackend::LeaveRoom() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!m_inRoom || !m_room) return false;

        m_room->RemoveActor(m_localActorNr);
        m_inRoom = false;
        m_localActorNr = 0;
        m_state = ConnectionState::ConnectedToMasterServer;

        if (m_opResponseHandler) {
            Protocol::OperationResponse resp(Protocol::OpCode::Leave, Protocol::ErrorCode::Ok);
            m_opResponseHandler(resp);
        }
        return true;
    }

    bool CustomPhotonBackend::SetCustomProperties(const Protocol::PhotonHashtable& properties) {
        if (!m_inRoom || !m_room) return false;
        return m_room->UpdateCustomProperties(properties);
    }

    bool CustomPhotonBackend::SetActorProperties(int32_t actorNr, const Protocol::PhotonHashtable& properties) {
        if (!m_inRoom || !m_room) return false;
        return m_room->UpdateActorProperties(actorNr, properties);
    }

    bool CustomPhotonBackend::RaiseEvent(uint8_t eventCode, const Protocol::PhotonValue& eventContent,
                                         uint8_t receiverGroup, uint8_t cachingOption) {
        if (!m_inRoom) return false;

        Protocol::EventData evt(eventCode, m_localActorNr);
        evt.SetParam(Protocol::ParameterCode::Data, eventContent);

        if (m_transport && m_transport->IsConnected()) {
            std::vector<uint8_t> bytes = evt.Serialize();
            m_transport->SendPacket(bytes, true);
        }
        return true;
    }

    RoomStateInfo CustomPhotonBackend::GetCurrentRoom() const {
        if (m_room) return m_room->GetInfo(m_localActorNr);
        return RoomStateInfo();
    }

} // namespace ReFix::Photon::Backend
