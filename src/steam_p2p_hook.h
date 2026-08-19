// =============================================================================
// ReFix - Steam P2P Winsock Redirect Layer
// =============================================================================
// Hooks ws2_32.dll Winsock functions (sendto / recvfrom / connect / select)
// and transparently routes UDP game traffic through Steam ISteamNetworking
// P2P relay (SendP2PPacket / ReadP2PPacket) using SteamIDs resolved from the
// lobby member list.
//
// This allows players to connect over the Internet without port forwarding
// or external VPN software — traffic travels through Valve's SDR relay.
//
// Architecture:
//   1. On SteamAPI_Init success, call SteamP2PHook::Install().
//   2. When a peer joins (lobby member list updated), call
//      SteamP2PHook::RegisterPeer(steamID, remoteIP) to create the IP<->SteamID
//      mapping.
//   3. Winsock sendto() to a known peer IP is silently redirected to
//      ISteamNetworking::SendP2PPacket(steamID, ...).
//   4. A background pump thread calls ReadP2PPacket and places incoming
//      packets in a lock-free ring buffer; recvfrom() drains that buffer.
//   5. On DLL_PROCESS_DETACH, call SteamP2PHook::Uninstall().
// =============================================================================
#pragma once
#include <cstdint>

namespace SteamP2PHook {

    // Call once after SteamAPI_Init() succeeds.
    // hSteamOriginal = handle to steam_api64_valve.dll (for function lookup).
    void Install(void* hSteamOriginal);

    // Call on DLL_PROCESS_DETACH.
    void Uninstall();

    // Register a mapping: when the game tries to send UDP to ipv4 (host byte order):port,
    // the packets will be sent via Steam P2P to steamID instead.
    // Call whenever a new lobby member is detected (LobbyDataUpdate, LobbyChatUpdate).
    void RegisterPeer(uint64_t steamID, uint32_t ipv4_host);

    // Remove a peer mapping (e.g. when they leave the lobby).
    void UnregisterPeer(uint64_t steamID);

    // Log wrapper — forwards to ReFix.log via steam_proxy's logger.
    void Log(const char* fmt, ...);

} // namespace SteamP2PHook
