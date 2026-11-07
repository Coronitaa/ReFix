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

    // 9101: Matchmaking Start
    services->gc->RegisterClientHandler(kMsgGCCStrike15_v2_MatchmakingStart, [](const uint8_t* payload, size_t size, bool is_protobuf) {
        // 1. Send Searching Update (1)
        std::vector<uint8_t> updateSearching;
        PutVarintField(updateSearching, 1, 1);
        s_services->gc->SendClient(kMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate, updateSearching.data(), updateSearching.size(), true);

        // 2. Resolve map and launch listen server via SourceEngine (Rule R5!)
        std::string mapName = "de_dust2";
        uint16_t port = 27015;
        char cmdBuf[256];
        snprintf(cmdBuf, sizeof(cmdBuf), "sv_lan 1; hostport %u; map %s", port, mapName.c_str());
        s_services->engine->EnqueueCommand(cmdBuf);

        // 3. Build Matchmaking Reservation (9107) and broadcast over LAN
        uint64_t localSteamId = s_services->gc->GetActiveSteamID();
        uint32_t accountId = (uint32_t)(localSteamId & 0xFFFFFFFF);
        uint64_t serverId = 0x0110000100000000ULL | (uint64_t)(accountId ^ 0x12345678ULL);
        uint32_t localIp = 0x7F000001; // 127.0.0.1 fallback
        uint64_t reservationId = ((uint64_t)time(nullptr) << 16) | 1;

        std::vector<uint8_t> reserveBytes = BuildMatchmakingReserve(serverId, localIp, port, reservationId, mapName, accountId);
        s_services->gc->BroadcastMatchmakingReserve(reserveBytes.data(), reserveBytes.size());

        // 4. Send Match Complete Update (0)
        std::vector<uint8_t> updateDone;
        PutVarintField(updateDone, 1, 0);
        s_services->gc->SendClient(kMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate, updateDone.data(), updateDone.size(), true);

        if (s_services->Log) {
            s_services->Log("[CSGO Mod] MatchmakingStart: Created listen server on port %u, map %s", port, mapName.c_str());
        }
    });

    // 9103: Matchmaking Stop
    services->gc->RegisterClientHandler(kMsgGCCStrike15_v2_MatchmakingStop, [](const uint8_t* payload, size_t size, bool is_protobuf) {
        std::vector<uint8_t> updateDone;
        PutVarintField(updateDone, 1, 0);
        s_services->gc->SendClient(kMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate, updateDone.data(), updateDone.size(), true);
    });

    // Server hello and cache checks (27/28) also stay with the built-in GC emulator: it sends the listen server
    // the player's equipped items with the same serializer the client gets.

    if (services->Log) {
        services->Log("[CSGO Mod] Registered 2 client GC handlers");
    }
}

} // namespace refix::csgo
