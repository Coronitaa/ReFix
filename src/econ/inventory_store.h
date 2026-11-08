// IInventoryStore service: thin adapter over the GC emulator's persisted inventory, so native mods and the
// built-in GC handlers read and write the same items (one inventory.json per ReFix profile).
#pragma once

#include "../include/refix_services.h"
#include <cstdint>
#include <vector>

namespace refix::econ {

// shortcut: only the local player's inventory exists; steam_id is ignored. Upgrade when remote caches are relayed.
class InventoryStore : public refix::IInventoryStore {
public:
    static InventoryStore& Get();

    InventoryStore();
    virtual ~InventoryStore() override = default;

    virtual bool Load(uint64_t steam_id) override;
    virtual bool Save(uint64_t steam_id) override;
    virtual std::vector<InventoryItem> GetItems(uint64_t steam_id) override;
    virtual uint64_t GrantItem(uint64_t steam_id, uint32_t defIndex, uint32_t level, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed, const char* customName = nullptr) override;
    virtual bool SetEquipped(uint64_t steam_id, uint64_t item_id, uint32_t team, uint32_t slot) override;
    virtual uint64_t AllocateItemId(uint64_t steam_id) override;
};

} // namespace refix::econ
