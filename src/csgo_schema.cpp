#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "csgo_schema.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <random>
#include <ctime>

namespace refix::csgo {

static std::string CleanStr(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" \t\r\n\"");
    return s.substr(first, (last - first + 1));
}

SchemaManager& SchemaManager::Get() {
    static SchemaManager s_instance;
    return s_instance;
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

bool SchemaManager::Load() {
    if (m_loaded) return true;

    // Initialize universal rare knives and gloves pool for cases
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

    std::string path = DetectGamePath();
    if (!path.empty()) {
        if (LoadFromItemsGameFile(path)) {
            m_loaded = true;
            return true;
        }
    }

    BuildFallbackSchema();
    m_loaded = true;
    return true;
}

bool SchemaManager::LoadFromItemsGameFile(const std::string& filePath) {
    std::string content;
    if (!filePath.empty()) {
        std::ifstream ifs(filePath, std::ios::binary);
        if (ifs.is_open()) {
            content.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            ifs.close();
        }
    }

    // 1. Build weapon_name -> defIndex mapping
    // Common weapon defIndexes in CS:GO
    m_weaponNameToDef["weapon_deagle"] = 1;
    m_weaponNameToDef["weapon_elite"] = 2;
    m_weaponNameToDef["weapon_fiveseven"] = 3;
    m_weaponNameToDef["weapon_glock"] = 4;
    m_weaponNameToDef["weapon_ak47"] = 7;
    m_weaponNameToDef["weapon_aug"] = 8;
    m_weaponNameToDef["weapon_awp"] = 9;
    m_weaponNameToDef["weapon_famas"] = 10;
    m_weaponNameToDef["weapon_g3sg1"] = 11;
    m_weaponNameToDef["weapon_galilar"] = 13;
    m_weaponNameToDef["weapon_m249"] = 14;
    m_weaponNameToDef["weapon_m4a1"] = 16;
    m_weaponNameToDef["weapon_mac10"] = 17;
    m_weaponNameToDef["weapon_p90"] = 19;
    m_weaponNameToDef["weapon_mp5sd"] = 23;
    m_weaponNameToDef["weapon_ump45"] = 24;
    m_weaponNameToDef["weapon_xm1014"] = 25;
    m_weaponNameToDef["weapon_bizon"] = 26;
    m_weaponNameToDef["weapon_mag7"] = 27;
    m_weaponNameToDef["weapon_negev"] = 28;
    m_weaponNameToDef["weapon_sawedoff"] = 29;
    m_weaponNameToDef["weapon_tec9"] = 30;
    m_weaponNameToDef["weapon_taser"] = 31;
    m_weaponNameToDef["weapon_hkp2000"] = 32;
    m_weaponNameToDef["weapon_mp7"] = 33;
    m_weaponNameToDef["weapon_mp9"] = 34;
    m_weaponNameToDef["weapon_nova"] = 35;
    m_weaponNameToDef["weapon_p250"] = 36;
    m_weaponNameToDef["weapon_scar20"] = 38;
    m_weaponNameToDef["weapon_sg556"] = 39;
    m_weaponNameToDef["weapon_ssg08"] = 40;
    m_weaponNameToDef["weapon_m4a1_silencer"] = 60;
    m_weaponNameToDef["weapon_usp_silencer"] = 61;
    m_weaponNameToDef["weapon_cz75a"] = 63;
    m_weaponNameToDef["weapon_revolver"] = 64;
    m_weaponNameToDef["weapon_bayonet"] = 500;
    m_weaponNameToDef["weapon_knife_flip"] = 505;
    m_weaponNameToDef["weapon_knife_gut"] = 506;
    m_weaponNameToDef["weapon_knife_karambit"] = 507;
    m_weaponNameToDef["weapon_knife_m9_bayonet"] = 508;
    m_weaponNameToDef["weapon_knife_tactical"] = 509;
    m_weaponNameToDef["weapon_knife_falchion"] = 512;
    m_weaponNameToDef["weapon_knife_survival_bowie"] = 514;
    m_weaponNameToDef["weapon_knife_butterfly"] = 515;
    m_weaponNameToDef["weapon_knife_push"] = 516;
    m_weaponNameToDef["weapon_knife_ursus"] = 519;
    m_weaponNameToDef["weapon_knife_gypsy_jackknife"] = 520;
    m_weaponNameToDef["weapon_knife_stiletto"] = 522;
    m_weaponNameToDef["weapon_knife_widowmaker"] = 523;
    m_weaponNameToDef["weapon_knife_skeleton"] = 525;

    for (const auto& [name, def] : m_weaponNameToDef) {
        m_defToWeaponName[def] = name;
    }

    // 2. Parse paint_kits: id -> name
    size_t pkPos = 0;
    while ((pkPos = content.find("\"paint_kits\"", pkPos)) != std::string::npos) {
        size_t braceStart = content.find('{', pkPos);
        if (braceStart == std::string::npos) break;
        size_t p = braceStart + 1;
        int depth = 1;
        while (p < content.size() && depth > 0) {
            if (content[p] == '{') depth++;
            else if (content[p] == '}') depth--;
            p++;
        }
        std::string pkBlock = content.substr(braceStart, p - braceStart);

        // Find each "id" { "name" "..." }
        size_t pos = 0;
        while (pos < pkBlock.size()) {
            size_t q1 = pkBlock.find('\"', pos);
            if (q1 == std::string::npos) break;
            size_t q2 = pkBlock.find('\"', q1 + 1);
            if (q2 == std::string::npos) break;
            std::string key = pkBlock.substr(q1 + 1, q2 - q1 - 1);

            size_t nextOpen = pkBlock.find('{', q2);
            if (nextOpen == std::string::npos) break;
            size_t nextClose = pkBlock.find('}', nextOpen);
            if (nextClose == std::string::npos) break;

            if (std::all_of(key.begin(), key.end(), ::isdigit)) {
                uint32_t pkId = (uint32_t)std::stoul(key);
                std::string sub = pkBlock.substr(nextOpen, nextClose - nextOpen + 1);
                size_t npos = sub.find("\"name\"");
                if (npos != std::string::npos) {
                    size_t s1 = sub.find('\"', npos + 6);
                    if (s1 != std::string::npos) {
                        size_t s2 = sub.find('\"', s1 + 1);
                        if (s2 != std::string::npos) {
                            std::string name = sub.substr(s1 + 1, s2 - s1 - 1);
                            m_paintKitNameToId[name] = pkId;
                            m_paintKitIdToName[pkId] = name;
                        }
                    }
                }
            }
            pos = nextClose + 1;
        }
        pkPos = p;
    }

    // 3. Known CS:GO Cases and their internal names
    struct CaseMap { uint32_t def; const char* internalName; const char* dispName; };
    static const CaseMap kAllCases[] = {
        { 4001, "crate_valve_1", "CS:GO Weapon Case 1" },
        { 4002, "crate_esports_2013", "eSports 2013 Case" },
        { 4003, "crate_operation_ii", "Operation Bravo Case" },
        { 4004, "crate_valve_2", "CS:GO Weapon Case 2" },
        { 4005, "crate_esports_2013_winter", "eSports 2013 Winter Case" },
        { 4009, "crate_community_1", "Winter Offensive Weapon Case" },
        { 4010, "crate_valve_3", "CS:GO Weapon Case 3" },
        { 4011, "crate_community_2", "Operation Phoenix Weapon Case" },
        { 4017, "crate_community_3", "Huntsman Weapon Case" },
        { 4018, "crate_community_4", "Operation Breakout Weapon Case" },
        { 4019, "crate_esports_2014_summer", "eSports 2014 Summer Case" },
        { 4029, "crate_operation_vanguard", "Operation Vanguard Case" },
        { 4061, "crate_community_6", "Chroma Case" },
        { 4089, "crate_community_7", "Chroma 2 Case" },
        { 4091, "crate_community_8", "Falchion Case" },
        { 4138, "crate_community_9", "Shadow Case" },
        { 4186, "crate_community_10", "Revolver Case" },
        { 4187, "crate_community_11", "Operation Wildfire Case" },
        { 4233, "crate_community_12", "Chroma 3 Case" },
        { 4236, "crate_community_13", "Gamma Case" },
        { 4281, "crate_gamma_2", "Gamma 2 Case" },
        { 4288, "crate_community_15", "Glove Case" },
        { 4351, "crate_community_16", "Spectrum Case" },
        { 4352, "crate_community_17", "Operation Hydra Case" },
        { 4403, "crate_community_18", "Spectrum 2 Case" },
        { 4471, "crate_community_19", "Clutch Case" },
        { 4482, "crate_community_20", "Horizon Case" },
        { 4548, "crate_community_21", "Danger Zone Case" },
        { 4598, "crate_community_22", "Prisma Case" },
        { 4620, "crate_community_23", "CS20 Case" },
        { 4669, "crate_community_24", "Shattered Web Case" },
        { 4698, "crate_community_25", "Fracture Case" },
        { 4790, "crate_community_26", "Snakebite Case" },
        { 4846, "crate_community_27", "Dreams & Nightmares Case" },
        { 4874, "crate_community_28", "Recoil Case" },
        { 4901, "crate_community_29", "Revolution Case" }
    };

    // Helper to parse a client_loot_list block like "[paint_kit]weapon_name"
    auto parseLootList = [&](const std::string& listName, uint32_t rarity) -> std::vector<SkinDrop> {
        std::vector<SkinDrop> drops;
        size_t pos = content.find("\"" + listName + "\"");
        if (pos == std::string::npos) return drops;

        size_t openBrace = content.find('{', pos);
        if (openBrace == std::string::npos) return drops;
        size_t closeBrace = content.find('}', openBrace);
        if (closeBrace == std::string::npos) return drops;

        std::string block = content.substr(openBrace, closeBrace - openBrace + 1);
        size_t p = 0;
        while ((p = block.find('[', p)) != std::string::npos) {
            size_t pEnd = block.find(']', p);
            if (pEnd == std::string::npos) break;
            std::string pkName = block.substr(p + 1, pEnd - p - 1);

            size_t wStart = pEnd + 1;
            while (wStart < block.size() && (block[wStart] == ' ' || block[wStart] == '\t')) wStart++;
            size_t wEnd = wStart;
            while (wEnd < block.size() && (isalnum(block[wEnd]) || block[wEnd] == '_')) wEnd++;
            std::string wpName = block.substr(wStart, wEnd - wStart);

            SkinDrop d;
            d.rarity = rarity;
            d.skinName = pkName;
            d.weaponName = wpName;

            auto itW = m_weaponNameToDef.find(wpName);
            d.defIndex = (itW != m_weaponNameToDef.end()) ? itW->second : 7;

            auto itP = m_paintKitNameToId.find(pkName);
            d.paintKit = (itP != m_paintKitNameToId.end()) ? itP->second : 0;

            if (d.paintKit > 0) {
                drops.push_back(d);
            }
            p = wEnd;
        }
        return drops;
    };

    for (const auto& cm : kAllCases) {
        CaseDropTable dt;
        dt.caseDefIndex = cm.def;
        dt.caseName = cm.internalName;
        dt.displayName = cm.dispName;

        dt.rare = parseLootList(std::string(cm.internalName) + "_rare", 3);
        dt.mythical = parseLootList(std::string(cm.internalName) + "_mythical", 4);
        dt.legendary = parseLootList(std::string(cm.internalName) + "_legendary", 5);
        dt.ancient = parseLootList(std::string(cm.internalName) + "_ancient", 6);
        dt.special = m_universalSpecialKnives;

        // If specific lootlist name differed, provide fallbacks
        if (dt.rare.empty()) {
            dt.rare = { { 33, 11, 3, "weapon_mp7", "Skulls" }, { 8, 12, 3, "weapon_aug", "Wings" }, { 39, 14, 3, "weapon_sg556", "Ultraviolet" } };
        }
        if (dt.mythical.empty()) {
            dt.mythical = { { 4, 48, 4, "weapon_glock", "Dragon Tattoo" }, { 61, 60, 4, "weapon_usp_silencer", "Dark Water" }, { 60, 60, 4, "weapon_m4a1_silencer", "Dark Water" } };
        }
        if (dt.legendary.empty()) {
            dt.legendary = { { 7, 44, 5, "weapon_ak47", "Case Hardened" }, { 1, 61, 5, "weapon_deagle", "Hypnotic" } };
        }
        if (dt.ancient.empty()) {
            dt.ancient = { { 9, 51, 6, "weapon_awp", "Lightning Strike" } };
        }

        m_caseTables[cm.def] = dt;
    }

    // 4. Build complete Market Catalog
    // Tab 1: All 36 Cases
    for (const auto& cm : kAllCases) {
        MarketItem mi;
        mi.defIndex = cm.def;
        mi.name = cm.dispName;
        mi.category = "Caja";
        mi.quality = 4;
        mi.rarity = 0;
        mi.paintKit = 0;
        mi.wear = 0.0f;
        mi.seed = 0;
        m_marketItems.push_back(mi);
    }

    // Tab 2: All 36 Keys
    static const struct { uint32_t def; const char* name; } kKeysList[] = {
        { 1203, "CS:GO Case Key" }, { 1204, "eSports Key" }, { 1214, "Winter Offensive Key" },
        { 1303, "Operation Phoenix Key" }, { 1305, "Huntsman Key" }, { 1309, "Operation Breakout Key" },
        { 1313, "Operation Vanguard Key" }, { 1322, "Chroma Key" }, { 1325, "Chroma 2 Key" },
        { 1330, "Falchion Key" }, { 1333, "Shadow Key" }, { 1334, "Revolver Key" },
        { 1347, "Chroma 3 Key" }, { 1350, "Gamma Key" }, { 1351, "Gamma 2 Key" },
        { 1356, "Glove Key" }, { 1364, "Spectrum Key" }, { 1365, "Operation Hydra Key" },
        { 1366, "Spectrum 2 Key" }, { 1373, "Clutch Key" }, { 1374, "Horizon Key" },
        { 1375, "Danger Zone Key" }, { 1383, "Prisma Key" }, { 1384, "CS20 Key" },
        { 1385, "Shattered Web Key" }, { 1391, "Fracture Key" }, { 1398, "Snakebite Key" },
        { 1403, "Dreams & Nightmares Key" }, { 1405, "Recoil Key" }, { 1407, "Revolution Key" },
        { 7000, "Operation Wildfire Key" }
    };
    for (const auto& k : kKeysList) {
        MarketItem mi;
        mi.defIndex = k.def;
        mi.name = k.name;
        mi.category = "Llave";
        mi.quality = 4;
        mi.rarity = 0;
        mi.paintKit = 0;
        mi.wear = 0.0f;
        mi.seed = 0;
        m_marketItems.push_back(mi);
    }

    // Tab 3: Rifles & Snipers
    static const struct { uint32_t def; uint32_t pk; uint32_t rar; const char* name; } kRiflesList[] = {
        { 9,   344, 6, "AWP | Dragon Lore" },
        { 9,   279, 6, "AWP | Asiimov" },
        { 9,   475, 6, "AWP | Hyper Beast" },
        { 9,   446, 6, "AWP | Medusa" },
        { 9,   51,  6, "AWP | Lightning Strike" },
        { 9,   803, 6, "AWP | Wildfire" },
        { 9,   756, 5, "AWP | Containment Breach" },
        { 7,   180, 6, "AK-47 | Fire Serpent" },
        { 7,   801, 6, "AK-47 | Asiimov" },
        { 7,   639, 6, "AK-47 | Bloodsport" },
        { 7,   302, 6, "AK-47 | Vulcan" },
        { 7,   282, 5, "AK-47 | Redline" },
        { 7,   44,  5, "AK-47 | Case Hardened" },
        { 7,   707, 6, "AK-47 | The Empress" },
        { 16,  309, 8, "M4A4 | Howl" },
        { 16,  255, 6, "M4A4 | Asiimov" },
        { 16,  695, 6, "M4A4 | Neo-Noir" },
        { 16,  738, 5, "M4A4 | Buzz Kill" },
        { 16,  384, 5, "M4A4 | Dragon King" },
        { 60,  430, 6, "M4A1-S | Hyper Beast" },
        { 60,  757, 6, "M4A1-S | Printstream" },
        { 60,  644, 5, "M4A1-S | Decimator" },
        { 60,  540, 5, "M4A1-S | Chantico's Fire" },
        { 60,  321, 5, "M4A1-S | Cyrex" },
        { 40,  222, 5, "SSG 08 | Blood in the Water" },
        { 40,  319, 4, "SSG 08 | Slashed" },
        { 39,  287, 5, "SG 553 | Cyrex" },
        { 8,   73,  5, "AUG | Chameleon" },
        { 10,  260, 5, "FAMAS | Pulse" },
        { 13,  379, 5, "Galil AR | Chatterbox" }
    };
    for (const auto& r : kRiflesList) {
        MarketItem mi;
        mi.defIndex = r.def;
        mi.name = r.name;
        mi.category = "Rifle";
        mi.quality = 4;
        mi.rarity = r.rar;
        mi.paintKit = r.pk;
        mi.wear = 0.001f;
        mi.seed = 420;
        m_marketItems.push_back(mi);
    }

    // Tab 4: Pistols
    static const struct { uint32_t def; uint32_t pk; uint32_t rar; const char* name; } kPistolsList[] = {
        { 1,   757, 6, "Desert Eagle | Printstream" },
        { 1,   711, 6, "Desert Eagle | Code Red" },
        { 1,   527, 5, "Desert Eagle | Kumicho Dragon" },
        { 1,   61,  5, "Desert Eagle | Hypnotic" },
        { 1,   231, 5, "Desert Eagle | Conspiracy" },
        { 4,   38,  6, "Glock-18 | Fade" },
        { 4,   353, 5, "Glock-18 | Water Elemental" },
        { 4,   48,  4, "Glock-18 | Dragon Tattoo" },
        { 4,   799, 5, "Glock-18 | Bullet Queen" },
        { 61,  504, 6, "USP-S | Kill Confirmed" },
        { 61,  653, 6, "USP-S | Neo-Noir" },
        { 61,  290, 5, "USP-S | Caiman" },
        { 61,  313, 5, "USP-S | Orion" },
        { 36,  258, 5, "P250 | Mehndi" },
        { 36,  551, 5, "P250 | See Ya Later" },
        { 3,   427, 5, "Five-SeveN | Monkey Business" },
        { 3,   44,  4, "Five-SeveN | Case Hardened" },
        { 30,  179, 5, "Tec-9 | Fuel Injector" },
        { 63,  297, 5, "CZ75-Auto | Victoria" },
        { 64,  522, 5, "R8 Revolver | Fade" }
    };
    for (const auto& p : kPistolsList) {
        MarketItem mi;
        mi.defIndex = p.def;
        mi.name = p.name;
        mi.category = "Pistola";
        mi.quality = 4;
        mi.rarity = p.rar;
        mi.paintKit = p.pk;
        mi.wear = 0.001f;
        mi.seed = 420;
        m_marketItems.push_back(mi);
    }

    // Tab 5: Knives
    static const struct { uint32_t def; uint32_t pk; const char* name; } kKnivesList[] = {
        { 507, 416, "Karambit | Doppler Sapphire" },
        { 507, 415, "Karambit | Doppler Ruby" },
        { 507, 38,  "Karambit | Fade" },
        { 507, 44,  "Karambit | Case Hardened (Blue Gem)" },
        { 507, 562, "Karambit | Lore" },
        { 507, 413, "Karambit | Tiger Tooth" },
        { 515, 38,  "Butterfly Knife | Fade" },
        { 515, 418, "Butterfly Knife | Doppler" },
        { 515, 568, "Butterfly Knife | Gamma Doppler Emerald" },
        { 515, 562, "Butterfly Knife | Lore" },
        { 515, 12,  "Butterfly Knife | Crimson Web" },
        { 508, 562, "M9 Bayonet | Lore" },
        { 508, 418, "M9 Bayonet | Doppler" },
        { 508, 38,  "M9 Bayonet | Fade" },
        { 500, 419, "Bayonet | Doppler Phase 2" },
        { 500, 38,  "Bayonet | Fade" },
        { 523, 413, "Talon Knife | Tiger Tooth" },
        { 523, 416, "Talon Knife | Doppler Sapphire" },
        { 525, 12,  "Skeleton Knife | Crimson Web" },
        { 525, 38,  "Skeleton Knife | Fade" },
        { 516, 415, "Shadow Daggers | Doppler Ruby" },
        { 509, 38,  "Huntsman Knife | Fade" },
        { 522, 413, "Stiletto Knife | Tiger Tooth" },
        { 519, 418, "Ursus Knife | Doppler" },
        { 505, 38,  "Flip Knife | Fade" },
        { 506, 418, "Gut Knife | Doppler" }
    };
    for (const auto& k : kKnivesList) {
        MarketItem mi;
        mi.defIndex = k.def;
        mi.name = k.name;
        mi.category = "Cuchillo";
        mi.quality = 3; // ★ Unusual
        mi.rarity = 7;
        mi.paintKit = k.pk;
        mi.wear = 0.001f;
        mi.seed = 420;
        m_marketItems.push_back(mi);
    }

    // Tab 6: Gloves & Agents
    static const struct { uint32_t def; uint32_t pk; const char* name; } kGlovesList[] = {
        { 5030, 10048, "Sport Gloves | Vice" },
        { 5030, 10045, "Sport Gloves | Pandora's Box" },
        { 5030, 10046, "Sport Gloves | Hedge Maze" },
        { 5031, 10041, "Driver Gloves | King Snake" },
        { 5031, 10044, "Driver Gloves | Imperial Plaid" },
        { 5032, 10053, "Hand Wraps | Cobalt Skulls" },
        { 5032, 10021, "Hand Wraps | Slaughter" },
        { 5033, 10026, "Moto Gloves | Spearmint" },
        { 5034, 10033, "Specialist Gloves | Crimson Kimono" },
        { 5034, 10030, "Specialist Gloves | Emerald Web" }
    };
    for (const auto& g : kGlovesList) {
        MarketItem mi;
        mi.defIndex = g.def;
        mi.name = g.name;
        mi.category = "Guantes";
        mi.quality = 3;
        mi.rarity = 7;
        mi.paintKit = g.pk;
        mi.wear = 0.06f;
        mi.seed = 420;
        m_marketItems.push_back(mi);
    }

    // Tab 7: Stickers
    static const struct { uint32_t def; const char* name; } kStickersList[] = {
        { 6000, "Pegatina | Titan (Holo) | Katowice 2014" },
        { 6001, "Pegatina | iBUYPOWER (Holo) | Katowice 2014" },
        { 6002, "Pegatina | Howling Dawn" },
        { 6003, "Pegatina | Corona (Foil / Crown Foil)" },
        { 6004, "Pegatina | Reason Gaming (Holo) | Katowice 2014" },
        { 6005, "Pegatina | Vox Eminor (Holo) | Katowice 2014" },
        { 6006, "Pegatina | Team Dignitas (Holo) | Katowice 2014" }
    };
    for (const auto& st : kStickersList) {
        MarketItem mi;
        mi.defIndex = st.def;
        mi.name = st.name;
        mi.category = "Pegatina";
        mi.quality = 4;
        mi.rarity = 7;
        mi.paintKit = 0;
        mi.wear = 0.0f;
        mi.seed = 0;
        m_marketItems.push_back(mi);
    }

    // Tab 7: Agentes Oficiales (Operaciones Shattered Web, Broken Fang, Riptide)
    static const struct { uint32_t def; const char* name; uint32_t rar; } kAgentsList[] = {
        { 5038, "Anarquista", 1 },
        { 5039, "Anarquista (Variante A)", 1 },
        { 5040, "Anarquista (Variante B)", 1 },
        { 5041, "Anarquista (Variante C)", 1 },
        { 5042, "Anarquista (Variante D)", 1 },
        { 5043, "Pirata", 1 },
        { 5044, "Pirata (Variante A)", 1 },
        { 5045, "Pirata (Variante B)", 1 },
        { 5046, "Pirata (Variante C)", 1 },
        { 5047, "Pirata (Variante D)", 1 },
        { 5048, "Profesional", 1 },
        { 5049, "Profesional (Variante A)", 1 },
        { 5050, "Profesional (Variante B)", 1 },
        { 5051, "Profesional (Variante C)", 1 },
        { 5052, "Profesional (Variante D)", 1 },
        { 5053, "Separatista", 1 },
        { 5054, "Separatista (Variante A)", 1 },
        { 5055, "Separatista (Variante B)", 1 },
        { 5056, "Separatista (Variante C)", 1 },
        { 5057, "Separatista (Variante D)", 1 },
        { 5058, "GIGN", 1 },
        { 5059, "GIGN (Variante A)", 1 },
        { 5060, "GIGN (Variante B)", 1 },
        { 5061, "GIGN (Variante C)", 1 },
        { 5062, "GIGN (Variante D)", 1 },
        { 5063, "GSG-9", 1 },
        { 5064, "GSG-9 (Variante A)", 1 },
        { 5065, "GSG-9 (Variante B)", 1 },
        { 5066, "GSG-9 (Variante C)", 1 },
        { 5067, "GSG-9 (Variante D)", 1 },
        { 5068, "IDF", 1 },
        { 5069, "IDF (Variante B)", 1 },
        { 5070, "IDF (Variante C)", 1 },
        { 5071, "IDF (Variante D)", 1 },
        { 5072, "IDF (Variante E)", 1 },
        { 5073, "IDF (Variante F)", 1 },
        { 5074, "SWAT", 1 },
        { 5075, "SWAT (Variante A)", 1 },
        { 5076, "SWAT (Variante B)", 1 },
        { 5077, "SWAT (Variante C)", 1 },
        { 5078, "SWAT (Variante D)", 1 },
        { 5079, "SAS (Variante A)", 1 },
        { 5080, "SAS (Variante B)", 1 },
        { 5081, "SAS (Variante C)", 1 },
        { 5082, "SAS (Variante D)", 1 },
        { 5083, "SEAL Team 6", 1 },
        { 5084, "SEAL Team 6 (Variante A)", 1 },
        { 5085, "SEAL Team 6 (Variante B)", 1 },
        { 5086, "SEAL Team 6 (Variante C)", 1 },
        { 5087, "SEAL Team 6 (Variante D)", 1 },
        { 5088, "Balkan (Variante E)", 1 },
        { 5089, "Balkan (Variante A)", 1 },
        { 5090, "Balkan (Variante B)", 1 },
        { 5091, "Balkan (Variante C)", 1 },
        { 5092, "Balkan (Variante D)", 1 },
        { 5093, "Prisionero (Overpass)", 1 },
        { 5094, "Prisionero (Variante B)", 1 },
        { 5095, "Prisionero (Variante C)", 1 },
        { 5096, "Phoenix Pesado (Juggernaut)", 6 },
        { 5097, "CT Pesado (Juggernaut)", 6 },
        { 5100, "Unidad de Elite (Clasico)", 1 },
        { 5101, "Unidad de Elite (Variante A)", 1 },
        { 5102, "Unidad de Elite (Variante B)", 1 },
        { 5103, "Unidad de Elite (Variante C)", 1 },
        { 5104, "Unidad de Elite (Variante D)", 1 },
        { 5105, "Rebelde terrestre | Unidad de Elite", 3 },
        { 5106, "Osiris | Unidad de Elite", 4 },
        { 5107, "Profesor Shahmat | Unidad de Elite", 5 },
        { 5108, "The Elite Mr. Muhlik | Unidad de Elite", 6 },
        { 5200, "Soldado Phoenix", 1 },
        { 5201, "Soldado Phoenix (Var A)", 1 },
        { 5202, "Soldado Phoenix (Var B)", 1 },
        { 5203, "Soldado Phoenix (Var C)", 1 },
        { 5204, "Soldado Phoenix (Var D)", 1 },
        { 5205, "Enforcer | Phoenix", 3 },
        { 5206, "Soldier | Phoenix", 3 },
        { 5207, "Slingshot | Phoenix", 4 },
        { 5300, "Agente del FBI", 1 },
        { 5301, "Agente del FBI (Var A)", 1 },
        { 5302, "Agente del FBI (Var C)", 1 },
        { 5303, "Agente del FBI (Var D)", 1 },
        { 5304, "Agente del FBI (Var E)", 1 },
        { 5305, "Operador de SWAT | FBI SWAT", 3 },
        { 5306, "Markus Delrow | FBI HRT", 4 },
        { 5307, "Agente especial Ava (HRT) | FBI", 5 },
        { 5308, "Agente especial Ava | FBI", 6 },
        { 5400, "Soldado del SEAL Team 6 | ST6", 3 },
        { 5401, "Comando de la 3.a Compania | KSK", 3 },
        { 5402, "Buckshot | SEAL del NSWC", 4 },
        { 5403, "Francotirador de los SEAL | ST6", 5 },
        { 5404, "Capitan de corbeta Ricksaw | SEAL", 6 },
        { 5500, "Blackwolf | Sabre", 4 },
        { 5501, "Rezan el Listo | Sabre", 4 },
        { 5502, "Dragomir | Sabre", 5 },
        { 5503, "Rezan el Camisa Roja | Sabre", 5 },
        { 5504, "El Doctor Romanov | Sabre", 6 },
        { 5600, "Oficial del SAS", 1 },
        { 5601, "Oficial del escuadron B | SAS", 3 },
    };
    for (const auto& ag : kAgentsList) {
        MarketItem mi;
        mi.defIndex = ag.def;
        mi.name = ag.name;
        mi.category = "Agentes";
        mi.quality = 4;
        mi.rarity = ag.rar;
        mi.paintKit = 0;
        mi.wear = 0.0f;
        mi.seed = 0;
        m_marketItems.push_back(mi);
    }


    return true;
}

void SchemaManager::BuildFallbackSchema() {
    // If items_game.txt is not found, load built-in comprehensive database
    LoadFromItemsGameFile("");
}

const CaseDropTable* SchemaManager::GetCaseDrops(uint32_t caseDefIndex) const {
    auto it = m_caseTables.find(caseDefIndex);
    if (it != m_caseTables.end()) {
        return &it->second;
    }
    // Fallback to first available case
    if (!m_caseTables.empty()) {
        return &m_caseTables.begin()->second;
    }
    return nullptr;
}

bool SchemaManager::RollCaseDrop(uint32_t caseDefIndex, uint32_t& outDef, uint32_t& outPaintKit, uint32_t& outRarity, uint32_t& outQuality, float& outWear, uint32_t& outSeed) const {
    const CaseDropTable* dt = GetCaseDrops(caseDefIndex);
    if (!dt) return false;

    static std::mt19937 s_rng((unsigned int)time(nullptr));
    std::uniform_real_distribution<float> probDist(0.0f, 100.0f);
    float roll = probDist(s_rng);

    // Official CS:GO unboxing odds:
    // Special Rare (Gold/Knife): 0.256%
    // Covert (Ancient / Red): 0.639%  (0.256 + 0.639 = 0.895%)
    // Classified (Legendary / Pink): 3.197% (0.895 + 3.197 = 4.092%)
    // Restricted (Mythical / Purple): 15.985% (4.092 + 15.985 = 20.077%)
    // Mil-Spec (Rare / Blue): 79.923% (remaining 100%)
    const std::vector<SkinDrop>* tierList = nullptr;
    uint32_t tierRarity = 3;
    bool isSpecial = false;

    if (roll < 0.256f && (!dt->special.empty() || !m_universalSpecialKnives.empty())) {
        tierList = !dt->special.empty() ? &dt->special : &m_universalSpecialKnives;
        tierRarity = 7;
        isSpecial = true;
    } else if (roll < 0.895f && !dt->ancient.empty()) {
        tierList = &dt->ancient;
        tierRarity = 6;
    } else if (roll < 4.092f && !dt->legendary.empty()) {
        tierList = &dt->legendary;
        tierRarity = 5;
    } else if (roll < 20.077f && !dt->mythical.empty()) {
        tierList = &dt->mythical;
        tierRarity = 4;
    } else {
        tierList = !dt->rare.empty() ? &dt->rare : &dt->mythical;
        tierRarity = 3;
    }

    if (!tierList || tierList->empty()) {
        if (!dt->rare.empty()) tierList = &dt->rare;
        else if (!dt->ancient.empty()) tierList = &dt->ancient;
        else return false;
    }

    std::uniform_int_distribution<size_t> itemDist(0, tierList->size() - 1);
    const SkinDrop& picked = (*tierList)[itemDist(s_rng)];

    outDef = picked.defIndex;
    outPaintKit = picked.paintKit;
    outRarity = tierRarity;

    // StatTrak roll: 10% probability
    std::uniform_int_distribution<int> stDist(0, 9);
    bool isStatTrak = (stDist(s_rng) == 0);
    outQuality = isStatTrak ? 9 : (isSpecial ? 3 : 4);

    // Wear float: random distribution biased toward Factory New / Minimal Wear
    std::uniform_real_distribution<float> wearDist(0.001f, 0.38f);
    outWear = wearDist(s_rng);

    std::uniform_int_distribution<uint32_t> seedDist(1, 999);
    outSeed = seedDist(s_rng);

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
