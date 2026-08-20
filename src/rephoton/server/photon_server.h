#pragma once

#include "enet_protocol.h"
#include "peer_connection.h"
#include "../realtime/room_state.h"
#include "../protocol/photon_message.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>
#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

namespace ReFix::Photon::Server {

    class PhotonServer {
    public:
        PhotonServer();
        ~PhotonServer();

        bool Start(uint16_t port = 5055);
        void Stop();
        bool IsRunning() const { return m_running; }

        void Update(); // Main tick / polling routine

        // Room Management
        std::shared_ptr<Realtime::RoomState> GetRoom(const std::string& name);
        std::vector<std::shared_ptr<Realtime::RoomState>> GetAllRooms();

    private:
        void ProcessIncomingDatagram(const uint8_t* data, size_t len, const sockaddr_in& fromAddr);
        void ProcessCommand(std::shared_ptr<PeerConnection> peer, const ENetCommandHeader& cmdHeader,
                            const uint8_t* cmdData, size_t cmdDataLen, uint32_t sentTimestamp);

        void HandleOperationRequest(std::shared_ptr<PeerConnection> peer, const Protocol::OperationRequest& req, uint8_t channelId);

        // Outgoing packet helpers
        void SendDatagram(const sockaddr_in& toAddr, const std::vector<uint8_t>& datagram);
        void SendAck(std::shared_ptr<PeerConnection> peer, uint8_t channelId, uint32_t seq, uint32_t sentTime);
        void SendVerifyConnect(const sockaddr_in& toAddr, uint16_t assignedPeerId, uint32_t challenge);
        void SendOperationResponse(std::shared_ptr<PeerConnection> peer, const Protocol::OperationResponse& resp, uint8_t channelId = 0);
        void SendEventData(std::shared_ptr<PeerConnection> peer, const Protocol::EventData& evt, uint8_t channelId = 0, bool reliable = true);
        void BroadcastEventToRoom(const std::string& roomName, const Protocol::EventData& evt, int32_t senderActorNr,
                                 uint8_t receiverGroup = Protocol::ReceiverGroup::Others, uint8_t channelId = 0);

        std::shared_ptr<PeerConnection> GetOrCreatePeer(const sockaddr_in& addr, uint16_t peerId);
        std::shared_ptr<PeerConnection> FindPeerByAddress(const sockaddr_in& addr);
        std::string AddrKey(const sockaddr_in& addr);

        void CleanupStaleConnections();

        SOCKET m_socket = INVALID_SOCKET;
        uint16_t m_port = 5055;
        std::atomic<bool> m_running{ false };
        uint16_t m_nextPeerId = 1;

        std::map<std::string, std::shared_ptr<PeerConnection>> m_peersByAddr;
        std::map<uint16_t, std::shared_ptr<PeerConnection>> m_peersById;
        std::map<std::string, std::shared_ptr<Realtime::RoomState>> m_rooms;

        mutable std::mutex m_peersMutex;
        mutable std::mutex m_roomsMutex;
        mutable std::mutex m_socketMutex;
    };

} // namespace ReFix::Photon::Server
