#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace refix::csgo {

struct SkinDrop {
    uint32_t defIndex;
    uint32_t paintKit;
    uint32_t rarity;
    std::string weaponName;
    std::string skinName;
};

struct CaseDropTable {
    uint32_t caseDefIndex;
    std::string caseName;
    std::string displayName;
    std::vector<SkinDrop> rare;      // Mil-Spec (Rarity 3)
    std::vector<SkinDrop> mythical;  // Restricted (Rarity 4)
    std::vector<SkinDrop> legendary; // Classified (Rarity 5)
    std::vector<SkinDrop> ancient;   // Covert (Rarity 6)
    std::vector<SkinDrop> special;   // Knives / Gloves (Rarity 7, Quality 3)
};

struct MarketItem {
    uint32_t defIndex;
    std::string name;
    std::string category; // "Caja", "Llave", "Rifle", "Pistola", "Subfusil", "Pesada", "Cuchillo", "Guantes", "Pegatina"
    uint32_t quality;
    uint32_t rarity;
    uint32_t paintKit;
    float wear;
    uint32_t seed;
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

    std::string GetWeaponDisplayName(uint32_t defIndex) const;
    std::string GetPaintKitDisplayName(uint32_t paintKit) const;

private:
    SchemaManager() = default;
    bool LoadFromItemsGameFile(const std::string& filePath);
    void BuildFallbackSchema();

    bool m_loaded = false;
    std::unordered_map<std::string, uint32_t> m_weaponNameToDef;
    std::unordered_map<uint32_t, std::string> m_defToWeaponName;
    std::unordered_map<std::string, uint32_t> m_paintKitNameToId;
    std::unordered_map<uint32_t, std::string> m_paintKitIdToName;
    std::unordered_map<std::string, uint32_t> m_paintKitRarity;
    std::unordered_map<uint32_t, CaseDropTable> m_caseTables;
    std::vector<MarketItem> m_marketItems;
    std::vector<SkinDrop> m_universalSpecialKnives;
};

} // namespace refix::csgo
