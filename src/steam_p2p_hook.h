// =============================================================================
// ReFix - Steam P2P Winsock Redirect Layer
// =============================================================================
// Hooks ws2_32.dll Winsock functions (sendto / recvfrom / connect / select /
// bind / closesocket) and transparently routes UDP game traffic through Steam
// ISteamNetworking P2P relay (SendP2PPacket / ReadP2PPacket) using SteamIDs
// resolved from the lobby member list.
//
// Hardened against Red Team vulnerabilities:
//   - V-01: Multi-socket context table (std::unordered_map<SOCKET, SocketContext>)
//           separating Game (ports 7777 / 27015 / 5055) and Voice (port 5058).
//   - V-02: Pre-Lobby Hold Buffer with 3000 ms TTL for Photon Fusion & nanosockets.
//   - V-04: Port Virtualization on WSAEADDRINUSE (10048) in Hook_bind.
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
    void RegisterPeer(uint64_t steamID, uint32_t ipv4_host, uint16_t port = 0);

    // Remove a peer mapping (e.g. when they leave the lobby).
    void UnregisterPeer(uint64_t steamID);

    // Log wrapper — forwards to ReFix.log via steam_proxy's logger.
    void Log(const char* fmt, ...);

    // Returns virtualized or actual bound game port (for lobby metadata advertising).
    uint16_t GetBoundGamePort();

    // Flush any pending packets held in Pre-Lobby Hold Buffer.
    void FlushHoldBuffer();

} // namespace SteamP2PHook

extern "C" {
    __declspec(dllexport) void ReFix_RegisterP2PPeer(uint64_t steamID, uint32_t ipv4_host);
    __declspec(dllexport) void ReFix_RegisterP2PPeerStr(uint64_t steamID, const char* ipStr);
    __declspec(dllexport) uint16_t ReFix_GetBoundGamePort();
}
