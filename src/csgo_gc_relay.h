#pragma once
#include <cstdint>
#include <vector>

namespace refix::csgo {

constexpr uint8_t kLanChannelGC = 248;

void InitializeGCRelay();
void ShutdownGCRelay();
void OnGameServerClientAuth(uint64_t clientSteamId, uint32_t ticket);
void BroadcastEquippedUpdate(uint64_t itemId, uint32_t newClass, uint32_t newSlot, const std::vector<uint8_t>& soUpdateBytes);

void BroadcastMatchmakingReserve(const std::vector<uint8_t>& reserveBytes);

} // namespace refix::csgo
