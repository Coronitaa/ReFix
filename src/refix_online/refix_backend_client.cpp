#include <windows.h>
// =============================================================================
// ReFix Online v2 - Backend Client Implementation
// =============================================================================
#include "refix_backend_client.h"

namespace ReFixOnline {

InProcessDirectTransport::InProcessDirectTransport(BackendServerState& server)
    : m_server(server), m_connected(false) {}

bool InProcessDirectTransport::Connect(const std::string& endpoint, uint16_t port) {
    m_connected = true;
    return true;
}

void InProcessDirectTransport::Disconnect() {
    m_connected = false;
}

bool InProcessDirectTransport::IsConnected() const {
    return m_connected;
}

bool InProcessDirectTransport::Send(const uint8_t* data, size_t size) {
    if (!m_connected || !data || size < sizeof(RefixPacketHeader)) return false;

    ByteReader reader(data, size);
    RefixPacketHeader header = {};
    if (!DeserializeHeader(reader, header)) return false;

    ByteWriter responseWriter;
    RefixPacketHeader respHeader = {};
    respHeader.Magic = REFIX_PROTOCOL_MAGIC;
    respHeader.Version = REFIX_PROTOCOL_VERSION;
    respHeader.RequestId = header.RequestId;

    switch (header.MessageType) {
    case MSG_AUTH: {
        uint16_t ver = 0;
        std::string uid, name;
        if (ReadAuthRequest(reader, ver, uid, name)) {
            std::string token;
            EBackendResult res = m_server.AuthenticateSession(uid, name, token);
            if (res == SUCCESS) {
                m_authUserId = uid;
                m_authDisplayName = name;
            }
            respHeader.MessageType = MSG_AUTH_RESULT;
            WriteAuthResult(responseWriter, res, uid, GetTickCount64(), token);
        }
        break;
    }
    case MSG_CREATE_LOBBY: {
        uint32_t maxMem = 0;
        std::unordered_map<std::string, std::string> attrs;
        if (ReadCreateLobbyRequest(reader, maxMem, attrs)) {
            LobbyData lob;
            EBackendResult res = m_server.CreateLobby(m_authUserId, maxMem, attrs, lob);
            respHeader.MessageType = MSG_CREATE_LOBBY_RESULT;
            WriteCreateLobbyResult(responseWriter, res, lob);
        }
        break;
    }
    case MSG_FIND_LOBBIES: {
        uint32_t maxRes = 0;
        std::unordered_map<std::string, std::string> filters;
        if (ReadFindLobbiesRequest(reader, maxRes, filters)) {
            std::vector<LobbyData> lobs;
            EBackendResult res = m_server.FindLobbies(m_authUserId, maxRes, filters, lobs);
            respHeader.MessageType = MSG_FIND_LOBBIES_RESULT;
            WriteFindLobbiesResult(responseWriter, res, lobs);
        }
        break;
    }
    case MSG_JOIN_LOBBY: {
        std::string lobId;
        if (ReadJoinLobbyRequest(reader, lobId)) {
            LobbyData lob;
            EBackendResult res = m_server.JoinLobby(m_authUserId, m_authDisplayName, lobId, lob);
            respHeader.MessageType = MSG_JOIN_LOBBY_RESULT;
            WriteJoinLobbyResult(responseWriter, res, lob);
        }
        break;
    }
    case MSG_LEAVE_LOBBY: {
        std::string lobId;
        if (ReadLeaveLobbyRequest(reader, lobId)) {
            std::string newOwner;
            EBackendResult res = m_server.LeaveLobby(m_authUserId, lobId, newOwner);
            respHeader.MessageType = MSG_LEAVE_LOBBY_RESULT;
            WriteLeaveLobbyResult(responseWriter, res, lobId);
        }
        break;
    }
    case MSG_RESYNC_LOBBY: {
        std::string lobId;
        if (ReadResyncLobbyRequest(reader, lobId)) {
            LobbyData lob;
            EBackendResult res = m_server.ResyncLobby(m_authUserId, lobId, lob);
            respHeader.MessageType = MSG_RESYNC_LOBBY_RESULT;
            WriteResyncLobbyResult(responseWriter, res, lob);
        }
        break;
    }
    default:
        break;
    }

    respHeader.PayloadLength = (uint32_t)responseWriter.GetSize();
    ByteWriter fullPacket;
    SerializeHeader(respHeader, fullPacket);
    fullPacket.WriteBytes(responseWriter.GetData(), responseWriter.GetSize());

    std::lock_guard<std::mutex> lock(m_transportMutex);
    m_inbox.push_back(fullPacket.GetBuffer());
    return true;
}

bool InProcessDirectTransport::Receive(std::vector<uint8_t>& outBuffer) {
    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_inbox.empty()) return false;
    outBuffer = m_inbox.front();
    m_inbox.erase(m_inbox.begin());
    return true;
}

