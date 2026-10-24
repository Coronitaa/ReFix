// =============================================================================
// ReFix - Generic Local Inventory Persistence & Store Emulation
// =============================================================================
#include "refix_inventory.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <atomic>

// Declare external logger from steam_proxy.cpp
extern void ReFixLog(const char* fmt, ...);

// Declare external VTable hooker from steam_proxy.cpp
extern bool HookVTableMethod(void* pInterface, int vtableIndex, void* pHookFn, void** ppOriginalFn);

namespace ReFixInventory {

// Configuration settings
static bool g_enableLocalInventory = true;
static bool g_enableStoreEmulation = true;
static bool g_autoAuthorizeMicroTxn = true;
static bool g_instantPurchaseSuccess = true;
static std::string g_inventoryFileName = "ReFix_Inventory.json";
static uint32_t g_defaultQuantity = 1;
static bool g_initialized = false;

// Synchronization & state
static std::recursive_mutex g_invMutex;
static std::vector<LocalItem> g_localItems;
static std::unordered_map<SteamInventoryResult_t, InventoryResultEntry> g_activeResults;
static std::atomic<SteamInventoryResult_t> g_nextResultHandle{ 1 };
static std::atomic<uint64_t> g_nextItemId{ 100000001ULL };
static std::atomic<uint64_t> g_nextOrderId{ 88000000001ULL };

// Helper to get executable / working directory
static std::string GetBasePath() {
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
        return std::string(exePath);
    }
    return ".\\";
}

std::string GetInventoryFilePath() {
    std::string base = GetBasePath();
    std::string path = base + g_inventoryFileName;
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        // Fallback to current working directory
        if (GetFileAttributesA(g_inventoryFileName.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return g_inventoryFileName;
        }
    }
    return path;
}

bool IsLocalInventoryEnabled() { return g_enableLocalInventory; }
bool IsStoreEmulationEnabled() { return g_enableStoreEmulation; }
bool IsAutoAuthorizeMicroTxnEnabled() { return g_autoAuthorizeMicroTxn; }
bool IsInstantPurchaseSuccessEnabled() { return g_instantPurchaseSuccess; }

uint64_t GenerateOrderId() {
    return g_nextOrderId.fetch_add(1);
}

// -----------------------------------------------------------------------------
// Lightweight JSON Parser and Serializer for ReFix_Inventory.json
// -----------------------------------------------------------------------------
static std::string TrimWhitespace(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool LoadFromFile(const std::string& customPath) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    std::string path = customPath.empty() ? GetInventoryFilePath() : customPath;

    std::ifstream file(path);
    if (!file.is_open()) {
        ReFixLog("[Inventory] Inventory file not found at '%s'. Creating default inventory.", path.c_str());
        // Populate default starter cosmetic items if missing
        g_localItems.clear();
        uint32_t now = (uint32_t)time(nullptr);
        for (int i = 0; i < 5; ++i) {
            LocalItem item;
            item.itemId = g_nextItemId.fetch_add(1);
            item.definitionId = 100 + i;
            item.quantity = 1;
            item.flags = 0;
            item.acquiredTimestamp = now;
            g_localItems.push_back(item);
        }
        SaveToFile(path);
        return true;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    g_localItems.clear();
    uint64_t maxId = 100000000ULL;

    // Streamlined JSON parsing for the "items" array:
    // Looks for JSON objects containing itemId, definitionId, quantity, flags, acquired
    size_t itemsPos = content.find("\"items\"");
    if (itemsPos != std::string::npos) {
        size_t arrayStart = content.find('[', itemsPos);
        size_t arrayEnd = content.rfind(']');
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos && arrayEnd > arrayStart) {
            std::string itemsBlock = content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            size_t objStart = 0;
            while ((objStart = itemsBlock.find('{', objStart)) != std::string::npos) {
                size_t objEnd = itemsBlock.find('}', objStart);
                if (objEnd == std::string::npos) break;

                std::string objStr = itemsBlock.substr(objStart, objEnd - objStart + 1);
                LocalItem item;
                item.acquiredTimestamp = (uint32_t)time(nullptr);

                auto extractUint64 = [&](const char* key, uint64_t defaultVal) -> uint64_t {
                    size_t kp = objStr.find(key);
                    if (kp == std::string::npos) return defaultVal;
                    size_t colon = objStr.find(':', kp);
                    if (colon == std::string::npos) return defaultVal;
                    return (uint64_t)_strtoui64(objStr.c_str() + colon + 1, nullptr, 10);
                };

                auto extractInt32 = [&](const char* key, int32_t defaultVal) -> int32_t {
                    size_t kp = objStr.find(key);
                    if (kp == std::string::npos) return defaultVal;
                    size_t colon = objStr.find(':', kp);
                    if (colon == std::string::npos) return defaultVal;
                    return (int32_t)strtol(objStr.c_str() + colon + 1, nullptr, 10);
                };

                item.itemId = extractUint64("\"itemId\"", 0);
                if (item.itemId == 0) item.itemId = extractUint64("\"item_id\"", 0);
                item.definitionId = extractInt32("\"definitionId\"", 0);
                if (item.definitionId == 0) item.definitionId = extractInt32("\"item_def\"", 0);
                item.quantity = (uint16_t)extractInt32("\"quantity\"", 1);
                item.flags = (uint16_t)extractInt32("\"flags\"", 0);
                item.acquiredTimestamp = (uint32_t)extractUint64("\"acquired\"", (uint64_t)time(nullptr));

                if (item.itemId > 0 && item.definitionId != 0) {
                    if (item.itemId > maxId) maxId = item.itemId;
                    g_localItems.push_back(item);
                }

                objStart = objEnd + 1;
            }
        }
    }

    g_nextItemId.store(maxId + 1);
    ReFixLog("[Inventory] Successfully loaded %zu items from '%s' (next ItemID=%llu)",
             g_localItems.size(), path.c_str(), g_nextItemId.load());
    return true;
}

