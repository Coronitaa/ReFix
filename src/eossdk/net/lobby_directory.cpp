#include "lobby_directory.h"
#include "../core/refix_config.h"
#include "../core/refix_log.h"
#include "../core/refix_hash.h"
#include <chrono>

namespace refix {
namespace {

// 'RFX3' - datagrams that do not start with this are not ours and are dropped
// before anything else is parsed.
const uint32_t kMagic = 0x33584652u;

enum MsgType : uint8_t {
    MSG_ANNOUNCE   = 1,   // host -> everyone: here is a lobby I own
    MSG_WITHDRAW   = 2,   // host -> everyone: that lobby is gone
    MSG_QUERY      = 3,   // searcher -> everyone: please re-announce
    MSG_JOIN_REQ   = 4,   // client -> host
    MSG_JOIN_RSP   = 5,   // host -> client
    MSG_LEAVE      = 6,   // client -> host
    MSG_UPDATE     = 7,   // host -> members: authoritative new state
    MSG_KICK       = 8,   // host -> client
    MSG_INVITE     = 9,   // member -> target
    MSG_P2P        = 10,  // peer -> peer, game traffic
};

// How long a lobby stays in the cache after its last advertisement. Long enough
// to survive a dropped datagram, short enough that a closed lobby disappears
// from the browser promptly.
const uint64_t kLobbyTtlMs      = 12000;
const uint64_t kAnnounceEveryMs = 2000;

uint64_t NowMillis() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string NewLobbyId(const std::string& ownerPuid) {
    // Unique without a coordinator, and stable enough to log: owner + time.
    char buf[128];
    snprintf(buf, sizeof(buf), "%s|%llu|%lu", ownerPuid.c_str(),
             (unsigned long long)NowMillis(), (unsigned long)GetCurrentProcessId());
    return "rfx" + DerivedId(buf, 12);
}

} // namespace

LobbyDirectory& LobbyDirectory::Get() { static LobbyDirectory d; return d; }

void LobbyDirectory::BeginMessage(Writer& w, uint8_t type) const {
    w.U32(kMagic);
    w.U8(type);
    w.Str(m_scope);
}

void LobbyDirectory::SetLocalPuid(const std::string& puid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_localPuid = puid;
}

bool LobbyDirectory::Start(const std::string& scope) {
    if (m_running) return true;
    m_scope = scope;
    if (!Transport::Get().Start()) {
        RFLOG(Net, "LobbyDirectory: transport unavailable; online play is disabled");
        return false;
    }
    Transport::Get().SetHandler([this](const Endpoint& from, const uint8_t* d, size_t n) {
        OnDatagram(from, d, n);
    });
    m_running = true;
    m_maintenance = std::thread(&LobbyDirectory::MaintenanceLoop, this);
    RFLOG(Net, "LobbyDirectory up for scope '%s' at %s",
          m_scope.c_str(), Transport::Get().LocalEndpoint().ToString().c_str());
    return true;
}

void LobbyDirectory::Stop() {
    if (!m_running) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& kv : m_hosted) {
            Writer w;
            BeginMessage(w, MSG_WITHDRAW);
            w.Str(kv.first);
            Transport::Get().SendToGroup(w.Data().data(), w.Size());
        }
        m_hosted.clear();
    }
    m_running = false;
    if (m_maintenance.joinable()) m_maintenance.join();
    Transport::Get().Stop();
}

// ---------------------------------------------------------------------------
// Host side
// ---------------------------------------------------------------------------
std::string LobbyDirectory::CreateLobby(LobbyRecord record) {
    if (record.LobbyId.empty()) record.LobbyId = NewLobbyId(record.OwnerPuid);
    record.HostAddress = Transport::Get().LocalEndpoint();
    record.Revision    = 1;
    record.LastSeenMs  = NowMillis();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_hosted[record.LobbyId] = record;
    }
    RFLOG(Lobby, "hosting lobby %s owner=%s bucket='%s' maxMembers=%u members=%zu",
          record.LobbyId.c_str(), record.OwnerPuid.c_str(), record.BucketId.c_str(),
          record.MaxMembers, record.Members.size());
    Announce(record);
    return record.LobbyId;
}

bool LobbyDirectory::UpdateLobby(const LobbyRecord& record) {
    LobbyRecord updated;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_hosted.find(record.LobbyId);
        if (it == m_hosted.end()) return false;
        updated = record;
        updated.HostAddress = Transport::Get().LocalEndpoint();
        updated.Revision    = it->second.Revision + 1;
        updated.LastSeenMs  = NowMillis();
        it->second = updated;
    }
    PublishUpdate(updated);
    return true;
}

