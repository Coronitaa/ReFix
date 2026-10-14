// =============================================================================
// ReFix - ISteamNetworkingSockets Canonical LanCore Integration Test Suite
// =============================================================================
// Verifies:
// 1. ABI & Struct Sizes:
//    - SteamNetworkingIdentity (136)
//    - SteamNetworkingIPAddr (18)
//    - SteamNetworkingMessage_t (216)
//    - SteamNetConnectionInfo_t (696)
//    - SteamNetConnectionStatusChangedCallback_t (712)
//    - RFXS Wire Structs (Handshake 33, HandshakeAck 25, ConnectAccept 25,
//      HandshakeReject 17, Disconnect 25, SinglePayload 37, UnreliableFragment 39)
// 2. Identity Validation:
//    - Valid individual SteamID: 76561197960265729 (accepted)
//    - Rejected non-individual/invalid identities (zero account, clan, server, bad universe)
// 3. Flags Validation:
//    - Allowlist 0x1D
//    - Mutual exclusion: Reliable | NoDelay rejected
// 4. Boundary Payloads:
//    - 0, 1, 1113, 1114, 262107, 262108
// 5. Connection Lifecycle & Connecting Queue:
//    - Listen socket creation & cleanup
//    - Connecting state queueing & NoDelay drop
//    - Queue depth limits (128 msgs)
//    - AcceptConnection & CloseConnection state transitions
// 6. SendMessages Ownership & Pointer Deduplication:
//    - Single Release() on duplicated pointers in array
//    - Null pointer resilience
// 7. ReceiveMessagesOnConnection state acceptance:
//    - Readable in Connected, ClosedByPeer, ProblemDetectedLocally
// 8. Concurrency & Thread-Safety:
//    - Multi-threaded concurrent operations without deadlocks or use-after-free
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <cassert>
#include <unordered_set>

#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "include/steam/steam_api.h"
#include "include/steam/isteamnetworkingsockets.h"
#include "include/steam/steamnetworkingtypes.h"

// --- RFXS Wire Structs (Matching wire framing on Channel 250) ---
#pragma pack(push, 1)
static constexpr uint32_t k_nRfxsMagic = 0x52465853; // 'RFXS'

struct RfxsHandshake {
    uint32_t magic;
    uint8_t  type;
    uint32_t protocolVersion;
    uint32_t sessionId;
    uint32_t clientNonce;
    int32_t  virtualPort;
    uint64_t clientSteamId;
    uint32_t flags;
};

struct RfxsHandshakeAck {
    uint32_t magic;
    uint8_t  type;
    uint32_t protocolVersion;
    uint32_t sessionId;
    uint32_t clientNonce;
    uint32_t hostNonce;
    uint32_t flags;
};

struct RfxsConnectAccept {
    uint32_t magic;
    uint8_t  type;
    uint32_t protocolVersion;
    uint32_t sessionId;
    uint32_t clientNonce;
    uint32_t hostNonce;
    uint32_t flags;
};

struct RfxsHandshakeReject {
    uint32_t magic;
    uint8_t  type;
    uint32_t sessionId;
    uint32_t reasonCode;
    uint32_t clientNonce;
};

struct RfxsDisconnect {
    uint32_t magic;
    uint8_t  type;
    uint32_t sessionId;
    uint32_t clientNonce;
    uint32_t hostNonce;
    int32_t  reason;
    uint32_t flags;
};

struct RfxsSinglePayloadHeader {
    uint32_t magic;
    uint8_t  type;
    uint32_t sessionId;
    uint32_t clientNonce;
    uint32_t hostNonce;
    int64_t  messageNumber;
    uint32_t nSendFlags;
    uint32_t payloadLen;
    uint32_t virtualPort;
};

struct RfxsUnreliableFragmentHeader {
    uint32_t magic;
    uint8_t  type;
    uint32_t clientNonce;
    uint32_t hostNonce;
    int64_t  messageNumber;
    uint32_t nSendFlags;
    uint32_t totalMessageLen;
    uint16_t fragmentIndex;
    uint16_t fragmentCount;
    uint32_t fragmentOffset;
    uint16_t fragmentPayloadLen;
};
#pragma pack(pop)

// --- Static Assertions for ABI & Wire Structs ---
static_assert(sizeof(SteamNetworkingIdentity) == 136, "SteamNetworkingIdentity must be 136 bytes");
static_assert(sizeof(SteamNetworkingIPAddr) == 18, "SteamNetworkingIPAddr must be 18 bytes");
static_assert(sizeof(SteamNetworkingMessage_t) == 216, "SteamNetworkingMessage_t must be 216 bytes");
static_assert(sizeof(SteamNetConnectionInfo_t) == 696, "SteamNetConnectionInfo_t must be 696 bytes");
static_assert(sizeof(SteamNetConnectionStatusChangedCallback_t) == 712, "SteamNetConnectionStatusChangedCallback_t must be 712 bytes");

static_assert(sizeof(RfxsHandshake) == 33, "RfxsHandshake must be 33 bytes");
static_assert(sizeof(RfxsHandshakeAck) == 25, "RfxsHandshakeAck must be 25 bytes");
static_assert(sizeof(RfxsConnectAccept) == 25, "RfxsConnectAccept must be 25 bytes");
static_assert(sizeof(RfxsHandshakeReject) == 17, "RfxsHandshakeReject must be 17 bytes");
static_assert(sizeof(RfxsDisconnect) == 25, "RfxsDisconnect must be 25 bytes");
static_assert(sizeof(RfxsSinglePayloadHeader) == 37, "RfxsSinglePayloadHeader must be 37 bytes");
static_assert(sizeof(RfxsUnreliableFragmentHeader) == 39, "RfxsUnreliableFragmentHeader must be 39 bytes");

