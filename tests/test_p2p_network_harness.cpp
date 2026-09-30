// =============================================================================
// ReFix - P2P Network Regression Test Harness
// tests/test_p2p_network_harness.cpp
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
#include <deque>
#include <iostream>
#include <atomic>

#define REFIX_TESTING 1
#include "../src/steam_p2p_hook.h"

// Stubs required by steam_p2p_hook.cpp
void ReFixLog(const char* fmt, ...) {
    // Silent or minimal log during tests
}
extern "C" void ReFix_OnGamePortVirtualized(uint16_t newPort) {
}

using namespace SteamP2PHookTest;

// =============================================================================
// Mock ISteamNetworking Implementation
// =============================================================================
struct MockP2PPacket {
    uint64_t steamIDRemote = 0;
    std::vector<uint8_t> data;
    int sendType = 0;
    int channel = 0;
};

static std::vector<MockP2PPacket> g_mockSentPackets;
static std::vector<uint64_t> g_mockAcceptedSessions;

static bool __thiscall Mock_SendP2PPacket(void* self, uint64_t steamIDRemote,
    const void* pubData, uint32_t cubData, int eP2PSendType, int nChannel)
{
    MockP2PPacket pkt;
    pkt.steamIDRemote = steamIDRemote;
    if (pubData && cubData > 0) {
        pkt.data.assign((const uint8_t*)pubData, (const uint8_t*)pubData + cubData);
    }
    pkt.sendType = eP2PSendType;
    pkt.channel = nChannel;
    g_mockSentPackets.push_back(std::move(pkt));
    return true;
}

static bool __thiscall Mock_IsP2PPacketAvailable(void* self, uint32_t* pcubMsgSize, int nChannel) {
    if (pcubMsgSize) *pcubMsgSize = 0;
    return false;
}

static bool __thiscall Mock_ReadP2PPacket(void* self, void* pubDest, uint32_t cubDest,
    uint32_t* pcubMsgSize, uint64_t* psteamIDRemote, int nChannel)
{
    return false;
}

static bool __thiscall Mock_AcceptP2PSessionWithUser(void* self, uint64_t steamIDRemote) {
    g_mockAcceptedSessions.push_back(steamIDRemote);
    return true;
}

static void* g_mockNetworkingVTable[8] = {
    (void*)Mock_SendP2PPacket,            // 0
    (void*)Mock_IsP2PPacketAvailable,     // 1
    (void*)Mock_ReadP2PPacket,            // 2
    (void*)Mock_AcceptP2PSessionWithUser, // 3
    nullptr,                              // 4
    nullptr,                              // 5
    nullptr,                              // 6
    nullptr                               // 7
};

struct MockNetworkingObject {
    void** vptr;
};
static MockNetworkingObject g_mockNetworkingObj = { g_mockNetworkingVTable };
static void* g_mockNetworkingInstance = (void*)&g_mockNetworkingObj;


// =============================================================================
// Mock Winsock Original Functions
// =============================================================================
struct MockRawSendCall {
    SOCKET s;
    std::vector<uint8_t> data;
    int flags;
    sockaddr_in to;
};
static std::vector<MockRawSendCall> g_mockRawSends;
static std::atomic<uint16_t> g_mockNextEphemeralPort{ 50000 };

static int WSAAPI Mock_orig_sendto(SOCKET s, const char* buf, int len, int flags,
    const struct sockaddr* to, int tolen)
{
    MockRawSendCall call;
    call.s = s;
    if (buf && len > 0) call.data.assign((const uint8_t*)buf, (const uint8_t*)buf + len);
    call.flags = flags;
    if (to && tolen >= sizeof(sockaddr_in)) {
        call.to = *(const sockaddr_in*)to;
    }
    g_mockRawSends.push_back(std::move(call));
    return len;
}

static int WSAAPI Mock_orig_recvfrom(SOCKET s, char* buf, int len, int flags,
    struct sockaddr* from, int* fromlen)
{
    WSASetLastError(WSAEWOULDBLOCK);
    return SOCKET_ERROR;
}

static int WSAAPI Mock_orig_select(int nfds, fd_set* readfds, fd_set* writefds,
    fd_set* exceptfds, const struct timeval* timeout)
{
    if (readfds) FD_ZERO(readfds);
    if (writefds) FD_ZERO(writefds);
    if (exceptfds) FD_ZERO(exceptfds);
    return 0; // Timeout
}

