// Versioned ReFix Mod Services Interface (ABI)
// Exposes generic host capabilities to official native and sandboxed mods.
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace refix {

// ---------------------------------------------------------------------------
// 1. IGcCore: Valve GameCoordinator routing, SOCache, and multi-peer relay
// ---------------------------------------------------------------------------
typedef std::function<void(const uint8_t* payload, size_t size, bool is_protobuf)> GcMessageHandlerFn;

struct IGcCore {
    virtual ~IGcCore() = default;
    virtual void RegisterClientHandler(uint32_t msg_type, GcMessageHandlerFn fn) = 0;
    virtual void RegisterServerHandler(uint32_t msg_type, GcMessageHandlerFn fn) = 0;
    virtual void SendClient(uint32_t msg_type, const void* data, size_t size, bool is_protobuf) = 0;
    virtual void SendServer(uint32_t msg_type, const void* data, size_t size, bool is_protobuf) = 0;
    virtual void SendSOCacheSubscribed(uint64_t target_steam_id, const void* items, size_t count) = 0;
    virtual void SendSingleObject(uint64_t steam_id, int type_id, const void* data, size_t size) = 0;
    virtual uint64_t GetActiveSteamID() = 0;
    virtual uint32_t GetActiveAppID() = 0;
    virtual void RelayCacheToServer(uint64_t remote_steam_id) = 0;
};

// ---------------------------------------------------------------------------
// 2. IEconSchema: Generic schema definition, paint kits, qualities, loot
// ---------------------------------------------------------------------------
struct EconItemDefinition {
    uint32_t defIndex = 0;
    std::string name;
    std::string itemType;
    std::string itemTypeName;
    std::string category;
    uint32_t rarity = 0;
    uint32_t quality = 4;
    uint32_t paintKit = 0;
    float wear = 0.001f;
    uint32_t seed = 1;
};

struct IEconSchema {
    virtual ~IEconSchema() = default;
    virtual bool LoadSchema(const char* items_game_path) = 0;
    virtual const EconItemDefinition* FindItemDef(uint32_t def_index) = 0;
    virtual const std::vector<EconItemDefinition>& GetAllItems() = 0;
    virtual const std::vector<std::string>& GetCategories() = 0;
    virtual std::vector<EconItemDefinition> GetItemsByCategory(const char* category) = 0;
    virtual uint32_t RollLoot(const char* loot_list_name, float* out_wear, uint32_t* out_seed, uint32_t* out_paint_kit) = 0;
};

// ---------------------------------------------------------------------------
// 3. IInventoryStore: Inventory JSON persistence, IDs, equipment
// ---------------------------------------------------------------------------
struct EquippedState {
    uint32_t team = 0;
    uint32_t slot = 0;
};

struct InventoryItem {
    uint64_t id = 0;
    uint32_t defIndex = 0;
    uint32_t level = 1;
    uint32_t quality = 4;
    uint32_t rarity = 0;
    uint32_t paintKit = 0;
    float wear = 0.0f;
    uint32_t seed = 0;
    std::string customName;
    std::vector<EquippedState> equipped;
};

struct IInventoryStore {
    virtual ~IInventoryStore() = default;
    virtual bool Load(uint64_t steam_id) = 0;
    virtual bool Save(uint64_t steam_id) = 0;
    virtual std::vector<InventoryItem> GetItems(uint64_t steam_id) = 0;
    virtual uint64_t GrantItem(uint64_t steam_id, uint32_t defIndex, uint32_t level, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed, const char* customName = nullptr) = 0;
    virtual bool SetEquipped(uint64_t steam_id, uint64_t item_id, uint32_t team, uint32_t slot) = 0;
    virtual uint64_t AllocateItemId(uint64_t steam_id) = 0;
};

// ---------------------------------------------------------------------------
// 4. ISourceEngine: Safe command execution (R5) and engine interface access
// ---------------------------------------------------------------------------
struct ISourceEngine {
    virtual ~ISourceEngine() = default;
    virtual void EnqueueCommand(const char* command) = 0;
    virtual void* FindInterface(const char* module_name, const char* interface_name) = 0;
};

// ---------------------------------------------------------------------------
// 5. IPanorama: Script hook injection and custom XML/JS UI integration
// ---------------------------------------------------------------------------
typedef std::function<void()> PanoramaScriptHookFn;

struct IPanorama {
    virtual ~IPanorama() = default;
    virtual void RegisterScriptInjection(const char* target_script, const char* injected_code) = 0;
    virtual void RegisterOnScriptRun(PanoramaScriptHookFn fn) = 0;
    virtual void RunScript(const char* script) = 0;
};

// ---------------------------------------------------------------------------
// 6. IOverlayActions: Mod overlay cards, modals, and navigation
// ---------------------------------------------------------------------------
typedef std::function<void(float width, float height)> OverlayRenderFn;

struct IOverlayActions {
    virtual ~IOverlayActions() = default;
    virtual void RegisterCard(const char* id, const char* title, const char* subtitle, OverlayRenderFn render_fn) = 0;
    virtual void RegisterModal(const char* id, OverlayRenderFn render_fn) = 0;
    virtual void OpenOverlay() = 0;
    virtual void CloseOverlay() = 0;
    virtual void ToggleOverlay() = 0;
    virtual void OpenModal(const char* id) = 0;
    virtual void CloseModal(const char* id) = 0;
};

// ---------------------------------------------------------------------------
// 7. IEntitlements: Game entitlements (Prime, passes, DLC) and purchase prompts
// ---------------------------------------------------------------------------
typedef std::function<void(bool approved)> EntitlementCallbackFn;

struct IEntitlements {
    virtual ~IEntitlements() = default;
    virtual bool HasEntitlement(uint64_t steam_id, uint32_t entitlement_id) = 0;
    virtual void GrantEntitlement(uint64_t steam_id, uint32_t entitlement_id) = 0;
    virtual void PromptPurchase(uint32_t app_id, uint32_t item_id, const char* name, const char* description, bool requires_elevation, EntitlementCallbackFn callback) = 0;
};

// ---------------------------------------------------------------------------
// Host Services Container
// ---------------------------------------------------------------------------
struct IRefixServices {
    uint32_t version = 1;
    IGcCore* gc = nullptr;
    IEconSchema* econ = nullptr;
    IInventoryStore* inventory = nullptr;
    ISourceEngine* engine = nullptr;
    IPanorama* panorama = nullptr;
    IOverlayActions* overlay = nullptr;
    IEntitlements* entitlements = nullptr;
};

// Singleton host access
IRefixServices& GetRefixServices();

} // namespace refix