bool LobbyDirectory::DestroyLobby(const std::string& lobbyId) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_hosted.erase(lobbyId)) return false;
        m_known.erase(lobbyId);
    }
    Writer w;
    BeginMessage(w, MSG_WITHDRAW);
    w.Str(lobbyId);
    Transport::Get().SendToGroup(w.Data().data(), w.Size());
    RFLOG(Lobby, "destroyed lobby %s", lobbyId.c_str());
    return true;
}

bool LobbyDirectory::KickMember(const std::string& lobbyId, const std::string& puid) {
    LobbyRecord updated;
    Endpoint victim;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_hosted.find(lobbyId);
        if (it == m_hosted.end()) return false;
        auto& members = it->second.Members;
        bool found = false;
        for (size_t i = 0; i < members.size(); i++) {
            if (members[i].Puid == puid) { victim = members[i].Address; members.erase(members.begin() + i); found = true; break; }
        }
        if (!found) return false;
        it->second.Revision++;
        updated = it->second;
    }
    Writer w;
    BeginMessage(w, MSG_KICK);
    w.Str(lobbyId);
    w.Str(puid);
    if (victim.Valid()) Transport::Get().SendTo(victim, w.Data().data(), w.Size());
    PublishUpdate(updated);
    if (m_events.OnMemberChanged) m_events.OnMemberChanged(updated, puid, MemberChange::Kicked);
    return true;
}

bool LobbyDirectory::PromoteMember(const std::string& lobbyId, const std::string& puid) {
    LobbyRecord updated;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_hosted.find(lobbyId);
        if (it == m_hosted.end()) return false;
        if (!it->second.FindMember(puid)) return false;
        it->second.OwnerPuid = puid;
        for (auto& m : it->second.Members) m.IsOwner = (m.Puid == puid);
        it->second.Revision++;
        updated = it->second;
    }
    PublishUpdate(updated);
    if (m_events.OnMemberChanged) m_events.OnMemberChanged(updated, puid, MemberChange::Promoted);
    return true;
}

// ---------------------------------------------------------------------------
// Client side
// ---------------------------------------------------------------------------
void LobbyDirectory::RequestSearch() {
    Writer w;
    BeginMessage(w, MSG_QUERY);
    Transport::Get().SendToGroup(w.Data().data(), w.Size());
}

std::vector<LobbyRecord> LobbyDirectory::Snapshot() const {
    std::vector<LobbyRecord> out;
    std::lock_guard<std::mutex> lock(m_mutex);
    out.reserve(m_hosted.size() + m_known.size());
    for (const auto& kv : m_hosted) out.push_back(kv.second);
    for (const auto& kv : m_known)  if (!m_hosted.count(kv.first)) out.push_back(kv.second);
    return out;
}

bool LobbyDirectory::Join(const LobbyRecord& target, const LobbyMember& self) {
    if (!target.HostAddress.Valid()) {
        RFLOG(Lobby, "join %s: host address unknown", target.LobbyId.c_str());
        return false;
    }
    Writer w;
    BeginMessage(w, MSG_JOIN_REQ);
    w.Str(target.LobbyId);
    w.Str(self.Puid);
    w.Str(self.DisplayName);
    w.U32(Transport::Get().LocalEndpoint().Ipv4);
    w.U16(Transport::Get().LocalEndpoint().Port);
    RFLOG(Lobby, "join request for %s -> host %s",
          target.LobbyId.c_str(), target.HostAddress.ToString().c_str());
    return Transport::Get().SendTo(target.HostAddress, w.Data().data(), w.Size());
}

bool LobbyDirectory::Leave(const std::string& lobbyId, const std::string& puid) {
    LobbyRecord record;
    if (!Get(lobbyId, record)) return false;

    if (IsHosting(lobbyId)) return DestroyLobby(lobbyId);

    Writer w;
    BeginMessage(w, MSG_LEAVE);
    w.Str(lobbyId);
    w.Str(puid);
    Transport::Get().SendTo(record.HostAddress, w.Data().data(), w.Size());
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_known.erase(lobbyId);
    }
    return true;
}

