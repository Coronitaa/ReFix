#include "../src/eos/eos_types.h"
#include "../src/eos/eos_identity.h"
#include "../src/eos/eos_callbacks.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <atomic>

// Mock structures matching exact EOS SDK ABI
#pragma pack(push, 8)

struct EOS_Connect_Credentials {
    int32_t ApiVersion;
    const char* Token;
    int32_t Type;
};

struct EOS_Connect_LoginOptions {
    int32_t ApiVersion;
    const EOS_Connect_Credentials* Credentials;
    void* UserLoginInfo;
};

struct EOS_Connect_LoginCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
    EOS_ContinuanceToken ContinuanceToken;
};

struct EOS_Connect_CreateUserOptions {
    int32_t ApiVersion;
    EOS_ContinuanceToken ContinuanceToken;
};

struct EOS_Connect_CreateUserCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
};

struct EOS_Connect_QueryProductUserIdMappingsOptions {
    int32_t ApiVersion;
    EOS_ProductUserId LocalUserId;
    EOS_ProductUserId* ProductUserIds;
    uint32_t ProductUserIdCount;
};

struct EOS_Connect_QueryProductUserIdMappingsCallbackInfo {
    EOS_EResult ResultCode;
    void* ClientData;
    EOS_ProductUserId LocalUserId;
};

struct EOS_Connect_CopyProductUserInfoOptions {
    int32_t ApiVersion;
    EOS_ProductUserId TargetUserId;
};

struct EOS_Connect_ExternalAccountInfo {
    int32_t ApiVersion;
    EOS_ProductUserId ProductUserId;
    const char* DisplayName;
    int32_t AccountIdType;
    const char* AccountId;
    int64_t LastLoginTime;
};

#pragma pack(pop)

// Opaque ContinuanceToken Mock Struct
struct MockContinuanceToken {
    uint32_t magic; // 0x43544F4B ('CTOK')
    std::string tokenData;
};

// Simulated Connect Engine implementing exact ABI contract
class MockConnectEngine {
public:
    static MockConnectEngine& Get() {
        static MockConnectEngine s_inst;
        return s_inst;
    }

    void Login(const EOS_Connect_LoginOptions* options, void* clientData, EOS_CallbackFn callback, bool simulateNewUser = false) {
        assert(options != nullptr);
        assert(options->Credentials != nullptr);

        if (simulateNewUser) {
            auto* ctok = new MockContinuanceToken();
            ctok->magic = 0x43544F4B;
            ctok->tokenData = options->Credentials->Token ? options->Credentials->Token : "default_token";

            EOS_Connect_LoginCallbackInfo cbInfo = {};
            cbInfo.ResultCode = EOS_InvalidUser; // Trigger ContinuanceToken flow
            cbInfo.ClientData = clientData;
            cbInfo.LocalUserId = nullptr;
            cbInfo.ContinuanceToken = (EOS_ContinuanceToken)ctok;

            ReFixEOS::CallbackManager::Get().QueueCallback((void*)callback, cbInfo);
        } else {
            EOS_Connect_LoginCallbackInfo cbInfo = {};
            cbInfo.ResultCode = EOS_Success;
            cbInfo.ClientData = clientData;
            cbInfo.LocalUserId = ReFixEOS::IdentityManager::Get().GetLocalProductUserId();
            cbInfo.ContinuanceToken = nullptr;

            ReFixEOS::CallbackManager::Get().QueueCallback((void*)callback, cbInfo);
        }
    }

    void CreateUser(const EOS_Connect_CreateUserOptions* options, void* clientData, EOS_CallbackFn callback) {
        assert(options != nullptr);
        assert(options->ContinuanceToken != nullptr);

        auto* ctok = (MockContinuanceToken*)options->ContinuanceToken;
        assert(ctok->magic == 0x43544F4B);
        delete ctok;

        EOS_Connect_CreateUserCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_Success;
        cbInfo.ClientData = clientData;
        cbInfo.LocalUserId = ReFixEOS::IdentityManager::Get().GetLocalProductUserId();

        ReFixEOS::CallbackManager::Get().QueueCallback((void*)callback, cbInfo);
    }

    void QueryProductUserIdMappings(const EOS_Connect_QueryProductUserIdMappingsOptions* options, void* clientData, EOS_CallbackFn callback) {
        EOS_Connect_QueryProductUserIdMappingsCallbackInfo cbInfo = {};
        cbInfo.ResultCode = EOS_Success;
        cbInfo.ClientData = clientData;
        cbInfo.LocalUserId = ReFixEOS::IdentityManager::Get().GetLocalProductUserId();

        ReFixEOS::CallbackManager::Get().QueueCallback((void*)callback, cbInfo);
    }

    EOS_EResult CopyProductUserInfo(const EOS_Connect_CopyProductUserInfoOptions* options, EOS_Connect_ExternalAccountInfo** outInfo) {
        if (!options || !outInfo) return EOS_InvalidParameters;
        void* allocInfo = ReFixEOS::IdentityManager::Get().AllocateExternalAccountInfo(options->TargetUserId, 0, EOS_EAT_STEAM);
        if (!allocInfo) return EOS_NotFound;
        *outInfo = (EOS_Connect_ExternalAccountInfo*)allocInfo;
        return EOS_Success;
    }

    void ReleaseExternalAccountInfo(EOS_Connect_ExternalAccountInfo* info) {
        ReFixEOS::IdentityManager::Get().FreeExternalAccountInfo(info);
    }
};

// Test state tracking
static std::atomic<bool> s_loginCallbackFired = false;
static std::atomic<bool> s_createUserCallbackFired = false;
static std::atomic<bool> s_queryMappingsCallbackFired = false;
static EOS_ProductUserId s_receivedUserId = nullptr;
static EOS_ContinuanceToken s_receivedContinuanceToken = nullptr;

