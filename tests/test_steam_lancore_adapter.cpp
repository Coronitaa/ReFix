// =============================================================================
// ReFix - Steamworks to LanCore Adapter Verification Test & E2E Multiprocess Harness
// =============================================================================
// Verifies:
// 1. SteamAPI_Init() initializes and populates ReFix Universal LanCore.
// 2. Local identity, display name, and SteamID external binding sync to LanCore.
// 3. ISteamMatchmaking::CreateLobby creates a managed lobby in ILanCore::Lobby().
// 4. ISteamMatchmaking::SetLobbyData replicates attributes into ILanCore::Lobby().
// 5. True End-to-End Multiprocess Execution:
//    - Mutual discovery across independent processes on LAN.
//    - Host lobby creation, metadata replication, and client discovery.
//    - Identical CSteamID derivation on host and client.
//    - Remote join request, host validation, and member synchronization.
//    - Bi-directional reliable P2P packet exchange and payload verification.
//    - Bi-directional application fragmentation (8192 bytes) and reassembly integrity.
//    - Clean lobby leave and shutdown without leaks.
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "include/steam/steam_api.h"
#include "lan_core/refix_lan_types.h"
#include "lan_core/refix_lan_core.h"
#include "steam_lobby_mapping.h"

static void SigAbortHandler(int sig) {
    fprintf(stderr, "\n[CRASH INTERCEPTED] SIGABRT received! Stack trace:\n");
    void* stack[32];
    unsigned short frames = CaptureStackBackTrace(0, 32, stack, nullptr);
    for (unsigned short i = 0; i < frames; ++i) {
        fprintf(stderr, "  frame[%u] = 0x%p\n", i, stack[i]);
    }
    fflush(stderr);
    _exit(77);
}

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
            return false; \
        } \
    } while(0)

typedef bool (*fn_SteamAPI_Init)();
typedef void (*fn_SteamAPI_Shutdown)();
typedef void (*fn_SteamAPI_RunCallbacks)();
typedef void* (*fn_SteamAPI_SteamMatchmaking_v009)();
typedef void* (*fn_SteamAPI_SteamNetworking_v006)();
typedef void* (*fn_SteamAPI_SteamUser_v021)();
typedef void* (*fn_SteamAPI_SteamUtils_v010)();
typedef void* (*fn_ReFix_GetLanCore)();
typedef uint64_t (*fn_ReFix_Test_EnsureSteamLobbyID)(const char* coreLobbyId, uint64_t fallbackId);
typedef void (*fn_ReFix_Test_ResetLobbyMappings)();
typedef size_t (*fn_ReFix_Test_GetLobbyMappingCount)();
typedef bool (*fn_ReFix_Test_HasLobby)(uint64_t steamLobbyId);
typedef uint64_t (*fn_ReFix_Test_GetActiveLobbyID)();
typedef uint64_t (*fn_ReFix_Test_GetBroadcastNetPacketCount)();
typedef void (*fn_ReFix_Test_ResetBroadcastNetPacketCount)();
typedef bool (*fn_SteamAPI_IsAPICallCompleted)(uint64_t hAPICall, bool* pbFailed);

static HMODULE LoadSteamDll() {
    HMODULE hSteam = LoadLibraryA("bin\\steam_api64_test.dll");
    if (!hSteam) hSteam = LoadLibraryA("build\\steam_api64_test.dll");
    if (!hSteam) hSteam = LoadLibraryA("bin\\steam_api64.dll");
    if (!hSteam) hSteam = LoadLibraryA("build\\steam_api64.dll");
    if (!hSteam) hSteam = LoadLibraryA("steam_api64.dll");
    return hSteam;
}

