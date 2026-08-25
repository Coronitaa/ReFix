#include "../src/eos/eos_connect.h"
#include "../src/identity/online_identity_provider.h"
#include <iostream>
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>

static std::atomic<bool> s_loginCalled = false;
static std::atomic<int32_t> s_loginResult = -1;
static EOS_ProductUserId s_receivedUserId = nullptr;

static void OnLoginCallback(const void* data) {
    const auto* info = (const EOS_Connect_LoginCallbackInfo*)data;
    assert(info != nullptr);
    s_loginResult = info->ResultCode;
    s_receivedUserId = info->LocalUserId;
    s_loginCalled = true;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Connect Login Unit Test" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto provider = ReFixIdentity::GetActiveIdentityProvider();
    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();

    // Setup fixture: capture a 64-byte ticket for testing
    std::vector<uint8_t> fixtureTicket(64, 0xAB);
    fixtureTicket[0] = 0x14; // gc_len
    provider->SetCapturedSteamTicket(fixtureTicket.data(), fixtureTicket.size(), 1001);
    std::string validTicketHex = provider->GetCapturedTicketHex();

    // TEST 1: Valid Steam Session Ticket Login (Exact Captured Ticket)
    std::cout << "\n[TEST 1] Valid Steam Session Ticket Login (Exact Match)..." << std::endl;
    EOS_Connect_Credentials creds = {};
    creds.ApiVersion = 1;
    creds.Type = EOS_ECT_STEAM_SESSION_TICKET;
    creds.Token = validTicketHex.c_str();

    EOS_Connect_LoginOptions loginOpts = {};
    loginOpts.ApiVersion = 2;
    loginOpts.Credentials = &creds;
    loginOpts.UserLoginInfo = nullptr;

    s_loginCalled = false;
    EOS_Connect_Login(nullptr, &loginOpts, (void*)0x1111, (void*)OnLoginCallback);

    assert(!s_loginCalled); // Strictly async
    size_t flushed = cbMgr.FlushCallbacks();
    assert(flushed == 1);
    assert(s_loginCalled);
    assert(s_loginResult == EOS_Success);
    assert(s_receivedUserId == idMgr.GetLocalProductUserId());
    assert(EOS_ProductUserId_IsValid(s_receivedUserId) == 1);
    std::cout << "  Steam login successful -> Persistent PUID resolved!" << std::endl;

    // TEST 2: Repeated Login Idempotence
    std::cout << "\n[TEST 2] Repeated Login Idempotence..." << std::endl;
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, &loginOpts, (void*)0x2222, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled);
    assert(s_loginResult == EOS_Success);
    assert(s_receivedUserId == idMgr.GetLocalProductUserId()); // Must match
    std::cout << "  Repeated login returned identical ProductUserId handle!" << std::endl;

    // TEST 3: Arbitrary / Random / Fake Token REJECTION
    std::cout << "\n[TEST 3] Arbitrary / Random Token Rejection..." << std::endl;
    creds.Token = "DEADBEEF0123456789ABCDEF"; // Random 24-char hex not matching captured ticket
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, &loginOpts, (void*)0x3333, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled && s_loginResult == EOS_InvalidAuth);
    std::cout << "  [PASS] Arbitrary random token strictly rejected with EOS_InvalidAuth!" << std::endl;

    // TEST 4: Invalid Parameters (Null options / Null credentials / Short token)
    std::cout << "\n[TEST 4] Invalid Parameters Validation..." << std::endl;
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, nullptr, (void*)0x4444, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled && s_loginResult == EOS_InvalidParameters);

    EOS_Connect_LoginOptions badOpts = {};
    badOpts.ApiVersion = 0; // Bad version
    badOpts.Credentials = nullptr;
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, &badOpts, (void*)0x5555, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled && s_loginResult == EOS_InvalidParameters);

    creds.Token = "ab"; // Invalid length
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, &loginOpts, (void*)0x6666, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled && s_loginResult == EOS_InvalidAuth);
    std::cout << "  Bad parameters safely rejected with EOS_InvalidParameters / EOS_InvalidAuth!" << std::endl;

    // TEST 5: Concurrent Multi-Threaded Login Queuing with Valid Ticket
    std::cout << "\n[TEST 5] Concurrent Multi-Threaded Login Queuing..." << std::endl;
    creds.Token = validTicketHex.c_str();
    constexpr int NUM_THREADS = 4;
    std::vector<std::thread> threads;
    std::atomic<int> completedLogins = 0;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&loginOpts, &completedLogins]() {
            for (int j = 0; j < 50; ++j) {
                EOS_Connect_Login(nullptr, &loginOpts, nullptr, (void*)(+[](const void* d) {}));
                completedLogins++;
            }
        });
    }
    for (auto& th : threads) th.join();
    flushed = cbMgr.FlushCallbacks();
    assert(flushed == 200);
    std::cout << "  200 concurrent logins enqueued and drained safely!" << std::endl;

    // TEST 6: Ticket Invalidation (CancelAuthTicket) strictly causes EOS_InvalidAuth
    std::cout << "\n[TEST 6] Ticket Invalidation (CancelAuthTicket)..." << std::endl;
    provider->InvalidateCapturedTicket(1001);
    assert(!provider->HasCapturedTicket());
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, &loginOpts, (void*)0x7777, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled);
    assert(s_loginResult == EOS_InvalidAuth);
    std::cout << "  [PASS] Invalidated/canceled ticket rejected with EOS_InvalidAuth!" << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Connect Login Unit Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
