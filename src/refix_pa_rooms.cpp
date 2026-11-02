#include "refix_pa_rooms.h"
#include "refix_pa_gql.h"
#include "refix_steam_flat.h"
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <vector>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixPARooms {

namespace {

constexpr const char* kPresenceKey = "refix_pa_room";
constexpr const char* kPresenceVersion = "v1";
constexpr const char* kRelayHost = "127.0.0.1"; // every peer runs its own ReFix relay on loopback
constexpr size_t kMaxPresenceBytes = 255;
constexpr size_t kPresenceFields = 7;
constexpr int kDefaultPing = 40;

struct Room {
    std::string roomId; // host SteamID64
    std::string name;
    std::string mode;
    std::string mapList;
    std::string curMapId = "0";
    int gamers = 1;
    bool isGaming = false;
    int64_t createTime = 0;
};

std::mutex g_mutex;
bool g_hosting = false;
Room g_ownRoom;

std::string OwnerAccountId(const std::string& steamId) {
    return "fake_" + steamId; // matches the offline AccountManager fake id
}

std::string Sanitize(const std::string& s) {
    std::string out = s;
    for (char& c : out) if (c == '|') c = '/';
    return out;
}

// --- Rich presence encoding: v1|gamers|isGaming|curMapId|mode|mapList|name ------------------------------

std::string EncodePresence(const Room& room) {
    std::string value = std::string(kPresenceVersion) + "|" + std::to_string(room.gamers) + "|" + (room.isGaming ? "1" : "0") + "|" +
                        Sanitize(room.curMapId) + "|" + Sanitize(room.mode) + "|" + Sanitize(room.mapList) + "|";
    std::string name = room.name; // name is last: it may be truncated to fit Steam's 256-byte limit
    if (value.size() + name.size() > kMaxPresenceBytes) name = name.substr(0, kMaxPresenceBytes > value.size() ? kMaxPresenceBytes - value.size() : 0);
    return value + name;
}

bool DecodePresence(uint64_t steamId, const std::string& value, Room& room) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (parts.size() + 1 < kPresenceFields) {
        size_t bar = value.find('|', start);
        if (bar == std::string::npos) return false;
        parts.push_back(value.substr(start, bar - start));
        start = bar + 1;
    }
    parts.push_back(value.substr(start));
    if (parts[0] != kPresenceVersion) return false;
    room.roomId = std::to_string(steamId);
    room.gamers = atoi(parts[1].c_str());
    room.isGaming = parts[2] == "1";
    room.curMapId = parts[3].empty() ? "0" : parts[3];
    room.mode = parts[4];
    room.mapList = parts[5];
    room.name = parts[6];
    return true;
}

void PublishOwnRoom() {
    // Caller holds g_mutex.
    bool ok = ReFixSteamFlat::SetRichPresence(kPresenceKey, g_hosting ? EncodePresence(g_ownRoom) : std::string());
    ReFixLog("[PA-Rooms] Rich presence %s %s", g_hosting ? "published" : "cleared", ok ? "" : "(Steam unavailable)");
}

// --- JSON -------------------------------------------------------------------------------------------------

std::string RoomJson(const Room& room) {
    using ReFixPAGql::JsonString;
    const char* digits = room.curMapId.find_first_not_of("-0123456789") == std::string::npos ? room.curMapId.c_str() : "0";
    return "{\"roomId\":" + JsonString(room.roomId) + ",\"roomOwnerId\":" + JsonString(OwnerAccountId(room.roomId)) +
           ",\"roomName\":" + JsonString(room.name) + ",\"p2pServerIp\":" + JsonString(kRelayHost) +
           ",\"curGamerNum\":" + std::to_string(room.gamers) + ",\"closedSeatsMask\":0,\"pwdHash\":\"\"" +
           ",\"createTime\":" + std::to_string(room.createTime) + ",\"isGaming\":" + (room.isGaming ? "true" : "false") +
           ",\"mapIndexListStr\":" + JsonString(room.mapList) + ",\"curMapId\":" + digits +
           ",\"modeName\":" + JsonString(room.mode) + ",\"roomRegion\":\"\",\"ping\":" + std::to_string(kDefaultPing) + "}";
}

std::string OkJson(bool ok = true) {
    return ok ? "{\"ok\":true}" : "{\"ok\":false}";
}

// --- Directory --------------------------------------------------------------------------------------------

Room LookupRoom(const std::string& roomId) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_hosting && roomId == g_ownRoom.roomId) return g_ownRoom;
    }
    Room room;
    uint64_t steamId = strtoull(roomId.c_str(), nullptr, 10);
    if (!DecodePresence(steamId, ReFixSteamFlat::FriendRichPresence(steamId, kPresenceKey), room)) {
        room = Room();
        room.roomId = roomId; // unknown (not a friend / not published): still routable through the relay
    }
    return room;
}

