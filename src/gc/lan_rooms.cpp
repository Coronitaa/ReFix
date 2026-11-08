#include "lan_rooms.h"

#include <windows.h>
#include <algorithm>
#include <string>
#include "unreal_steam_emu.h"
#include "source_main_thread_queue.h"
#include "gc_relay.h"

extern void ReFixLog(const char* fmt, ...);

namespace refix::rooms {

namespace {

constexpr const char* kRoomKey = "refix:room";
constexpr const char* kServerKey = "refix:server";

uint64_t ParseSteamId(const std::string& s) {
    return s.empty() ? 0 : strtoull(s.c_str(), nullptr, 10);
}

std::string PeerValue(uint64_t steamId, const char* key) {
    for (const auto& p : UnrealSteamEmu::PeersWithPresence(key))
        if (p.first == steamId) return p.second;
    return {};
}

std::string NameOf(uint64_t steamId) {
    const char* n = steamId == UnrealSteamEmu::GetLocalSteamID() ? UnrealSteamEmu::GetPersonaName()
                                                                 : UnrealSteamEmu::GetFriendPersonaName(steamId);
    return n && *n ? n : "Player";
}

std::string g_followedServer; // owner server we already connected to (game thread only)
ULONGLONG g_lastTick = 0;
int g_ownerMissingTicks = 0;  // a presence clear + re-send can briefly hide the owner's key

} // namespace

uint64_t Current() {
    return ParseSteamId(UnrealSteamEmu::GetLocalPresence(kRoomKey));
}

bool IsOwner() {
    uint64_t room = Current();
    return room != 0 && room == UnrealSteamEmu::GetLocalSteamID();
}

bool OwnsRoom(uint64_t steamId) {
    if (steamId == UnrealSteamEmu::GetLocalSteamID()) return IsOwner();
    return ParseSteamId(PeerValue(steamId, kRoomKey)) == steamId;
}

void Create() {
    uint64_t self = UnrealSteamEmu::GetLocalSteamID();
    UnrealSteamEmu::SetLocalPresence(kRoomKey, std::to_string(self).c_str());
    ReFixLog("[Rooms] Created room %llu", (unsigned long long)self);
}

void Join(uint64_t ownerSteamId) {
    if (ownerSteamId == 0) return;
    UnrealSteamEmu::SetLocalPresence(kRoomKey, std::to_string(ownerSteamId).c_str());
    ReFixLog("[Rooms] Joined room of %llu", (unsigned long long)ownerSteamId);
}

void Leave() {
    UnrealSteamEmu::SetLocalPresence(kRoomKey, "");
    ReFixLog("[Rooms] Left room");
}

std::vector<Room> List() {
    auto rooms = UnrealSteamEmu::PeersWithPresence(kRoomKey);
    auto servers = UnrealSteamEmu::PeersWithPresence(kServerKey);
    uint64_t self = UnrealSteamEmu::GetLocalSteamID();
    uint64_t local = Current();

    std::vector<Room> out;
    auto addRoom = [&](uint64_t owner) {
        Room r;
        r.ownerSteamId = owner;
        r.ownerName = NameOf(owner);
        for (const auto& p : rooms) if (ParseSteamId(p.second) == owner) ++r.memberCount;
        if (local == owner) ++r.memberCount;
        r.inMatch = owner == self ? !UnrealSteamEmu::GetLocalPresence(kServerKey).empty()
                                  : std::any_of(servers.begin(), servers.end(), [&](const auto& s) { return s.first == owner; });
        out.push_back(r);
    };
    if (IsOwner()) addRoom(self);
    for (const auto& p : rooms)
        if (ParseSteamId(p.second) == p.first) addRoom(p.first);
    return out;
}

std::vector<Member> Members() {
    std::vector<Member> out;
    uint64_t room = Current();
    if (room == 0) return out;
    uint64_t self = UnrealSteamEmu::GetLocalSteamID();
    out.push_back({ self, NameOf(self), room == self, true });
    for (const auto& p : UnrealSteamEmu::PeersWithPresence(kRoomKey))
        if (ParseSteamId(p.second) == room) out.push_back({ p.first, NameOf(p.first), p.first == room, false });
    std::stable_sort(out.begin(), out.end(), [](const Member& a, const Member& b) { return a.isOwner > b.isOwner; });
    return out;
}

void Tick() {
    ULONGLONG now = GetTickCount64();
    if (now - g_lastTick < 500) return;
    g_lastTick = now;

    uint64_t room = Current();
    if (room == 0 || IsOwner()) { g_followedServer.clear(); g_ownerMissingTicks = 0; return; }

    // The owner closed the room (or left the LAN): drop it so the member is not stuck in a ghost room.
    if (ParseSteamId(PeerValue(room, kRoomKey)) != room) {
        if (++g_ownerMissingTicks < 4) return;  // ~2 s, rides out a presence clear + re-send
        g_ownerMissingTicks = 0;
        if (Current() != room) return;          // the player joined another room meanwhile
        ReFixLog("[Rooms] Owner %llu closed the room", (unsigned long long)room);
        Leave();
        g_followedServer.clear();
        return;
    }
    g_ownerMissingTicks = 0;

    std::string server = PeerValue(room, kServerKey);
    if (server.empty() || !refix::gc::IsValidLanServerAddress(server)) { g_followedServer.clear(); return; }
    if (server == g_followedServer) return;
    g_followedServer = server;
    ReFixLog("[Rooms] Room owner started a match on %s, following", server.c_str());
    refix::source::MainThreadQueue::ExecuteClientCmdImmediate(("connect " + server).c_str());
}

} // namespace refix::rooms
