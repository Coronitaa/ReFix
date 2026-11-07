// Valve Economy Inventory Persistence: JSON profile inventory, persistent 64-bit IDs, equipment
#pragma once

#include "../include/refix_services.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>

namespace refix::econ {

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

    static uint64_t ComposeItemId(uint32_t account_id, uint32_t low);

private:
    std::string GetInventoryFilePath(uint64_t steam_id) const;

    std::mutex m_mu;
    std::unordered_map<uint64_t, std::vector<InventoryItem>> m_profiles;
    std::unordered_map<uint64_t, uint32_t> m_lastLowIds;
};

} // namespace refix::econ