std::string ListRooms() {
    std::string out = "[";
    size_t count = 0;
    for (const auto& entry : ReFixSteamFlat::FriendsWithRichPresence(kPresenceKey)) {
        Room room;
        if (!DecodePresence(entry.steamID, entry.value, room)) continue;
        out += (count++ ? "," : "") + RoomJson(room);
    }
    ReFixLog("[PA-Rooms] listRoom -> %zu friend room(s)", count);
    return out + "]";
}

std::string CreateRoom(const std::string& query) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_ownRoom = Room();
    g_ownRoom.roomId = std::to_string(ReFixSteamFlat::LocalSteamID());
    g_ownRoom.name = ReFixPAGql::StringArg(query, "roomName");
    g_ownRoom.mode = ReFixPAGql::StringArg(query, "gameMode");
    if (g_ownRoom.mode.empty()) g_ownRoom.mode = ReFixPAGql::ScalarArg(query, "gameMode"); // enum token
    g_ownRoom.mapList = ReFixPAGql::StringArg(query, "mapIndexListStr");
    g_ownRoom.createTime = (int64_t)time(nullptr);
    g_hosting = true;
    ReFixLog("[PA-Rooms] createRoom id=%s name='%s' mode='%s' maps='%s'", g_ownRoom.roomId.c_str(), g_ownRoom.name.c_str(),
             g_ownRoom.mode.c_str(), g_ownRoom.mapList.c_str());
    PublishOwnRoom();
    return "{\"roomInfo\":" + RoomJson(g_ownRoom) + "}";
}

// updateRoom(roomInfoStr: "<P2PRoomInfo JSON>", isFirst: bool)
std::string UpdateRoom(const std::string& query) {
    std::string info = ReFixPAGql::StringArg(query, "roomInfoStr");
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_hosting || info.empty()) return OkJson();
    std::string gamers = ReFixPAGql::JsonScalarMember(info, "curGamerNum");
    std::string gaming = ReFixPAGql::JsonScalarMember(info, "isGaming");
    std::string mapId = ReFixPAGql::JsonScalarMember(info, "curMapId");
    std::string name = ReFixPAGql::JsonStringMember(info, "roomName");
    std::string mode = ReFixPAGql::JsonStringMember(info, "modeName");
    std::string maps = ReFixPAGql::JsonStringMember(info, "mapIndexListStr");
    if (!gamers.empty()) g_ownRoom.gamers = atoi(gamers.c_str());
    if (!gaming.empty()) g_ownRoom.isGaming = gaming == "true";
    if (!mapId.empty()) g_ownRoom.curMapId = mapId;
    if (!name.empty()) g_ownRoom.name = name;
    if (!mode.empty()) g_ownRoom.mode = mode;
    if (!maps.empty()) g_ownRoom.mapList = maps;
    PublishOwnRoom();
    return OkJson();
}

std::string CloseOwnRoom() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_hosting) {
        g_hosting = false;
        PublishOwnRoom();
    }
    return OkJson();
}

} // namespace

bool Handle(const std::string& operation, const std::string& query, std::string& dataJson) {
    if (operation == "createRoom") dataJson = CreateRoom(query);
    else if (operation == "describeRoom") dataJson = RoomJson(LookupRoom(ReFixPAGql::StringArg(query, "roomId")));
    else if (operation == "listRoom") dataJson = ListRooms();
    else if (operation == "updateRoom") dataJson = UpdateRoom(query);
    else if (operation == "deleteRoom") dataJson = CloseOwnRoom();
    // Room access is controlled by who can see the room (Steam friends), so join tokens are opaque.
    else if (operation == "createP2pJwt") dataJson = "{\"jwt\":" + ReFixPAGql::JsonString("refix." + ReFixPAGql::StringArg(query, "roomId")) + "}";
    else if (operation == "checkP2pJwt") dataJson = OkJson();
    else if (operation == "checkReconnectRoom") dataJson = "{\"canReConnect\":false,\"roomId\":\"\",\"roomRegion\":\"\",\"roomCreateTime\":0}";
    else if (operation == "pingPongRoom" || operation == "updateSelfRoomInfo" || operation == "clearSelfRoomInfo" || operation == "reportData") dataJson = OkJson();
    else return false;
    return true;
}

} // namespace ReFixPARooms
