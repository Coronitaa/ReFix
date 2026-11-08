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
    std::string name;     // game-language name (Spanish tokens over English)
    std::string nameEn;   // English name, for the overlay in English
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
    uint32_t stickerKit = 0;  // sticker / graffiti / patch kit ("sticker slot 0 id")
    uint32_t musicKit = 0;    // music kit ("music id")
    std::string pattern;      // paint kit texture name (materials/.../customization/paints/*/<pattern>.vtf)
    int seasonAccess = -1;    // operation pass: the season it unlocks ("season access" attribute)
};

// One case/capsule roll: a skin (paintKit), a sticker/graffiti/patch (stickerKit) or a music kit (musicKit).
struct CaseDrop {
    uint32_t defIndex = 0;
    uint32_t paintKit = 0;
    uint32_t stickerKit = 0;
    uint32_t musicKit = 0;
    uint32_t rarity = 0;
    uint32_t quality = 4;
    float wear = 0.001f;
    uint32_t seed = 0;
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
    bool RollCaseDrop(uint32_t caseDefIndex, CaseDrop& out) const;

    const std::vector<MarketItem>& GetMarketItems() const { return m_marketItems; }
    std::vector<MarketItem> GetMarketItemsByCategory(const std::string& category) const;
    const std::vector<CategoryInfo>& GetCategories() const { return m_categories; }
    size_t GetTotalGrantableCount() const { return m_marketItems.size(); }

    std::string GetWeaponDisplayName(uint32_t defIndex) const;
    uint32_t FindDefByName(const std::string& itemName) const;   // items_game "name" -> def index, 0 if absent
    std::string GetItemName(uint32_t defIndex) const;            // def index -> items_game "name"
    uint32_t GetMissionCardCount(uint32_t season) const;         // weeks of an operation (quest_mission_card entries)
    // Points to complete a quest and the stars (operational_points) it awards; false for an unknown quest.
    bool GetQuestGoal(uint32_t questId, uint32_t& points, uint32_t& stars) const;
    std::string GetPaintKitDisplayName(uint32_t paintKit) const;
    std::string ResolveToken(const std::string& token) const;
    std::string ResolveTokenEn(const std::string& token) const;  // English only

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
    std::unordered_map<std::string, std::string> m_tokensEn;
    std::unordered_map<uint32_t, uint32_t> m_missionCards;  // season -> mission card count
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> m_questGoals;  // quest id -> (points, stars)
};

} // namespace refix::csgo