bool LobbyDirectory::SendInvite(const std::string& lobbyId, const std::string& targetPuid) {
    Endpoint to = AddressOf(targetPuid);
    if (!to.Valid()) {
        RFLOG(Lobby, "invite to %s: no known address", targetPuid.c_str());
        return false;
    }
    LobbyRecord record;
    if (!Get(lobbyId, record)) return false;

    Writer w;
    BeginMessage(w, MSG_INVITE);
    w.Str(lobbyId);
    w.Str(record.OwnerPuid);
    w.Str("inv" + DerivedId(lobbyId + targetPuid + std::to_string(NowMillis()), 8));
    record.Write(w);
    return Transport::Get().SendTo(to, w.Data().data(), w.Size());
}

bool LobbyDirectory::Get(const std::string& lobbyId, LobbyRecord& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto h = m_hosted.find(lobbyId);
    if (h != m_hosted.end()) { out = h->second; return true; }
    auto k = m_known.find(lobbyId);
    if (k != m_known.end()) { out = k->second; return true; }
    return false;
}

bool LobbyDirectory::IsHosting(const std::string& lobbyId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hosted.count(lobbyId) != 0;
}

// ---------------------------------------------------------------------------
// Peer addressing and game traffic
// ---------------------------------------------------------------------------
Endpoint LobbyDirectory::AddressOf(const std::string& puid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_addresses.find(puid);
    return it == m_addresses.end() ? Endpoint{} : it->second;
}

void LobbyDirectory::RememberAddress(const std::string& puid, const Endpoint& ep) {
    if (puid.empty() || !ep.Valid()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_addresses[puid] = ep;
}

bool LobbyDirectory::SendP2P(const std::string& toPuid, const std::string& socketName,
                             uint8_t channel, const void* data, size_t len) {
    Endpoint to = AddressOf(toPuid);
    if (!to.Valid()) return false;
    Writer w;
    BeginMessage(w, MSG_P2P);
    w.Str(socketName);
    w.U8(channel);
    w.Bytes(data, len);
    return Transport::Get().SendTo(to, w.Data().data(), w.Size());
}

// ---------------------------------------------------------------------------
// Announcements and maintenance
// ---------------------------------------------------------------------------
void LobbyDirectory::Announce(const LobbyRecord& record, const Endpoint* to) {
    // Private lobbies are not advertised; they are reachable by invite or id.
    if (!to && record.Permission == EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY) return;

    Writer w;
    BeginMessage(w, MSG_ANNOUNCE);
    record.Write(w);
    if (to) Transport::Get().SendTo(*to, w.Data().data(), w.Size());
    else    Transport::Get().SendToGroup(w.Data().data(), w.Size());
}

void LobbyDirectory::PublishUpdate(const LobbyRecord& record) {
    Writer w;
    BeginMessage(w, MSG_UPDATE);
    record.Write(w);
    // Members are told directly so an update never depends on multicast
    // reaching them, and the group copy keeps lobby browsers current.
    for (const auto& m : record.Members)
        if (m.Address.Valid()) Transport::Get().SendTo(m.Address, w.Data().data(), w.Size());
    Transport::Get().SendToGroup(w.Data().data(), w.Size());

    if (m_events.OnLobbyUpdated) m_events.OnLobbyUpdated(record);
}

void LobbyDirectory::MaintenanceLoop() {
    while (m_running) {
        std::vector<LobbyRecord> toAnnounce;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            uint64_t now = NowMillis();
            if (now - m_lastAnnounceMs >= kAnnounceEveryMs) {
                m_lastAnnounceMs = now;
                for (const auto& kv : m_hosted) toAnnounce.push_back(kv.second);
            }
            for (auto it = m_known.begin(); it != m_known.end(); ) {
                if (now - it->second.LastSeenMs > kLobbyTtlMs) it = m_known.erase(it);
                else ++it;
            }
        }
        for (const auto& r : toAnnounce) Announce(r);
        Sleep(250);
    }
}

