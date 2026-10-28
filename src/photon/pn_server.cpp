#include "pn_server.h"
#include "pn_crypto.h"
#include "pn_fusion.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace PnServer {

using namespace PnProto18;

namespace {

// Message types (ExitGames.Client.Photon.EgMessageType); 0x80 marks an encrypted body.
constexpr uint8_t kMagic = 0xF3;
constexpr uint8_t MT_INIT = 0, MT_INIT_RESPONSE = 1, MT_OPERATION = 2, MT_RESPONSE = 3, MT_EVENT = 4,
                  MT_INTERNAL = 6, MT_INTERNAL_RESPONSE = 7, MT_ENCRYPTED = 0x80;
constexpr size_t kInitAppIdOffset = 9, kInitAppIdSize = 32;

// Photon.Realtime OperationCode / ParameterCode / EventCode / GamePropertyKey / ErrorCode.
constexpr uint8_t OP_AUTHENTICATE = 230, OP_AUTHENTICATE_ONCE = 231, OP_JOIN_LOBBY = 229, OP_CREATE_GAME = 227,
                  OP_JOIN_GAME = 226, OP_JOIN_RANDOM = 225, OP_LEAVE = 254, OP_RAISE_EVENT = 253,
                  OP_SET_PROPERTIES = 252, OP_GET_PROPERTIES = 251, OP_CHANGE_GROUPS = 248, OP_GET_REGIONS = 220;
constexpr uint8_t OPI_INIT_ENCRYPTION = 0, OPI_PING = 1;
constexpr uint8_t P_ROOM_NAME = 255, P_ACTOR_NR = 254, P_TARGET_ACTOR = 253, P_ACTOR_LIST = 252, P_PROPERTIES = 251,
                  P_BROADCAST = 250, P_PLAYER_PROPS = 249, P_GAME_PROPS = 248, P_CACHE = 247, P_RECEIVERS = 246,
                  P_DATA = 245, P_CODE = 244, P_CLEANUP_CACHE = 241, P_GROUP = 240, P_REMOVE = 239,
                  P_PUBLISH_USER_ID = 239, P_ADD = 238, P_IS_INACTIVE = 233, P_EXPECTED = 231, P_ADDRESS = 230,
                  P_USER_ID = 225, P_GAME_LIST = 222, P_TOKEN = 221, P_JOIN_MODE = 215, P_REGION = 210,
                  P_MASTER_CLIENT = 203, P_CLUSTER = 196, P_ROOM_FLAGS = 191;
constexpr uint8_t EV_GAME_LIST = 230, EV_JOIN = 255, EV_LEAVE = 254, EV_PROPERTIES_CHANGED = 253;
constexpr uint8_t GP_MAX_PLAYERS = 255, GP_IS_OPEN = 253, GP_MASTER_CLIENT = 248, GP_MAX_PLAYERS_INT = 243;
constexpr uint8_t AP_USER_ID = 253;
constexpr int16_t ERR_GAME_EXISTS = 32766, ERR_GAME_FULL = 32765, ERR_GAME_CLOSED = 32764,
                  ERR_NO_RANDOM_MATCH = 32760, ERR_GAME_MISSING = 32758, ERR_INVALID_OPERATION = -2;
constexpr uint8_t JOIN_MODE_CREATE_IF_MISSING = 1;
// RoomOptionFlags and EventCaching.
constexpr int32_t RF_DELETE_CACHE_ON_LEAVE = 2, RF_SUPPRESS_ROOM_EVENTS = 4, RF_PUBLISH_USER_ID = 8,
                  RF_DELETE_NULL_PROPS = 16, RF_BROADCAST_PROPS_TO_ALL = 32;
constexpr int64_t CACHE_ADD = 4, CACHE_ADD_GLOBAL = 5, CACHE_REMOVE = 6, CACHE_REMOVE_LEFT = 7;
constexpr int64_t RECEIVERS_OTHERS = 0, RECEIVERS_ALL = 1, RECEIVERS_MASTER = 2;

const char* const kRegions[] = { "eu", "us", "usw", "asia", "jp", "au", "sa", "cae", "kr", "in", "za", "ru", "rue", "tr", "uae", "hk" };

int64_t IntParam(const Params& p, uint8_t code, int64_t fallback) {
    auto it = p.find(code);
    int64_t v;
    return it != p.end() && AsInt(it->second, v) ? v : fallback;
}

bool BoolParam(const Params& p, uint8_t code, bool fallback) {
    auto it = p.find(code);
    bool v;
    return it != p.end() && AsBool(it->second, v) ? v : fallback;
}

std::string StrParam(const Params& p, uint8_t code) {
    auto it = p.find(code);
    std::string v;
    return it != p.end() && AsString(it->second, v) ? v : std::string();
}

Table TableParam(const Params& p, uint8_t code) {
    auto it = p.find(code);
    Table t;
    if (it != p.end()) AsTable(it->second, t);
    return t;
}

// ByteArray parameter; returns false when absent or null.
bool ByteArrayParam(const Params& p, uint8_t code, std::vector<uint8_t>& out) {
    auto it = p.find(code);
    if (it == p.end() || IsNull(it->second)) return false;
    Reader r(it->second);
    if (r.U8() != kByteArray) return false;
    uint32_t n = r.VarU32();
    if (!r.Has(n)) return false;
    out.assign(r.data + r.pos, r.data + r.pos + n);
    return true;
}

Raw ByteArray(const std::string& bytes) {
    Raw r(1, (char)kByteArray);
    PutVarU32(r, (uint32_t)bytes.size());
    return r + bytes;
}

std::string Message(uint8_t type, const Bytes& body) {
    return std::string{ (char)kMagic, (char)type } + body;
}

} // namespace