bool SaveToFile(const std::string& customPath) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    std::string path = customPath.empty() ? GetInventoryFilePath() : customPath;

    // Prune consumed or removed items before saving
    g_localItems.erase(
        std::remove_if(g_localItems.begin(), g_localItems.end(),
            [](const LocalItem& it) {
                return it.quantity == 0 || (it.flags & k_ESteamItemRemoved) != 0;
            }),
        g_localItems.end());

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        ReFixLog("[Inventory] ERROR: Failed to open '%s' for writing", path.c_str());
        return false;
    }

    uint32_t realAppId = ReFix_GetRealAppID();
    file << "{\n";
    file << "  \"version\": 1,\n";
    file << "  \"appId\": " << realAppId << ",\n";
    file << "  \"items\": [\n";

    for (size_t i = 0; i < g_localItems.size(); ++i) {
        const auto& it = g_localItems[i];
        file << "    {\n";
        file << "      \"itemId\": " << it.itemId << ",\n";
        file << "      \"definitionId\": " << it.definitionId << ",\n";
        file << "      \"quantity\": " << it.quantity << ",\n";
        file << "      \"flags\": " << it.flags << ",\n";
        file << "      \"acquired\": " << it.acquiredTimestamp << "\n";
        file << "    }" << (i + 1 < g_localItems.size() ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";
    file.close();

    ReFixLog("[Inventory] Saved %zu items to '%s'", g_localItems.size(), path.c_str());
    return true;
}

void Initialize(const std::string& iniPath) {
    if (g_initialized) return;
    g_initialized = true;

    char buf[MAX_PATH];
    // [Store]
    GetPrivateProfileStringA("Store", "EnableStoreEmulation", "true", buf, sizeof(buf), iniPath.c_str());
    g_enableStoreEmulation = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);

    GetPrivateProfileStringA("Store", "AutoAuthorizeMicroTxn", "true", buf, sizeof(buf), iniPath.c_str());
    g_autoAuthorizeMicroTxn = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);

    GetPrivateProfileStringA("Store", "InstantPurchaseSuccess", "true", buf, sizeof(buf), iniPath.c_str());
    g_instantPurchaseSuccess = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);

    // [Inventory]
    GetPrivateProfileStringA("Inventory", "EnableLocalInventory", "true", buf, sizeof(buf), iniPath.c_str());
    g_enableLocalInventory = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0);

    GetPrivateProfileStringA("Inventory", "InventoryFile", "ReFix_Inventory.json", buf, sizeof(buf), iniPath.c_str());
    g_inventoryFileName = buf;

    g_defaultQuantity = GetPrivateProfileIntA("Inventory", "DefaultQuantity", 1, iniPath.c_str());

    ReFixLog("[Inventory] Initialized: LocalInventory=%d, StoreEmulation=%d, AutoAuth=%d, InstantSuccess=%d, File='%s'",
             (int)g_enableLocalInventory, (int)g_enableStoreEmulation, (int)g_autoAuthorizeMicroTxn,
             (int)g_instantPurchaseSuccess, g_inventoryFileName.c_str());

    if (g_enableLocalInventory) {
        LoadFromFile();
    }
}

// -----------------------------------------------------------------------------
// Microtransaction Authorization (Callback 152)
// -----------------------------------------------------------------------------
void AuthorizeMicroTxn(uint64_t orderId, uint32_t appId) {
    if (!g_enableStoreEmulation || !g_autoAuthorizeMicroTxn) return;

    if (appId == 0) appId = ReFix_GetRealAppID();

    MicroTxnAuthorizationResponse_t resp{};
    resp.m_unAppID = appId;
    resp.m_ulOrderID = orderId;
    resp.m_bAuthorized = 1;

    ReFixLog("[Store] Emitting MicroTxnAuthorizationResponse_t (Callback 152): AppID=%u, OrderID=%llu, Authorized=1",
             appId, orderId);

    // Post to unified callback dispatcher (covers ManualDispatch and CCallback)
    ReFix_PostCallback(MicroTxnAuthorizationResponse_t::k_iCallback, &resp, sizeof(resp));
}

