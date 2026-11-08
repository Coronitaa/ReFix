#include "econ_schema.h"
#include <algorithm>
#include <cstdlib>

namespace refix::econ {

EconSchema& EconSchema::Get() {
    static EconSchema s_instance;
    return s_instance;
}

EconSchema::EconSchema() {
    refix::GetRefixServices().econ = this;
}

void EconSchema::AddItemDef(const EconItemDefinition& item) {
    std::lock_guard<std::mutex> lock(m_mu);
    size_t idx = m_items.size();
    m_items.push_back(item);
    m_itemIndexByDef[item.defIndex] = idx;

    if (!item.category.empty()) {
        if (std::find(m_categories.begin(), m_categories.end(), item.category) == m_categories.end()) {
            m_categories.push_back(item.category);
        }
    }
}

bool EconSchema::LoadSchema(const char* items_game_path) {
    std::lock_guard<std::mutex> lock(m_mu);
    m_loaded = true;
    return true;
}

const EconItemDefinition* EconSchema::FindItemDef(uint32_t def_index) {
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = m_itemIndexByDef.find(def_index);
    if (it != m_itemIndexByDef.end()) {
        return &m_items[it->second];
    }
    return nullptr;
}

const std::vector<EconItemDefinition>& EconSchema::GetAllItems() {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_items;
}

const std::vector<std::string>& EconSchema::GetCategories() {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_categories;
}

std::vector<EconItemDefinition> EconSchema::GetItemsByCategory(const char* category) {
    std::lock_guard<std::mutex> lock(m_mu);
    std::vector<EconItemDefinition> res;
    if (!category) return res;
    std::string targetCat = category;
    for (const auto& itm : m_items) {
        if (itm.category == targetCat) {
            res.push_back(itm);
        }
    }
    return res;
}

uint32_t EconSchema::RollLoot(const char* loot_list_name, float* out_wear, uint32_t* out_seed, uint32_t* out_paint_kit) {
    // Standard Valve wear roll: (rand() / RAND_MAX)
    if (out_wear) {
        *out_wear = (float)rand() / (float)RAND_MAX;
    }
    if (out_seed) {
        *out_seed = (rand() % 1000) + 1;
    }
    if (out_paint_kit) {
        *out_paint_kit = 0;
    }
    return 0;
}

} // namespace refix::econ