Emulator::Emulator(Environment env) : m_env(std::move(env)) {}

bool Emulator::HostsRoom(const std::string& name) const {
    for (const auto& [key, room] : m_rooms) if (room->name == name) return true;
    return false;
}

Table Emulator::LocalPublicRooms() const {
    Table gameList;
    for (const auto& [key, room] : m_rooms) {
        bool open = true;
        if (const Raw* v = room->props.Find(Byte(GP_IS_OPEN))) AsBool(*v, open);
        bool visible = true;
        if (const Raw* v = room->props.Find(Byte(254))) AsBool(*v, visible);
        if (!visible) continue;
        Table props = room->props;
        props.Set(Byte(GP_MAX_PLAYERS), Int(room->maxPlayers));
        props.Set(Byte(GP_IS_OPEN), Bool(open));
        props.Set(Byte(252), Int((int32_t)room->actors.size()));
        gameList.Set(Str(room->name), props.Encode());
    }
    return gameList;
}

void Emulator::OnConnect(uint64_t peerKey, Role role, const std::string& userId, SendFn send) {
    Peer peer;
    peer.role = role;
    peer.userId = userId;
    peer.send = std::move(send);
    m_peers[peerKey] = std::move(peer);
}

void Emulator::OnDisconnect(uint64_t peerKey) {
    LeaveRoom(peerKey, false);
    m_peers.erase(peerKey);
}

void Emulator::OnMessage(uint64_t peerKey, const std::string& message, bool reliable, uint8_t channel) {
    auto it = m_peers.find(peerKey);
    if (it == m_peers.end() || message.size() < 2 || (uint8_t)message[0] != kMagic) return;
    Peer& peer = it->second;
    uint8_t type = (uint8_t)message[1] & 0x7F;
    bool encrypted = ((uint8_t)message[1] & MT_ENCRYPTED) != 0;
    Bytes body = message.substr(2);

    if (type == MT_INIT) {
        if (message.size() >= kInitAppIdOffset + kInitAppIdSize) {
            std::string appId = message.substr(kInitAppIdOffset, kInitAppIdSize);
            peer.appId = appId.substr(0, appId.find('\0'));
        }
        peer.send(Message(MT_INIT_RESPONSE, std::string(1, '\0')), true, 0);
        return;
    }
    if (encrypted) {
        Bytes plain;
        if (peer.aesKey.empty() || !PnCrypto::AesCbcDecrypt(peer.aesKey, body, plain)) {
            m_env.log("[Photon] Could not decrypt a type %u message (%zu bytes)", type, body.size());
            return;
        }
        body = plain;
    }
    uint8_t op;
    Params params;
    if (!ParseOperationRequest(body, op, params)) {
        m_env.log("[Photon] Malformed request (type %u, %zu bytes)", type, body.size());
        return;
    }
    if (type == MT_INTERNAL) HandleInternal(peer, op, params);
    else if (type == MT_OPERATION) HandleOperation(peer, peerKey, op, params, reliable, channel);
}

void Emulator::HandleInternal(Peer& peer, uint8_t op, const Params& params) {
    Params out;
    if (op == OPI_INIT_ENCRYPTION) {
        std::vector<uint8_t> clientKey;
        PnDiffieHellman dh;
        if (!ByteArrayParam(params, 1, clientKey) ||
            !dh.DeriveKey(std::string(clientKey.begin(), clientKey.end()), peer.aesKey)) {
            m_env.log("[Photon] InitEncryption without a usable client key");
        }
        out[1] = ByteArray(dh.PublicKey());
    } else if (op == OPI_PING) {
        auto it = params.find(1);
        if (it != params.end()) out[1] = it->second;
        out[2] = Int((int32_t)GetTickCount());
    }
    peer.send(Message(MT_INTERNAL_RESPONSE, OperationResponse(op, 0, "", out)), true, 0);
}

void Emulator::HandleOperation(Peer& peer, uint64_t key, uint8_t op, const Params& params, bool reliable, uint8_t channel) {
    if (peer.role != Role::Game || (op != OP_RAISE_EVENT && op != OP_SET_PROPERTIES)) {
        static const char* const kRoleNames[] = { "NameServer", "Master", "Game" };
        m_env.log("[Photon] %s op %u from %s (%zu params)", kRoleNames[(int)peer.role], op, peer.userId.c_str(), params.size());
    }
    switch (peer.role) {
        case Role::NameServer: HandleNameServer(peer, op, params); break;
        case Role::Master: HandleMaster(peer, op, params); break;
        case Role::Game: HandleGame(peer, key, op, params, reliable, channel); break;
    }
}

void Emulator::HandleNameServer(Peer& peer, uint8_t op, const Params& params) {
    if (op == OP_AUTHENTICATE || op == OP_AUTHENTICATE_ONCE) {
        Respond(peer, op, 0, { { P_ADDRESS, Str(m_env.masterAddress) }, { P_USER_ID, Str(peer.userId) },
                               { P_TOKEN, Str("refix") }, { P_CLUSTER, Str("default") } });
    } else if (op == OP_GET_REGIONS) {
        std::vector<std::string> regions(std::begin(kRegions), std::end(kRegions));
        std::vector<std::string> addresses(regions.size(), m_env.masterAddress);
        Respond(peer, op, 0, { { P_REGION, StrArray(regions) }, { P_ADDRESS, StrArray(addresses) } });
    } else {
        Respond(peer, op, 0);
    }
}

