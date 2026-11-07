// Portions of this file are derived from csgo_gc:
// Copyright (c) 2024-2026, Mikko Kokko
// Copyright (c) 2024-2026, Theeto
// Licensed under the BSD-2-Clause License (see THIRD_PARTY_NOTICES)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "csgo_schema.h"
#include "keyvalue.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <random>
#include <ctime>

extern void ReFixLog(const char* fmt, ...);

namespace refix::csgo {

namespace {
static thread_local std::minstd_rand s_rng{ std::random_device{}() };

inline uint32_t RandUint32(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return std::uniform_int_distribution<uint32_t>{ min, max }(s_rng);
}

inline float RandFloat(float min, float max) {
    if (min >= max) return min;
    return std::uniform_real_distribution<float>{ min, max }(s_rng);
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

static Rarity PaintedItemRarity(Rarity itemRarity, Rarity paintKitRarity) {
    int rarityValue = (static_cast<int>(itemRarity) - 1) + static_cast<int>(paintKitRarity);
    if (rarityValue < 0) return Rarity::Default;
    Rarity rarity = static_cast<Rarity>(rarityValue);
    if (rarity > Rarity::Ancient) {
        return (paintKitRarity == Rarity::Immortal) ? Rarity::Immortal : Rarity::Ancient;
    }
    return rarity;
}

static std::string DetectGamePath() {
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) > 0) {
        std::string p(exePath);
        size_t lastSlash = p.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            std::string dir = p.substr(0, lastSlash);
            std::string candidate = dir + "\\csgo\\scripts\\items\\items_game.txt";
            DWORD attr = GetFileAttributesA(candidate.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                return candidate;
            }
        }
    }

    const char* fallbacks[] = {
        "csgo\\scripts\\items\\items_game.txt",
        "D:\\Games\\CSGO\\csgo\\scripts\\items\\items_game.txt",
        "C:\\Users\\Valen\\Documents\\Counter-Strike Global Offensive\\csgo\\scripts\\items\\items_game.txt",
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Global Offensive\\csgo\\scripts\\items\\items_game.txt",
        "D:\\SteamLibrary\\steamapps\\common\\Counter-Strike Global Offensive\\csgo\\scripts\\items\\items_game.txt"
    };

    for (const char* fb : fallbacks) {
        DWORD attr = GetFileAttributesA(fb);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return std::string(fb);
        }
    }
    return "";
}

static std::string DetectExtraPath(const std::string& gamePath) {
    if (!gamePath.empty()) {
        size_t csgoPos = gamePath.find("\\csgo\\");
        if (csgoPos != std::string::npos) {
            std::string dir = gamePath.substr(0, csgoPos);
            std::string candidate = dir + "\\csgo\\items_extra.txt";
            DWORD attr = GetFileAttributesA(candidate.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                return candidate;
            }
        }
    }
    const char* extraFallbacks[] = {
        "csgo\\items_extra.txt",
        "third_party\\csgo\\items_extra.txt",
        "D:\\Games\\CSGO\\csgo\\items_extra.txt"
    };
    for (const char* fb : extraFallbacks) {
        DWORD attr = GetFileAttributesA(fb);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return std::string(fb);
        }
    }
    return "";
}

} // anonymous namespace

struct SchemaItemInfo {
    uint32_t m_defIndex = 0;
    std::string m_name;
    Rarity m_rarity = Rarity::Common;
    Quality m_quality = Quality::Normal;
    uint32_t m_level = 1;
    std::string m_lootListName;
    bool m_willProduceStatTrak = false;
    uint32_t m_supplyCrateSeries = 0;
};

struct SchemaPaintKitInfo {
    uint32_t m_defIndex = 0;
    Rarity m_rarity = Rarity::Common;
    float m_minFloat = 0.0f;
    float m_maxFloat = 1.0f;
};

enum SchemaLootType {
    LootNoAttribute,
    LootPaintable,
    LootSticker,
    LootSpray,
    LootPatch,
    LootMusicKit
};

struct SchemaLootItem {
    Rarity CaseRarity() const {
        if (quality == Quality::Unusual) return Rarity::Unusual;
        return rarity;
    }
    const SchemaItemInfo* itemInfo = nullptr;
    SchemaLootType type = LootNoAttribute;
    uint32_t kitDefIndex = 0;
    float minFloat = 0.0f;
    float maxFloat = 1.0f;
    Rarity rarity = Rarity::Common;
    Quality quality = Quality::Normal;
};

struct SchemaLootList {
    std::vector<SchemaLootItem> items;
    std::vector<const SchemaLootList*> subLists;
    bool willProduceStatTrak = false;
    bool isUnusual = false;
    bool allEntriesAsAdditionalDrops = false;
};