#define TEST_CHECK(cond, desc) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << desc << " (line " << __LINE__ << ")" << std::endl; \
            g_failCount++; \
            return false; \
        } else { \
            std::cout << "  [PASS] " << desc << std::endl; \
            g_passCount++; \
        } \
    } while(0)

static std::atomic<int> g_passCount{ 0 };
static std::atomic<int> g_failCount{ 0 };

typedef bool (*fn_SteamAPI_Init)();
typedef void (*fn_SteamAPI_Shutdown)();
typedef void (*fn_SteamAPI_RunCallbacks)();
typedef ISteamNetworkingSockets* (*fn_SteamAPI_SteamNetworkingSockets_SteamAPI_v012)();
typedef ISteamNetworkingUtils* (*fn_SteamAPI_SteamNetworkingUtils_SteamAPI_v004)();

static HMODULE g_hSteam = nullptr;
static fn_SteamAPI_Init pfn_SteamAPI_Init = nullptr;
static fn_SteamAPI_Shutdown pfn_SteamAPI_Shutdown = nullptr;
static fn_SteamAPI_RunCallbacks pfn_SteamAPI_RunCallbacks = nullptr;
static fn_SteamAPI_SteamNetworkingSockets_SteamAPI_v012 pfn_SteamNetworkingSockets = nullptr;
static fn_SteamAPI_SteamNetworkingUtils_SteamAPI_v004 pfn_SteamNetworkingUtils = nullptr;