void Emulator::HandleMaster(Peer& peer, uint8_t op, const Params& params) {
    switch (op) {
        case OP_AUTHENTICATE:
        case OP_AUTHENTICATE_ONCE:
            Respond(peer, op, 0, { { P_USER_ID, Str(peer.userId) }, { P_TOKEN, Str("refix") } });
            return;
        case OP_JOIN_LOBBY: {
            Respond(peer, op, 0);
            Table gameList = LocalPublicRooms();
            if (m_env.publicLobbies) {
                for (const auto& [name, props] : m_env.publicLobbies()) {
                    if (!gameList.Find(Str(name))) {
                        gameList.Set(Str(name), props.Encode());
                    }
                }
            }
            peer.send(Message(MT_EVENT, EventData(EV_GAME_LIST, { { P_GAME_LIST, gameList.Encode() } })), true, 0);
            return;
        }
        case OP_CREATE_GAME: {
            std::string name = StrParam(params, P_ROOM_NAME);
            if (name.empty()) name = m_env.newRoomName();
            Respond(peer, op, 0, { { P_ADDRESS, Str(m_env.localGameAddress) }, { P_ROOM_NAME, Str(name) } });
            return;
        }
        case OP_JOIN_GAME: {
            std::string name = StrParam(params, P_ROOM_NAME);
            std::string address = HostsRoom(name) ? m_env.localGameAddress : m_env.gameAddress(name);
            if (address.empty() && IntParam(params, P_JOIN_MODE, 0) == JOIN_MODE_CREATE_IF_MISSING) address = m_env.localGameAddress;
            m_env.log("[Photon] JoinGame '%s' -> %s", name.c_str(), address.empty() ? "<not found>" : address.c_str());
            if (address.empty()) {
                Respond(peer, op, ERR_GAME_MISSING, {}, "Game does not exist");
                return;
            }
            Respond(peer, op, 0, { { P_ADDRESS, Str(address) }, { P_ROOM_NAME, Str(name) } });
            return;
        }
        case OP_JOIN_RANDOM: {
            for (const std::string& name : m_env.joinableRooms()) {
                std::string address = m_env.gameAddress(name);
                if (address.empty()) continue;
                Respond(peer, op, 0, { { P_ADDRESS, Str(address) }, { P_ROOM_NAME, Str(name) } });
                return;
            }
            if (IntParam(params, P_JOIN_MODE, 0) == JOIN_MODE_CREATE_IF_MISSING) {
                std::string name = StrParam(params, P_ROOM_NAME);
                if (name.empty()) name = m_env.newRoomName();
                Respond(peer, op, 0, { { P_ADDRESS, Str(m_env.localGameAddress) }, { P_ROOM_NAME, Str(name) } });
                return;
            }
            Respond(peer, op, ERR_NO_RANDOM_MATCH, {}, "No match found");
            return;
        }
        default:
            Respond(peer, op, 0);
            return;
    }
}

void Emulator::HandleGame(Peer& peer, uint64_t key, uint8_t op, const Params& params, bool reliable, uint8_t channel) {
    switch (op) {
        case OP_AUTHENTICATE:
        case OP_AUTHENTICATE_ONCE: Respond(peer, op, 0, { { P_USER_ID, Str(peer.userId) } }); break;
        case OP_CREATE_GAME:
        case OP_JOIN_GAME: EnterRoom(peer, key, op, params); break;
        case OP_RAISE_EVENT: RaiseEvent(peer, params, reliable, channel); break;
        case OP_SET_PROPERTIES: SetProperties(peer, params); break;
        case OP_CHANGE_GROUPS: ChangeGroups(peer, params); break;
        case OP_LEAVE: LeaveRoom(key, true); break;
        case OP_GET_PROPERTIES:
            if (peer.room) Respond(peer, op, 0, { { P_GAME_PROPS, peer.room->props.Encode() }, { P_PLAYER_PROPS, ActorTable(*peer.room) } });
            else Respond(peer, op, ERR_INVALID_OPERATION);
            break;
        default: Respond(peer, op, 0); break;
    }
}

