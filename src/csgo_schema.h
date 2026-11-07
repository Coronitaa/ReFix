// Portions of this file are derived from csgo_gc:
// Copyright (c) 2024-2026, Mikko Kokko
// Copyright (c) 2024-2026, Theeto
// Licensed under the BSD-2-Clause License (see THIRD_PARTY_NOTICES)

#pragma once

#include "csgo_schema_types.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace refix::csgo {

struct SkinDrop {
    uint32_t defIndex = 0;
    uint32_t paintKit = 0;
    uint32_t rarity = 0;
    std::string weaponName;
    std::string skinName;
};

struct CaseDropTable {
    uint32_t caseDefIndex = 0;
    std::string caseName;
    std::string displayName;
    std::vector<SkinDrop> rare;      // Mil-Spec (Rarity 3)
    std::vector<SkinDrop> mythical;  // Restricted (Rarity 4)
    std::vector<SkinDrop> legendary; // Classified (Rarity 5)
    std::vector<SkinDrop> ancient;   // Covert (Rarity 6)
    std::vector<SkinDrop> special;   // Knives / Gloves (Rarity 7/99, Quality 3)
};

struct MarketItem {
    uint32_t defIndex = 0;
    std::string name;
    std::string category; // "Rifles", "Pistolas", "Subfusiles", "Pesadas", "Cuchillos", "Guantes", "Agentes", "Cajas", "Llaves", "Música y Coleccionables"
    uint32_t quality = 4;
    uint32_t rarity = 0;
    uint32_t paintKit = 0;
    float wear = 0.001f;
    uint32_t seed = 0;
    float minWear = 0.0f;
    float maxWear = 1.0f;
    std::string weaponName;
    std::string skinName;
    std::string imageInventory;
};

struct CategoryInfo {
    std::string name;
    size_t count = 0;
};

class SchemaManager {
public:
    static SchemaManager& Get();

    bool Load();
    bool IsLoaded() const { return m_loaded; }

    const CaseDropTable* GetCaseDrops(uint32_t caseDefIndex) const;
    bool RollCaseDrop(uint32_t caseDefIndex, uint32_t& outDef, uint32_t& outPaintKit, uint32_t& outRarity, uint32_t& outQuality, float& outWear, uint32_t& outSeed) const;

    const std::vector<MarketItem>& GetMarketItems() const { return m_marketItems; }
    std::vector<MarketItem> GetMarketItemsByCategory(const std::string& category) const;
    const std::vector<CategoryInfo>& GetCategories() const { return m_categories; }
    size_t GetTotalGrantableCount() const { return m_marketItems.size(); }

    std::string GetWeaponDisplayName(uint32_t defIndex) const;
    std::string GetPaintKitDisplayName(uint32_t paintKit) const;
    std::string ResolveToken(const std::string& token) const;

private:
    SchemaManager();
    ~SchemaManager();

    class Impl;
    std::unique_ptr<Impl> m_impl;

    bool m_loaded = false;
    std::unordered_map<std::string, uint32_t> m_weaponNameToDef;
    std::unordered_map<uint32_t, std::string> m_defToWeaponName;
    std::unordered_map<std::string, uint32_t> m_paintKitNameToId;
    std::unordered_map<uint32_t, std::string> m_paintKitIdToName;
    std::unordered_map<uint32_t, CaseDropTable> m_caseTables;
    std::vector<MarketItem> m_marketItems;
    std::vector<CategoryInfo> m_categories;
    std::vector<SkinDrop> m_universalSpecialKnives;
    std::unordered_map<std::string, std::string> m_tokens;
};

} // namespace refix::csgo
