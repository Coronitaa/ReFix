// LAN presence for the overlay's player list.
// Every ReFix LAN instance broadcasts who it is (profile name, avatar, game, joinable lobby) on UDP 47690
// every 2 s, whatever game it runs, so the list shows everyone on the LAN in real time. Avatars and
// invites travel by unicast to each instance's own port (several instances can share one PC).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace refix::overlay::net {

constexpr uint16_t kPresencePort = 47690;

enum class AvatarKind : uint8_t { Stock = 0, Custom = 1 };

struct Presence {
    uint64_t steamId = 0;
    uint32_t appId = 0;
    std::string name;            // UTF-8
    std::string game;            // UTF-8 window title of the game
    AvatarKind avatarKind = AvatarKind::Stock;
    uint32_t avatarKey = 0;      // stock index, or CRC32 of the custom JPEG
    uint64_t lobbyId = 0;        // Steam lobby the player is in, 0 when none
    bool joinable = false;       // lobby accepts more players
    uint8_t members = 0;
    uint8_t maxMembers = 0;
    std::string connect;         // rich presence "connect" string, when the game sets one
    uint64_t steamOnlineId = 0;  // 0 when Steam online services are off, or LocalSteamId() when on
};

struct RemotePlayer : Presence {
    uint32_t ip = 0;             // host byte order
    uint16_t port = 0;           // the instance's unicast port
    uint64_t lastSeen = 0;       // GetTickCount64
};

struct Invite {
    uint32_t id = 0;
    uint64_t from = 0;
    uint32_t appId = 0;
    uint64_t lobbyId = 0;
    std::string connect;
};

bool Start();
void SetLocal(const Presence& p);
void SetLocalAvatar(uint32_t key, std::vector<uint8_t> jpeg);
std::vector<RemotePlayer> Players();                      // live players, own instance excluded
bool SendInvite(uint64_t to, uint64_t lobbyId, const std::string& connect);
std::vector<Invite> TakeInvites();
// Starts (or continues) downloading a player's custom avatar; true with the JPEG once complete.
bool FetchAvatar(uint64_t steamId, uint32_t key, std::vector<uint8_t>& jpeg);

uint32_t Crc32(const uint8_t* data, size_t len);

} // namespace refix::overlay::net
