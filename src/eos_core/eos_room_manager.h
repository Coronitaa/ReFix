// =============================================================================
// ReFix EOS Online v2 - EOS Room Bridge & Translation Layer
// =============================================================================
#pragma once

#include "../refix_online/refix_backend_client.h"
#include "../refix_online/refix_backend_state.h"
#include "../eos/eos_types.h"
#include <memory>
#include <string>

namespace ReFixEOS {

class RoomManagerBridge {
public:
    static RoomManagerBridge& Get();

    void Initialize();
    void Reset();
    void Shutdown();
    void Tick();

    void Authenticate(const std::string& userId, const std::string& displayName, std::function<void(ReFixOnline::EBackendResult, const std::string&)> callback = nullptr);
    void EnsureAuthenticated();

    // Internal Bridge APIs (Prepared for subsequent EOS_Lobby_* translation)
    void CreateLobby(uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback);
    void FindLobbies(uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters, std::function<void(ReFixOnline::EBackendResult, const std::vector<ReFixOnline::LobbyData>&)> callback);
    void JoinLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback);
    void LeaveLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const std::string&)> callback);
    void DestroyLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const std::string&)> callback);
    void ResyncLobby(const std::string& lobbyId, std::function<void(ReFixOnline::EBackendResult, const ReFixOnline::LobbyData&)> callback);
    bool GetLobby(const std::string& lobbyId, ReFixOnline::LobbyData& outLobby);

    ReFixOnline::BackendServerState& GetServerState() { return m_serverState; }
    ReFixOnline::BackendClient* GetClient() { return m_client.get(); }

private:
    RoomManagerBridge();
    ~RoomManagerBridge() = default;

    ReFixOnline::BackendServerState m_serverState;
    std::unique_ptr<ReFixOnline::BackendClient> m_client;
};

} // namespace ReFixEOS