// -----------------------------------------------------------------------------
// Result Handle Management & ISteamInventory Emulation
// -----------------------------------------------------------------------------
SteamInventoryResult_t CreateResult(const std::vector<SteamItemDetails_t>& items) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    SteamInventoryResult_t handle = g_nextResultHandle.fetch_add(1);
    if (handle <= 0) handle = 1;

    InventoryResultEntry entry;
    entry.handle = handle;
    entry.status = k_EResultOK;
    entry.timestamp = (uint32_t)time(nullptr);
    entry.items = items;

    g_activeResults[handle] = entry;
    return handle;
}

EResult GetResultStatus(SteamInventoryResult_t resultHandle) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    auto it = g_activeResults.find(resultHandle);
    if (it != g_activeResults.end()) {
        return it->second.status;
    }
    return k_EResultInvalidParam;
}

bool GetResultItems(SteamInventoryResult_t resultHandle, SteamItemDetails_t* pOutItemsArray, uint32_t* punOutItemsArraySize) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    if (!punOutItemsArraySize) return false;

    auto it = g_activeResults.find(resultHandle);
    if (it == g_activeResults.end()) {
        *punOutItemsArraySize = 0;
        return false;
    }

    const auto& items = it->second.items;
    uint32_t totalCount = (uint32_t)items.size();

    if (!pOutItemsArray) {
        *punOutItemsArraySize = totalCount;
        return true;
    }

    uint32_t countToCopy = (std::min)(*punOutItemsArraySize, totalCount);
    for (uint32_t i = 0; i < countToCopy; ++i) {
        pOutItemsArray[i] = items[i];
    }
    *punOutItemsArraySize = countToCopy;
    ReFixLog("[Inventory] GetResultItems(handle=%d): Returning %u / %u items",
             resultHandle, countToCopy, totalCount);
    return true;
}

uint32_t GetResultTimestamp(SteamInventoryResult_t resultHandle) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    auto it = g_activeResults.find(resultHandle);
    if (it != g_activeResults.end()) {
        return it->second.timestamp;
    }
    return (uint32_t)time(nullptr);
}

bool CheckResultSteamID(SteamInventoryResult_t resultHandle, uint64_t steamIDExpected) {
    return true;
}

void DestroyResult(SteamInventoryResult_t resultHandle) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    g_activeResults.erase(resultHandle);
}

bool GetAllItems(SteamInventoryResult_t* pResultHandle) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    if (!pResultHandle) return false;

    if (!g_enableLocalInventory) return false;

    std::vector<SteamItemDetails_t> details;
    details.reserve(g_localItems.size());
    for (const auto& it : g_localItems) {
        if (it.quantity == 0 || (it.flags & k_ESteamItemRemoved) != 0) {
            continue;
        }
        SteamItemDetails_t d{};
        d.m_itemId = it.itemId;
        d.m_iDefinition = it.definitionId;
        d.m_unQuantity = it.quantity;
        d.m_unFlags = it.flags;
        details.push_back(d);
    }

    SteamInventoryResult_t handle = CreateResult(details);
    *pResultHandle = handle;

    ReFixLog("[Inventory] GetAllItems: Prepared %zu items into result handle %d", details.size(), handle);

    // Notify listeners via standard Steamworks callbacks
    SteamInventoryResultReady_t ready{};
    ready.m_handle = handle;
    ready.m_result = k_EResultOK;
    ReFix_PostCallback(SteamInventoryResultReady_t::k_iCallback, &ready, sizeof(ready));

    SteamInventoryFullUpdate_t full{};
    full.m_handle = handle;
    ReFix_PostCallback(SteamInventoryFullUpdate_t::k_iCallback, &full, sizeof(full));

    return true;
}

bool GetItemsByID(SteamInventoryResult_t* pResultHandle, const SteamItemInstanceID_t* pInstanceIDs, uint32_t unCountInstanceIDs) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    if (!pResultHandle) return false;

    std::vector<SteamItemDetails_t> matches;
    if (pInstanceIDs && unCountInstanceIDs > 0) {
        for (uint32_t i = 0; i < unCountInstanceIDs; ++i) {
            uint64_t reqId = pInstanceIDs[i];
            for (const auto& item : g_localItems) {
                if (item.itemId == reqId) {
                    SteamItemDetails_t d{};
                    d.m_itemId = item.itemId;
                    d.m_iDefinition = item.definitionId;
                    d.m_unQuantity = item.quantity;
                    d.m_unFlags = item.flags;
                    matches.push_back(d);
                    break;
                }
            }
        }
    }

    SteamInventoryResult_t handle = CreateResult(matches);
    *pResultHandle = handle;

    SteamInventoryResultReady_t ready{};
    ready.m_handle = handle;
    ready.m_result = k_EResultOK;
    ReFix_PostCallback(SteamInventoryResultReady_t::k_iCallback, &ready, sizeof(ready));
    return true;
}

