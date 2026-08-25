// =============================================================================
// ReFix EOS Online v2 - Lobby Join, Leave & Destroy Unit Test Suite
// =============================================================================
#include "../src/eos/eos_lobby.h"
#include "../src/eos/eos_identity.h"
#include "../src/eos/eos_callbacks.h"
#include "../src/eos_core/eos_room_manager.h"
#include <iostream>
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>

static std::atomic<bool> s_createComplete = false;
static std::atomic<int32_t> s_createResult = -1;
static std::string s_createdLobbyId = "";

static void OnCreateLobbyCallback(const void* data) {
    const auto* info = (const EOS_Lobby_CreateLobbyCallbackInfo*)data;
    assert(info != nullptr);
    s_createResult = info->ResultCode;
    if (info->LobbyId) s_createdLobbyId = info->LobbyId;
    s_createComplete = true;
}

static std::atomic<bool> s_joinComplete = false;
static std::atomic<int32_t> s_joinResult = -1;

static void OnJoinLobbyCallback(const void* data) {
    const auto* info = (const EOS_Lobby_JoinLobbyCallbackInfo*)data;
    assert(info != nullptr);
    s_joinResult = info->ResultCode;
    s_joinComplete = true;
}

static std::atomic<bool> s_leaveComplete = false;
static std::atomic<int32_t> s_leaveResult = -1;

static void OnLeaveLobbyCallback(const void* data) {
    const auto* info = (const EOS_Lobby_LeaveLobbyCallbackInfo*)data;
    assert(info != nullptr);
    s_leaveResult = info->ResultCode;
    s_leaveComplete = true;
}

static std::atomic<bool> s_destroyComplete = false;
static std::atomic<int32_t> s_destroyResult = -1;

static void OnDestroyLobbyCallback(const void* data) {
    const auto* info = (const EOS_Lobby_DestroyLobbyCallbackInfo*)data;
    assert(info != nullptr);
    s_destroyResult = info->ResultCode;
    s_destroyComplete = true;
}

