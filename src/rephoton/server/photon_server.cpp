#include "photon_server.h"
#include "../diagnostics/photon_diagnostics.h"
#include "../protocol/photon_constants.h"
#include "../protocol/photon_serializer.h"
#include <algorithm>
#include <cstdio>

#pragma comment(lib, "ws2_32.lib")

namespace ReFix::Photon::Server {

    PhotonServer::PhotonServer() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    PhotonServer::~PhotonServer() {
        Stop();
        WSACleanup();
    }

    std::vector<std::string> PhotonServer::GetAvailableRegions() const {
        return { "sa", "us", "eu", "asia" };
    }

    bool PhotonServer::Start(uint16_t masterPort, uint16_t nameServerPort) {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        if (m_running) return true;

        m_port = masterPort;
        m_nameServerPort = nameServerPort;

        // 1. Create and bind Master Server socket
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET) {
            Diagnostics::LogError(Diagnostics::LogChannel::General, "PhotonServer: socket creation failed (%d)", WSAGetLastError());
            return false;
        }

        // Set non-blocking socket
        u_long nonBlocking = 1;
        ioctlsocket(m_socket, FIONBIO, &nonBlocking);

        // Increase socket buffers
        int bufferSize = 1024 * 1024;
        setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize));
        setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize));

        sockaddr_in bindAddr{};
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_port = htons(masterPort);
        bindAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_socket, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
            Diagnostics::LogError(Diagnostics::LogChannel::General, "PhotonServer: bind failed on Master port %u (%d)", masterPort, WSAGetLastError());
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        // 2. Create and bind Name Server socket (if distinct port requested)
        if (nameServerPort != 0 && nameServerPort != masterPort) {
            m_nameServerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (m_nameServerSocket != INVALID_SOCKET) {
                ioctlsocket(m_nameServerSocket, FIONBIO, &nonBlocking);
                setsockopt(m_nameServerSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize));
                setsockopt(m_nameServerSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize));

                sockaddr_in nsBindAddr{};
                nsBindAddr.sin_family = AF_INET;
                nsBindAddr.sin_port = htons(nameServerPort);
                nsBindAddr.sin_addr.s_addr = INADDR_ANY;

                if (bind(m_nameServerSocket, reinterpret_cast<sockaddr*>(&nsBindAddr), sizeof(nsBindAddr)) == SOCKET_ERROR) {
                    Diagnostics::LogWarn(Diagnostics::LogChannel::General, "PhotonServer: bind failed on NameServer port %u (%d) - operating in unified Master mode", nameServerPort, WSAGetLastError());
                    closesocket(m_nameServerSocket);
                    m_nameServerSocket = INVALID_SOCKET;
                }
            }
        }

        m_running = true;
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "====================================================================");
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, " [Re:Photon] Master Server listening on 0.0.0.0:%u", masterPort);
        if (m_nameServerSocket != INVALID_SOCKET) {
            Diagnostics::LogInfo(Diagnostics::LogChannel::General, " [Re:Photon] Name Server listening on   0.0.0.0:%u", nameServerPort);
        }
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, " [Re:Photon] Default Region: 'sa' (South America) -> 127.0.0.1:%u", masterPort);
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "====================================================================");

        return true;
    }

    void PhotonServer::Stop() {
        m_running = false;
        std::lock_guard<std::mutex> lock(m_socketMutex);
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        if (m_nameServerSocket != INVALID_SOCKET) {
            closesocket(m_nameServerSocket);
            m_nameServerSocket = INVALID_SOCKET;
        }
        Diagnostics::LogInfo(Diagnostics::LogChannel::General, "PhotonServer stopped");
    }

    std::string PhotonServer::AddrKey(const sockaddr_in& addr) {
        char ipStr[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));
        return std::string(ipStr) + ":" + std::to_string(ntohs(addr.sin_port));
    }

    std::shared_ptr<PeerConnection> PhotonServer::FindPeerByAddress(const sockaddr_in& addr) {
        std::lock_guard<std::mutex> lock(m_peersMutex);
        auto it = m_peersByAddr.find(AddrKey(addr));
        return (it != m_peersByAddr.end()) ? it->second : nullptr;
    }

    std::shared_ptr<PeerConnection> PhotonServer::GetOrCreatePeer(const sockaddr_in& addr, uint16_t peerId, uint16_t localPort) {
        std::lock_guard<std::mutex> lock(m_peersMutex);
        std::string key = AddrKey(addr);
        auto it = m_peersByAddr.find(key);
        if (it != m_peersByAddr.end()) {
            it->second->UpdateActivity();
            if (localPort != 0) it->second->SetLocalPort(localPort);
            return it->second;
        }

        uint16_t assignedId = (peerId == 0 || peerId == 0xFFFF) ? (m_nextPeerId++) : peerId;
        auto peer = std::make_shared<PeerConnection>(assignedId, addr);
        peer->SetLocalPort(localPort);
        m_peersByAddr[key] = peer;
        m_peersById[assignedId] = peer;

        Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "New Peer Connection from %s (Assigned PeerId: %u, Port: %u)",
                             key.c_str(), assignedId, localPort);

        return peer;
    }

    void PhotonServer::Update() {
        if (!m_running) return;

        uint8_t buffer[8192];
        sockaddr_in fromAddr{};
        int fromLen = sizeof(fromAddr);

        // 1. Poll MasterServer socket
        if (m_socket != INVALID_SOCKET) {
            while (true) {
                int bytesRead = recvfrom(m_socket, reinterpret_cast<char*>(buffer), sizeof(buffer),
                                         0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

                if (bytesRead == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK && err != WSAECONNRESET) {
                        Diagnostics::LogError(Diagnostics::LogChannel::Transport, "PhotonServer Master: recvfrom error %d", err);
                    }
                    break;
                }

                if (bytesRead > 0) {
                    Diagnostics::DiagnosticsEngine::Instance().RecordPacketReceived(bytesRead);
                    ProcessIncomingDatagram(buffer, static_cast<size_t>(bytesRead), fromAddr, m_port);
                }
            }
        }

        // 2. Poll NameServer socket
        if (m_nameServerSocket != INVALID_SOCKET) {
            while (true) {
                int bytesRead = recvfrom(m_nameServerSocket, reinterpret_cast<char*>(buffer), sizeof(buffer),
                                         0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

                if (bytesRead == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK && err != WSAECONNRESET) {
                        Diagnostics::LogError(Diagnostics::LogChannel::Transport, "PhotonServer NameServer: recvfrom error %d", err);
                    }
                    break;
                }

                if (bytesRead > 0) {
                    Diagnostics::DiagnosticsEngine::Instance().RecordPacketReceived(bytesRead);
                    ProcessIncomingDatagram(buffer, static_cast<size_t>(bytesRead), fromAddr, m_nameServerPort);
                }
            }
        }

        CleanupStaleConnections();
    }

    void PhotonServer::ProcessIncomingDatagram(const uint8_t* data, size_t len, const sockaddr_in& fromAddr, uint16_t localPort) {
        // 1. Photon Region UDP Ping Packet (PingMono / PhotonPing: 13+ bytes starting with 0x7D 0x7D)
        if (len >= 2 && data[0] == 0x7D && data[1] == 0x7D) {
            std::vector<uint8_t> echoPacket(data, data + len);
            SendDatagram(fromAddr, echoPacket, localPort);
            Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "[Ping] Responded to Photon Region UDP Ping from %s (Len: %zu, Port: %u)",
                                 AddrKey(fromAddr).c_str(), len, localPort);
            return;
        }

        if (len < sizeof(ENetDatagramHeader)) return;

        uint16_t peerId = ReadBE16(data);
        uint8_t flags = data[2];
        uint8_t cmdCount = data[3];
        uint32_t timestamp = ReadBE32(data + 4);
        uint32_t challenge = ReadBE32(data + 8);

        auto peer = GetOrCreatePeer(fromAddr, peerId, localPort);
        if (!peer) return;

        peer->SetChallenge(challenge);

        size_t offset = (flags == 0xCC) ? 16 : sizeof(ENetDatagramHeader);

        for (uint8_t c = 0; c < cmdCount && offset < len; ++c) {
            if (offset + sizeof(ENetCommandHeader) > len) break;

            ENetCommandHeader cmdHeader;
            cmdHeader.commandType = data[offset];
            cmdHeader.channelId = data[offset + 1];
            cmdHeader.commandFlags = data[offset + 2];
            cmdHeader.reserved = data[offset + 3];
            cmdHeader.commandLength = ReadBE32(data + offset + 4);
            cmdHeader.reliableSequenceNumber = ReadBE32(data + offset + 8);

            if (cmdHeader.commandLength < sizeof(ENetCommandHeader) || offset + cmdHeader.commandLength > len) {
                Diagnostics::LogWarn(Diagnostics::LogChannel::Transport, "Malformed ENet command (Length: %u, Offset: %zu, Total: %zu)",
                                     cmdHeader.commandLength, offset, len);
                break;
            }

            const uint8_t* cmdData = data + offset + sizeof(ENetCommandHeader);
            size_t cmdDataLen = cmdHeader.commandLength - sizeof(ENetCommandHeader);

            ProcessCommand(peer, cmdHeader, cmdData, cmdDataLen, timestamp);

            offset += cmdHeader.commandLength;
        }
    }

    void PhotonServer::ProcessCommand(std::shared_ptr<PeerConnection> peer, const ENetCommandHeader& cmdHeader,
                                      const uint8_t* cmdData, size_t cmdDataLen, uint32_t sentTimestamp) {
        // Send ACK if reliable flag is set
        if (cmdHeader.commandFlags & 0x01) {
            SendAck(peer, cmdHeader.channelId, cmdHeader.reliableSequenceNumber, sentTimestamp);
        }

        switch (cmdHeader.commandType) {
            case ENetCommandType::Connect: {
                uint32_t challenge = peer->GetChallenge();
                SendVerifyConnect(peer->GetAddress(), peer->GetPeerId(), challenge, peer->GetLocalPort());
                peer->SetState(PeerState::Connected);
                Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "Peer %u: Handshake Connect -> VerifyConnect sent (Echo Challenge: 0x%08X, Port: %u)",
                                     peer->GetPeerId(), challenge, peer->GetLocalPort());
                break;
            }
            case ENetCommandType::Acknowledge: {
                peer->UpdateActivity();
                break;
            }
            case ENetCommandType::Ping: {
                peer->UpdateActivity();
                break;
            }
            case ENetCommandType::Disconnect: {
                Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "Peer %u disconnected gracefully", peer->GetPeerId());
                HandlePeerDisconnect(peer, "Graceful disconnect");
                break;
            }
            case ENetCommandType::SendReliable:
            case ENetCommandType::SendUnreliable: {
                peer->UpdateActivity();

                size_t payloadOffset = 0;
                if (cmdHeader.commandType == ENetCommandType::SendUnreliable) {
                    // Unreliable has 4 bytes sequence header before payload
                    if (cmdDataLen < 4) return;
                    payloadOffset = 4;
                }

                if (cmdDataLen <= payloadOffset) return;

                const uint8_t* payload = cmdData + payloadOffset;
                size_t payloadLen = cmdDataLen - payloadOffset;

                Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "Peer %u: Payload received (Len: %zu, CmdType: %u, Ch: %u, Byte0: 0x%02X, Byte1: 0x%02X)",
                                     peer->GetPeerId(), payloadLen, static_cast<uint8_t>(cmdHeader.commandType), cmdHeader.channelId,
                                     payloadLen > 0 ? payload[0] : 0, payloadLen > 1 ? payload[1] : 0);

                // Photon Magic Header: 0xF3
                if (payloadLen >= 2 && payload[0] == 0xF3) {
                    uint8_t photonMsgType = payload[1] & 0x7F;

                    if (photonMsgType == 0x00) {
                        // Photon Init message from client -> Respond with InitResponse (0xF3 0x01)
                        Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "Peer %u: Received Photon Init -> Responding InitResponse (0xF3 0x01)", peer->GetPeerId());
                        std::vector<uint8_t> initResp = { 0xF3, 0x01 };
                        uint32_t seq = peer->GetNextOutgoingSequence(cmdHeader.channelId);
                        uint32_t cmdLen = sizeof(ENetCommandHeader) + static_cast<uint32_t>(initResp.size());

                        std::vector<uint8_t> dgram;
                        dgram.resize(sizeof(ENetDatagramHeader) + cmdLen);
                        WriteBE16(dgram.data(), peer->GetPeerId());
                        dgram.data()[2] = 0x00;
                        dgram.data()[3] = 1;
                        WriteBE32(dgram.data() + 4, GetTickCount());
                        WriteBE32(dgram.data() + 8, peer->GetChallenge());

                        uint8_t* cmd = dgram.data() + sizeof(ENetDatagramHeader);
                        cmd[0] = ENetCommandType::SendReliable;
                        cmd[1] = cmdHeader.channelId;
                        cmd[2] = 0x01; // reliable
                        cmd[3] = 0x00;
                        WriteBE32(cmd + 4, cmdLen);
                        WriteBE32(cmd + 8, seq);
                        std::memcpy(cmd + sizeof(ENetCommandHeader), initResp.data(), initResp.size());

                        SendDatagram(peer->GetAddress(), dgram, peer->GetLocalPort());
                    } else if (photonMsgType == static_cast<uint8_t>(Protocol::MessageType::OperationRequest) ||
                               photonMsgType == static_cast<uint8_t>(Protocol::MessageType::InternalOperationRequest)) {
                        std::vector<uint8_t> msgBytes(payload + 1, payload + payloadLen); // Includes msgType + opCode + dictionary
                        std::string fullHex;
                        char hexb[8];
                        for (size_t i = 0; i < msgBytes.size(); ++i) {
                            sprintf_s(hexb, "%02X ", msgBytes[i]);
                            fullHex += hexb;
                        }
                        Diagnostics::LogInfo(Diagnostics::LogChannel::Realtime, "Peer %u: OpReq full bytes (Len: %zu): %s",
                                             peer->GetPeerId(), msgBytes.size(), fullHex.c_str());

                        size_t deserOffset = 0;
                        Protocol::OperationRequest req;
                        if (Protocol::OperationRequest::Deserialize(msgBytes, deserOffset, req)) {
                            HandleOperationRequest(peer, req, cmdHeader.channelId);
                        } else {
                            Diagnostics::LogWarn(Diagnostics::LogChannel::Realtime, "Peer %u: Failed to deserialize OperationRequest (Len: %zu, Offset: %zu, Hex: %s)",
                                                 peer->GetPeerId(), msgBytes.size(), deserOffset, fullHex.c_str());
                        }
                    }
                }
                break;
            }
            case ENetCommandType::SendFragment: {
                if (cmdDataLen >= 20) {
                    uint32_t seq = ReadBE32(cmdData);
                    uint32_t fragCount = ReadBE32(cmdData + 4);
                    uint32_t fragNum = ReadBE32(cmdData + 8);
                    uint32_t totalLen = ReadBE32(cmdData + 12);
                    uint32_t fragOffset = ReadBE32(cmdData + 16);

                    const uint8_t* fragPayload = cmdData + 20;
                    size_t fragPayloadLen = cmdDataLen - 20;

                    std::vector<uint8_t> assembled;
                    if (peer->ProcessFragment(cmdHeader.channelId, seq, fragCount, fragNum, totalLen, fragOffset, fragPayload, fragPayloadLen, assembled)) {
                        if (assembled.size() >= 2 && assembled[0] == 0xF3 &&
                            ((assembled[1] & 0x7F) == static_cast<uint8_t>(Protocol::MessageType::OperationRequest) ||
                             (assembled[1] & 0x7F) == static_cast<uint8_t>(Protocol::MessageType::InternalOperationRequest))) {
                            std::vector<uint8_t> msgBytes(assembled.begin() + 1, assembled.end());
                            size_t deserOffset = 0;
                            Protocol::OperationRequest req;
                            if (Protocol::OperationRequest::Deserialize(msgBytes, deserOffset, req)) {
                                HandleOperationRequest(peer, req, cmdHeader.channelId);
                            }
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    void PhotonServer::HandlePeerDisconnect(std::shared_ptr<PeerConnection> peer, const std::string& reason) {
        if (!peer) return;
        peer->SetState(PeerState::Disconnected);

        std::string roomName = peer->GetCurrentRoomName();
        int32_t actorNr = peer->GetActorNumber();

        if (!roomName.empty() && actorNr > 0) {
            std::shared_ptr<Realtime::RoomState> room;
            {
                std::lock_guard<std::mutex> lock(m_roomsMutex);
                auto it = m_rooms.find(roomName);
                if (it != m_rooms.end()) {
                    room = it->second;
                }
            }

            if (room) {
                int32_t oldMaster = room->GetMasterClientId();
                room->RemoveActor(actorNr);
                int32_t newMaster = room->GetMasterClientId();
                size_t remainingActors = room->GetActorCount();
                uint8_t maxPlayers = room->GetMaxPlayers();

                Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] LEAVE name=%s actor=%d master=%d players=%zu/%u",
                                     roomName.c_str(), actorNr, newMaster, remainingActors, maxPlayers);

                if (oldMaster == actorNr && remainingActors > 0) {
                    Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] MASTER MIGRATION name=%s old_master=%d new_master=%d",
                                         roomName.c_str(), oldMaster, newMaster);
                }

                // Broadcast EventCode::Leave to remaining actors
                Protocol::EventData leaveEvt(Protocol::EventCode::Leave, actorNr);
                leaveEvt.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(actorNr));
                leaveEvt.SetParam(Protocol::ParameterCode::MasterClientId, Protocol::PhotonValue(newMaster));
                BroadcastEventToRoom(roomName, leaveEvt, actorNr, Protocol::ReceiverGroup::Others, 0);

                if (remainingActors == 0) {
                    std::lock_guard<std::mutex> lock(m_roomsMutex);
                    m_rooms.erase(roomName);
                    Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] DESTROY name=%s", roomName.c_str());
                    BroadcastRoomListUpdate(roomName, true);
                } else {
                    BroadcastRoomListUpdate(roomName, false);
                }
            }

            peer->SetCurrentRoomName("");
            peer->SetActorNumber(0);
        }
    }

    void PhotonServer::BroadcastRoomListUpdate(const std::string& roomName, bool removed) {
        std::vector<std::shared_ptr<PeerConnection>> lobbyPeers;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            for (const auto& [k, p] : m_peersByAddr) {
                if (p->GetState() == PeerState::InLobby) {
                    lobbyPeers.push_back(p);
                }
            }
        }

        if (lobbyPeers.empty()) return;

        Protocol::PhotonHashtable gamesTable;
        if (removed) {
            Protocol::PhotonHashtable roomProps;
            roomProps[Protocol::PhotonValue(static_cast<uint8_t>(Protocol::GamePropertyKey::Removed))] = Protocol::PhotonValue(true);
            gamesTable[Protocol::PhotonValue(roomName)] = Protocol::PhotonValue(roomProps);
        } else {
            auto room = GetRoom(roomName);
            if (room && room->IsVisible()) {
                gamesTable[Protocol::PhotonValue(roomName)] = Protocol::PhotonValue(room->GetLobbyProperties());
            } else {
                Protocol::PhotonHashtable roomProps;
                roomProps[Protocol::PhotonValue(static_cast<uint8_t>(Protocol::GamePropertyKey::Removed))] = Protocol::PhotonValue(true);
                gamesTable[Protocol::PhotonValue(roomName)] = Protocol::PhotonValue(roomProps);
            }
        }

        Protocol::EventData evt(Protocol::EventCode::GameListUpdate);
        evt.SetParam(Protocol::ParameterCode::GameList, Protocol::PhotonValue(gamesTable));

        for (auto& p : lobbyPeers) {
            SendEventData(p, evt, 0, true);
        }
    }

    // =========================================================================
    // PHOTON OPERATIONS ROUTING
    // =========================================================================
    void PhotonServer::HandleOperationRequest(std::shared_ptr<PeerConnection> peer, const Protocol::OperationRequest& req, uint8_t channelId) {
        Diagnostics::LogInfo(Diagnostics::LogChannel::Realtime, "Peer %u: OperationRequest (OpCode: %u)",
                             peer->GetPeerId(), req.opCode);

        switch (req.opCode) {
            case 0: { // Internal OpCode 0 (Init / Handshake / InitEncryption)
                Diagnostics::LogInfo(Diagnostics::LogChannel::Auth, "Peer %u: Internal OpCode 0 InitEncryption Handshake", peer->GetPeerId());
                Protocol::OperationResponse resp(0, Protocol::ErrorCode::Ok);
                std::vector<uint8_t> clientKey;
                if (req.HasParam(1)) {
                    clientKey = req.GetParam(1).AsByteArray();
                }
                if (clientKey.empty()) {
                    clientKey.resize(96, 0x02);
                }
                resp.SetParam(1, Protocol::PhotonValue(clientKey)); // ServerKey
                resp.SetParam(Protocol::ParameterCode::Address, Protocol::PhotonValue("127.0.0.1:" + std::to_string(m_port)));
                SendOperationResponse(peer, resp, channelId);
                break;
            }
            case Protocol::OpCode::GetRegions: {
                std::string appId = req.GetParam(Protocol::ParameterCode::AppId).AsString();
                std::string appVersion = req.GetParam(Protocol::ParameterCode::AppVersion).AsString();

                Diagnostics::LogInfo(Diagnostics::LogChannel::Auth, "[Region] GET_REGIONS REQUEST peer=%u (AppId: %s, Version: %s)",
                                     peer->GetPeerId(), appId.c_str(), appVersion.c_str());

                // Build string array of available regions
                Protocol::PhotonArray regionArr;
                regionArr.push_back(Protocol::PhotonValue("sa"));
                regionArr.push_back(Protocol::PhotonValue("us"));
                regionArr.push_back(Protocol::PhotonValue("eu"));
                regionArr.push_back(Protocol::PhotonValue("asia"));
                Protocol::PhotonValue regionVal(regionArr);
                regionVal.type = Protocol::GpType::StringArray;

                // Build string array of Master Server endpoints for each region
                std::string masterEndpoint = "127.0.0.1:" + std::to_string(m_port);
                Protocol::PhotonArray addressArr;
                addressArr.push_back(Protocol::PhotonValue(masterEndpoint));
                addressArr.push_back(Protocol::PhotonValue(masterEndpoint));
                addressArr.push_back(Protocol::PhotonValue(masterEndpoint));
                addressArr.push_back(Protocol::PhotonValue(masterEndpoint));
                Protocol::PhotonValue addressVal(addressArr);
                addressVal.type = Protocol::GpType::StringArray;

                Protocol::OperationResponse resp(Protocol::OpCode::GetRegions, Protocol::ErrorCode::Ok);
                resp.SetParam(Protocol::ParameterCode::Region, regionVal);
                resp.SetParam(Protocol::ParameterCode::Address, addressVal);

                SendOperationResponse(peer, resp, channelId);

                Diagnostics::LogInfo(Diagnostics::LogChannel::Auth, "[Region] GET_REGIONS RESPONSE peer=%u regions=[sa, us, eu, asia] master=%s",
                                     peer->GetPeerId(), masterEndpoint.c_str());
                break;
            }
            case Protocol::OpCode::Authenticate:
            case Protocol::OpCode::AuthenticateOnce: {
                std::string appId = req.GetParam(Protocol::ParameterCode::AppId).AsString();
                std::string appVersion = req.GetParam(Protocol::ParameterCode::AppVersion).AsString();
                std::string userId = req.GetParam(Protocol::ParameterCode::UserId).AsString();
                
                bool hasExplicitRegion = req.HasParam(Protocol::ParameterCode::Region) || req.HasParam(Protocol::ParameterCode::Cluster);
                std::string region = req.GetParam(Protocol::ParameterCode::Region).AsString();
                if (region.empty() && req.HasParam(Protocol::ParameterCode::Cluster)) {
                    region = req.GetParam(Protocol::ParameterCode::Cluster).AsString();
                }

                if (userId.empty()) userId = "Player_" + std::to_string(peer->GetPeerId());

                peer->SetAppId(appId);
                peer->SetAppVersion(appVersion);
                peer->SetUserId(userId);
                peer->SetRegion(region);
                peer->SetState(PeerState::Authenticated);

                if (hasExplicitRegion && !region.empty()) {
                    Diagnostics::LogInfo(Diagnostics::LogChannel::Auth, "[Auth] Peer %u Authenticated (User: '%s', Version: '%s', Selected Region: '%s', AppId: %s)",
                                         peer->GetPeerId(), userId.c_str(), appVersion.c_str(), region.c_str(), appId.c_str());
                } else {
                    Diagnostics::LogInfo(Diagnostics::LogChannel::Auth, "[Auth] Peer %u Authenticated - Region Discovery Mode (User: '%s', Version: '%s', AppId: %s)",
                                         peer->GetPeerId(), userId.c_str(), appVersion.c_str(), appId.c_str());
                }

                // Build string array of available regions for RegionHandler (Photon C# SDK expects string[])
                Protocol::PhotonArray regionArr;
                regionArr.push_back(Protocol::PhotonValue("sa"));
                regionArr.push_back(Protocol::PhotonValue("us"));
                regionArr.push_back(Protocol::PhotonValue("eu"));
                regionArr.push_back(Protocol::PhotonValue("asia"));
                Protocol::PhotonValue regionVal(regionArr);
                regionVal.type = Protocol::GpType::StringArray;

                // Build string array of Master Server endpoints for each region
                std::string masterEndpoint = "127.0.0.1:" + std::to_string(m_port);
                Protocol::PhotonArray addressArr;
                addressArr.push_back(Protocol::PhotonValue(masterEndpoint));
                addressArr.push_back(Protocol::PhotonValue(masterEndpoint));
                addressArr.push_back(Protocol::PhotonValue(masterEndpoint));
                addressArr.push_back(Protocol::PhotonValue(masterEndpoint));
                Protocol::PhotonValue addressVal(addressArr);
                addressVal.type = Protocol::GpType::StringArray;

                Protocol::OperationResponse resp(req.opCode, Protocol::ErrorCode::Ok);
                resp.SetParam(Protocol::ParameterCode::UserId, Protocol::PhotonValue(userId));
                resp.SetParam(Protocol::ParameterCode::Region, regionVal);
                resp.SetParam(Protocol::ParameterCode::Address, addressVal);

                if (hasExplicitRegion && !region.empty()) {
                    resp.SetParam(Protocol::ParameterCode::Cluster, Protocol::PhotonValue(region));
                }

                if (req.HasParam(Protocol::ParameterCode::CustomAuthenticationData)) {
                    resp.SetParam(Protocol::ParameterCode::CustomAuthenticationData, req.GetParam(Protocol::ParameterCode::CustomAuthenticationData));
                }

                SendOperationResponse(peer, resp, channelId);
                break;
            }
            case Protocol::OpCode::JoinLobby: {
                peer->SetState(PeerState::InLobby);
                Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] LIST REQUEST peer=%u", peer->GetPeerId());

                Protocol::OperationResponse resp(Protocol::OpCode::JoinLobby, Protocol::ErrorCode::Ok);
                SendOperationResponse(peer, resp, channelId);

                // Send initial GameList event (230) containing all visible rooms
                Protocol::PhotonHashtable gamesTable;
                {
                    std::lock_guard<std::mutex> lock(m_roomsMutex);
                    for (const auto& [name, r] : m_rooms) {
                        if (r->IsVisible()) {
                            gamesTable[Protocol::PhotonValue(name)] = Protocol::PhotonValue(r->GetLobbyProperties());
                        }
                    }
                }

                Protocol::EventData gameListEvt(Protocol::EventCode::GameList);
                gameListEvt.SetParam(Protocol::ParameterCode::GameList, Protocol::PhotonValue(gamesTable));
                SendEventData(peer, gameListEvt, channelId, true);

                Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] LIST RESPONSE peer=%u count=%zu",
                                     peer->GetPeerId(), gamesTable.size());
                break;
            }
            case Protocol::OpCode::LeaveLobby: {
                peer->SetState(PeerState::Authenticated);
                Protocol::OperationResponse resp(Protocol::OpCode::LeaveLobby, Protocol::ErrorCode::Ok);
                SendOperationResponse(peer, resp, channelId);
                break;
            }
            case Protocol::OpCode::GetGameList: {
                Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] LIST REQUEST (OpCode 217) peer=%u", peer->GetPeerId());
                Protocol::PhotonHashtable gamesTable;
                {
                    std::lock_guard<std::mutex> lock(m_roomsMutex);
                    for (const auto& [name, r] : m_rooms) {
                        if (r->IsVisible()) {
                            gamesTable[Protocol::PhotonValue(name)] = Protocol::PhotonValue(r->GetLobbyProperties());
                        }
                    }
                }
                Protocol::OperationResponse resp(Protocol::OpCode::GetGameList, Protocol::ErrorCode::Ok);
                resp.SetParam(Protocol::ParameterCode::GameList, Protocol::PhotonValue(gamesTable));
                SendOperationResponse(peer, resp, channelId);

                Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] LIST RESPONSE (OpCode 217) peer=%u count=%zu",
                                     peer->GetPeerId(), gamesTable.size());
                break;
            }
            case Protocol::OpCode::CreateGame: {
                std::string roomName = req.GetParam(Protocol::ParameterCode::GameId).AsString();
                if (roomName.empty()) roomName = "Room_" + std::to_string(GetTickCount());

                RoomOptions opts;
                opts.maxPlayers = req.GetParam(Protocol::ParameterCode::MaxPlayers).AsByte(4);
                opts.isOpen = req.GetParam(Protocol::ParameterCode::IsOpen).AsBool(true);
                opts.isVisible = req.GetParam(Protocol::ParameterCode::IsVisible).AsBool(true);

                if (req.HasParam(Protocol::ParameterCode::GameProperties)) {
                    opts.customRoomProperties = req.GetParam(Protocol::ParameterCode::GameProperties).AsHashtable();
                }

                std::shared_ptr<Realtime::RoomState> room;
                {
                    std::lock_guard<std::mutex> lock(m_roomsMutex);
                    if (m_rooms.find(roomName) != m_rooms.end()) {
                        Diagnostics::LogWarn(Diagnostics::LogChannel::Room, "Peer %u attempted to create already existing room '%s'",
                                             peer->GetPeerId(), roomName.c_str());
                        Protocol::OperationResponse resp(Protocol::OpCode::CreateGame, Protocol::ErrorCode::GameIdAlreadyExists, "A game with the requested identifier already exists.");
                        resp.SetParam(Protocol::ParameterCode::GameId, Protocol::PhotonValue(roomName));
                        SendOperationResponse(peer, resp, channelId);
                        return;
                    }
                    room = std::make_shared<Realtime::RoomState>(roomName, opts);
                    m_rooms[roomName] = room;
                }

                int32_t actorNr = room->AddActor(peer->GetUserId(), peer->GetUserId());
                peer->SetCurrentRoomName(roomName);
                peer->SetActorNumber(actorNr);
                peer->SetState(PeerState::InRoom);

                Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] CREATE name=%s actor=%d master=%d players=%zu/%u",
                                     roomName.c_str(), actorNr, room->GetMasterClientId(), room->GetActorCount(), opts.maxPlayers);

                Protocol::OperationResponse resp(Protocol::OpCode::CreateGame, Protocol::ErrorCode::Ok);
                resp.SetParam(Protocol::ParameterCode::GameId, Protocol::PhotonValue(roomName));
                resp.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(static_cast<int32_t>(actorNr)));
                resp.SetParam(Protocol::ParameterCode::Address, Protocol::PhotonValue("127.0.0.1:" + std::to_string(m_port)));

                Protocol::PhotonArray actorList;
                actorList.push_back(Protocol::PhotonValue(static_cast<int32_t>(actorNr)));
                Protocol::PhotonValue actorListVal(actorList);
                actorListVal.type = Protocol::GpType::Array;
                resp.SetParam(Protocol::ParameterCode::ActorList, actorListVal);
                resp.SetParam(Protocol::ParameterCode::MasterClientId, Protocol::PhotonValue(static_cast<int32_t>(actorNr)));
                resp.SetParam(Protocol::ParameterCode::GameProperties, Protocol::PhotonValue(room->GetCustomProperties()));

                Protocol::PhotonHashtable actorTable;
                actorTable[Protocol::PhotonValue(static_cast<int32_t>(actorNr))] = Protocol::PhotonValue(room->GetActorProperties(actorNr));
                resp.SetParam(Protocol::ParameterCode::PlayerProperties, Protocol::PhotonValue(actorTable));

                SendOperationResponse(peer, resp, channelId);

                // Send Join Event (255) to creator so PUN 2 transitions to ClientState.Joined
                Protocol::EventData joinEvt(Protocol::EventCode::Join, static_cast<int32_t>(actorNr));
                joinEvt.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(static_cast<int32_t>(actorNr)));
                joinEvt.SetParam(Protocol::ParameterCode::ActorList, actorListVal);
                joinEvt.SetParam(Protocol::ParameterCode::MasterClientId, Protocol::PhotonValue(static_cast<int32_t>(actorNr)));
                joinEvt.SetParam(Protocol::ParameterCode::PlayerProperties, Protocol::PhotonValue(room->GetActorProperties(actorNr)));
                SendEventData(peer, joinEvt, channelId);

                if (opts.isVisible) {
                    BroadcastRoomListUpdate(roomName, false);
                }
                break;
            }
            case Protocol::OpCode::JoinGame: {
                std::string roomName = req.GetParam(Protocol::ParameterCode::GameId).AsString();
                std::shared_ptr<Realtime::RoomState> room = GetRoom(roomName);

                if (!room) {
                    Diagnostics::LogWarn(Diagnostics::LogChannel::Room, "Peer %u failed to join room '%s': Not Found", peer->GetPeerId(), roomName.c_str());
                    Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::GameDoesNotExist, "Game does not exist");
                    SendOperationResponse(peer, resp, channelId);
                    return;
                }

                if (!room->IsOpen()) {
                    Diagnostics::LogWarn(Diagnostics::LogChannel::Room, "Peer %u failed to join room '%s': Room is closed", peer->GetPeerId(), roomName.c_str());
                    Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::GameClosed, "Game is closed");
                    SendOperationResponse(peer, resp, channelId);
                    return;
                }

                if (room->GetActorCount() >= room->GetMaxPlayers()) {
                    Diagnostics::LogWarn(Diagnostics::LogChannel::Room, "Peer %u failed to join room '%s': Full (%zu/%u)",
                                         peer->GetPeerId(), roomName.c_str(), room->GetActorCount(), room->GetMaxPlayers());
                    Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::GameFull, "Game is full");
                    SendOperationResponse(peer, resp, channelId);
                    return;
                }

                int32_t actorNr = room->AddActor(peer->GetUserId(), peer->GetUserId());
                if (actorNr == 0) {
                    Diagnostics::LogWarn(Diagnostics::LogChannel::Room, "Peer %u failed to join room '%s': Full or Closed", peer->GetPeerId(), roomName.c_str());
                    Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::GameFull, "Game full");
                    SendOperationResponse(peer, resp, channelId);
                    return;
                }

                peer->SetCurrentRoomName(roomName);
                peer->SetActorNumber(actorNr);
                peer->SetState(PeerState::InRoom);

                Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "[Room] JOIN name=%s actor=%d master=%d players=%zu/%u",
                                     roomName.c_str(), actorNr, room->GetMasterClientId(), room->GetActorCount(), room->GetMaxPlayers());

                // Build JoinGame response
                Protocol::OperationResponse resp(Protocol::OpCode::JoinGame, Protocol::ErrorCode::Ok);
                resp.SetParam(Protocol::ParameterCode::GameId, Protocol::PhotonValue(roomName));
                resp.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(static_cast<int32_t>(actorNr)));
                resp.SetParam(Protocol::ParameterCode::Address, Protocol::PhotonValue("127.0.0.1:" + std::to_string(m_port)));

                // ActorList array
                Protocol::PhotonArray actorList;
                for (int32_t nr : room->GetActorNumbers()) {
                    actorList.push_back(Protocol::PhotonValue(static_cast<int32_t>(nr)));
                }
                Protocol::PhotonValue actorListVal(actorList);
                actorListVal.type = Protocol::GpType::Array;
                resp.SetParam(Protocol::ParameterCode::ActorList, actorListVal);
                resp.SetParam(Protocol::ParameterCode::MasterClientId, Protocol::PhotonValue(static_cast<int32_t>(room->GetMasterClientId())));
                resp.SetParam(Protocol::ParameterCode::GameProperties, Protocol::PhotonValue(room->GetCustomProperties()));

                Protocol::PhotonHashtable actorTable;
                for (int32_t nr : room->GetActorNumbers()) {
                    actorTable[Protocol::PhotonValue(static_cast<int32_t>(nr))] = Protocol::PhotonValue(room->GetActorProperties(nr));
                }
                resp.SetParam(Protocol::ParameterCode::PlayerProperties, Protocol::PhotonValue(actorTable));

                SendOperationResponse(peer, resp, channelId);

                // Broadcast EventCode::Join (255) to ALL actors in the room (including the joiner)
                Protocol::EventData joinEvt(Protocol::EventCode::Join, static_cast<int32_t>(actorNr));
                joinEvt.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(static_cast<int32_t>(actorNr)));
                joinEvt.SetParam(Protocol::ParameterCode::ActorList, actorListVal);
                joinEvt.SetParam(Protocol::ParameterCode::MasterClientId, Protocol::PhotonValue(static_cast<int32_t>(room->GetMasterClientId())));
                joinEvt.SetParam(Protocol::ParameterCode::PlayerProperties, Protocol::PhotonValue(room->GetActorProperties(actorNr)));

                BroadcastEventToRoom(roomName, joinEvt, actorNr, Protocol::ReceiverGroup::All, channelId);

                BroadcastRoomListUpdate(roomName, false);
                break;
            }
            case Protocol::OpCode::JoinRandomGame: {
                std::shared_ptr<Realtime::RoomState> targetRoom;
                {
                    std::lock_guard<std::mutex> lock(m_roomsMutex);
                    for (const auto& [k, r] : m_rooms) {
                        if (r->IsOpen() && r->IsVisible() && r->GetActorCount() < r->GetMaxPlayers()) {
                            targetRoom = r;
                            break;
                        }
                    }
                }

                if (!targetRoom) {
                    Diagnostics::LogInfo(Diagnostics::LogChannel::Room, "Peer %u: JoinRandomGame found no open/visible room", peer->GetPeerId());
                    Protocol::OperationResponse resp(Protocol::OpCode::JoinRandomGame, Protocol::ErrorCode::NoRandomMatchFound, "No match found");
                    SendOperationResponse(peer, resp, channelId);
                    return;
                }

                // Delegate to JoinGame logic
                Protocol::OperationRequest joinReq(Protocol::OpCode::JoinGame);
                joinReq.SetParam(Protocol::ParameterCode::GameId, Protocol::PhotonValue(targetRoom->GetName()));
                HandleOperationRequest(peer, joinReq, channelId);
                break;
            }
            case Protocol::OpCode::Leave: {
                HandlePeerDisconnect(peer, "Explicit OpLeave");
                peer->SetState(PeerState::Authenticated);

                Protocol::OperationResponse resp(Protocol::OpCode::Leave, Protocol::ErrorCode::Ok);
                SendOperationResponse(peer, resp, channelId);
                break;
            }
            case Protocol::OpCode::SetProperties: {
                std::string roomName = peer->GetCurrentRoomName();
                std::shared_ptr<Realtime::RoomState> room = GetRoom(roomName);
                if (!room) return;

                if (req.HasParam(Protocol::ParameterCode::GameProperties)) {
                    auto props = req.GetParam(Protocol::ParameterCode::GameProperties).AsHashtable();
                    room->UpdateCustomProperties(props);

                    Protocol::EventData propEvt(Protocol::EventCode::PropertiesChanged, peer->GetActorNumber());
                    propEvt.SetParam(Protocol::ParameterCode::GameProperties, Protocol::PhotonValue(props));
                    BroadcastEventToRoom(roomName, propEvt, peer->GetActorNumber(), Protocol::ReceiverGroup::All, channelId);

                    BroadcastRoomListUpdate(roomName, false);
                }

                if (req.HasParam(Protocol::ParameterCode::ActorProperties)) {
                    int32_t targetActor = req.GetParam(Protocol::ParameterCode::ActorNr).AsInt(peer->GetActorNumber());
                    auto props = req.GetParam(Protocol::ParameterCode::ActorProperties).AsHashtable();
                    room->UpdateActorProperties(targetActor, props);

                    Protocol::EventData propEvt(Protocol::EventCode::PropertiesChanged, peer->GetActorNumber());
                    propEvt.SetParam(Protocol::ParameterCode::ActorProperties, Protocol::PhotonValue(props));
                    propEvt.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(targetActor));
                    BroadcastEventToRoom(roomName, propEvt, peer->GetActorNumber(), Protocol::ReceiverGroup::All, channelId);
                }

                Protocol::OperationResponse resp(Protocol::OpCode::SetProperties, Protocol::ErrorCode::Ok);
                SendOperationResponse(peer, resp, channelId);
                break;
            }
            case Protocol::OpCode::GetProperties: {
                std::string roomName = peer->GetCurrentRoomName();
                std::shared_ptr<Realtime::RoomState> room = GetRoom(roomName);

                Protocol::OperationResponse resp(Protocol::OpCode::GetProperties, Protocol::ErrorCode::Ok);
                if (room) {
                    resp.SetParam(Protocol::ParameterCode::GameProperties, Protocol::PhotonValue(room->GetCustomProperties()));
                }
                SendOperationResponse(peer, resp, channelId);
                break;
            }
            case Protocol::OpCode::RaiseEvent: {
                std::string roomName = peer->GetCurrentRoomName();
                if (roomName.empty()) return;

                uint8_t eventCode = req.GetParam(Protocol::ParameterCode::Code).AsByte(0);
                Protocol::PhotonValue data = req.GetParam(Protocol::ParameterCode::Data);
                uint8_t receiverGroup = req.GetParam(Protocol::ParameterCode::ReceiverGroup).AsByte(Protocol::ReceiverGroup::Others);

                Diagnostics::LogDebug(Diagnostics::LogChannel::Realtime, "Peer %u: RaiseEvent Code %u in Room '%s' (Group: %u)",
                                      peer->GetPeerId(), eventCode, roomName.c_str(), receiverGroup);

                Protocol::EventData evt(eventCode, peer->GetActorNumber());
                evt.SetParam(Protocol::ParameterCode::ActorNr, Protocol::PhotonValue(static_cast<int32_t>(peer->GetActorNumber())));
                evt.SetParam(Protocol::ParameterCode::Data, data);
                evt.SetParam(Protocol::ParameterCode::Code, Protocol::PhotonValue(eventCode));

                BroadcastEventToRoom(roomName, evt, peer->GetActorNumber(), receiverGroup, channelId);
                break;
            }
            case Protocol::OpCode::ChangeGroups: {
                Protocol::OperationResponse resp(Protocol::OpCode::ChangeGroups, Protocol::ErrorCode::Ok);
                SendOperationResponse(peer, resp, channelId);
                break;
            }
            default: {
                // Structured UNSUPPORTED_OPERATION diagnostic
                Diagnostics::LogWarn(Diagnostics::LogChannel::Realtime,
                                     "UNSUPPORTED_OPERATION: OpCode=%u, State=%s, Peer=%u, ParamCount=%zu",
                                     req.opCode, PeerStateToString(peer->GetState()), peer->GetPeerId(), req.parameters.size());

                Protocol::OperationResponse resp(req.opCode, Protocol::ErrorCode::Ok);
                SendOperationResponse(peer, resp, channelId);
                break;
            }
        }
    }

    std::shared_ptr<Realtime::RoomState> PhotonServer::GetRoom(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        auto it = m_rooms.find(name);
        return (it != m_rooms.end()) ? it->second : nullptr;
    }

    std::vector<std::shared_ptr<Realtime::RoomState>> PhotonServer::GetAllRooms() {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        std::vector<std::shared_ptr<Realtime::RoomState>> res;
        for (const auto& [k, r] : m_rooms) res.push_back(r);
        return res;
    }

    void PhotonServer::BroadcastEventToRoom(const std::string& roomName, const Protocol::EventData& evt,
                                            int32_t senderActorNr, uint8_t receiverGroup, uint8_t channelId) {
        auto room = GetRoom(roomName);
        int32_t masterClientId = room ? room->GetMasterClientId() : 1;

        std::vector<std::shared_ptr<PeerConnection>> targets;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            for (const auto& [k, p] : m_peersByAddr) {
                if (p->GetCurrentRoomName() == roomName) {
                    if (receiverGroup == Protocol::ReceiverGroup::Others && p->GetActorNumber() == senderActorNr) {
                        continue;
                    }
                    if (receiverGroup == Protocol::ReceiverGroup::MasterClient && p->GetActorNumber() != masterClientId) {
                        continue;
                    }
                    targets.push_back(p);
                }
            }
        }

        for (auto& peer : targets) {
            SendEventData(peer, evt, channelId, true);
        }
    }

    // =========================================================================
    // OUTGOING PACKET BUILDERS
    // =========================================================================
    void PhotonServer::SendDatagram(const sockaddr_in& toAddr, const std::vector<uint8_t>& datagram, uint16_t localPort) {
        std::lock_guard<std::mutex> lock(m_socketMutex);
        SOCKET targetSocket = m_socket;
        if (localPort == m_nameServerPort && m_nameServerSocket != INVALID_SOCKET) {
            targetSocket = m_nameServerSocket;
        }
        if (targetSocket == INVALID_SOCKET || datagram.empty()) return;

        sendto(targetSocket, reinterpret_cast<const char*>(datagram.data()), static_cast<int>(datagram.size()),
               0, reinterpret_cast<const sockaddr*>(&toAddr), sizeof(toAddr));

        Diagnostics::DiagnosticsEngine::Instance().RecordPacketSent(datagram.size());
    }

    void PhotonServer::SendVerifyConnect(const sockaddr_in& toAddr, uint16_t assignedPeerId, uint32_t challenge, uint16_t localPort) {
        std::vector<uint8_t> dgram;
        dgram.resize(sizeof(ENetDatagramHeader) + sizeof(ENetCommandHeader) + 32);

        // Datagram Header (12 bytes)
        WriteBE16(dgram.data(), 0xFFFF);
        dgram.data()[2] = 0x00; // flags
        dgram.data()[3] = 1;    // commandCount
        WriteBE32(dgram.data() + 4, GetTickCount());
        WriteBE32(dgram.data() + 8, challenge);

        // Command Header (12 bytes)
        uint8_t* cmd = dgram.data() + sizeof(ENetDatagramHeader);
        cmd[0] = ENetCommandType::VerifyConnect;
        cmd[1] = 255;  // channelId
        cmd[2] = 0x00; // commandFlags
        cmd[3] = 0x00; // reserved
        WriteBE32(cmd + 4, 44); // commandLength (12 + 32)
        WriteBE32(cmd + 8, 0);  // reliableSeq

        // Connect Payload (32 bytes)
        uint8_t* payload = cmd + sizeof(ENetCommandHeader);
        std::memset(payload, 0, 32);
        WriteBE16(payload, assignedPeerId);
        WriteBE16(payload + 2, 1200);   // mtu
        WriteBE32(payload + 4, 256);    // windowSize
        WriteBE32(payload + 8, 2);      // channelCount

        SendDatagram(toAddr, dgram, localPort);
    }

    void PhotonServer::SendAck(std::shared_ptr<PeerConnection> peer, uint8_t channelId, uint32_t seq, uint32_t sentTime) {
        std::vector<uint8_t> dgram;
        dgram.resize(sizeof(ENetDatagramHeader) + sizeof(ENetCommandHeader) + sizeof(ENetAckPayload));

        WriteBE16(dgram.data(), peer->GetPeerId());
        dgram.data()[2] = 0x00;
        dgram.data()[3] = 1;
        WriteBE32(dgram.data() + 4, GetTickCount());
        WriteBE32(dgram.data() + 8, peer->GetChallenge());

        uint8_t* cmd = dgram.data() + sizeof(ENetDatagramHeader);
        cmd[0] = ENetCommandType::Acknowledge;
        cmd[1] = channelId;
        cmd[2] = 0x00;
        cmd[3] = 0x00;
        WriteBE32(cmd + 4, sizeof(ENetCommandHeader) + sizeof(ENetAckPayload));
        WriteBE32(cmd + 8, 0);

        uint8_t* payload = cmd + sizeof(ENetCommandHeader);
        WriteBE32(payload, seq);
        WriteBE32(payload + 4, sentTime);

        SendDatagram(peer->GetAddress(), dgram, peer->GetLocalPort());
    }

    void PhotonServer::SendOperationResponse(std::shared_ptr<PeerConnection> peer, const Protocol::OperationResponse& resp, uint8_t channelId) {
        std::vector<uint8_t> respPayload = resp.Serialize(); // [0x03][opCode][returnCode]...

        // Add Photon Magic Header 0xF3
        std::vector<uint8_t> photonMsg;
        photonMsg.push_back(0xF3);
        photonMsg.insert(photonMsg.end(), respPayload.begin(), respPayload.end());

        uint32_t seq = peer->GetNextOutgoingSequence(channelId);
        uint32_t cmdLen = sizeof(ENetCommandHeader) + static_cast<uint32_t>(photonMsg.size());

        std::vector<uint8_t> dgram;
        dgram.resize(sizeof(ENetDatagramHeader) + cmdLen);

        WriteBE16(dgram.data(), peer->GetPeerId());
        dgram.data()[2] = 0x00;
        dgram.data()[3] = 1;
        WriteBE32(dgram.data() + 4, GetTickCount());
        WriteBE32(dgram.data() + 8, peer->GetChallenge());

        uint8_t* cmd = dgram.data() + sizeof(ENetDatagramHeader);
        cmd[0] = ENetCommandType::SendReliable;
        cmd[1] = channelId;
        cmd[2] = 0x01; // reliable
        cmd[3] = 0x00;
        WriteBE32(cmd + 4, cmdLen);
        WriteBE32(cmd + 8, seq);

        std::memcpy(cmd + sizeof(ENetCommandHeader), photonMsg.data(), photonMsg.size());

        SendDatagram(peer->GetAddress(), dgram, peer->GetLocalPort());
    }

    void PhotonServer::SendEventData(std::shared_ptr<PeerConnection> peer, const Protocol::EventData& evt, uint8_t channelId, bool reliable) {
        std::vector<uint8_t> evtPayload = evt.Serialize(); // [0x04][code][senderActorNr]...

        std::vector<uint8_t> photonMsg;
        photonMsg.push_back(0xF3);
        photonMsg.insert(photonMsg.end(), evtPayload.begin(), evtPayload.end());

        uint32_t seq = peer->GetNextOutgoingSequence(channelId);
        uint32_t cmdLen = sizeof(ENetCommandHeader) + static_cast<uint32_t>(photonMsg.size());

        std::vector<uint8_t> dgram;
        dgram.resize(sizeof(ENetDatagramHeader) + cmdLen);

        WriteBE16(dgram.data(), peer->GetPeerId());
        dgram.data()[2] = 0x00;
        dgram.data()[3] = 1;
        WriteBE32(dgram.data() + 4, GetTickCount());
        WriteBE32(dgram.data() + 8, peer->GetChallenge());

        uint8_t* cmd = dgram.data() + sizeof(ENetDatagramHeader);
        cmd[0] = reliable ? ENetCommandType::SendReliable : ENetCommandType::SendUnreliable;
        cmd[1] = channelId;
        cmd[2] = reliable ? 0x01 : 0x00;
        cmd[3] = 0x00;
        WriteBE32(cmd + 4, cmdLen);
        WriteBE32(cmd + 8, seq);

        std::memcpy(cmd + sizeof(ENetCommandHeader), photonMsg.data(), photonMsg.size());

        SendDatagram(peer->GetAddress(), dgram, peer->GetLocalPort());
    }

    void PhotonServer::CleanupStaleConnections() {
        DWORD now = GetTickCount();
        std::vector<std::shared_ptr<PeerConnection>> timedOutPeers;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            for (auto it = m_peersByAddr.begin(); it != m_peersByAddr.end(); ) {
                if (now - it->second->GetLastActivityTime() > 30000) { // 30s timeout
                    timedOutPeers.push_back(it->second);
                    m_peersById.erase(it->second->GetPeerId());
                    it = m_peersByAddr.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (auto& p : timedOutPeers) {
            Diagnostics::LogInfo(Diagnostics::LogChannel::Transport, "Peer %u timed out (inactive > 30s)", p->GetPeerId());
            HandlePeerDisconnect(p, "30s timeout");
        }
    }

} // namespace ReFix::Photon::Server