bool GenerateItems(SteamInventoryResult_t* pResultHandle, const SteamItemDef_t* pArrayItemDefs, const uint32_t* punArrayQuantity, uint32_t unArrayLength) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    if (!pResultHandle) return false;

    std::vector<SteamItemDetails_t> generated;
    uint32_t now = (uint32_t)time(nullptr);

    if (pArrayItemDefs && unArrayLength > 0) {
        for (uint32_t i = 0; i < unArrayLength; ++i) {
            int32_t defId = pArrayItemDefs[i];
            uint32_t qty = (punArrayQuantity ? punArrayQuantity[i] : 1);
            if (qty == 0) qty = 1;

            LocalItem newItem;
            newItem.itemId = g_nextItemId.fetch_add(1);
            newItem.definitionId = defId;
            newItem.quantity = (uint16_t)qty;
            newItem.flags = 0;
            newItem.acquiredTimestamp = now;
            g_localItems.push_back(newItem);

            SteamItemDetails_t d{};
            d.m_itemId = newItem.itemId;
            d.m_iDefinition = newItem.definitionId;
            d.m_unQuantity = newItem.quantity;
            d.m_unFlags = newItem.flags;
            generated.push_back(d);
        }
        SaveToFile();
    }

    SteamInventoryResult_t handle = CreateResult(generated);
    *pResultHandle = handle;

    ReFixLog("[Inventory] GenerateItems: Created %zu new items (handle=%d)", generated.size(), handle);

    SteamInventoryResultReady_t ready{};
    ready.m_handle = handle;
    ready.m_result = k_EResultOK;
    ReFix_PostCallback(SteamInventoryResultReady_t::k_iCallback, &ready, sizeof(ready));

    return true;
}

bool TriggerItemDrop(SteamInventoryResult_t* pResultHandle, SteamItemDef_t dropListDefinition) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    if (!pResultHandle) return false;

    LocalItem newItem;
    newItem.itemId = g_nextItemId.fetch_add(1);
    newItem.definitionId = (dropListDefinition != 0 ? dropListDefinition : 200);
    newItem.quantity = 1;
    newItem.flags = 0;
    newItem.acquiredTimestamp = (uint32_t)time(nullptr);
    g_localItems.push_back(newItem);
    SaveToFile();

    SteamItemDetails_t d{};
    d.m_itemId = newItem.itemId;
    d.m_iDefinition = newItem.definitionId;
    d.m_unQuantity = newItem.quantity;
    d.m_unFlags = newItem.flags;

    SteamInventoryResult_t handle = CreateResult({ d });
    *pResultHandle = handle;

    ReFixLog("[Inventory] TriggerItemDrop: Dropped item %d as ItemID=%llu (handle=%d)",
             dropListDefinition, newItem.itemId, handle);

    SteamInventoryResultReady_t ready{};
    ready.m_handle = handle;
    ready.m_result = k_EResultOK;
    ReFix_PostCallback(SteamInventoryResultReady_t::k_iCallback, &ready, sizeof(ready));

    return true;
}

bool ConsumeItem(SteamInventoryResult_t* pResultHandle, SteamItemInstanceID_t itemConsume, uint32_t unQuantity) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    if (!pResultHandle) return false;

    std::vector<SteamItemDetails_t> res;
    for (auto& item : g_localItems) {
        if (item.itemId == itemConsume) {
            if (item.quantity > unQuantity) {
                item.quantity -= (uint16_t)unQuantity;
                item.flags |= k_ESteamItemConsumed;
            } else {
                item.quantity = 0;
                item.flags |= (k_ESteamItemConsumed | k_ESteamItemRemoved);
            }
            SteamItemDetails_t d{};
            d.m_itemId = item.itemId;
            d.m_iDefinition = item.definitionId;
            d.m_unQuantity = item.quantity;
            d.m_unFlags = item.flags;
            res.push_back(d);
            break;
        }
    }
    SaveToFile();

    SteamInventoryResult_t handle = CreateResult(res);
    *pResultHandle = handle;

    SteamInventoryResultReady_t ready{};
    ready.m_handle = handle;
    ready.m_result = k_EResultOK;
    ReFix_PostCallback(SteamInventoryResultReady_t::k_iCallback, &ready, sizeof(ready));

    return true;
}

bool ExchangeItems(
    SteamInventoryResult_t* pResultHandle,
    const SteamItemDef_t* pArrayGenerate,
    const uint32_t* punArrayGenerateQuantity,
    uint32_t unArrayGenerateLength,
    const SteamItemInstanceID_t* pArrayDestroy,
    const uint32_t* punArrayDestroyQuantity,
    uint32_t unArrayDestroyLength
) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    if (!pResultHandle) return false;

    // Remove or decrement destroyed items
    if (pArrayDestroy && unArrayDestroyLength > 0) {
        for (uint32_t i = 0; i < unArrayDestroyLength; ++i) {
            uint64_t destroyId = pArrayDestroy[i];
            uint32_t qty = (punArrayDestroyQuantity ? punArrayDestroyQuantity[i] : 1);
            for (auto& it : g_localItems) {
                if (it.itemId == destroyId) {
                    if (it.quantity > qty) {
                        it.quantity -= (uint16_t)qty;
                    } else {
                        it.quantity = 0;
                        it.flags |= k_ESteamItemRemoved;
                    }
                    break;
                }
            }
        }
    }

    // Add generated items
    std::vector<SteamItemDetails_t> generated;
    uint32_t now = (uint32_t)time(nullptr);
    if (pArrayGenerate && unArrayGenerateLength > 0) {
        for (uint32_t i = 0; i < unArrayGenerateLength; ++i) {
            int32_t defId = pArrayGenerate[i];
            uint32_t qty = (punArrayGenerateQuantity ? punArrayGenerateQuantity[i] : 1);

            LocalItem item;
            item.itemId = g_nextItemId.fetch_add(1);
            item.definitionId = defId;
            item.quantity = (uint16_t)qty;
            item.flags = 0;
            item.acquiredTimestamp = now;
            g_localItems.push_back(item);

            SteamItemDetails_t d{};
            d.m_itemId = item.itemId;
            d.m_iDefinition = item.definitionId;
            d.m_unQuantity = item.quantity;
            d.m_unFlags = item.flags;
            generated.push_back(d);
        }
    }
    SaveToFile();

    SteamInventoryResult_t handle = CreateResult(generated);
    *pResultHandle = handle;

    SteamInventoryResultReady_t ready{};
    ready.m_handle = handle;
    ready.m_result = k_EResultOK;
    ReFix_PostCallback(SteamInventoryResultReady_t::k_iCallback, &ready, sizeof(ready));

    return true;
}

