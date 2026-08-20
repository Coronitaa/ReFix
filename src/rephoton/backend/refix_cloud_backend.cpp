#include "refix_cloud_backend.h"
#include "../diagnostics/photon_diagnostics.h"
#include "../transport/loopback_transport.h"
#include "../transport/udp_transport.h"

namespace ReFix::Photon::Backend {

    ReFixCloudBackend::ReFixCloudBackend(const ReFixCloudConfig& config, std::shared_ptr<IPhotonTransport> transport)
        : m_config(config), m_transport(transport) {
        if (!m_transport) {
            m_transport = std::make_shared<Transport::UdpTransport>();
        }

        // Initialize available regions
        m_availableRegions = {
            { PhotonRegion::SouthAmerica, "sa", "South America (São Paulo)", "127.0.0.1", 5055, 15, true },
            { PhotonRegion::NorthAmerica, "us", "North America (US East)", "127.0.0.1", 5055, 45, true },
            { PhotonRegion::Europe, "eu", "Europe (Frankfurt)", "127.0.0.1", 5055, 120, true },
            { PhotonRegion::Asia, "asia", "Asia (Singapore)", "127.0.0.1", 5055, 210, true }
        };

        SelectRegion(m_config.preferredRegion);
    }

    ReFixCloudBackend::~ReFixCloudBackend() {
        Shutdown();
    }

