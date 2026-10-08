#include "refix_lan_core.h"
#include "refix_lan_wire.h"

#include <windows.h>
#include <algorithm>
#include <iostream>

namespace refix::lan {

// =============================================================================
// Helper: 64-bit FNV-1a Hash
// =============================================================================
static uint64_t Fnv1a64(const void* data, size_t len) {
    const auto* p = reinterpret_cast<const uint8_t*>(data);
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

// =============================================================================
// LocalIdentityServiceImpl
// =============================================================================
class LocalIdentityServiceImpl : public ILocalIdentityService {
public:
    LocalIdentityServiceImpl() {
        // Derive MachineId from Volume Serial + Computer Name
        char compName[MAX_COMPUTERNAME_LENGTH + 1] = "REFIX_PC";
        DWORD compLen = sizeof(compName);
        GetComputerNameA(compName, &compLen);

        DWORD volSerial = 0x12345678;
        GetVolumeInformationA("C:\\", nullptr, 0, &volSerial, nullptr, nullptr, nullptr, 0);

        uint64_t h1 = Fnv1a64(compName, strlen(compName));
        uint64_t h2 = Fnv1a64(&volSerial, sizeof(volSerial));
        m_machineId = h1 ^ (h2 * 0x9E3779B97F4A7C15ULL);

        // Derive LocalPeerId (MachineId + Instance PID + Unique counter)
        m_localPeerId.high = m_machineId;
        uint32_t pid = GetCurrentProcessId();
        uint64_t now = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        m_localPeerId.low = (static_cast<uint64_t>(pid) << 32) ^ (now & 0xFFFFFFFF);

        // Display Name from OS User
        char userName[256] = "Player";
        DWORD userLen = sizeof(userName);
        if (GetUserNameA(userName, &userLen)) {
            m_displayName = userName;
        } else {
            m_displayName = "Player_" + std::to_string(pid);
        }

        m_instanceTag = std::to_string(pid);
    }

    MachineId GetMachineId() const override { return m_machineId; }
    PeerId GetLocalPeerId() const override { return m_localPeerId; }
    std::string GetDisplayName() const override { return m_displayName; }
    void SetDisplayName(std::string_view name) override { m_displayName = std::string(name); }
    std::string GetInstanceTag() const override { return m_instanceTag; }

    void SetCustomAttribute(std::string_view key, std::string_view value) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_customAttrs[std::string(key)] = std::string(value);
    }

    std::string GetCustomAttribute(std::string_view key) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_customAttrs.find(std::string(key));
        return it != m_customAttrs.end() ? it->second : "";
    }

private:
    MachineId m_machineId = 0;
    PeerId m_localPeerId;
    std::string m_displayName;
    std::string m_instanceTag;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_customAttrs;
};

// =============================================================================
// PeerRegistryImpl
// =============================================================================
class PeerRegistryImpl : public IPeerRegistry {
public:
    void RegisterOrUpdatePeer(const PeerInfo& info) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& p = m_peers[info.peerId];
        p = info;
        p.lastSeen = std::chrono::steady_clock::now();