void Emulator::EnterRoom(Peer& peer, uint64_t key, uint8_t op, const Params& params) {
    std::string name = StrParam(params, P_ROOM_NAME);
    if (name.empty()) name = m_env.newRoomName();
    std::string roomKey = peer.appId + '\n' + name;
    auto it = m_rooms.find(roomKey);
    bool exists = it != m_rooms.end();
    bool create = op == OP_CREATE_GAME || (!exists && IntParam(params, P_JOIN_MODE, 0) == JOIN_MODE_CREATE_IF_MISSING);
    if (op == OP_CREATE_GAME && exists) { Respond(peer, op, ERR_GAME_EXISTS, {}, "Game already exists"); return; }
    if (!exists && !create) { Respond(peer, op, ERR_GAME_MISSING, {}, "Game does not exist"); return; }
    if (peer.room) LeaveRoom(key, false);

    if (!exists) {
        auto room = std::make_unique<Room>();
        room->appId = peer.appId;
        room->name = name;
        room->props = TableParam(params, P_GAME_PROPS);
        room->flags = (int32_t)IntParam(params, P_ROOM_FLAGS, 0);
        int64_t maxPlayers = 0;
        if (const Raw* v = room->props.Find(Byte(GP_MAX_PLAYERS))) AsInt(*v, maxPlayers);
        if (const Raw* v = room->props.Find(Byte(GP_MAX_PLAYERS_INT))) AsInt(*v, maxPlayers);
        room->maxPlayers = (int32_t)maxPlayers;
        room->cleanupCacheOnLeave = BoolParam(params, P_CLEANUP_CACHE, true) || (room->flags & RF_DELETE_CACHE_ON_LEAVE);
        room->publishUserId = BoolParam(params, P_PUBLISH_USER_ID, false) || (room->flags & RF_PUBLISH_USER_ID);
        it = m_rooms.emplace(roomKey, std::move(room)).first;
        m_env.roomHosted(name, true);
        m_env.log("[Photon] Room '%s' created (app %.8s..., max %d)", name.c_str(), peer.appId.c_str(), (int)maxPlayers);
    }
    Room& room = *it->second;
    // IsOpen only gates later joins: like Photon Cloud, the creator always enters (Fusion creates rooms closed).
    bool open = true;
    if (const Raw* v = room.props.Find(Byte(GP_IS_OPEN))) AsBool(*v, open);
    if (!open && exists && !room.fusionShared && !m_env.startFusionShared) { Respond(peer, op, ERR_GAME_CLOSED, {}, "Game closed"); return; }
    if (room.maxPlayers > 0 && (int32_t)room.actors.size() >= room.maxPlayers) { Respond(peer, op, ERR_GAME_FULL, {}, "Game full"); return; }

    Actor actor;
    actor.peerKey = key;
    actor.number = room.nextActor++;
    actor.props = TableParam(params, P_PLAYER_PROPS);
    if (room.publishUserId) actor.props.Set(Byte(AP_USER_ID), Str(peer.userId));
    int32_t number = actor.number;
    room.actors[number] = std::move(actor);
    peer.room = &room;
    peer.actor = number;
    if (!room.master) SetMaster(room, number);

    Respond(peer, op, 0, { { P_ACTOR_NR, Int(number) }, { P_PLAYER_PROPS, ActorTable(room) },
                           { P_GAME_PROPS, room.props.Encode() }, { P_ACTOR_LIST, IntArray(ActorNumbers(room)) },
                           { P_MASTER_CLIENT, Int(room.master) } });
    if (!(room.flags & RF_SUPPRESS_ROOM_EVENTS)) {
        Params join = { { P_ACTOR_NR, Int(number) }, { P_PLAYER_PROPS, room.actors[number].props.Encode() },
                        { P_ACTOR_LIST, IntArray(ActorNumbers(room)) } };
        for (const auto& [nr, a] : room.actors) SendEvent(room, nr, EV_JOIN, join, true, 0);
    }
    for (const CachedEvent& ev : room.cache) {
        Params p = { { P_ACTOR_NR, Int(ev.sender) } };
        if (!ev.data.empty()) p[P_DATA] = ev.data;
        SendEvent(room, number, ev.code, p, true, 0);
    }
    m_env.log("[Photon] Actor %d (%s) joined '%s' (%zu actors, master %d)", number, peer.userId.c_str(), room.name.c_str(),
              room.actors.size(), room.master);
}

// Photon Cloud runs a "Fusion" plugin in Fusion rooms that answers the protocol messages peers send to the
// server actor; peers time out without them. The emulator plays its client/server part:
//  - Join is confirmed to the sender with its PlayerRef, then Start tells it which actor runs the Fusion
//    server (the peer that joined as Server).
//  - A client whose Join asks for the NetworkConfig is started only after the host's config was requested
//    (NetworkConfigSync Request) and the host's Response forwarded to it.
//  - ReflexiveInfo (NAT-punch addresses) is relayed between clients and the host unchanged. A client only
//    connects once it has the server's ReflexiveInfo, and Fusion hosts here never send one, so the emulator
//    sends each client one without addresses shortly after Start: the client then connects through the relay
//    (Photon events to the server's actor), which is what the Steam tunnel carries.
void Emulator::HandleFusionProtocol(Peer& peer, const Raw& data) {
    std::string buffer;
    if (!AsBytes(data, buffer) || !peer.room) return;
    Room& room = *peer.room;
    switch (PnFusion::FirstMessageId(buffer)) {
        case PnFusion::MSG_JOIN: HandleFusionJoin(peer, room, buffer); return;
        case PnFusion::MSG_NETWORK_CONFIG_SYNC: HandleFusionConfigSync(peer, room, buffer); return;
        case PnFusion::MSG_REFLEXIVE_INFO:
            if (peer.actor == room.fusionServer) {
                room.fusionRelayInfo.clear(); // the host's own addresses replace the relay-only ones
                for (const auto& [nr, a] : room.actors)
                    if (nr != room.fusionServer) SendFusionMessage(room, nr, buffer);
            } else if (room.fusionServer) {
                SendFusionMessage(room, room.fusionServer, buffer);
            }
            return;
        default:
            m_env.log("[Photon] Fusion: unhandled protocol message %d from actor %d (%zu bytes)",
                      PnFusion::FirstMessageId(buffer), peer.actor, buffer.size());
    }
}