static bool CollectLootListItems(const SchemaLootList& lootList, std::vector<const SchemaLootItem*>& outItems) {
    bool unusuals = lootList.isUnusual;
    for (const SchemaLootList* sub : lootList.subLists) {
        unusuals |= CollectLootListItems(*sub, outItems);
    }
    for (const SchemaLootItem& itm : lootList.items) {
        outItems.push_back(&itm);
    }
    return unusuals;
}

class SchemaManager::Impl {
public:
    std::unordered_map<uint32_t, SchemaItemInfo> m_itemInfo;
    std::unordered_map<std::string, SchemaPaintKitInfo> m_paintKitInfo;
    std::unordered_map<uint32_t, std::string> m_paintKitIdToName;
    std::unordered_map<std::string, SchemaLootList> m_lootLists;
    std::unordered_map<uint32_t, SchemaLootList> m_revolvingLootLists;

    const SchemaItemInfo* ItemInfoByName(std::string_view name) const {
        for (const auto& pair : m_itemInfo) {
            if (pair.second.m_name == name) return &pair.second;
        }
        return nullptr;
    }

    SchemaPaintKitInfo* PaintKitInfoByName(std::string_view name) {
        auto it = m_paintKitInfo.find(std::string(name));
        return (it != m_paintKitInfo.end()) ? &it->second : nullptr;
    }

    bool ParseLootListItem(SchemaLootItem& item, std::string_view name) {
        std::string_view attributeName, itemName = name;
        if (name.size() > 2 && name[0] == '[') {
            size_t endBracket = name.find(']', 1);
            if (endBracket != std::string_view::npos) {
                attributeName = name.substr(1, endBracket - 1);
                itemName = name.substr(endBracket + 1);
            }
        }

        const SchemaItemInfo* info = ItemInfoByName(itemName);
        if (!info) return false;

        item.itemInfo = info;
        item.rarity = info->m_rarity;
        item.quality = info->m_quality;

        if (attributeName.empty()) {
            item.type = LootNoAttribute;
        } else {
            item.type = LootPaintable;
            SchemaPaintKitInfo* pk = PaintKitInfoByName(attributeName);
            if (pk) {
                item.kitDefIndex = pk->m_defIndex;
                item.minFloat = pk->m_minFloat;
                item.maxFloat = pk->m_maxFloat;
                item.rarity = PaintedItemRarity(info->m_rarity, pk->m_rarity);
            }
        }
        return true;
    }