        if (info.endpoint.IsValid()) {
            m_byEndpoint[info.endpoint] = info.peerId;
        }
        if (info.externalId.numericId != 0) {
            m_bySteamId[info.externalId.numericId] = info.peerId;
        }
        if (!info.externalId.stringId.empty()) {
            m_byPuid[info.externalId.stringId] = info.peerId;
        }
    }

    void BindExternalId(const PeerId& peerId, const ExternalId& extId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_peers.find(peerId);
        if (it != m_peers.end()) {
            it->second.externalId = extId;
            if (extId.numericId != 0) m_bySteamId[extId.numericId] = peerId;
            if (!extId.stringId.empty()) m_byPuid[extId.stringId] = peerId;
        }
    }

    std::optional<PeerInfo> FindByPeerId(const PeerId& peerId) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_peers.find(peerId);
        if (it != m_peers.end()) return it->second;
        return std::nullopt;
    }

    std::optional<PeerId> FindByEndpoint(const LanEndpoint& endpoint) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_byEndpoint.find(endpoint);
        if (it != m_byEndpoint.end()) return it->second;
        return std::nullopt;
    }

    std::optional<PeerId> FindByExternalId(const ExternalId& extId) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (extId.numericId != 0) {
            auto it = m_bySteamId.find(extId.numericId);
            if (it != m_bySteamId.end()) return it->second;
        }
        if (!extId.stringId.empty()) {
            auto it = m_byPuid.find(extId.stringId);
            if (it != m_byPuid.end()) return it->second;
        }
        return std::nullopt;
    }

    std::optional<PeerId> FindBySteamId(uint64_t steamId) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_bySteamId.find(steamId);
        if (it != m_bySteamId.end()) return it->second;
        return std::nullopt;
    }

    std::optional<PeerId> FindByPuid(std::string_view puid) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_byPuid.find(std::string(puid));
        if (it != m_byPuid.end()) return it->second;
        return std::nullopt;
    }

    std::vector<PeerInfo> GetAllPeers() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<PeerInfo> res;
        res.reserve(m_peers.size());
        for (const auto& [_, p] : m_peers) {
            res.push_back(p);
        }
        return res;
    }

    void PruneStalePeers(std::chrono::milliseconds maxAge) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = m_peers.begin(); it != m_peers.end();) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastSeen) > maxAge) {
                m_byEndpoint.erase(it->second.endpoint);
                if (it->second.externalId.numericId != 0) m_bySteamId.erase(it->second.externalId.numericId);
                if (!it->second.externalId.stringId.empty()) m_byPuid.erase(it->second.externalId.stringId);
                it = m_peers.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<PeerId, PeerInfo> m_peers;
    std::unordered_map<LanEndpoint, PeerId> m_byEndpoint;
    std::unordered_map<uint64_t, PeerId> m_bySteamId;
    std::unordered_map<std::string, PeerId> m_byPuid;
};

// =============================================================================
// CallbackDispatcherImpl
// =============================================================================
class CallbackDispatcherImpl : public ICallbackDispatcher {
public:
    void PostEvent(const LanEvent& event) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(event);
    }

    void Subscribe(LanEvent::Type type, std::function<void(const LanEvent&)> handler) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers[type].push_back(handler);
    }

    void DispatchPending(uint32_t maxEvents) override {
        std::vector<LanEvent> toDispatch;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            uint32_t count = 0;
            while (!m_events.empty() && count < maxEvents) {
                toDispatch.push_back(std::move(m_events.front()));
                m_events.pop_front();
                count++;
            }
        }

        for (const auto& ev : toDispatch) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_subscribers.find(ev.type);
            if (it != m_subscribers.end()) {
                for (const auto& fn : it->second) {
                    fn(ev);
                }
            }
        }
    }

private:
    std::mutex m_mutex;
    std::deque<LanEvent> m_events;
    std::unordered_map<LanEvent::Type, std::vector<std::function<void(const LanEvent&)>>> m_subscribers;
};

// =============================================================================
// LobbyServiceImpl
// =============================================================================
class LobbyServiceImpl : public ILobbyService {
public:
    explicit LobbyServiceImpl(ILocalIdentityService& identity, ICallbackDispatcher& callbacks)
        : m_identity(identity), m_callbacks(callbacks) {}

    std::string CreateLobby(uint32_t maxMembers, LobbyPermissionLevel permission) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        static uint32_t s_lobbyCount = 1;

        std::string lobbyId = "LOBBY_" + m_identity.GetLocalPeerId().ToString().substr(0, 8) + "_" + std::to_string(s_lobbyCount++);
        LobbyRecord r;
        r.lobbyId = lobbyId;
        r.ownerPeerId = m_identity.GetLocalPeerId();
        r.maxMembers = maxMembers;
        r.permission = permission;
        r.joinable = true;
        r.revision = 1;

        LobbyMember me;
        me.peerId = m_identity.GetLocalPeerId();
        me.displayName = m_identity.GetDisplayName();
        me.isOwner = true;
        r.members.push_back(me);

        m_lobbies[lobbyId] = r;

        LanEvent ev;
        ev.type = LanEvent::Type::LobbyCreated;
        ev.lobbyId = lobbyId;
        ev.peerId = m_identity.GetLocalPeerId();
        m_callbacks.PostEvent(ev);

