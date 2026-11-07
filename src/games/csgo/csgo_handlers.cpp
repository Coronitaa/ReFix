#include "csgo_handlers.h"
#include "csgo_proto.h"
#include "../../csgo_schema.h"
#include "../../csgo_item_defs.h"

#include <windows.h>
#include <algorithm>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>

namespace refix::csgo {

static refix::IRefixServices* s_services = nullptr;
static std::vector<uint8_t> BuildMatchmakingReserve(uint64_t serverId, uint32_t ip, uint16_t port, uint64_t reservationId, const std::string& mapName, uint32_t targetAccountId) {
    std::vector<uint8_t> srvReserve;
    PutVarintField(srvReserve, 1, targetAccountId);
    uint32_t localAccountId = (uint32_t)(s_services->gc->GetActiveSteamID() & 0xFFFFFFFF);
    if (localAccountId != targetAccountId) {
        PutVarintField(srvReserve, 1, localAccountId);
    }
    PutVarintField(srvReserve, 2, 0); // game_type
    PutVarintField(srvReserve, 3, reservationId);
    PutVarintField(srvReserve, 4, 1014); // server_version

    std::vector<uint8_t> clientReserve;
    PutVarintField(clientReserve, 1, serverId);
    PutVarintField(clientReserve, 2, ip);
    PutVarintField(clientReserve, 3, port);
    PutVarintField(clientReserve, 4, reservationId);
    PutBytesField(clientReserve, 5, srvReserve);
    PutStringField(clientReserve, 6, mapName.empty() ? "de_dust2" : mapName);
    return clientReserve;
}

void RegisterCSGOHandlers(refix::IRefixServices* services) {
    s_services = services;
    if (!services || !services->gc) return;

    // --- CLIENT HANDLERS ---

    // Hello, equip (1059), crates (1007), inspect (9156) and the store stay with the built-in GC emulator, which owns
    // the persisted inventory and replies with the full SO cache; registering them here would shadow it.

    // Matchmaking start/stop also stay there: it joins a match hosted on the LAN or hosts one.

    // Server hello and cache checks (27/28) also stay with the built-in GC emulator: it sends the listen server
    // the player's equipped items with the same serializer the client gets.

    if (services->Log) {
        services->Log("[CSGO Mod] GC messages are handled by the built-in GC emulator");
    }
}

} // namespace refix::csgo
