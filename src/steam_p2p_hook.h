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

#ifdef REFIX_TESTING
#include <winsock2.h>
#include <cstddef>

namespace SteamP2PHookTest {
    typedef int (WSAAPI* fn_sendto_t)(SOCKET s, const char* buf, int len, int flags,
        const struct sockaddr* to, int tolen);
    typedef int (WSAAPI* fn_recvfrom_t)(SOCKET s, char* buf, int len, int flags,
        struct sockaddr* from, int* fromlen);
    typedef int (WSAAPI* fn_select_t)(int nfds, fd_set* readfds, fd_set* writefds,
        fd_set* exceptfds, const struct timeval* timeout);
    typedef int (WSAAPI* fn_bind_t)(SOCKET s, const struct sockaddr* name, int namelen);
    typedef int (WSAAPI* fn_closesocket_t)(SOCKET s);

    void ResetState();
    void SetMockSteamNetworking(void* mock);
    void SetOrigWinsock(fn_sendto_t s, fn_recvfrom_t r, fn_select_t sel, fn_bind_t b, fn_closesocket_t c);

    int TestHook_sendto(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen);
    int TestHook_recvfrom(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen);
    int TestHook_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout);
    int TestHook_bind(SOCKET s, const struct sockaddr* name, int namelen);
    int TestHook_closesocket(SOCKET s);

    void TestRouteIncomingPacket(const uint8_t* data, size_t len, uint64_t fromSteamID, uint16_t fromPort, uint16_t destPort, int service);
    void TestRouteIncomingRawPacket(const uint8_t* rawPkt, size_t len, uint64_t fromSteamID, int channel);

    size_t GetHoldBufferSize();
    size_t GetPendingGamePacketsCount();
    size_t GetPendingVoicePacketsCount();
    size_t GetSocketQueueCount(SOCKET s);
    uint32_t GetPeerIP(uint64_t steamID);
    uint64_t GetPeerSteamID(uint32_t ip);
}
#endif

extern "C" {
    __declspec(dllexport) void ReFix_RegisterP2PPeer(uint64_t steamID, uint32_t ipv4_host);
    __declspec(dllexport) void ReFix_RegisterP2PPeerStr(uint64_t steamID, const char* ipStr);
    __declspec(dllexport) uint16_t ReFix_GetBoundGamePort();
}

