#include "../src/eos/eos_lobby.h"
#include "../src/eos/eos_identity.h"
#include "../src/eos/eos_callbacks.h"
#include "../src/eos_core/eos_room_manager.h"
#include <iostream>
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>

static std::atomic<bool> s_lobbyCreated = false;
static std::atomic<int32_t> s_createResult = -1;
static std::string s_createdLobbyId = "";
static void* s_receivedClientData = nullptr;

static void OnCreateLobbyCallback(const void* data) {
    const auto* info = (const EOS_Lobby_CreateLobbyCallbackInfo*)data;
    assert(info != nullptr);
    s_createResult = info->ResultCode;
    s_receivedClientData = info->ClientData;
    if (info->LobbyId) {
        s_createdLobbyId = info->LobbyId;
    }
    s_lobbyCreated = true;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Lobby Create Unit Test Suite" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();
    auto& roomBridge = ReFixEOS::RoomManagerBridge::Get();
    roomBridge.GetServerState().Reset();

    EOS_ProductUserId localPuid = idMgr.GetLocalProductUserId();
    std::string puidStr = idMgr.GetLocalProductUserIdString();

    // Authenticate local session in server state
    std::string token;
    roomBridge.GetServerState().AuthenticateSession(puidStr, idMgr.GetLocalDisplayName(), token);

    // -------------------------------------------------------------------------
    // TEST 1: Successful Creation & Deferred Callback Execution
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 1] Successful Lobby Creation & Asynchronous Queueing..." << std::endl;
    EOS_Lobby_CreateLobbyOptions createOpts = {};
    createOpts.ApiVersion = 8;
    createOpts.LocalUserId = localPuid;
    createOpts.MaxLobbyMembers = 4;
    createOpts.PermissionLevel = EOS_LPL_PUBLICADVERTISED;
    createOpts.bPresenceEnabled = 1;
    createOpts.BucketId = "Stage1_Normal";

    s_lobbyCreated = false;
    s_createResult = -1;
    s_createdLobbyId = "";
    s_receivedClientData = nullptr;

    EOS_Lobby_CreateLobby(nullptr, &createOpts, (void*)0xCAFE, (void*)OnCreateLobbyCallback);

    // Verify deferred execution (Callback must NOT be called synchronously)
    assert(!s_lobbyCreated);
    std::cout << "  [PASS] Async verification: Callback was not executed synchronously inside API." << std::endl;

    // Simulate EOS_Platform_Tick()
    size_t flushed = cbMgr.FlushCallbacks();
    assert(flushed == 1);
    assert(s_lobbyCreated);
    assert(s_createResult == EOS_Success);
    assert(s_receivedClientData == (void*)0xCAFE);
    assert(!s_createdLobbyId.empty());
    std::cout << "  [PASS] Callback executed on Tick with Result=EOS_Success, LobbyId=" << s_createdLobbyId << std::endl;

    // -------------------------------------------------------------------------
    // TEST 2: Creation with Attributes (STRING, INT64, DOUBLE, BOOLEAN)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 2] Attribute Normalization (STRING, INT64, DOUBLE, BOOLEAN)..." << std::endl;
    ::EOS_Lobby_AttributeDataValue strVal, intVal, dblVal, boolVal;
    strVal.AsUtf8 = "Deathmatch";
    intVal.AsInt64 = 100ULL;
    dblVal.AsDouble = 3.14159;
    boolVal.AsBool = 1;

    std::string nStr = ReFixEOS::NormalizeAttributeValue(EOS_AT_STRING, strVal);
    std::string nInt = ReFixEOS::NormalizeAttributeValue(EOS_AT_INT64, intVal);
    std::string nDbl = ReFixEOS::NormalizeAttributeValue(EOS_AT_DOUBLE, dblVal);
    std::string nBool = ReFixEOS::NormalizeAttributeValue(EOS_AT_BOOLEAN, boolVal);

    assert(nStr == "s:Deathmatch");
    assert(nInt == "i:100");
    assert(nDbl.rfind("d:3.14159", 0) == 0);
    assert(nBool == "b:1");
    std::cout << "  [PASS] All EOS attribute data types normalized into wire strings!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 3: Invalid Parameters & Invalid Capacity
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 3] Invalid Parameters & Boundary Rejections..." << std::endl;
    s_lobbyCreated = false;
    EOS_Lobby_CreateLobby(nullptr, nullptr, nullptr, (void*)OnCreateLobbyCallback);
    cbMgr.FlushCallbacks();
    assert(s_lobbyCreated && s_createResult == EOS_InvalidParameters);

    // Invalid LocalUserId (nullptr or 0xDEADBEEF)
    createOpts.LocalUserId = (EOS_ProductUserId)(uintptr_t)0xDEADBEEF;
    s_lobbyCreated = false;
    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, (void*)OnCreateLobbyCallback);
    cbMgr.FlushCallbacks();
    assert(s_lobbyCreated && s_createResult == EOS_InvalidUser);

    // Invalid Capacity (0 members or > 64)
    createOpts.LocalUserId = localPuid;
    createOpts.MaxLobbyMembers = 0;
    s_lobbyCreated = false;
    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, (void*)OnCreateLobbyCallback);
    cbMgr.FlushCallbacks();
    assert(s_lobbyCreated && s_createResult == EOS_InvalidParameters);

    createOpts.MaxLobbyMembers = 1000;
    s_lobbyCreated = false;
    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, (void*)OnCreateLobbyCallback);
    cbMgr.FlushCallbacks();
    assert(s_lobbyCreated && s_createResult == EOS_InvalidParameters);
    std::cout << "  [PASS] Bad pointers, unmapped PUIDs, and invalid capacities safely rejected!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 4: Two Users Creating Independent Lobbies Simultaneously
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 4] Concurrent Lobbies & Uniqueness..." << std::endl;
    std::string peerPuidStr = "99998888777766665555444433332222";
    EOS_ProductUserId peerPuid = idMgr.GetOrCreateProductUserId(peerPuidStr);
    std::string peerToken;
    roomBridge.GetServerState().AuthenticateSession(peerPuidStr, "Peer_Player", peerToken);

    createOpts.MaxLobbyMembers = 8;
    createOpts.LocalUserId = localPuid;
    std::string lobA = "";
    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, (void*)(+[](const void* d) {
        const auto* info = (const EOS_Lobby_CreateLobbyCallbackInfo*)d;
        assert(info->ResultCode == EOS_Success);
        s_createdLobbyId = info->LobbyId;
    }));
    cbMgr.FlushCallbacks();
    lobA = s_createdLobbyId;

    createOpts.LocalUserId = peerPuid;
    std::string lobB = "";
    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, (void*)(+[](const void* d) {
        const auto* info = (const EOS_Lobby_CreateLobbyCallbackInfo*)d;
        assert(info->ResultCode == EOS_Success);
        s_createdLobbyId = info->LobbyId;
    }));
    cbMgr.FlushCallbacks();
    lobB = s_createdLobbyId;

    assert(!lobA.empty() && !lobB.empty());
    assert(lobA != lobB);
    std::cout << "  [PASS] Distinct lobbies created: Lobby_A=" << lobA << ", Lobby_B=" << lobB << std::endl;

    // Verify backend authority on both lobbies
    ReFixOnline::LobbyData verifyA, verifyB;
    assert(roomBridge.GetServerState().ResyncLobby(puidStr, lobA, verifyA) == ReFixOnline::SUCCESS);
    assert(roomBridge.GetServerState().ResyncLobby(peerPuidStr, lobB, verifyB) == ReFixOnline::SUCCESS);
    assert(verifyA.ownerUserId == puidStr);
    assert(verifyB.ownerUserId == peerPuidStr);
    assert(verifyA.maxMembers == 8);
    assert(verifyB.maxMembers == 8);
    std::cout << "  [PASS] Backend authoritative ownership and capacity verified!" << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Lobby Create Unit Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
