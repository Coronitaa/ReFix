#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Photon Quantum 2.x deterministic server-plugin protocol (Photon.Deterministic.Protocol).
//
// Quantum clients send Serializer bit-streams on Photon Event 100 (reliable control) and Event 101
// (unreliable control, e.g. RTT/checksums) to server actor 0, and local inputs on Event 102.
// The plugin answers with Joined, requests RuntimeConfig/SessionConfig from the host, starts the
// simulation with SimulationStart, and broadcasts verified tick packets on Event 102.
namespace PnQuantum {

constexpr uint8_t EV_PROTOCOL = 100;
constexpr uint8_t EV_UNRELIABLE = 101;
constexpr uint8_t EV_INPUT = 102;
constexpr int32_t SERVER_ACTOR = 0;
constexpr uint8_t P_QUANTUM_DATA = 245;

// Serializer message IDs registered in Photon.Deterministic.Protocol.Serializer.
constexpr uint8_t MSG_START_REQUEST = 1;
constexpr uint8_t MSG_JOINED = 2;
constexpr uint8_t MSG_SESSION_CONFIG = 3;
constexpr uint8_t MSG_RUNTIME_CONFIG = 4;
constexpr uint8_t MSG_SIMULATION_START = 5;
constexpr uint8_t MSG_RTT_UPDATE = 8;
constexpr uint8_t MSG_TICK_CHECKSUM = 10;
constexpr uint8_t MSG_COMMAND = 11;
constexpr uint8_t MSG_SET_PLAYER_DATA = 14;

constexpr int32_t JOIN_STATUS_FAILED = 0;
constexpr int32_t JOIN_STATUS_CONFIRMED = 1;
constexpr int32_t JOIN_STATUS_WAIT_FOR_SNAPSHOT = 2;

struct OptionalBytes {
    bool present = false;
    std::string bytes;
};

struct SessionConfigData {
    bool present = false;
    // Exact bit-packed layout of DeterministicSessionConfig.Serialize (647 payload bits).
    int32_t i0 = 2;        // +14h ChecksumInterval
    int32_t playerCount = 8; // +20h PlayerCount
    uint8_t b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0; // +19h, +10h, +18h, +25h, +26h
    int32_t i2 = 60, i3 = 60, i4 = 60;              // +5Ch, +60h, +64h
    int32_t updateFps = 60;                         // +28h UpdateFPS
    int32_t i6[13] = {};                            // +2Ch..+58h
    uint8_t b5 = 0, b6 = 0;                         // +24h, +68h
    int32_t i19 = 0;                                // +6Ch
};

struct StartRequest {
    OptionalBytes id;
    OptionalBytes protocolVersion;
    int32_t playerSlots = 1;
    int32_t initialTick = 0;
    int32_t playerCount = 8;
    bool rejoin = false;
};

struct Joined {
    int32_t status = JOIN_STATUS_CONFIRMED;
    std::vector<int32_t> playerSlots;
};

struct SessionConfigMsg {
    bool requested = false;
    SessionConfigData config;
};

struct RuntimeConfigMsg {
    bool requested = false;
    OptionalBytes config;
};

struct SimulationStart {
    bool reconnect = false;
    double serverTime = 0.0;
    OptionalBytes runtimeConfig;
    SessionConfigData sessionConfig;
};

struct ClientInput {
    uint8_t player = 0;
    int32_t tick = 0;
    OptionalBytes data;
    OptionalBytes rpc;
    uint8_t flags = 0;
};

struct ServerTickEntry {
    int32_t tick = 0;
    bool verified = true;
    uint64_t playerMask = 0;
    std::map<uint8_t, ClientInput> inputs; // keyed by player slot [0..63]
};

struct ActorState {
    int32_t actor = 0;
    int32_t protoVer = 7;
    std::vector<int32_t> slots;
    bool started = false;
    int32_t lastRttMs = 0;
};

struct RoomState {
    bool active = false;
    bool running = false;
    bool configRequested = false;
    int32_t configRequester = 0;
    bool hasSessionConfig = false;
    bool hasRuntimeConfig = false;
    SessionConfigData sessionConfig;
    OptionalBytes runtimeConfig;
    std::string protocolVersion;
    int32_t protoVer = 7;
    int32_t playerCount = 8;
    int32_t updateFps = 60;
    uint32_t startMs = 0;
    int32_t nextTick = 0;
    double lastPacketTime = 0.0;
    std::map<int32_t, ActorState> actors;
    std::map<int32_t, std::map<uint8_t, ClientInput>> pendingInputs; // tick -> (slot -> input)
};

// Maps a Quantum ProtocolVersion string (e.g. "2.2.0.0") to the internal version integer
// checked by Join.Serialize (>= 7 includes Rejoin) and Joined.Serialize (>= 6 uses int32 Status).
int ParseProtocolVersion(const std::string& ver);

// Returns true when data looks like a Quantum StartRequest (msgId 1 with two non-null strings).
bool DetectStartRequest(const std::string& buffer, std::string& versionOut);

struct ParsedMessage {
    uint8_t msgId = 0;
    std::string rawSlice; // byte-aligned slice for forwarding (e.g. Command / SetPlayerData)
    StartRequest startReq;
    SessionConfigMsg sessionCfg;
    RuntimeConfigMsg runtimeCfg;
    int32_t rtt = 0;
};

// Decodes one or more Serializer messages from an Event 100 or Event 101 payload.
bool ParseProtocolPacket(const std::string& buffer, int protoHint, std::vector<ParsedMessage>& out);

// Decodes one or more local client inputs from a client->server Event 102 payload.
bool ParseClientInputs(const std::string& buffer, std::vector<ClientInput>& out);

std::string BuildJoined(int protoVer, const Joined& msg);
std::string BuildSessionConfigRequest();
std::string BuildRuntimeConfigRequest();
std::string BuildSimulationStart(const SimulationStart& msg);
std::string BuildRttUpdate(int32_t rtt);

// Builds a server->client Event 102 input/tick packet matching DequeueNetworkRecvQueue.
std::string BuildServerTickPacket(int32_t rttMs, int32_t playerCount, double serverTimeSec,
                                  const std::vector<ServerTickEntry>& ticks);

} // namespace PnQuantum