    bool ReFixCloudBackend::Initialize() {
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Initializing ReFixCloud Backend (Region: %s, Preferred NS: %s)",
                             m_currentRegion.code.c_str(), m_config.nameServerEndpoint.c_str());
        m_state = ConnectionState::Disconnected;
        return true;
    }

    void ReFixCloudBackend::Shutdown() {
        Disconnect();
        if (m_transport) {
            m_transport->Disconnect();
        }
        m_state = ConnectionState::Disconnected;
    }

    bool ReFixCloudBackend::Connect(const std::string& endpoint, uint16_t port) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_state = ConnectionState::ConnectingToNameServer;
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Connecting to NameServer at %s:%u...", endpoint.c_str(), port);

        bool ok = m_transport->Connect(endpoint, port);
        if (ok) {
            m_state = ConnectionState::ConnectedToNameServer;
            Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Connected to NameServer successfully");
        } else {
            m_state = ConnectionState::Disconnected;
            Diagnostics::LogError(Diagnostics::LogChannel::General, "Failed to connect to NameServer at %s:%u", endpoint.c_str(), port);
        }
        return ok;
    }

    void ReFixCloudBackend::Disconnect() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_inRoom && m_room) {
            m_room->RemoveActor(m_localActorNr);
            m_inRoom = false;
            m_localActorNr = 0;
        }

        m_authenticated = false;
        m_state = ConnectionState::Disconnected;
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Disconnected from ReFixCloud backend");
    }

    void ReFixCloudBackend::Update() {
        if (!m_transport) return;

        std::vector<uint8_t> packet;
        while (m_transport->PollPacket(packet)) {
            HandleIncomingPacket(packet);
        }
    }

    void ReFixCloudBackend::HandleIncomingPacket(const std::vector<uint8_t>& packet) {
        if (packet.empty()) return;

        size_t offset = 0;
        uint8_t msgType = packet[0];

        if (msgType == static_cast<uint8_t>(Protocol::MessageType::OperationResponse)) {
            Protocol::OperationResponse resp;
            if (Protocol::OperationResponse::Deserialize(packet, offset, resp)) {
                Diagnostics::LogDebug(Diagnostics::LogChannel::General, "Received OpResponse (OpCode: %u, ReturnCode: %d)",
                                      resp.opCode, resp.returnCode);
                if (m_opResponseHandler) {
                    m_opResponseHandler(resp);
                }
            }
        } else if (msgType == static_cast<uint8_t>(Protocol::MessageType::Event)) {
            Protocol::EventData evt;
            if (Protocol::EventData::Deserialize(packet, offset, evt)) {
                Diagnostics::LogDebug(Diagnostics::LogChannel::General, "Received Event (EventCode: %u, Sender: %d)",
                                      evt.code, evt.senderActorNumber);
                if (m_eventHandler) {
                    m_eventHandler(evt);
                }
            }
        }
    }

    // =========================================================================
    // IPhotonAuthentication
    // =========================================================================
    bool ReFixCloudBackend::Authenticate(const std::string& appId, const std::string& appVersion,
                                         const std::string& userId, const std::string& token) {
        m_appId = appId;
        m_appVersion = appVersion;
        m_userId = userId.empty() ? "ReFixUser_" + std::to_string(GetTickCount()) : userId;
        m_token = token.empty() ? "refix_token_valid" : token;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Auth, "Authenticating user '%s' (AppId: %s, Version: %s)",
                             m_userId.c_str(), m_appId.c_str(), m_appVersion.c_str());

        m_state = ConnectionState::Authenticating;

        // Build OpAuthenticate request
        Protocol::OperationRequest req(Protocol::OpCode::Authenticate);
        req.SetParam(Protocol::ParameterCode::AppId, Protocol::PhotonValue(m_appId));
        req.SetParam(Protocol::ParameterCode::AppVersion, Protocol::PhotonValue(m_appVersion));
        req.SetParam(Protocol::ParameterCode::UserId, Protocol::PhotonValue(m_userId));

        m_authenticated = true;
        m_state = ConnectionState::ConnectedToMasterServer;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Auth, "Authentication successful -> Transitioned to MasterServer");

        // Dispatch synthetic OpResponse for local testing if handler attached
        if (m_opResponseHandler) {
            Protocol::OperationResponse resp(Protocol::OpCode::Authenticate, Protocol::ErrorCode::Ok);
            resp.SetParam(Protocol::ParameterCode::Address, Protocol::PhotonValue(m_config.masterServerEndpoint));
            resp.SetParam(Protocol::ParameterCode::UserId, Protocol::PhotonValue(m_userId));
            m_opResponseHandler(resp);
        }

        return true;
    }

    // =========================================================================
    // IPhotonRegionProvider
    // =========================================================================
    std::vector<RegionInfo> ReFixCloudBackend::GetAvailableRegions() {
        return m_availableRegions;
    }

    bool ReFixCloudBackend::SelectRegion(const std::string& regionCode) {
        for (const auto& reg : m_availableRegions) {
            if (reg.code == regionCode) {
                m_currentRegion = reg;
                Diagnostics::LogInfo(Diagnostics::LogChannel::General, "Selected Region: %s (%s)", reg.code.c_str(), reg.name.c_str());
                return true;
            }
        }
        if (!m_availableRegions.empty()) {
            m_currentRegion = m_availableRegions[0];
            return true;
        }
        return false;
    }

    void ReFixCloudBackend::PingAllRegions(std::function<void(const std::vector<RegionInfo>&)> onComplete) {
        if (onComplete) {
            onComplete(m_availableRegions);
        }
    }

    // =========================================================================
    // IPhotonRoomService
    // =========================================================================
    void ReFixCloudBackend::SetDirectRoomInstance(std::shared_ptr<Realtime::RoomState> room) {
        m_room = room;
    }

    bool ReFixCloudBackend::CreateRoom(const std::string& roomName, const RoomOptions& options) {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        if (!m_room) {
            m_room = std::make_shared<Realtime::RoomState>(roomName, options);
        }

        m_localActorNr = m_room->AddActor(m_userId, m_userId);
        if (m_localActorNr == 0) {
            Diagnostics::LogError(Diagnostics::LogChannel::Room, "Failed to create room '%s': capacity exceeded or invalid state", roomName.c_str());
            return false;
        }

        m_inRoom = true;
        m_state = ConnectionState::InRoom;
        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Room '%s' created successfully (Assigned ActorNumber: %d, MasterClient: %d)",
                             roomName.c_str(), m_localActorNr, m_room->GetMasterClientId());

        if (m_opResponseHandler) {
            Protocol::OperationResponse resp(Protocol::OpCode::CreateGame, Protocol::ErrorCode::Ok);
            resp.SetParam(Protocol::ParameterCode::GameId, Protocol::PhotonValue(roomName));
            resp.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(m_localActorNr));
            resp.SetParam(Protocol::ParameterCode::Address, Protocol::PhotonValue(m_config.gameServerEndpoint));
            m_opResponseHandler(resp);
        }

        return true;
    }

    bool ReFixCloudBackend::JoinRoom(const std::string& roomName) {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        if (!m_room) {
            Diagnostics::LogError(Diagnostics::LogChannel::Room, "Cannot join room '%s': Room does not exist", roomName.c_str());
            if (m_opResponseHandler) {
                Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::GameDoesNotExist, "Game does not exist");
                m_opResponseHandler(resp);
            }
            return false;
        }

        m_localActorNr = m_room->AddActor(m_userId, m_userId);
        if (m_localActorNr == 0) {
            Diagnostics::LogError(Diagnostics::LogChannel::Room, "Failed to join room '%s': Room is full or closed", roomName.c_str());
            if (m_opResponseHandler) {
                Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::GameFull, "Game is full");
                m_opResponseHandler(resp);
            }
            return false;
        }

        m_inRoom = true;
        m_state = ConnectionState::InRoom;
        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Joined room '%s' successfully (Assigned ActorNumber: %d)",
                             roomName.c_str(), m_localActorNr);

        if (m_opResponseHandler) {
            Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::Ok);
            resp.SetParam(Protocol::ParameterCode::GameId, Protocol::PhotonValue(roomName));
            resp.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(m_localActorNr));
            resp.SetParam(Protocol::ParameterCode::Address, Protocol::PhotonValue(m_config.gameServerEndpoint));
            m_opResponseHandler(resp);
        }

        // Notify room members of join event
        if (m_eventHandler) {
            Protocol::EventData joinEvt(Protocol::EventCode::Join, m_localActorNr);
            joinEvt.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(m_localActorNr));
            m_eventHandler(joinEvt);
        }

        return true;
    }

    bool ReFixCloudBackend::JoinRandomRoom(const Protocol::PhotonHashtable& expectedProperties) {
        if (!m_room) {
            // Auto-create room for testing
            RoomOptions opts;
            return CreateRoom("AutoRoom_" + std::to_string(GetTickCount()), opts);
        }
        return JoinRoom(m_room->GetName());
    }

    bool ReFixCloudBackend::LeaveRoom() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!m_inRoom || !m_room) return false;

        int32_t leavingActor = m_localActorNr;
        m_room->RemoveActor(leavingActor);
        m_inRoom = false;
        m_localActorNr = 0;
        m_state = ConnectionState::ConnectedToMasterServer;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Actor %d left the room", leavingActor);

        if (m_opResponseHandler) {
            Protocol::OperationResponse resp(Protocol::OpCode::Leave, Protocol::ErrorCode::Ok);
            m_opResponseHandler(resp);
        }

        if (m_eventHandler) {
            Protocol::EventData leaveEvt(Protocol::EventCode::Leave, leavingActor);
            leaveEvt.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(leavingActor));
            m_eventHandler(leaveEvt);
        }

        return true;
    }

    bool ReFixCloudBackend::SetCustomProperties(const Protocol::PhotonHashtable& properties) {
        if (!m_inRoom || !m_room) return false;
        bool ok = m_room->UpdateCustomProperties(properties);
        if (ok && m_eventHandler) {
            Protocol::EventData propEvt(Protocol::EventCode::PropertiesChanged, m_localActorNr);
            propEvt.SetParam(Protocol::ParameterCode::Properties, Protocol::PhotonValue(properties));
            m_eventHandler(propEvt);
        }
        return ok;
    }

    bool ReFixCloudBackend::SetActorProperties(int32_t actorNr, const Protocol::PhotonHashtable& properties) {
        if (!m_inRoom || !m_room) return false;
        bool ok = m_room->UpdateActorProperties(actorNr, properties);
        if (ok && m_eventHandler) {
            Protocol::EventData propEvt(Protocol::EventCode::PropertiesChanged, m_localActorNr);
            propEvt.SetParam(Protocol::ParameterCode::ActorProperties, Protocol::PhotonValue(properties));
            propEvt.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(actorNr));
            m_eventHandler(propEvt);
        }
        return ok;
    }

    bool ReFixCloudBackend::RaiseEvent(uint8_t eventCode, const Protocol::PhotonValue& eventContent,
                                       uint8_t receiverGroup, uint8_t cachingOption) {
        if (!m_inRoom) return false;

        Diagnostics::LogDebug(Diagnostics::LogChannel::Realtime, "RaiseEvent Code=%u from Actor %d (Group: %u)",
                              eventCode, m_localActorNr, receiverGroup);

        Protocol::EventData evt(eventCode, m_localActorNr);
        evt.SetParam(Protocol::ParameterCode::Data, eventContent);
        evt.SetParam(Protocol::ParameterCode::Code, Protocol::PhotonValue(eventCode));

        // Transmit across transport if connected
        if (m_transport && m_transport->IsConnected()) {
            std::vector<uint8_t> bytes = evt.Serialize();
            m_transport->SendPacket(bytes, true);
        }

        // Local loopback echo if handler attached
        if (receiverGroup == Protocol::ReceiverGroup::All && m_eventHandler) {
            m_eventHandler(evt);
        }

        return true;
    }

    RoomStateInfo ReFixCloudBackend::GetCurrentRoom() const {
        if (m_room) {
            return m_room->GetInfo(m_localActorNr);
        }
        return RoomStateInfo();
    }

} // namespace ReFix::Photon::Backend
