// LAN rooms: players gather in a room before the match, and when the room owner starts a listen server every
// member connects to it. Built on rich presence, which reaches every ReFix peer:
//   "refix:room"   = owner SteamID (decimal) -> the player is in that room (the owner points at itself)
//   "refix:server" = "ip:port"               -> the player hosts a listen server (set by unreal_steam_emu)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace refix::rooms {

struct Member {
    uint64_t steamId = 0;
    std::string name;
    bool isOwner = false;
    bool isLocal = false;
};

struct Room {
    uint64_t ownerSteamId = 0;
    std::string ownerName;
    size_t memberCount = 0;
    bool inMatch = false;      // the owner hosts a listen server right now
};

void Create();
void Join(uint64_t ownerSteamId);
void Leave();
uint64_t Current();            // owner of the room the local player is in, 0 when none
bool IsOwner();
std::vector<Room> List();      // rooms on the LAN, the local one included
std::vector<Member> Members(); // members of the local player's room
bool OwnsRoom(uint64_t steamId);

// Game thread (SteamAPI_RunCallbacks): follows the owner into its match and drops a room whose owner left.
void Tick();

} // namespace refix::rooms
