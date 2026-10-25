#pragma once
#include "pn_proto18.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// Photon LoadBalancing server emulator (NameServer, MasterServer and GameServer roles) for P2P play.
//
// NameServer and MasterServer are answered locally on every machine. The GameServer role runs on the host:
// its rooms hold the actors of local and remote players and relay events, properties and the event cache the
// same way Photon Cloud does. Transport and room discovery are injected through Environment, so this file
// knows nothing about sockets or Steam.
namespace PnServer {

enum class Role { NameServer, Master, Game };

struct Environment {
    std::string localUserId;                                          // e.g. own SteamID64
    std::string masterAddress;                                        // "127.0.0.1:27001"
    std::string localGameAddress;                                     // game server role of this machine
    std::function<std::string(const std::string& room)> gameAddress;  // where a room is served ("" unknown)
    std::function<std::string()> newRoomName;                         // name for CreateGame without a name
    std::function<std::vector<std::string>()> joinableRooms;          // candidates for JoinRandomGame
    std::function<void(const std::string& room, bool open)> roomHosted; // host-side room created / closed
    void (*log)(const char* fmt, ...) = nullptr; // printf-style; required
};

class Emulator {
public:
    // reliable/channel: transport options for this message.
    using SendFn = std::function<void(const std::string& message, bool reliable, uint8_t channel)>;

    explicit Emulator(Environment env);

    void OnConnect(uint64_t peerKey, Role role, const std::string& userId, SendFn send);
    void OnMessage(uint64_t peerKey, const std::string& message, bool reliable, uint8_t channel);
    void OnDisconnect(uint64_t peerKey);

    // True when this machine currently hosts the room (any app id).
    bool HostsRoom(const std::string& name) const;

private:
    struct Actor {
        uint64_t peerKey = 0;
        int32_t number = 0;
        PnProto18::Table props;
        std::set<uint8_t> groups;
    };
    struct CachedEvent {
        uint8_t code;
        int32_t sender;
        bool global;
        PnProto18::Raw data;   // empty when the event had no data
    };
    struct Room {
        std::string appId;
        std::string name;
        PnProto18::Table props;
        int32_t flags = 0;
        int32_t maxPlayers = 0;
        bool cleanupCacheOnLeave = true;
        bool publishUserId = false;
        int32_t nextActor = 1;
        int32_t master = 0;
        int32_t fusionServer = 0;   // actor that joined Fusion as Server (client/server mode), 0 if none
        std::map<int32_t, Actor> actors;
        std::vector<CachedEvent> cache;
    };
    struct Peer {
        Role role;
        std::string userId;
        std::string appId;
        SendFn send;
        std::string aesKey;   // payload encryption key once InitEncryption completed
        Room* room = nullptr;
        int32_t actor = 0;
    };

    void HandleOperation(Peer& peer, uint64_t key, uint8_t op, const PnProto18::Params& params, bool reliable, uint8_t channel);
    void HandleNameServer(Peer& peer, uint8_t op, const PnProto18::Params& params);
    void HandleMaster(Peer& peer, uint8_t op, const PnProto18::Params& params);
    void HandleGame(Peer& peer, uint64_t key, uint8_t op, const PnProto18::Params& params, bool reliable, uint8_t channel);
    void HandleInternal(Peer& peer, uint8_t op, const PnProto18::Params& params);

    void EnterRoom(Peer& peer, uint64_t key, uint8_t op, const PnProto18::Params& params);
    void RaiseEvent(Peer& peer, const PnProto18::Params& params, bool reliable, uint8_t channel);
    void HandleFusionProtocol(Peer& peer, const PnProto18::Raw& data);
    void SendFusionMessage(Room& room, int32_t actor, const std::string& buffer);
    void LogEventSample(const Peer& peer, uint8_t code, int64_t receivers, const std::vector<int32_t>& targets,
                        const PnProto18::Params& params);
    void SetProperties(Peer& peer, const PnProto18::Params& params);
    void ChangeGroups(Peer& peer, const PnProto18::Params& params);
    void LeaveRoom(uint64_t key, bool sendResponse);
    void RemoveFromCache(Room& room, uint8_t code, const std::vector<int32_t>& senders, const PnProto18::Raw& filter);
    void SetMaster(Room& room, int32_t actor);

    void Respond(Peer& peer, uint8_t op, int16_t code, const PnProto18::Params& params = {}, const std::string& message = "");
    void SendEvent(Room& room, int32_t actor, uint8_t code, const PnProto18::Params& params, bool reliable, uint8_t channel);
    PnProto18::Raw ActorTable(const Room& room) const;
    std::vector<int32_t> ActorNumbers(const Room& room) const;

    Environment m_env;
    std::map<uint64_t, Peer> m_peers;
    std::map<std::string, std::unique_ptr<Room>> m_rooms; // key: appId + '\n' + name
    int m_sampledEvents = 0;
};

} // namespace PnServer