// -----------------------------------------------------------------------------
// BackendClient
// -----------------------------------------------------------------------------
BackendClient::BackendClient(std::unique_ptr<IRefixTransport> transport)
    : m_transport(std::move(transport)),
      m_state(EClientConnectionState::DISCONNECTED),
      m_requestIdGen(1) {}

BackendClient::~BackendClient() {
    Disconnect();
}

uint64_t BackendClient::GetNextRequestId() {
    return m_requestIdGen.fetch_add(1);
}

bool BackendClient::Connect(const std::string& endpoint, uint16_t port) {
    m_state = EClientConnectionState::CONNECTING;
    if (m_transport && m_transport->Connect(endpoint, port)) {
        m_state = EClientConnectionState::CONNECTED;
        return true;
    }
    m_state = EClientConnectionState::ERROR_STATE;
    return false;
}

void BackendClient::Disconnect() {
    if (m_transport) m_transport->Disconnect();
    m_state = EClientConnectionState::DISCONNECTED;
    m_sessionToken.clear();
    m_activeLobbyId.clear();
}

void BackendClient::Tick() {
    if (!m_transport || !m_transport->IsConnected()) return;

    std::vector<uint8_t> pkt;
    while (m_transport->Receive(pkt)) {
        if (pkt.size() < sizeof(RefixPacketHeader)) continue;

        ByteReader reader(pkt.data(), pkt.size());
        RefixPacketHeader header = {};
        if (!DeserializeHeader(reader, header)) continue;

        std::function<void(ByteReader&)> cb = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_clientMutex);
            auto it = m_pendingCallbacks.find(header.RequestId);
            if (it != m_pendingCallbacks.end()) {
                cb = it->second;
                m_pendingCallbacks.erase(it);
            }
        }
        if (cb) {
            cb(reader);
        }
    }
}

uint64_t BackendClient::Authenticate(const std::string& userId, const std::string& displayName, std::function<void(EBackendResult, const std::string&)> onComplete) {
    uint64_t reqId = GetNextRequestId();
    m_userId = userId;
    m_displayName = displayName;

    RefixPacketHeader header = {};
    header.Magic = REFIX_PROTOCOL_MAGIC;
    header.Version = REFIX_PROTOCOL_VERSION;
    header.MessageType = MSG_AUTH;
    header.RequestId = reqId;

    ByteWriter payload;
    WriteAuthRequest(payload, REFIX_PROTOCOL_VERSION, userId, displayName);
    header.PayloadLength = (uint32_t)payload.GetSize();

    ByteWriter fullPacket;
    SerializeHeader(header, fullPacket);
    fullPacket.WriteBytes(payload.GetData(), payload.GetSize());

    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_pendingCallbacks[reqId] = [this, onComplete](ByteReader& r) {
            EBackendResult res = SERVER_ERROR;
            std::string uid, token;
            uint64_t sTime = 0;
            if (ReadAuthResult(r, res, uid, sTime, token)) {
                if (res == SUCCESS) {
                    m_sessionToken = token;
                    m_state = EClientConnectionState::AUTHENTICATED;
                }
                if (onComplete) onComplete(res, token);
            } else {
                if (onComplete) onComplete(INVALID_PACKET, "");
            }
        };
    }

    m_transport->Send(fullPacket.GetData(), fullPacket.GetSize());
    return reqId;
}

uint64_t BackendClient::CreateLobby(uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes, std::function<void(EBackendResult, const LobbyData&)> onComplete) {
    uint64_t reqId = GetNextRequestId();

    RefixPacketHeader header = {};
    header.Magic = REFIX_PROTOCOL_MAGIC;
    header.Version = REFIX_PROTOCOL_VERSION;
    header.MessageType = MSG_CREATE_LOBBY;
    header.RequestId = reqId;

    ByteWriter payload;
    WriteCreateLobbyRequest(payload, maxMembers, attributes);
    header.PayloadLength = (uint32_t)payload.GetSize();

    ByteWriter fullPacket;
    SerializeHeader(header, fullPacket);
    fullPacket.WriteBytes(payload.GetData(), payload.GetSize());

    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_pendingCallbacks[reqId] = [this, onComplete](ByteReader& r) {
            EBackendResult res = SERVER_ERROR;
            LobbyData lob;
            if (ReadCreateLobbyResult(r, res, lob)) {
                if (res == SUCCESS) {
                    m_activeLobbyId = lob.lobbyId;
                }
                if (onComplete) onComplete(res, lob);
            } else {
                if (onComplete) onComplete(INVALID_PACKET, lob);
            }
        };
    }

    m_transport->Send(fullPacket.GetData(), fullPacket.GetSize());
    return reqId;
}

