// =============================================================================
// ReFix Online v2 - Authoritative Binary Protocol Implementation
// =============================================================================
#include "refix_backend_protocol.h"

namespace ReFixOnline {

bool SerializeHeader(const RefixPacketHeader& header, ByteWriter& writer) {
    writer.WriteUint32(header.Magic);
    writer.WriteUint16(header.Version);
    writer.WriteUint16(header.MessageType);
    writer.WriteUint64(header.RequestId);
    writer.WriteUint32(header.PayloadLength);
    return true;
}

bool DeserializeHeader(ByteReader& reader, RefixPacketHeader& header) {
    if (!reader.ReadUint32(header.Magic)) return false;
    if (!reader.ReadUint16(header.Version)) return false;
    if (!reader.ReadUint16(header.MessageType)) return false;
    if (!reader.ReadUint64(header.RequestId)) return false;
    if (!reader.ReadUint32(header.PayloadLength)) return false;

    if (header.Magic != REFIX_PROTOCOL_MAGIC) return false;
    if (header.PayloadLength > MAX_PACKET_SIZE) return false;
    return true;
}

// Helper: Serialize/Deserialize single LobbyData
static void WriteLobbyDataInternal(ByteWriter& writer, const LobbyData& lobby) {
    writer.WriteString(lobby.lobbyId);
    writer.WriteString(lobby.ownerUserId);
    writer.WriteUint32(lobby.maxMembers);
    writer.WriteUint32((uint32_t)lobby.members.size());
    writer.WriteInt32(lobby.state);
    writer.WriteUint64(lobby.createdAt);

    // Attributes
    writer.WriteUint32((uint32_t)lobby.attributes.size());
    for (const auto& kv : lobby.attributes) {
        writer.WriteString(kv.first);
        writer.WriteString(kv.second);
    }

    // Members
    writer.WriteUint32((uint32_t)lobby.members.size());
    for (const auto& m : lobby.members) {
        writer.WriteString(m.userId);
        writer.WriteString(m.displayName);
        writer.WriteUint64(m.joinTime);
        writer.WriteUint8(m.isOwner ? 1 : 0);
    }
}

static bool ReadLobbyDataInternal(ByteReader& reader, LobbyData& lobby) {
    if (!reader.ReadString(lobby.lobbyId, MAX_LOBBY_ID_LEN)) return false;
    if (!reader.ReadString(lobby.ownerUserId, 64)) return false;
    if (!reader.ReadUint32(lobby.maxMembers)) return false;
    if (!reader.ReadUint32(lobby.currentMembers)) return false;
    if (!reader.ReadInt32(lobby.state)) return false;
    if (!reader.ReadUint64(lobby.createdAt)) return false;

    // Attributes
    uint32_t attrCount = 0;
    if (!reader.ReadUint32(attrCount)) return false;
    if (attrCount > MAX_LOBBY_ATTRIBUTES) return false;
    lobby.attributes.clear();
    for (uint32_t i = 0; i < attrCount; ++i) {
        std::string k, v;
        if (!reader.ReadString(k, MAX_ATTRIBUTE_KEY_LEN)) return false;
        if (!reader.ReadString(v, MAX_ATTRIBUTE_VAL_LEN)) return false;
        lobby.attributes[k] = v;
    }

    // Members
    uint32_t memberCount = 0;
    if (!reader.ReadUint32(memberCount)) return false;
    if (memberCount > MAX_LOBBY_MEMBERS) return false;
    lobby.members.clear();
    for (uint32_t i = 0; i < memberCount; ++i) {
        LobbyMemberInfo m;
        uint8_t isOwnerByte = 0;
        if (!reader.ReadString(m.userId, 64)) return false;
        if (!reader.ReadString(m.displayName, MAX_DISPLAY_NAME_LEN)) return false;
        if (!reader.ReadUint64(m.joinTime)) return false;
        if (!reader.ReadUint8(isOwnerByte)) return false;
        m.isOwner = (isOwnerByte != 0);
        lobby.members.push_back(m);
    }
    return true;
}

// Auth
void WriteAuthRequest(ByteWriter& writer, uint16_t version, const std::string& userId, const std::string& displayName) {
    writer.WriteUint16(version);
    writer.WriteString(userId);
    writer.WriteString(displayName);
}

bool ReadAuthRequest(ByteReader& reader, uint16_t& version, std::string& userId, std::string& displayName) {
    if (!reader.ReadUint16(version)) return false;
    if (!reader.ReadString(userId, 64)) return false;
    if (!reader.ReadString(displayName, MAX_DISPLAY_NAME_LEN)) return false;
    return true;
}

void WriteAuthResult(ByteWriter& writer, EBackendResult result, const std::string& userId, uint64_t serverTime, const std::string& sessionToken) {
    writer.WriteInt32((int32_t)result);
    writer.WriteString(userId);
    writer.WriteUint64(serverTime);
    writer.WriteString(sessionToken);
}

bool ReadAuthResult(ByteReader& reader, EBackendResult& result, std::string& userId, uint64_t& serverTime, std::string& sessionToken) {
    int32_t resInt = 0;
    if (!reader.ReadInt32(resInt)) return false;
    result = (EBackendResult)resInt;
    if (!reader.ReadString(userId, 64)) return false;
    if (!reader.ReadUint64(serverTime)) return false;
    if (!reader.ReadString(sessionToken, 128)) return false;
    return true;
}

// Create Lobby
void WriteCreateLobbyRequest(ByteWriter& writer, uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes) {
    writer.WriteUint32(maxMembers);
    writer.WriteUint32((uint32_t)attributes.size());
    for (const auto& kv : attributes) {
        writer.WriteString(kv.first);
        writer.WriteString(kv.second);
    }
}

bool ReadCreateLobbyRequest(ByteReader& reader, uint32_t& maxMembers, std::unordered_map<std::string, std::string>& attributes) {
    if (!reader.ReadUint32(maxMembers)) return false;
    uint32_t count = 0;
    if (!reader.ReadUint32(count)) return false;
    if (count > MAX_LOBBY_ATTRIBUTES) return false;
    attributes.clear();
    for (uint32_t i = 0; i < count; ++i) {
        std::string k, v;
        if (!reader.ReadString(k, MAX_ATTRIBUTE_KEY_LEN)) return false;
        if (!reader.ReadString(v, MAX_ATTRIBUTE_VAL_LEN)) return false;
        attributes[k] = v;
    }
    return true;
}

void WriteCreateLobbyResult(ByteWriter& writer, EBackendResult result, const LobbyData& lobby) {
    writer.WriteInt32((int32_t)result);
    if (result == SUCCESS) {
        WriteLobbyDataInternal(writer, lobby);
    }
}

bool ReadCreateLobbyResult(ByteReader& reader, EBackendResult& result, LobbyData& lobby) {
    int32_t resInt = 0;
    if (!reader.ReadInt32(resInt)) return false;
    result = (EBackendResult)resInt;
    if (result == SUCCESS) {
        return ReadLobbyDataInternal(reader, lobby);
    }
    return true;
}

// Find Lobbies
void WriteFindLobbiesRequest(ByteWriter& writer, uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters) {
    writer.WriteUint32(maxResults);
    writer.WriteUint32((uint32_t)filters.size());
    for (const auto& kv : filters) {
        writer.WriteString(kv.first);
        writer.WriteString(kv.second);
    }
}

bool ReadFindLobbiesRequest(ByteReader& reader, uint32_t& maxResults, std::unordered_map<std::string, std::string>& filters) {
    if (!reader.ReadUint32(maxResults)) return false;
    uint32_t count = 0;
    if (!reader.ReadUint32(count)) return false;
    if (count > MAX_LOBBY_ATTRIBUTES) return false;
    filters.clear();
    for (uint32_t i = 0; i < count; ++i) {
        std::string k, v;
        if (!reader.ReadString(k, MAX_ATTRIBUTE_KEY_LEN)) return false;
        if (!reader.ReadString(v, MAX_ATTRIBUTE_VAL_LEN)) return false;
        filters[k] = v;
    }
    return true;
}

void WriteFindLobbiesResult(ByteWriter& writer, EBackendResult result, const std::vector<LobbyData>& lobbies) {
    writer.WriteInt32((int32_t)result);
    writer.WriteUint32((uint32_t)lobbies.size());
    for (const auto& lob : lobbies) {
        WriteLobbyDataInternal(writer, lob);
    }
}

bool ReadFindLobbiesResult(ByteReader& reader, EBackendResult& result, std::vector<LobbyData>& lobbies) {
    int32_t resInt = 0;
    if (!reader.ReadInt32(resInt)) return false;
    result = (EBackendResult)resInt;
    uint32_t count = 0;
    if (!reader.ReadUint32(count)) return false;
    if (count > 256) return false;
    lobbies.clear();
    for (uint32_t i = 0; i < count; ++i) {
        LobbyData lob;
        if (!ReadLobbyDataInternal(reader, lob)) return false;
        lobbies.push_back(lob);
    }
    return true;
}

// Join Lobby
void WriteJoinLobbyRequest(ByteWriter& writer, const std::string& lobbyId) {
    writer.WriteString(lobbyId);
}

bool ReadJoinLobbyRequest(ByteReader& reader, std::string& lobbyId) {
    return reader.ReadString(lobbyId, MAX_LOBBY_ID_LEN);
}

void WriteJoinLobbyResult(ByteWriter& writer, EBackendResult result, const LobbyData& lobby) {
    writer.WriteInt32((int32_t)result);
    if (result == SUCCESS) {
        WriteLobbyDataInternal(writer, lobby);
    }
}

bool ReadJoinLobbyResult(ByteReader& reader, EBackendResult& result, LobbyData& lobby) {
    int32_t resInt = 0;
    if (!reader.ReadInt32(resInt)) return false;
    result = (EBackendResult)resInt;
    if (result == SUCCESS) {
        return ReadLobbyDataInternal(reader, lobby);
    }
    return true;
}

// Leave Lobby
void WriteLeaveLobbyRequest(ByteWriter& writer, const std::string& lobbyId) {
    writer.WriteString(lobbyId);
}

bool ReadLeaveLobbyRequest(ByteReader& reader, std::string& lobbyId) {
    return reader.ReadString(lobbyId, MAX_LOBBY_ID_LEN);
}

void WriteLeaveLobbyResult(ByteWriter& writer, EBackendResult result, const std::string& lobbyId) {
    writer.WriteInt32((int32_t)result);
    writer.WriteString(lobbyId);
}

bool ReadLeaveLobbyResult(ByteReader& reader, EBackendResult& result, std::string& lobbyId) {
    int32_t resInt = 0;
    if (!reader.ReadInt32(resInt)) return false;
    result = (EBackendResult)resInt;
    return reader.ReadString(lobbyId, MAX_LOBBY_ID_LEN);
}

// Notifications
void WriteMemberJoinedNotification(ByteWriter& writer, const std::string& lobbyId, const LobbyMemberInfo& member) {
    writer.WriteString(lobbyId);
    writer.WriteString(member.userId);
    writer.WriteString(member.displayName);
    writer.WriteUint64(member.joinTime);
    writer.WriteUint8(member.isOwner ? 1 : 0);
}

bool ReadMemberJoinedNotification(ByteReader& reader, std::string& lobbyId, LobbyMemberInfo& member) {
    if (!reader.ReadString(lobbyId, MAX_LOBBY_ID_LEN)) return false;
    if (!reader.ReadString(member.userId, 64)) return false;
    if (!reader.ReadString(member.displayName, MAX_DISPLAY_NAME_LEN)) return false;
    if (!reader.ReadUint64(member.joinTime)) return false;
    uint8_t isOwn = 0;
    if (!reader.ReadUint8(isOwn)) return false;
    member.isOwner = (isOwn != 0);
    return true;
}

void WriteMemberLeftNotification(ByteWriter& writer, const std::string& lobbyId, const std::string& userId, const std::string& newOwnerUserId) {
    writer.WriteString(lobbyId);
    writer.WriteString(userId);
    writer.WriteString(newOwnerUserId);
}

bool ReadMemberLeftNotification(ByteReader& reader, std::string& lobbyId, std::string& userId, std::string& newOwnerUserId) {
    if (!reader.ReadString(lobbyId, MAX_LOBBY_ID_LEN)) return false;
    if (!reader.ReadString(userId, 64)) return false;
    if (!reader.ReadString(newOwnerUserId, 64)) return false;
    return true;
}

void WriteLobbyUpdateNotification(ByteWriter& writer, const LobbyData& lobby) {
    WriteLobbyDataInternal(writer, lobby);
}

bool ReadLobbyUpdateNotification(ByteReader& reader, LobbyData& lobby) {
    return ReadLobbyDataInternal(reader, lobby);
}

// Heartbeat
void WriteHeartbeat(ByteWriter& writer, uint64_t clientTime) {
    writer.WriteUint64(clientTime);
}

bool ReadHeartbeat(ByteReader& reader, uint64_t& clientTime) {
    return reader.ReadUint64(clientTime);
}

void WriteHeartbeatAck(ByteWriter& writer, uint64_t serverTime) {
    writer.WriteUint64(serverTime);
}

bool ReadHeartbeatAck(ByteReader& reader, uint64_t& serverTime) {
    return reader.ReadUint64(serverTime);
}

// Resync
void WriteResyncLobbyRequest(ByteWriter& writer, const std::string& lobbyId) {
    writer.WriteString(lobbyId);
}

bool ReadResyncLobbyRequest(ByteReader& reader, std::string& lobbyId) {
    return reader.ReadString(lobbyId, MAX_LOBBY_ID_LEN);
}

void WriteResyncLobbyResult(ByteWriter& writer, EBackendResult result, const LobbyData& lobby) {
    writer.WriteInt32((int32_t)result);
    if (result == SUCCESS) {
        WriteLobbyDataInternal(writer, lobby);
    }
}

bool ReadResyncLobbyResult(ByteReader& reader, EBackendResult& result, LobbyData& lobby) {
    int32_t resInt = 0;
    if (!reader.ReadInt32(resInt)) return false;
    result = (EBackendResult)resInt;
    if (result == SUCCESS) {
        return ReadLobbyDataInternal(reader, lobby);
    }
    return true;
}

} // namespace ReFixOnline