SteamAPICall_t ProcessStartPurchase(
    const SteamItemDef_t* pArrayItemDefs,
    const uint32_t* punArrayQuantity,
    uint32_t unArrayLength,
    uint64_t& outOrderId
) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);

    static std::atomic<SteamAPICall_t> s_nextCall{ 77000000001ULL };
    SteamAPICall_t callHandle = s_nextCall.fetch_add(1);
    outOrderId = (uint64_t)callHandle;

    ReFixLog("[Store] ProcessStartPurchase: Initiating order %llu for %u items (callHandle=%llu)",
             outOrderId, unArrayLength, callHandle);

    if (g_instantPurchaseSuccess) {
        // Automatically grant items into local inventory
        uint32_t now = (uint32_t)time(nullptr);
        std::vector<SteamItemDetails_t> granted;
        if (pArrayItemDefs && unArrayLength > 0) {
            for (uint32_t i = 0; i < unArrayLength; ++i) {
                int32_t defId = pArrayItemDefs[i];
                uint32_t qty = (punArrayQuantity ? punArrayQuantity[i] : 1);

                LocalItem item;
                item.itemId = g_nextItemId.fetch_add(1);
                item.definitionId = defId;
                item.quantity = (uint16_t)qty;
                item.flags = 0;
                item.acquiredTimestamp = now;
                g_localItems.push_back(item);

                SteamItemDetails_t d{};
                d.m_itemId = item.itemId;
                d.m_iDefinition = item.definitionId;
                d.m_unQuantity = item.quantity;
                d.m_unFlags = item.flags;
                granted.push_back(d);
            }
            SaveToFile();
        }

        // Complete the StartPurchase APICall (Callback 1004)
        ReFix_QueueSyntheticAPICall(callHandle, SteamInventoryStartPurchaseResult_t::k_iCallback, sizeof(SteamInventoryStartPurchaseResult_t));

        // Immediately authorize the microtransaction (Callback 152)
        AuthorizeMicroTxn(outOrderId, ReFix_GetRealAppID());

        // Emit inventory result ready for granted items
        if (!granted.empty()) {
            SteamInventoryResult_t resH = CreateResult(granted);
            SteamInventoryResultReady_t ready{};
            ready.m_handle = resH;
            ready.m_result = k_EResultOK;
            ReFix_PostCallback(SteamInventoryResultReady_t::k_iCallback, &ready, sizeof(ready));
        }
    }

    return callHandle;
}

bool LoadItemDefinitions() {
    ReFixLog("[Inventory] LoadItemDefinitions requested -> Returning OK");
    SteamInventoryDefinitionUpdate_t defUpdate{};
    ReFix_PostCallback(SteamInventoryDefinitionUpdate_t::k_iCallback, &defUpdate, sizeof(defUpdate));
    return true;
}

bool GetItemDefinitionIDs(SteamItemDef_t* pItemDefIDs, uint32_t* punItemDefIDsArraySize) {
    std::lock_guard<std::recursive_mutex> lock(g_invMutex);
    if (!punItemDefIDsArraySize) return false;

    // Collect unique definition IDs from current inventory
    std::vector<SteamItemDef_t> uniqueDefs;
    for (const auto& item : g_localItems) {
        if (std::find(uniqueDefs.begin(), uniqueDefs.end(), item.definitionId) == uniqueDefs.end()) {
            uniqueDefs.push_back(item.definitionId);
        }
    }

    uint32_t total = (uint32_t)uniqueDefs.size();
    if (!pItemDefIDs) {
        *punItemDefIDsArraySize = total;
        return true;
    }

    uint32_t copyCount = (std::min)(*punItemDefIDsArraySize, total);
    for (uint32_t i = 0; i < copyCount; ++i) {
        pItemDefIDs[i] = uniqueDefs[i];
    }
    *punItemDefIDsArraySize = copyCount;
    return true;
}

