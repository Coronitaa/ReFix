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
static std::mutex s_txMu;
struct PendingTx {
    uint32_t defIndex;
    uint32_t quantity;
};
static std::unordered_map<uint64_t, std::vector<PendingTx>> s_pendingPurchases;
static uint64_t s_soUpdateVer = 100;

// Serialize single CSOEconItem (type 1) protobuf payload
static std::vector<uint8_t> SerializeEconItem(const InventoryItem& itm, uint32_t accountId, uint32_t origin = 0) {
    std::vector<uint8_t> item;
    PutVarintField(item, 1, itm.id);
    PutVarintField(item, 2, accountId);
    PutVarintField(item, 3, itm.defIndex);
    PutVarintField(item, 4, itm.level);
    PutVarintField(item, 5, itm.quality);
    PutVarintField(item, 6, 0); // inventory position
    PutVarintField(item, 7, itm.rarity);
    PutVarintField(item, 8, 4); // flags
    PutVarintField(item, 9, origin);

    for (const auto& eq : itm.equipped) {
        std::vector<uint8_t> eqProto;
        PutVarintField(eqProto, 1, eq.team);
        PutVarintField(eqProto, 2, eq.slot);
        PutBytesField(item, 10, eqProto);
    }

    if (itm.paintKit != 0 || itm.wear > 0.0f || itm.seed != 0) {
        if (itm.paintKit != 0) {
            std::vector<uint8_t> attr;
            PutVarintField(attr, 1, 6); // def_index 6 = set item texture prefab
            float pVal = (float)itm.paintKit;
            PutFixed32Field(attr, 3, *(uint32_t*)&pVal);
            PutBytesField(item, 12, attr);
        }
        if (itm.wear > 0.0f) {
            std::vector<uint8_t> attr;
            PutVarintField(attr, 1, 8); // def_index 8 = set item texture wear
            float wVal = itm.wear;
            PutFixed32Field(attr, 3, *(uint32_t*)&wVal);
            PutBytesField(item, 12, attr);
        }
        if (itm.seed != 0) {
            std::vector<uint8_t> attr;
            PutVarintField(attr, 1, 7); // def_index 7 = set item texture seed
            float sVal = (float)itm.seed;
            PutFixed32Field(attr, 3, *(uint32_t*)&sVal);
            PutBytesField(item, 12, attr);
        }
    }
    return item;
}

// Build CMsgClientWelcome (4004) for CS:GO
static std::vector<uint8_t> BuildClientWelcome(uint32_t version, uint64_t steamId) {
    uint32_t accountId = (uint32_t)(steamId & 0xFFFFFFFF);
    std::vector<uint8_t> owner;
    PutVarintField(owner, 1, kSOTypeSteamId);
    PutVarintField(owner, 2, steamId);

    auto items = s_services->inventory->GetItems(steamId);

    // SubscribedType 1: CSOEconItem
    std::vector<uint8_t> itemType;
    PutVarintField(itemType, 1, kSOTypeItem);
    for (const auto& itm : items) {
        std::vector<uint8_t> itemBytes = SerializeEconItem(itm, accountId);
        PutBytesField(itemType, 2, itemBytes);
    }

    // SubscribedType 7: CSOEconGameAccountClient (Prime Status)
    std::vector<uint8_t> accountType;
    PutVarintField(accountType, 1, kSOTypeGameAccountClient);
    std::vector<uint8_t> accountObj;
    PutVarintField(accountObj, 1, 0); // additional_backpack_slots
    PutFixed32Field(accountObj, 2, 0); // bonus_rep_value
    bool isPrime = s_services->entitlements->HasEntitlement(steamId, 54029) ||
                   s_services->entitlements->HasEntitlement(steamId, 10001);
    if (isPrime) {
        PutVarintField(accountObj, 10, 1); // elevated_state = 1
        PutVarintField(accountObj, 11, 1); // elevated_status = 1
    }
    PutBytesField(accountType, 2, accountObj);

    // SubscribedType 40: CSOEconItemSeasonalOperation (Operation Passes)
    std::vector<uint8_t> seasonType;
    PutVarintField(seasonType, 1, kSOTypeSeasonalOperation);
    if (s_services->entitlements->HasEntitlement(steamId, 4549)) {
        std::vector<uint8_t> seasonObj;
        PutVarintField(seasonObj, 1, 8); // season_id = 8 (Shattered Web)
        PutVarintField(seasonObj, 2, 1); // tier = 1
        PutBytesField(seasonType, 2, seasonObj);
    }

    // Compose SOCacheSubscribed
    std::vector<uint8_t> cache;
    PutBytesField(cache, 2, itemType);
    PutBytesField(cache, 2, accountType);
    if (s_services->entitlements->HasEntitlement(steamId, 4549)) {
        PutBytesField(cache, 2, seasonType);
    }
    PutFixed64Field(cache, 3, 1);       // version = 3
    PutBytesField(cache, 4, owner);     // owner_soid = 4

    // CMsgCStrike15Welcome with official session cookie
    std::vector<uint8_t> cstrikeWelcome;
    PutFixed32Field(cstrikeWelcome, 1, 0);
    PutFixed64Field(cstrikeWelcome, 2, 0x293A206F6C6C6548ULL); // "Hello :)" cookie

    std::vector<uint8_t> welcome;
    PutVarintField(welcome, 1, 1); // version = 1
    PutBytesField(welcome, 2, cstrikeWelcome); // game_data = 2
    std::vector<uint8_t> subCaches;
    PutBytesField(subCaches, 1, cache);
    PutBytesField(welcome, 3, subCaches); // socache = 3
    PutFixed32Field(welcome, 4, (uint32_t)time(nullptr)); // rtime32_gc_welcome_timestamp = 4
    return welcome;
}

