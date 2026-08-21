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
    m_client->Authenticate(localPuid, dispName, nullptr);
}

void RoomManagerBridge::Shutdown() {
    if (m_client) {
        m_client->Disconnect();
    }
}

void RoomManagerBridge::CreateLobby(uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback) {
    if (!m_client) return;
    std::string localPuid = IdentityManager::Get().GetLocalProductUserIdString();
    ReFixOnline::LobbyData lob;
    auto res = m_serverState.CreateLobby(localPuid, maxMembers, attributes, lob);
    if (callback) callback(res, lob);
}

void RoomManagerBridge::FindLobbies(uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters, std::function<void(ReFixOnline::EBackendResult, const std::vector<ReFixOnline::LobbyData>&)> callback) {
    if (!m_client) return;
    std::string localPuid = IdentityManager::Get().GetLocalProductUserIdString();
    std::vector<ReFixOnline::LobbyData> lobs;
    auto res = m_serverState.FindLobbies(localPuid, maxResults, filters, lobs);
    if (callback) callback(res, lobs);
}

void RoomManagerBridge::JoinLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback) {
    if (!m_client) return;
    std::string localPuid = IdentityManager::Get().GetLocalProductUserIdString();
    std::string dispName = IdentityManager::Get().GetLocalDisplayName();
    ReFixOnline::LobbyData lob;
    auto res = m_serverState.JoinLobby(localPuid, dispName, lobbyId, lob);
    if (callback) callback(res, lob);
}

void RoomManagerBridge::LeaveLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const std::string&)> callback) {
    if (!m_client) return;
    std::string localPuid = IdentityManager::Get().GetLocalProductUserIdString();
    std::string newOwner;
    auto res = m_serverState.LeaveLobby(localPuid, lobbyId, newOwner);
    if (callback) callback(res, lobbyId);
}

void RoomManagerBridge::ResyncLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback) {
    if (!m_client) return;
    std::string localPuid = IdentityManager::Get().GetLocalProductUserIdString();
    ReFixOnline::LobbyData lob;
    auto res = m_serverState.ResyncLobby(localPuid, lobbyId, lob);
    if (callback) callback(res, lob);
}

} // namespace ReFixEOS