static int WSAAPI Mock_orig_bind(SOCKET s, const struct sockaddr* name, int namelen) {
    return 0;
}

static int WSAAPI Mock_orig_closesocket(SOCKET s) {
    return 0;
}

// =============================================================================
// Helper Setup
// =============================================================================
static void ResetHarness() {
    ResetState();
    g_mockSentPackets.clear();
    g_mockAcceptedSessions.clear();
    g_mockRawSends.clear();
    SetMockSteamNetworking(g_mockNetworkingInstance);
    SetOrigWinsock(Mock_orig_sendto, Mock_orig_recvfrom, Mock_orig_select, Mock_orig_bind, Mock_orig_closesocket);
}

// =============================================================================
// TEST CASES (FASE 8)
// =============================================================================

// 1. test_known_peer_send
// Sockets sends to mapped IP -> must route via Steam P2P, NOT fall through to raw sendto
static bool test_known_peer_send() {
    ResetHarness();
    SOCKET s = 1001;
    uint64_t peerSteamID = 76561198000000001ULL;
    uint32_t peerIP = 0x0A000002u; // 10.0.0.2

    SteamP2PHook::RegisterPeer(peerSteamID, peerIP, 7777);

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(peerIP);

    const char msg[] = "HELLO_P2P";
    int res = TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));

    if (res != (int)strlen(msg)) return false;
    if (g_mockSentPackets.size() != 1) return false;
    if (g_mockSentPackets[0].steamIDRemote != peerSteamID) return false;
    if (g_mockSentPackets[0].channel != 0) return false; // Game channel
    if (!g_mockRawSends.empty()) return false; // Must NOT leak to raw winsock
    return true;
}

// 2. test_unknown_peer_send
// Sending to unmapped peer -> should buffer in hold buffer and NOT leak to raw sendto immediately
static bool test_unknown_peer_send() {
    ResetHarness();
    SOCKET s = 1002;
    uint32_t unmappedIP = 0xC0A80132u; // 192.168.1.50

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(unmappedIP);

    const char msg[] = "PRE_LOBBY_HANDSHAKE";
    int res = TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));

    if (res != (int)strlen(msg)) return false;
    if (GetHoldBufferSize() != 1) return false;
    // Regression check for 577f1bc: Must NOT leak raw packet immediately
    if (!g_mockRawSends.empty()) {
        std::cout << "[REGRESSION DETECTED] Packet leaked to raw Winsock before peer resolution!\n";
        return false;
    }
    return true;
}

// 3. test_late_peer_resolution
// Packet held in buffer -> peer is registered -> flushed via Steam P2P exactly once
static bool test_late_peer_resolution() {
    ResetHarness();
    SOCKET s = 1003;
    uint32_t peerIP = 0x0A000003u; // 10.0.0.3
    uint64_t peerSteamID = 76561198000000002ULL;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(peerIP);

    const char msg[] = "HELD_PACKET";
    TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));
    if (GetHoldBufferSize() != 1) return false;

    // Register peer now
    SteamP2PHook::RegisterPeer(peerSteamID, peerIP, 7777);

    // Should be flushed
    if (GetHoldBufferSize() != 0) return false;
    if (g_mockSentPackets.size() != 1) return false;
    if (g_mockSentPackets[0].steamIDRemote != peerSteamID) return false;
    if (!g_mockRawSends.empty()) return false;
    return true;
}

// 4. test_single_peer_fallback
// When exactly 1 peer is known, connecting to loopback or private subnet or game port routes to that peer
static bool test_single_peer_fallback() {
    ResetHarness();
    SOCKET s = 1004;
    uint64_t hostSteamID = 76561198000000099ULL;
    SteamP2PHook::RegisterPeer(hostSteamID, 0x0A000099u, 7777);

    // Send to 127.0.0.1:7777 (common in Unreal / Unity client connect)
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(0x7F000001u);

    const char msg[] = "CONNECT_HOST";
    int res = TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));

    if (res != (int)strlen(msg)) return false;
    if (g_mockSentPackets.size() != 1) return false;
    if (g_mockSentPackets[0].steamIDRemote != hostSteamID) return false;
    return true;
}

