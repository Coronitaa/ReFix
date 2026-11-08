// Portions of this file are derived from csgo_gc:
// Copyright (c) 2024-2026, Mikko Kokko
// Copyright (c) 2024-2026, Theeto
// Licensed under the BSD-2-Clause License (see THIRD_PARTY_NOTICES)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "csgo_schema.h"
#include <tuple>
#include "keyvalue.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <random>
#include <ctime>
#include <set>

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

static std::string Utf16ToUtf8(const wchar_t* wstr, int len) {
    if (len <= 0) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr, len, NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}

static std::string StringToLower(std::string s) {
    for (char& c : s) c = (char)::tolower((unsigned char)c);
    return s;
}

static void LoadLanguageTokens(const std::wstring& path, std::unordered_map<std::string, std::string>& tokens) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < 2) return;
    std::vector<char> raw(size);
    file.read(raw.data(), size);
    const wchar_t* p = reinterpret_cast<const wchar_t*>(raw.data());
    size_t wlen = size / 2;
    if (wlen > 0 && p[0] == 0xFEFF) { p++; wlen--; }

    size_t i = 0;
    while (i < wlen) {
        while (i < wlen && (p[i] == L' ' || p[i] == L'\t' || p[i] == L'\r' || p[i] == L'\n')) i++;
        if (i >= wlen) break;
        if (p[i] == L'/' && i + 1 < wlen && p[i+1] == L'/') {
            while (i < wlen && p[i] != L'\n') i++;
            continue;
        }
        if (p[i] == L'"') {
            i++;
            size_t kStart = i;
            while (i < wlen && p[i] != L'"' && p[i] != L'\n') i++;
            size_t kLen = i - kStart;
            if (i < wlen && p[i] == L'"') i++;
            while (i < wlen && (p[i] == L' ' || p[i] == L'\t')) i++;
            if (i < wlen && p[i] == L'"') {
                i++;
                size_t vStart = i;
                while (i < wlen && p[i] != L'"' && p[i] != L'\n') {
                    if (p[i] == L'\\' && i + 1 < wlen) i += 2;
                    else i++;
                }
                size_t vLen = i - vStart;
                if (i < wlen && p[i] == L'"') i++;
                std::string key = StringToLower(Utf16ToUtf8(p + kStart, (int)kLen));
                std::string val = Utf16ToUtf8(p + vStart, (int)vLen);
                tokens[key] = val;
            }
        } else {
            i++;
        }
    }
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

static std::wstring DetectResourcePath(const std::string& gamePath, const wchar_t* langFileName) {
    if (!gamePath.empty()) {
        size_t csgoPos = gamePath.find("\\csgo\\");
        if (csgoPos != std::string::npos) {
            std::string dir = gamePath.substr(0, csgoPos);
            std::wstring wcand(dir.begin(), dir.end());
            wcand += L"\\csgo\\resource\\";
            wcand += langFileName;
            DWORD attr = GetFileAttributesW(wcand.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                return wcand;
            }
        }
    }
    std::wstring fallbacks[] = {
        std::wstring(L"D:\\Games\\CSGO\\csgo\\resource\\") + langFileName,
        std::wstring(L"csgo\\resource\\") + langFileName
    };
    for (const auto& fb : fallbacks) {
        DWORD attr = GetFileAttributesW(fb.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return fb;
        }
    }
    return L"";
}

} // anonymous namespace

struct SchemaItemInfo {
    uint32_t m_defIndex = 0;
    std::string m_name;
    std::string m_itemNameToken;
    std::string m_itemTypeName;
    std::string m_prefab;
    std::string m_imageInventory;
    Rarity m_rarity = Rarity::Common;
    Quality m_quality = Quality::Normal;
    uint32_t m_level = 1;
    std::string m_lootListName;
    bool m_willProduceStatTrak = false;
    uint32_t m_supplyCrateSeries = 0;
    int m_seasonAccess = -1;
};

struct SchemaPaintKitInfo {
    uint32_t m_defIndex = 0;
    std::string m_name;
    std::string m_descriptionTag;
    std::string m_pattern;
    Rarity m_rarity = Rarity::Common;
    float m_minFloat = 0.0f;
    float m_maxFloat = 1.0f;
};

