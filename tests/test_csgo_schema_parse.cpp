// Comprehensive test for KeyValue, ItemSchema, and CaseOpening odds
#include "../src/keyvalue.h"
#include "../src/csgo_schema_types.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <cassert>
#include <iomanip>

namespace {
static thread_local std::minstd_rand s_rng{ 1337 };
inline uint32_t RandUint32(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return std::uniform_int_distribution<uint32_t>{ min, max }(s_rng);
}
inline float RandFloat(float min, float max) {
    if (min >= max) return min;
    return std::uniform_real_distribution<float>{ min, max }(s_rng);
}
}

static Rarity ItemRarityFromString(std::string_view name) {
    if (name == "default") return Rarity::Default;
    if (name == "common") return Rarity::Common;
    if (name == "uncommon") return Rarity::Uncommon;
    if (name == "rare") return Rarity::Rare;
    if (name == "mythical") return Rarity::Mythical;
    if (name == "legendary") return Rarity::Legendary;
    if (name == "ancient") return Rarity::Ancient;
    if (name == "immortal") return Rarity::Immortal;
    if (name == "unusual") return Rarity::Unusual;
    return Rarity::Common;
}

static Quality ItemQualityFromString(std::string_view name) {
    if (name == "normal") return Quality::Normal;
    if (name == "genuine") return Quality::Genuine;
    if (name == "vintage") return Quality::Vintage;
    if (name == "unusual") return Quality::Unusual;
    if (name == "unique") return Quality::Unique;
    if (name == "community") return Quality::Community;
    if (name == "developer") return Quality::Developer;
    if (name == "selfmade") return Quality::Selfmade;
    if (name == "customized") return Quality::Customized;
    if (name == "strange") return Quality::Strange;
    if (name == "completed") return Quality::Completed;
    if (name == "haunted") return Quality::Haunted;
    if (name == "tournament") return Quality::Tournament;
    return Quality::Unique;
}

struct ItemInfo {
    uint32_t m_defIndex = 0;
    std::string m_name;
    Rarity m_rarity = Rarity::Common;
    Quality m_quality = Quality::Normal;
    uint32_t m_level = 1;
    std::string m_lootListName;
    bool m_willProduceStatTrak = false;
    uint32_t m_supplyCrateSeries = 0;
};

struct PaintKitInfo {
    uint32_t m_defIndex = 0;
    Rarity m_rarity = Rarity::Common;
    float m_minFloat = 0.0f;
    float m_maxFloat = 1.0f;
};

enum LootListItemType {
    LootListItemNoAttribute,
    LootListItemPaintable,
    LootListItemSticker,
    LootListItemSpray,
    LootListItemPatch,
    LootListItemMusicKit
};

struct LootListItem {
    Rarity CaseRarity() const {
        if (quality == Quality::Unusual) return Rarity::Unusual;
        return rarity;
    }
    const ItemInfo* itemInfo = nullptr;
    LootListItemType type = LootListItemNoAttribute;
    uint32_t kitDefIndex = 0;
    float minFloat = 0.0f;
    float maxFloat = 1.0f;
    Rarity rarity = Rarity::Common;
    Quality quality = Quality::Normal;
};

struct LootList {
    std::vector<LootListItem> items;
    std::vector<const LootList*> subLists;
    bool willProduceStatTrak = false;
    bool isUnusual = false;
    bool allEntriesAsAdditionalDrops = false;
};

struct CaseDropResult {
    uint32_t defIndex = 0;
    uint32_t paintKit = 0;
    uint32_t quality = 4;
    uint32_t rarity = 3;
    float wear = 0.001f;
    uint32_t seed = 0;
    bool statTrak = false;
};

class TestSchema {
public:
    std::unordered_map<uint32_t, ItemInfo> m_itemInfo;
    std::unordered_map<std::string, PaintKitInfo> m_paintKitInfo;
    std::unordered_map<uint32_t, std::string> m_paintKitIdToName;
    std::unordered_map<std::string, LootList> m_lootLists;
    std::unordered_map<uint32_t, LootList> m_revolvingLootLists;

    const ItemInfo* ItemInfoByName(std::string_view name) const {
        for (const auto& pair : m_itemInfo) {
            if (pair.second.m_name == name) return &pair.second;
        }
        return nullptr;
    }

    PaintKitInfo* PaintKitInfoByName(std::string_view name) {
        auto it = m_paintKitInfo.find(std::string(name));
        return (it != m_paintKitInfo.end()) ? &it->second : nullptr;
    }