        return lobbyId;
    }

    bool DestroyLobby(std::string_view lobbyId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end()) {
            m_lobbies.erase(it);
            LanEvent ev;
            ev.type = LanEvent::Type::LobbyDestroyed;
            ev.lobbyId = std::string(lobbyId);
            m_callbacks.PostEvent(ev);
            return true;
        }
        return false;
    }

    bool SetLobbyData(std::string_view lobbyId, std::string_view key, const AttributeValue& value) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end()) {
            it->second.attributes[std::string(key)] = value;
            it->second.revision++;
            LanEvent ev;
            ev.type = LanEvent::Type::LobbyUpdated;
            ev.lobbyId = std::string(lobbyId);
            m_callbacks.PostEvent(ev);
            return true;
        }
        return false;
    }

    bool DeleteLobbyData(std::string_view lobbyId, std::string_view key) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end()) {
            it->second.attributes.erase(std::string(key));
            it->second.revision++;
            return true;
        }
        return false;
    }

    bool SetMemberData(std::string_view lobbyId, std::string_view key, const AttributeValue& value) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end()) {
            for (auto& m : it->second.members) {
                if (m.peerId == m_identity.GetLocalPeerId()) {
                    m.attributes[std::string(key)] = value;
                    it->second.revision++;
                    return true;
                }
            }
        }
        return false;
    }

    bool RequestJoin(std::string_view lobbyId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end()) {
            if (it->second.AvailableSlots() > 0 && it->second.joinable) {
                LobbyMember me;
                me.peerId = m_identity.GetLocalPeerId();
                me.displayName = m_identity.GetDisplayName();
                me.isOwner = false;
                it->second.members.push_back(me);
                it->second.revision++;

                LanEvent ev;
                ev.type = LanEvent::Type::MemberJoined;
                ev.lobbyId = std::string(lobbyId);
                ev.peerId = me.peerId;
                m_callbacks.PostEvent(ev);
                return true;
            }
        }
        return false;
    }

    bool LeaveLobby(std::string_view lobbyId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end()) {
            auto& members = it->second.members;
            auto mit = std::remove_if(members.begin(), members.end(), [&](const LobbyMember& m) {
                return m.peerId == m_identity.GetLocalPeerId();
            });
            if (mit != members.end()) {
                members.erase(mit, members.end());
                it->second.revision++;

                LanEvent ev;
                ev.type = LanEvent::Type::MemberLeft;
                ev.lobbyId = std::string(lobbyId);
                ev.peerId = m_identity.GetLocalPeerId();
                ev.leaveReason = MemberLeaveReason::LeftGracefully;
                m_callbacks.PostEvent(ev);

                // If owner left, migrate or destroy
                if (it->second.ownerPeerId == m_identity.GetLocalPeerId()) {
                    if (members.empty()) {
                        m_lobbies.erase(it);
                    } else {
                        it->second.ownerPeerId = members.front().peerId;
                        members.front().isOwner = true;
                    }
                }
                return true;
            }
        }
        return false;
    }

    bool KickMember(std::string_view lobbyId, const PeerId& target) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end() && it->second.ownerPeerId == m_identity.GetLocalPeerId()) {
            auto& members = it->second.members;
            auto mit = std::remove_if(members.begin(), members.end(), [&](const LobbyMember& m) {
                return m.peerId == target;
            });
            if (mit != members.end()) {
                members.erase(mit, members.end());
                it->second.revision++;
                LanEvent ev;
                ev.type = LanEvent::Type::MemberLeft;
                ev.lobbyId = std::string(lobbyId);
                ev.peerId = target;
                ev.leaveReason = MemberLeaveReason::Kicked;
                m_callbacks.PostEvent(ev);
                return true;
            }
        }
        return false;
    }

    bool PromoteMember(std::string_view lobbyId, const PeerId& newOwner) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end() && it->second.ownerPeerId == m_identity.GetLocalPeerId()) {
            for (auto& m : it->second.members) {
                m.isOwner = (m.peerId == newOwner);
            }
            it->second.ownerPeerId = newOwner;
            it->second.revision++;
            return true;
        }
        return false;
    }

    std::optional<LobbyRecord> GetLobby(std::string_view lobbyId) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        if (it != m_lobbies.end()) return it->second;
        return std::nullopt;
    }

    std::vector<LobbyRecord> GetAllKnownLobbies() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<LobbyRecord> res;
        res.reserve(m_lobbies.size());
        for (const auto& [_, lob] : m_lobbies) {
            res.push_back(lob);
        }
        return res;
    }

    bool IsHosting(std::string_view lobbyId) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(std::string(lobbyId));
        return (it != m_lobbies.end() && it->second.ownerPeerId == m_identity.GetLocalPeerId());
    }

    void ImportRemoteLobby(const LobbyRecord& record) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lobbies.find(record.lobbyId);
        if (it == m_lobbies.end() || record.revision > it->second.revision) {
            m_lobbies[record.lobbyId] = record;
            LanEvent ev;
            ev.type = LanEvent::Type::LobbyUpdated;
            ev.lobbyId = record.lobbyId;
            m_callbacks.PostEvent(ev);
        }
    }