static void OnLoginComplete(const void* data) {
    const auto* info = (const EOS_Connect_LoginCallbackInfo*)data;
    assert(info != nullptr);
    s_loginCallbackFired = true;
    s_receivedUserId = info->LocalUserId;
    s_receivedContinuanceToken = info->ContinuanceToken;
}

static void OnCreateUserComplete(const void* data) {
    const auto* info = (const EOS_Connect_CreateUserCallbackInfo*)data;
    assert(info != nullptr);
    assert(info->ResultCode == EOS_Success);
    s_createUserCallbackFired = true;
    s_receivedUserId = info->LocalUserId;
}

static void OnQueryMappingsComplete(const void* data) {
    const auto* info = (const EOS_Connect_QueryProductUserIdMappingsCallbackInfo*)data;
    assert(info != nullptr);
    assert(info->ResultCode == EOS_Success);
    s_queryMappingsCallbackFired = true;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Connect Contract Mock Test Suite" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();
    auto& connectMock = MockConnectEngine::Get();

    // -------------------------------------------------------------------------
    // TEST 1: Direct Steam Ticket Login Flow
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 1] Steam Ticket Direct Login Flow..." << std::endl;
    EOS_Connect_Credentials creds = {};
    creds.ApiVersion = 1;
    creds.Type = EOS_ECT_STEAM_SESSION_TICKET;
    creds.Token = "140000002A3B4C5D6E7F8091A2B3C4D5"; // Mock hex ticket

    EOS_Connect_LoginOptions loginOpts = {};
    loginOpts.ApiVersion = 2;
    loginOpts.Credentials = &creds;
    loginOpts.UserLoginInfo = nullptr;

    s_loginCallbackFired = false;
    s_receivedUserId = nullptr;

    connectMock.Login(&loginOpts, (void*)0xCAFE, (EOS_CallbackFn)OnLoginComplete, false);

    // Verify deferred execution (no synchronous callback inside API)
    assert(!s_loginCallbackFired);
    std::cout << "  Async queuing verified (callback not called synchronously)." << std::endl;

    // Simulate EOS_Platform_Tick
    size_t flushed = cbMgr.FlushCallbacks();
    assert(flushed == 1);
    assert(s_loginCallbackFired);
    assert(s_receivedUserId == idMgr.GetLocalProductUserId());
    assert(EOS_ProductUserId_IsValid(s_receivedUserId) == 1);
    std::cout << "  Login callback executed on Tick with valid ProductUserId handle!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 2: ContinuanceToken -> CreateUser Flow (Unlinked First-Time User)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 2] ContinuanceToken & CreateUser Flow..." << std::endl;
    s_loginCallbackFired = false;
    s_receivedContinuanceToken = nullptr;

    connectMock.Login(&loginOpts, (void*)0xBEEF, (EOS_CallbackFn)OnLoginComplete, true);
    cbMgr.FlushCallbacks();
    assert(s_loginCallbackFired);
    assert(s_receivedContinuanceToken != nullptr);
    std::cout << "  EOS_InvalidUser returned with valid ContinuanceToken handle." << std::endl;

    // Call EOS_Connect_CreateUser with ContinuanceToken
    EOS_Connect_CreateUserOptions createOpts = {};
    createOpts.ApiVersion = 1;
    createOpts.ContinuanceToken = s_receivedContinuanceToken;

    s_createUserCallbackFired = false;
    connectMock.CreateUser(&createOpts, (void*)0xBEEF, (EOS_CallbackFn)OnCreateUserComplete);

    cbMgr.FlushCallbacks();
    assert(s_createUserCallbackFired);
    assert(s_receivedUserId == idMgr.GetLocalProductUserId());
    std::cout << "  CreateUser completed on Tick and resolved local ProductUserId!" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 3: User Mapping & ProductUserInfo Queries
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 3] QueryProductUserIdMappings & CopyProductUserInfo..." << std::endl;
    EOS_Connect_QueryProductUserIdMappingsOptions queryOpts = {};
    queryOpts.ApiVersion = 1;
    queryOpts.LocalUserId = s_receivedUserId;
    queryOpts.ProductUserIds = &s_receivedUserId;
    queryOpts.ProductUserIdCount = 1;

    s_queryMappingsCallbackFired = false;
    connectMock.QueryProductUserIdMappings(&queryOpts, nullptr, (EOS_CallbackFn)OnQueryMappingsComplete);
    cbMgr.FlushCallbacks();
    assert(s_queryMappingsCallbackFired);
    std::cout << "  QueryProductUserIdMappings completed on Tick." << std::endl;

    EOS_Connect_CopyProductUserInfoOptions copyOpts = {};
    copyOpts.ApiVersion = 1;
    copyOpts.TargetUserId = s_receivedUserId;

    EOS_Connect_ExternalAccountInfo* outInfo = nullptr;
    EOS_EResult copyRes = connectMock.CopyProductUserInfo(&copyOpts, &outInfo);
    assert(copyRes == EOS_Success);
    assert(outInfo != nullptr);
    assert(outInfo->ProductUserId == s_receivedUserId);
    assert(outInfo->AccountIdType == EOS_EAT_STEAM);
    assert(outInfo->AccountId != nullptr);
    assert(outInfo->DisplayName != nullptr);
    std::cout << "  CopyProductUserInfo verified: AccountId=" << outInfo->AccountId 
              << ", DisplayName=" << outInfo->DisplayName << std::endl;

    connectMock.ReleaseExternalAccountInfo(outInfo);
    std::cout << "  ExternalAccountInfo memory safely released!" << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Connect Contract Mock Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
