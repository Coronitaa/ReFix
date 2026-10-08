#pragma once

#include "refix_lan_types.h"
#include "refix_lan_transport.h"

#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <chrono>

namespace refix::lan {

// =============================================================================
// 1. LocalIdentityService Interface
// =============================================================================
class ILocalIdentityService {
public:
    virtual ~ILocalIdentityService() = default;

    virtual MachineId GetMachineId() const = 0;
    virtual PeerId GetLocalPeerId() const = 0;
    virtual std::string GetDisplayName() const = 0;
    virtual void SetDisplayName(std::string_view name) = 0;
    virtual std::string GetInstanceTag() const = 0;
    virtual void SetCustomAttribute(std::string_view key, std::string_view value) = 0;
    virtual std::string GetCustomAttribute(std::string_view key) const = 0;
};

// =============================================================================
// 2. PeerRegistry Interface
// =============================================================================
enum class PeerTransportState : uint8_t {
    Unknown,
    Discovered,
    Verified,
    Connected,
    Stale,
    Unreachable
};

struct PeerInfo {
    PeerId peerId;
    MachineId machineId = 0;
    LanEndpoint endpoint;
    std::string displayName;
    ExternalId externalId;
    PeerTransportState state = PeerTransportState::Unknown;
    uint32_t pingMs = 0;
    std::chrono::steady_clock::time_point lastSeen;
};

class IPeerRegistry {
public:
    virtual ~IPeerRegistry() = default;

    virtual void RegisterOrUpdatePeer(const PeerInfo& info) = 0;
    virtual void BindExternalId(const PeerId& peerId, const ExternalId& extId) = 0;
    virtual std::optional<PeerInfo> FindByPeerId(const PeerId& peerId) const = 0;
    virtual std::optional<PeerId> FindByEndpoint(const LanEndpoint& endpoint) const = 0;
    virtual std::optional<PeerId> FindByExternalId(const ExternalId& extId) const = 0;
    virtual std::optional<PeerId> FindBySteamId(uint64_t steamId) const = 0;
    virtual std::optional<PeerId> FindByPuid(std::string_view puid) const = 0;

    virtual std::vector<PeerInfo> GetAllPeers() const = 0;
    virtual void PruneStalePeers(std::chrono::milliseconds maxAge) = 0;
};

// =============================================================================
// 3. DiscoveryService Interface
// =============================================================================
class IDiscoveryService {
public:
    virtual ~IDiscoveryService() = default;

    virtual bool Start(std::string_view appScope, uint16_t discoveryPort = 47584) = 0;
    virtual void Stop() = 0;
    virtual void BroadcastQuery() = 0;
    virtual void AnnouncePresence() = 0;
    virtual LanEndpoint GetBoundEndpoint() const = 0;
};

// =============================================================================
// 4. LobbyService Interface
// =============================================================================
struct LobbyMember {
    PeerId peerId;
    std::string displayName;
    LanEndpoint endpoint;
    bool isOwner = false;
    std::unordered_map<std::string, AttributeValue> attributes;
};

struct LobbyRecord {
    std::string lobbyId;
    PeerId ownerPeerId;
    uint32_t maxMembers = 4;
    LobbyPermissionLevel permission = LobbyPermissionLevel::PublicAdvertised;
    bool joinable = true;
    uint32_t revision = 0;
    LanEndpoint hostEndpoint;

    std::unordered_map<std::string, AttributeValue> attributes;
    std::vector<LobbyMember> members;

    uint32_t AvailableSlots() const {
        return maxMembers > members.size() ? static_cast<uint32_t>(maxMembers - members.size()) : 0;
    }
    const LobbyMember* FindMember(const PeerId& id) const {
        for (const auto& m : members) {
            if (m.peerId == id) return &m;
        }
        return nullptr;
    }
};

class ILobbyService {
public:
    virtual ~ILobbyService() = default;

    virtual std::string CreateLobby(uint32_t maxMembers, LobbyPermissionLevel permission) = 0;
    virtual bool DestroyLobby(std::string_view lobbyId) = 0;
    virtual bool SetLobbyData(std::string_view lobbyId, std::string_view key, const AttributeValue& value) = 0;
    virtual bool DeleteLobbyData(std::string_view lobbyId, std::string_view key) = 0;
    virtual bool SetMemberData(std::string_view lobbyId, std::string_view key, const AttributeValue& value) = 0;

    virtual bool RequestJoin(std::string_view lobbyId) = 0;
    virtual bool LeaveLobby(std::string_view lobbyId) = 0;
    virtual bool KickMember(std::string_view lobbyId, const PeerId& target) = 0;
    virtual bool PromoteMember(std::string_view lobbyId, const PeerId& newOwner) = 0;