private:
    mutable std::mutex m_mutex;
    ILocalIdentityService& m_identity;
    ICallbackDispatcher& m_callbacks;
    std::unordered_map<std::string, LobbyRecord> m_lobbies;
};

// =============================================================================
// MatchmakingServiceImpl
// =============================================================================
bool SearchFilter::Matches(const LobbyRecord& lobby) const {
    auto it = lobby.attributes.find(key);
    if (it == lobby.attributes.end()) return false;
    const AttributeValue& actual = it->second;

    int cmp = actual.Compare(value);
    switch (op) {
        case ComparisonOp::Equal: return cmp == 0;
        case ComparisonOp::NotEqual: return cmp != 0;
        case ComparisonOp::GreaterThan: return cmp > 0;
        case ComparisonOp::GreaterThanOrEqual: return cmp >= 0;
        case ComparisonOp::LessThan: return cmp < 0;
        case ComparisonOp::LessThanOrEqual: return cmp <= 0;
        case ComparisonOp::Contains:
            if (actual.type == AttributeType::String && value.type == AttributeType::String) {
                return actual.asString.find(value.asString) != std::string::npos;
            }
            return false;
        default: return true;
    }
}

class MatchmakingServiceImpl : public IMatchmakingService {
public:
    explicit MatchmakingServiceImpl(ILobbyService& lobby) : m_lobby(lobby) {}

    void RefreshLobbyList() override {
        // Triggered via Discovery queries
    }

    std::vector<LobbyRecord> SearchLobbies(const MatchmakingCriteria& criteria) override {
        std::vector<LobbyRecord> all = m_lobby.GetAllKnownLobbies();
        std::vector<LobbyRecord> matches;

        for (const auto& lob : all) {
            if (criteria.requireJoinableOnly && !lob.joinable) continue;
            if (lob.AvailableSlots() < criteria.minAvailableSlots) continue;

            bool allMatched = true;
            for (const auto& f : criteria.filters) {
                if (!f.Matches(lob)) {
                    allMatched = false;
                    break;
                }
            }
            if (allMatched) {
                matches.push_back(lob);
                if (matches.size() >= criteria.maxResults) break;
            }
        }
        return matches;
    }

private:
    ILobbyService& m_lobby;
};

// =============================================================================
// SessionServiceImpl
// =============================================================================
class SessionServiceImpl : public ISessionService {
public:
    explicit SessionServiceImpl(ILocalIdentityService& identity) : m_identity(identity) {}

    std::string CreateSession(std::string_view lobbyId, const LanEndpoint& gameServer) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_session.sessionId = "SESSION_" + std::string(lobbyId);
        m_session.lobbyId = std::string(lobbyId);
        m_session.hostPeerId = m_identity.GetLocalPeerId();
        m_session.gameServerEndpoint = gameServer;
        m_session.state = SessionState::Starting;
        m_session.sessionNonce = static_cast<uint64_t>(GetCurrentProcessId()) ^ 0xF00DCAFE;
        return m_session.sessionId;
    }

    bool UpdateSessionState(std::string_view sessionId, SessionState newState) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_session.sessionId == sessionId) {
            m_session.state = newState;
            return true;
        }
        return false;
    }

    bool RegisterPlayerReady(std::string_view sessionId, const PeerId& peer) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_session.sessionId == sessionId) {
            m_session.playerReadiness[peer] = true;
            return true;
        }
        return false;
    }

    std::optional<SessionRecord> GetActiveSession() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_session.sessionId.empty()) return m_session;
        return std::nullopt;
    }

    void TerminateSession(std::string_view sessionId) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_session.sessionId == sessionId) {
            m_session.state = SessionState::Terminated;
            m_session.sessionId.clear();
        }
    }