// sticker_kits (stickers, graffiti, patches) and music_definitions entries.
struct SchemaKitInfo {
    enum Kind { Sticker, Spray, Patch, Music };
    uint32_t m_id = 0;
    std::string m_token;   // display name token
    std::string m_image;   // image_inventory-style path ("econ/stickers/...")
    Rarity m_rarity = Rarity::Rare;
    Kind m_kind = Sticker;
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
    std::set<std::pair<std::string, std::string>> m_paintedPairs; // (weaponName, paintKitName)
    std::unordered_map<std::string, SchemaKitInfo> m_stickerKits;     // by kit name
    std::unordered_map<std::string, SchemaKitInfo> m_musicKits;       // by kit name
    std::unordered_map<uint32_t, uint32_t> m_missionCards;            // season -> quest_mission_card count

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

    void CountMissionCards(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return;
        std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        size_t pos = 0;
        while ((pos = text.find("\"seasonaloperations\"", pos)) != std::string::npos) {
            pos += 20;
            int depth = 0;
            uint32_t season = 0;
            std::string lastKey;
            for (size_t i = pos; i < text.size(); ++i) {
                char ch = text[i];
                if (ch == '/' && i + 1 < text.size() && text[i + 1] == '/') { while (i < text.size() && text[i] != '\n') ++i; continue; }
                if (ch == '"') {
                    size_t e = text.find('"', i + 1);
                    if (e == std::string::npos) return;
                    lastKey = text.substr(i + 1, e - i - 1);
                    i = e;
                } else if (ch == '{') {
                    ++depth;
                    if (depth == 2) season = (uint32_t)strtoul(lastKey.c_str(), nullptr, 10);
                    if (depth == 3 && lastKey == "quest_mission_card") ++m_missionCards[season];
                } else if (ch == '}') {
                    if (--depth == 0) { pos = i; break; }
                }
            }
        }
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

        if (!attributeName.empty()) {
            std::string kit(attributeName);
            bool stickerLike = itemName == "sticker" || itemName == "spray" || itemName == "spraypaint" || itemName == "patch";
            if (stickerLike || itemName == "musickit") {
                const auto& kits = stickerLike ? m_stickerKits : m_musicKits;
                auto it = kits.find(kit);
                if (it == kits.end()) return false;
                item.type = itemName == "musickit" ? LootMusicKit
                          : itemName == "sticker" ? LootSticker
                          : itemName == "patch" ? LootPatch : LootSpray;
                item.kitDefIndex = it->second.m_id;
                item.rarity = it->second.m_rarity;
                return true;
            }
        }

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

        std::string_view in = itemKey.GetString("item_name");
        if (!in.empty()) info.m_itemNameToken = std::string(in);

        std::string_view itn = itemKey.GetString("item_type_name");
        if (!itn.empty()) info.m_itemTypeName = std::string(itn);

        std::string_view pf = itemKey.GetString("prefab");
        if (!pf.empty()) {
            if (!info.m_prefab.empty()) info.m_prefab += " ";
            info.m_prefab += std::string(pf);
        }

        std::string_view quality = itemKey.GetString("item_quality");
        if (!quality.empty()) info.m_quality = ItemQualityFromString(quality);

        std::string_view rarity = itemKey.GetString("item_rarity");
        if (!rarity.empty()) info.m_rarity = ItemRarityFromString(rarity);

        std::string_view lootListName = itemKey.GetString("loot_list_name");
        if (!lootListName.empty()) info.m_lootListName = std::string(lootListName);

        std::string_view img = itemKey.GetString("image_inventory");
        if (!img.empty()) info.m_imageInventory = std::string(img);

        info.m_willProduceStatTrak = itemKey.GetNumber<bool>("will_produce_stattrak", info.m_willProduceStatTrak);

        const KeyValue* attrs = itemKey.GetSubkey("attributes");
        if (attrs) {
            for (const KeyValue& a : attrs->Subkeys()) {
                if (a.GetString("attribute_class") == "supply_crate_series" || a.Name() == "set supply crate series") {
                    info.m_supplyCrateSeries = a.GetNumber<uint32_t>("value", 0);
                }
                if (a.Name() == "season access") info.m_seasonAccess = FromString<int>(a.String());
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
                info.m_name = std::string(pkName);
                info.m_descriptionTag = std::string(sub.GetString("description_tag"));
                info.m_pattern = std::string(sub.GetString("pattern"));
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

        // 2a. Operation weeks: one "quest_mission_card" per week. The KeyValue reader merges repeated keys, so count
        // them in the raw text: seasonaloperations { "<season>" { "quest_mission_card" {...} ... } }.
        CountMissionCards(gamePath);

        // 2b. Sticker, graffiti and patch kits; music kits
        if (const KeyValue* skKey = root->GetSubkey("sticker_kits")) {
            for (const KeyValue& sub : skKey->Subkeys()) {
                std::string name(sub.GetString("name"));
                if (name.empty() || name == "default") continue;
                SchemaKitInfo kit;
                kit.m_id = FromString<uint32_t>(sub.Name());
                kit.m_token = std::string(sub.GetString("item_name"));
                std::string_view rarity = sub.GetString("item_rarity");
                if (!rarity.empty()) kit.m_rarity = ItemRarityFromString(rarity);
                std::string_view patch = sub.GetString("patch_material");
                std::string_view material = patch.empty() ? sub.GetString("sticker_material") : patch;
                // Graffiti: "#SprayKit_*" tokens, and tournament team graffiti named "*_graffiti" with sticker tokens.
                bool spray = kit.m_token.rfind("#SprayKit", 0) == 0 ||
                             (name.size() > 9 && name.compare(name.size() - 9, 9, "_graffiti") == 0);
                kit.m_kind = !patch.empty() ? SchemaKitInfo::Patch : spray ? SchemaKitInfo::Spray : SchemaKitInfo::Sticker;
                if (!material.empty()) kit.m_image = std::string(patch.empty() ? "econ/stickers/" : "econ/patches/") + std::string(material);
                if (kit.m_id) m_stickerKits[name] = kit;
            }
        }
        if (const KeyValue* mdKey = root->GetSubkey("music_definitions")) {
            for (const KeyValue& sub : mdKey->Subkeys()) {
                std::string name(sub.GetString("name"));
                if (name.empty()) continue;
                SchemaKitInfo kit;
                kit.m_id = FromString<uint32_t>(sub.Name());
                kit.m_token = std::string(sub.GetString("loc_name"));
                kit.m_image = std::string(sub.GetString("image_inventory"));
                kit.m_kind = SchemaKitInfo::Music;
                if (kit.m_id) m_musicKits[name] = kit;
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

        // 4. Client loot lists & painted pairs extraction
        const KeyValue* cllKey = root->GetSubkey("client_loot_lists");
        if (cllKey) {
            // Create every list first: a list may name sub-lists defined further down (sticker capsules do),
            // and unordered_map keeps element addresses stable for the sub-list pointers.
            for (const KeyValue& sub : cllKey->Subkeys()) m_lootLists[std::string(sub.Name())];
            for (const KeyValue& sub : cllKey->Subkeys()) {
                SchemaLootList list;
                list.isUnusual = (sub.Name().find("unusual") != std::string_view::npos);
                for (const KeyValue& entry : sub.Subkeys()) {
                    std::string_view en = entry.Name();
                    if (en == "will_produce_stattrak") { list.willProduceStatTrak = true; continue; }
                    if (en == "all_entries_as_additional_drops") { list.allEntriesAsAdditionalDrops = true; continue; }

                    if (en.size() > 2 && en[0] == '[') {
                        size_t endBracket = en.find(']', 1);
                        if (endBracket != std::string_view::npos) {
                            std::string pkName = std::string(en.substr(1, endBracket - 1));
                            std::string wpName = std::string(en.substr(endBracket + 1));
                            m_paintedPairs.insert({ wpName, pkName });
                        }
                    }

                    auto subSearch = m_lootLists.find(std::string(en));
                    if (subSearch != m_lootLists.end() && subSearch->first != sub.Name()) {
                        list.subLists.push_back(&subSearch->second);
                        continue;
                    }

                    SchemaLootItem itm;
                    if (ParseLootListItem(itm, en)) {
                        if (list.isUnusual) itm.quality = Quality::Unusual;
                        list.items.push_back(itm);
                    }
                }
                SchemaLootList& slot = m_lootLists[std::string(sub.Name())];
                slot.items = std::move(list.items);
                slot.subLists = std::move(list.subLists);
                slot.willProduceStatTrak = list.willProduceStatTrak;
                slot.isUnusual = list.isUnusual;
                slot.allEntriesAsAdditionalDrops = list.allEntriesAsAdditionalDrops;
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
        ReFixLog("[GC] CSGOSchema: Loaded %zu items, %zu paint kits, %zu loot lists, %zu revolving lists, %zu painted combos",
                 m_itemInfo.size(), m_paintKitInfo.size(), m_lootLists.size(), m_revolvingLootLists.size(), m_paintedPairs.size());
        return true;
    }

    const SchemaLootList* GetCrateLootList(uint32_t crateDefIndex) const {
        auto itItem = m_itemInfo.find(crateDefIndex);
        if (itItem == m_itemInfo.end()) return nullptr;
        uint32_t series = itItem->second.m_supplyCrateSeries;
        if (!series) {
            // Self-opening boxes (graffiti, some capsules) name their list directly instead of a crate series.
            auto itList = m_lootLists.find(itItem->second.m_lootListName);
            return itList != m_lootLists.end() ? &itList->second : nullptr;
        }
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

std::string SchemaManager::ResolveToken(const std::string& token) const {
    if (token.empty()) return "";
    std::string key = token;
    if (key[0] == '#') key = key.substr(1);
    std::string lk = StringToLower(key);
    auto it = m_tokens.find(lk);
    if (it != m_tokens.end()) return it->second;
    return key;
}

uint32_t SchemaManager::FindDefByName(const std::string& itemName) const {
    auto it = m_weaponNameToDef.find(itemName);
    return it != m_weaponNameToDef.end() ? it->second : 0;
}

std::string SchemaManager::GetItemName(uint32_t defIndex) const {
    auto it = m_defToWeaponName.find(defIndex);
    return it != m_defToWeaponName.end() ? it->second : std::string();
}

uint32_t SchemaManager::GetMissionCardCount(uint32_t season) const {
    auto it = m_missionCards.find(season);
    return it != m_missionCards.end() ? it->second : 0;
}

std::string SchemaManager::ResolveTokenEn(const std::string& token) const {
    if (token.empty()) return "";
    std::string key = token[0] == '#' ? token.substr(1) : token;
    auto it = m_tokensEn.find(StringToLower(key));
    return it != m_tokensEn.end() ? it->second : ResolveToken(token);
}

bool SchemaManager::Load() {
    if (m_loaded) return true;

    // Load language resource files (Spanish first, English as fallback)
    std::string gamePath = DetectGamePath();
    std::wstring spanishPath = DetectResourcePath(gamePath, L"csgo_spanish.txt");
    std::wstring englishPath = DetectResourcePath(gamePath, L"csgo_english.txt");

    if (!englishPath.empty()) {
        LoadLanguageTokens(englishPath, m_tokens);
        LoadLanguageTokens(englishPath, m_tokensEn);
    }
    if (!spanishPath.empty()) {
        LoadLanguageTokens(spanishPath, m_tokens);
    }
    ReFixLog("[GC] CSGOSchema: Loaded %zu localization tokens", m_tokens.size());

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

    std::string extraPath = DetectExtraPath(gamePath);

    if (!gamePath.empty()) {
        ReFixLog("[GC] CSGOSchema: Loading game schema from %s (extra: %s)", gamePath.c_str(), extraPath.c_str());
        if (m_impl->Load(gamePath, extraPath)) {
            m_loaded = true;
        }
    }

    m_missionCards = m_impl->m_missionCards;

    // Populate lookup maps
    std::unordered_map<std::string, uint32_t> nameToDef;
    for (const auto& pair : m_impl->m_itemInfo) {
        const auto& itm = pair.second;
        m_weaponNameToDef[itm.m_name] = itm.m_defIndex;
        m_defToWeaponName[itm.m_defIndex] = itm.m_name;
        nameToDef[itm.m_name] = itm.m_defIndex;
    }
    for (const auto& pair : m_impl->m_paintKitInfo) {
        m_paintKitNameToId[pair.first] = pair.second.m_defIndex;
        m_paintKitIdToName[pair.second.m_defIndex] = pair.first;
    }

    // Categorization helper
    auto determineCategory = [](const SchemaItemInfo& itm) -> std::string {
        // Graffiti boxes share the sticker capsule prefab.
        if (itm.m_name.rfind("crate_graffiti_pack", 0) == 0 || itm.m_name.rfind("crate_spray_pack", 0) == 0) return "Grafitis";
        if (itm.m_prefab.find("sticker_capsule") != std::string::npos || itm.m_prefab.find("patch_capsule") != std::string::npos ||
            itm.m_name.rfind("crate_sticker_pack", 0) == 0 || itm.m_name.rfind("crate_signature_pack", 0) == 0 ||
            itm.m_name.rfind("crate_patch_pack", 0) == 0) {
            return "Pegatinas y Parches";
        }
        if (itm.m_name.find("musickit") != std::string::npos && itm.m_supplyCrateSeries > 0) return "Música y Coleccionables";
        if (itm.m_supplyCrateSeries > 0 || itm.m_prefab.find("weapon_case") != std::string::npos || itm.m_prefab.find("supply_crate") != std::string::npos) {
            return "Cajas";
        }
        if (itm.m_prefab.find("case_key") != std::string::npos || (itm.m_prefab.find("tool") != std::string::npos && itm.m_name.find("key") != std::string::npos)) {
            return "Llaves";
        }
        if (itm.m_defIndex >= 500 && itm.m_defIndex <= 530 || itm.m_prefab.find("melee") != std::string::npos) {
            return "Cuchillos";
        }
        if (itm.m_defIndex >= 5027 && itm.m_defIndex <= 5035 || itm.m_prefab.find("hands") != std::string::npos || itm.m_prefab.find("gloves") != std::string::npos) {
            return "Guantes";
        }
        if (itm.m_prefab.find("customplayer") != std::string::npos || (itm.m_defIndex >= 4613 && itm.m_defIndex <= 5601)) {
            return "Agentes";
        }
        if (itm.m_prefab.find("sticker") != std::string::npos || itm.m_defIndex == 1209 || itm.m_prefab.find("patch") != std::string::npos || itm.m_defIndex == 4609) {
            return "Pegatinas y Parches";
        }
        if (itm.m_seasonAccess >= 0 && itm.m_prefab.find("season_pass") != std::string::npos) {
            return "Pases";
        }
        if (itm.m_prefab.find("music") != std::string::npos || itm.m_defIndex == 1314 || itm.m_prefab.find("operation_pass") != std::string::npos || itm.m_defIndex == 4549 || itm.m_defIndex == 4550 || itm.m_prefab.find("collectible") != std::string::npos) {
            return "Música y Coleccionables";
        }
        const std::string& n = itm.m_name;
        if (n.find("ak47") != std::string::npos || n.find("m4a1") != std::string::npos || n.find("m4a4") != std::string::npos || n.find("awp") != std::string::npos ||
            n.find("aug") != std::string::npos || n.find("sg556") != std::string::npos || n.find("galilar") != std::string::npos || n.find("famas") != std::string::npos ||
            n.find("ssg08") != std::string::npos || n.find("g3sg1") != std::string::npos || n.find("scar20") != std::string::npos) {
            return "Rifles";
        }
        if (n.find("glock") != std::string::npos || n.find("usp") != std::string::npos || n.find("hkp2000") != std::string::npos || n.find("deagle") != std::string::npos ||
            n.find("p250") != std::string::npos || n.find("fiveseven") != std::string::npos || n.find("tec9") != std::string::npos || n.find("cz75a") != std::string::npos ||
            n.find("elite") != std::string::npos || n.find("revolver") != std::string::npos) {
            return "Pistolas";
        }
        if (n.find("mp9") != std::string::npos || n.find("mac10") != std::string::npos || n.find("mp7") != std::string::npos || n.find("mp5sd") != std::string::npos ||
            n.find("ump45") != std::string::npos || n.find("p90") != std::string::npos || n.find("bizon") != std::string::npos) {
            return "Subfusiles";
        }
        if (n.find("nova") != std::string::npos || n.find("xm1014") != std::string::npos || n.find("mag7") != std::string::npos || n.find("sawedoff") != std::string::npos ||
            n.find("m249") != std::string::npos || n.find("negev") != std::string::npos) {
            return "Pesadas";
        }
        return "Música y Coleccionables";
    };

    m_marketItems.clear();
    std::unordered_map<std::string, size_t> catCounts;

    // 1. Painted weapon, knife, and glove combos from client_loot_lists
    for (const auto& pair : m_impl->m_paintedPairs) {
        const std::string& wpName = pair.first;
        const std::string& pkName = pair.second;

        auto itWp = nameToDef.find(wpName);
        if (itWp == nameToDef.end()) continue;
        const SchemaItemInfo& wpItem = m_impl->m_itemInfo[itWp->second];

        auto itPk = m_impl->m_paintKitInfo.find(pkName);
        if (itPk == m_impl->m_paintKitInfo.end()) continue;
        const SchemaPaintKitInfo& pk = itPk->second;

        MarketItem mi;
        mi.defIndex = wpItem.m_defIndex;
        mi.paintKit = pk.m_defIndex;
        mi.minWear = pk.m_minFloat;
        mi.maxWear = pk.m_maxFloat;
        mi.wear = pk.m_minFloat + 0.001f;
        mi.seed = 420;
        mi.weaponName = wpItem.m_name;
        mi.skinName = pk.m_name;
        mi.imageInventory = wpItem.m_imageInventory;
        mi.pattern = pk.m_pattern;

        std::string wpDisp = ResolveToken(wpItem.m_itemNameToken);
        if (wpDisp.empty()) wpDisp = wpItem.m_name;

        std::string pkDisp = ResolveToken(pk.m_descriptionTag);
        if (pkDisp.empty()) pkDisp = pk.m_name;

        mi.name = wpDisp + " | " + pkDisp;
        mi.nameEn = ResolveTokenEn(wpItem.m_itemNameToken) + " | " + ResolveTokenEn(pk.m_descriptionTag);
        mi.category = determineCategory(wpItem);

        bool isUnusual = (mi.category == "Cuchillos" || mi.category == "Guantes");
        mi.quality = isUnusual ? 3 : 4;
        mi.rarity = isUnusual ? 7 : FromEnum(PaintedItemRarity(wpItem.m_rarity, pk.m_rarity));

        m_marketItems.push_back(mi);
        catCounts[mi.category]++;
    }

    // 2. Base grantable items from items (cases, keys, agents, collectibles, base knives/gloves, base weapons)
    for (const auto& pair : m_impl->m_itemInfo) {
        const SchemaItemInfo& itm = pair.second;
        if (itm.m_defIndex == 0) continue;
        if (itm.m_name.empty() || itm.m_name.find("test") != std::string::npos) continue;
        if (itm.m_name == "sticker" || itm.m_name == "spray" || itm.m_name == "spraypaint" || itm.m_name == "patch" ||
            itm.m_name == "musickit")
            continue;  // listed per kit below

        std::string cat = determineCategory(itm);

        MarketItem mi;
        mi.defIndex = itm.m_defIndex;
        mi.paintKit = 0;
        mi.minWear = 0.0f;
        mi.maxWear = 1.0f;
        mi.wear = 0.0f;
        mi.seed = 0;
        mi.category = cat;
        mi.weaponName = itm.m_name;
        mi.quality = FromEnum(itm.m_quality);
        mi.rarity = FromEnum(itm.m_rarity);
        mi.imageInventory = itm.m_imageInventory;
        mi.seasonAccess = itm.m_seasonAccess;

        if (cat == "Cuchillos" || cat == "Guantes") {
            mi.quality = 3;
            if (mi.rarity < 7) mi.rarity = 7;
        }

        std::string nameDisp = ResolveToken(itm.m_itemNameToken);
        if (nameDisp.empty()) nameDisp = itm.m_name;
        mi.name = nameDisp;
        mi.nameEn = itm.m_itemNameToken.empty() ? itm.m_name : ResolveTokenEn(itm.m_itemNameToken);

        m_marketItems.push_back(mi);
        catCounts[cat]++;
    }

    // 2b. Every sticker, graffiti, patch and music kit, granted on its carrier item with the kit attribute
    auto carrierName = [&](const char* name, const char* fallback) {
        for (const auto& pair : m_impl->m_itemInfo)
            if (pair.second.m_name == name) {
                std::string d = ResolveToken(pair.second.m_itemNameToken);
                std::string en = ResolveTokenEn(pair.second.m_itemNameToken);
                return std::make_tuple(pair.first, d.empty() ? std::string(fallback) : d, en.empty() ? std::string(fallback) : en);
            }
        return std::make_tuple(0u, std::string(fallback), std::string(fallback));
    };
    auto addKits = [&](const std::unordered_map<std::string, SchemaKitInfo>& kits) {
        auto sticker = carrierName("sticker", "Sticker"), spray = carrierName("spray", "Graffiti"),
             patch = carrierName("patch", "Patch"), music = carrierName("musickit", "Music Kit");
        for (const auto& pair : kits) {
            const SchemaKitInfo& k = pair.second;
            const auto& carrier = k.m_kind == SchemaKitInfo::Spray ? spray : k.m_kind == SchemaKitInfo::Patch ? patch
                                : k.m_kind == SchemaKitInfo::Music ? music : sticker;
            if (std::get<0>(carrier) == 0) continue;  // this build has no such carrier item
            MarketItem mi;
            mi.defIndex = std::get<0>(carrier);
            if (k.m_kind == SchemaKitInfo::Music) mi.musicKit = k.m_id; else mi.stickerKit = k.m_id;
            mi.quality = 4;
            mi.rarity = FromEnum(k.m_rarity);
            mi.imageInventory = k.m_image;
            mi.weaponName = pair.first;
            std::string disp = ResolveToken(k.m_token);
            mi.name = std::get<1>(carrier) + " | " + (disp.empty() ? pair.first : disp);
            std::string dispEn = ResolveTokenEn(k.m_token);
            mi.nameEn = std::get<2>(carrier) + " | " + (dispEn.empty() ? pair.first : dispEn);
            mi.category = k.m_kind == SchemaKitInfo::Music ? "Música y Coleccionables"
                        : k.m_kind == SchemaKitInfo::Spray ? "Grafitis" : "Pegatinas y Parches";
            m_marketItems.push_back(mi);
            catCounts[mi.category]++;
        }
    };
    addKits(m_impl->m_stickerKits);
    addKits(m_impl->m_musicKits);

    // 3. Build CategoryInfo list
    m_categories.clear();
    m_categories.push_back({ "Todos", m_marketItems.size() });

    const char* orderedCats[] = {
        "Rifles", "Pistolas", "Subfusiles", "Pesadas",
        "Cuchillos", "Guantes", "Agentes", "Cajas",
        "Llaves", "Pases", "Pegatinas y Parches", "Grafitis", "Música y Coleccionables"
    };
    for (const char* oc : orderedCats) {
        size_t cnt = catCounts[oc];
        if (cnt > 0) {
            m_categories.push_back({ oc, cnt });
        }
    }

    ReFixLog("[GC] CSGOSchema: Total %zu grantable items catalogued across %zu categories",
             m_marketItems.size(), m_categories.size());

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

bool SchemaManager::RollCaseDrop(uint32_t caseDefIndex, CaseDrop& out) const {
    out = CaseDrop();
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

            out.defIndex = picked->itemInfo->m_defIndex;
            bool kit = picked->type == LootSticker || picked->type == LootSpray || picked->type == LootPatch;
            out.paintKit = picked->type == LootPaintable ? picked->kitDefIndex : 0;
            out.stickerKit = kit ? picked->kitDefIndex : 0;
            out.musicKit = picked->type == LootMusicKit ? picked->kitDefIndex : 0;
            if (kit || out.musicKit) statTrak = statTrak && out.musicKit != 0;  // only music kits come in StatTrak
            out.rarity = FromEnum(picked->CaseRarity());
            out.quality = statTrak ? (picked->quality == Quality::Unusual ? 3 : 9) : (picked->quality == Quality::Unusual ? 3 : 4);
            out.wear = RandFloat(picked->minFloat, picked->maxFloat);
            out.seed = RandUint32(1, 999);

            ReFixLog("[GC] SchemaManager: Rolled authentic drop def=%u paintKit=%u rarity=%u quality=%u wear=%.4f seed=%u",
                     out.defIndex, out.paintKit, out.rarity, out.quality, out.wear, out.seed);
            return true;
        }
    }

    // Fallback: roll from universal specials if knife or default skin
    ReFixLog("[GC] SchemaManager: Falling back to universal specials for case %u", caseDefIndex);
    if (!m_universalSpecialKnives.empty()) {
        const auto& sp = m_universalSpecialKnives[RandUint32(0, (uint32_t)m_universalSpecialKnives.size() - 1)];
        out.defIndex = sp.defIndex;
        out.paintKit = sp.paintKit;
        out.rarity = 7;
        out.quality = (RandUint32(1, 10) == 1) ? 9 : 3;
        out.wear = RandFloat(0.001f, 0.38f);
        out.seed = RandUint32(1, 999);
        return true;
    }

    out.defIndex = 7; // AK-47
    out.paintKit = 180; // Fire Serpent
    out.rarity = 6;
    out.quality = 4;
    out.wear = 0.05f;
    out.seed = 420;
    return true;
}

std::vector<MarketItem> SchemaManager::GetMarketItemsByCategory(const std::string& category) const {
    if (category.empty() || category == "Todos") {
        return m_marketItems;
    }
    std::vector<MarketItem> res;
    for (const auto& it : m_marketItems) {
        if (it.category == category) {
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
