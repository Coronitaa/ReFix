// =============================================================================
// ReFix Online v2 - Authoritative In-Memory Backend Implementation
// =============================================================================
#include "refix_backend_state.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ReFixOnline {

BackendServerState& BackendServerState::Get() {
    static BackendServerState s_instance;
    return s_instance;
}

BackendServerState::BackendServerState() {}

void BackendServerState::Reset() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_sessions.clear();
    m_lobbies.clear();
    m_lobbyStates.clear();
}

uint64_t BackendServerState::GetCurrentTimeMs() {
    return (uint64_t)GetTickCount64();
}

std::string BackendServerState::GenerateUniqueLobbyId() {
    GUID guid;
    if (CoCreateGuid(&guid) == S_OK) {
        char buf[64];
        sprintf_s(buf, "%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return std::string(buf);
    }
    // Fallback pseudo-random
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << GetCurrentTimeMs()
       << std::setw(16) << std::setfill('0') << rand();
    return ss.str();
}

EBackendResult BackendServerState::AuthenticateSession(const std::string& userId, const std::string& displayName, std::string& outSessionToken) {
    if (userId.empty()) return INVALID_USER;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::string token = "sess_" + GenerateUniqueLobbyId();
    outSessionToken = token;

    SessionInfo sess;
    sess.userId = userId;
    sess.displayName = displayName.empty() ? "Player" : displayName;
    sess.sessionToken = token;
    sess.lastHeartbeat = GetCurrentTimeMs();
    sess.activeLobbyId = "";

    m_sessions[userId] = sess;
    return SUCCESS;
}

bool BackendServerState::ValidateSession(const std::string& userId, const std::string& sessionToken) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_sessions.find(userId);
    if (it == m_sessions.end()) return false;
    return (it->second.sessionToken == sessionToken);
}

EBackendResult BackendServerState::CreateLobby(const std::string& userId, uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes, LobbyData& outLobby) {
    if (userId.empty()) return NOT_AUTHENTICATED;
    if (maxMembers == 0 || maxMembers > MAX_LOBBY_MEMBERS) maxMembers = 4;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto sessIt = m_sessions.find(userId);
    if (sessIt == m_sessions.end()) return NOT_AUTHENTICATED;

    std::string lobbyId = "lob_" + GenerateUniqueLobbyId();

    LobbyData lob;
    lob.lobbyId = lobbyId;
    lob.ownerUserId = userId;
    lob.maxMembers = maxMembers;
    lob.currentMembers = 1;
    lob.attributes = attributes;
    lob.createdAt = GetCurrentTimeMs();
    lob.lastHeartbeat = lob.createdAt;
    lob.state = (int32_t)ELobbyState::ACTIVE;

    LobbyMemberInfo ownerMember;
    ownerMember.userId = userId;
    ownerMember.displayName = sessIt->second.displayName;
    ownerMember.joinTime = lob.createdAt;
    ownerMember.isOwner = true;
    lob.members.push_back(ownerMember);

    m_lobbies[lobbyId] = lob;
    m_lobbyStates[lobbyId] = ELobbyState::ACTIVE;
    sessIt->second.activeLobbyId = lobbyId;

    outLobby = lob;
    return SUCCESS;
}

EBackendResult BackendServerState::UpdateLobby(const std::string& userId, const std::string& lobbyId, const std::unordered_map<std::string, std::string>& attributes, uint32_t maxMembers, const std::string& bucketId, int32_t permissionLevel, bool invitesAllowed, LobbyData& outLobby) {
    if (userId.empty() || lobbyId.empty()) return LOBBY_NOT_FOUND;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto lobIt = m_lobbies.find(lobbyId);
    if (lobIt == m_lobbies.end() || lobIt->second.state != (int32_t)ELobbyState::ACTIVE) {
        return LOBBY_NOT_FOUND;
    }
    auto& lob = lobIt->second;
    if (lob.ownerUserId != userId) {
        return NOT_OWNER;
    }
    for (const auto& kv : attributes) {
        lob.attributes[kv.first] = kv.second;
    }
    if (maxMembers > 0 && maxMembers <= MAX_LOBBY_MEMBERS) {
        lob.maxMembers = maxMembers;
    }
    if (!bucketId.empty()) {
        lob.attributes["bucket_id"] = "s:" + bucketId;
    }
    lob.attributes["permission_level"] = "i:" + std::to_string(permissionLevel);
    lob.attributes["invites_allowed"] = invitesAllowed ? "b:1" : "b:0";
    lob.lastHeartbeat = GetCurrentTimeMs();
    outLobby = lob;
    return SUCCESS;
}

EBackendResult BackendServerState::GetLobby(const std::string& lobbyId, LobbyData& outLobby) {
    if (lobbyId.empty()) return LOBBY_NOT_FOUND;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto lobIt = m_lobbies.find(lobbyId);
    if (lobIt == m_lobbies.end() || lobIt->second.state != (int32_t)ELobbyState::ACTIVE) {
        return LOBBY_NOT_FOUND;
    }
    outLobby = lobIt->second;
    return SUCCESS;
}

