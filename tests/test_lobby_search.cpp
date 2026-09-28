// =============================================================================
// ReFix EOS Online v2 - Lobby Search Unit Test Suite
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

static std::atomic<bool> s_findComplete = false;
static std::atomic<int32_t> s_findResult = -1;

static void OnFindLobbiesCallback(const EOS_LobbySearch_FindCallbackInfo* Data) {
    assert(Data != nullptr);
    s_findResult = Data->ResultCode;
    s_findComplete = true;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "ReFix EOS Online v2 - Lobby Search Unit Test Suite" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto& idMgr = ReFixEOS::IdentityManager::Get();
    idMgr.Initialize();
    auto& cbMgr = ReFixEOS::CallbackManager::Get();
    cbMgr.Reset();
    auto& roomBridge = ReFixEOS::RoomManagerBridge::Get();
    roomBridge.Reset();

    EOS_ProductUserId localPuid = idMgr.GetLocalProductUserId();
    std::string puidStr = idMgr.GetLocalProductUserIdString();

    // 1. Create two test lobbies in authoritative backend
    std::cout << "\n[TEST 1] Setting up two lobbies for search testing..." << std::endl;
    std::unordered_map<std::string, std::string> attrsA = {
        {"bucket_id", "s:Chameleon_Deathmatch"},
        {"map", "s:Desert"},
        {"mode", "s:DM"},
        {"skill_level", "i:5"}
    };
    std::unordered_map<std::string, std::string> attrsB = {
        {"bucket_id", "s:Chameleon_Coop"},
        {"map", "s:Jungle"},
        {"mode", "s:COOP"},
        {"skill_level", "i:10"}
    };

    bool createDoneA = false;
    std::string lobIdA;
    roomBridge.CreateLobby(4, attrsA, [&createDoneA, &lobIdA](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::SUCCESS);
        lobIdA = lob.lobbyId;
        createDoneA = true;
    });
    roomBridge.Tick();
    assert(createDoneA);

    bool createDoneB = false;
    std::string lobIdB;
    roomBridge.CreateLobby(8, attrsB, [&createDoneB, &lobIdB](ReFixOnline::EBackendResult res, const ReFixOnline::LobbyData& lob) {
        assert(res == ReFixOnline::SUCCESS);
        lobIdB = lob.lobbyId;
        createDoneB = true;
    });
    roomBridge.Tick();
    assert(createDoneB);
    std::cout << "  [PASS] Lobbies created: A=" << lobIdA << ", B=" << lobIdB << std::endl;

    // 2. Test Search without filters (returns both)
    std::cout << "\n[TEST 2] Unfiltered Search (returns all active lobbies)..." << std::endl;
    EOS_HLobbySearch searchHandleAll = nullptr;
    EOS_Lobby_CreateLobbySearchOptions searchOpts = {};
    searchOpts.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
    searchOpts.MaxResults = 50;
    EOS_EResult searchCreateRes = EOS_Lobby_CreateLobbySearch(nullptr, &searchOpts, &searchHandleAll);
    assert(searchCreateRes == EOS_Success);
    assert(searchHandleAll != nullptr);

    s_findComplete = false;
    s_findResult = -1;
    EOS_LobbySearch_FindOptions findOpts = {};
    findOpts.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
    findOpts.LocalUserId = localPuid;
    EOS_LobbySearch_Find(searchHandleAll, &findOpts, (void*)0x123, OnFindLobbiesCallback);

    assert(!s_findComplete);
    roomBridge.Tick();
    cbMgr.FlushCallbacks();
    assert(s_findComplete);
    assert(s_findResult == EOS_Success);

    EOS_LobbySearch_GetSearchResultCountOptions countOpts = {};
    countOpts.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
    uint32_t count = EOS_LobbySearch_GetSearchResultCount(searchHandleAll, &countOpts);
    assert(count == 2);
    std::cout << "  [PASS] Unfiltered search returned count = " << count << std::endl;
    EOS_LobbySearch_Release(searchHandleAll);

    // 3. Test Search with Attribute Filter (map = Desert)
    std::cout << "\n[TEST 3] Filtered Search (map = Desert)..." << std::endl;
    EOS_HLobbySearch searchHandleFiltered = nullptr;
    searchCreateRes = EOS_Lobby_CreateLobbySearch(nullptr, &searchOpts, &searchHandleFiltered);
    assert(searchCreateRes == EOS_Success);

    EOS_Lobby_AttributeData filterAttr = {};
    filterAttr.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    filterAttr.Key = "map";
    filterAttr.ValueType = EOS_AT_STRING;
    filterAttr.Value.AsUtf8 = "Desert";

    EOS_LobbySearch_SetParameterOptions paramOpts = {};
    paramOpts.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
    paramOpts.Parameter = &filterAttr;
    paramOpts.ComparisonOp = 0; // Equal
    EOS_EResult setParamRes = EOS_LobbySearch_SetParameter(searchHandleFiltered, &paramOpts);
    assert(setParamRes == EOS_Success);

    s_findComplete = false;
    EOS_LobbySearch_Find(searchHandleFiltered, &findOpts, nullptr, OnFindLobbiesCallback);
    roomBridge.Tick();
    cbMgr.FlushCallbacks();
    assert(s_findComplete);
    assert(s_findResult == EOS_Success);

    count = EOS_LobbySearch_GetSearchResultCount(searchHandleFiltered, &countOpts);
    assert(count == 1);
    std::cout << "  [PASS] Filtered search returned exact count = 1" << std::endl;

    // 4. Test Inspection of Search Result via EOS_LobbyDetails_*
    std::cout << "\n[TEST 4] Inspecting Search Result via EOS_LobbyDetails_*..." << std::endl;
    EOS_HLobbyDetails detailsHandle = nullptr;
    EOS_LobbySearch_CopySearchResultByIndexOptions copyOpts = {};
    copyOpts.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
    copyOpts.LobbyIndex = 0;
    EOS_EResult copyRes = EOS_LobbySearch_CopySearchResultByIndex(searchHandleFiltered, &copyOpts, &detailsHandle);
    assert(copyRes == EOS_Success);
    assert(detailsHandle != nullptr);

    // CopyInfo
    EOS_LobbyDetails_CopyInfoOptions copyInfoOpts = {};
    copyInfoOpts.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
    EOS_LobbyDetails_Info* detailsInfo = nullptr;
    EOS_EResult copyInfoRes = EOS_LobbyDetails_CopyInfo(detailsHandle, &copyInfoOpts, &detailsInfo);
    assert(copyInfoRes == EOS_Success);
    assert(detailsInfo != nullptr);
    assert(std::string(detailsInfo->LobbyId) == lobIdA);
    assert(detailsInfo->MaxMembers == 4);
    assert(detailsInfo->AvailableSlots == 3);
    assert(std::string(detailsInfo->BucketId) == "Chameleon_Deathmatch");
    std::cout << "  [PASS] LobbyDetails_CopyInfo: LobbyId=" << detailsInfo->LobbyId << ", MaxMembers=" << detailsInfo->MaxMembers << ", AvailableSlots=" << detailsInfo->AvailableSlots << std::endl;
    EOS_LobbyDetails_Info_Release(detailsInfo);

    // Attribute inspection by key
    EOS_LobbyDetails_CopyAttributeByKeyOptions attrKeyOpts = {};
    attrKeyOpts.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST;
    attrKeyOpts.AttrKey = "map";
    EOS_Lobby_Attribute* mapAttr = nullptr;
    EOS_EResult copyAttrRes = EOS_LobbyDetails_CopyAttributeByKey(detailsHandle, &attrKeyOpts, &mapAttr);
    assert(copyAttrRes == EOS_Success);
    assert(mapAttr != nullptr);
    assert(std::string(mapAttr->Data->Key) == "map");
    assert(std::string(mapAttr->Data->Value.AsUtf8) == "Desert");
    std::cout << "  [PASS] LobbyDetails_CopyAttributeByKey('map') = '" << mapAttr->Data->Value.AsUtf8 << "'" << std::endl;
    EOS_Lobby_Attribute_Release(mapAttr);

    // Attribute count & index copy
    EOS_LobbyDetails_GetAttributeCountOptions attrCountOpts = {};
    attrCountOpts.ApiVersion = EOS_LOBBYDETAILS_GETATTRIBUTECOUNT_API_LATEST;
    uint32_t attrCount = EOS_LobbyDetails_GetAttributeCount(detailsHandle, &attrCountOpts);
    assert(attrCount >= 4);
    std::cout << "  [PASS] LobbyDetails_GetAttributeCount = " << attrCount << std::endl;

    // Member count & owner
    EOS_LobbyDetails_GetMemberCountOptions memCountOpts = {};
    memCountOpts.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;
    uint32_t memCount = EOS_LobbyDetails_GetMemberCount(detailsHandle, &memCountOpts);
    assert(memCount == 1);

    EOS_LobbyDetails_GetLobbyOwnerOptions ownerOpts = {};
    ownerOpts.ApiVersion = EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST;
    EOS_ProductUserId ownerPuid = EOS_LobbyDetails_GetLobbyOwner(detailsHandle, &ownerOpts);
    assert(ownerPuid == localPuid);
    std::cout << "  [PASS] LobbyDetails_GetMemberCount = " << memCount << ", Owner matched local PUID" << std::endl;

    EOS_LobbyDetails_Release(detailsHandle);
    EOS_LobbySearch_Release(searchHandleFiltered);

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "[SUCCESS] 100% of Lobby Search Unit Tests Passed!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