void Emulator::HandleFusionJoin(Peer& peer, Room& room, const std::string& buffer) {
    PnFusion::Header header;
    PnFusion::Join join;
    if (!PnFusion::ParseJoin(buffer, header, join) || join.type != PnFusion::JOIN_REQUEST) return;
    if (join.gameMode == PnFusion::GAME_MODE_SHARED) {
        HandleFusionSharedJoin(peer, room, header, join);
        return;
    }
    if (join.peerMode == PnFusion::PEER_SERVER) room.fusionServer = peer.actor;
    join.type = PnFusion::JOIN_CONFIRMATION;
    join.playerRef = peer.actor;
    if (join.uniqueId.bytes.size() != PnFusion::UNIQUE_ID_SIZE) join.uniqueId = { true, FusionUniqueId(peer) };
    room.fusionUniqueIds[peer.actor] = join.uniqueId.bytes;
    SendFusionMessage(room, peer.actor, PnFusion::BuildJoin(header, join));

    bool wantsConfig = (join.joinRequests & PnFusion::JOIN_REQUEST_NETWORK_CONFIG) && room.fusionServer &&
                       room.fusionServer != peer.actor;
    if (wantsConfig) {
        room.fusionConfigWaiters[peer.actor] = header;
        SendFusionMessage(room, room.fusionServer,
                          PnFusion::BuildNetworkConfigSync(header, PnFusion::SYNC_REQUEST, PnFusion::OptionalBytes()));
        m_env.log("[Photon] Fusion: actor %d joined; NetworkConfig requested from server actor %d", peer.actor,
                  room.fusionServer);
        return;
    }
    StartFusionPeer(room, peer.actor, header);
    m_env.log("[Photon] Fusion: actor %d joined and started (mode %u, peer %u, protocol %u, Fusion %d.%d.%d)", peer.actor,
              join.gameMode, join.peerMode, header.protocolVersion, header.versionMajor, header.versionMinor,
              header.versionBuild);
}

void Emulator::HandleFusionConfigSync(Peer& peer, Room& room, const std::string& buffer) {
    PnFusion::Header header;
    uint8_t type = 0;
    PnFusion::OptionalBytes config;
    if (room.fusionShared) {
        if (!PnFusion::ParseNetworkConfigSync(buffer, header, type, config) || type != PnFusion::SYNC_RESPONSE ||
            !config.present || room.fusionSharedConfigSent) {
            return;
        }
        room.fusionSharedConfigSent = true;
        room.fusionShared->Config(room.maxPlayers, config.bytes);
        m_env.log("[Photon] Fusion Shared: actor %d sent the NetworkProjectConfig (%zu bytes)", peer.actor,
                  config.bytes.size());
        return;
    }
    if (peer.actor != room.fusionServer || !PnFusion::ParseNetworkConfigSync(buffer, header, type, config)) {
        m_env.log("[Photon] Fusion: ignored NetworkConfigSync from actor %d", peer.actor);
        return;
    }
    if (type == PnFusion::SYNC_OVERRIDE) { // host pushes a new config to everyone
        for (const auto& [nr, a] : room.actors)
            if (nr != room.fusionServer) SendFusionMessage(room, nr, buffer);
        return;
    }
    auto waiters = std::move(room.fusionConfigWaiters);
    room.fusionConfigWaiters.clear();
    for (const auto& [actor, clientHeader] : waiters) {
        if (!room.actors.count(actor)) continue;
        SendFusionMessage(room, actor, buffer);
        StartFusionPeer(room, actor, clientHeader);
        m_env.log("[Photon] Fusion: forwarded host NetworkConfig (%zu bytes) to actor %d and started it",
                  config.bytes.size(), actor);
    }
}

// Shared mode: every client connects to the plugin's server simulation (actor 0), which the emulator runs in the
// companion process. The first client is asked for its NetworkProjectConfig, the server is configured from it,
// and each client is started (ConnectToShared) once that server is running.
void Emulator::HandleFusionSharedJoin(Peer& peer, Room& room, const PnFusion::Header& header, PnFusion::Join join) {
    if (!room.fusionShared && m_env.startFusionShared) room.fusionShared = m_env.startFusionShared();
    if (!room.fusionShared) {
        m_env.log("[Photon] Fusion Shared: no server simulation available, actor %d cannot start", peer.actor);
        return;
    }
    join.type = PnFusion::JOIN_CONFIRMATION;
    join.playerRef = peer.actor;
    if (join.uniqueId.bytes.size() != PnFusion::UNIQUE_ID_SIZE) join.uniqueId = { true, FusionUniqueId(peer) };
    room.fusionUniqueIds[peer.actor] = join.uniqueId.bytes;
    room.fusionShared->Join(peer.actor, join.uniqueId.bytes);
    room.fusionShared->Master(room.master);

    join.joinRequests &= ~(PnFusion::JOIN_REQUEST_NETWORK_CONFIG | PnFusion::JOIN_REQUEST_REFLEXIVE_INFO);
    if (!room.fusionSharedConfigSent && !room.fusionSharedConfigActor) {
        room.fusionSharedConfigActor = peer.actor;
        join.joinRequests |= PnFusion::JOIN_REQUEST_NETWORK_CONFIG;
    }
    SendFusionMessage(room, peer.actor, PnFusion::BuildJoin(header, join));
    m_env.log("[Photon] Fusion Shared: actor %d joined (protocol %u, Fusion %d.%d.%d)%s", peer.actor,
              header.protocolVersion, header.versionMajor, header.versionMinor, header.versionBuild,
              room.fusionSharedConfigActor == peer.actor ? ", NetworkProjectConfig requested" : "");
    if (room.fusionSharedRunning) StartFusionSharedPeer(room, peer.actor, header);
    else room.fusionConfigWaiters[peer.actor] = header;
}

