#include "../src/eos/eos_callbacks.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <atomic>

struct TestPayload {
    int32_t resultCode;
    void* clientData;
    uint64_t customId;
};

static std::atomic<int> s_callbackExecutionCount = 0;
static std::atomic<uint64_t> s_lastReceivedCustomId = 0;

static void TestCallbackHandler(const void* data) {
    const auto* payload = (const TestPayload*)data;
    assert(payload != nullptr);
    assert(payload->resultCode == 0);
    s_lastReceivedCustomId = payload->customId;
    s_callbackExecutionCount++;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Callback Dispatcher Unit Test" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();

    // 1. Test Simple Queue & Flush
    TestPayload p1 = { 0, (void*)0x1234, 42 };
    cbMgr.QueueCallback((void*)TestCallbackHandler, p1);

    assert(s_callbackExecutionCount == 0); // Not executed until flush (Tick)
    size_t flushed = cbMgr.FlushCallbacks();
    assert(flushed == 1);
    assert(s_callbackExecutionCount == 1);
    assert(s_lastReceivedCustomId == 42);
    std::cout << "[*] Single callback queue and deferred execution verified!" << std::endl;

    // 2. Test Multi-threaded Producer Queueing
    s_callbackExecutionCount = 0;
    constexpr int NUM_THREADS = 4;
    constexpr int ITEMS_PER_THREAD = 250;

    std::vector<std::thread> workers;
    for (int t = 0; t < NUM_THREADS; ++t) {
        workers.emplace_back([&cbMgr, t, ITEMS_PER_THREAD]() {
            for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                TestPayload p = { 0, nullptr, (uint64_t)(t * 1000 + i) };
                cbMgr.QueueCallback((void*)TestCallbackHandler, p);
            }
        });
    }

    for (auto& th : workers) th.join();

    // Flush all 1000 queued items on the main tick thread
    flushed = cbMgr.FlushCallbacks();
    assert(flushed == 1000);
    assert(s_callbackExecutionCount == 1000);
    std::cout << "[*] Multi-threaded concurrent callback queueing (1000 items) verified!" << std::endl;

    // 3. Test Notification Subscriptions
    s_callbackExecutionCount = 0;
    constexpr int EVENT_ROOM_JOIN = 101;

    EOS_NotificationId notifId = cbMgr.AddNotification(EVENT_ROOM_JOIN, (void*)0x5678, (void*)TestCallbackHandler);
    assert(notifId != EOS_INVALID_NOTIFICATIONID);

    TestPayload notifPayload = { 0, (void*)0x5678, 9999 };
    cbMgr.DispatchNotification(EVENT_ROOM_JOIN, notifPayload);

    // Flushed on Tick
    flushed = cbMgr.FlushCallbacks();
    assert(flushed == 1);
    assert(s_callbackExecutionCount == 1);
    assert(s_lastReceivedCustomId == 9999);

    // Unregister notification
    cbMgr.RemoveNotification(notifId);
    cbMgr.DispatchNotification(EVENT_ROOM_JOIN, notifPayload);
    flushed = cbMgr.FlushCallbacks();
    assert(flushed == 0); // No callback queued after removal
    std::cout << "[*] Notification registration, dispatch, and removal verified!" << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] Callback Dispatcher Unit Tests Passed 100%!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