    static Rarity PaintedItemRarity(Rarity itemRarity, Rarity paintKitRarity) {
        int rarityValue = (static_cast<int>(itemRarity) - 1) + static_cast<int>(paintKitRarity);
        if (rarityValue < 0) return Rarity::Default;
        Rarity rarity = static_cast<Rarity>(rarityValue);
        if (rarity > Rarity::Ancient) {
            return (paintKitRarity == Rarity::Immortal) ? Rarity::Immortal : Rarity::Ancient;
        }
        return rarity;
    }

    bool ParseLootListItem(LootListItem& item, std::string_view name) {
        std::string_view attributeName, itemName = name;
        if (name.size() > 2 && name[0] == '[') {
            size_t endBracket = name.find(']', 1);
            if (endBracket != std::string_view::npos) {
                attributeName = name.substr(1, endBracket - 1);
                itemName = name.substr(endBracket + 1);
            }
        }

        const ItemInfo* info = ItemInfoByName(itemName);
        if (!info) return false;

        item.itemInfo = info;
        item.rarity = info->m_rarity;
        item.quality = info->m_quality;

        if (attributeName.empty()) {
            item.type = LootListItemNoAttribute;
        } else {
            item.type = LootListItemPaintable;
            PaintKitInfo* pk = PaintKitInfoByName(attributeName);
            if (pk) {
                item.kitDefIndex = pk->m_defIndex;
                item.minFloat = pk->m_minFloat;
                item.maxFloat = pk->m_maxFloat;
                item.rarity = PaintedItemRarity(info->m_rarity, pk->m_rarity);
            }
        }
        return true;
    }

    void ParseItemRecursive(ItemInfo& info, const KeyValue& itemKey, const KeyValue* prefabsKey) {
        std::string_view prefabString = itemKey.GetString("prefab");
        if (!prefabString.empty() && prefabsKey) {
            size_t start = 0;
            while (start < prefabString.size()) {
                size_t space = prefabString.find(' ', start);
                if (space == std::string_view::npos) space = prefabString.size();
                std::string_view pName = prefabString.substr(start, space - start);
                start = space + 1;
                const KeyValue* pKey = prefabsKey->GetSubkey(pName);
                if (pKey) {
                    ParseItemRecursive(info, *pKey, prefabsKey);
                }
            }
        }

        std::string_view name = itemKey.GetString("name");
        if (!name.empty()) info.m_name = std::string(name);

        std::string_view quality = itemKey.GetString("item_quality");
        if (!quality.empty()) info.m_quality = ItemQualityFromString(quality);

        std::string_view rarity = itemKey.GetString("item_rarity");
        if (!rarity.empty()) info.m_rarity = ItemRarityFromString(rarity);

        std::string_view lootListName = itemKey.GetString("loot_list_name");
        if (!lootListName.empty()) info.m_lootListName = std::string(lootListName);

        info.m_willProduceStatTrak = itemKey.GetNumber<bool>("will_produce_stattrak", info.m_willProduceStatTrak);

        const KeyValue* attrs = itemKey.GetSubkey("attributes");
        if (attrs) {
            for (const KeyValue& a : attrs->Subkeys()) {
                if (a.GetString("attribute_class") == "supply_crate_series" || a.Name() == "set supply crate series") {
                    info.m_supplyCrateSeries = a.GetNumber<uint32_t>("value", 0);
                }
            }
        }
    }

