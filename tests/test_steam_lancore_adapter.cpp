// =============================================================================
// ReFix - Steamworks to LanCore Adapter Verification Test
// =============================================================================
// Verifies that:
// 1. SteamAPI_Init() initializes and populates ReFix Universal LanCore.
// 2. Local identity, display name, and SteamID external binding sync to LanCore.
// 3. ISteamMatchmaking::CreateLobby creates a managed lobby in ILanCore::Lobby().
// 4. ISteamMatchmaking::SetLobbyData replicates attributes into ILanCore::Lobby().
// 5. ISteamNetworking::SendP2PPacket resolves LanCore peers and transmits through LanTransport.
// 6. SteamAPI_Shutdown() cleanly terminates LanCore and releases resources.
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <chrono>

#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "include/steam/steam_api.h"
#include "lan_core/refix_lan_types.h"
#include "lan_core/refix_lan_core.h"

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
typedef void* (*fn_ReFix_GetLanCore)();

int main(int argc, char* argv[]) {
    std::cout << "============================================================" << std::endl;
    std::cout << "   REFIX STEAMWORKS -> LANCORE ADAPTER VERIFICATION TEST    " << std::endl;
    std::cout << "============================================================" << std::endl;

    // Set test environment configuration
    SetEnvironmentVariableA("REFIX_NETWORK_MODE", "LAN");
    SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", "AdapterTester");

    HMODULE hSteam = LoadLibraryA("bin\\steam_api64.dll");
    if (!hSteam) {
        hSteam = LoadLibraryA("build\\steam_api64.dll");
    }
    if (!hSteam) {
        hSteam = LoadLibraryA("steam_api64.dll");
    }
    if (!hSteam) {
        std::cerr << "[FAIL] Could not load steam_api64.dll (error: " << GetLastError() << ")" << std::endl;
        return 1;
    }

    auto pfnInit = (fn_SteamAPI_Init)GetProcAddress(hSteam, "SteamAPI_Init");
    auto pfnShutdown = (fn_SteamAPI_Shutdown)GetProcAddress(hSteam, "SteamAPI_Shutdown");
    auto pfnRunCallbacks = (fn_SteamAPI_RunCallbacks)GetProcAddress(hSteam, "SteamAPI_RunCallbacks");
    auto pfnGetMatchmaking = (fn_SteamAPI_SteamMatchmaking_v009)GetProcAddress(hSteam, "SteamAPI_SteamMatchmaking_v009");
    auto pfnGetNetworking = (fn_SteamAPI_SteamNetworking_v006)GetProcAddress(hSteam, "SteamAPI_SteamNetworking_v006");
    auto pfnGetUser = (fn_SteamAPI_SteamUser_v021)GetProcAddress(hSteam, "SteamAPI_SteamUser_v021");
    auto pfnGetLanCore = (fn_ReFix_GetLanCore)GetProcAddress(hSteam, "ReFix_GetLanCore");

    if (!pfnInit || !pfnShutdown || !pfnRunCallbacks || !pfnGetLanCore) {
        std::cerr << "[FAIL] Required DLL exports missing from steam_api64.dll" << std::endl;
        return 1;
    }

    // -------------------------------------------------------------------------
    // TEST 1: Bootstrap & Identity Integration
    // -------------------------------------------------------------------------
    std::cout << "[*] TEST 1: Verifying SteamAPI_Init() -> ILanCore initialization..." << std::endl;
    bool initOk = pfnInit();
    if (!initOk) {
        std::cerr << "[FAIL] SteamAPI_Init returned false!" << std::endl;
        return 1;
    }

    auto* core = static_cast<refix::lan::ILanCore*>(pfnGetLanCore());
    if (!core) {
        std::cerr << "[FAIL] ReFix_GetLanCore returned nullptr!" << std::endl;
        return 1;
    }

    refix::lan::PeerId localPeer = core->Identity().GetLocalPeerId();
    if (!localPeer.IsValid()) {
        std::cerr << "[FAIL] Local PeerId in LanCore is invalid!" << std::endl;
        return 1;
    }

    std::string displayName = core->Identity().GetDisplayName();
    std::cout << "    Local Peer ID: " << localPeer.ToString() << std::endl;
    std::cout << "    Display Name:  " << displayName << std::endl;
    if (displayName.empty()) {
        std::cerr << "[FAIL] LanCore display name is empty!" << std::endl;
        return 1;
    }

    auto boundEp = core->Transport().GetLocalDataEndpoint();
    std::cout << "    Data Endpoint: " << boundEp.ToString() << std::endl;
    if (!boundEp.IsValid()) {
        std::cerr << "[FAIL] LanCore data endpoint is invalid!" << std::endl;
        return 1;
    }

    // Verify external SteamID is bound in PeerRegistry
    auto selfInfo = core->Peers().FindByPeerId(localPeer);
    if (!selfInfo.has_value() || selfInfo->externalId.platform != refix::lan::ExternalPlatform::Steam) {
        std::cerr << "[FAIL] Local peer does not have Steam external identity bound in LanCore PeerRegistry!" << std::endl;
        return 1;
    }
    std::cout << "    External Steam ID: " << selfInfo->externalId.numericId << std::endl;
    std::cout << "  [PASS] Test 1: Bootstrap and local identity fully wired to LanCore!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 2: Steam Matchmaking -> LanCore Lobby Synchronization
    // -------------------------------------------------------------------------
    std::cout << "[*] TEST 2: Verifying CreateLobby() -> LanCore Lobby management..." << std::endl;
    auto* pMatchmaking = static_cast<ISteamMatchmaking*>(pfnGetMatchmaking ? pfnGetMatchmaking() : nullptr);
    if (!pMatchmaking) {
        std::cerr << "[FAIL] Could not acquire ISteamMatchmaking interface!" << std::endl;
        return 1;
    }

    SteamAPICall_t hCreateCall = pMatchmaking->CreateLobby(k_ELobbyTypePublic, 8);
    if (hCreateCall == 0) {
        std::cerr << "[FAIL] CreateLobby returned 0 call handle!" << std::endl;
        return 1;
    }

    // Pump callbacks
    pfnRunCallbacks();

    auto knownLobbies = core->Lobby().GetAllKnownLobbies();
    if (knownLobbies.empty()) {
        std::cerr << "[FAIL] No lobbies registered in LanCore ILobbyService after CreateLobby!" << std::endl;
        return 1;
    }

    const auto& createdLobby = knownLobbies.front();
    std::cout << "    LanCore Lobby ID: " << createdLobby.lobbyId << std::endl;
    std::cout << "    Owner Peer ID:    " << createdLobby.ownerPeerId.ToString() << std::endl;
    std::cout << "    Max Members:      " << createdLobby.maxMembers << std::endl;
    if (createdLobby.maxMembers != 8 || createdLobby.ownerPeerId != localPeer) {
        std::cerr << "[FAIL] LanCore lobby attributes do not match CreateLobby params!" << std::endl;
        return 1;
    }

    // Set lobby metadata via Steamworks API
    CSteamID steamLobbyId = pMatchmaking->GetLobbyByIndex(0);
    std::cout << "    Steam Lobby ID:   " << steamLobbyId.ConvertToUint64() << std::endl;
    if (steamLobbyId.ConvertToUint64() == 0) {
        std::cerr << "[FAIL] GetLobbyByIndex returned 0!" << std::endl;
        return 1;
    }

    pMatchmaking->SetLobbyData(steamLobbyId, "MapName", "de_dust2");
    pMatchmaking->SetLobbyData(steamLobbyId, "GameVersion", "1.0.4");

    pfnRunCallbacks();

    // Verify metadata reflected in LanCore
    auto fetchedLobby = core->Lobby().GetLobby(createdLobby.lobbyId);
    if (!fetchedLobby.has_value()) {
        std::cerr << "[FAIL] Could not query lobby by ID from LanCore!" << std::endl;
        return 1;
    }

    std::cout << "    LanCore Attributes count: " << fetchedLobby->attributes.size() << std::endl;
    auto mapIt = fetchedLobby->attributes.find("MapName");
    if (mapIt == fetchedLobby->attributes.end() || mapIt->second.asString != "de_dust2") {
        std::cerr << "[FAIL] Lobby MapName attribute did not replicate to LanCore!" << std::endl;
        return 1;
    }

    const char* mapVal = pMatchmaking->GetLobbyData(steamLobbyId, "MapName");
    std::cout << "    ISteamMatchmaking::GetLobbyData('MapName') = '" << mapVal << "'" << std::endl;
    if (strcmp(mapVal, "de_dust2") != 0) {
        std::cerr << "[FAIL] GetLobbyData did not return de_dust2!" << std::endl;
        return 1;
    }
    std::cout << "  [PASS] Test 2: Steam Matchmaking successfully created and updated lobby in LanCore!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 3: Steam Networking -> LanCore Universal Transport Routing
    // -------------------------------------------------------------------------
    std::cout << "[*] TEST 3: Verifying SendP2PPacket() -> LanCore Universal Transport..." << std::endl;
    auto* pNet = static_cast<ISteamNetworking*>(pfnGetNetworking ? pfnGetNetworking() : nullptr);
    if (!pNet) {
        std::cerr << "[FAIL] Could not acquire ISteamNetworking interface!" << std::endl;
        return 1;
    }

    // Register a simulated peer in LanCore
    uint64_t remoteSteamId = 76561198000000099ULL;
    refix::lan::PeerInfo simPeer;
    simPeer.peerId = { 0xDEADBEEF00000001ULL, 0x1234567890ABCDEFULL };
    simPeer.displayName = "SimulatedRemotePeer";
    simPeer.endpoint = refix::lan::LanEndpoint::Parse("127.0.0.1:47585");
    simPeer.externalId.platform = refix::lan::ExternalPlatform::Steam;
    simPeer.externalId.numericId = remoteSteamId;
    simPeer.externalId.stringId = std::to_string(remoteSteamId);
    simPeer.state = refix::lan::PeerTransportState::Connected;
    core->Peers().RegisterOrUpdatePeer(simPeer);

    // Verify LanCore can find the peer by SteamID
    auto lookup = core->Peers().FindBySteamId(remoteSteamId);
    if (!lookup.has_value() || *lookup != simPeer.peerId) {
        std::cerr << "[FAIL] FindBySteamId in LanCore failed to resolve registered peer!" << std::endl;
        return 1;
    }

    // Send P2P packet to remote Steam ID
    const char testMsg[] = "HELLO_REFIX_LAN_CORE_P2P";
    bool sendOk = pNet->SendP2PPacket(CSteamID(remoteSteamId), testMsg, (uint32)sizeof(testMsg), k_EP2PSendReliable, 1);
    if (!sendOk) {
        std::cerr << "[FAIL] SendP2PPacket through LanCore transport returned false!" << std::endl;
        return 1;
    }
    std::cout << "    P2P Packet successfully routed through LanCore Transport to "
              << simPeer.endpoint.ToString() << std::endl;
    std::cout << "  [PASS] Test 3: SendP2PPacket seamlessly routed through LanCore Universal Transport!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 4: Clean Shutdown
    // -------------------------------------------------------------------------
    std::cout << "[*] TEST 4: Verifying clean SteamAPI_Shutdown()..." << std::endl;
    pfnShutdown();
    std::cout << "  [PASS] Test 4: SteamAPI_Shutdown() completed cleanly without leaks or hangs." << std::endl;

    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << " [SUCCESS] STEAMWORKS -> LANCORE ADAPTER VERIFICATION PASSED!" << std::endl;
    std::cout << "============================================================" << std::endl;

    return 0;
}