private:
    mutable std::mutex m_mutex;
    ILocalIdentityService& m_identity;
    SessionRecord m_session;
};

// =============================================================================
// LocalRelayServiceImpl
// =============================================================================
class LocalRelayServiceImpl : public ILocalRelayService {
public:
    LocalRelayServiceImpl(LanTransport& transport, IPeerRegistry& peers)
        : m_transport(transport), m_peers(peers) {}

    bool SendOrRelay(const PeerId& target, uint8_t channel, const void* data, size_t len) override {
        auto pInfo = m_peers.FindByPeerId(target);
        if (!pInfo.has_value() || !pInfo->endpoint.IsValid()) {
            return false;
        }

        // Direct unicast via Universal Transport
        return m_transport.SendReliable(target, pInfo->endpoint, channel, data, len);
    }

    void SetDirectRouteAvailable(const PeerId& target, bool available) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_directRoutes[target] = available;
    }

    bool IsDirectRouteAvailable(const PeerId& target) const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_directRoutes.find(target);
        return it != m_directRoutes.end() ? it->second : true;
    }

private:
    mutable std::mutex m_mutex;
    LanTransport& m_transport;
    IPeerRegistry& m_peers;
    std::unordered_map<PeerId, bool> m_directRoutes;
};

// =============================================================================
// DiscoveryServiceImpl
// =============================================================================
class DiscoveryServiceImpl : public IDiscoveryService, public ILanTransportListener {
public:
    DiscoveryServiceImpl(LanTransport& transport, ILocalIdentityService& identity,
                         IPeerRegistry& peers, LobbyServiceImpl& lobby, ICallbackDispatcher& callbacks)
        : m_transport(transport), m_identity(identity), m_peers(peers), m_lobby(lobby), m_callbacks(callbacks) {}

    bool Start(std::string_view appScope, uint16_t discoveryPort) override {
        m_appScope = std::string(appScope);
        m_transport.SetLocalPeerId(m_identity.GetLocalPeerId());
        return m_transport.Start(discoveryPort, this);
    }

    void Stop() override {
        m_transport.Stop();
    }

    void BroadcastQuery() override {
        ByteWriter w;
        w.WriteString(m_appScope);
        w.WritePeerId(m_identity.GetLocalPeerId());
        w.WriteEndpoint(m_transport.GetLocalDataEndpoint());
        m_transport.BroadcastDiscovery(w.Data(), w.Size());
    }

    void AnnouncePresence() override {
        ByteWriter w;
        w.WriteString(m_appScope);
        w.WritePeerId(m_identity.GetLocalPeerId());
        w.WriteString(m_identity.GetDisplayName());
        w.WriteEndpoint(m_transport.GetLocalDataEndpoint());
        m_transport.BroadcastDiscovery(w.Data(), w.Size());
    }

    LanEndpoint GetBoundEndpoint() const override {
        return m_transport.GetLocalDataEndpoint();
    }