    virtual std::optional<LobbyRecord> GetLobby(std::string_view lobbyId) const = 0;
    virtual std::vector<LobbyRecord> GetAllKnownLobbies() const = 0;
    virtual bool IsHosting(std::string_view lobbyId) const = 0;
};

// =============================================================================
// 5. SessionService Interface
// =============================================================================
enum class SessionState : uint8_t {
    None,
    Configuring,
    LobbyWaiting,
    Starting,
    InProgress,
    Ending,
    Terminated
};

struct SessionRecord {
    std::string sessionId;
    std::string lobbyId;
    PeerId hostPeerId;
    LanEndpoint gameServerEndpoint;
    SessionState state = SessionState::None;
    uint64_t sessionNonce = 0;
    std::unordered_map<PeerId, bool> playerReadiness;
};

class ISessionService {
public:
    virtual ~ISessionService() = default;

    virtual std::string CreateSession(std::string_view lobbyId, const LanEndpoint& gameServer) = 0;
    virtual bool UpdateSessionState(std::string_view sessionId, SessionState newState) = 0;
    virtual bool RegisterPlayerReady(std::string_view sessionId, const PeerId& peer) = 0;
    virtual std::optional<SessionRecord> GetActiveSession() const = 0;
    virtual void TerminateSession(std::string_view sessionId) = 0;
};

// =============================================================================
// 6. MatchmakingService Interface
// =============================================================================
struct SearchFilter {
    std::string key;
    AttributeValue value;
    ComparisonOp op = ComparisonOp::Equal;
    bool Matches(const LobbyRecord& lobby) const;
};

struct MatchmakingCriteria {
    std::vector<SearchFilter> filters;
    uint32_t maxResults = 50;
    uint32_t minAvailableSlots = 1;
    bool requireJoinableOnly = true;
};

class IMatchmakingService {
public:
    virtual ~IMatchmakingService() = default;

    virtual void RefreshLobbyList() = 0;
    virtual std::vector<LobbyRecord> SearchLobbies(const MatchmakingCriteria& criteria) = 0;
};

// =============================================================================
// 7. CallbackDispatcher Interface
// =============================================================================
struct LanEvent {
    enum class Type {
        PeerDiscovered,
        PeerLost,
        LobbyCreated,
        LobbyUpdated,
        LobbyDestroyed,
        MemberJoined,
        MemberLeft,
        SessionStateChanged,
        DataPacketReceived
    } type;

    PeerId peerId;
    std::string lobbyId;
    MemberLeaveReason leaveReason = MemberLeaveReason::LeftGracefully;
    uint8_t channel = 0;
    std::vector<uint8_t> payload;
};

class ICallbackDispatcher {
public:
    virtual ~ICallbackDispatcher() = default;

    virtual void PostEvent(const LanEvent& event) = 0;
    virtual void Subscribe(LanEvent::Type type, std::function<void(const LanEvent&)> handler) = 0;
    virtual void DispatchPending(uint32_t maxEvents = 100) = 0;
};

// =============================================================================
// 8. LocalRelayService Interface
// =============================================================================
class ILocalRelayService {
public:
    virtual ~ILocalRelayService() = default;

    virtual bool SendOrRelay(const PeerId& target, uint8_t channel, const void* data, size_t len) = 0;
    virtual void SetDirectRouteAvailable(const PeerId& target, bool available) = 0;
    virtual bool IsDirectRouteAvailable(const PeerId& target) const = 0;
};

// =============================================================================
// Unified ILanCore Interface Facade
// =============================================================================
class ILanCore {
public:
    virtual ~ILanCore() = default;

    static ILanCore& Get();

    virtual bool Initialize(std::string_view appScope, uint16_t discoveryPort = 47584) = 0;
    virtual void Shutdown() = 0;
    virtual void Tick() = 0; // Pumps socket IO, TTL checks, and CallbackDispatcher

    virtual ILocalIdentityService& Identity() = 0;
    virtual IPeerRegistry& Peers() = 0;
    virtual IDiscoveryService& Discovery() = 0;
    virtual ILobbyService& Lobby() = 0;
    virtual ISessionService& Session() = 0;
    virtual IMatchmakingService& Matchmaking() = 0;
    virtual ICallbackDispatcher& Callbacks() = 0;
    virtual ILocalRelayService& Relay() = 0;
    virtual LanTransport& Transport() = 0;
};

} // namespace refix::lan