// 5. test_multiple_peer_no_ambiguous_route
// When >1 peer is known, sending to an unknown IP does NOT guess randomly or misroute to wrong peer
static bool test_multiple_peer_no_ambiguous_route() {
    ResetHarness();
    SOCKET s = 1005;
    SteamP2PHook::RegisterPeer(1001ULL, 0x0A000001u, 7777);
    SteamP2PHook::RegisterPeer(1002ULL, 0x0A000002u, 7777);

    // Destination is unmapped IP
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(0xC0A80199u);

    const char msg[] = "AMBIGUOUS";
    TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));

    // Must NOT immediately send to any peer because destination is ambiguous
    if (!g_mockSentPackets.empty()) return false;
    // Must be in hold buffer
    if (GetHoldBufferSize() != 1) return false;
    return true;
}

// 6. test_incoming_exact_socket_route
// Incoming packet with destPort matches local socket -> routed to its queue
static bool test_incoming_exact_socket_route() {
    ResetHarness();
    SOCKET s = 2001;
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(7777);
    TestHook_bind(s, (sockaddr*)&bindAddr, sizeof(bindAddr));

    uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    TestRouteIncomingPacket(payload, sizeof(payload), 76561198000000001ULL, 5555, 7777, 1);

    if (GetSocketQueueCount(s) != 1) return false;
    if (GetPendingGamePacketsCount() != 0) return false;

    char recvBuf[64];
    sockaddr_in from{};
    int fromLen = sizeof(from);
    int recvd = TestHook_recvfrom(s, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&from, &fromLen);

    if (recvd != sizeof(payload)) return false;
    if (memcmp(recvBuf, payload, sizeof(payload)) != 0) return false;
    if (ntohs(from.sin_port) != 5555) return false;
    return true;
}

// 7. test_incoming_ephemeral_socket
// Socket bound with port 0 receives packet and select() reports it readable!
static bool test_incoming_ephemeral_socket() {
    ResetHarness();
    SOCKET s = 2002;
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(0); // Ephemeral
    TestHook_bind(s, (sockaddr*)&bindAddr, sizeof(bindAddr));

    uint8_t payload[] = { 'R', 'E', 'S', 'P', 'O', 'N', 'S', 'E' };
    // Packet arrives from server to client
    TestRouteIncomingPacket(payload, sizeof(payload), 76561198000000001ULL, 7777, 54321, 1);

    // Check select() readability
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    timeval tv{ 0, 10000 };
    int selRes = TestHook_select(0, &readfds, nullptr, nullptr, &tv);

    // In bug state, selRes == 0 and socket is not set
    if (selRes <= 0 || !FD_ISSET(s, &readfds)) {
        std::cout << "[REGRESSION DETECTED] select() failed to signal readability for ephemeral client socket!\n";
        return false;
    }

    char recvBuf[64];
    sockaddr_in from{};
    int fromLen = sizeof(from);
    int recvd = TestHook_recvfrom(s, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&from, &fromLen);

    if (recvd != sizeof(payload)) return false;
    if (memcmp(recvBuf, payload, sizeof(payload)) != 0) return false;
    return true;
}

// 8. test_nonstandard_game_port
// Game on custom port 9999 or 14000 (not in default port list) -> must route and not be trapped in Unknown
static bool test_nonstandard_game_port() {
    ResetHarness();
    SOCKET s = 2003;
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(9999);
    TestHook_bind(s, (sockaddr*)&bindAddr, sizeof(bindAddr));

    uint8_t payload[] = { 1, 2, 3, 4, 5 };
    TestRouteIncomingPacket(payload, sizeof(payload), 76561198000000001ULL, 8888, 9999, 1);

    // Check select() readability
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    timeval tv{ 0, 10000 };
    int selRes = TestHook_select(0, &readfds, nullptr, nullptr, &tv);

    if (selRes <= 0 || !FD_ISSET(s, &readfds)) {
        std::cout << "[REGRESSION DETECTED] select() failed on nonstandard port 9999!\n";
        return false;
    }

    char recvBuf[64];
    sockaddr_in from{};
    int fromLen = sizeof(from);
    int recvd = TestHook_recvfrom(s, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&from, &fromLen);

    if (recvd != sizeof(payload)) return false;
    return true;
}

