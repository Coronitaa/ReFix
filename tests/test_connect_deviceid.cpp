#include "../src/eos/eos_connect.h"
#include <iostream>
#include <cassert>
#include <atomic>

static std::atomic<bool> s_createDeviceIdCalled = false;
static std::atomic<bool> s_loginCalled = false;
static EOS_ProductUserId s_receivedUserId = nullptr;

static void OnCreateDeviceId(const void* data) {
    const auto* info = (const EOS_Connect_CreateDeviceIdCallbackInfo*)data;
    assert(info != nullptr);
    assert(info->ResultCode == EOS_Success);
    s_createDeviceIdCalled = true;
}

static void OnLogin(const void* data) {
    const auto* info = (const EOS_Connect_LoginCallbackInfo*)data;
    assert(info != nullptr);
    assert(info->ResultCode == EOS_Success);
    s_receivedUserId = info->LocalUserId;
    s_loginCalled = true;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Connect DeviceId Unit Test" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();

    // TEST 1: CreateDeviceId
    std::cout << "\n[TEST 1] CreateDeviceId Call..." << std::endl;
    EOS_Connect_CreateDeviceIdOptions devOpts = {};
    devOpts.ApiVersion = 1;
    devOpts.DeviceModel = "ReFix_PC_Client";

    EOS_Connect_CreateDeviceId(nullptr, &devOpts, nullptr, (void*)OnCreateDeviceId);
    cbMgr.FlushCallbacks();
    assert(s_createDeviceIdCalled);
    std::cout << "  CreateDeviceId completed successfully on Tick." << std::endl;

    // TEST 2: Login with DeviceId Credentials
    std::cout << "\n[TEST 2] Login with DeviceId Access Token..." << std::endl;
    EOS_Connect_Credentials creds = {};
    creds.ApiVersion = 1;
    creds.Type = EOS_ECT_DEVICEID_ACCESS_TOKEN;
    creds.Token = "mock_device_access_token_8888";

    EOS_Connect_LoginOptions loginOpts = {};
    loginOpts.ApiVersion = 2;
    loginOpts.Credentials = &creds;

    EOS_Connect_Login(nullptr, &loginOpts, nullptr, (void*)OnLogin);
    cbMgr.FlushCallbacks();
    assert(s_loginCalled);
    assert(s_receivedUserId == idMgr.GetLocalProductUserId());
    assert(EOS_ProductUserId_IsValid(s_receivedUserId) == 1);
    std::cout << "  DeviceId login successful -> Persistent PUID resolved!" << std::endl;

    // TEST 3: DeleteDeviceId
    std::cout << "\n[TEST 3] DeleteDeviceId..." << std::endl;
    bool deleteCalled = false;
    EOS_Connect_DeleteDeviceId(nullptr, nullptr, nullptr, (void*)(+[](const void* d) {
        const auto* info = (const EOS_Connect_DeleteDeviceIdCallbackInfo*)d;
        assert(info != nullptr && info->ResultCode == EOS_Success);
    }));
    size_t flushed = cbMgr.FlushCallbacks();
    assert(flushed == 1);
    std::cout << "  DeleteDeviceId completed successfully." << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Connect DeviceId Unit Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
