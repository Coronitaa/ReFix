// =============================================================================
// ReFix Online v2 - Authoritative Binary Protocol Specification
// =============================================================================
#pragma once

#include "refix_wire.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ReFixOnline {

constexpr uint32_t REFIX_PROTOCOL_MAGIC   = 0x52464958; // 'RFIX'
constexpr uint16_t REFIX_PROTOCOL_VERSION = 1;

// Protocol Limits
constexpr size_t MAX_PACKET_SIZE        = 65536; // 64 KB
constexpr size_t MAX_DISPLAY_NAME_LEN   = 64;
constexpr size_t MAX_LOBBY_ATTRIBUTES   = 64;
constexpr size_t MAX_ATTRIBUTE_KEY_LEN  = 64;
constexpr size_t MAX_ATTRIBUTE_VAL_LEN  = 1024;
constexpr uint32_t MAX_LOBBY_MEMBERS    = 64;
constexpr size_t MAX_LOBBY_ID_LEN       = 64;

// Message Types
enum EMessageType : uint16_t {
    MSG_HELLO                   = 1,
    MSG_HELLO_ACK               = 2,
    MSG_AUTH                    = 3,
    MSG_AUTH_RESULT             = 4,
    MSG_CREATE_LOBBY            = 5,
    MSG_CREATE_LOBBY_RESULT     = 6,
    MSG_FIND_LOBBIES            = 7,
    MSG_FIND_LOBBIES_RESULT     = 8,
    MSG_JOIN_LOBBY              = 9,
    MSG_JOIN_LOBBY_RESULT       = 10,
    MSG_LEAVE_LOBBY             = 11,
    MSG_LEAVE_LOBBY_RESULT      = 12,
    MSG_LOBBY_UPDATE            = 13,
    MSG_MEMBER_JOINED           = 14,
    MSG_MEMBER_LEFT             = 15,
    MSG_HEARTBEAT               = 16,
    MSG_HEARTBEAT_ACK           = 17,
    MSG_RESYNC_LOBBY            = 18,
    MSG_RESYNC_LOBBY_RESULT     = 19,
    MSG_DESTROY_LOBBY           = 20,
    MSG_DESTROY_LOBBY_RESULT    = 21,
    MSG_ERROR                   = 99
};

// Independent Result Codes
enum EBackendResult : int32_t {
    SUCCESS                 = 0,
    INVALID_PACKET          = 1,
    UNSUPPORTED_VERSION     = 2,
    NOT_AUTHENTICATED       = 3,
    INVALID_USER            = 4,
    LOBBY_NOT_FOUND         = 5,
    LOBBY_FULL              = 6,
    ALREADY_MEMBER          = 7,
    NOT_MEMBER              = 8,
    NOT_OWNER               = 9,
    INVALID_ATTRIBUTE       = 10,
    REQUEST_TIMEOUT         = 11,
    SERVER_ERROR            = 12
};

#pragma pack(push, 1)
struct RefixPacketHeader {
    uint32_t Magic;
    uint16_t Version;
    uint16_t MessageType;
    uint64_t RequestId;
    uint32_t PayloadLength;
};
#pragma pack(pop)

// Protocol Data Models
struct LobbyMemberInfo {
    std::string userId;
    std::string displayName;
    uint64_t joinTime;
    bool isOwner;
};

struct LobbyData {
    std::string lobbyId;
    std::string ownerUserId;
    uint32_t maxMembers;
    uint32_t currentMembers;
    std::unordered_map<std::string, std::string> attributes;
    std::vector<LobbyMemberInfo> members;
    uint64_t createdAt;
    uint64_t lastHeartbeat;
    int32_t state; // 0=Creating, 1=Active, 2=Destroyed
};

// Packet Serialization Functions
bool SerializeHeader(const RefixPacketHeader& header, ByteWriter& writer);
bool DeserializeHeader(ByteReader& reader, RefixPacketHeader& header);