// 9. test_game_and_voice_isolation
// Voice packet goes to voice socket (5058), game packet to game socket
static bool test_game_and_voice_isolation() {
    ResetHarness();
    SOCKET sGame = 2004;
    SOCKET sVoice = 2005;

    sockaddr_in gameAddr{};
    gameAddr.sin_family = AF_INET;
    gameAddr.sin_port = htons(7777);
    TestHook_bind(sGame, (sockaddr*)&gameAddr, sizeof(gameAddr));

    sockaddr_in voiceAddr{};
    voiceAddr.sin_family = AF_INET;
    voiceAddr.sin_port = htons(5058);
    TestHook_bind(sVoice, (sockaddr*)&voiceAddr, sizeof(voiceAddr));

    uint8_t gameData[] = { 'G', 'A', 'M', 'E' };
    uint8_t voiceData[] = { 'V', 'O', 'I', 'C', 'E' };

    TestRouteIncomingPacket(gameData, sizeof(gameData), 1ULL, 7777, 7777, 1); // Game
    TestRouteIncomingPacket(voiceData, sizeof(voiceData), 1ULL, 5058, 5058, 2); // Voice

    if (GetSocketQueueCount(sGame) != 1) return false;
    if (GetSocketQueueCount(sVoice) != 1) return false;

    char buf[32];
    int rG = TestHook_recvfrom(sGame, buf, sizeof(buf), 0, nullptr, nullptr);
    if (rG != sizeof(gameData) || memcmp(buf, gameData, sizeof(gameData)) != 0) return false;

    int rV = TestHook_recvfrom(sVoice, buf, sizeof(buf), 0, nullptr, nullptr);
    if (rV != sizeof(voiceData) || memcmp(buf, voiceData, sizeof(voiceData)) != 0) return false;

    return true;
}

// 10. test_legacy_unframed_packet
// Raw packet without REFX header is received and delivered without corruption
static bool test_legacy_unframed_packet() {
    ResetHarness();
    SOCKET s = 2006;
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(7777);
    TestHook_bind(s, (sockaddr*)&bindAddr, sizeof(bindAddr));

    uint8_t rawLegacy[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    TestRouteIncomingRawPacket(rawLegacy, sizeof(rawLegacy), 1ULL, 0);

    char buf[32];
    int r = TestHook_recvfrom(s, buf, sizeof(buf), 0, nullptr, nullptr);
    if (r != sizeof(rawLegacy)) return false;
    if (memcmp(buf, rawLegacy, sizeof(rawLegacy)) != 0) return false;
    return true;
}

// 11. test_hold_buffer_flush
// Verify FlushHoldBuffer flushes packets
static bool test_hold_buffer_flush() {
    ResetHarness();
    SOCKET s = 2007;
    uint32_t peerIP = 0x0A000007u;
    uint64_t peerSteamID = 76561198000000007ULL;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(peerIP);

    const char msg[] = "TEST_FLUSH";
    TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));

    // Register mapping
    SteamP2PHook::RegisterPeer(peerSteamID, peerIP, 7777);
    SteamP2PHook::FlushHoldBuffer();

    if (GetHoldBufferSize() != 0) return false;
    if (g_mockSentPackets.size() != 1) return false;
    return true;
}

// 12. test_hold_buffer_expiry
// Verify expired packet falls through cleanly without crashing
static bool test_hold_buffer_expiry() {
    ResetHarness();
    SOCKET s = 2008;
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(0x0A000099u);

    const char msg[] = "EXPIRE_ME";
    TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));

    // Wait or simulate TTL expiration
    // In FlushHoldBuffer, now - timestampMs >= 5000
    Sleep(5100);
    SteamP2PHook::FlushHoldBuffer();

    if (GetHoldBufferSize() != 0) return false;
    return true;
}

// 13. test_socket_close_cleanup
// Verify closing a socket cleans up its context and queues
static bool test_socket_close_cleanup() {
    ResetHarness();
    SOCKET s = 2009;
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(7777);
    TestHook_bind(s, (sockaddr*)&bindAddr, sizeof(bindAddr));

    uint8_t payload[] = { 1, 2, 3 };
    TestRouteIncomingPacket(payload, sizeof(payload), 1ULL, 5555, 7777, 1);
    if (GetSocketQueueCount(s) != 1) return false;

    TestHook_closesocket(s);
    if (GetSocketQueueCount(s) != 0) return false;
    return true;
}