bool GetItemDefinitionProperty(SteamItemDef_t iDefinition, const char* pchPropertyName, char* pchValueBuffer, uint32_t* punValueBufferSizeOut) {
    if (!punValueBufferSizeOut) return false;
    if (!pchPropertyName) {
        const char* propList = "name,description,type,icon_url";
        uint32_t len = (uint32_t)strlen(propList) + 1;
        if (!pchValueBuffer) {
            *punValueBufferSizeOut = len;
            return true;
        }
        if (*punValueBufferSizeOut == 0) return false;
        strncpy_s(pchValueBuffer, *punValueBufferSizeOut, propList, _TRUNCATE);
        *punValueBufferSizeOut = (uint32_t)strlen(pchValueBuffer) + 1;
        return true;
    }

    std::string val = "";
    if (_stricmp(pchPropertyName, "name") == 0) {
        val = "ReFix Cosmetic Item #" + std::to_string(iDefinition);
    } else if (_stricmp(pchPropertyName, "type") == 0) {
        val = "item";
    }

    uint32_t need = (uint32_t)val.size() + 1;
    if (!pchValueBuffer) {
        *punValueBufferSizeOut = need;
        return true;
    }
    if (*punValueBufferSizeOut == 0) return false;
    strncpy_s(pchValueBuffer, *punValueBufferSizeOut, val.c_str(), _TRUNCATE);
    *punValueBufferSizeOut = (uint32_t)strlen(pchValueBuffer) + 1;
    return true;
}

// -----------------------------------------------------------------------------
// VTable Hook Implementations for ISteamInventory003
// -----------------------------------------------------------------------------
typedef EResult (*fn_ISteamInventory_GetResultStatus_t)(void* self, SteamInventoryResult_t resultHandle);
typedef bool (*fn_ISteamInventory_GetResultItems_t)(void* self, SteamInventoryResult_t resultHandle, SteamItemDetails_t* pOutItemsArray, uint32_t* punOutItemsArraySize);
typedef bool (*fn_ISteamInventory_GetResultItemProperty_t)(void* self, SteamInventoryResult_t resultHandle, uint32_t unItemIndex, const char* pchPropertyName, char* pchValueBuffer, uint32_t* punValueBufferSizeOut);
typedef uint32_t (*fn_ISteamInventory_GetResultTimestamp_t)(void* self, SteamInventoryResult_t resultHandle);
typedef bool (*fn_ISteamInventory_CheckResultSteamID_t)(void* self, SteamInventoryResult_t resultHandle, uint64_t steamIDExpected);
typedef void (*fn_ISteamInventory_DestroyResult_t)(void* self, SteamInventoryResult_t resultHandle);
typedef bool (*fn_ISteamInventory_GetAllItems_t)(void* self, SteamInventoryResult_t* pResultHandle);
typedef bool (*fn_ISteamInventory_GetItemsByID_t)(void* self, SteamInventoryResult_t* pResultHandle, const SteamItemInstanceID_t* pInstanceIDs, uint32_t unCountInstanceIDs);
typedef bool (*fn_ISteamInventory_GenerateItems_t)(void* self, SteamInventoryResult_t* pResultHandle, const SteamItemDef_t* pArrayItemDefs, const uint32_t* punArrayQuantity, uint32_t unArrayLength);
typedef bool (*fn_ISteamInventory_ConsumeItem_t)(void* self, SteamInventoryResult_t* pResultHandle, SteamItemInstanceID_t itemConsume, uint32_t unQuantity);
typedef bool (*fn_ISteamInventory_ExchangeItems_t)(void* self, SteamInventoryResult_t* pResultHandle, const SteamItemDef_t* pArrayGenerate, const uint32_t* punArrayGenerateQuantity, uint32_t unArrayGenerateLength, const SteamItemInstanceID_t* pArrayDestroy, const uint32_t* punArrayDestroyQuantity, uint32_t unArrayDestroyLength);
typedef bool (*fn_ISteamInventory_TriggerItemDrop_t)(void* self, SteamInventoryResult_t* pResultHandle, SteamItemDef_t dropListDefinition);
typedef bool (*fn_ISteamInventory_LoadItemDefinitions_t)(void* self);
typedef bool (*fn_ISteamInventory_GetItemDefinitionIDs_t)(void* self, SteamItemDef_t* pItemDefIDs, uint32_t* punItemDefIDsArraySize);
typedef bool (*fn_ISteamInventory_GetItemDefinitionProperty_t)(void* self, SteamItemDef_t iDefinition, const char* pchPropertyName, char* pchValueBuffer, uint32_t* punValueBufferSizeOut);
typedef SteamAPICall_t (*fn_ISteamInventory_StartPurchase_t)(void* self, const SteamItemDef_t* pArrayItemDefs, const uint32_t* punArrayQuantity, uint32_t unArrayLength);