    void ParseItemRecursive(SchemaItemInfo& info, const KeyValue& itemKey, const KeyValue* prefabsKey) {
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

    bool Load(const std::string& gamePath, const std::string& extraPath) {
        KeyValueRead kv(extraPath.empty() ? nullptr : extraPath.c_str());
        if (!gamePath.empty()) {
            if (!kv.MergeToFromFile(gamePath.c_str())) {
                ReFixLog("[GC] CSGOSchema: MergeToFromFile failed for %s", gamePath.c_str());
                return false;
            }
        }

        const KeyValue* root = kv.Root().GetSubkey("items_game");
        if (!root) return false;

        const KeyValue* prefabsKey = root->GetSubkey("prefabs");

        // 1. Paint kits
        const KeyValue* pkKey = root->GetSubkey("paint_kits");
        if (pkKey) {
            for (const KeyValue& sub : pkKey->Subkeys()) {
                std::string_view pkName = sub.GetString("name");
                if (pkName.empty()) continue;
                SchemaPaintKitInfo info;
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
                SchemaItemInfo info;
                info.m_defIndex = defId;
                ParseItemRecursive(info, sub, prefabsKey);
                m_itemInfo[defId] = info;
            }
        }

        // 4. Client loot lists
        const KeyValue* cllKey = root->GetSubkey("client_loot_lists");
        if (cllKey) {
            for (const KeyValue& sub : cllKey->Subkeys()) {
                SchemaLootList list;
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

                    SchemaLootItem itm;
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
        ReFixLog("[GC] CSGOSchema: Loaded %zu items, %zu paint kits, %zu loot lists, %zu revolving lists",
                 m_itemInfo.size(), m_paintKitInfo.size(), m_lootLists.size(), m_revolvingLootLists.size());
        return true;
    }

    const SchemaLootList* GetCrateLootList(uint32_t crateDefIndex) const {
        auto itItem = m_itemInfo.find(crateDefIndex);
        if (itItem == m_itemInfo.end()) return nullptr;
        uint32_t series = itItem->second.m_supplyCrateSeries;
        if (!series) return nullptr;
        auto itRev = m_revolvingLootLists.find(series);
        if (itRev == m_revolvingLootLists.end()) return nullptr;
        return &itRev->second;
    }
};

SchemaManager::SchemaManager()
    : m_impl(std::make_unique<Impl>())
{
}

SchemaManager::~SchemaManager() = default;

SchemaManager& SchemaManager::Get() {
    static SchemaManager s_instance;
    return s_instance;
}

bool SchemaManager::Load() {
    if (m_loaded) return true;

    // Initialize universal knives and gloves
    struct SpecialEntry { uint32_t def; uint32_t pk; const char* w; const char* s; };
    static const SpecialEntry kSpecials[] = {
        { 507, 416, "Karambit", "Doppler Sapphire" },
        { 507, 415, "Karambit", "Doppler Ruby" },
        { 507, 38,  "Karambit", "Fade" },
        { 507, 44,  "Karambit", "Case Hardened" },
        { 515, 38,  "Butterfly Knife", "Fade" },
        { 515, 418, "Butterfly Knife", "Doppler" },
        { 515, 568, "Butterfly Knife", "Gamma Doppler Emerald" },
        { 515, 562, "Butterfly Knife", "Lore" },
        { 508, 562, "M9 Bayonet", "Lore" },
        { 508, 418, "M9 Bayonet", "Doppler" },
        { 508, 38,  "M9 Bayonet", "Fade" },
        { 500, 419, "Bayonet", "Doppler Phase 2" },
        { 500, 38,  "Bayonet", "Fade" },
        { 523, 413, "Talon Knife", "Tiger Tooth" },
        { 523, 418, "Talon Knife", "Doppler" },
        { 525, 12,  "Skeleton Knife", "Crimson Web" },
        { 525, 44,  "Skeleton Knife", "Case Hardened" },
        { 516, 415, "Shadow Daggers", "Doppler Ruby" },
        { 509, 38,  "Huntsman Knife", "Fade" },
        { 522, 413, "Stiletto Knife", "Tiger Tooth" },
        { 519, 418, "Ursus Knife", "Doppler" },
        { 5030, 10048, "Sport Gloves", "Vice" },
        { 5031, 10041, "Driver Gloves", "King Snake" },
        { 5032, 10053, "Hand Wraps", "Cobalt Skulls" },
        { 5033, 10026, "Moto Gloves", "Spearmint" },
        { 5034, 10033, "Specialist Gloves", "Crimson Kimono" }
    };
    for (const auto& sp : kSpecials) {
        SkinDrop d;
        d.defIndex = sp.def;
        d.paintKit = sp.pk;
        d.rarity = 7;
        d.weaponName = sp.w;
        d.skinName = sp.s;
        m_universalSpecialKnives.push_back(d);
    }

    std::string gamePath = DetectGamePath();
    std::string extraPath = DetectExtraPath(gamePath);

    if (!gamePath.empty()) {
        ReFixLog("[GC] CSGOSchema: Loading game schema from %s (extra: %s)", gamePath.c_str(), extraPath.c_str());
        if (m_impl->Load(gamePath, extraPath)) {
            m_loaded = true;
        }
    }

    // Populate lookup maps and market items
    for (const auto& pair : m_impl->m_itemInfo) {
        const auto& itm = pair.second;
        m_weaponNameToDef[itm.m_name] = itm.m_defIndex;
        m_defToWeaponName[itm.m_defIndex] = itm.m_name;
    }
    for (const auto& pair : m_impl->m_paintKitInfo) {
        m_paintKitNameToId[pair.first] = pair.second.m_defIndex;
        m_paintKitIdToName[pair.second.m_defIndex] = pair.first;
    }

    // Categorized market items for give menu
    for (const auto& pair : m_impl->m_itemInfo) {
        const auto& itm = pair.second;
        MarketItem mi;
        mi.defIndex = itm.m_defIndex;
        mi.name = itm.m_name;
        mi.quality = FromEnum(itm.m_quality);
        mi.rarity = FromEnum(itm.m_rarity);

        if (itm.m_supplyCrateSeries > 0) {
            mi.category = "Caja";
        } else if (itm.m_defIndex >= 1200 && itm.m_defIndex <= 1390) {
            mi.category = "Llave";
        } else if (itm.m_defIndex >= 500 && itm.m_defIndex <= 530) {
            mi.category = "Cuchillo";
        } else if (itm.m_defIndex >= 5030 && itm.m_defIndex <= 5035) {
            mi.category = "Guantes";
        } else if (itm.m_name.find("weapon_ak47") != std::string::npos || itm.m_name.find("weapon_m4a1") != std::string::npos || itm.m_name.find("weapon_awp") != std::string::npos) {
            mi.category = "Rifle";
        } else if (itm.m_name.find("weapon_deagle") != std::string::npos || itm.m_name.find("weapon_glock") != std::string::npos || itm.m_name.find("weapon_usp_silencer") != std::string::npos) {
            mi.category = "Pistola";
        } else {
            mi.category = "Otro";
        }
        m_marketItems.push_back(mi);
    }

    m_loaded = true;
    return true;
}

const CaseDropTable* SchemaManager::GetCaseDrops(uint32_t caseDefIndex) const {
    auto it = m_caseTables.find(caseDefIndex);
    if (it != m_caseTables.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SchemaManager::RollCaseDrop(uint32_t caseDefIndex, uint32_t& outDef, uint32_t& outPaintKit, uint32_t& outRarity, uint32_t& outQuality, float& outWear, uint32_t& outSeed) const {
    if (!m_loaded) {
        const_cast<SchemaManager*>(this)->Load();
    }

    const SchemaLootList* root = m_impl->GetCrateLootList(caseDefIndex);
    if (root) {
        std::vector<const SchemaLootItem*> allItems;
        bool containsUnusuals = CollectLootListItems(*root, allItems);

        if (!allItems.empty()) {
            std::sort(allItems.begin(), allItems.end(), [](const SchemaLootItem* a, const SchemaLootItem* b) {
                return a->CaseRarity() < b->CaseRarity();
            });

            struct RGroup {
                std::vector<const SchemaLootItem*> items;
                float weight = 0.0f;
            };
            std::vector<RGroup> groups;
            float totalWeight = 0.0f;

            for (auto first = allItems.begin(); first != allItems.end();) {
                Rarity r = (*first)->CaseRarity();
                auto last = first + 1;
                while (last != allItems.end() && (*last)->CaseRarity() == r) {
                    last++;
                }
                float w = GetRarityWeight(r);
                groups.push_back({ std::vector<const SchemaLootItem*>(first, last), w });
                totalWeight += w;
                first = last;
            }

            float roll = RandFloat(0.0f, totalWeight);
            float accum = 0.0f;
            const SchemaLootItem* picked = nullptr;
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

            outDef = picked->itemInfo->m_defIndex;
            outPaintKit = picked->kitDefIndex;
            outRarity = FromEnum(picked->CaseRarity());
            outQuality = statTrak ? (picked->quality == Quality::Unusual ? 3 : 9) : (picked->quality == Quality::Unusual ? 3 : 4);
            outWear = RandFloat(picked->minFloat, picked->maxFloat);
            outSeed = RandUint32(1, 999);

            ReFixLog("[GC] SchemaManager: Rolled authentic drop def=%u paintKit=%u rarity=%u quality=%u wear=%.4f seed=%u",
                     outDef, outPaintKit, outRarity, outQuality, outWear, outSeed);
            return true;
        }
    }

    // Fallback: roll from universal specials if knife or default skin
    ReFixLog("[GC] SchemaManager: Falling back to universal specials for case %u", caseDefIndex);
    if (!m_universalSpecialKnives.empty()) {
        const auto& sp = m_universalSpecialKnives[RandUint32(0, (uint32_t)m_universalSpecialKnives.size() - 1)];
        outDef = sp.defIndex;
        outPaintKit = sp.paintKit;
        outRarity = 7;
        outQuality = (RandUint32(1, 10) == 1) ? 9 : 3;
        outWear = RandFloat(0.001f, 0.38f);
        outSeed = RandUint32(1, 999);
        return true;
    }

    outDef = 7; // AK-47
    outPaintKit = 180; // Fire Serpent
    outRarity = 6;
    outQuality = 4;
    outWear = 0.05f;
    outSeed = 420;
    return true;
}

std::vector<MarketItem> SchemaManager::GetMarketItemsByCategory(const std::string& category) const {
    std::vector<MarketItem> res;
    for (const auto& it : m_marketItems) {
        if (category.empty() || it.category == category) {
            res.push_back(it);
        }
    }
    return res;
}

std::string SchemaManager::GetWeaponDisplayName(uint32_t defIndex) const {
    auto it = m_defToWeaponName.find(defIndex);
    if (it != m_defToWeaponName.end()) return it->second;
    return "Item #" + std::to_string(defIndex);
}

std::string SchemaManager::GetPaintKitDisplayName(uint32_t paintKit) const {
    auto it = m_paintKitIdToName.find(paintKit);
    if (it != m_paintKitIdToName.end()) return it->second;
    return "Skin #" + std::to_string(paintKit);
}

} // namespace refix::csgo
