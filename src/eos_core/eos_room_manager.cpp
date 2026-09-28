#include "../eos/eos_connect.h"
// =============================================================================
// ReFix EOS Online v2 - EOS Room Bridge Implementation
// =============================================================================
#include "eos_room_manager.h"
#include "../eos/eos_identity.h"

namespace ReFixEOS {

RoomManagerBridge& RoomManagerBridge::Get() {
    static RoomManagerBridge s_instance;
    return s_instance;
}

RoomManagerBridge::RoomManagerBridge() {
    Initialize();
}

void RoomManagerBridge::Initialize() {
    auto transport = std::make_unique<ReFixOnline::InProcessDirectTransport>(m_serverState);
    m_client = std::make_unique<ReFixOnline::BackendClient>(std::move(transport));
    m_client->Connect("127.0.0.1", 47584);

    std::string localPuid = IdentityManager::Get().GetLocalProductUserIdString();
    std::string dispName = IdentityManager::Get().GetLocalDisplayName();
    if (!localPuid.empty()) {
        m_client->Authenticate(localPuid, dispName, nullptr);
        m_client->Tick();
    }
}

void RoomManagerBridge::Reset() {
    m_serverState.Reset();
    Initialize();
}

void RoomManagerBridge::Shutdown() {
    if (m_client) {
        m_client->Disconnect();
    }
}

void RoomManagerBridge::Tick() {
    if (m_client) {
        m_client->Tick();
    }
}

void RoomManagerBridge::Authenticate(const std::string& userId, const std::string& displayName, std::function<void(ReFixOnline::EBackendResult, const std::string&)> callback) {
    if (!m_client) return;
    m_client->Authenticate(userId, displayName, callback);
    m_client->Tick();
}

void RoomManagerBridge::EnsureAuthenticated() {
    if (!m_client) return;
    if (m_client->GetConnectionState() != ReFixOnline::EClientConnectionState::AUTHENTICATED) {
        std::string localPuid = IdentityManager::Get().GetLocalProductUserIdString();
        std::string dispName = IdentityManager::Get().GetLocalDisplayName();
        if (!localPuid.empty()) {
            m_client->Authenticate(localPuid, dispName, nullptr);
            m_client->Tick();
        }
    }
}

void RoomManagerBridge::CreateLobby(uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback) {
    if (!m_client) {
        if (callback) callback(ReFixOnline::SERVER_ERROR, {});
        return;
    }
    EnsureAuthenticated();
    if (m_client->GetConnectionState() != ReFixOnline::EClientConnectionState::AUTHENTICATED) {
        LogDiagnostic("[RFIX_BACKEND] CreateLobby rejected: BackendClient not authenticated (state=%d)", (int)m_client->GetConnectionState());
        if (callback) callback(ReFixOnline::NOT_AUTHENTICATED, {});
        return;
    }
    m_client->CreateLobby(maxMembers, attributes, callback);
}

void RoomManagerBridge::FindLobbies(uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters, std::function<void(ReFixOnline::EBackendResult, const std::vector<ReFixOnline::LobbyData>&)> callback) {
    if (!m_client) {
        if (callback) callback(ReFixOnline::SERVER_ERROR, {});
        return;
    }
    EnsureAuthenticated();
    m_client->FindLobbies(maxResults, filters, callback);
}

void RoomManagerBridge::JoinLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback) {
    if (!m_client) {
        if (callback) callback(ReFixOnline::SERVER_ERROR, {});
        return;
    }
    EnsureAuthenticated();
    m_client->JoinLobby(lobbyId, callback);
}

void RoomManagerBridge::LeaveLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const std::string&)> callback) {
    if (!m_client) {
        if (callback) callback(ReFixOnline::SERVER_ERROR, "");
        return;
    }
    EnsureAuthenticated();
    m_client->LeaveLobby(lobbyId, callback);
}

void RoomManagerBridge::DestroyLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const std::string&)> callback) {
    if (!m_client) {
        if (callback) callback(ReFixOnline::SERVER_ERROR, "");
        return;
    }
    EnsureAuthenticated();
    std::string puid = IdentityManager::Get().GetLocalProductUserIdString();
    ReFixOnline::EBackendResult res = m_serverState.DestroyLobby(puid, lobbyId);
    if (callback) callback(res, lobbyId);
}

void RoomManagerBridge::ResyncLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback) {
    if (!m_client) {
        if (callback) callback(ReFixOnline::SERVER_ERROR, {});
        return;
    }
    EnsureAuthenticated();
    m_client->ResyncLobby(lobbyId, callback);
}

bool RoomManagerBridge::GetLobby(const std::string& lobbyId, ReFixOnline::LobbyData& outLobby) {
    return (m_serverState.GetLobby(lobbyId, outLobby) == ReFixOnline::SUCCESS);
}

} // namespace ReFixEOS