static fn_ISteamInventory_GetResultStatus_t g_orig_GetResultStatus = nullptr;
static fn_ISteamInventory_GetResultItems_t g_orig_GetResultItems = nullptr;
static fn_ISteamInventory_GetResultTimestamp_t g_orig_GetResultTimestamp = nullptr;
static fn_ISteamInventory_DestroyResult_t g_orig_DestroyResult = nullptr;
static fn_ISteamInventory_GetAllItems_t g_orig_GetAllItems = nullptr;
static fn_ISteamInventory_GetItemsByID_t g_orig_GetItemsByID = nullptr;
static fn_ISteamInventory_GenerateItems_t g_orig_GenerateItems = nullptr;
static fn_ISteamInventory_ConsumeItem_t g_orig_ConsumeItem = nullptr;
static fn_ISteamInventory_ExchangeItems_t g_orig_ExchangeItems = nullptr;
static fn_ISteamInventory_TriggerItemDrop_t g_orig_TriggerItemDrop = nullptr;
static fn_ISteamInventory_LoadItemDefinitions_t g_orig_LoadItemDefinitions = nullptr;
static fn_ISteamInventory_GetItemDefinitionIDs_t g_orig_GetItemDefinitionIDs = nullptr;
static fn_ISteamInventory_GetItemDefinitionProperty_t g_orig_GetItemDefinitionProperty = nullptr;
static fn_ISteamInventory_StartPurchase_t g_orig_StartPurchase = nullptr;

static EResult Hooked_VTable_GetResultStatus(void* self, SteamInventoryResult_t resultHandle) {
    if (g_enableLocalInventory) {
        return GetResultStatus(resultHandle);
    }
    if (g_orig_GetResultStatus) return g_orig_GetResultStatus(self, resultHandle);
    return k_EResultOK;
}

static bool Hooked_VTable_GetResultItems(void* self, SteamInventoryResult_t resultHandle, SteamItemDetails_t* pOutItemsArray, uint32_t* punOutItemsArraySize) {
    if (g_enableLocalInventory) {
        return GetResultItems(resultHandle, pOutItemsArray, punOutItemsArraySize);
    }
    if (g_orig_GetResultItems) return g_orig_GetResultItems(self, resultHandle, pOutItemsArray, punOutItemsArraySize);
    return false;
}

static uint32_t Hooked_VTable_GetResultTimestamp(void* self, SteamInventoryResult_t resultHandle) {
    if (g_enableLocalInventory) {
        return GetResultTimestamp(resultHandle);
    }
    if (g_orig_GetResultTimestamp) return g_orig_GetResultTimestamp(self, resultHandle);
    return (uint32_t)time(nullptr);
}

static void Hooked_VTable_DestroyResult(void* self, SteamInventoryResult_t resultHandle) {
    if (g_enableLocalInventory) {
        DestroyResult(resultHandle);
        return;
    }
    if (g_orig_DestroyResult) g_orig_DestroyResult(self, resultHandle);
}

static bool Hooked_VTable_GetAllItems(void* self, SteamInventoryResult_t* pResultHandle) {
    if (g_enableLocalInventory) {
        return GetAllItems(pResultHandle);
    }
    if (g_orig_GetAllItems) return g_orig_GetAllItems(self, pResultHandle);
    return false;
}

static bool Hooked_VTable_GetItemsByID(void* self, SteamInventoryResult_t* pResultHandle, const SteamItemInstanceID_t* pInstanceIDs, uint32_t unCountInstanceIDs) {
    if (g_enableLocalInventory) {
        return GetItemsByID(pResultHandle, pInstanceIDs, unCountInstanceIDs);
    }
    if (g_orig_GetItemsByID) return g_orig_GetItemsByID(self, pResultHandle, pInstanceIDs, unCountInstanceIDs);
    return false;
}

static bool Hooked_VTable_GenerateItems(void* self, SteamInventoryResult_t* pResultHandle, const SteamItemDef_t* pArrayItemDefs, const uint32_t* punArrayQuantity, uint32_t unArrayLength) {
    if (g_enableLocalInventory) {
        return GenerateItems(pResultHandle, pArrayItemDefs, punArrayQuantity, unArrayLength);
    }
    if (g_orig_GenerateItems) return g_orig_GenerateItems(self, pResultHandle, pArrayItemDefs, punArrayQuantity, unArrayLength);
    return false;
}

static bool Hooked_VTable_ConsumeItem(void* self, SteamInventoryResult_t* pResultHandle, SteamItemInstanceID_t itemConsume, uint32_t unQuantity) {
    if (g_enableLocalInventory) {
        return ConsumeItem(pResultHandle, itemConsume, unQuantity);
    }
    if (g_orig_ConsumeItem) return g_orig_ConsumeItem(self, pResultHandle, itemConsume, unQuantity);
    return false;
}

static bool Hooked_VTable_ExchangeItems(void* self, SteamInventoryResult_t* pResultHandle, const SteamItemDef_t* pArrayGenerate, const uint32_t* punArrayGenerateQuantity, uint32_t unArrayGenerateLength, const SteamItemInstanceID_t* pArrayDestroy, const uint32_t* punArrayDestroyQuantity, uint32_t unArrayDestroyLength) {
    if (g_enableLocalInventory) {
        return ExchangeItems(pResultHandle, pArrayGenerate, punArrayGenerateQuantity, unArrayGenerateLength, pArrayDestroy, punArrayDestroyQuantity, unArrayDestroyLength);
    }
    if (g_orig_ExchangeItems) return g_orig_ExchangeItems(self, pResultHandle, pArrayGenerate, punArrayGenerateQuantity, unArrayGenerateLength, pArrayDestroy, punArrayDestroyQuantity, unArrayDestroyLength);
    return false;
}

