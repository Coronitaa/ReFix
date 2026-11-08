// Valve Economy Schema & Loot Generation: items_game.txt parser, paint kits, loot lists
// Generic across CS:GO, TF2, and other Valve Econ titles
#pragma once

#include "../include/refix_services.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace refix::econ {

class EconSchema : public refix::IEconSchema {
public:
    static EconSchema& Get();

    EconSchema();
    virtual ~EconSchema() override = default;

    virtual bool LoadSchema(const char* items_game_path) override;
    virtual const EconItemDefinition* FindItemDef(uint32_t def_index) override;
    virtual const std::vector<EconItemDefinition>& GetAllItems() override;
    virtual const std::vector<std::string>& GetCategories() override;
    virtual std::vector<EconItemDefinition> GetItemsByCategory(const char* category) override;
    virtual uint32_t RollLoot(const char* loot_list_name, float* out_wear, uint32_t* out_seed, uint32_t* out_paint_kit) override;

    void AddItemDef(const EconItemDefinition& item);

private:
    std::mutex m_mu;
    std::vector<EconItemDefinition> m_items;
    std::unordered_map<uint32_t, size_t> m_itemIndexByDef;
    std::vector<std::string> m_categories;
    bool m_loaded = false;
};

} // namespace refix::econ
