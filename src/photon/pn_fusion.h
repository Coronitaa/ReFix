#pragma once
#include <cstdint>
#include <string>

// Photon Fusion server-plugin protocol (Fusion.Protocol), the part Photon Cloud's "Fusion" room plugin answers.
//
// Fusion clients send protocol messages as Photon events (code PROTOCOL, parameter 245 = byte[]) to the
// server actor 0 and wait for the plugin's replies; without them the client times out (OperationTimeout).
// A buffer is a bit stream (LSB first, little-endian integers, zero padded to a 4-byte multiple) holding
// [message id:8][message]. Every message starts with ProtocolVersion:8 and, from V1_1_0, the Fusion
// serialization version as three int32 (major, minor, build); the message body follows.
// Layouts were read from Fusion.Sockets (Fusion 2.0.8, protocol V1_6_0).
namespace PnFusion {

constexpr uint8_t EV_PROTOCOL = 100, EV_DATA = 101, EV_DUMMY = 102;
constexpr int32_t SERVER_ACTOR = 0;
constexpr uint8_t P_FUSION_DATA = 245;

// Message ids (ProtocolSerializer registration order): 1 Join, 2 NetworkConfigSync, 3 ReflexiveInfo,
// 4 Disconnect, 5 Start, 6 Snapshot, 7 HostMigration, 8 PlayerRefMapping, 9 ChangeMasterClient,
// 10 DummyTrafficSync.
constexpr uint8_t MSG_JOIN = 1, MSG_NETWORK_CONFIG_SYNC = 2, MSG_REFLEXIVE_INFO = 3, MSG_START = 5,
                  MSG_PLAYER_REF_MAPPING = 8;
constexpr uint8_t SYNC_REQUEST = 1, SYNC_RESPONSE = 2, SYNC_OVERRIDE = 3; // NetworkConfigSync.SyncType
constexpr int32_t JOIN_REQUEST_NETWORK_CONFIG = 1, JOIN_REQUEST_REFLEXIVE_INFO = 2; // Join.JoinRequests flags
constexpr uint8_t JOIN_REQUEST = 1, JOIN_CONFIRMATION = 2;
constexpr uint8_t PEER_NONE = 0, PEER_SERVER = 1, PEER_CLIENT = 2;
constexpr uint8_t GAME_MODE_CLIENT_SERVER = 1, GAME_MODE_SHARED = 2;
constexpr size_t UNIQUE_ID_SIZE = 8; // Join/ReflexiveInfo UniqueId: a 64-bit id, little-endian

struct Header {
    uint8_t protocolVersion = 0;
    int32_t versionMajor = 0, versionMinor = 0, versionBuild = 0;
};

// byte[] fields: a presence bit, then a 16-bit length and the bytes.
struct OptionalBytes {
    bool present = false;
    std::string bytes;
};

struct Join {
    uint8_t type = 0;
    uint8_t gameMode = 0;
    uint8_t peerMode = 0;
    int32_t joinRequests = 0;
    OptionalBytes uniqueId;             // from protocol V1_2_3 (6)
    int32_t playerRef = 0;              // from protocol V1_3_0 (7)
    OptionalBytes encryptionKey;        // from protocol V1_5_0 (9)
    OptionalBytes encryptionKeySecret;  // from protocol V1_5_0 (9)
};

// Sent by the plugin after Join: the client then initializes its NetworkRunner and, as a client, connects to
// the peer RemoteServerID (the host's actor number).
struct Start {
    uint32_t remoteServerId = 0;
    uint32_t startRequests = 0;  // StartRequests flags: ConnectToShared 1, WaitForReflexiveInfo 2
};

// NAT-punch addresses of one peer. A client connects to its server only after receiving the server's
// ReflexiveInfo: directly when the addresses are usable, otherwise through the relay (Photon events to the
// server's actor). A NetAddress is three 64-bit blocks; all zero is "no address".
struct ReflexiveInfo {
    uint32_t actorId = 0;
    uint64_t publicAddr[3] = {};
    uint64_t privateAddr[3] = {};
    uint8_t natType = 0;     // StunNatType, from protocol V1_2_0 (4); 0 = invalid
    OptionalBytes uniqueId;  // from protocol V1_2_3 (6)
};

// Tells the server which player a connecting client is: the server answers a connect request only once the
// UniqueId in it is registered (Simulation.RegisterUniqueIdPlayerMapping), otherwise it leaves it waiting.
// PlayerRef.FromIndex(playerIndex) is the client's PlayerRef, i.e. playerIndex = Join.playerRef - 1.
struct PlayerRefMapping {
    uint32_t actorId = 0;
    uint32_t playerIndex = 0;
    OptionalBytes uniqueId;
};

// Message id of the first message in the buffer, or -1 when the buffer is empty or truncated.
int FirstMessageId(const std::string& buffer);

bool ParseJoin(const std::string& buffer, Header& header, Join& join);
std::string BuildJoin(const Header& header, const Join& join);
bool ParseStart(const std::string& buffer, Header& header, Start& start);
std::string BuildStart(const Header& header, const Start& start);
bool ParseReflexiveInfo(const std::string& buffer, Header& header, ReflexiveInfo& info);
std::string BuildReflexiveInfo(const Header& header, const ReflexiveInfo& info);
bool ParsePlayerRefMapping(const std::string& buffer, Header& header, PlayerRefMapping& mapping);
std::string BuildPlayerRefMapping(const Header& header, const PlayerRefMapping& mapping);

// NetworkConfigSync: [Type:8][NetworkConfig: string, same presence-bit/16-bit-length form as byte[]]. The plugin
// asks the host for its config (Request) and forwards the host's Response to clients that asked in their Join.
bool ParseNetworkConfigSync(const std::string& buffer, Header& header, uint8_t& type, OptionalBytes& config);
std::string BuildNetworkConfigSync(const Header& header, uint8_t type, const OptionalBytes& config);

} // namespace PnFusion