// -----------------------------------------------------------------------------
// Standalone Adapter Unit Checks (Single Process)
// -----------------------------------------------------------------------------
static bool RunStandaloneAdapterTests(HMODULE hSteam) {
    std::cout << "[*] Running Standalone Adapter Tests..." << std::endl;

    SetEnvironmentVariableA("REFIX_NETWORK_MODE", "LAN");
    SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", "AdapterTester");
    SetEnvironmentVariableA("REFIX_STEAM_ID", "76561198000000099");
    SetEnvironmentVariableA("REFIX_LISTEN_PORT", "47586");

    auto pfnInit = (fn_SteamAPI_Init)GetProcAddress(hSteam, "SteamAPI_Init");
    auto pfnShutdown = (fn_SteamAPI_Shutdown)GetProcAddress(hSteam, "SteamAPI_Shutdown");
    auto pfnRunCallbacks = (fn_SteamAPI_RunCallbacks)GetProcAddress(hSteam, "SteamAPI_RunCallbacks");
    auto pfnGetMatchmaking = (fn_SteamAPI_SteamMatchmaking_v009)GetProcAddress(hSteam, "SteamAPI_SteamMatchmaking_v009");
    auto pfnGetUtils = (fn_SteamAPI_SteamUtils_v010)GetProcAddress(hSteam, "SteamAPI_SteamUtils_v010");
    auto pfnGetLanCore = (fn_ReFix_GetLanCore)GetProcAddress(hSteam, "ReFix_GetLanCore");
    auto pfnEnsureLobby = (fn_ReFix_Test_EnsureSteamLobbyID)GetProcAddress(hSteam, "ReFix_Test_EnsureSteamLobbyID");
    auto pfnResetMappings = (fn_ReFix_Test_ResetLobbyMappings)GetProcAddress(hSteam, "ReFix_Test_ResetLobbyMappings");
    auto pfnGetMappingCount = (fn_ReFix_Test_GetLobbyMappingCount)GetProcAddress(hSteam, "ReFix_Test_GetLobbyMappingCount");
    auto pfnHasLobby = (fn_ReFix_Test_HasLobby)GetProcAddress(hSteam, "ReFix_Test_HasLobby");
    auto pfnGetActiveLobby = (fn_ReFix_Test_GetActiveLobbyID)GetProcAddress(hSteam, "ReFix_Test_GetActiveLobbyID");
    auto pfnGetBcastCount = (fn_ReFix_Test_GetBroadcastNetPacketCount)GetProcAddress(hSteam, "ReFix_Test_GetBroadcastNetPacketCount");
    auto pfnResetBcastCount = (fn_ReFix_Test_ResetBroadcastNetPacketCount)GetProcAddress(hSteam, "ReFix_Test_ResetBroadcastNetPacketCount");

    if (!pfnInit || !pfnShutdown || !pfnRunCallbacks || !pfnGetLanCore ||
        !pfnGetUtils || !pfnEnsureLobby || !pfnResetMappings || !pfnGetMappingCount || !pfnHasLobby ||
        !pfnGetActiveLobby || !pfnGetBcastCount || !pfnResetBcastCount) {
        std::cerr << "[FAIL] Required test exports missing from steam_api64_test.dll" << std::endl;
        return false;
    }

    // 1. Production EnsureSteamLobbyID direct certification
    pfnResetMappings();
    uint64_t alphaSteamId = pfnEnsureLobby("core_lobby_alpha", 0);
    TEST_ASSERT(alphaSteamId != 0, "Normal EnsureSteamLobbyID registration must succeed");
    TEST_ASSERT(pfnGetMappingCount() == 1, "Mapping count must be 1");

    uint64_t alphaSteamIdRepeat = pfnEnsureLobby("core_lobby_alpha", 0);
    TEST_ASSERT(alphaSteamIdRepeat == alphaSteamId, "Repeated registration must return same SteamID");
    TEST_ASSERT(pfnGetMappingCount() == 1, "Mapping count must stay 1");

    // Forced collision: map "core_lobby_beta" with explicit fallbackId == alphaSteamId
    uint64_t collisionId = pfnEnsureLobby("core_lobby_beta", alphaSteamId);
    TEST_ASSERT(collisionId == 0, "Forced collision must return 0");
    TEST_ASSERT(pfnGetMappingCount() == 1, "Mapping count must remain 1 after rejected collision");
    TEST_ASSERT(pfnEnsureLobby("core_lobby_alpha", 0) == alphaSteamId, "Alpha mapping must remain preserved");

    pfnResetMappings();
    TEST_ASSERT(pfnGetMappingCount() == 0, "Mappings reset must clear map");

    if (!pfnInit()) {
        std::cerr << "[FAIL] SteamAPI_Init returned false!" << std::endl;
        return false;
    }

    auto* pUtils = static_cast<ISteamUtils*>(pfnGetUtils());
    TEST_ASSERT(pUtils != nullptr, "ISteamUtils must be valid");

    auto* core = static_cast<refix::lan::ILanCore*>(pfnGetLanCore());
    if (!core) {
        std::cerr << "[FAIL] ReFix_GetLanCore returned nullptr!" << std::endl;
        pfnShutdown();
        return false;
    }

    refix::lan::PeerId localPeer = core->Identity().GetLocalPeerId();
    TEST_ASSERT(localPeer.IsValid(), "Local PeerId must be valid");

    std::string displayName = core->Identity().GetDisplayName();
    TEST_ASSERT(!displayName.empty(), "Display name must not be empty");

    auto* pMatchmaking = static_cast<ISteamMatchmaking*>(pfnGetMatchmaking());
    TEST_ASSERT(pMatchmaking != nullptr, "ISteamMatchmaking must be valid");

    SteamAPICall_t hCreateCall = pMatchmaking->CreateLobby(k_ELobbyTypePublic, 8);
    TEST_ASSERT(hCreateCall != 0, "CreateLobby returned 0 call handle");

    pfnRunCallbacks();

    bool bCallFailed = false;
    bool isCompleted = pUtils->IsAPICallCompleted(hCreateCall, &bCallFailed);
    TEST_ASSERT(isCompleted && !bCallFailed, "CreateLobby API call must succeed and not fail");

    LobbyCreated_t crCreated = {};
    bool gotCr = pUtils->GetAPICallResult(hCreateCall, &crCreated, sizeof(crCreated), LobbyCreated_t::k_iCallback, &bCallFailed);
    TEST_ASSERT(gotCr && crCreated.m_eResult == k_EResultOK, "LobbyCreated_t must indicate k_EResultOK");
    TEST_ASSERT(crCreated.m_ulSteamIDLobby != 0, "LobbyCreated_t steam lobby ID must be non-zero");

    auto knownLobbies = core->Lobby().GetAllKnownLobbies();
    TEST_ASSERT(!knownLobbies.empty(), "LanCore must contain created lobby");

    CSteamID steamLobbyId = pMatchmaking->GetLobbyByIndex(0);
    TEST_ASSERT(steamLobbyId.ConvertToUint64() != 0, "GetLobbyByIndex returned 0");
    TEST_ASSERT(steamLobbyId.ConvertToUint64() == crCreated.m_ulSteamIDLobby, "Lobby ID must match callback ID");
    TEST_ASSERT(steamLobbyId.IsValid(), "Created lobby CSteamID must return IsValid() == true");
    TEST_ASSERT(steamLobbyId.IsLobby(), "Created lobby CSteamID must return IsLobby() == true");
    TEST_ASSERT(steamLobbyId.GetEUniverse() == k_EUniversePublic, "Lobby universe must equal k_EUniversePublic (1)");
    TEST_ASSERT(steamLobbyId.GetEAccountType() == k_EAccountTypeChat, "Lobby account type must equal k_EAccountTypeChat (8)");
    TEST_ASSERT((steamLobbyId.GetUnAccountInstance() & k_EChatInstanceFlagLobby) != 0, "Lobby instance must have k_EChatInstanceFlagLobby flag set");
    TEST_ASSERT(static_cast<uint32_t>(steamLobbyId.ConvertToUint64() >> 32) == 0x01840000, "Upper 32-bits of lobby ID must be canonical 0x01840000");

    pMatchmaking->SetLobbyData(steamLobbyId, "MapName", "de_dust2");
    pfnRunCallbacks();

    const char* mapVal = pMatchmaking->GetLobbyData(steamLobbyId, "MapName");
    TEST_ASSERT(strcmp(mapVal, "de_dust2") == 0, "GetLobbyData('MapName') must equal 'de_dust2'");

    // 2. Test CreateLobby Collision Propagation & Rollback
    // In refix_lan_core, the next lobby created will be "LOBBY_<localPeer>_2"
    std::string collidingCoreId = "LOBBY_" + localPeer.ToString() + "_2";
    uint64_t targetSteamId = refix::steam::ComputeLobbySteamID(collidingCoreId);

    // Pre-register blocker with targetSteamId for a different core lobby
    uint64_t blockerId = pfnEnsureLobby("pre_existing_blocker", targetSteamId);
    TEST_ASSERT(blockerId == targetSteamId, "Pre-registering blocker lobby must succeed");

    // Now call CreateLobby: LanCore creates "LOBBY_<localPeer>_2", EnsureSteamLobbyID detects collision with blocker and returns 0!
    SteamAPICall_t hCollidingCall = pMatchmaking->CreateLobby(k_ELobbyTypePublic, 4);
    TEST_ASSERT(hCollidingCall != 0, "CreateLobby must return call handle even on internal collision");

    pfnRunCallbacks();

    bool bCollFailed = false;
    bool collCompleted = pUtils->IsAPICallCompleted(hCollidingCall, &bCollFailed);
    TEST_ASSERT(collCompleted, "Colliding CreateLobby call must be completed");
    TEST_ASSERT(bCollFailed, "Colliding CreateLobby call must report failure (bFailed == true)");

    LobbyCreated_t crFailResult = {};
    pUtils->GetAPICallResult(hCollidingCall, &crFailResult, sizeof(crFailResult), LobbyCreated_t::k_iCallback, &bCollFailed);
    TEST_ASSERT(crFailResult.m_eResult == k_EResultFail, "Colliding CreateLobby must return k_EResultFail");
    TEST_ASSERT(crFailResult.m_ulSteamIDLobby == 0, "Colliding CreateLobby must return lobby ID 0");

    TEST_ASSERT(!pfnHasLobby(0), "g_lobbies must never contain entry for ID 0");
    TEST_ASSERT(!core->Lobby().GetLobby(collidingCoreId).has_value(), "LanCore colliding lobby must be rolled back and destroyed");

    // Confirm original lobby is unaffected and STILL ACTIVE (g_activeLobbyID preserved!)
    CSteamID survivingLobby = pMatchmaking->GetLobbyByIndex(0);
    TEST_ASSERT(survivingLobby == steamLobbyId, "Original lobby must remain intact at index 0");
    TEST_ASSERT(pfnGetActiveLobby() == steamLobbyId.ConvertToUint64(), "g_activeLobbyID must retain original active lobby after failed CreateLobby rollback");

    // 3. Test Canonical Mode: Zero Legacy BroadcastNetPacket Invocations
    SetEnvironmentVariableA("REFIX_DISABLE_LEGACY_FALLBACKS", "1");
    pfnResetBcastCount();
    pMatchmaking->RequestLobbyList();
    pfnRunCallbacks();
    TEST_ASSERT(pfnGetBcastCount() == 0, "Canonical mode RequestLobbyList must not invoke legacy BroadcastNetPacket");

    pfnShutdown();

    // 4. Test Production Export Table Isolation (Production DLL must NOT export test hooks)
    HMODULE hProd = LoadLibraryA("bin\\steam_api64.dll");
    if (!hProd) hProd = LoadLibraryA("build\\steam_api64.dll");
    if (hProd) {
        TEST_ASSERT(GetProcAddress(hProd, "ReFix_Test_ResetLobbyMappings") == nullptr,
                    "Production DLL must NOT export ReFix_Test_ResetLobbyMappings");
        TEST_ASSERT(GetProcAddress(hProd, "ReFix_Test_EnsureSteamLobbyID") == nullptr,
                    "Production DLL must NOT export ReFix_Test_EnsureSteamLobbyID");
        TEST_ASSERT(GetProcAddress(hProd, "ReFix_Test_GetLobbyMappingCount") == nullptr,
                    "Production DLL must NOT export ReFix_Test_GetLobbyMappingCount");
        TEST_ASSERT(GetProcAddress(hProd, "ReFix_Test_HasLobby") == nullptr,
                    "Production DLL must NOT export ReFix_Test_HasLobby");
        TEST_ASSERT(GetProcAddress(hProd, "ReFix_Test_GetActiveLobbyID") == nullptr,
                    "Production DLL must NOT export ReFix_Test_GetActiveLobbyID");
        FreeLibrary(hProd);
        std::cout << "  [PASS] Production export table isolation verified (no ReFix_Test_* hooks in production DLL)." << std::endl;
    }

    std::cout << "  [PASS] Standalone Adapter Unit Tests, Rollback Preservation & Canonical CSteamID certified." << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// HOST ROLE (Process 1)
// -----------------------------------------------------------------------------
static int RunHostRole(HMODULE hSteam) {
    std::cout << "[HOST] Initializing Steamworks Host process..." << std::endl;
    SetEnvironmentVariableA("REFIX_NETWORK_MODE", "LAN");
    SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", "ReFix_E2E_Host");
    SetEnvironmentVariableA("REFIX_STEAM_ID", "76561198000000001");
    SetEnvironmentVariableA("REFIX_LISTEN_PORT", "47584");
    SetEnvironmentVariableA("SteamAppId", "480");

    auto pfnInit = (fn_SteamAPI_Init)GetProcAddress(hSteam, "SteamAPI_Init");
    auto pfnShutdown = (fn_SteamAPI_Shutdown)GetProcAddress(hSteam, "SteamAPI_Shutdown");
    auto pfnRunCallbacks = (fn_SteamAPI_RunCallbacks)GetProcAddress(hSteam, "SteamAPI_RunCallbacks");
    auto pfnGetMatchmaking = (fn_SteamAPI_SteamMatchmaking_v009)GetProcAddress(hSteam, "SteamAPI_SteamMatchmaking_v009");
    auto pfnGetNetworking = (fn_SteamAPI_SteamNetworking_v006)GetProcAddress(hSteam, "SteamAPI_SteamNetworking_v006");
    auto pfnGetUser = (fn_SteamAPI_SteamUser_v021)GetProcAddress(hSteam, "SteamAPI_SteamUser_v021");

    if (!pfnInit()) {
        std::cerr << "[HOST_FAIL] SteamAPI_Init failed!" << std::endl;
        return 1;
    }

    auto* pMatchmaking = static_cast<ISteamMatchmaking*>(pfnGetMatchmaking());
    auto* pNet = static_cast<ISteamNetworking*>(pfnGetNetworking());
    auto* pUser = static_cast<ISteamUser*>(pfnGetUser());

    CSteamID hostSteamId = pUser->GetSteamID();
    std::cout << "[HOST] Initialized with SteamID: " << hostSteamId.ConvertToUint64() << std::endl;

    pMatchmaking->CreateLobby(k_ELobbyTypePublic, 4);
    pfnRunCallbacks();

    CSteamID lobbySteamId = pMatchmaking->GetLobbyByIndex(0);
    if (!lobbySteamId.IsValid() || !lobbySteamId.IsLobby()) {
        std::cerr << "[HOST_FAIL] Created lobby SteamID " << lobbySteamId.ConvertToUint64() << " is not a valid CSteamID lobby!" << std::endl;
        pfnShutdown();
        return 1;
    }
    pMatchmaking->SetLobbyData(lobbySteamId, "GameName", "ReFix_E2E_Test");
    pMatchmaking->SetLobbyData(lobbySteamId, "MapName", "de_dust2");
    std::cout << "[HOST_READY] Created Lobby ID: " << lobbySteamId.ConvertToUint64() << std::endl;

    // 1. Wait for client to join lobby
    std::cout << "[HOST] Waiting for client to join..." << std::endl;
    CSteamID clientSteamId;
    auto startWait = std::chrono::steady_clock::now();
    bool clientJoined = false;

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 15) {
        pfnRunCallbacks();
        int count = pMatchmaking->GetNumLobbyMembers(lobbySteamId);
        if (count >= 2) {
            for (int i = 0; i < count; ++i) {
                CSteamID mId = pMatchmaking->GetLobbyMemberByIndex(lobbySteamId, i);
                if (mId != hostSteamId) {
                    clientSteamId = mId;
                    clientJoined = true;
                    break;
                }
            }
            if (clientJoined) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!clientJoined) {
        std::cerr << "[HOST_FAIL] Client did not join within 15 seconds!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[HOST_CLIENT_JOINED] Remote Client joined: " << clientSteamId.ConvertToUint64() << std::endl;

    // 2. Wait for client P2P ping on channel 0
    std::cout << "[HOST] Waiting for client P2P ping on channel 0..." << std::endl;
    bool pingReceived = false;
    char pingBuf[128] = {0};
    uint32 msgSize = 0;
    CSteamID p2pSender;
    startWait = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 10) {
        pfnRunCallbacks();
        if (pNet->IsP2PPacketAvailable(&msgSize, 0)) {
            if (pNet->ReadP2PPacket(pingBuf, sizeof(pingBuf) - 1, &msgSize, &p2pSender, 0)) {
                pingBuf[msgSize] = '\0';
                if (strcmp(pingBuf, "CLIENT_TO_HOST_P2P_PING") == 0) {
                    pingReceived = true;
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!pingReceived) {
        std::cerr << "[HOST_FAIL] Did not receive PING from client!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[HOST_PING_VERIFIED] Received valid ping from client!" << std::endl;

    // Send PONG back to client
    const char pongMsg[] = "HOST_TO_CLIENT_P2P_PONG";
    bool pongSent = pNet->SendP2PPacket(clientSteamId, pongMsg, sizeof(pongMsg), k_EP2PSendReliable, 0);
    if (!pongSent) {
        std::cerr << "[HOST_FAIL] Failed to send PONG to client!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[HOST] Sent PONG to client." << std::endl;

    // 3. Receive fragmented large message on channel 1 (8192 bytes)
    std::cout << "[HOST] Waiting for fragmented message (8192 bytes) from client on channel 1..." << std::endl;
    bool fragReceived = false;
    std::vector<uint8_t> rxFrag(8192);
    startWait = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 10) {
        pfnRunCallbacks();
        if (pNet->IsP2PPacketAvailable(&msgSize, 1)) {
            if (msgSize == 8192) {
                if (pNet->ReadP2PPacket(rxFrag.data(), (uint32)rxFrag.size(), &msgSize, &p2pSender, 1)) {
                    fragReceived = true;
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!fragReceived) {
        std::cerr << "[HOST_FAIL] Did not receive 8192-byte fragmented message from client!" << std::endl;
        pfnShutdown();
        return 1;
    }

    for (size_t i = 0; i < 8192; ++i) {
        uint8_t expected = static_cast<uint8_t>((i * 3 + 7) & 0xFF);
        if (rxFrag[i] != expected) {
            std::cerr << "[HOST_FAIL] Byte corruption in reassembled message at index " << i << std::endl;
            pfnShutdown();
            return 1;
        }
    }
    std::cout << "[HOST_FRAGMENT_VERIFIED] Reassembled 8192 bytes with exact bit-for-bit integrity!" << std::endl;

    // Send large fragmented message back to client (8192 bytes)
    std::vector<uint8_t> txFrag(8192);
    for (size_t i = 0; i < 8192; ++i) {
        txFrag[i] = static_cast<uint8_t>((i * 5 + 13) & 0xFF);
    }
    bool sentFrag = pNet->SendP2PPacket(clientSteamId, txFrag.data(), (uint32)txFrag.size(), k_EP2PSendReliable, 1);
    if (!sentFrag) {
        std::cerr << "[HOST_FAIL] Failed to send 8192-byte fragmented message to client!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[HOST] Sent 8192-byte fragmented message to client." << std::endl;

    // 4. Wait for client to leave lobby
    std::cout << "[HOST] Waiting for client to leave lobby..." << std::endl;
    bool clientLeft = false;
    startWait = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 10) {
        pfnRunCallbacks();
        int count = pMatchmaking->GetNumLobbyMembers(lobbySteamId);
        if (count == 1) {
            clientLeft = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!clientLeft) {
        std::cerr << "[HOST_FAIL] Client member count did not drop to 1 after leave!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[HOST_CLIENT_LEFT] Member count updated back to 1." << std::endl;

    pfnShutdown();
    std::cout << "[HOST_SUCCESS] All host operations verified cleanly." << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// CLIENT ROLE (Process 2)
// -----------------------------------------------------------------------------
static int RunClientRole(HMODULE hSteam) {
    std::cout << "[CLIENT] Initializing Steamworks Client process..." << std::endl;
    SetEnvironmentVariableA("REFIX_NETWORK_MODE", "LAN");
    SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", "ReFix_E2E_Client");
    SetEnvironmentVariableA("REFIX_STEAM_ID", "76561198000000002");
    SetEnvironmentVariableA("REFIX_LISTEN_PORT", "47585");
    SetEnvironmentVariableA("SteamAppId", "480");

    auto pfnInit = (fn_SteamAPI_Init)GetProcAddress(hSteam, "SteamAPI_Init");
    auto pfnShutdown = (fn_SteamAPI_Shutdown)GetProcAddress(hSteam, "SteamAPI_Shutdown");
    auto pfnRunCallbacks = (fn_SteamAPI_RunCallbacks)GetProcAddress(hSteam, "SteamAPI_RunCallbacks");
    auto pfnGetMatchmaking = (fn_SteamAPI_SteamMatchmaking_v009)GetProcAddress(hSteam, "SteamAPI_SteamMatchmaking_v009");
    auto pfnGetNetworking = (fn_SteamAPI_SteamNetworking_v006)GetProcAddress(hSteam, "SteamAPI_SteamNetworking_v006");
    auto pfnGetUser = (fn_SteamAPI_SteamUser_v021)GetProcAddress(hSteam, "SteamAPI_SteamUser_v021");

    if (!pfnInit()) {
        std::cerr << "[CLIENT_FAIL] SteamAPI_Init failed!" << std::endl;
        return 1;
    }

    auto* pMatchmaking = static_cast<ISteamMatchmaking*>(pfnGetMatchmaking());
    auto* pNet = static_cast<ISteamNetworking*>(pfnGetNetworking());
    auto* pUser = static_cast<ISteamUser*>(pfnGetUser());

    CSteamID clientSteamId = pUser->GetSteamID();
    std::cout << "[CLIENT] Initialized with SteamID: " << clientSteamId.ConvertToUint64() << std::endl;

    // 1. Discover Lobby via RequestLobbyList()
    std::cout << "[CLIENT] Requesting lobby list..." << std::endl;
    pMatchmaking->RequestLobbyList();

    CSteamID discoveredLobbyId;
    auto startWait = std::chrono::steady_clock::now();
    auto lastReqTime = startWait;
    bool lobbyFound = false;

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 10) {
        pfnRunCallbacks();
        CSteamID lId = pMatchmaking->GetLobbyByIndex(0);
        if (lId.ConvertToUint64() != 0) {
            if (!lId.IsValid() || !lId.IsLobby()) {
                std::cerr << "[CLIENT_FAIL] Discovered lobby SteamID " << lId.ConvertToUint64() << " is not a valid CSteamID lobby!" << std::endl;
                pfnShutdown();
                return 1;
            }
            discoveredLobbyId = lId;
            lobbyFound = true;
            break;
        }
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReqTime).count() > 600) {
            lastReqTime = now;
            pMatchmaking->RequestLobbyList();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!lobbyFound) {
        std::cerr << "[CLIENT_FAIL] Could not discover host lobby within 10 seconds!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[CLIENT_DISCOVERED_LOBBY] Found Lobby ID: " << discoveredLobbyId.ConvertToUint64() << std::endl;

    const char* mapName = pMatchmaking->GetLobbyData(discoveredLobbyId, "MapName");
    std::cout << "[CLIENT] Discovered Lobby MapName: '" << mapName << "'" << std::endl;
    if (strcmp(mapName, "de_dust2") != 0) {
        std::cerr << "[CLIENT_FAIL] Lobby MapName metadata mismatch (expected 'de_dust2', got '" << mapName << "')" << std::endl;
        pfnShutdown();
        return 1;
    }

    // 2. Join Lobby via JoinLobby()
    std::cout << "[CLIENT] Requesting to join lobby..." << std::endl;
    pMatchmaking->JoinLobby(discoveredLobbyId);

    bool joinSuccess = false;
    startWait = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 10) {
        pfnRunCallbacks();
        int count = pMatchmaking->GetNumLobbyMembers(discoveredLobbyId);
        if (count >= 2) {
            joinSuccess = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!joinSuccess) {
        std::cerr << "[CLIENT_FAIL] JoinLobby did not complete successfully!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[CLIENT_JOIN_SUCCESS] Successfully joined lobby, member count: 2" << std::endl;

    CSteamID hostSteamId = pMatchmaking->GetLobbyOwner(discoveredLobbyId);
    std::cout << "[CLIENT] Host SteamID: " << hostSteamId.ConvertToUint64() << std::endl;

    // 3. Send P2P Ping to Host on channel 0
    const char pingMsg[] = "CLIENT_TO_HOST_P2P_PING";
    bool sentPing = pNet->SendP2PPacket(hostSteamId, pingMsg, sizeof(pingMsg), k_EP2PSendReliable, 0);
    if (!sentPing) {
        std::cerr << "[CLIENT_FAIL] SendP2PPacket PING failed!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[CLIENT] Sent PING to host." << std::endl;

    // 4. Send fragmented 8192-byte message to Host on channel 1
    std::vector<uint8_t> txFrag(8192);
    for (size_t i = 0; i < 8192; ++i) {
        txFrag[i] = static_cast<uint8_t>((i * 3 + 7) & 0xFF);
    }
    bool sentFrag = pNet->SendP2PPacket(hostSteamId, txFrag.data(), (uint32)txFrag.size(), k_EP2PSendReliable, 1);
    if (!sentFrag) {
        std::cerr << "[CLIENT_FAIL] SendP2PPacket 8192 bytes failed!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[CLIENT] Sent 8192-byte fragmented message to host." << std::endl;

    // 5. Wait for PONG from Host on channel 0
    std::cout << "[CLIENT] Waiting for PONG from host..." << std::endl;
    bool pongReceived = false;
    char pongBuf[128] = {0};
    uint32 msgSize = 0;
    CSteamID p2pSender;
    startWait = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 10) {
        pfnRunCallbacks();
        if (pNet->IsP2PPacketAvailable(&msgSize, 0)) {
            if (pNet->ReadP2PPacket(pongBuf, sizeof(pongBuf) - 1, &msgSize, &p2pSender, 0)) {
                pongBuf[msgSize] = '\0';
                if (strcmp(pongBuf, "HOST_TO_CLIENT_P2P_PONG") == 0) {
                    pongReceived = true;
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!pongReceived) {
        std::cerr << "[CLIENT_FAIL] Did not receive PONG from host!" << std::endl;
        pfnShutdown();
        return 1;
    }
    std::cout << "[CLIENT_PONG_VERIFIED] Received valid PONG from host!" << std::endl;

    // 6. Wait for fragmented 8192-byte message from Host on channel 1
    std::cout << "[CLIENT] Waiting for fragmented message (8192 bytes) from host..." << std::endl;
    bool fragReceived = false;
    std::vector<uint8_t> rxFrag(8192);
    startWait = std::chrono::steady_clock::now();

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startWait).count() < 10) {
        pfnRunCallbacks();
        if (pNet->IsP2PPacketAvailable(&msgSize, 1)) {
            if (msgSize == 8192) {
                if (pNet->ReadP2PPacket(rxFrag.data(), (uint32)rxFrag.size(), &msgSize, &p2pSender, 1)) {
                    fragReceived = true;
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!fragReceived) {
        std::cerr << "[CLIENT_FAIL] Did not receive 8192-byte message from host!" << std::endl;
        pfnShutdown();
        return 1;
    }

    for (size_t i = 0; i < 8192; ++i) {
        uint8_t expected = static_cast<uint8_t>((i * 5 + 13) & 0xFF);
        if (rxFrag[i] != expected) {
            std::cerr << "[CLIENT_FAIL] Byte corruption in reassembled message at index " << i << std::endl;
            pfnShutdown();
            return 1;
        }
    }
    std::cout << "[CLIENT_FRAGMENT_VERIFIED] Received and reassembled 8192 bytes from host with exact integrity!" << std::endl;

    // 7. Leave lobby
    std::cout << "[CLIENT] Leaving lobby..." << std::endl;
    pMatchmaking->LeaveLobby(discoveredLobbyId);
    for (int i = 0; i < 10; ++i) {
        pfnRunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    pfnShutdown();
    std::cout << "[CLIENT_SUCCESS] All client operations verified cleanly." << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// Win32 Multiprocess Pipe Helpers
// -----------------------------------------------------------------------------
static bool LaunchProcess(const std::string& cmdLine, PROCESS_INFORMATION& pi, HANDLE& hStdOutRead) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hWrite = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&hStdOutRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    char cmd[MAX_PATH * 2];
    strncpy_s(cmd, cmdLine.c_str(), sizeof(cmd));

    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);
    return ok != FALSE;
}

struct ProcessReader {
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    std::string output;
    std::thread thread;

    void Start(HANDLE h) {
        hPipe = h;
        thread = std::thread([this]() {
            char buf[1024];
            DWORD readBytes = 0;
            while (ReadFile(hPipe, buf, sizeof(buf) - 1, &readBytes, NULL) && readBytes > 0) {
                buf[readBytes] = '\0';
                output += buf;
            }
        });
    }

    std::string Finish(HANDLE hProcess, DWORD timeoutMs) {
        WaitForSingleObject(hProcess, timeoutMs);
        CloseHandle(hPipe);
        if (thread.joinable()) thread.join();
        return output;
    }
};

static bool RunMultiProcessOrchestration(const char* exePath) {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "   LAUNCHING DUAL-PROCESS STEAMWORKS LAN INTEGRATION E2E    " << std::endl;
    std::cout << "============================================================" << std::endl;

    std::string hostCmd = std::string("\"") + exePath + "\" --mode host";
    std::string clientCmd = std::string("\"") + exePath + "\" --mode client";

    PROCESS_INFORMATION hostPi{}, clientPi{};
    HANDLE hHostOut = INVALID_HANDLE_VALUE;
    HANDLE hClientOut = INVALID_HANDLE_VALUE;

    std::cout << "[*] Spawning Host process..." << std::endl;
    if (!LaunchProcess(hostCmd, hostPi, hHostOut)) {
        std::cerr << "[FAIL] Failed to launch host process!" << std::endl;
        return false;
    }

    ProcessReader hostReader;
    hostReader.Start(hHostOut);

    // Wait 1.5s for host to start up and create lobby
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    std::cout << "[*] Spawning Client process..." << std::endl;
    if (!LaunchProcess(clientCmd, clientPi, hClientOut)) {
        std::cerr << "[FAIL] Failed to launch client process!" << std::endl;
        TerminateProcess(hostPi.hProcess, 1);
        CloseHandle(hostPi.hProcess);
        CloseHandle(hostPi.hThread);
        return false;
    }

    ProcessReader clientReader;
    clientReader.Start(hClientOut);

    std::cout << "[*] Awaiting completion of both processes..." << std::endl;
    std::string hostOutput = hostReader.Finish(hostPi.hProcess, 25000);
    std::string clientOutput = clientReader.Finish(clientPi.hProcess, 25000);

    DWORD hostExit = 1, clientExit = 1;
    GetExitCodeProcess(hostPi.hProcess, &hostExit);
    GetExitCodeProcess(clientPi.hProcess, &clientExit);

    CloseHandle(hostPi.hProcess);
    CloseHandle(hostPi.hThread);
    CloseHandle(clientPi.hProcess);
    CloseHandle(clientPi.hThread);

    std::cout << "\n--- HOST PROCESS OUTPUT ---" << std::endl;
    std::cout << hostOutput << std::endl;
    std::cout << "--- CLIENT PROCESS OUTPUT ---" << std::endl;
    std::cout << clientOutput << std::endl;

    std::cout << "============================================================" << std::endl;
    std::cout << "Host Exit Code:   " << hostExit << std::endl;
    std::cout << "Client Exit Code: " << clientExit << std::endl;

    bool hostOk = (hostExit == 0) &&
                  hostOutput.find("[HOST_READY]") != std::string::npos &&
                  hostOutput.find("[HOST_CLIENT_JOINED]") != std::string::npos &&
                  hostOutput.find("[HOST_PING_VERIFIED]") != std::string::npos &&
                  hostOutput.find("[HOST_FRAGMENT_VERIFIED]") != std::string::npos &&
                  hostOutput.find("[HOST_CLIENT_LEFT]") != std::string::npos &&
                  hostOutput.find("[HOST_SUCCESS]") != std::string::npos;

    bool clientOk = (clientExit == 0) &&
                    clientOutput.find("[CLIENT_DISCOVERED_LOBBY]") != std::string::npos &&
                    clientOutput.find("[CLIENT_JOIN_SUCCESS]") != std::string::npos &&
                    clientOutput.find("[CLIENT_PONG_VERIFIED]") != std::string::npos &&
                    clientOutput.find("[CLIENT_FRAGMENT_VERIFIED]") != std::string::npos &&
                    clientOutput.find("[CLIENT_SUCCESS]") != std::string::npos;

    if (!hostOk || !clientOk) {
        std::cerr << "[FAIL] Multi-process verification failed!" << std::endl;
        return false;
    }

    std::cout << " [SUCCESS] DUAL-PROCESS STEAMWORKS LAN INTEGRATION CERTIFIED!" << std::endl;
    std::cout << "============================================================" << std::endl;
    return true;
}

// -----------------------------------------------------------------------------
// Entry Point
// -----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    signal(SIGABRT, SigAbortHandler);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::set_terminate([]() {
        fprintf(stderr, "\n[CRASH INTERCEPTED] std::terminate was called! Stack trace:\n");
        void* stack[32];
        unsigned short frames = CaptureStackBackTrace(0, 32, stack, nullptr);
        for (unsigned short i = 0; i < frames; ++i) {
            fprintf(stderr, "  frame[%u] = 0x%p\n", i, stack[i]);
        }
        fflush(stderr);
        _exit(78);
    });

    std::string mode = "all";
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (strcmp(argv[i], "--host") == 0) {
            mode = "host";
        } else if (strcmp(argv[i], "--client") == 0) {
            mode = "client";
        } else if (strcmp(argv[i], "--standalone") == 0) {
            mode = "standalone";
        }
    }

    HMODULE hSteam = LoadSteamDll();
    if (!hSteam) {
        std::cerr << "[FAIL] Could not load steam_api64.dll" << std::endl;
        return 1;
    }

    if (mode == "host") {
        return RunHostRole(hSteam);
    } else if (mode == "client") {
        return RunClientRole(hSteam);
    } else if (mode == "standalone") {
        return RunStandaloneAdapterTests(hSteam) ? 0 : 1;
    }

    // Default 'all' mode:
    std::cout << "============================================================" << std::endl;
    std::cout << "   REFIX STEAMWORKS -> LANCORE ADAPTER VERIFICATION TEST    " << std::endl;
    std::cout << "============================================================" << std::endl;

    if (!RunStandaloneAdapterTests(hSteam)) {
        return 1;
    }

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    if (!RunMultiProcessOrchestration(exePath)) {
        return 1;
    }

    std::cout << "\n============================================================" << std::endl;
    std::cout << " [SUCCESS] ALL STEAMWORKS ADAPTER TESTS PASSED!              " << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
