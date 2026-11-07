#pragma once
#include <cstdint>
#include <functional>
#include "lan_core/refix_lan_types.h"
#include <vector>

namespace refix::gc {

// Shares the rich presence channel: packets on a channel of their own (248) never reached the other process.
// Relay packets are told apart by their 'RGCS' magic in the LanCore event loop (unreal_steam_emu).
constexpr uint8_t kLanChannelGC = 247;
constexpr uint32_t kGCRelayMagicValue = 0x53434752; // "RGCS" little-endian

void InitializeGCRelay();
void HandleRelayPacket(const refix::lan::PeerId& sender, const uint8_t* data, size_t len);
void ShutdownGCRelay();
void OnGameServerClientAuth(uint64_t clientSteamId, uint32_t ticket);
void BroadcastEquippedUpdate(uint64_t itemId, uint32_t newClass, uint32_t newSlot, const std::vector<uint8_t>& soUpdateBytes);
void BroadcastMatchmakingReserve(const std::vector<uint8_t>& reserveBytes);

// Joins the match hosted by targetSteamId (or by any LAN player when 0) with "connect ip:port"; runs onNotFound
// when nobody hosts one.
void FindLanMatch(uint64_t targetSteamId, const std::function<void()>& onNotFound);

} // namespace refix::gc