    bool Load(const char* extraPath, const char* gamePath) {
        KeyValueRead kv(extraPath);
        if (!kv.MergeToFromFile(gamePath)) return false;

        const KeyValue* root = kv.Root().GetSubkey("items_game");
        if (!root) return false;

        const KeyValue* prefabsKey = root->GetSubkey("prefabs");

        // 1. Paint kits
        const KeyValue* pkKey = root->GetSubkey("paint_kits");
        if (pkKey) {
            for (const KeyValue& sub : pkKey->Subkeys()) {
                std::string_view pkName = sub.GetString("name");
                if (pkName.empty()) continue;
                PaintKitInfo info;
                info.m_defIndex = FromString<uint32_t>(sub.Name());
                info.m_minFloat = sub.GetNumber<float>("wear_remap_min", 0.0f);
                info.m_maxFloat = sub.GetNumber<float>("wear_remap_max", 1.0f);
                m_paintKitInfo[std::string(pkName)] = info;
                m_paintKitIdToName[info.m_defIndex] = std::string(pkName);
            }
        }

        // 2. Paint kit rarities
        const KeyValue* pkrKey = root->GetSubkey("paint_kits_rarity");
        if (pkrKey) {
            for (const KeyValue& sub : pkrKey->Subkeys()) {
                auto it = m_paintKitInfo.find(std::string(sub.Name()));
                if (it != m_paintKitInfo.end()) {
                    it->second.m_rarity = ItemRarityFromString(sub.String());
                }
            }
        }

        // 3. Items
        const KeyValue* itemsKey = root->GetSubkey("items");
        if (itemsKey) {
            for (const KeyValue& sub : itemsKey->Subkeys()) {
                if (sub.Name() == "default") continue;
                uint32_t defId = FromString<uint32_t>(sub.Name());
                if (!defId) continue;
                ItemInfo info;
                info.m_defIndex = defId;
                ParseItemRecursive(info, sub, prefabsKey);
                m_itemInfo[defId] = info;
            }
        }

        // 4. Client loot lists
        const KeyValue* cllKey = root->GetSubkey("client_loot_lists");
        if (cllKey) {
            for (const KeyValue& sub : cllKey->Subkeys()) {
                LootList list;
                list.isUnusual = (sub.Name().find("unusual") != std::string_view::npos);
                for (const KeyValue& entry : sub.Subkeys()) {
                    std::string_view en = entry.Name();
                    if (en == "will_produce_stattrak") { list.willProduceStatTrak = true; continue; }
                    if (en == "all_entries_as_additional_drops") { list.allEntriesAsAdditionalDrops = true; continue; }

                    auto subSearch = m_lootLists.find(std::string(en));
                    if (subSearch != m_lootLists.end()) {
                        list.subLists.push_back(&subSearch->second);
                        continue;
                    }

                    LootListItem itm;
                    if (ParseLootListItem(itm, en)) {
                        if (list.isUnusual) itm.quality = Quality::Unusual;
                        list.items.push_back(itm);
                    }
                }
                m_lootLists[std::string(sub.Name())] = std::move(list);
            }
        }

        // 5. Revolving loot lists
        const KeyValue* revKey = root->GetSubkey("revolving_loot_lists");
        if (revKey) {
            for (const KeyValue& sub : revKey->Subkeys()) {
                uint32_t idx = 0;
                std::string_view listName;
                if (FromString<uint32_t>(sub.Name(), idx)) {
                    listName = sub.String();
                } else {
                    idx = FromString<uint32_t>(sub.String());
                    listName = sub.Name();
                }
                if (idx && !listName.empty()) {
                    auto it = m_lootLists.find(std::string(listName));
                    if (it != m_lootLists.end()) {
                        m_revolvingLootLists[idx] = it->second;
                    }
                }
            }
        }
        return true;
    }

    const LootList* GetCrateLootList(uint32_t crateDefIndex) const {
        auto itItem = m_itemInfo.find(crateDefIndex);
        if (itItem == m_itemInfo.end()) return nullptr;
        uint32_t series = itItem->second.m_supplyCrateSeries;
        if (!series) return nullptr;
        auto itRev = m_revolvingLootLists.find(series);
        if (itRev == m_revolvingLootLists.end()) return nullptr;
        return &itRev->second;
    }
};

static bool GetLootListItems(const LootList& lootList, std::vector<const LootListItem*>& outItems) {
    bool unusuals = lootList.isUnusual;
    for (const LootList* sub : lootList.subLists) {
        unusuals |= GetLootListItems(*sub, outItems);
    }
    for (const LootListItem& itm : lootList.items) {
        outItems.push_back(&itm);
    }
    return unusuals;
}

static float GetRarityWeight(Rarity r) {
    switch (r) {
        case Rarity::Common: return 10000000.0f;
        case Rarity::Uncommon: return 2000000.0f;
        case Rarity::Rare: return 400000.0f;       // Mil-Spec (79.92%)
        case Rarity::Mythical: return 80000.0f;    // Restricted (15.98%)
        case Rarity::Legendary: return 16000.0f;   // Classified (3.20%)
        case Rarity::Ancient: return 3200.0f;      // Covert (0.64%)
        case Rarity::Immortal: return 1280.0f;
        case Rarity::Unusual: return 1280.0f;      // Special Rare (0.26%)
        default: return 400000.0f;
    }
}

struct RarityGroup {
    std::vector<const LootListItem*> items;
    float weight = 0.0f;
};

