#pragma once

#include "enet_protocol.h"
#include "../protocol/photon_message.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <memory>

namespace ReFix::Photon::Server {

    enum class PeerState {
        Connecting = 0,
        Connected,
        Authenticated,
        InLobby,
        InRoom,
        Disconnected
    };

    inline const char* PeerStateToString(PeerState s) {
        switch (s) {
            case PeerState::Connecting:    return "Connecting";
            case PeerState::Connected:     return "Connected";
            case PeerState::Authenticated: return "Authenticated";
            case PeerState::InLobby:       return "InLobby";
            case PeerState::InRoom:        return "InRoom";
            case PeerState::Disconnected:  return "Disconnected";
            default:                       return "Unknown";
        }
    }

    struct FragmentInfo {
        uint32_t sequenceNumber = 0;
        uint32_t fragmentCount = 0;
        uint32_t totalLength = 0;
        std::map<uint32_t, std::vector<uint8_t>> fragments;
    };

    class PeerConnection {
    public:
        PeerConnection(uint16_t peerId, const sockaddr_in& addr);
        ~PeerConnection() = default;

        uint16_t GetPeerId() const { return m_peerId; }
        sockaddr_in GetAddress() const { return m_address; }
        std::string GetAddressString() const;

        PeerState GetState() const { return m_state; }
        void SetState(PeerState s) { m_state = s; }

        std::string GetUserId() const { return m_userId; }
        void SetUserId(const std::string& id) { m_userId = id; }

        std::string GetAppId() const { return m_appId; }
        void SetAppId(const std::string& id) { m_appId = id; }

        std::string GetAppVersion() const { return m_appVersion; }
        void SetAppVersion(const std::string& v) { m_appVersion = v; }

        std::string GetRegion() const { return m_region; }
        void SetRegion(const std::string& r) { m_region = r; }

        std::string GetCurrentRoomName() const { return m_currentRoomName; }
        void SetCurrentRoomName(const std::string& r) { m_currentRoomName = r; }

        int32_t GetActorNumber() const { return m_actorNr; }
        void SetActorNumber(int32_t nr) { m_actorNr = nr; }

        uint32_t GetChallenge() const { return m_challenge; }
        void SetChallenge(uint32_t c) { m_challenge = c; }

        uint32_t GetNextOutgoingSequence(uint8_t channelId);
        uint32_t GetLastIncomingSequence(uint8_t channelId);
        void SetLastIncomingSequence(uint8_t channelId, uint32_t seq);

        // Fragment Assembly
        bool ProcessFragment(uint8_t channelId, uint32_t seq, uint32_t fragCount, uint32_t fragNum,
                             uint32_t totalLen, uint32_t fragOffset, const uint8_t* fragData, size_t fragLen,
                             std::vector<uint8_t>& outAssembledPayload);

        DWORD GetLastActivityTime() const { return m_lastActivity; }
        void UpdateActivity() { m_lastActivity = GetTickCount(); }

        uint16_t GetLocalPort() const { return m_localPort; }
        void SetLocalPort(uint16_t p) { m_localPort = p; }

    private:
        uint16_t m_peerId;
        sockaddr_in m_address;
        PeerState m_state = PeerState::Connecting;

        std::string m_userId;
        std::string m_appId;
        std::string m_appVersion;
        std::string m_region = "sa";
        std::string m_currentRoomName;
        int32_t m_actorNr = 0;
        uint32_t m_challenge = 0;
        uint16_t m_localPort = 5055;

        std::map<uint8_t, uint32_t> m_outgoingSeq;
        std::map<uint8_t, uint32_t> m_incomingSeq;
        std::map<uint8_t, FragmentInfo> m_fragmentAssembly;

        DWORD m_lastActivity = 0;
        mutable std::mutex m_mutex;
    };

} // namespace ReFix::Photon::Server
