// ISteamGameCoordinator for ReFix LAN: answers the GC SDK session handshake shared by Valve's GC games
// (ClientHello -> ClientWelcome + ConnectionStatus, ServerHello -> ServerWelcome + ServerConnectionStatus) so the
// game sees a live coordinator session. Game-specific GC messages (matchmaking, ranks, items) get no answer.
#pragma once
#include <cstdint>
#include <vector>
#include "include/refix_services.h"

class ISteamGameCoordinator;

namespace ReFixGC {

// One coordinator per pipe: the client and an in-process game server (listen server) poll their own queues.
ISteamGameCoordinator* Get(bool gameServer);
void SendToGameServer(uint32_t msg, const std::vector<uint8_t>& body);
void SendToClient(uint32_t msg, const std::vector<uint8_t>& body);
bool IsGameServerActive();
void SetHostConfigJson(const std::string& json);
std::string GetHostConfigJson();
// Set by the play menu's "Host match" button: the next MatchmakingStart hosts instead of searching first.
void RequestHostOnNextStart();
// Starts a listen server now with the stored host settings (map, mode, rounds, bots, password...).
void HostLanMatch();
bool ConsumeHostRequest();

// Protobuf bodies of the replies, exposed for tests.
std::vector<uint8_t> BuildClientWelcome(uint32_t version, uint64_t steamId);
std::vector<uint8_t> BuildServerWelcome(uint32_t version, uint64_t steamId);
std::vector<uint8_t> BuildCacheSubscribed(uint64_t steamId);
std::vector<uint8_t> BuildEquippedCacheSubscribed(uint64_t steamId);
std::vector<uint8_t> BuildConnectionStatus();
// Local player's persisted inventory (the one the GC handlers serve), for the IInventoryStore service.
std::vector<refix::InventoryItem> GetLocalInventory();
// Field 1 (version) of a CMsgClientHello body, 0 when absent.
uint32_t ParseHelloVersion(const uint8_t* body, size_t size);

bool IsPrimeEnabled();
void SetPrimeEnabled(bool enabled);
bool HasOperationPass(uint32_t season);
void ActivateOperationPass(uint32_t season, uint64_t passItemId = 0);
// Adds a PlayerQuestData (match-end quest points from the game server) to the local player's operation progress.
void ApplyQuestProgress(const uint8_t* data, size_t size);

} // namespace ReFixGC

extern "C" {
// Sticker / graffiti / patch (stickerKit) or music kit (musicKit) on its carrier item (1209, 1348, 4609, 1314).
__declspec(dllexport) void CSGO_GrantKitItemToLocal(uint32_t defIndex, uint32_t count, uint32_t quality, uint32_t rarity, uint32_t stickerKit, uint32_t musicKit);
__declspec(dllexport) void CSGO_GrantItemToLocal(uint32_t defIndex, uint32_t count, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed, const char* customName = nullptr);
__declspec(dllexport) bool CSGO_IsPrimeStatus();
__declspec(dllexport) void CSGO_SetPrimeStatus(bool enabled);
__declspec(dllexport) bool CSGO_HasOperationPass(uint32_t season);
__declspec(dllexport) void CSGO_ActivateOperationPass(uint32_t season, uint64_t passItemId);
__declspec(dllexport) void CSGO_SetHostConfig(const char* json);
__declspec(dllexport) void CSGO_OpenMarketHUD();
__declspec(dllexport) void CSGO_CloseMarketHUD();
__declspec(dllexport) void CSGO_OpenOverlay();
__declspec(dllexport) void CSGO_CloseOverlay();
__declspec(dllexport) void CSGO_ToggleOverlay();
}
