#include "../src/eos/eos_connect.h"
#include <iostream>
#include <cassert>
#include <cstring>

static bool s_queryMappingsCalled = false;

static void OnQueryMappings(const void* data) {
    const auto* info = (const EOS_Connect_QueryProductUserIdMappingsCallbackInfo*)data;
    assert(info != nullptr && info->ResultCode == EOS_Success);
    s_queryMappingsCalled = true;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - External Account Mappings Test" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();

    EOS_ProductUserId localPuid = idMgr.GetLocalProductUserId();

    // TEST 1: QueryProductUserIdMappings
    std::cout << "\n[TEST 1] QueryProductUserIdMappings..." << std::endl;
    EOS_Connect_QueryProductUserIdMappingsOptions qOpts = {};
    qOpts.ApiVersion = 2;
    qOpts.LocalUserId = localPuid;
    qOpts.ProductUserIds = &localPuid;
    qOpts.ProductUserIdCount = 1;

    EOS_Connect_QueryProductUserIdMappings(nullptr, &qOpts, nullptr, (void*)OnQueryMappings);
    cbMgr.FlushCallbacks();
    assert(s_queryMappingsCalled);
    std::cout << "  QueryProductUserIdMappings completed on Tick." << std::endl;

    // TEST 2: CopyProductUserInfo
    std::cout << "\n[TEST 2] CopyProductUserInfo..." << std::endl;
    EOS_Connect_CopyProductUserInfoOptions copyOpts = {};
    copyOpts.ApiVersion = 1;
    copyOpts.TargetUserId = localPuid;

    EOS_Connect_ExternalAccountInfo* info = nullptr;
    EOS_EResult res = EOS_Connect_CopyProductUserInfo(nullptr, &copyOpts, &info);
    assert(res == EOS_Success);
    assert(info != nullptr);
    assert(info->ProductUserId == localPuid);
    assert(info->AccountIdType == EOS_EAT_STEAM);
    assert(info->AccountId != nullptr);
    assert(info->DisplayName != nullptr);
    assert(strcmp(info->DisplayName, idMgr.GetLocalDisplayName().c_str()) == 0);

    std::cout << "  DisplayName: " << info->DisplayName << std::endl;
    std::cout << "  AccountId:   " << info->AccountId << std::endl;
    std::cout << "  AccountType: " << info->AccountIdType << " (EOS_EAT_STEAM)" << std::endl;

    EOS_Connect_ExternalAccountInfo_Release(info);
    std::cout << "  ExternalAccountInfo safely released." << std::endl;

    // TEST 3: External Account Index & Type queries
    std::cout << "\n[TEST 3] CopyProductUserExternalAccountByIndex & ByType..." << std::endl;
    EOS_Connect_CopyProductUserExternalAccountByIndexOptions byIndexOpts = {};
    byIndexOpts.ApiVersion = 1;
    byIndexOpts.TargetUserId = localPuid;
    byIndexOpts.ExternalAccountInfoIndex = 0;

    EOS_Connect_ExternalAccountInfo* indexInfo = nullptr;
    res = EOS_Connect_CopyProductUserExternalAccountByIndex(nullptr, &byIndexOpts, &indexInfo);
    assert(res == EOS_Success && indexInfo != nullptr);
    EOS_Connect_ExternalAccountInfo_Release(indexInfo);

    EOS_Connect_CopyProductUserExternalAccountByAccountTypeOptions byTypeOpts = {};
    byTypeOpts.ApiVersion = 1;
    byTypeOpts.TargetUserId = localPuid;
    byTypeOpts.AccountIdType = EOS_EAT_STEAM;

    EOS_Connect_ExternalAccountInfo* typeInfo = nullptr;
    res = EOS_Connect_CopyProductUserExternalAccountByAccountType(nullptr, &byTypeOpts, &typeInfo);
    assert(res == EOS_Success && typeInfo != nullptr);
    assert(typeInfo->AccountIdType == EOS_EAT_STEAM);
    EOS_Connect_ExternalAccountInfo_Release(typeInfo);

    std::cout << "  Index and AccountType queries verified successfully!" << std::endl;

    // TEST 4: GetLoggedInUser and LoginStatus
    std::cout << "\n[TEST 4] GetLoggedInUser & LoginStatus..." << std::endl;
    assert(EOS_Connect_GetLoggedInUsersCount(nullptr) == 1);
    assert(EOS_Connect_GetLoggedInUserByIndex(nullptr, 0) == localPuid);
    assert(EOS_Connect_GetLoginStatus(nullptr, localPuid) == EOS_LS_LoggedIn);
    assert(EOS_Connect_GetLoginStatus(nullptr, nullptr) == EOS_LS_NotLoggedIn);
    std::cout << "  GetLoggedInUser and LoginStatus APIs verified." << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of External Account Mapping Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