// Auth Packets
void WriteAuthRequest(ByteWriter& writer, uint16_t version, const std::string& userId, const std::string& displayName);
bool ReadAuthRequest(ByteReader& reader, uint16_t& version, std::string& userId, std::string& displayName);

void WriteAuthResult(ByteWriter& writer, EBackendResult result, const std::string& userId, uint64_t serverTime, const std::string& sessionToken);
bool ReadAuthResult(ByteReader& reader, EBackendResult& result, std::string& userId, uint64_t& serverTime, std::string& sessionToken);

// Create Lobby Packets
void WriteCreateLobbyRequest(ByteWriter& writer, uint32_t maxMembers, const std::unordered_map<std::string, std::string>& attributes);
bool ReadCreateLobbyRequest(ByteReader& reader, uint32_t& maxMembers, std::unordered_map<std::string, std::string>& attributes);

void WriteCreateLobbyResult(ByteWriter& writer, EBackendResult result, const LobbyData& lobby);
bool ReadCreateLobbyResult(ByteReader& reader, EBackendResult& result, LobbyData& lobby);

// Find Lobbies Packets
void WriteFindLobbiesRequest(ByteWriter& writer, uint32_t maxResults, const std::unordered_map<std::string, std::string>& filters);
bool ReadFindLobbiesRequest(ByteReader& reader, uint32_t& maxResults, std::unordered_map<std::string, std::string>& filters);

void WriteFindLobbiesResult(ByteWriter& writer, EBackendResult result, const std::vector<LobbyData>& lobbies);
bool ReadFindLobbiesResult(ByteReader& reader, EBackendResult& result, std::vector<LobbyData>& lobbies);

// Join Lobby Packets
void WriteJoinLobbyRequest(ByteWriter& writer, const std::string& lobbyId);
bool ReadJoinLobbyRequest(ByteReader& reader, std::string& lobbyId);

void WriteJoinLobbyResult(ByteWriter& writer, EBackendResult result, const LobbyData& lobby);
bool ReadJoinLobbyResult(ByteReader& reader, EBackendResult& result, LobbyData& lobby);

// Leave & Destroy Lobby Packets
void WriteLeaveLobbyRequest(ByteWriter& writer, const std::string& lobbyId);
bool ReadLeaveLobbyRequest(ByteReader& reader, std::string& lobbyId);

void WriteLeaveLobbyResult(ByteWriter& writer, EBackendResult result, const std::string& lobbyId);
bool ReadLeaveLobbyResult(ByteReader& reader, EBackendResult& result, std::string& lobbyId);

// Notifications & Updates
void WriteMemberJoinedNotification(ByteWriter& writer, const std::string& lobbyId, const LobbyMemberInfo& member);
bool ReadMemberJoinedNotification(ByteReader& reader, std::string& lobbyId, LobbyMemberInfo& member);

void WriteMemberLeftNotification(ByteWriter& writer, const std::string& lobbyId, const std::string& userId, const std::string& newOwnerUserId);
bool ReadMemberLeftNotification(ByteReader& reader, std::string& lobbyId, std::string& userId, std::string& newOwnerUserId);

void WriteLobbyUpdateNotification(ByteWriter& writer, const LobbyData& lobby);
bool ReadLobbyUpdateNotification(ByteReader& reader, LobbyData& lobby);

// Heartbeat & Resync
void WriteHeartbeat(ByteWriter& writer, uint64_t clientTime);
bool ReadHeartbeat(ByteReader& reader, uint64_t& clientTime);

void WriteHeartbeatAck(ByteWriter& writer, uint64_t serverTime);
bool ReadHeartbeatAck(ByteReader& reader, uint64_t& serverTime);

void WriteResyncLobbyRequest(ByteWriter& writer, const std::string& lobbyId);
bool ReadResyncLobbyRequest(ByteReader& reader, std::string& lobbyId);

void WriteResyncLobbyResult(ByteWriter& writer, EBackendResult result, const LobbyData& lobby);
bool ReadResyncLobbyResult(ByteReader& reader, EBackendResult& result, LobbyData& lobby);

} // namespace ReFixOnline
