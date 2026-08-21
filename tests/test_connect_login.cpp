#include "../src/eos/eos_connect.h"
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

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();

    // TEST 1: Valid Steam Session Ticket Login
    std::cout << "\n[TEST 1] Valid Steam Session Ticket Login..." << std::endl;
    EOS_Connect_Credentials creds = {};
    creds.ApiVersion = 1;
    creds.Type = EOS_ECT_STEAM_SESSION_TICKET;
    creds.Token = "14000000AABBCCDDEEFF001122334455";

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

    // TEST 3: Invalid Credentials (Null options / Null credentials / Short token)
    std::cout << "\n[TEST 3] Invalid Credentials Validation..." << std::endl;
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, nullptr, (void*)0x3333, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled && s_loginResult == EOS_InvalidParameters);

    EOS_Connect_LoginOptions badOpts = {};
    badOpts.ApiVersion = 0; // Bad version
    badOpts.Credentials = nullptr;
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, &badOpts, (void*)0x4444, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled && s_loginResult == EOS_InvalidParameters);

    creds.Token = "ab"; // Too short
    s_loginCalled = false;
    EOS_Connect_Login(nullptr, &loginOpts, (void*)0x5555, (void*)OnLoginCallback);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled && s_loginResult == EOS_InvalidAuth);
    std::cout << "  Bad credentials safely rejected with EOS_InvalidParameters / EOS_InvalidAuth!" << std::endl;

    // TEST 4: Concurrent Thread Safety Login
    std::cout << "\n[TEST 4] Concurrent Multi-Threaded Login Queuing..." << std::endl;
    creds.Token = "14000000AABBCCDDEEFF001122334455";
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

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Connect Login Unit Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