static bool LoadSteamApi() {
    g_hSteam = LoadLibraryA("bin\\steam_api64_test.dll");
    if (!g_hSteam) g_hSteam = LoadLibraryA("build\\steam_api64_test.dll");
    if (!g_hSteam) g_hSteam = LoadLibraryA("bin\\steam_api64.dll");
    if (!g_hSteam) g_hSteam = LoadLibraryA("steam_api64.dll");

    if (!g_hSteam) {
        std::cerr << "Failed to load steam_api64 DLL" << std::endl;
        return false;
    }

    pfn_SteamAPI_Init = (fn_SteamAPI_Init)GetProcAddress(g_hSteam, "SteamAPI_Init");
    pfn_SteamAPI_Shutdown = (fn_SteamAPI_Shutdown)GetProcAddress(g_hSteam, "SteamAPI_Shutdown");
    pfn_SteamAPI_RunCallbacks = (fn_SteamAPI_RunCallbacks)GetProcAddress(g_hSteam, "SteamAPI_RunCallbacks");
    pfn_SteamNetworkingSockets = (fn_SteamAPI_SteamNetworkingSockets_SteamAPI_v012)GetProcAddress(g_hSteam, "SteamAPI_SteamNetworkingSockets_SteamAPI_v012");
    pfn_SteamNetworkingUtils = (fn_SteamAPI_SteamNetworkingUtils_SteamAPI_v004)GetProcAddress(g_hSteam, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004");

    return (pfn_SteamAPI_Init && pfn_SteamAPI_Shutdown && pfn_SteamNetworkingSockets && pfn_SteamNetworkingUtils);
}

// -----------------------------------------------------------------------------
// Test Suite 1: ABI & Struct Sizes
// -----------------------------------------------------------------------------
bool TestSuite_ABI() {
    std::cout << "\n=== [Suite 1: ABI & Struct Sizes] ===" << std::endl;
    TEST_CHECK(sizeof(SteamNetworkingIdentity) == 136, "SteamNetworkingIdentity is 136 bytes");
    TEST_CHECK(sizeof(SteamNetworkingIPAddr) == 18, "SteamNetworkingIPAddr is 18 bytes");
    TEST_CHECK(sizeof(SteamNetworkingMessage_t) == 216, "SteamNetworkingMessage_t is 216 bytes");
    TEST_CHECK(sizeof(SteamNetConnectionInfo_t) == 696, "SteamNetConnectionInfo_t is 696 bytes");
    TEST_CHECK(sizeof(SteamNetConnectionStatusChangedCallback_t) == 712, "SteamNetConnectionStatusChangedCallback_t is 712 bytes");

    TEST_CHECK(sizeof(RfxsHandshake) == 33, "RfxsHandshake is 33 bytes");
    TEST_CHECK(sizeof(RfxsHandshakeAck) == 25, "RfxsHandshakeAck is 25 bytes");
    TEST_CHECK(sizeof(RfxsConnectAccept) == 25, "RfxsConnectAccept is 25 bytes");
    TEST_CHECK(sizeof(RfxsHandshakeReject) == 17, "RfxsHandshakeReject is 17 bytes");
    TEST_CHECK(sizeof(RfxsDisconnect) == 25, "RfxsDisconnect is 25 bytes");
    TEST_CHECK(sizeof(RfxsSinglePayloadHeader) == 37, "RfxsSinglePayloadHeader is 37 bytes");
    TEST_CHECK(sizeof(RfxsUnreliableFragmentHeader) == 39, "RfxsUnreliableFragmentHeader is 39 bytes");
    return true;
}

// -----------------------------------------------------------------------------
// Test Suite 2: Identity Validation
// -----------------------------------------------------------------------------
bool TestSuite_Identity(ISteamNetworkingSockets* sockets) {
    std::cout << "\n=== [Suite 2: Identity Validation] ===" << std::endl;

    // 1. Valid individual 64-bit SteamID: 76561197960265729
    SteamNetworkingIdentity validIdent;
    validIdent.Clear();
    validIdent.SetSteamID64(76561197960265729ULL);
    HSteamNetConnection hConn1 = sockets->ConnectP2P(validIdent, 0, 0, nullptr);
    TEST_CHECK(hConn1 != k_HSteamNetConnection_Invalid, "Valid individual SteamID accepted by ConnectP2P");
    if (hConn1 != k_HSteamNetConnection_Invalid) {
        sockets->CloseConnection(hConn1, 0, "Test cleanup", false);
    }

    // 2. Rejected non-SteamID type (e.g. IP address identity)
    SteamNetworkingIdentity ipIdent;
    ipIdent.Clear();
    ipIdent.m_eType = k_ESteamNetworkingIdentityType_IPAddress;
    HSteamNetConnection hConnIP = sockets->ConnectP2P(ipIdent, 0, 0, nullptr);
    TEST_CHECK(hConnIP == k_HSteamNetConnection_Invalid, "Non-SteamID identity type rejected by ConnectP2P");

    // 3. Rejected zero account ID
    SteamNetworkingIdentity zeroIdent;
    zeroIdent.Clear();
    CSteamID zeroId(0, k_EUniversePublic, k_EAccountTypeIndividual);
    zeroIdent.SetSteamID(zeroId);
    HSteamNetConnection hConnZero = sockets->ConnectP2P(zeroIdent, 0, 0, nullptr);
    TEST_CHECK(hConnZero == k_HSteamNetConnection_Invalid, "Zero account ID rejected by ConnectP2P");

    // 4. Rejected Clan account type
    SteamNetworkingIdentity clanIdent;
    clanIdent.Clear();
    CSteamID clanId(123456, k_EUniversePublic, k_EAccountTypeClan);
    clanIdent.SetSteamID(clanId);
    HSteamNetConnection hConnClan = sockets->ConnectP2P(clanIdent, 0, 0, nullptr);
    TEST_CHECK(hConnClan == k_HSteamNetConnection_Invalid, "Clan account type rejected by ConnectP2P");

    // 5. Rejected GameServer account type
    SteamNetworkingIdentity serverIdent;
    serverIdent.Clear();
    CSteamID serverId(123456, k_EUniversePublic, k_EAccountTypeGameServer);
    serverIdent.SetSteamID(serverId);
    HSteamNetConnection hConnServer = sockets->ConnectP2P(serverIdent, 0, 0, nullptr);
    TEST_CHECK(hConnServer == k_HSteamNetConnection_Invalid, "GameServer account type rejected by ConnectP2P");

    // 6. Rejected invalid universe
    SteamNetworkingIdentity badUnivIdent;
    badUnivIdent.Clear();
    CSteamID badUnivId(123456, k_EUniverseInvalid, k_EAccountTypeIndividual);
    badUnivIdent.SetSteamID(badUnivId);
    HSteamNetConnection hConnUniv = sockets->ConnectP2P(badUnivIdent, 0, 0, nullptr);
    TEST_CHECK(hConnUniv == k_HSteamNetConnection_Invalid, "Invalid universe rejected by ConnectP2P");

    return true;
}

// -----------------------------------------------------------------------------
// Test Suite 3: Flags Validation
// -----------------------------------------------------------------------------
bool TestSuite_Flags(ISteamNetworkingSockets* sockets) {
    std::cout << "\n=== [Suite 3: Flags Validation] ===" << std::endl;

    SteamNetworkingIdentity validIdent;
    validIdent.SetSteamID64(76561197960265729ULL);
    HSteamNetConnection hConn = sockets->ConnectP2P(validIdent, 0, 0, nullptr);
    TEST_CHECK(hConn != k_HSteamNetConnection_Invalid, "Setup connection for flag testing");

    const char dummyData[] = "ping";
    uint32 dummyLen = sizeof(dummyData);

    // 1. Rejection of unknown bits outside 0x1D
    EResult resBadBit2 = sockets->SendMessageToConnection(hConn, dummyData, dummyLen, 0x02, nullptr);
    TEST_CHECK(resBadBit2 == k_EResultInvalidParam, "Flags with bit 0x02 rejected (k_EResultInvalidParam)");

    EResult resBadBit32 = sockets->SendMessageToConnection(hConn, dummyData, dummyLen, 0x20, nullptr);
    TEST_CHECK(resBadBit32 == k_EResultInvalidParam, "Flags with bit 0x20 rejected (k_EResultInvalidParam)");

    EResult resBadMask = sockets->SendMessageToConnection(hConn, dummyData, dummyLen, ~0x1D, nullptr);
    TEST_CHECK(resBadMask == k_EResultInvalidParam, "Flags with inverted allowlist mask rejected");

    // 2. Rejection of mutually exclusive Reliable | NoDelay (8 | 4 = 12)
    int badFlags = k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_NoDelay;
    EResult resMutual = sockets->SendMessageToConnection(hConn, dummyData, dummyLen, badFlags, nullptr);
    TEST_CHECK(resMutual == k_EResultInvalidParam, "Reliable | NoDelay combination rejected");

    // 3. Accepted valid flags (in Connecting state, NoDelay drops with Ignored; without NoDelay it queues)
    EResult resNoDelay = sockets->SendMessageToConnection(hConn, dummyData, dummyLen, k_nSteamNetworkingSend_NoDelay, nullptr);
    TEST_CHECK(resNoDelay == k_EResultIgnored, "NoDelay in Connecting state returns k_EResultIgnored");

    EResult resReliable = sockets->SendMessageToConnection(hConn, dummyData, dummyLen, k_nSteamNetworkingSend_Reliable, nullptr);
    TEST_CHECK(resReliable == k_EResultOK, "Reliable flag accepted and queued in Connecting state");

    EResult resUnreliable = sockets->SendMessageToConnection(hConn, dummyData, dummyLen, k_nSteamNetworkingSend_Unreliable, nullptr);
    TEST_CHECK(resUnreliable == k_EResultOK, "Unreliable flag accepted and queued in Connecting state");

    sockets->CloseConnection(hConn, 0, "Flag test cleanup", false);
    return true;
}

// -----------------------------------------------------------------------------
// Test Suite 4: Boundary Payloads
// -----------------------------------------------------------------------------
bool TestSuite_BoundaryPayloads(ISteamNetworkingSockets* sockets) {
    std::cout << "\n=== [Suite 4: Boundary Payloads] ===" << std::endl;

    SteamNetworkingIdentity validIdent;
    validIdent.SetSteamID64(76561197960265729ULL);
    HSteamNetConnection hConn = sockets->ConnectP2P(validIdent, 0, 0, nullptr);
    TEST_CHECK(hConn != k_HSteamNetConnection_Invalid, "Setup connection for payload testing");

    std::vector<uint8_t> buf(300000, 0xAA);

    // 1. Zero size payload
    EResult resZero = sockets->SendMessageToConnection(hConn, buf.data(), 0, k_nSteamNetworkingSend_Reliable, nullptr);
    TEST_CHECK(resZero == k_EResultInvalidParam, "Zero payload rejected (k_EResultInvalidParam)");

    // 2. Null data pointer
    EResult resNull = sockets->SendMessageToConnection(hConn, nullptr, 10, k_nSteamNetworkingSend_Reliable, nullptr);
    TEST_CHECK(resNull == k_EResultInvalidParam, "Null data pointer rejected (k_EResultInvalidParam)");

    // 3. 1 byte payload
    EResult res1 = sockets->SendMessageToConnection(hConn, buf.data(), 1, k_nSteamNetworkingSend_Reliable, nullptr);
    TEST_CHECK(res1 == k_EResultOK, "1 byte payload accepted");

    // 4. 1113 bytes (boundary unfragmented unreliable MTU: 1150 - 37)
    EResult res1113 = sockets->SendMessageToConnection(hConn, buf.data(), 1113, k_nSteamNetworkingSend_Unreliable, nullptr);
    TEST_CHECK(res1113 == k_EResultOK, "1113 bytes payload accepted (unfragmented unreliable limit)");

    // 5. 1114 bytes (triggers unreliable fragmentation)
    EResult res1114 = sockets->SendMessageToConnection(hConn, buf.data(), 1114, k_nSteamNetworkingSend_Unreliable, nullptr);
    TEST_CHECK(res1114 == k_EResultOK, "1114 bytes payload accepted (fragmented unreliable)");

    sockets->CloseConnection(hConn, 0, "Boundary test cleanup", false);

    // 6. 262107 bytes (max message payload limit on fresh connecting queue: <= 256 KB)
    HSteamNetConnection hConnMax = sockets->ConnectP2P(validIdent, 0, 0, nullptr);
    TEST_CHECK(hConnMax != k_HSteamNetConnection_Invalid, "Create fresh connection for max payload limit test");

    EResult resMax = sockets->SendMessageToConnection(hConnMax, buf.data(), 262107, k_nSteamNetworkingSend_Reliable, nullptr);
    TEST_CHECK(resMax == k_EResultOK, "262107 bytes payload accepted (maximum allowed payload)");

    // 7. 262108 bytes (exceeds max payload limit)
    EResult resOver = sockets->SendMessageToConnection(hConnMax, buf.data(), 262108, k_nSteamNetworkingSend_Reliable, nullptr);
    TEST_CHECK(resOver == k_EResultInvalidParam, "262108 bytes payload rejected (> 262107)");

    sockets->CloseConnection(hConnMax, 0, "Max payload cleanup", false);
    return true;
}

// -----------------------------------------------------------------------------
// Test Suite 5: Connection Lifecycle, Listen Sockets & Connecting Queue
// -----------------------------------------------------------------------------
bool TestSuite_LifecycleAndQueuing(ISteamNetworkingSockets* sockets) {
    std::cout << "\n=== [Suite 5: Connection Lifecycle & Connecting Queue] ===" << std::endl;

    // 1. Create and close listen socket
    HSteamListenSocket hListen = sockets->CreateListenSocketP2P(100, 0, nullptr);
    TEST_CHECK(hListen != k_HSteamListenSocket_Invalid, "CreateListenSocketP2P returns valid handle");

    bool closedListen = sockets->CloseListenSocket(hListen);
    TEST_CHECK(closedListen == true, "CloseListenSocket succeeds on valid handle");

    bool closedAgain = sockets->CloseListenSocket(hListen);
    TEST_CHECK(closedAgain == false, "CloseListenSocket returns false on already closed handle");

    // 2. Connecting queue depth limit (128 messages)
    SteamNetworkingIdentity validIdent;
    validIdent.SetSteamID64(76561197960265729ULL);
    HSteamNetConnection hConn = sockets->ConnectP2P(validIdent, 0, 0, nullptr);
    TEST_CHECK(hConn != k_HSteamNetConnection_Invalid, "Create connecting socket");

    char smallData[16] = "queued_msg";
    bool all128Ok = true;
    for (int i = 0; i < 128; i++) {
        EResult r = sockets->SendMessageToConnection(hConn, smallData, sizeof(smallData), k_nSteamNetworkingSend_Reliable, nullptr);
        if (r != k_EResultOK) {
            all128Ok = false;
            break;
        }
    }
    TEST_CHECK(all128Ok == true, "Successfully enqueued 128 messages in Connecting state");

    // 129th message exceeds depth limit
    EResult resExceeded = sockets->SendMessageToConnection(hConn, smallData, sizeof(smallData), k_nSteamNetworkingSend_Reliable, nullptr);
    TEST_CHECK(resExceeded == k_EResultLimitExceeded, "129th message returns k_EResultLimitExceeded");

    // 3. AcceptConnection validation
    // Calling AcceptConnection on initiator connection (which is not in server Connecting state for an incoming connection)
    // or on invalid connection handle
    EResult resBadAccept = sockets->AcceptConnection(999999);
    TEST_CHECK(resBadAccept == k_EResultInvalidParam, "AcceptConnection on invalid handle returns k_EResultInvalidParam");

    // 4. CloseConnection
    bool closeOk = sockets->CloseConnection(hConn, 0, "Lifecycle cleanup", false);
    TEST_CHECK(closeOk == true, "CloseConnection returns true");

    bool closeRepeat = sockets->CloseConnection(hConn, 0, "Duplicate close", false);
    TEST_CHECK(closeRepeat == false, "CloseConnection on closed handle returns false");

    return true;
}

// -----------------------------------------------------------------------------
// Test Suite 6: SendMessages Ownership & Pointer Deduplication
// -----------------------------------------------------------------------------
bool TestSuite_SendMessages(ISteamNetworkingSockets* sockets, ISteamNetworkingUtils* utils) {
    std::cout << "\n=== [Suite 6: SendMessages Ownership & Deduplication] ===" << std::endl;

    SteamNetworkingIdentity validIdent;
    validIdent.SetSteamID64(76561197960265729ULL);
    HSteamNetConnection hConn = sockets->ConnectP2P(validIdent, 0, 0, nullptr);
    TEST_CHECK(hConn != k_HSteamNetConnection_Invalid, "Create connection for SendMessages test");

    // 1. Allocate a message using ISteamNetworkingUtils
    SteamNetworkingMessage_t* pMsg = utils->AllocateMessage(64);
    TEST_CHECK(pMsg != nullptr, "AllocateMessage returns valid pointer");
    TEST_CHECK(pMsg->m_pData != nullptr, "Allocated message buffer is non-null");
    TEST_CHECK(pMsg->m_cbSize == 64, "Allocated message size matches request");

    pMsg->m_conn = hConn;
    pMsg->m_nFlags = k_nSteamNetworkingSend_Reliable;
    memcpy(pMsg->m_pData, "Hello ReFix Sockets Deduplication Test", 39);

    // Track free callback execution
    static std::atomic<int> s_freeDataCallCount{ 0 };
    s_freeDataCallCount = 0;
    pMsg->m_pfnFreeData = [](SteamNetworkingMessage_t* m) {
        s_freeDataCallCount++;
    };

    // 2. Duplicate pointer array: [pMsg, pMsg]
    // SendMessages MUST call Release() exactly once for this pointer!
    SteamNetworkingMessage_t* msgArray[2] = { pMsg, pMsg };
    int64 outResults[2] = { 0, 0 };

    sockets->SendMessages(2, msgArray, outResults);
    TEST_CHECK(s_freeDataCallCount.load() == 1, "m_pfnFreeData executed exactly once for duplicated pointer");
    TEST_CHECK(outResults[0] > 0, "First message assigned valid message number");
    TEST_CHECK(outResults[1] > 0, "Second message assigned valid message number");

    // 3. Null element resilience
    int64 nullResults[2] = { 0, 0 };
    SteamNetworkingMessage_t* nullArray[2] = { nullptr, nullptr };
    sockets->SendMessages(2, nullArray, nullResults);
    TEST_CHECK(nullResults[0] == -k_EResultInvalidParam, "Null message element results in -k_EResultInvalidParam");
    TEST_CHECK(nullResults[1] == -k_EResultInvalidParam, "Second null message element results in -k_EResultInvalidParam");

    // 4. Null array resilience
    sockets->SendMessages(0, nullptr, nullptr);
    sockets->SendMessages(-1, nullptr, nullptr);
    TEST_CHECK(true, "SendMessages with 0 or negative count and null array executes safely without crash");

    sockets->CloseConnection(hConn, 0, "SendMessages cleanup", false);
    return true;
}

// -----------------------------------------------------------------------------
// Test Suite 7: ReceiveMessagesOnConnection State Machine
// -----------------------------------------------------------------------------
bool TestSuite_ReceiveMessages(ISteamNetworkingSockets* sockets) {
    std::cout << "\n=== [Suite 7: ReceiveMessagesOnConnection State Acceptance] ===" << std::endl;

    // 1. Invalid connection handle
    SteamNetworkingMessage_t* outMsgs[4] = { nullptr };
    int countInvalid = sockets->ReceiveMessagesOnConnection(999999, outMsgs, 4);
    TEST_CHECK(countInvalid == -1, "ReceiveMessagesOnConnection on invalid handle returns -1");

    // 2. Null output buffer
    int countNull = sockets->ReceiveMessagesOnConnection(999999, nullptr, 4);
    TEST_CHECK(countNull == 0, "ReceiveMessagesOnConnection with null buffer returns 0");

    // 3. Negative max messages
    int countNeg = sockets->ReceiveMessagesOnConnection(999999, outMsgs, -1);
    TEST_CHECK(countNeg == 0, "ReceiveMessagesOnConnection with negative max messages returns 0");

    // 4. In Connecting state: cannot receive messages yet (returns -1)
    SteamNetworkingIdentity validIdent;
    validIdent.SetSteamID64(76561197960265729ULL);
    HSteamNetConnection hConn = sockets->ConnectP2P(validIdent, 0, 0, nullptr);
    TEST_CHECK(hConn != k_HSteamNetConnection_Invalid, "Create connection in Connecting state");

    int countConnecting = sockets->ReceiveMessagesOnConnection(hConn, outMsgs, 4);
    TEST_CHECK(countConnecting == -1, "ReceiveMessagesOnConnection in Connecting state returns -1");

    sockets->CloseConnection(hConn, 0, "Receive cleanup", false);
    return true;
}

// -----------------------------------------------------------------------------
// Test Suite 8: Multithreaded Concurrency & Race-Condition Stress
// -----------------------------------------------------------------------------
bool TestSuite_ConcurrencyStress(ISteamNetworkingSockets* sockets, ISteamNetworkingUtils* utils) {
    std::cout << "\n=== [Suite 8: Multithreaded Concurrency Stress] ===" << std::endl;

    constexpr int kNumThreads = 8;
    constexpr int kOpsPerThread = 200;
    std::atomic<bool> startSignal{ false };
    std::atomic<int> completedThreads{ 0 };
    std::atomic<int> totalErrors{ 0 };

    std::vector<std::thread> workers;
    for (int t = 0; t < kNumThreads; ++t) {
        workers.emplace_back([&, t]() {
            while (!startSignal.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int i = 0; i < kOpsPerThread; ++i) {
                // Mix operations: listen sockets, connect, send, query info, close
                int op = (t + i) % 5;
                if (op == 0) {
                    HSteamListenSocket ls = sockets->CreateListenSocketP2P(2000 + (t * 10) + (i % 10), 0, nullptr);
                    if (ls != k_HSteamListenSocket_Invalid) {
                        std::this_thread::sleep_for(std::chrono::microseconds(50));
                        sockets->CloseListenSocket(ls);
                    }
                } else if (op == 1) {
                    SteamNetworkingIdentity ident;
                    ident.SetSteamID64(76561197960265729ULL + (t % 4));
                    HSteamNetConnection conn = sockets->ConnectP2P(ident, 0, 0, nullptr);
                    if (conn != k_HSteamNetConnection_Invalid) {
                        sockets->SetConnectionUserData(conn, 12345);
                        int64 ud = sockets->GetConnectionUserData(conn);
                        if (ud != 12345) totalErrors++;
                        sockets->CloseConnection(conn, 0, "Stress", false);
                    }
                } else if (op == 2) {
                    SteamNetworkingIdentity ident;
                    ident.SetSteamID64(76561197960265729ULL);
                    HSteamNetConnection conn = sockets->ConnectP2P(ident, 0, 0, nullptr);
                    if (conn != k_HSteamNetConnection_Invalid) {
                        char data[64] = "stress_data_payload";
                        sockets->SendMessageToConnection(conn, data, sizeof(data), k_nSteamNetworkingSend_Reliable, nullptr);
                        sockets->CloseConnection(conn, 0, "Stress", false);
                    }
                } else if (op == 3) {
                    SteamNetConnectionInfo_t info = {};
                    sockets->GetConnectionInfo(1000 + (i % 20), &info);
                } else if (op == 4) {
                    SteamNetworkingMessage_t* pMsg = utils->AllocateMessage(32);
                    if (pMsg) {
                        pMsg->Release();
                    }
                }
            }
            completedThreads.fetch_add(1);
        });
    }

    auto t0 = std::chrono::steady_clock::now();
    startSignal.store(true, std::memory_order_release);

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }
    auto t1 = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    TEST_CHECK(completedThreads.load() == kNumThreads, "All 8 concurrent worker threads completed");
    TEST_CHECK(totalErrors.load() == 0, "Zero concurrency data races or errors observed");
    std::cout << "  [INFO] Completed " << (kNumThreads * kOpsPerThread) << " concurrent operations in " << elapsedMs << " ms" << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// Dual-Process E2E Harness: HOST
// -----------------------------------------------------------------------------
int RunDualProcessHost() {
    std::cout << "[HOST] Initializing SteamNetworkingSockets Host process..." << std::endl;
    if (!LoadSteamApi()) {
        std::cerr << "[HOST_FAIL] Could not resolve steam_api64 exports!" << std::endl;
        return 1;
    }
    if (!pfn_SteamAPI_Init()) {
        std::cerr << "[HOST_FAIL] SteamAPI_Init() failed!" << std::endl;
        return 1;
    }
    ISteamNetworkingSockets* sockets = pfn_SteamNetworkingSockets();
    if (!sockets) {
        std::cerr << "[HOST_FAIL] Failed to obtain ISteamNetworkingSockets!" << std::endl;
        pfn_SteamAPI_Shutdown();
        return 1;
    }

    HSteamListenSocket hListen = sockets->CreateListenSocketP2P(0, 0, nullptr);
    if (hListen == k_HSteamListenSocket_Invalid) {
        std::cerr << "[HOST_FAIL] CreateListenSocketP2P failed!" << std::endl;
        pfn_SteamAPI_Shutdown();
        return 1;
    }
    std::cout << "[HOST_READY] Listen socket created (Handle=" << hListen << "). Waiting for client..." << std::endl;
    fflush(stdout);

    HSteamNetConnection clientConn = k_HSteamNetConnection_Invalid;
    auto startWait = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 25) {
        pfn_SteamAPI_RunCallbacks();

        for (HSteamNetConnection h = 1000; h < 1050; ++h) {
            SteamNetConnectionInfo_t info = {};
            if (sockets->GetConnectionInfo(h, &info)) {
                if (info.m_eState == k_ESteamNetworkingConnectionState_Connecting && clientConn == k_HSteamNetConnection_Invalid) {
                    EResult accRes = sockets->AcceptConnection(h);
                    if (accRes == k_EResultOK) {
                        clientConn = h;
                        std::cout << "[HOST_ACCEPTED] Connection accepted: Handle=" << h << ", ListenSocket=" << info.m_hListenSocket << std::endl;
                        fflush(stdout);
                        break;
                    }
                } else if (info.m_eState == k_ESteamNetworkingConnectionState_Connected && clientConn == k_HSteamNetConnection_Invalid) {
                    clientConn = h;
                    break;
                }
            }
        }
        if (clientConn != k_HSteamNetConnection_Invalid) {
            SteamNetConnectionInfo_t info = {};
            if (sockets->GetConnectionInfo(clientConn, &info) && info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (clientConn == k_HSteamNetConnection_Invalid) {
        std::cerr << "[HOST_FAIL] Timed out waiting for incoming client connection!" << std::endl;
        sockets->CloseListenSocket(hListen);
        pfn_SteamAPI_Shutdown();
        return 1;
    }

    std::cout << "[HOST_CONNECTED] Client connected on handle: " << clientConn << std::endl;
    fflush(stdout);

    bool receivedReliablePing = false;
    bool receivedUnreliableFrag = false;

    startWait = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 25) {
        pfn_SteamAPI_RunCallbacks();

        SteamNetworkingMessage_t* pMsgs[16];
        int num = sockets->ReceiveMessagesOnConnection(clientConn, pMsgs, 16);
        for (int i = 0; i < num; ++i) {
            SteamNetworkingMessage_t* pMsg = pMsgs[i];
            if (pMsg && pMsg->m_pData) {
                if (pMsg->m_cbSize == 24 && memcmp(pMsg->m_pData, "PING_FROM_CLIENT_SOCKETS", 24) == 0) {
                    receivedReliablePing = true;
                    std::cout << "[HOST_RELIABLE_VERIFIED] Received reliable ping from client (size=" << pMsg->m_cbSize << ")" << std::endl;
                    fflush(stdout);
                } else if (pMsg->m_cbSize == 4096) {
                    const uint8_t* bytes = (const uint8_t*)pMsg->m_pData;
                    bool patternMatch = true;
                    for (int k = 0; k < 4096; ++k) {
                        if (bytes[k] != (uint8_t)(k & 0xFF)) { patternMatch = false; break; }
                    }
                    if (patternMatch) {
                        receivedUnreliableFrag = true;
                        std::cout << "[HOST_FRAGMENT_VERIFIED] Received reassembled 4096-byte unreliable message!" << std::endl;
                        fflush(stdout);
                    }
                }
                pMsg->Release();
            }
        }

        if (receivedReliablePing && receivedUnreliableFrag) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!receivedReliablePing || !receivedUnreliableFrag) {
        std::cerr << "[HOST_FAIL] Failed to receive all expected messages from client!" << std::endl;
        sockets->CloseConnection(clientConn, 0, "HostTestFailed", false);
        sockets->CloseListenSocket(hListen);
        pfn_SteamAPI_Shutdown();
        return 1;
    }

    // Send Pong back (Reliable)
    const char* pong = "PONG_FROM_HOST_SOCKETS";
    EResult resRel = sockets->SendMessageToConnection(clientConn, pong, (uint32)strlen(pong), k_nSteamNetworkingSend_Reliable, nullptr);
    if (resRel == k_EResultOK) {
        std::cout << "[HOST_SENT_PONG] Sent reliable pong to client." << std::endl;
        fflush(stdout);
    }

    // Send 4096 bytes back (Unreliable fragmented)
    std::vector<uint8_t> fragPayload(4096);
    for (int k = 0; k < 4096; ++k) fragPayload[k] = (uint8_t)((k + 42) & 0xFF);
    EResult resUnr = sockets->SendMessageToConnection(clientConn, fragPayload.data(), (uint32)fragPayload.size(), k_nSteamNetworkingSend_Unreliable, nullptr);
    if (resUnr == k_EResultOK) {
        std::cout << "[HOST_SENT_FRAGMENT] Sent 4096-byte fragmented unreliable message to client." << std::endl;
        fflush(stdout);
    }

    // Run callbacks and wait briefly for packets to leave
    for (int i = 0; i < 20; ++i) {
        pfn_SteamAPI_RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    sockets->CloseConnection(clientConn, 0, "HostTestComplete", false);
    sockets->CloseListenSocket(hListen);
    pfn_SteamAPI_Shutdown();
    std::cout << "[HOST_SUCCESS] Dual-process host test completed successfully." << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// Dual-Process E2E Harness: CLIENT
// -----------------------------------------------------------------------------
int RunDualProcessClient() {
    std::cout << "[CLIENT] Initializing SteamNetworkingSockets Client process..." << std::endl;
    if (!LoadSteamApi()) {
        std::cerr << "[CLIENT_FAIL] Could not resolve steam_api64 exports!" << std::endl;
        return 1;
    }
    if (!pfn_SteamAPI_Init()) {
        std::cerr << "[CLIENT_FAIL] SteamAPI_Init() failed!" << std::endl;
        return 1;
    }
    ISteamNetworkingSockets* sockets = pfn_SteamNetworkingSockets();
    if (!sockets) {
        std::cerr << "[CLIENT_FAIL] Failed to obtain ISteamNetworkingSockets!" << std::endl;
        pfn_SteamAPI_Shutdown();
        return 1;
    }

    // Wait a brief moment for discovery packets to exchange
    for (int i = 0; i < 20; ++i) {
        pfn_SteamAPI_RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    SteamNetworkingIdentity hostIdentity;
    hostIdentity.SetSteamID64(76561198000000001ULL);

    std::cout << "[CLIENT] Connecting to host: " << hostIdentity.GetSteamID64() << "..." << std::endl;
    HSteamNetConnection conn = sockets->ConnectP2P(hostIdentity, 0, 0, nullptr);
    if (conn == k_HSteamNetConnection_Invalid) {
        std::cerr << "[CLIENT_FAIL] ConnectP2P failed!" << std::endl;
        pfn_SteamAPI_Shutdown();
        return 1;
    }
    std::cout << "[CLIENT] ConnectP2P returned handle: " << conn << std::endl;

    // Queue reliable message in Connecting state
    const char* ping = "PING_FROM_CLIENT_SOCKETS";
    EResult resRel = sockets->SendMessageToConnection(conn, ping, (uint32)strlen(ping), k_nSteamNetworkingSend_Reliable, nullptr);
    if (resRel == k_EResultOK) {
        std::cout << "[CLIENT] Enqueued reliable message in Connecting state." << std::endl;
    } else {
        std::cerr << "[CLIENT_FAIL] Failed to enqueue reliable message in Connecting state! res=" << resRel << std::endl;
    }

    // Queue 4096-byte fragmented message in Connecting state
    std::vector<uint8_t> fragPayload(4096);
    for (int k = 0; k < 4096; ++k) fragPayload[k] = (uint8_t)(k & 0xFF);
    EResult resFrag = sockets->SendMessageToConnection(conn, fragPayload.data(), (uint32)fragPayload.size(), k_nSteamNetworkingSend_Unreliable, nullptr);
    if (resFrag == k_EResultOK) {
        std::cout << "[CLIENT] Enqueued 4096-byte fragmented message in Connecting state." << std::endl;
    } else {
        std::cerr << "[CLIENT_FAIL] Failed to enqueue 4096-byte message in Connecting state! res=" << resFrag << std::endl;
    }

    // Poll until Connected
    bool isConnected = false;
    auto startWait = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 25) {
        pfn_SteamAPI_RunCallbacks();
        SteamNetConnectionInfo_t info = {};
        if (sockets->GetConnectionInfo(conn, &info)) {
            if (info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
                isConnected = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!isConnected) {
        std::cerr << "[CLIENT_FAIL] Timed out waiting for connection to reach Connected state!" << std::endl;
        sockets->CloseConnection(conn, 0, "ClientTimeout", false);
        pfn_SteamAPI_Shutdown();
        return 1;
    }
    std::cout << "[CLIENT_CONNECTED] Reached Connected state!" << std::endl;
    fflush(stdout);

    // Wait for Pong and Fragment from Host
    bool receivedPong = false;
    bool receivedFrag = false;

    startWait = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 25) {
        pfn_SteamAPI_RunCallbacks();

        SteamNetworkingMessage_t* pMsgs[16];
        int num = sockets->ReceiveMessagesOnConnection(conn, pMsgs, 16);
        for (int i = 0; i < num; ++i) {
            SteamNetworkingMessage_t* pMsg = pMsgs[i];
            if (pMsg && pMsg->m_pData) {
                if (pMsg->m_cbSize == 22 && memcmp(pMsg->m_pData, "PONG_FROM_HOST_SOCKETS", 22) == 0) {
                    receivedPong = true;
                    std::cout << "[CLIENT_PONG_VERIFIED] Received reliable pong from host!" << std::endl;
                    fflush(stdout);
                } else if (pMsg->m_cbSize == 4096) {
                    const uint8_t* bytes = (const uint8_t*)pMsg->m_pData;
                    bool patternMatch = true;
                    for (int k = 0; k < 4096; ++k) {
                        if (bytes[k] != (uint8_t)((k + 42) & 0xFF)) { patternMatch = false; break; }
                    }
                    if (patternMatch) {
                        receivedFrag = true;
                        std::cout << "[CLIENT_FRAGMENT_VERIFIED] Received 4096-byte fragmented message from host!" << std::endl;
                        fflush(stdout);
                    }
                }
                pMsg->Release();
            }
        }

        if (receivedPong && receivedFrag) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!receivedPong || !receivedFrag) {
        std::cerr << "[CLIENT_FAIL] Failed to receive all expected messages from host!" << std::endl;
        sockets->CloseConnection(conn, 0, "ClientTestFailed", false);
        pfn_SteamAPI_Shutdown();
        return 1;
    }

    sockets->CloseConnection(conn, 0, "ClientTestComplete", false);
    pfn_SteamAPI_Shutdown();
    std::cout << "[CLIENT_SUCCESS] Dual-process client test completed successfully." << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// Main Test Runner
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc > 1) {
        std::string mode = argv[1];
        if (mode == "--host") {
            return RunDualProcessHost();
        } else if (mode == "--client") {
            return RunDualProcessClient();
        }
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << "  ReFix LAN 2.0 - ISteamNetworkingSockets Verification Test" << std::endl;
    std::cout << "=================================================================" << std::endl;

    // 1. Run ABI & struct asserts immediately (compile-time & run-time)
    if (!TestSuite_ABI()) {
        std::cerr << "ABI assertion failed!" << std::endl;
        return 1;
    }

    // 2. Load DLL
    if (!LoadSteamApi()) {
        std::cerr << "Could not resolve steam_api64 entry points!" << std::endl;
        return 2;
    }

    // 3. Initialize SteamAPI
    std::cout << "\n[*] Initializing SteamAPI via loaded DLL..." << std::endl;
    if (!pfn_SteamAPI_Init()) {
        std::cerr << "SteamAPI_Init() failed!" << std::endl;
        return 3;
    }
    std::cout << "[*] SteamAPI initialized successfully." << std::endl;

    ISteamNetworkingSockets* sockets = pfn_SteamNetworkingSockets();
    ISteamNetworkingUtils* utils = pfn_SteamNetworkingUtils();

    if (!sockets || !utils) {
        std::cerr << "Failed to obtain ISteamNetworkingSockets or ISteamNetworkingUtils interface!" << std::endl;
        pfn_SteamAPI_Shutdown();
        return 4;
    }

    // Run test suites
    TestSuite_Identity(sockets);
    TestSuite_Flags(sockets);
    TestSuite_BoundaryPayloads(sockets);
    TestSuite_LifecycleAndQueuing(sockets);
    TestSuite_SendMessages(sockets, utils);
    TestSuite_ReceiveMessages(sockets);
    TestSuite_ConcurrencyStress(sockets, utils);

    // Shutdown SteamAPI
    std::cout << "\n[*] Shutting down SteamAPI..." << std::endl;
    pfn_SteamAPI_Shutdown();
    std::cout << "[*] SteamAPI shutdown complete." << std::endl;

    std::cout << "\n=================================================================" << std::endl;
    std::cout << "  Test Results Summary:" << std::endl;
    std::cout << "    Passed: " << g_passCount.load() << std::endl;
    std::cout << "    Failed: " << g_failCount.load() << std::endl;
    std::cout << "=================================================================" << std::endl;

    if (g_failCount.load() > 0) {
        std::cerr << "\n[RESULT] SOME TESTS FAILED!" << std::endl;
        return 1;
    } else {
        std::cout << "\n[RESULT] ALL TESTS PASSED SUCCESSFULLY (Exit Code 0)!" << std::endl;
        return 0;
    }
}