// 14. test_host_migration_routing_integrity
// Verify host migration preserves server IP exclusively for new host
static bool test_host_migration_routing_integrity() {
    ResetHarness();
    uint32_t serverIP = 0xC0A80132u; // 192.168.1.50
    uint64_t oldHostID = 100ULL;
    uint64_t newHostID = 200ULL;

    // Step 1: Initial host is oldHostID
    SteamP2PHook::RegisterPeer(oldHostID, serverIP, 7777);

    // Step 2: Host migration occurs -> newHostID is registered as the server IP
    SteamP2PHook::RegisterPeer(newHostID, serverIP, 7777);

    // Step 3: oldHostID is re-registered as a client with synthetic IP
    uint32_t oldHostSynthetic = 0x0A000001u | (uint32_t)(oldHostID & 0x00FFFFFFu);
    SteamP2PHook::RegisterPeer(oldHostID, oldHostSynthetic, 7777);

    // Step 4: Game sends to serverIP -> MUST route to newHostID, NOT oldHostID
    SOCKET s = 2014;
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(serverIP);

    const char msg[] = "HOST_MIGRATION_TEST";
    TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));

    if (g_mockSentPackets.empty()) return false;
    if (g_mockSentPackets[0].steamIDRemote != newHostID) return false;
    return true;
}

// 15. test_multi_socket_game_port_isolation
// Verify multiple game sockets on different ports receive only their respective packets
static bool test_multi_socket_game_port_isolation() {
    ResetHarness();
    SOCKET sGame = 2015;
    SOCKET sBeacon = 2016;

    sockaddr_in addrGame{};
    addrGame.sin_family = AF_INET;
    addrGame.sin_port = htons(7777);
    TestHook_bind(sGame, (sockaddr*)&addrGame, sizeof(addrGame));

    sockaddr_in addrBeacon{};
    addrBeacon.sin_family = AF_INET;
    addrBeacon.sin_port = htons(7778);
    TestHook_bind(sBeacon, (sockaddr*)&addrBeacon, sizeof(addrBeacon));

    uint8_t pktGame[] = { 0xAA, 0xBB };
    uint8_t pktBeacon[] = { 0xCC, 0xDD };

    TestRouteIncomingPacket(pktGame, sizeof(pktGame), 10ULL, 5555, 7777, 1);
    TestRouteIncomingPacket(pktBeacon, sizeof(pktBeacon), 10ULL, 5555, 7778, 1);

    if (GetSocketQueueCount(sGame) != 1) return false;
    if (GetSocketQueueCount(sBeacon) != 1) return false;
    return true;
}

// 16. test_pending_queue_port_aware_drain
// Verify pre-bind pending packets are only drained by matching port
static bool test_pending_queue_port_aware_drain() {
    ResetHarness();
    // Route packet with destPort 7777 before any socket binds
    uint8_t payload[] = { 1, 2, 3, 4 };
    TestRouteIncomingPacket(payload, sizeof(payload), 10ULL, 5555, 7777, 1);

    // Socket binds to 7778 (beacon) -> MUST NOT steal packet for 7777
    SOCKET sBeacon = 2017;
    sockaddr_in addrBeacon{};
    addrBeacon.sin_family = AF_INET;
    addrBeacon.sin_port = htons(7778);
    TestHook_bind(sBeacon, (sockaddr*)&addrBeacon, sizeof(addrBeacon));

    if (GetSocketQueueCount(sBeacon) != 0) return false;

    // Socket binds to 7777 (game) -> MUST drain the packet
    SOCKET sGame = 2018;
    sockaddr_in addrGame{};
    addrGame.sin_family = AF_INET;
    addrGame.sin_port = htons(7777);
    TestHook_bind(sGame, (sockaddr*)&addrGame, sizeof(addrGame));

    if (GetSocketQueueCount(sGame) != 1) return false;
    return true;
}

// 17. test_hold_buffer_overflow_no_raw_leak
// Verify hold buffer saturation caps at 512 without leaking to raw Winsock
static bool test_hold_buffer_overflow_no_raw_leak() {
    ResetHarness();
    SOCKET s = 2019;
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(0x0A0000AAu);

    const char msg[] = "BURST";
    for (int i = 0; i < 600; i++) {
        TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));
    }

    if (GetHoldBufferSize() != 512) return false;
    if (!g_mockRawSends.empty()) return false; // 0 leaks to raw Winsock!
    return true;
}