static bool Hooked_VTable_TriggerItemDrop(void* self, SteamInventoryResult_t* pResultHandle, SteamItemDef_t dropListDefinition) {
    if (g_enableLocalInventory) {
        return TriggerItemDrop(pResultHandle, dropListDefinition);
    }
    if (g_orig_TriggerItemDrop) return g_orig_TriggerItemDrop(self, pResultHandle, dropListDefinition);
    return false;
}

static bool Hooked_VTable_LoadItemDefinitions(void* self) {
    if (g_enableLocalInventory) {
        return LoadItemDefinitions();
    }
    if (g_orig_LoadItemDefinitions) return g_orig_LoadItemDefinitions(self);
    return false;
}

static bool Hooked_VTable_GetItemDefinitionIDs(void* self, SteamItemDef_t* pItemDefIDs, uint32_t* punItemDefIDsArraySize) {
    if (g_enableLocalInventory) {
        return GetItemDefinitionIDs(pItemDefIDs, punItemDefIDsArraySize);
    }
    if (g_orig_GetItemDefinitionIDs) return g_orig_GetItemDefinitionIDs(self, pItemDefIDs, punItemDefIDsArraySize);
    return false;
}

static bool Hooked_VTable_GetItemDefinitionProperty(void* self, SteamItemDef_t iDefinition, const char* pchPropertyName, char* pchValueBuffer, uint32_t* punValueBufferSizeOut) {
    if (g_enableLocalInventory) {
        return GetItemDefinitionProperty(iDefinition, pchPropertyName, pchValueBuffer, punValueBufferSizeOut);
    }
    if (g_orig_GetItemDefinitionProperty) return g_orig_GetItemDefinitionProperty(self, iDefinition, pchPropertyName, pchValueBuffer, punValueBufferSizeOut);
    return false;
}

static SteamAPICall_t Hooked_VTable_StartPurchase(void* self, const SteamItemDef_t* pArrayItemDefs, const uint32_t* punArrayQuantity, uint32_t unArrayLength) {
    if (g_enableStoreEmulation) {
        uint64_t orderId = 0;
        return ProcessStartPurchase(pArrayItemDefs, punArrayQuantity, unArrayLength, orderId);
    }
    if (g_orig_StartPurchase) return g_orig_StartPurchase(self, pArrayItemDefs, punArrayQuantity, unArrayLength);
    return 0;
}

void HookISteamInventory(void* pInterface, const char* pszVersion) {
    if (!pInterface) return;
    ReFixLog("[Inventory] Installing VTable hooks on ISteamInventory interface %p (version='%s')",
             pInterface, pszVersion ? pszVersion : "unknown");

    HookVTableMethod(pInterface, 0, (void*)Hooked_VTable_GetResultStatus, (void**)&g_orig_GetResultStatus);
    HookVTableMethod(pInterface, 1, (void*)Hooked_VTable_GetResultItems, (void**)&g_orig_GetResultItems);
    HookVTableMethod(pInterface, 3, (void*)Hooked_VTable_GetResultTimestamp, (void**)&g_orig_GetResultTimestamp);
    HookVTableMethod(pInterface, 5, (void*)Hooked_VTable_DestroyResult, (void**)&g_orig_DestroyResult);
    HookVTableMethod(pInterface, 6, (void*)Hooked_VTable_GetAllItems, (void**)&g_orig_GetAllItems);
    HookVTableMethod(pInterface, 7, (void*)Hooked_VTable_GetItemsByID, (void**)&g_orig_GetItemsByID);
    HookVTableMethod(pInterface, 10, (void*)Hooked_VTable_GenerateItems, (void**)&g_orig_GenerateItems);
    HookVTableMethod(pInterface, 14, (void*)Hooked_VTable_ConsumeItem, (void**)&g_orig_ConsumeItem);
    HookVTableMethod(pInterface, 15, (void*)Hooked_VTable_ExchangeItems, (void**)&g_orig_ExchangeItems);
    HookVTableMethod(pInterface, 18, (void*)Hooked_VTable_TriggerItemDrop, (void**)&g_orig_TriggerItemDrop);
    HookVTableMethod(pInterface, 20, (void*)Hooked_VTable_LoadItemDefinitions, (void**)&g_orig_LoadItemDefinitions);
    HookVTableMethod(pInterface, 21, (void*)Hooked_VTable_GetItemDefinitionIDs, (void**)&g_orig_GetItemDefinitionIDs);
    HookVTableMethod(pInterface, 22, (void*)Hooked_VTable_GetItemDefinitionProperty, (void**)&g_orig_GetItemDefinitionProperty);
    HookVTableMethod(pInterface, 25, (void*)Hooked_VTable_StartPurchase, (void**)&g_orig_StartPurchase);

    ReFixLog("[Inventory] VTable hooks for ISteamInventory installed successfully");
}

} // namespace ReFixInventory