// ---------------------------------------------------------------------------
// Receive path. Everything below runs on the transport thread and treats the
// datagram as untrusted input.
// ---------------------------------------------------------------------------
void LobbyDirectory::OnDatagram(const Endpoint& from, const uint8_t* data, size_t len) {
    Reader r(data, len);
    if (r.U32() != kMagic) return;
    const uint8_t type = r.U8();
    const std::string scope = r.Str(256);
    if (!r.Ok() || scope != m_scope) return;

    switch (type) {
    case MSG_ANNOUNCE:
    case MSG_UPDATE: {
        LobbyRecord record;
        if (!LobbyRecord::Read(r, record)) return;
        // Never let a remote advertisement overwrite a lobby we own.
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_hosted.count(record.LobbyId)) return;
            // Somebody else is advertising a lobby owned by *our* id. The two
            // installs will fight over every packet addressed to that id, so
            // say so plainly rather than letting it fail mysteriously later.
            if (!m_localPuid.empty() && record.OwnerPuid == m_localPuid &&
                !(record.HostAddress == Transport::Get().LocalEndpoint())) {
                if (!m_warnedDuplicateIdentity) {
                    m_warnedDuplicateIdentity = true;
                    RFLOG(Lobby, "DUPLICATE IDENTITY: %s at %s is advertising a lobby owned by "
                                 "our own ProductUserId. Set a different [User] Instance "
                                 "(or REFIX_USER_INSTANCE) on one of them.",
                          record.OwnerPuid.c_str(), record.HostAddress.ToString().c_str());
                }
                return;
            }
            record.LastSeenMs = NowMillis();
            if (!record.HostAddress.Valid()) record.HostAddress = from;
            auto it = m_known.find(record.LobbyId);
            if (it != m_known.end() && it->second.Revision > record.Revision) {
                it->second.LastSeenMs = record.LastSeenMs;   // stale copy, keep ours
                return;
            }
            for (const auto& m : record.Members)
                if (m.Address.Valid()) m_addresses[m.Puid] = m.Address;
            m_addresses[record.OwnerPuid] = record.HostAddress;
            m_known[record.LobbyId] = record;
        }
        if (type == MSG_UPDATE && m_events.OnLobbyUpdated) m_events.OnLobbyUpdated(record);
        break;
    }

    case MSG_WITHDRAW: {
        std::string lobbyId = r.Str(256);
        if (!r.Ok()) return;
        LobbyRecord gone;
        bool had = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_known.find(lobbyId);
            if (it != m_known.end()) { gone = it->second; had = true; m_known.erase(it); }
        }
        if (had && m_events.OnMemberChanged)
            m_events.OnMemberChanged(gone, gone.OwnerPuid, MemberChange::Closed);
        break;
    }

    case MSG_QUERY: {
        std::vector<LobbyRecord> mine;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& kv : m_hosted) mine.push_back(kv.second);
        }
        // Answer directly as well as on the group: a searcher that just started
        // gets its results without waiting for the next periodic announcement.
        for (const auto& record : mine) { Announce(record, &from); Announce(record); }
        break;
    }

    case MSG_JOIN_REQ: {
        std::string lobbyId = r.Str(256);
        LobbyMember joiner;
        joiner.Puid        = r.Str(64);
        joiner.DisplayName = r.Str(256);
        joiner.Address.Ipv4 = r.U32();
        joiner.Address.Port = r.U16();
        if (!r.Ok() || joiner.Puid.empty()) return;
        // Trust the source address over anything the datagram claims.
        joiner.Address = from;

        ER result = ER::EOS_NotFound;
        LobbyRecord updated;
        bool isNew = false;
        bool duplicateIdentity = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_hosted.find(lobbyId);
            const bool fromElsewhere = !(from == Transport::Get().LocalEndpoint());

            if (it == m_hosted.end()) {
                result = ER::EOS_NotFound;
            } else if (fromElsewhere && joiner.Puid == it->second.OwnerPuid) {
                // A different process is presenting the host's own identity.
                // That happens when two copies of the game run on one machine
                // under the same account: both derive the same ProductUserId.
                // Accepting it would silently produce a one-member lobby that
                // the second player can never appear in, so refuse loudly.
                result = ER::EOS_DuplicateNotAllowed;
                duplicateIdentity = true;
            } else if (it->second.Permission == EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY) {
                result = ER::EOS_Lobby_NotAllowed;
            } else if (const LobbyMember* existing = it->second.FindMember(joiner.Puid)) {
                if (fromElsewhere && existing->Address.Valid() && !(existing->Address == from)) {
                    // Same id, different machine: also a duplicate identity.
                    result = ER::EOS_DuplicateNotAllowed;
                    duplicateIdentity = true;
                } else {
                    result = ER::EOS_Success;             // genuine re-join
                    updated = it->second;
                }
            } else if (it->second.AvailableSlots() == 0) {
                result = ER::EOS_Lobby_TooManyPlayers;
            } else {
                it->second.Members.push_back(joiner);
                it->second.Revision++;
                updated = it->second;
                result = ER::EOS_Success;
                isNew = true;
            }
            // Never let a joiner's address replace the host's own entry.
            if (result == ER::EOS_Success && joiner.Puid != it->second.OwnerPuid)
                m_addresses[joiner.Puid] = joiner.Address;
        }

        if (duplicateIdentity) {
            RFLOG(Lobby, "REJECTED join for %s: the joining player presents the same "
                         "ProductUserId as someone already in the lobby (%s). Two instances "
                         "on one PC share a Steam account and therefore an identity - give "
                         "the second one a different [User] Instance (or REFIX_USER_INSTANCE).",
                  lobbyId.c_str(), joiner.Puid.c_str());
        }

        Writer w;
        BeginMessage(w, MSG_JOIN_RSP);
        w.Str(lobbyId);
        w.U32((uint32_t)result);
        if (result == ER::EOS_Success) updated.Write(w);
        Transport::Get().SendTo(from, w.Data().data(), w.Size());

        RFLOG(Lobby, "join request from %s ('%s') for %s -> result=%d",
              joiner.Puid.c_str(), joiner.DisplayName.c_str(), lobbyId.c_str(), (int)result);

        if (isNew) {
            PublishUpdate(updated);
            if (m_events.OnMemberChanged)
                m_events.OnMemberChanged(updated, joiner.Puid, MemberChange::Joined);
        }
        break;
    }

    case MSG_JOIN_RSP: {
        std::string lobbyId = r.Str(256);
        ER result = (ER)r.U32();
        if (!r.Ok()) return;
        LobbyRecord record;
        if (result == ER::EOS_Success) {
            if (!LobbyRecord::Read(r, record)) return;
            record.LastSeenMs = NowMillis();
            if (!record.HostAddress.Valid()) record.HostAddress = from;
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& m : record.Members)
                if (m.Address.Valid()) m_addresses[m.Puid] = m.Address;
            m_addresses[record.OwnerPuid] = record.HostAddress;
            m_known[record.LobbyId] = record;
        }
        RFLOG(Lobby, "join response for %s -> result=%d", lobbyId.c_str(), (int)result);
        if (m_events.OnLobbyUpdated && result == ER::EOS_Success) m_events.OnLobbyUpdated(record);
        break;
    }

    case MSG_LEAVE: {
        std::string lobbyId = r.Str(256);
        std::string puid    = r.Str(64);
        if (!r.Ok()) return;
        LobbyRecord updated;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_hosted.find(lobbyId);
            if (it == m_hosted.end()) return;
            auto& members = it->second.Members;
            for (size_t i = 0; i < members.size(); i++) {
                if (members[i].Puid == puid) { members.erase(members.begin() + i); changed = true; break; }
            }
            if (!changed) return;
            it->second.Revision++;
            updated = it->second;
        }
        RFLOG(Lobby, "%s left %s", puid.c_str(), lobbyId.c_str());
        PublishUpdate(updated);
        if (m_events.OnMemberChanged) m_events.OnMemberChanged(updated, puid, MemberChange::Left);
        break;
    }

    case MSG_KICK: {
        std::string lobbyId = r.Str(256);
        std::string puid    = r.Str(64);
        if (!r.Ok()) return;
        LobbyRecord record;
        bool had = Get(lobbyId, record);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_known.erase(lobbyId);
        }
        if (had && m_events.OnMemberChanged)
            m_events.OnMemberChanged(record, puid, MemberChange::Kicked);
        break;
    }

    case MSG_INVITE: {
        std::string lobbyId  = r.Str(256);
        std::string fromPuid = r.Str(64);
        std::string inviteId = r.Str(64);
        LobbyRecord record;
        if (!LobbyRecord::Read(r, record)) return;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            record.LastSeenMs = NowMillis();
            if (!record.HostAddress.Valid()) record.HostAddress = from;
            m_known[record.LobbyId] = record;
            m_addresses[record.OwnerPuid] = record.HostAddress;
        }
        RFLOG(Lobby, "invite %s to %s from %s", inviteId.c_str(), lobbyId.c_str(), fromPuid.c_str());
        if (m_events.OnInviteReceived) m_events.OnInviteReceived(lobbyId, fromPuid, inviteId);
        break;
    }

    case MSG_P2P: {
        std::string socketName = r.Str(256);
        uint8_t channel = r.U8();
        std::vector<uint8_t> payload = r.Bytes(64 * 1024);
        if (!r.Ok()) return;
        // Identify the sender by the address it announced itself with.
        std::string fromPuid;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& kv : m_addresses) if (kv.second == from) { fromPuid = kv.first; break; }
        }
        if (m_events.OnP2PPacket)
            m_events.OnP2PPacket(fromPuid, socketName, channel, payload.data(), payload.size());
        break;
    }

    default:
        break;
    }
}

} // namespace refix