// 18. test_hold_buffer_purge_on_socket_close
// Verify closing a socket purges any held packets for that socket
static bool test_hold_buffer_purge_on_socket_close() {
    ResetHarness();
    SOCKET s = 2020;
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(7777);
    dest.sin_addr.s_addr = htonl(0x0A0000BBu);

    const char msg[] = "HELD";
    TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));
    if (GetHoldBufferSize() != 1) return false;

    TestHook_closesocket(s);
    if (GetHoldBufferSize() != 0) return false; // Purged on closesocket!
    return true;
}

// 19. test_single_peer_fallback_after_peer_leave
// Verify single_peer_fallback continues working after a peer unregisters
static bool test_single_peer_fallback_after_peer_leave() {
    ResetHarness();
    uint64_t peer1 = 111ULL;
    uint64_t peer2 = 222ULL;

    SteamP2PHook::RegisterPeer(peer1, 0x0A000001u, 7777);
    SteamP2PHook::RegisterPeer(peer2, 0x0A000002u, 7777);

    // Unregister peer2 -> exactly 1 peer remains
    SteamP2PHook::UnregisterPeer(peer2);

    SOCKET s = 2021;
    sockaddr_in destLoopback{};
    destLoopback.sin_family = AF_INET;
    destLoopback.sin_port = htons(7777);
    destLoopback.sin_addr.s_addr = htonl(0x7F000001u);

    const char msg[] = "LEAVE_FALLBACK";
    TestHook_sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&destLoopback, sizeof(destLoopback));

    if (g_mockSentPackets.empty()) return false;
    if (g_mockSentPackets[0].steamIDRemote != peer1) return false;
    return true;
}

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================
int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    std::cout << "====================================================================\n";
    std::cout << "[*] Running ReFix P2P Network Regression Test Harness...\n";
    std::cout << "====================================================================\n";


    struct TestCase {
        const char* name;
        bool (*func)();
    };

    TestCase tests[] = {
        { "test_known_peer_send", test_known_peer_send },
        { "test_unknown_peer_send", test_unknown_peer_send },
        { "test_late_peer_resolution", test_late_peer_resolution },
        { "test_single_peer_fallback", test_single_peer_fallback },
        { "test_multiple_peer_no_ambiguous_route", test_multiple_peer_no_ambiguous_route },
        { "test_incoming_exact_socket_route", test_incoming_exact_socket_route },
        { "test_incoming_ephemeral_socket", test_incoming_ephemeral_socket },
        { "test_nonstandard_game_port", test_nonstandard_game_port },
        { "test_game_and_voice_isolation", test_game_and_voice_isolation },
        { "test_legacy_unframed_packet", test_legacy_unframed_packet },
        { "test_hold_buffer_flush", test_hold_buffer_flush },
        { "test_hold_buffer_expiry", test_hold_buffer_expiry },
        { "test_socket_close_cleanup", test_socket_close_cleanup },
        { "test_host_migration_routing_integrity", test_host_migration_routing_integrity },
        { "test_multi_socket_game_port_isolation", test_multi_socket_game_port_isolation },
        { "test_pending_queue_port_aware_drain", test_pending_queue_port_aware_drain },
        { "test_hold_buffer_overflow_no_raw_leak", test_hold_buffer_overflow_no_raw_leak },
        { "test_hold_buffer_purge_on_socket_close", test_hold_buffer_purge_on_socket_close },
        { "test_single_peer_fallback_after_peer_leave", test_single_peer_fallback_after_peer_leave }
    };

    int passed = 0;
    int failed = 0;

    for (const auto& t : tests) {
        std::cout << "[RUN ] " << t.name << "... ";
        std::cout.flush();
        bool ok = t.func();
        if (ok) {
            std::cout << "PASSED\n";
            passed++;
        } else {
            std::cout << "FAILED\n";
            failed++;
        }
    }

    std::cout << "====================================================================\n";
    std::cout << "[SUMMARY] Total: " << (passed + failed) << " | Passed: " << passed << " | Failed: " << failed << "\n";
    std::cout << "====================================================================\n";

    return (failed == 0) ? 0 : 1;
}