void Emulator::StartFusionSharedPeer(Room& room, int32_t actor, const PnFusion::Header& header) {

    PnFusion::Start start;
    start.remoteServerId = PnFusion::SERVER_ACTOR;
    start.startRequests = PnFusion::START_REQUEST_CONNECT_TO_SHARED;
    SendFusionMessage(room, actor, PnFusion::BuildStart(header, start));
    m_env.log("[Photon] Fusion Shared: actor %d started", actor);
}

void Emulator::PumpFusionShared(Room& room) {
    bool started = room.fusionShared->Poll(m_nowMs, [&](int32_t target, const std::string& packet, bool reliable) {
        if (!room.actors.count(target)) return;
        Params ev = { { P_ACTOR_NR, Int(PnFusion::SERVER_ACTOR) }, { P_DATA, ByteArray(packet) } };
        SendEvent(room, target, PnFusion::EV_DATA, ev, reliable, reliable ? 1 : 0);
    });
    if (!started || room.fusionSharedRunning) return;
    room.fusionSharedRunning = true;
    auto waiters = std::move(room.fusionConfigWaiters);
    room.fusionConfigWaiters.clear();
    for (const auto& [actor, header] : waiters)
        if (room.actors.count(actor)) StartFusionSharedPeer(room, actor, header);
}

void Emulator::StartFusionPeer(Room& room, int32_t actor, const PnFusion::Header& header) {
    PnFusion::Start start;
    start.remoteServerId = (uint32_t)(room.fusionServer ? room.fusionServer : room.master);
    SendFusionMessage(room, actor, PnFusion::BuildStart(header, start));
    if (room.fusionServer && actor != room.fusionServer) ScheduleServerReflexiveInfo(room, actor, header);
}

// Photon Cloud's plugin gives every peer an 8-byte UniqueId in its Join confirmation; a client cannot connect
// without one ("UniqueId is required"). The SteamID is unique per player and never zero.
std::string Emulator::FusionUniqueId(const Peer& peer) const {
    uint64_t id = std::strtoull(peer.userId.c_str(), nullptr, 10);
    if (!id) id = std::hash<std::string>()(peer.userId) | 1;
    std::string bytes(PnFusion::UNIQUE_ID_SIZE, '\0');
    for (size_t i = 0; i < bytes.size(); i++) bytes[i] = (char)(id >> (8 * i));
    return bytes;
}

// The client handles Start asynchronously and waits only ~100 ms for it to finish when a ReflexiveInfo arrives
// (it then fails with StartGameException), so the message is held back for a moment instead of following Start.
void Emulator::ScheduleServerReflexiveInfo(Room& room, int32_t actor, const PnFusion::Header& header) {
    constexpr uint32_t kReflexiveInfoDelayMs = 1500;
    PnFusion::ReflexiveInfo info;
    info.actorId = (uint32_t)room.fusionServer;
    room.fusionRelayInfo[actor] = { m_nowMs + kReflexiveInfoDelayMs, PnFusion::BuildReflexiveInfo(header, info) };

    // The server maps the UniqueId in a client's connect request to that client's actor from the client's own
    // ReflexiveInfo, which these clients never send, so the plugin sends it on their behalf (no addresses).
    const std::string& uniqueId = room.fusionUniqueIds[actor];
    PnFusion::ReflexiveInfo client;
    client.actorId = (uint32_t)actor;
    client.uniqueId = { true, uniqueId };
    SendFusionMessage(room, room.fusionServer, PnFusion::BuildReflexiveInfo(header, client));
    // ...and only accepts the connect once that UniqueId is registered as the client's player.
    PnFusion::PlayerRefMapping mapping;
    mapping.actorId = (uint32_t)actor;
    mapping.playerIndex = (uint32_t)actor; // same value as the client's Join.playerRef; the host is player 1
    mapping.uniqueId = { true, uniqueId };
    SendFusionMessage(room, room.fusionServer, PnFusion::BuildPlayerRefMapping(header, mapping));
}

void Emulator::Tick(uint32_t nowMs) {
    m_nowMs = nowMs;
    for (auto& [key, room] : m_rooms) {
        if (room->fusionShared) PumpFusionShared(*room);
        for (auto it = room->fusionRelayInfo.begin(); it != room->fusionRelayInfo.end();) {
            if ((int32_t)(nowMs - it->second.dueMs) < 0) {
                ++it;
                continue;
            }
            if (room->actors.count(it->first)) {
                SendFusionMessage(*room, it->first, it->second.buffer);
                m_env.log("[Photon] Fusion: sent server actor %d's relay ReflexiveInfo to actor %d", room->fusionServer,
                          it->first);
            }
            it = room->fusionRelayInfo.erase(it);
        }
    }
}

void Emulator::SendFusionMessage(Room& room, int32_t actor, const std::string& buffer) {
    Params ev = { { P_ACTOR_NR, Int(PnFusion::SERVER_ACTOR) }, { PnFusion::P_FUSION_DATA, ByteArray(buffer) } };
    SendEvent(room, actor, PnFusion::EV_PROTOCOL, ev, true, 0);
}

