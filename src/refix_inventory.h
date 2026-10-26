#pragma once
// =============================================================================
// ReFix - Generic Local Inventory Persistence & Store Emulation
// =============================================================================
// Persists unlocked items in ReFix_Inventory.json.
// Emulates ISteamInventory (v003) queries: GetAllItems, GetResultItems,
// GenerateItems, TriggerItemDrop, ExchangeItems, ConsumeItem.
// Opt-in ([Inventory] EnableLocalInventory). Purchases are never emulated: StartPurchase and
// MicroTxnAuthorizationResponse_t always go to Steam.
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

#ifndef STEAM_WIN32
#define STEAM_WIN32 1
#endif
#ifndef STEAM_API_EXPORTS
#define STEAM_API_EXPORTS 1
#endif
#ifndef STEAM_API_FUNCTIONS_IMPL
#define STEAM_API_FUNCTIONS_IMPL 1
#endif
#include "include/steam/isteaminventory.h"
#include "include/steam/isteamuser.h"

// Forward callback dispatching functions implemented in steam_proxy.cpp
extern void ReFix_PostCallback(int32_t iCallback, const void* pData, size_t size, int32_t hSteamUser = 0);
extern void ReFix_QueueSyntheticAPICall(uint64_t callHandle, int iCallback, uint32_t cubParam);
extern uint32_t ReFix_GetRealAppID();

namespace ReFixInventory {

struct LocalItem {
    uint64_t itemId = 0;
    int32_t  definitionId = 0;
    uint16_t quantity = 1;
    uint16_t flags = 0;
    uint32_t acquiredTimestamp = 0;
    std::string propertiesJson;
};

struct InventoryResultEntry {
    SteamInventoryResult_t handle = -1;
    EResult status = k_EResultOK;
    uint32_t timestamp = 0;
    std::vector<SteamItemDetails_t> items;
};

// Initialization and Lifecycle
void Initialize(const std::string& iniPath = "ReFix.ini");
bool LoadFromFile(const std::string& customPath = "");
bool SaveToFile(const std::string& customPath = "");

// Configuration accessors
bool IsLocalInventoryEnabled();
std::string GetInventoryFilePath();

// ISteamInventory emulation routines
bool GetAllItems(SteamInventoryResult_t* pResultHandle);
bool GetResultItems(SteamInventoryResult_t resultHandle, SteamItemDetails_t* pOutItemsArray, uint32_t* punOutItemsArraySize);
EResult GetResultStatus(SteamInventoryResult_t resultHandle);
uint32_t GetResultTimestamp(SteamInventoryResult_t resultHandle);
bool CheckResultSteamID(SteamInventoryResult_t resultHandle, uint64_t steamIDExpected);
void DestroyResult(SteamInventoryResult_t resultHandle);
bool GetItemsByID(SteamInventoryResult_t* pResultHandle, const SteamItemInstanceID_t* pInstanceIDs, uint32_t unCountInstanceIDs);
bool GenerateItems(SteamInventoryResult_t* pResultHandle, const SteamItemDef_t* pArrayItemDefs, const uint32_t* punArrayQuantity, uint32_t unArrayLength);
bool TriggerItemDrop(SteamInventoryResult_t* pResultHandle, SteamItemDef_t dropListDefinition);
bool ConsumeItem(SteamInventoryResult_t* pResultHandle, SteamItemInstanceID_t itemConsume, uint32_t unQuantity);
bool ExchangeItems(
    SteamInventoryResult_t* pResultHandle,
    const SteamItemDef_t* pArrayGenerate,
    const uint32_t* punArrayGenerateQuantity,
    uint32_t unArrayGenerateLength,
    const SteamItemInstanceID_t* pArrayDestroy,
    const uint32_t* punArrayDestroyQuantity,
    uint32_t unArrayDestroyLength
);

// Item Definition queries
bool LoadItemDefinitions();
bool GetItemDefinitionIDs(SteamItemDef_t* pItemDefIDs, uint32_t* punItemDefIDsArraySize);
bool GetItemDefinitionProperty(SteamItemDef_t iDefinition, const char* pchPropertyName, char* pchValueBuffer, uint32_t* punValueBufferSizeOut);

// Result creation helper
SteamInventoryResult_t CreateResult(const std::vector<SteamItemDetails_t>& items);

// VTable & Interface hook
void HookISteamInventory(void* pInterface, const char* pszVersion);

} // namespace ReFixInventory