uint64_t BackendClient::FindLobbies(uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters, std::function<void(EBackendResult, const std::vector<LobbyData>&)> onComplete) {
    uint64_t reqId = GetNextRequestId();

    RefixPacketHeader header = {};
    header.Magic = REFIX_PROTOCOL_MAGIC;
    header.Version = REFIX_PROTOCOL_VERSION;
    header.MessageType = MSG_FIND_LOBBIES;
    header.RequestId = reqId;

    ByteWriter payload;
    WriteFindLobbiesRequest(payload, maxResults, filters);
    header.PayloadLength = (uint32_t)payload.GetSize();

    ByteWriter fullPacket;
    SerializeHeader(header, fullPacket);
    fullPacket.WriteBytes(payload.GetData(), payload.GetSize());

    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_pendingCallbacks[reqId] = [onComplete](ByteReader& r) {
            EBackendResult res = SERVER_ERROR;
            std::vector<LobbyData> lobs;
            if (ReadFindLobbiesResult(r, res, lobs)) {
                if (onComplete) onComplete(res, lobs);
            } else {
                if (onComplete) onComplete(INVALID_PACKET, lobs);
            }
        };
    }

    m_transport->Send(fullPacket.GetData(), fullPacket.GetSize());
    return reqId;
}

uint64_t BackendClient::JoinLobby(const std::string& lobbyId, std::function<void(EBackendResult, const LobbyData&)> onComplete) {
    uint64_t reqId = GetNextRequestId();

    RefixPacketHeader header = {};
    header.Magic = REFIX_PROTOCOL_MAGIC;
    header.Version = REFIX_PROTOCOL_VERSION;
    header.MessageType = MSG_JOIN_LOBBY;
    header.RequestId = reqId;

    ByteWriter payload;
    WriteJoinLobbyRequest(payload, lobbyId);
    header.PayloadLength = (uint32_t)payload.GetSize();

    ByteWriter fullPacket;
    SerializeHeader(header, fullPacket);
    fullPacket.WriteBytes(payload.GetData(), payload.GetSize());

    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_pendingCallbacks[reqId] = [this, onComplete](ByteReader& r) {
            EBackendResult res = SERVER_ERROR;
            LobbyData lob;
            if (ReadJoinLobbyResult(r, res, lob)) {
                if (res == SUCCESS) {
                    m_activeLobbyId = lob.lobbyId;
                }
                if (onComplete) onComplete(res, lob);
            } else {
                if (onComplete) onComplete(INVALID_PACKET, lob);
            }
        };
    }

    m_transport->Send(fullPacket.GetData(), fullPacket.GetSize());
    return reqId;
}

uint64_t BackendClient::LeaveLobby(const std::string& lobbyId, std::function<void(EBackendResult, const std::string&)> onComplete) {
    uint64_t reqId = GetNextRequestId();

    RefixPacketHeader header = {};
    header.Magic = REFIX_PROTOCOL_MAGIC;
    header.Version = REFIX_PROTOCOL_VERSION;
    header.MessageType = MSG_LEAVE_LOBBY;
    header.RequestId = reqId;

    ByteWriter payload;
    WriteLeaveLobbyRequest(payload, lobbyId);
    header.PayloadLength = (uint32_t)payload.GetSize();

    ByteWriter fullPacket;
    SerializeHeader(header, fullPacket);
    fullPacket.WriteBytes(payload.GetData(), payload.GetSize());

    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_pendingCallbacks[reqId] = [this, onComplete](ByteReader& r) {
            EBackendResult res = SERVER_ERROR;
            std::string lobId;
            if (ReadLeaveLobbyResult(r, res, lobId)) {
                if (res == SUCCESS) {
                    m_activeLobbyId.clear();
                }
                if (onComplete) onComplete(res, lobId);
            } else {
                if (onComplete) onComplete(INVALID_PACKET, lobId);
            }
        };
    }

    m_transport->Send(fullPacket.GetData(), fullPacket.GetSize());
    return reqId;
}

uint64_t BackendClient::ResyncLobby(const std::string& lobbyId, std::function<void(EBackendResult, const LobbyData&)> onComplete) {
    uint64_t reqId = GetNextRequestId();

    RefixPacketHeader header = {};
    header.Magic = REFIX_PROTOCOL_MAGIC;
    header.Version = REFIX_PROTOCOL_VERSION;
    header.MessageType = MSG_RESYNC_LOBBY;
    header.RequestId = reqId;

    ByteWriter payload;
    WriteResyncLobbyRequest(payload, lobbyId);
    header.PayloadLength = (uint32_t)payload.GetSize();

    ByteWriter fullPacket;
    SerializeHeader(header, fullPacket);
    fullPacket.WriteBytes(payload.GetData(), payload.GetSize());

    {
        std::lock_guard<std::mutex> lock(m_clientMutex);
        m_pendingCallbacks[reqId] = [onComplete](ByteReader& r) {
            EBackendResult res = SERVER_ERROR;
            LobbyData lob;
            if (ReadResyncLobbyResult(r, res, lob)) {
                if (onComplete) onComplete(res, lob);
            } else {
                if (onComplete) onComplete(INVALID_PACKET, lob);
            }
        };
    }

    m_transport->Send(fullPacket.GetData(), fullPacket.GetSize());
    return reqId;
}

} // namespace ReFixOnline