    // ILanTransportListener Implementation
    void OnDiscoveryPacket(const LanEndpoint& sender, MsgType type, const uint8_t* data, size_t len) override {
        ByteReader r(data, len);
        std::string scope = r.ReadString();
        if (scope != m_appScope) return; // Discard packets from other games on LAN!

        PeerId pid = r.ReadPeerId();
        if (pid == m_identity.GetLocalPeerId()) return;

        if (type == MsgType::DiscoveryBeacon || type == MsgType::DiscoveryResponse) {
            std::string name = r.ReadString();
            LanEndpoint ep = r.ReadEndpoint();
            if (!ep.IsValid()) ep = sender;

            PeerInfo info;
            info.peerId = pid;
            info.displayName = name;
            info.endpoint = ep;
            info.state = PeerTransportState::Discovered;
            m_peers.RegisterOrUpdatePeer(info);

            LanEvent ev;
            ev.type = LanEvent::Type::PeerDiscovered;
            ev.peerId = pid;
            m_callbacks.PostEvent(ev);

            // If we received a query, send a unicast response
            if (type == MsgType::DiscoveryQuery) {
                ByteWriter resp;
                resp.WriteString(m_appScope);
                resp.WritePeerId(m_identity.GetLocalPeerId());
                resp.WriteString(m_identity.GetDisplayName());
                resp.WriteEndpoint(m_transport.GetLocalDataEndpoint());
                m_transport.SendDiscoveryResponse(sender, resp.Data(), resp.Size());
            }
        }
    }

    void OnInboundData(const InboundPacket& packet) override {
        LanEvent ev;
        ev.type = LanEvent::Type::DataPacketReceived;
        ev.peerId = packet.senderPeerId;
        ev.payload = packet.payload;
        m_callbacks.PostEvent(ev);
    }

    void OnPeerTimeout(const PeerId& peerId, const LanEndpoint& endpoint) override {
        LanEvent ev;
        ev.type = LanEvent::Type::PeerLost;
        ev.peerId = peerId;
        m_callbacks.PostEvent(ev);
    }

private:
    LanTransport& m_transport;
    ILocalIdentityService& m_identity;
    IPeerRegistry& m_peers;
    LobbyServiceImpl& m_lobby;
    ICallbackDispatcher& m_callbacks;
    std::string m_appScope;
};

// =============================================================================
// LanCore Facade Implementation
// =============================================================================
class LanCoreImpl : public ILanCore {
public:
    LanCoreImpl()
        : m_identity(),
          m_peers(),
          m_callbacks(),
          m_lobby(m_identity, m_callbacks),
          m_session(m_identity),
          m_matchmaking(m_lobby),
          m_transport(),
          m_relay(m_transport, m_peers),
          m_discovery(m_transport, m_identity, m_peers, m_lobby, m_callbacks) {}

    bool Initialize(std::string_view appScope, uint16_t discoveryPort) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return true;
        m_initialized = m_discovery.Start(appScope, discoveryPort);
        m_lastAnnounce = std::chrono::steady_clock::now();
        m_discovery.AnnouncePresence();
        return m_initialized;
    }

    void Shutdown() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return;
        m_discovery.Stop();
        m_initialized = false;
    }

    void Tick() override {
        auto now = std::chrono::steady_clock::now();
        // Periodic heartbeat beacon every 2.0s
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAnnounce).count() > 2000) {
            m_lastAnnounce = now;
            m_discovery.AnnouncePresence();
        }

        // Prune stale peers inactive > 12s
        m_peers.PruneStalePeers(std::chrono::milliseconds(12000));

        // Pump event queue
        m_callbacks.DispatchPending(50);
    }

    ILocalIdentityService& Identity() override { return m_identity; }
    IPeerRegistry& Peers() override { return m_peers; }
    IDiscoveryService& Discovery() override { return m_discovery; }
    ILobbyService& Lobby() override { return m_lobby; }
    ISessionService& Session() override { return m_session; }
    IMatchmakingService& Matchmaking() override { return m_matchmaking; }
    ICallbackDispatcher& Callbacks() override { return m_callbacks; }
    ILocalRelayService& Relay() override { return m_relay; }
    LanTransport& Transport() override { return m_transport; }

private:
    std::mutex m_mutex;
    bool m_initialized = false;
    std::chrono::steady_clock::time_point m_lastAnnounce;

    LocalIdentityServiceImpl m_identity;
    PeerRegistryImpl m_peers;
    CallbackDispatcherImpl m_callbacks;
    LobbyServiceImpl m_lobby;
    SessionServiceImpl m_session;
    MatchmakingServiceImpl m_matchmaking;
    LanTransport m_transport;
    LocalRelayServiceImpl m_relay;
    DiscoveryServiceImpl m_discovery;
};

ILanCore& ILanCore::Get() {
    static LanCoreImpl s_instance;
    return s_instance;
}

} // namespace refix::lan
