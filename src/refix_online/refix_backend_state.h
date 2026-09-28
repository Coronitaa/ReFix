// =============================================================================
// ReFix Online v2 - Authoritative In-Memory Backend & Room State Server
// =============================================================================
#pragma once

#include "refix_backend_protocol.h"
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>

namespace ReFixOnline {

enum class ELobbyState : int32_t {
    NONEXISTENT = 0,
    CREATING    = 1,
    ACTIVE      = 2,
    DESTROYING  = 3,
    DESTROYED   = 4
};

enum class EMemberState : int32_t {
    NOT_MEMBER = 0,
    JOINING    = 1,
    MEMBER     = 2,
    LEAVING    = 3,
    LEFT       = 4
};

class BackendServerState {
public:
    static BackendServerState& Get();

    BackendServerState();
    ~BackendServerState() = default;

    void Reset();

    // Client Session Management
    EBackendResult AuthenticateSession(const std::string& userId, const std::string& displayName, std::string& outSessionToken);
    bool ValidateSession(const std::string& userId, const std::string& sessionToken);

    EBackendResult CreateLobby(const std::string& userId, uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes, LobbyData& outLobby);
    EBackendResult UpdateLobby(const std::string& userId, const std::string& lobbyId, const std::unordered_map<std::string, std::string>& attributes, uint32_t maxMembers, const std::string& bucketId, int32_t permissionLevel, bool invitesAllowed, LobbyData& outLobby);
    EBackendResult GetLobby(const std::string& lobbyId, LobbyData& outLobby);
    EBackendResult FindLobbies(const std::string& userId, uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters, std::vector<LobbyData>& outLobbies);
    EBackendResult JoinLobby(const std::string& userId, const std::string& displayName, const std::string& lobbyId, LobbyData& outLobby);
    EBackendResult LeaveLobby(const std::string& userId, const std::string& lobbyId, std::string& outNewOwnerUserId);
    EBackendResult DestroyLobby(const std::string& userId, const std::string& lobbyId);
    EBackendResult ResyncLobby(const std::string& userId, const std::string& lobbyId, LobbyData& outLobby);

    // Heartbeat & Timeout Management
    void UpdateHeartbeat(const std::string& userId);
    std::vector<std::pair<std::string, std::string>> CheckHeartbeatTimeouts(uint64_t timeoutMs = 15000); // returns {userId, lobbyId}

    // Diagnostics
    size_t GetActiveLobbiesCount();
    size_t GetActiveSessionsCount();

private:
    std::string GenerateUniqueLobbyId();
    uint64_t GetCurrentTimeMs();

    std::recursive_mutex m_mutex;

    struct SessionInfo {
        std::string userId;
        std::string displayName;
        std::string sessionToken;
        uint64_t lastHeartbeat;
        std::string activeLobbyId;
    };

    std::unordered_map<std::string, SessionInfo> m_sessions; // userId -> SessionInfo
    std::unordered_map<std::string, LobbyData> m_lobbies;     // lobbyId -> LobbyData
    std::unordered_map<std::string, ELobbyState> m_lobbyStates;
};

} // namespace ReFixOnline