// Diagnostic: the first events of each session, with their parameter codes and a hex prefix of each value, so
// server-plugin protocols (e.g. Photon Fusion's handshake to the server actor) can be read from ReFix.log.
void Emulator::LogEventSample(const Peer& peer, uint8_t code, int64_t receivers, const std::vector<int32_t>& targets,
                              const Params& params) {
    constexpr int kMaxSampledEvents = 24;
    constexpr size_t kMaxHexBytes = 96;
    if (m_sampledEvents >= kMaxSampledEvents) return;
    m_sampledEvents++;
    std::string detail;
    char buf[8];
    for (const auto& [key, value] : params) {
        std::snprintf(buf, sizeof(buf), " %u=", key);
        detail += buf;
        for (size_t i = 0; i < value.size() && i < kMaxHexBytes; i++) {
            std::snprintf(buf, sizeof(buf), "%02x", (uint8_t)value[i]);
            detail += buf;
        }
        if (value.size() > kMaxHexBytes) detail += "..";
    }
    m_env.log("[Photon] Event %u from actor %d (receivers %lld, %zu targets%s%d):%s", code, peer.actor,
              (long long)receivers, targets.size(), targets.empty() ? "" : " first ", targets.empty() ? 0 : targets[0],
              detail.c_str());
}

// Diagnostic: Fusion's peer-to-peer packets travel as DATA events between two actors; the first few of every
// sender -> target pair are logged with their size, so a direction that never carries anything shows up.
void Emulator::LogRelaySample(int32_t sender, const std::vector<int32_t>& targets, size_t size, bool reliable,
                              uint8_t channel) {
    constexpr int kMaxPerPair = 10;
    int32_t target = targets.empty() ? -1 : targets[0];
    int& count = m_relaySamples[{ sender, target }];
    if (count >= kMaxPerPair) return;
    count++;
    m_env.log("[Photon] Relay data actor %d -> %d (%zu bytes, %s, channel %u, #%d)", sender, target, size,
              reliable ? "reliable" : "unreliable", channel, count);
}

void Emulator::RaiseEvent(Peer& peer, const Params& params, bool reliable, uint8_t channel) {
    if (!peer.room) return;
    Room& room = *peer.room;
    uint8_t code = (uint8_t)IntParam(params, P_CODE, 0);
    int64_t cache = IntParam(params, P_CACHE, 0);
    int64_t receivers = IntParam(params, P_RECEIVERS, RECEIVERS_OTHERS);
    int64_t group = IntParam(params, P_GROUP, 0);
    auto dataIt = params.find(P_DATA);
    Raw data = dataIt != params.end() ? dataIt->second : Raw();
    std::vector<int32_t> targets;
    if (params.count(P_ACTOR_LIST)) AsIntArray(params.at(P_ACTOR_LIST), targets);
    if (code == PnFusion::EV_DATA) LogRelaySample(peer.actor, targets, data.size(), reliable, channel);
    else LogEventSample(peer, code, receivers, targets, params);
    if (code == PnFusion::EV_PROTOCOL && targets.size() == 1 && targets[0] == PnFusion::SERVER_ACTOR) {
        HandleFusionProtocol(peer, data);
        return;
    }
    if (code == PnFusion::EV_DATA && room.fusionShared && targets.size() == 1 && targets[0] == PnFusion::SERVER_ACTOR) {
        std::string packet;
        if (AsBytes(data, packet)) room.fusionShared->Data(peer.actor, packet, reliable);
        return;
    }

    if (cache == CACHE_REMOVE) { RemoveFromCache(room, code, targets, data); return; }
    if (cache == CACHE_REMOVE_LEFT) {
        room.cache.erase(std::remove_if(room.cache.begin(), room.cache.end(),
                                        [&](const CachedEvent& e) { return !e.global && !room.actors.count(e.sender); }),
                         room.cache.end());
        return;
    }
    if (cache == CACHE_ADD || cache == CACHE_ADD_GLOBAL) room.cache.push_back({ code, peer.actor, cache == CACHE_ADD_GLOBAL, data });

    Params ev = { { P_ACTOR_NR, Int(peer.actor) } };
    if (!data.empty()) ev[P_DATA] = data;
    if (!targets.empty()) {
        for (int32_t nr : targets) SendEvent(room, nr, code, ev, reliable, channel);
        return;
    }
    for (const auto& [nr, actor] : room.actors) {
        bool wanted = receivers == RECEIVERS_ALL || (receivers == RECEIVERS_OTHERS && nr != peer.actor) ||
                      (receivers == RECEIVERS_MASTER && nr == room.master);
        if (wanted && group && !actor.groups.count((uint8_t)group)) wanted = false;
        if (wanted) SendEvent(room, nr, code, ev, reliable, channel);
    }
}

void Emulator::RemoveFromCache(Room& room, uint8_t code, const std::vector<int32_t>& senders, const Raw& filter) {
    Table filterTable;
    bool hasFilter = !filter.empty() && AsTable(filter, filterTable) && !filterTable.entries.empty();
    room.cache.erase(std::remove_if(room.cache.begin(), room.cache.end(), [&](const CachedEvent& e) {
        if (code && e.code != code) return false;
        if (!senders.empty() && std::find(senders.begin(), senders.end(), e.sender) == senders.end()) return false;
        if (!hasFilter) return true;
        Table data;
        return AsTable(e.data, data) && data.Contains(filterTable);
    }), room.cache.end());
}