EBackendResult BackendServerState::FindLobbies(const std::string& userId, uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters, std::vector<LobbyData>& outLobbies) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    outLobbies.clear();

    if (maxResults == 0 || maxResults > 100) maxResults = 50;

    for (const auto& kv : m_lobbies) {
        if (outLobbies.size() >= maxResults) break;
        const auto& lob = kv.second;
        if (lob.state != (int32_t)ELobbyState::ACTIVE) continue;

        // Apply attribute filters
        bool match = true;
        for (const auto& f : filters) {
            auto attrIt = lob.attributes.find(f.first);
            if (attrIt == lob.attributes.end() || attrIt->second != f.second) {
                match = false;
                break;
            }
        }
        if (match) {
            outLobbies.push_back(lob);
        }
    }
    return SUCCESS;
}

EBackendResult BackendServerState::JoinLobby(const std::string& userId, const std::string& displayName, const std::string& lobbyId, LobbyData& outLobby) {
    if (userId.empty()) return INVALID_USER;
    if (lobbyId.empty()) return LOBBY_NOT_FOUND;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto lobIt = m_lobbies.find(lobbyId);
    if (lobIt == m_lobbies.end() || lobIt->second.state != (int32_t)ELobbyState::ACTIVE) {
        return LOBBY_NOT_FOUND;
    }

    auto& lob = lobIt->second;

    // Check already member
    for (const auto& m : lob.members) {
        if (m.userId == userId) {
            outLobby = lob;
            return ALREADY_MEMBER;
        }
    }

    // Check capacity
    if (lob.members.size() >= lob.maxMembers) {
        return LOBBY_FULL;
    }

    LobbyMemberInfo newMember;
    newMember.userId = userId;
    newMember.displayName = displayName.empty() ? "Player" : displayName;
    newMember.joinTime = GetCurrentTimeMs();
    newMember.isOwner = false;

    lob.members.push_back(newMember);
    lob.currentMembers = (uint32_t)lob.members.size();
    lob.lastHeartbeat = GetCurrentTimeMs();

    if (m_sessions.find(userId) != m_sessions.end()) {
        m_sessions[userId].activeLobbyId = lobbyId;
    }

    outLobby = lob;
    return SUCCESS;
}

EBackendResult BackendServerState::LeaveLobby(const std::string& userId, const std::string& lobbyId, std::string& outNewOwnerUserId) {
    if (userId.empty() || lobbyId.empty()) return LOBBY_NOT_FOUND;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    outNewOwnerUserId.clear();

    auto lobIt = m_lobbies.find(lobbyId);
    if (lobIt == m_lobbies.end()) return LOBBY_NOT_FOUND;

    auto& lob = lobIt->second;
    bool wasOwner = (lob.ownerUserId == userId);

    auto it = std::remove_if(lob.members.begin(), lob.members.end(), [&userId](const LobbyMemberInfo& m) {
        return m.userId == userId;
    });

    if (it == lob.members.end()) return NOT_MEMBER;
    lob.members.erase(it, lob.members.end());
    lob.currentMembers = (uint32_t)lob.members.size();

    if (m_sessions.find(userId) != m_sessions.end()) {
        m_sessions[userId].activeLobbyId = "";
    }

    // If lobby is empty, destroy it
    if (lob.members.empty()) {
        lob.state = (int32_t)ELobbyState::DESTROYED;
        m_lobbies.erase(lobbyId);
        m_lobbyStates[lobbyId] = ELobbyState::DESTROYED;
        return SUCCESS;
    }

    // If owner left, apply ownership transfer to oldest remaining member
    if (wasOwner) {
        lob.members[0].isOwner = true;
        lob.ownerUserId = lob.members[0].userId;
        outNewOwnerUserId = lob.ownerUserId;
    }

    return SUCCESS;
}

EBackendResult BackendServerState::DestroyLobby(const std::string& userId, const std::string& lobbyId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto lobIt = m_lobbies.find(lobbyId);
    if (lobIt == m_lobbies.end()) return LOBBY_NOT_FOUND;

    if (lobIt->second.ownerUserId != userId) {
        return NOT_OWNER;
    }

    lobIt->second.state = (int32_t)ELobbyState::DESTROYED;
    m_lobbies.erase(lobbyId);
    m_lobbyStates[lobbyId] = ELobbyState::DESTROYED;
    return SUCCESS;
}

EBackendResult BackendServerState::ResyncLobby(const std::string& userId, const std::string& lobbyId, LobbyData& outLobby) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto lobIt = m_lobbies.find(lobbyId);
    if (lobIt == m_lobbies.end() || lobIt->second.state != (int32_t)ELobbyState::ACTIVE) {
        return LOBBY_NOT_FOUND;
    }

    outLobby = lobIt->second;
    return SUCCESS;
}

void BackendServerState::UpdateHeartbeat(const std::string& userId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_sessions.find(userId);
    if (it != m_sessions.end()) {
        it->second.lastHeartbeat = GetCurrentTimeMs();
    }
}

std::vector<std::pair<std::string, std::string>> BackendServerState::CheckHeartbeatTimeouts(uint64_t timeoutMs) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<std::pair<std::string, std::string>> timedOut;
    uint64_t now = GetCurrentTimeMs();

    for (auto& kv : m_sessions) {
        if (now - kv.second.lastHeartbeat > timeoutMs) {
            if (!kv.second.activeLobbyId.empty()) {
                timedOut.push_back({ kv.first, kv.second.activeLobbyId });
            }
        }
    }

    for (const auto& item : timedOut) {
        std::string newOwner;
        LeaveLobby(item.first, item.second, newOwner);
    }
    return timedOut;
}

size_t BackendServerState::GetActiveLobbiesCount() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_lobbies.size();
}

size_t BackendServerState::GetActiveSessionsCount() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_sessions.size();
}

} // namespace ReFixOnline