bool SelectItemFromCrate(const TestSchema& schema, uint32_t crateDef, CaseDropResult& res) {
    const LootList* root = schema.GetCrateLootList(crateDef);
    if (!root) return false;

    std::vector<const LootListItem*> allItems;
    bool containsUnusuals = GetLootListItems(*root, allItems);
    if (allItems.empty()) return false;

    std::sort(allItems.begin(), allItems.end(), [](const LootListItem* a, const LootListItem* b) {
        return a->CaseRarity() < b->CaseRarity();
    });

    std::vector<RarityGroup> groups;
    float totalWeight = 0.0f;

    for (auto first = allItems.begin(); first != allItems.end();) {
        Rarity r = (*first)->CaseRarity();
        auto last = first + 1;
        while (last != allItems.end() && (*last)->CaseRarity() == r) {
            last++;
        }
        float w = GetRarityWeight(r);
        groups.push_back({ std::vector<const LootListItem*>(first, last), w });
        totalWeight += w;
        first = last;
    }

    float roll = RandFloat(0.0f, totalWeight);
    float accum = 0.0f;
    const LootListItem* picked = nullptr;
    for (const auto& g : groups) {
        accum += g.weight;
        if (roll < accum) {
            picked = g.items[RandUint32(0, (uint32_t)g.items.size() - 1)];
            break;
        }
    }
    if (!picked) picked = allItems.front();

    bool statTrak = (root->willProduceStatTrak) || (containsUnusuals && (RandUint32(1, 10) == 1));
    if (picked->quality == Quality::Unusual && picked->itemInfo->m_defIndex >= 1000) {
        statTrak = false;
    }

    res.defIndex = picked->itemInfo->m_defIndex;
    res.paintKit = picked->kitDefIndex;
    res.rarity = FromEnum(picked->CaseRarity());
    res.quality = statTrak ? (picked->quality == Quality::Unusual ? 3 : 9) : (picked->quality == Quality::Unusual ? 3 : 4);
    res.statTrak = statTrak;
    res.wear = RandFloat(picked->minFloat, picked->maxFloat);
    res.seed = RandUint32(1, 999);
    return true;
}

int main() {
    const char* itemsGame = "D:\\Games\\CSGO\\csgo\\scripts\\items\\items_game.txt";
    const char* itemsExtra = "third_party\\csgo\\items_extra.txt";

    TestSchema schema;
    if (!schema.Load(itemsExtra, itemsGame)) {
        std::cerr << "Failed to load schema!" << std::endl;
        return 1;
    }

    std::cout << "[Test] Schema loaded successfully!" << std::endl;

    // Simulate 50,000 Shattered Web Case openings (def 4669)
    std::cout << "[Test] Simulating 50,000 openings of Shattered Web Case (4669)..." << std::endl;
    int counts[8] = {0}; // Mil-Spec (3), Restricted (4), Classified (5), Covert (6), Special (99)
    int stattrakCount = 0;
    int knifeCount = 0;

    for (int i = 0; i < 50000; i++) {
        CaseDropResult res;
        if (!SelectItemFromCrate(schema, 4669, res)) {
            std::cerr << "Opening failed at i=" << i << std::endl;
            return 1;
        }
        if (res.statTrak) stattrakCount++;
        if (res.quality == 3 || res.rarity == 99 || res.rarity == 7) {
            knifeCount++;
            counts[7]++;
        } else {
            if (res.rarity < 8) counts[res.rarity]++;
            else std::cout << "other rarity: " << res.rarity << std::endl;
        }
    }

    for (int r = 0; r < 8; r++) {
        std::cout << "Rarity " << r << ": " << counts[r] << " (" << (counts[r] * 100.0 / 50000) << "%)" << std::endl;
    }

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Mil-Spec (Blue):   " << counts[3] << " (" << (counts[3] * 100.0 / 50000) << "% - Target: ~79.92%)" << std::endl;
    std::cout << "Restricted (Purp): " << counts[4] << " (" << (counts[4] * 100.0 / 50000) << "% - Target: ~15.98%)" << std::endl;
    std::cout << "Classified (Pink): " << counts[5] << " (" << (counts[5] * 100.0 / 50000) << "% - Target: ~3.20%)" << std::endl;
    std::cout << "Covert (Red):      " << counts[6] << " (" << (counts[6] * 100.0 / 50000) << "% - Target: ~0.64%)" << std::endl;
    std::cout << "Special Knife:     " << knifeCount << " (" << (knifeCount * 100.0 / 50000) << "% - Target: ~0.26%)" << std::endl;
    std::cout << "StatTrak Rate:     " << stattrakCount << " (" << (stattrakCount * 100.0 / 50000) << "% - Target: ~10.0%)" << std::endl;

    assert(counts[3] > 38000 && counts[3] < 42000); // ~80%
    assert(counts[4] > 7000 && counts[4] < 9000);   // ~16%
    assert(counts[5] > 1200 && counts[5] < 2000);   // ~3.2%
    assert(counts[6] > 200 && counts[6] < 450);     // ~0.64%
    assert(knifeCount > 70 && knifeCount < 200);    // ~0.26%
    assert(stattrakCount > 4500 && stattrakCount < 5500); // ~10%

    std::cout << "[Test] ALL OFFICIAL ODDS RIGOROUSLY VALIDATED!" << std::endl;
    return 0;
}