void Emulator::SetProperties(Peer& peer, const Params& params) {
    if (!peer.room) { Respond(peer, OP_SET_PROPERTIES, ERR_INVALID_OPERATION); return; }
    Room& room = *peer.room;
    int32_t target = (int32_t)IntParam(params, P_ACTOR_NR, 0);
    Table changes = TableParam(params, P_PROPERTIES);
    Table* props = &room.props;
    if (target) {
        auto it = room.actors.find(target);
        if (it == room.actors.end()) { Respond(peer, OP_SET_PROPERTIES, ERR_INVALID_OPERATION, {}, "Actor not found"); return; }
        props = &it->second.props;
    }
    if (params.count(P_EXPECTED) && !props->Contains(TableParam(params, P_EXPECTED))) {
        Respond(peer, OP_SET_PROPERTIES, ERR_INVALID_OPERATION, {}, "Expected values mismatch");
        return;
    }
    props->Merge(changes, (room.flags & RF_DELETE_NULL_PROPS) != 0);
    if (!target) {
        int64_t master;
        if (const Raw* v = changes.Find(Byte(GP_MASTER_CLIENT)); v && AsInt(*v, master) && room.actors.count((int32_t)master)) {
            room.master = (int32_t)master;
            if (room.fusionShared) room.fusionShared->Master(room.master);
        }
    }
    Respond(peer, OP_SET_PROPERTIES, 0);
    Params ev = { { P_TARGET_ACTOR, Int(target) }, { P_PROPERTIES, changes.Encode() }, { P_ACTOR_NR, Int(peer.actor) } };
    bool toAll = (room.flags & RF_BROADCAST_PROPS_TO_ALL) != 0;
    for (const auto& [nr, a] : room.actors) {
        if (toAll || nr != peer.actor) SendEvent(room, nr, EV_PROPERTIES_CHANGED, ev, true, 0);
    }
}

void Emulator::ChangeGroups(Peer& peer, const Params& params) {
    if (!peer.room) return;
    auto it = peer.room->actors.find(peer.actor);
    if (it == peer.room->actors.end()) return;
    std::vector<uint8_t> groups;
    if (ByteArrayParam(params, P_REMOVE, groups)) {
        if (groups.empty()) it->second.groups.clear();
        for (uint8_t g : groups) it->second.groups.erase(g);
    }
    if (ByteArrayParam(params, P_ADD, groups)) {
        for (uint8_t g : groups) it->second.groups.insert(g);
    }
}

void Emulator::LeaveRoom(uint64_t key, bool sendResponse) {
    auto pit = m_peers.find(key);
    if (pit == m_peers.end()) return;
    Peer& peer = pit->second;
    if (sendResponse) Respond(peer, OP_LEAVE, 0);
    if (!peer.room) return;
    Room& room = *peer.room;
    int32_t actor = peer.actor;
    peer.room = nullptr;
    peer.actor = 0;
    room.actors.erase(actor);
    if (room.fusionServer == actor) room.fusionServer = 0;
    room.fusionConfigWaiters.erase(actor);
    room.fusionRelayInfo.erase(actor);
    room.fusionUniqueIds.erase(actor);
    if (room.fusionShared) {
        room.fusionShared->Leave(actor);
        if (room.fusionSharedConfigActor == actor && !room.fusionSharedConfigSent) room.fusionSharedConfigActor = 0;
    }
    if (room.cleanupCacheOnLeave) {
        room.cache.erase(std::remove_if(room.cache.begin(), room.cache.end(),
                                        [&](const CachedEvent& e) { return !e.global && e.sender == actor; }),
                         room.cache.end());
    }
    m_env.log("[Photon] Actor %d left '%s' (%zu remaining)", actor, room.name.c_str(), room.actors.size());
    if (room.actors.empty()) {
        std::string name = room.name;
        m_rooms.erase(room.appId + '\n' + room.name);
        m_env.roomHosted(name, false);
        return;
    }
    if (room.master == actor) SetMaster(room, room.actors.begin()->first);
    if (room.flags & RF_SUPPRESS_ROOM_EVENTS) return;
    Params ev = { { P_ACTOR_NR, Int(actor) }, { P_MASTER_CLIENT, Int(room.master) }, { P_IS_INACTIVE, Bool(false) } };
    for (const auto& [nr, a] : room.actors) SendEvent(room, nr, EV_LEAVE, ev, true, 0);
}

void Emulator::SetMaster(Room& room, int32_t actor) {
    room.master = actor;
    if (room.fusionShared) room.fusionShared->Master(actor);
    room.props.Set(Byte(GP_MASTER_CLIENT), Int(actor));
}

void Emulator::Respond(Peer& peer, uint8_t op, int16_t code, const Params& params, const std::string& message) {
    peer.send(Message(MT_RESPONSE, OperationResponse(op, code, message, params)), true, 0);
}

void Emulator::SendEvent(Room& room, int32_t actor, uint8_t code, const Params& params, bool reliable, uint8_t channel) {
    auto it = room.actors.find(actor);
    if (it == room.actors.end()) return;
    auto pit = m_peers.find(it->second.peerKey);
    if (pit != m_peers.end()) pit->second.send(Message(MT_EVENT, EventData(code, params)), reliable, channel);
}

Raw Emulator::ActorTable(const Room& room) const {
    Table t;
    for (const auto& [nr, actor] : room.actors) t.Set(Int(nr), actor.props.Encode());
    return t.Encode();
}

std::vector<int32_t> Emulator::ActorNumbers(const Room& room) const {
    std::vector<int32_t> numbers;
    for (const auto& [nr, actor] : room.actors) numbers.push_back(nr);
    return numbers;
}

} // namespace PnServer