static std::atomic<bool> s_findComplete = false;
static void OnFindCallback(const EOS_LobbySearch_FindCallbackInfo* Data) {
    s_findComplete = true;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Lobby Join, Leave & Destroy Tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();
    auto& roomBridge = ReFixEOS::RoomManagerBridge::Get();
    roomBridge.Reset();

    EOS_ProductUserId hostPuid = idMgr.GetLocalProductUserId();

    // 1. Host creates a 4-player lobby
    std::cout << "\n[STEP 1] Host creates 4-player lobby..." << std::endl;
    s_createComplete = false;
    EOS_Lobby_CreateLobbyOptions createOpts = {};
    createOpts.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
    createOpts.LocalUserId = hostPuid;
    createOpts.MaxLobbyMembers = 4;
    createOpts.PermissionLevel = EOS_LPL_PUBLICADVERTISED;
    createOpts.bPresenceEnabled = 1;
    createOpts.BucketId = "Chameleon_Arena";

    EOS_Lobby_CreateLobby(nullptr, &createOpts, nullptr, OnCreateLobbyCallback);
    roomBridge.Tick();
    cbMgr.FlushCallbacks();
    assert(s_createComplete);
    assert(s_createResult == EOS_Success);
    assert(!s_createdLobbyId.empty());
    std::cout << "  [PASS] Lobby created: LobbyId=" << s_createdLobbyId << std::endl;

    // 2. Client searches and obtains LobbyDetailsHandle
    std::cout << "\n[STEP 2] Client searches and acquires LobbyDetails handle..." << std::endl;
    EOS_HLobbySearch searchHandle = nullptr;
    EOS_Lobby_CreateLobbySearchOptions searchOpts = {};
    searchOpts.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
    searchOpts.MaxResults = 10;
    EOS_Lobby_CreateLobbySearch(nullptr, &searchOpts, &searchHandle);

    s_findComplete = false;
    EOS_LobbySearch_FindOptions findOpts = {};
    findOpts.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
    findOpts.LocalUserId = hostPuid;
    EOS_LobbySearch_Find(searchHandle, &findOpts, nullptr, OnFindCallback);
    roomBridge.Tick();
    cbMgr.FlushCallbacks();
    assert(s_findComplete);

    EOS_HLobbyDetails detailsHandle = nullptr;
    EOS_LobbySearch_CopySearchResultByIndexOptions copyOpts = {};
    copyOpts.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
    copyOpts.LobbyIndex = 0;
    EOS_EResult copyRes = EOS_LobbySearch_CopySearchResultByIndex(searchHandle, &copyOpts, &detailsHandle);
    assert(copyRes == EOS_Success);
    assert(detailsHandle != nullptr);
    std::cout << "  [PASS] LobbyDetails handle acquired from search result." << std::endl;

    // 3. Client joins lobby
    std::cout << "\n[STEP 3] Client joins lobby via Backend..." << std::endl;
    std::string clientPuidStr = "11112222333344445555666677778888";
    EOS_ProductUserId clientPuid = idMgr.ProductUserIdFromString(clientPuidStr.c_str());
    std::string clientToken;
    roomBridge.GetServerState().AuthenticateSession(clientPuidStr, "PlayerB", clientToken);

    ReFixOnline::LobbyData joinLob;
    ReFixOnline::EBackendResult joinRes = roomBridge.GetServerState().JoinLobby(clientPuidStr, "PlayerB", s_createdLobbyId, joinLob);
    assert(joinRes == ReFixOnline::SUCCESS);
    assert(joinLob.currentMembers == 2);
    std::cout << "  [PASS] Client joined lobby! Member count = " << joinLob.currentMembers << std::endl;

    // 4. Client leaves lobby
    std::cout << "\n[STEP 4] Client leaves lobby..." << std::endl;
    std::string newOwner;
    ReFixOnline::EBackendResult leaveRes = roomBridge.GetServerState().LeaveLobby(clientPuidStr, s_createdLobbyId, newOwner);
    assert(leaveRes == ReFixOnline::SUCCESS);

    ReFixOnline::LobbyData lobData;
    assert(roomBridge.GetLobby(s_createdLobbyId, lobData));
    assert(lobData.currentMembers == 1);
    std::cout << "  [PASS] Client left lobby! Member count = " << lobData.currentMembers << std::endl;

    // 5. Host destroys lobby
    std::cout << "\n[STEP 5] Host destroys lobby via EOS_Lobby_DestroyLobby..." << std::endl;
    s_destroyComplete = false;
    EOS_Lobby_DestroyLobbyOptions destroyOpts = {};
    destroyOpts.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
    destroyOpts.LocalUserId = hostPuid;
    destroyOpts.LobbyId = s_createdLobbyId.c_str();

    EOS_Lobby_DestroyLobby(nullptr, &destroyOpts, nullptr, OnDestroyLobbyCallback);
    roomBridge.Tick();
    cbMgr.FlushCallbacks();
    assert(s_destroyComplete);
    assert(s_destroyResult == EOS_Success);
    std::cout << "  [PASS] DestroyLobby returned EOS_Success!" << std::endl;

    // 6. Search confirms lobby is gone
    std::cout << "\n[STEP 6] Search confirms 0 active lobbies remain..." << std::endl;
    s_findComplete = false;
    EOS_LobbySearch_Find(searchHandle, &findOpts, nullptr, OnFindCallback);
    roomBridge.Tick();
    cbMgr.FlushCallbacks();
    assert(s_findComplete);

    EOS_LobbySearch_GetSearchResultCountOptions countOpts = {};
    countOpts.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
    uint32_t count = EOS_LobbySearch_GetSearchResultCount(searchHandle, &countOpts);
    assert(count == 0);
    std::cout << "  [PASS] Search returned 0 results as expected." << std::endl;

    EOS_LobbyDetails_Release(detailsHandle);
    EOS_LobbySearch_Release(searchHandle);

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Lobby Join, Leave & Destroy Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
