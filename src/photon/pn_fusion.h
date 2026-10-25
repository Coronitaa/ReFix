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

constexpr uint8_t MSG_JOIN = 1;
constexpr uint8_t JOIN_REQUEST = 1, JOIN_CONFIRMATION = 2;
constexpr uint8_t PEER_NONE = 0, PEER_SERVER = 1, PEER_CLIENT = 2;
constexpr uint8_t GAME_MODE_CLIENT_SERVER = 1, GAME_MODE_SHARED = 2;

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

// Message id of the first message in the buffer, or -1 when the buffer is empty or truncated.
int FirstMessageId(const std::string& buffer);

bool ParseJoin(const std::string& buffer, Header& header, Join& join);
std::string BuildJoin(const Header& header, const Join& join);

} // namespace PnFusion
