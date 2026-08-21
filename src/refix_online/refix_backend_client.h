// =============================================================================
// ReFix Online v2 - Backend Client & Transport Abstraction
// =============================================================================
#pragma once

#include "refix_backend_protocol.h"
#include "refix_backend_state.h"
#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace ReFixOnline {

enum class EClientConnectionState {
    DISCONNECTED = 0,
    CONNECTING   = 1,
    CONNECTED    = 2,
    AUTHENTICATED= 3,
    ERROR_STATE  = 4,
    RECONNECTING = 5
};

class IRefixTransport {
public:
    virtual ~IRefixTransport() = default;
    virtual bool Connect(const std::string& endpoint, uint16_t port) = 0;
    virtual void Disconnect() = 0;
    virtual bool Send(const uint8_t* data, size_t size) = 0;
    virtual bool Receive(std::vector<uint8_t>& outBuffer) = 0;
    virtual bool IsConnected() const = 0;
};

// In-Process Direct Transport for local simulation & authoritative state routing
class InProcessDirectTransport : public IRefixTransport {
public:
    InProcessDirectTransport(BackendServerState& server);
    bool Connect(const std::string& endpoint, uint16_t port) override;
    void Disconnect() override;
    bool Send(const uint8_t* data, size_t size) override;
    bool Receive(std::vector<uint8_t>& outBuffer) override;
    bool IsConnected() const override;

private:
    BackendServerState& m_server;
    bool m_connected;
    std::vector<std::vector<uint8_t>> m_inbox;
    std::string m_authUserId;
    std::string m_authDisplayName;
    std::mutex m_transportMutex;
};

// Client class managing Requests, State Machine, and Asynchronous Responses
class BackendClient {
public:
    BackendClient(std::unique_ptr<IRefixTransport> transport);
    ~BackendClient();

    bool Connect(const std::string& endpoint = "127.0.0.1", uint16_t port = 47584);
    void Disconnect();
    void Tick();

    EClientConnectionState GetConnectionState() const { return m_state; }

    // Asynchronous Request APIs (Correlated via RequestId)
    uint64_t Authenticate(const std::string& userId, const std::string& displayName, std::function<void(EBackendResult, const std::string&)> onComplete);
    uint64_t CreateLobby(uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes, std::function<void(EBackendResult, const LobbyData&)> onComplete);
    uint64_t FindLobbies(uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters, std::function<void(EBackendResult, const std::vector<LobbyData>&)> onComplete);
    uint64_t JoinLobby(const std::string& lobbyId, std::function<void(EBackendResult, const LobbyData&)> onComplete);
    uint64_t LeaveLobby(const std::string& lobbyId, std::function<void(EBackendResult, const std::string&)> onComplete);
    uint64_t ResyncLobby(const std::string& lobbyId, std::function<void(EBackendResult, const LobbyData&)> onComplete);

    // Notification Handlers
    void SetOnMemberJoinedCallback(std::function<void(const std::string&, const LobbyMemberInfo&)> cb) { m_onMemberJoined = cb; }
    void SetOnMemberLeftCallback(std::function<void(const std::string&, const std::string&, const std::string&)> cb) { m_onMemberLeft = cb; }
    void SetOnLobbyUpdatedCallback(std::function<void(const LobbyData&)> cb) { m_onLobbyUpdated = cb; }

    // Diagnostic & Token State
    const std::string& GetCurrentUserId() const { return m_userId; }
    const std::string& GetSessionToken() const { return m_sessionToken; }
    const std::string& GetActiveLobbyId() const { return m_activeLobbyId; }

private:
    uint64_t GetNextRequestId();

    std::unique_ptr<IRefixTransport> m_transport;
    std::atomic<EClientConnectionState> m_state;
    std::atomic<uint64_t> m_requestIdGen;

    std::string m_userId;
    std::string m_displayName;
    std::string m_sessionToken;
    std::string m_activeLobbyId;

    std::mutex m_clientMutex;
    std::unordered_map<uint64_t, std::function<void(ByteReader&)>> m_pendingCallbacks;

    std::function<void(const std::string&, const LobbyMemberInfo&)> m_onMemberJoined;
    std::function<void(const std::string&, const std::string&, const std::string&)> m_onMemberLeft;
    std::function<void(const LobbyData&)> m_onLobbyUpdated;
};

} // namespace ReFixOnline