// Build CMsgServerWelcome (4005) for CS:GO host server
static std::vector<uint8_t> BuildServerWelcome(uint32_t version, uint64_t steamId) {
    std::vector<uint8_t> cstrikeWelcome;
    PutFixed32Field(cstrikeWelcome, 1, 0);
    PutFixed64Field(cstrikeWelcome, 2, 0x293A206F6C6C6548ULL);

    std::vector<uint8_t> welcome;
    PutVarintField(welcome, 1, 1);
    PutBytesField(welcome, 2, cstrikeWelcome);
    PutFixed32Field(welcome, 4, (uint32_t)time(nullptr));
    return welcome;
}

// Build Server Equipped SOCacheSubscribed (24)
static std::vector<uint8_t> BuildEquippedCacheSubscribed(uint64_t steamId) {
    uint32_t accountId = (uint32_t)(steamId & 0xFFFFFFFF);
    std::vector<uint8_t> owner;
    PutVarintField(owner, 1, kSOTypeSteamId);
    PutVarintField(owner, 2, steamId);

    auto items = s_services->inventory->GetItems(steamId);

    std::vector<uint8_t> itemType;
    PutVarintField(itemType, 1, kSOTypeItem);
    size_t count = 0;
    for (const auto& itm : items) {
        if (!itm.equipped.empty()) {
            std::vector<uint8_t> itemBytes = SerializeEconItem(itm, accountId);
            PutBytesField(itemType, 2, itemBytes);
            if (++count >= 64) break;
        }
    }

    std::vector<uint8_t> cache;
    if (count > 0) {
        PutBytesField(cache, 2, itemType);
    }
    PutFixed64Field(cache, 3, 1);
    PutBytesField(cache, 4, owner);
    return cache;
}

static std::vector<uint8_t> BuildConnectionStatus() {
    std::vector<uint8_t> status;
    PutVarintField(status, 1, 0); // GCConnectionStatus_HAVE_SESSION = 0
    return status;
}

static std::vector<uint8_t> BuildMatchmakingClientHello(uint32_t accountId) {
    std::vector<uint8_t> resp;
    PutVarintField(resp, 1, accountId);
    PutVarintField(resp, 16, 40);     // player_level = 40 (General de rango 40)
    PutVarintField(resp, 17, 40);
    PutVarintField(resp, 18, 327680000 + 3500); // player_cur_xp
    PutVarintField(resp, 19, 0);      // player_xp_bonus_flags
    return resp;
}

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

    // --- SERVER HANDLERS ---

    // 4007: ServerHello
    services->gc->RegisterServerHandler(kMsgServerHello, [](const uint8_t* payload, size_t size, bool is_protobuf) {
        uint64_t steamId = s_services->gc->GetActiveSteamID();
        auto welcome = BuildServerWelcome(1, steamId);
        s_services->gc->SendServer(kMsgServerWelcome, welcome.data(), welcome.size(), true);

        auto conn = BuildConnectionStatus();
        s_services->gc->SendServer(kMsgServerConnectionStatus, conn.data(), conn.size(), true);

        auto cache = BuildEquippedCacheSubscribed(steamId);
        s_services->gc->SendServer(24 /* k_ESOMsg_CacheSubscribed */, cache.data(), cache.size(), true);

        if (s_services->Log) {
            s_services->Log("[CSGO Mod] Handled ServerHello: Dispatched ServerWelcome & Equipped CacheSubscribed to host server");
        }
    });

    // 27 / 28: CacheSubscriptionCheck / Refresh
    services->gc->RegisterServerHandler(27, [](const uint8_t* payload, size_t size, bool is_protobuf) {
        uint64_t steamId = s_services->gc->GetActiveSteamID();
        auto cache = BuildEquippedCacheSubscribed(steamId);
        s_services->gc->SendServer(24, cache.data(), cache.size(), true);
    });
    services->gc->RegisterServerHandler(28, [](const uint8_t* payload, size_t size, bool is_protobuf) {
        uint64_t steamId = s_services->gc->GetActiveSteamID();
        auto cache = BuildEquippedCacheSubscribed(steamId);
        s_services->gc->SendServer(24, cache.data(), cache.size(), true);
    });

    if (services->Log) {
        services->Log("[CSGO Mod] Registered 2 client GC handlers and 3 server GC handlers");
    }
}

} // namespace refix::csgo
