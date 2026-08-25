#include "../src/eos/eos_lobby.h"
#include "../src/eos/eos_identity.h"
#include "../src/eos/eos_callbacks.h"
#include "../src/eos_core/eos_room_manager.h"
#include "../src/refix_online/refix_backend_client.h"
#include "../src/refix_online/refix_backend_state.h"
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
    roomBridge.Reset();

    EOS_ProductUserId localPuid = idMgr.GetLocalProductUserId();
    std::string puidStr = idMgr.GetLocalProductUserIdString();

    // -------------------------------------------------------------------------
    // TEST A: Unit Test del BackendClient (via InProcessDirectTransport)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST A] BackendClient Protocol Lifecycle via IRefixTransport..." << std::endl;
    ReFixOnline::BackendServerState testServer;
    auto testTransport = std::make_unique<ReFixOnline::InProcessDirectTransport>(testServer);
    auto testClient = std::make_unique<ReFixOnline::BackendClient>(std::move(testTransport));

    assert(testClient->Connect("127.0.0.1", 47584));
    assert(testClient->GetConnectionState() == ReFixOnline::EClientConnectionState::CONNECTED);

    bool authComplete = false;
    testClient->Authenticate("test_user_a", "Test Player A", [&authComplete](ReFixOnline::EBackendResult res, const std::string& token) {
        assert(res == ReFixOnline::SUCCESS);
        assert(!token.empty());
        authComplete = true;
    });
    testClient->Tick();
    assert(authComplete);
    assert(testClient->GetConnectionState() == ReFixOnline::EClientConnectionState::AUTHENTICATED);

    bool clientCreateDone = false;
    std::string clientCreatedLobbyId;
    testClient->CreateLobby(4, { {"mode", "deathmatch"} }, [&clientCreateDone, &clientCreatedLobbyId](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::SUCCESS);
        assert(!lob.lobbyId.empty());
        assert(lob.maxMembers == 4);
        assert(lob.ownerUserId == "test_user_a");
        clientCreatedLobbyId = lob.lobbyId;
        clientCreateDone = true;
    });
    testClient->Tick();
    assert(clientCreateDone);
    assert(!clientCreatedLobbyId.empty());
    std::cout << "  [PASS] BackendClient MSG_CREATE_LOBBY -> MSG_CREATE_LOBBY_RESULT verified on transport." << std::endl;

    // -------------------------------------------------------------------------
    // TEST B: Routing Test (RoomManagerBridge uses BackendClient, not direct State)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST B] RoomManagerBridge Routing through BackendClient..." << std::endl;
    roomBridge.Reset();
    roomBridge.Authenticate(puidStr, idMgr.GetLocalDisplayName());

    assert(roomBridge.GetClient() != nullptr);
    assert(roomBridge.GetClient()->GetConnectionState() == ReFixOnline::EClientConnectionState::AUTHENTICATED);

    bool bridgeCreateDone = false;
    std::string bridgeLobbyId;
    roomBridge.CreateLobby(6, { {"map", "temple"} }, [&bridgeCreateDone, &bridgeLobbyId](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::SUCCESS);
        bridgeLobbyId = lob.lobbyId;
        bridgeCreateDone = true;
    });

    // Verify it is asynchronous: callback has NOT run yet because Tick() has not executed
    assert(!bridgeCreateDone);
    roomBridge.Tick();
    assert(bridgeCreateDone);
    assert(!bridgeLobbyId.empty());
    std::cout << "  [PASS] RoomManagerBridge::CreateLobby delegates asynchronously through BackendClient." << std::endl;

    // -------------------------------------------------------------------------
    // TEST C: Request/Response Correlation with Multiple Concurrent Requests
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST C] Request/Response Correlation with Concurrent Requests..." << std::endl;
    bool req1Done = false, req2Done = false;
    std::string lob1Id, lob2Id;

    testClient->CreateLobby(2, { {"room", "one"} }, [&req1Done, &lob1Id](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::SUCCESS);
        assert(lob.maxMembers == 2);
        lob1Id = lob.lobbyId;
        req1Done = true;
    });

    testClient->CreateLobby(8, { {"room", "two"} }, [&req2Done, &lob2Id](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::SUCCESS);
        assert(lob.maxMembers == 8);
        lob2Id = lob.lobbyId;
        req2Done = true;
    });

    testClient->Tick();
    assert(req1Done && req2Done);
    assert(!lob1Id.empty() && !lob2Id.empty() && lob1Id != lob2Id);
    std::cout << "  [PASS] RequestIds correctly correlated with independent responses (Lobby1=" << lob1Id << ", Lobby2=" << lob2Id << ")" << std::endl;

    // -------------------------------------------------------------------------
    // TEST D: Backend Authentication Ordering (No Silent Auto-Registration Bypass)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST D] Backend Authentication Ordering..." << std::endl;
    ReFixOnline::BackendServerState strictServer;
    auto strictTransport = std::make_unique<ReFixOnline::InProcessDirectTransport>(strictServer);
    auto unauthClient = std::make_unique<ReFixOnline::BackendClient>(std::move(strictTransport));
    unauthClient->Connect("127.0.0.1", 47584);

    // 1. Attempt CreateLobby BEFORE authentication
    bool unauthFailed = false;
    unauthClient->CreateLobby(4, {}, [&unauthFailed](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::NOT_AUTHENTICATED);
        unauthFailed = true;
    });
    unauthClient->Tick();
    assert(unauthFailed);
    std::cout << "  [PASS] CreateLobby before authentication strictly rejected with NOT_AUTHENTICATED (no auto-registration)." << std::endl;

    // 2. Authenticate and retry CreateLobby
    bool authOk = false;
    unauthClient->Authenticate("strict_user", "Strict Player", [&authOk](ReFixOnline::EBackendResult res, const std::string& token) {
        assert(res == ReFixOnline::SUCCESS);
        authOk = true;
    });
    unauthClient->Tick();
    assert(authOk);

    bool createAfterAuth = false;
    unauthClient->CreateLobby(4, {}, [&createAfterAuth](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::SUCCESS);
        createAfterAuth = true;
    });
    unauthClient->Tick();
    assert(createAfterAuth);
    std::cout << "  [PASS] CreateLobby after authentication succeeded!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST E: EOS Deferred Callback Chain
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST E] Full EOS Deferred Callback Pipeline..." << std::endl;
    roomBridge.Reset();
    roomBridge.Authenticate(puidStr, idMgr.GetLocalDisplayName());

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

    // 1. Callback MUST NOT be executed synchronously inside API
    assert(!s_lobbyCreated);
    std::cout << "  [PASS] Step 1: Callback not executed synchronously inside EOS_Lobby_CreateLobby." << std::endl;

    // 2. Tick RoomBridge (network/client tick) -> queues EOS callback, does not dispatch to completion delegate yet
    roomBridge.Tick();
    assert(!s_lobbyCreated);
    std::cout << "  [PASS] Step 2: Backend response processed and queued into CallbackManager without early execution." << std::endl;

    // 3. FlushCallbacks (EOS_Platform_Tick) -> dispatches completion delegate
    size_t flushed = cbMgr.FlushCallbacks();
    assert(flushed == 1);
    assert(s_lobbyCreated);
    assert(s_createResult == EOS_Success);
    assert(s_receivedClientData == (void*)0xCAFE);
    assert(!s_createdLobbyId.empty());
    std::cout << "  [PASS] Step 3: Callback safely dispatched on FlushCallbacks with LobbyId=" << s_createdLobbyId << std::endl;

    // -------------------------------------------------------------------------
    // TEST F: Malformed & Truncated Packet Handling
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST F] Malformed & Truncated Packet Handling..." << std::endl;
    class MockMalformedTransport : public ReFixOnline::IRefixTransport {
    public:
        bool Connect(const std::string& e, uint16_t p) override { return true; }
        void Disconnect() override {}
        bool Send(const uint8_t* d, size_t s) override { return true; }
        bool Receive(std::vector<uint8_t>& out) override {
            if (!m_queue.empty()) {
                out = m_queue.front();
                m_queue.erase(m_queue.begin());
                return true;
            }
            return false;
        }
        bool IsConnected() const override { return true; }
        std::vector<std::vector<uint8_t>> m_queue;
    };

    auto malTransport = std::make_unique<MockMalformedTransport>();
    auto* rawMal = malTransport.get();
    auto malClient = std::make_unique<ReFixOnline::BackendClient>(std::move(malTransport));
    malClient->Connect("127.0.0.1", 47584);

    // 1. Packet smaller than header size
    rawMal->m_queue.push_back({ 0x52, 0x46 }); // 2 bytes only
    malClient->Tick(); // Should not crash

    // 2. Corrupted packet magic
    std::vector<uint8_t> badMagic(sizeof(ReFixOnline::RefixPacketHeader), 0xFF);
    rawMal->m_queue.push_back(badMagic);
    malClient->Tick(); // Should not crash

    // 3. Truncated payload
    ReFixOnline::RefixPacketHeader fakeHeader = {};
    fakeHeader.Magic = ReFixOnline::REFIX_PROTOCOL_MAGIC;
    fakeHeader.Version = ReFixOnline::REFIX_PROTOCOL_VERSION;
    fakeHeader.MessageType = ReFixOnline::MSG_CREATE_LOBBY_RESULT;
    fakeHeader.RequestId = 999;
    fakeHeader.PayloadLength = 100; // Claims 100 bytes but we provide 0 payload bytes
    ReFixOnline::ByteWriter bw;
    ReFixOnline::SerializeHeader(fakeHeader, bw);
    rawMal->m_queue.push_back(bw.GetBuffer());
    malClient->Tick(); // Should gracefully handle truncated payload

    std::cout << "  [PASS] Malformed and truncated packets safely rejected without crashes or lockups." << std::endl;

    // -------------------------------------------------------------------------
    // TEST G: Invalid Parameters & Boundary Rejections
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST G] Invalid Parameters & Boundary Rejections..." << std::endl;
    // Options == nullptr
    s_lobbyCreated = false;
    EOS_Lobby_CreateLobby(nullptr, nullptr, nullptr, (void*)OnCreateLobbyCallback);
    cbMgr.FlushCallbacks();
    assert(s_lobbyCreated && s_createResult == EOS_InvalidParameters);

    // CompletionDelegate == nullptr (must abort gracefully without crash or queueing)
    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, nullptr);

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

    // BucketId too long (> 1024 chars)
    std::string longBucket(2048, 'X');
    createOpts.MaxLobbyMembers = 4;
    createOpts.BucketId = longBucket.c_str();
    s_lobbyCreated = false;
    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, (void*)OnCreateLobbyCallback);
    cbMgr.FlushCallbacks();
    assert(s_lobbyCreated && s_createResult == EOS_InvalidParameters);
    createOpts.BucketId = "Stage1_Normal";

    std::cout << "  [PASS] Bad pointers, null delegates, unmapped PUIDs, and invalid boundaries safely rejected!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST H: Attribute Normalization & Concurrent Independent Lobbies
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST H] Attribute Normalization & Concurrent Independent Lobbies..." << std::endl;
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

    std::string peerPuidStr = "99998888777766665555444433332222";
    createOpts.MaxLobbyMembers = 8;
    createOpts.LocalUserId = localPuid;
    std::string lobA = "";
    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, (void*)(+[](const void* d) {
        const auto* info = (const EOS_Lobby_CreateLobbyCallbackInfo*)d;
        assert(info->ResultCode == EOS_Success);
        s_createdLobbyId = info->LobbyId;
    }));
    roomBridge.Tick();
    cbMgr.FlushCallbacks();
    lobA = s_createdLobbyId;

    // Independent Client B creates Lobby B on same authoritative server state
    auto peerTransport = std::make_unique<ReFixOnline::InProcessDirectTransport>(roomBridge.GetServerState());
    auto peerClient = std::make_unique<ReFixOnline::BackendClient>(std::move(peerTransport));
    peerClient->Connect("127.0.0.1", 47584);
    peerClient->Authenticate(peerPuidStr, "Peer_Player", nullptr);
    peerClient->Tick();

    std::string lobB = "";
    peerClient->CreateLobby(8, { {"bucket_id", "s:Stage1_Normal"} }, [&lobB](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::SUCCESS);
        lobB = lob.lobbyId;
    });
    peerClient->Tick();

    assert(!lobA.empty() && !lobB.empty());
    assert(lobA != lobB);
    std::cout << "  [PASS] Distinct lobbies created: Lobby_A=" << lobA << ", Lobby_B=" << lobB << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Lobby Create Unit Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
