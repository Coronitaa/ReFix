#pragma once

#include "photon_types.h"
#include "../protocol/photon_message.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ReFix::Photon {

    // Forward declarations
    struct RoomInfo;
    struct PlayerInfo;

    // Transport Interface
    class IPhotonTransport {
    public:
        virtual ~IPhotonTransport() = default;

        virtual bool Connect(const std::string& host, uint16_t port) = 0;
        virtual void Disconnect() = 0;
        virtual bool IsConnected() const = 0;

        virtual bool SendPacket(const std::vector<uint8_t>& data, bool reliable = true) = 0;
        virtual bool PollPacket(std::vector<uint8_t>& outData) = 0;

        virtual PhotonTransportType GetType() const = 0;
        virtual NetworkMetrics GetMetrics() const = 0;
    };

    // Region Discovery Interface
    struct RegionInfo {
        PhotonRegion region = PhotonRegion::Unknown;
        std::string code;
        std::string name;
        std::string endpoint;
        uint16_t port = 5055;
        uint32_t pingMs = 0;
        bool isAvailable = true;
    };

    class IPhotonRegionProvider {
    public:
        virtual ~IPhotonRegionProvider() = default;

        virtual std::vector<RegionInfo> GetAvailableRegions() = 0;
        virtual bool SelectRegion(const std::string& regionCode) = 0;
        virtual RegionInfo GetCurrentRegion() const = 0;
        virtual void PingAllRegions(std::function<void(const std::vector<RegionInfo>&)> onComplete) = 0;
    };

    // Room Management Interface
    struct RoomOptions {
        uint8_t maxPlayers = 4;
        bool isOpen = true;
        bool isVisible = true;
        uint32_t emptyRoomTtl = 0;
        uint32_t playerTtl = 0;
        Protocol::PhotonHashtable customRoomProperties;
        std::vector<std::string> propsListedInLobby;
    };

    struct ActorState {
        int32_t actorNr = 0;
        std::string userId;
        std::string nickname;
        bool isMasterClient = false;
        bool isInactive = false;
        Protocol::PhotonHashtable customProperties;
    };

    struct RoomStateInfo {
        std::string name;
        uint8_t maxPlayers = 4;
        bool isOpen = true;
        bool isVisible = true;
        int32_t masterClientId = 1;
        int32_t localActorNumber = 0;
        std::map<int32_t, ActorState> actors;
        Protocol::PhotonHashtable customProperties;
    };

    class IPhotonRoomService {
    public:
        virtual ~IPhotonRoomService() = default;

        virtual bool CreateRoom(const std::string& roomName, const RoomOptions& options) = 0;
        virtual bool JoinRoom(const std::string& roomName) = 0;
        virtual bool JoinRandomRoom(const Protocol::PhotonHashtable& expectedProperties = {}) = 0;
        virtual bool LeaveRoom() = 0;

        virtual bool SetCustomProperties(const Protocol::PhotonHashtable& properties) = 0;
        virtual bool SetActorProperties(int32_t actorNr, const Protocol::PhotonHashtable& properties) = 0;

        virtual bool RaiseEvent(uint8_t eventCode, const Protocol::PhotonValue& eventContent,
                                uint8_t receiverGroup = Protocol::ReceiverGroup::Others,
                                uint8_t cachingOption = Protocol::EventCaching::DoNotCache) = 0;

        virtual bool IsInRoom() const = 0;
        virtual RoomStateInfo GetCurrentRoom() const = 0;
    };

    // Authentication Interface
    class IPhotonAuthentication {
    public:
        virtual ~IPhotonAuthentication() = default;

        virtual bool Authenticate(const std::string& appId, const std::string& appVersion,
                                  const std::string& userId, const std::string& token = "") = 0;
        virtual bool IsAuthenticated() const = 0;
        virtual std::string GetUserId() const = 0;
        virtual std::string GetToken() const = 0;
    };

    // Core Backend Interface
    class IPhotonBackend {
    public:
        virtual ~IPhotonBackend() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual bool Connect(const std::string& endpoint, uint16_t port) = 0;
        virtual void Disconnect() = 0;
        virtual ConnectionState GetState() const = 0;

        virtual void Update() = 0; // Tick loop

        virtual IPhotonAuthentication* GetAuth() = 0;
        virtual IPhotonRegionProvider* GetRegionProvider() = 0;
        virtual IPhotonRoomService* GetRoomService() = 0;

        virtual BackendMode GetMode() const = 0;
        virtual const char* GetName() const = 0;

        // Callback hooks
        virtual void OnOperationResponse(std::function<void(const Protocol::OperationResponse&)> handler) = 0;
        virtual void OnEvent(std::function<void(const Protocol::EventData&)> handler) = 0;
    };

    // Stubs for Future Extensions
    class IPhotonVoiceBackend {
    public:
        virtual ~IPhotonVoiceBackend() = default;
        virtual bool InitializeVoice(const std::string& voiceAppId, const std::string& endpoint) = 0;
        virtual bool SendAudioData(const std::vector<uint8_t>& audioFrame, uint32_t channelId) = 0;
        virtual void ShutdownVoice() = 0;
        virtual bool IsVoiceActive() const = 0;
    };

    class IPhotonChatBackend {
    public:
        virtual ~IPhotonChatBackend() = default;
        virtual bool ConnectChat(const std::string& chatAppId, const std::string& userId) = 0;
        virtual bool SubscribeChannel(const std::string& channelName) = 0;
        virtual bool SendChannelMessage(const std::string& channelName, const std::string& message) = 0;
        virtual void DisconnectChat() = 0;
        virtual bool IsChatConnected() const = 0;
    };

    class IFusionBackend {
    public:
        virtual ~IFusionBackend() = default;
        virtual bool InitializeFusion(const std::string& fusionAppId, bool isSharedMode) = 0;
        virtual bool SendTickState(uint32_t tick, const std::vector<uint8_t>& stateData) = 0;
        virtual void ShutdownFusion() = 0;
        virtual bool IsFusionActive() const = 0;
    };

} // namespace ReFix::Photon
