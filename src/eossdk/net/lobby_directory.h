// ReFix EOS v3 - the distributed lobby directory.
//
// There is no central server. Each host advertises the lobbies it owns on the
// discovery group; every instance keeps a cache of what it has heard and
// expires entries that stop being advertised. Joining is a direct conversation
// with the host, which is the only authority on membership:
//
//     client --JOIN_REQ--> host        host validates seats/permission
//     client <--JOIN_RSP-- host        full lobby record on success
//              <--UPDATE-- host        every later change, to all members
//
// That single rule - the host owns the record, everyone else replicates it - is
// what removes the ghost lobbies and desynchronised member lists of the old
// Steam-tunnelling proxy.
#pragma once
#include "../core/refix_common.h"
#include "../core/lobby_model.h"
#include "refix_transport.h"
#include "steam_backend.h"
#include <functional>
#include <atomic>
#include <thread>

namespace refix {

enum class MemberChange { Joined, Left, Disconnected, Kicked, Promoted, Closed };

struct DirectoryEvents {
    // Raised on the instance that owns the lobby and on every replica.
    std::function<void(const LobbyRecord&)>                          OnLobbyUpdated;
    std::function<void(const LobbyRecord&, const std::string& puid,
                       MemberChange)>                                OnMemberChanged;
    std::function<void(const std::string& lobbyId,
                       const std::string& fromPuid,
                       const std::string& inviteId)>                 OnInviteReceived;
    // Raised when the player accepted an invite outside the game (the Steam
    // overlay), so the title can be told to join.
    std::function<void(const std::string& lobbyId,
                       const std::string& fromPuid)>                 OnInviteAccepted;
    // A datagram addressed to the game's own P2P layer.
    std::function<void(const std::string& fromPuid, const std::string& socketName,
                       uint8_t channel, const uint8_t* data, size_t len)> OnP2PPacket;
};

// Where a peer can be reached. A peer discovered on the LAN has a UDP endpoint;
// one discovered through Steam has a SteamID and is reachable from anywhere.
// Both may be set, in which case Steam wins - it is the path that survives a
// player changing network.
struct PeerRef {
    Endpoint Udp;
    uint64_t SteamId = 0;
    bool Valid() const { return SteamId != 0 || Udp.Valid(); }
};

class LobbyDirectory {
public:
    static LobbyDirectory& Get();

    // `scope` isolates one title's lobbies from another's on a shared network.
    bool Start(const std::string& scope);

    // Told to us once EOS_Connect_Login has resolved the local player. The
    // transport comes up before login, so this cannot be a Start() argument:
    // asking for the identity that early would resolve it from configuration
    // instead of from the credential the title is about to present.
    void SetLocalPuid(const std::string& puid);
    void SetLocalName(const std::string& name);
    void Stop();

    // Installing the lobby events must not detach a P2P handler that was
    // attached first: the two layers come up in whichever order the title
    // happens to touch them.
    void SetEvents(DirectoryEvents ev) {
        auto p2p = m_events.OnP2PPacket;
        m_events = std::move(ev);
        if (!m_events.OnP2PPacket) m_events.OnP2PPacket = std::move(p2p);
    }

    // The P2P layer attaches separately so lobby and game traffic can be wired
    // up independently, in whichever order the title happens to touch them.
    void SetP2PHandler(decltype(DirectoryEvents::OnP2PPacket) fn) { m_events.OnP2PPacket = std::move(fn); }

    // --- host side --------------------------------------------------------
    std::string CreateLobby(LobbyRecord record);          // returns the lobby id
    bool        UpdateLobby(const LobbyRecord& record);   // replaces + re-announces
    bool        DestroyLobby(const std::string& lobbyId);
    bool        KickMember(const std::string& lobbyId, const std::string& puid);
    bool        PromoteMember(const std::string& lobbyId, const std::string& puid);

    // --- client side ------------------------------------------------------
    void        RequestSearch();                          // ask hosts to re-announce
    std::vector<LobbyRecord> Snapshot() const;            // everything currently known
    bool        Join(const LobbyRecord& target, const LobbyMember& self);
    bool        Leave(const std::string& lobbyId, const std::string& puid);
    bool        SendInvite(const std::string& lobbyId, const std::string& targetPuid);

    bool        Get(const std::string& lobbyId, LobbyRecord& out) const;
    bool        IsHosting(const std::string& lobbyId) const;

    // --- peer addressing --------------------------------------------------
    PeerRef     AddressOf(const std::string& puid) const;
    void        RememberAddress(const std::string& puid, const PeerRef& peer);
    bool        SendP2P(const std::string& toPuid, const std::string& socketName,
                        uint8_t channel, const void* data, size_t len);

    Endpoint    LocalEndpoint() const { return Transport::Get().LocalEndpoint(); }

private:
    LobbyDirectory() = default;
    void OnDatagram(const PeerRef& from, const uint8_t* data, size_t len);
    void EnsureSteam();
    bool SendToPeer(const PeerRef& to, const void* data, size_t len);
    void InstallSteamEvents();
    void MaintenanceLoop();
    void Announce(const LobbyRecord& record, const PeerRef* to = nullptr);
    void PublishUpdate(const LobbyRecord& record);
    void BeginMessage(Writer& w, uint8_t type) const;

    mutable std::mutex                     m_mutex;
    std::string                            m_scope;
    std::string                            m_localPuid;
    std::string                            m_localName;
    bool                                   m_warnedDuplicateIdentity = false;
    std::map<std::string, LobbyRecord>     m_hosted;
    std::map<std::string, LobbyRecord>     m_known;
    std::map<std::string, PeerRef>         m_addresses;
    DirectoryEvents                        m_events;
    std::atomic<bool>                      m_running{false};
    std::thread                            m_maintenance;
    uint64_t                               m_lastAnnounceMs = 0;
    uint64_t                               m_lastSteamListMs = 0;
};

} // namespace refix
