#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "include/steam/steam_api.h"
#include "include/steam/isteamgamecoordinator.h"
#include "steam_gc_emu.h"
#include "unreal_steam_emu.h"
#include "overlay/refix_overlay.h"
#include "csgo_item_defs.h"

namespace refix::overlay {
    void ExecuteEngineClientCommand(const char* cmd);
    void OpenMatchmakingToast();
}

extern void ReFixLog(const char* fmt, ...);

namespace {

// GC SDK base messages (gcsystemmsgs.proto, EGCBaseClientMsg)
constexpr uint32_t kMsgClientWelcome = 4004;
constexpr uint32_t kMsgServerWelcome = 4005;
constexpr uint32_t kMsgClientHello = 4006;
constexpr uint32_t kMsgServerHello = 4007;
constexpr uint32_t kMsgClientConnectionStatus = 4009;
constexpr uint32_t kMsgServerConnectionStatus = 4010;

// CS:GO specific GC messages
constexpr uint32_t kMsgGCUnlockCrate = 1007;
constexpr uint32_t kMsgGCUnlockCrateResponse = 1008;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingStart = 9101;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingGC2ClientReserve = 9107;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingClient2GCHello = 9109;
constexpr uint32_t kMsgGCCStrike15_v2_MatchmakingGC2ClientHello = 9110;
constexpr uint32_t kMsgGCStorePurchaseInit = 2504;
constexpr uint32_t kMsgGCStorePurchaseInitResponse = 2505;
constexpr uint32_t kMsgGCStorePurchaseInitAlt = 2510;

constexpr uint32_t kProtoFlag = 0x80000000;
constexpr size_t kProtoHeaderSize = 8;
constexpr uint32_t kSOTypeSteamId = 1;

void PutVarint(std::vector<uint8_t>& out, uint64_t value) {
    while (value >= 0x80) { out.push_back((uint8_t)(value | 0x80)); value >>= 7; }
    out.push_back((uint8_t)value);
}

void PutVarintField(std::vector<uint8_t>& out, uint32_t field, uint64_t value) {
    PutVarint(out, (uint64_t)field << 3);
    PutVarint(out, value);
}

void PutFixed64Field(std::vector<uint8_t>& out, uint32_t field, uint64_t value) {
    PutVarint(out, ((uint64_t)field << 3) | 1);
    for (int i = 0; i < 8; i++) out.push_back((uint8_t)(value >> (8 * i)));
}

void PutFixed32Field(std::vector<uint8_t>& out, uint32_t field, uint32_t value) {
    PutVarint(out, ((uint64_t)field << 3) | 5);
    for (int i = 0; i < 4; i++) out.push_back((uint8_t)(value >> (8 * i)));
}

void PutBytesField(std::vector<uint8_t>& out, uint32_t field, const std::vector<uint8_t>& bytes) {
    PutVarint(out, ((uint64_t)field << 3) | 2);
    PutVarint(out, bytes.size());
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void PutStringField(std::vector<uint8_t>& out, uint32_t field, const std::string& str) {
    std::vector<uint8_t> bytes(str.begin(), str.end());
    PutBytesField(out, field, bytes);
}

bool GetVarint(const uint8_t*& p, const uint8_t* end, uint64_t& value) {
    value = 0;
    for (int shift = 0; p < end && shift < 64; shift += 7) {
        uint8_t b = *p++;
        value |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

struct EconItem {
    uint64_t id = 0;
    uint32_t defIndex = 0;
    uint32_t quantity = 1;
    uint32_t quality = 4;
    uint32_t rarity = 0;
    uint32_t paintKit = 0;
    float wear = 0.001f;
    uint32_t seed = 0;
    std::string customName;
};

struct EconCoupon {
    uint32_t entryId = 0;
    uint32_t defidx = 0;
    uint32_t expirationDate = 0x7FFFFFFF;
};

class CSGOInventoryManager {
public:
    static CSGOInventoryManager& Get() {
        static CSGOInventoryManager inst;
        return inst;
    }

    std::wstring GetInventoryPath() {
        std::wstring root = ReFixOverlay::InventoryFolder(730);
        if (root.empty()) {
            wchar_t appData[MAX_PATH] = {};
            GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
            root = std::wstring(appData) + L"\\ReFix\\Inventory\\730";
        }
        for (size_t i = 3; i <= root.size(); ++i) {
            if (i == root.size() || root[i] == L'\\')
                CreateDirectoryW(root.substr(0, i).c_str(), nullptr);
        }
        return root + L"\\inventory.json";
    }

    void EnsureLoaded() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_loaded) return;
        m_loaded = true;

        std::wstring path = GetInventoryPath();
        std::ifstream f(path, std::ios::binary);
        if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            f.close();
            ParseJson(content);

            // Automatically upgrade legacy items lacking paint kit
            bool changed = false;
            for (auto& itm : m_items) {
                if (itm.paintKit == 0) {
                    if (itm.defIndex == 9) { itm.paintKit = 344; itm.rarity = 6; changed = true; } // AWP Dragon Lore
                    else if (itm.defIndex == 7) { itm.paintKit = 180; itm.rarity = 6; changed = true; } // AK-47 Fire Serpent
                    else if (itm.defIndex == 16) { itm.paintKit = 309; itm.rarity = 8; changed = true; } // M4A4 Howl
                    else if (itm.defIndex == 60) { itm.paintKit = 430; itm.rarity = 6; changed = true; } // M4A1-S Hyper Beast
                    else if (itm.defIndex == 1) { itm.paintKit = 757; itm.rarity = 6; changed = true; } // Deagle Printstream
                    else if (itm.defIndex == 4) { itm.paintKit = 38; itm.rarity = 6; changed = true; } // Glock Fade
                    else if (itm.defIndex == 61) { itm.paintKit = 504; itm.rarity = 6; changed = true; } // USP Kill Confirmed
                    else if (itm.defIndex == 507) { itm.paintKit = 416; itm.rarity = 7; changed = true; } // Karambit Sapphire
                    else if (itm.defIndex == 515) { itm.paintKit = 38; itm.rarity = 7; changed = true; } // Butterfly Fade
                    else if (itm.defIndex == 508) { itm.paintKit = 562; itm.rarity = 7; changed = true; } // M9 Lore
                    else if (itm.defIndex == 500) { itm.paintKit = 419; itm.rarity = 7; changed = true; } // Bayonet Doppler
                    else if (itm.defIndex == 523) { itm.paintKit = 413; itm.rarity = 7; changed = true; } // Talon Tiger Tooth
                    else if (itm.defIndex == 516) { itm.paintKit = 415; itm.rarity = 7; changed = true; } // Shadow Daggers Ruby
                    else if (itm.defIndex >= 500 && itm.defIndex <= 530) { itm.paintKit = 38; itm.rarity = 7; changed = true; }
                }
            }
            if (changed) SaveLocked();
        }

        if (m_items.empty()) {
            // First time setup: Grant all cases & keys to test the inventory!
            uint64_t nextId = 1000000000ULL;
            // Grant starter skins and knives with real paint kits
            struct StarterSkin {
                uint32_t defIndex;
                uint32_t paintKit;
                uint32_t rarity;
            };
            static const StarterSkin kStarters[] = {
                { 9,   344, 6 }, // AWP | Dragon Lore
                { 7,   180, 6 }, // AK-47 | Fire Serpent
                { 16,  309, 8 }, // M4A4 | Howl
                { 60,  430, 6 }, // M4A1-S | Hyper Beast
                { 1,   757, 6 }, // Desert Eagle | Printstream
                { 4,   38,  6 }, // Glock-18 | Fade
                { 61,  504, 6 }, // USP-S | Kill Confirmed
                { 507, 416, 7 }, // Karambit | Doppler Sapphire
                { 515, 38,  7 }, // Butterfly Knife | Fade
                { 508, 562, 7 }, // M9 Bayonet | Lore
                { 500, 419, 7 }, // Bayonet | Doppler P2
                { 523, 413, 7 }, // Talon Knife | Tiger Tooth
                { 516, 415, 7 }, // Shadow Daggers | Doppler Ruby
                { 509, 38,  7 }, // Huntsman Knife | Fade
                { 522, 413, 7 }, // Stiletto Knife | Tiger Tooth
                { 525, 12,  7 }, // Skeleton Knife | Crimson Web
                { 5030, 10048, 7 }, // Sport Gloves | Vice
                { 5031, 10041, 7 }  // Driver Gloves | King Snake
            };
            for (const auto& s : kStarters) {
                EconItem itm;
                itm.id = ++nextId;
                itm.defIndex = s.defIndex;
                itm.quantity = 1;
                itm.quality = 4;
                itm.rarity = s.rarity;
                itm.paintKit = s.paintKit;
                itm.wear = 0.001f;
                itm.seed = 420;
                m_items.push_back(itm);
            }
            // Then keys
            for (size_t i = 0; i < ReFixGC::CSGO::kNumKeys; ++i) {
                EconItem itm;
                itm.id = ++nextId;
                itm.defIndex = ReFixGC::CSGO::kKeys[i];
                itm.quantity = 20; // 20 keys each for easy testing
                itm.quality = 4;
                m_items.push_back(itm);
            }
            // Then 36 cases last with highest IDs (displayed at top left under 'MÁS RECIENTE')
            for (size_t i = 0; i < ReFixGC::CSGO::kNumCases; ++i) {
                EconItem itm;
                itm.id = ++nextId;
                itm.defIndex = ReFixGC::CSGO::kCases[i];
                itm.quantity = 10; // 10 of every real weapon case
                itm.quality = 4;
                m_items.push_back(itm);
            }

            // 4 rotating coupons for main menu store offers
            uint32_t defCoupons[] = { 20000, 20004, 20007, 20020 };
            for (uint32_t i = 0; i < 4; ++i) {
                EconCoupon cpn;
                cpn.entryId = i + 1;
                cpn.defidx = (i < ReFixGC::CSGO::kNumCoupons) ? ReFixGC::CSGO::kCoupons[i] : defCoupons[i];
                cpn.expirationDate = 0x7FFFFFFF;
                m_coupons.push_back(cpn);
            }
            SaveLocked();
            ReFixLog("[GC] CSGOInventory: Initialized default inventory with %zu cases, %zu keys, and %zu store offers",
                     ReFixGC::CSGO::kNumCases, ReFixGC::CSGO::kNumKeys, m_coupons.size());
        }
    }

    std::vector<EconItem> GetItems() {
        EnsureLoaded();
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_items;
    }

    std::vector<EconCoupon> GetCoupons() {
        EnsureLoaded();
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_coupons;
    }

    uint64_t UnlockCrate(uint64_t crateId, uint64_t keyId) {
        EnsureLoaded();
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find crate by item ID or defIndex
        auto itCrate = std::find_if(m_items.begin(), m_items.end(), [crateId](const EconItem& it) {
            return it.id == crateId || (crateId != 0 && it.defIndex == (uint32_t)crateId);
        });
        if (itCrate == m_items.end()) {
            // Fallback: pick any crate in inventory
            for (auto it = m_items.begin(); it != m_items.end(); ++it) {
                for (size_t c = 0; c < ReFixGC::CSGO::kNumCases; ++c) {
                    if (it->defIndex == ReFixGC::CSGO::kCases[c]) {
                        itCrate = it;
                        break;
                    }
                }
                if (itCrate != m_items.end()) break;
            }
        }
        if (itCrate == m_items.end()) return 0;

        // Find key by item ID or defIndex
        auto itKey = std::find_if(m_items.begin(), m_items.end(), [keyId](const EconItem& it) {
            return it.id == keyId || (keyId != 0 && it.defIndex == (uint32_t)keyId);
        });
        if (itKey == m_items.end()) {
            // Fallback: pick any key in inventory
            for (auto it = m_items.begin(); it != m_items.end(); ++it) {
                for (size_t k = 0; k < ReFixGC::CSGO::kNumKeys; ++k) {
                    if (it->defIndex == ReFixGC::CSGO::kKeys[k]) {
                        itKey = it;
                        break;
                    }
                }
                if (itKey != m_items.end()) break;
            }
        }
        if (itKey != m_items.end()) {
            if (itKey->quantity > 1) itKey->quantity--;
            else m_items.erase(itKey);
        }

        // Consume crate
        if (itCrate->quantity > 1) itCrate->quantity--;
        else m_items.erase(itCrate);

        // Award weapon skin
        uint64_t maxId = 2000000000ULL;
        for (const auto& it : m_items) {
            if (it.id > maxId) maxId = it.id;
        }

        struct DropEntry {
            uint32_t defIndex;
            uint32_t paintKit;
            uint32_t quality;
            uint32_t rarity;
        };

        static const DropEntry kCrateDrops[] = {
            { 9,   344, 4, 6 }, // AWP | Dragon Lore
            { 9,   279, 4, 6 }, // AWP | Asiimov
            { 9,   475, 4, 6 }, // AWP | Hyper Beast
            { 9,   446, 4, 6 }, // AWP | Medusa
            { 7,   180, 4, 6 }, // AK-47 | Fire Serpent
            { 7,   801, 4, 6 }, // AK-47 | Asiimov
            { 7,   639, 4, 6 }, // AK-47 | Bloodsport
            { 7,   302, 4, 6 }, // AK-47 | Vulcan
            { 7,   282, 4, 5 }, // AK-47 | Redline
            { 16,  309, 4, 8 }, // M4A4 | Howl
            { 16,  255, 4, 6 }, // M4A4 | Asiimov
            { 16,  695, 4, 6 }, // M4A4 | Neo-Noir
            { 60,  430, 4, 6 }, // M4A1-S | Hyper Beast
            { 60,  757, 4, 6 }, // M4A1-S | Printstream
            { 60,  644, 4, 5 }, // M4A1-S | Decimator
            { 1,   757, 4, 6 }, // Desert Eagle | Printstream
            { 1,   711, 4, 6 }, // Desert Eagle | Code Red
            { 1,   527, 4, 5 }, // Desert Eagle | Kumicho Dragon
            { 4,   38,  4, 6 }, // Glock-18 | Fade
            { 4,   353, 4, 5 }, // Glock-18 | Water Elemental
            { 61,  504, 4, 6 }, // USP-S | Kill Confirmed
            { 61,  653, 4, 6 }, // USP-S | Neo-Noir
            { 507, 416, 4, 7 }, // Karambit | Doppler Sapphire
            { 507, 415, 4, 7 }, // Karambit | Doppler Ruby
            { 507, 38,  4, 7 }, // Karambit | Fade
            { 515, 38,  4, 7 }, // Butterfly Knife | Fade
            { 515, 418, 4, 7 }, // Butterfly Knife | Doppler
            { 515, 568, 4, 7 }, // Butterfly Knife | Gamma Doppler Emerald
            { 508, 562, 4, 7 }, // M9 Bayonet | Lore
            { 508, 418, 4, 7 }, // M9 Bayonet | Doppler
            { 500, 419, 4, 7 }, // Bayonet | Doppler P2
            { 523, 413, 4, 7 }, // Talon Knife | Tiger Tooth
            { 525, 12,  4, 7 }, // Skeleton Knife | Crimson Web
            { 516, 415, 4, 7 }, // Shadow Daggers | Doppler Ruby
            { 5030, 10048, 4, 7 }, // Sport Gloves | Vice
            { 5031, 10041, 4, 7 }  // Driver Gloves | King Snake
        };
        static const size_t kNumCrateDrops = sizeof(kCrateDrops) / sizeof(kCrateDrops[0]);

        static unsigned int s_seed = (unsigned int)time(nullptr);
        s_seed = s_seed * 1103515245 + 12347;
        const auto& drop = kCrateDrops[s_seed % kNumCrateDrops];

        EconItem unboxed;
        unboxed.id = maxId + 1;
        unboxed.defIndex = drop.defIndex;
        unboxed.quantity = 1;
        unboxed.quality = ((s_seed % 10) == 0) ? 9 : drop.quality; // 10% StatTrak chance
        unboxed.rarity = drop.rarity;
        unboxed.paintKit = drop.paintKit;
        unboxed.wear = 0.001f + ((s_seed % 20) * 0.001f);
        unboxed.seed = (s_seed % 999) + 1;
        m_items.push_back(unboxed);

        SaveLocked();
        ReFixLog("[GC] CSGOInventory: Unlocked crate! Awarded item %llu (def %u, rarity %u)",
                 (unsigned long long)unboxed.id, unboxed.defIndex, unboxed.rarity);
        return unboxed.id;
    }

    uint64_t AddItem(uint32_t defIndex, uint32_t count = 1, uint32_t quality = 4, uint32_t rarity = 0) {
        EnsureLoaded();
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t maxId = 2000000000ULL;
        for (const auto& i : m_items) if (i.id > maxId) maxId = i.id;

        auto it = std::find_if(m_items.begin(), m_items.end(), [defIndex](const EconItem& it) {
            return it.defIndex == defIndex;
        });
        if (it != m_items.end()) {
            it->quantity += count;
            SaveLocked();
            return it->id;
        }

        EconItem itm;
        itm.id = maxId + 1;
        itm.defIndex = defIndex;
        itm.quantity = count;
        itm.quality = quality;
        itm.rarity = rarity;
        m_items.push_back(itm);
        SaveLocked();
        ReFixLog("[GC] CSGOInventory: Added item id=%llu def=%u qty=%u", (unsigned long long)itm.id, defIndex, count);
        return itm.id;
    }

    uint64_t AddItemWithAttributes(uint32_t defIndex, uint32_t count, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed) {
        EnsureLoaded();
        std::lock_guard<std::mutex> lock(m_mutex);

        uint64_t maxId = 2000000000ULL;
        for (const auto& i : m_items) if (i.id > maxId) maxId = i.id;

        if (paintKit == 0) {
            auto it = std::find_if(m_items.begin(), m_items.end(), [defIndex](const EconItem& it) {
                return it.defIndex == defIndex && it.paintKit == 0;
            });
            if (it != m_items.end()) {
                it->quantity += count;
                SaveLocked();
                return it->id;
            }
        }

        EconItem itm;
        itm.id = maxId + 1;
        itm.defIndex = defIndex;
        itm.quantity = count;
        itm.quality = quality;
        itm.rarity = rarity;
        itm.paintKit = paintKit;
        itm.wear = wear > 0.0f ? wear : 0.001f;
        itm.seed = seed > 0 ? seed : (rand() % 999 + 1);
        m_items.push_back(itm);
        SaveLocked();
        ReFixLog("[GC] CSGOInventory: Added custom item id=%llu def=%u qty=%u pk=%u wear=%.4f",
                 (unsigned long long)itm.id, defIndex, count, paintKit, itm.wear);
        return itm.id;
    }

    void GrantKeys(uint32_t defIndex, uint32_t count) {
        AddItem(defIndex, count, 4, 0);
    }

private:
    std::mutex m_mutex;
    bool m_loaded = false;
    std::vector<EconItem> m_items;
    std::vector<EconCoupon> m_coupons;

    void SaveLocked() {
        std::wstring path = GetInventoryPath();
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) return;

        ofs << "{\n  \"items\": [\n";
        for (size_t i = 0; i < m_items.size(); ++i) {
            const auto& it = m_items[i];
            ofs << "    { \"id\": " << it.id << ", \"defIndex\": " << it.defIndex
                << ", \"quantity\": " << it.quantity << ", \"quality\": " << it.quality
                << ", \"rarity\": " << it.rarity << ", \"paintKit\": " << it.paintKit
                << ", \"wear\": " << it.wear << ", \"seed\": " << it.seed << " }" << (i + 1 < m_items.size() ? ",\n" : "\n");
        }
        ofs << "  ],\n  \"coupons\": [\n";
        for (size_t i = 0; i < m_coupons.size(); ++i) {
            const auto& cp = m_coupons[i];
            ofs << "    { \"entryId\": " << cp.entryId << ", \"defidx\": " << cp.defidx
                << ", \"expirationDate\": " << cp.expirationDate << " }" << (i + 1 < m_coupons.size() ? ",\n" : "\n");
        }
        ofs << "  ]\n}\n";
    }

    void ParseJson(const std::string& str) {
        // Simple regex / token parser for json
        size_t itemsPos = str.find("\"items\"");
        if (itemsPos != std::string::npos) {
            size_t endItems = str.find("\"coupons\"", itemsPos);
            std::string itemsBlock = str.substr(itemsPos, endItems == std::string::npos ? std::string::npos : endItems - itemsPos);
            size_t pos = 0;
            while ((pos = itemsBlock.find('{', pos)) != std::string::npos) {
                size_t close = itemsBlock.find('}', pos);
                if (close == std::string::npos) break;
                std::string obj = itemsBlock.substr(pos, close - pos + 1);
                auto getNum = [&](const char* key) -> uint64_t {
                    size_t kp = obj.find(key);
                    if (kp == std::string::npos) return 0;
                    size_t col = obj.find(':', kp);
                    if (col == std::string::npos) return 0;
                    return _strtoui64(obj.c_str() + col + 1, nullptr, 10);
                };
                EconItem itm;
                itm.id = getNum("\"id\"");
                itm.defIndex = (uint32_t)getNum("\"defIndex\"");
                itm.quantity = (uint32_t)getNum("\"quantity\"");
                itm.quality = (uint32_t)getNum("\"quality\"");
                itm.rarity = (uint32_t)getNum("\"rarity\"");
                itm.paintKit = (uint32_t)getNum("\"paintKit\"");
                itm.seed = (uint32_t)getNum("\"seed\"");
                size_t wp = obj.find("\"wear\"");
                if (wp != std::string::npos) {
                    size_t wc = obj.find(':', wp);
                    if (wc != std::string::npos) {
                        itm.wear = (float)atof(obj.c_str() + wc + 1);
                    }
                }
                if (itm.wear <= 0.0f) itm.wear = 0.001f;
                if (itm.id != 0 && itm.defIndex != 0) {
                    m_items.push_back(itm);
                }
                pos = close + 1;
            }
        }

        size_t cpnPos = str.find("\"coupons\"");
        if (cpnPos != std::string::npos) {
            std::string cpnBlock = str.substr(cpnPos);
            size_t pos = 0;
            while ((pos = cpnBlock.find('{', pos)) != std::string::npos) {
                size_t close = cpnBlock.find('}', pos);
                if (close == std::string::npos) break;
                std::string obj = cpnBlock.substr(pos, close - pos + 1);
                auto getNum = [&](const char* key) -> uint64_t {
                    size_t kp = obj.find(key);
                    if (kp == std::string::npos) return 0;
                    size_t col = obj.find(':', kp);
                    if (col == std::string::npos) return 0;
                    return _strtoui64(obj.c_str() + col + 1, nullptr, 10);
                };
                EconCoupon cpn;
                cpn.entryId = (uint32_t)getNum("\"entryId\"");
                cpn.defidx = (uint32_t)getNum("\"defidx\"");
                cpn.expirationDate = (uint32_t)getNum("\"expirationDate\"");
                if (cpn.defidx != 0) {
                    m_coupons.push_back(cpn);
                }
                pos = close + 1;
            }
        }
    }
};

std::vector<uint8_t> SerializeEconItem(const EconItem& item, uint32_t accountId, uint32_t pos) {
    std::vector<uint8_t> out;
    PutVarintField(out, 1, item.id);
    PutVarintField(out, 2, accountId);
    PutVarintField(out, 3, pos);
    PutVarintField(out, 4, item.defIndex);
    PutVarintField(out, 5, item.quantity);
    PutVarintField(out, 6, 1); // level

    // For Knives: CS:GO knives require quality 3 (★ Unusual) and rarity 6 (Covert)
    uint32_t quality = item.quality;
    if (quality == 4 && item.defIndex >= 500 && item.defIndex <= 530) {
        quality = 3; // ★ Unusual
    }
    PutVarintField(out, 7, quality);
    PutVarintField(out, 8, 0); // flags
    PutVarintField(out, 9, 0); // origin
    if (!item.customName.empty()) {
        PutStringField(out, 10, item.customName);
    }

    if (item.paintKit > 0) {
        // Attribute 6: paint kit (set item texture prefab) - stored_as_integer = 0 (FLOAT value_bytes only)
        float fpk = (float)item.paintKit;
        std::vector<uint8_t> attr6;
        PutVarintField(attr6, 1, 6);
        PutBytesField(attr6, 3, std::vector<uint8_t>((uint8_t*)&fpk, (uint8_t*)&fpk + 4));
        PutBytesField(out, 12, attr6);

        // Attribute 8: wear (set item texture wear) - stored_as_integer = 0 (FLOAT value_bytes only)
        float fwear = item.wear > 0.0f ? item.wear : 0.001f;
        std::vector<uint8_t> attr8;
        PutVarintField(attr8, 1, 8);
        PutBytesField(attr8, 3, std::vector<uint8_t>((uint8_t*)&fwear, (uint8_t*)&fwear + 4));
        PutBytesField(out, 12, attr8);

        // Attribute 7: seed (set item texture seed) - stored_as_integer = 0 (FLOAT value_bytes only)
        uint32_t seedVal = item.seed > 0 ? item.seed : 420;
        float fseed = (float)seedVal;
        std::vector<uint8_t> attr7;
        PutVarintField(attr7, 1, 7);
        PutBytesField(attr7, 3, std::vector<uint8_t>((uint8_t*)&fseed, (uint8_t*)&fseed + 4));
        PutBytesField(out, 12, attr7);

        // StatTrak: Attribute 80 (kill eater)
        if (quality == 9) {
            uint32_t zero = 0;
            std::vector<uint8_t> attr80;
            PutVarintField(attr80, 1, 80);
            PutVarintField(attr80, 2, 0);
            PutBytesField(attr80, 3, std::vector<uint8_t>((uint8_t*)&zero, (uint8_t*)&zero + 4));
            PutBytesField(out, 12, attr80);
        }
    }

    uint32_t rarity = item.rarity;
    if (rarity == 0 && item.defIndex >= 500 && item.defIndex <= 530) {
        rarity = 6;
    }
    if (rarity) PutVarintField(out, 19, rarity);
    return out;
}

std::vector<uint8_t> SerializeEconCoupon(const EconCoupon& coupon) {
    std::vector<uint8_t> out;
    PutVarintField(out, 1, coupon.entryId);
    PutVarintField(out, 2, coupon.defidx);
    PutVarintField(out, 3, coupon.expirationDate);
    return out;
}

class CGameCoordinatorLan : public ISteamGameCoordinator {
public:
    struct Message {
        uint32_t type;
        std::vector<uint8_t> data;
    };

    const char* Role() const { return m_gameServer ? "server" : "client"; }

    explicit CGameCoordinatorLan(bool gameServer) : m_gameServer(gameServer) {}

    static std::vector<uint8_t> BuildMatchmakingClientHello(uint32_t accountId) {
        std::vector<uint8_t> ranking;
        PutVarintField(ranking, 1, accountId);
        PutVarintField(ranking, 2, 18);  // Global Elite (rank 18)
        PutVarintField(ranking, 3, 150); // 150 Competitive wins
        PutVarintField(ranking, 6, 6);   // 5v5 Competitive rank type

        std::vector<uint8_t> commendation;
        PutVarintField(commendation, 1, 100);
        PutVarintField(commendation, 2, 100);
        PutVarintField(commendation, 4, 100);

        std::vector<uint8_t> medals;
        PutVarintField(medals, 7, 874); // 5 Year Veteran Coin (874)
        PutVarintField(medals, 8, 874); // featured display item

        std::vector<uint8_t> resp;
        PutVarintField(resp, 1, accountId);
        PutVarintField(resp, 6, 0);       // vac_banned = 0 (clean!)
        PutBytesField(resp, 7, ranking);
        PutBytesField(resp, 8, commendation);
        PutBytesField(resp, 9, medals);
        PutVarintField(resp, 16, 40);     // player_level = 40 (General de rango 40)
        PutVarintField(resp, 17, 40);     // ranking_type_id / player_level
        PutVarintField(resp, 18, 327680000 + 3500); // player_cur_xp
        PutVarintField(resp, 19, 0);      // player_xp_bonus_flags
        return resp;
    }

    EGCResults SendMessage_(uint32 unMsgType, const void* pubData, uint32 cubData) override {
        const uint32_t msg = unMsgType & ~kProtoFlag;
        const bool proto = (unMsgType & kProtoFlag) != 0;
        if (m_logged++ < 32)
            ReFixLog("[GC] %s sent %u%s (%u bytes)", Role(), msg, proto ? " (proto)" : "", cubData);
        if (!pubData || cubData < 4) return k_EGCResultOK;

        const uint8_t* bytes = (const uint8_t*)pubData;
        uint32_t headerLen = 0;
        const uint8_t* body = nullptr;
        size_t bodySize = 0;

        if (proto && cubData >= kProtoHeaderSize) {
            memcpy(&headerLen, bytes + 4, sizeof(headerLen));
            body = bytes + kProtoHeaderSize + headerLen;
            bodySize = (kProtoHeaderSize + headerLen <= cubData) ? (cubData - (kProtoHeaderSize + headerLen)) : 0;
        }

        if (!m_gameServer && msg == kMsgClientHello) {
            uint32_t version = ReFixGC::ParseHelloVersion(body, bodySize);
            uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            Reply(kMsgClientWelcome, ReFixGC::BuildClientWelcome(version, UnrealSteamEmu::GetLocalSteamID()));
            Reply(kMsgClientConnectionStatus, ReFixGC::BuildConnectionStatus());
            Reply(kMsgGCCStrike15_v2_MatchmakingGC2ClientHello, BuildMatchmakingClientHello(accountId));
        } else if (m_gameServer && msg == kMsgServerHello) {
            uint32_t version = ReFixGC::ParseHelloVersion(body, bodySize);
            if (version == 0) version = 13884;
            std::vector<uint8_t> welcome;
            PutVarintField(welcome, 1, version);
            PutVarintField(welcome, 2, version);
            Reply(kMsgServerWelcome, welcome);
            Reply(kMsgServerConnectionStatus, ReFixGC::BuildConnectionStatus());
        } else if (msg == kMsgGCCStrike15_v2_MatchmakingClient2GCHello) {
            uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            Reply(kMsgGCCStrike15_v2_MatchmakingGC2ClientHello, BuildMatchmakingClientHello(accountId));
        } else if (msg == kMsgGCCStrike15_v2_MatchmakingStart) {
            ReFixLog("[GC] kMsgGCCStrike15_v2_MatchmakingStart: Stopping queue cleanly and opening ReFix Matchmaking toast");
            // Reply with 9104 (MatchmakingGC2ClientUpdate): matchmaking = 0 (stops queue cleanly with no error modal)
            std::vector<uint8_t> updateResp;
            PutVarintField(updateResp, 1, 0); // matchmaking status = 0 (idle)
            Reply(9104, updateResp);

            // Cancel engine matchmaking search so it doesn't fail with VAC error or timeout
            refix::overlay::ExecuteEngineClientCommand("mm_match_search_cancel");

            // Open ReFix Matchmaking bottom-right toast mini-widget
            refix::overlay::OpenMatchmakingToast();
        } else if (msg == 9194) { // ClientGCRankUpdate
            uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            std::vector<uint8_t> ranking;
            PutVarintField(ranking, 1, accountId);
            PutVarintField(ranking, 2, 18);  // Global Elite
            PutVarintField(ranking, 3, 150); // 150 wins
            PutVarintField(ranking, 6, 6);   // 5v5 Competitive
            std::vector<uint8_t> rankResp;
            PutBytesField(rankResp, 1, ranking);
            Reply(9194, rankResp);
        } else if (msg == 9158) { // AccountPrivacySettings
            std::vector<uint8_t> s1;
            PutVarintField(s1, 1, 1);
            PutVarintField(s1, 2, 1);
            std::vector<uint8_t> s2;
            PutVarintField(s2, 1, 2);
            PutVarintField(s2, 2, 1);
            std::vector<uint8_t> privacyResp;
            PutBytesField(privacyResp, 1, s1);
            PutBytesField(privacyResp, 1, s2);
            Reply(9158, privacyResp);
        } else if (msg == 9127) { // ClientRequestPlayersProfile
            uint32_t reqId = 1;
            uint32_t targetAccountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            if (proto && body) {
                const uint8_t* p = body;
                const uint8_t* end = body + bodySize;
                while (p < end) {
                    uint64_t tag = 0, val = 0;
                    if (!GetVarint(p, end, tag)) break;
                    uint32_t f = (uint32_t)(tag >> 3);
                    uint32_t w = tag & 7;
                    if (w == 0) {
                        if (!GetVarint(p, end, val)) break;
                        if (f == 1) reqId = (uint32_t)val;
                        else if (f == 2) targetAccountId = (uint32_t)val;
                    } else if (w == 1) { if (end - p < 8) break; p += 8; }
                    else if (w == 5) { if (end - p < 4) break; p += 4; }
                    else if (w == 2) {
                        if (!GetVarint(p, end, val) || (size_t)(end - p) < val) break;
                        p += val;
                    }
                }
            }
            std::vector<uint8_t> profile = BuildMatchmakingClientHello(targetAccountId);
            std::vector<uint8_t> playersProfileResp;
            PutVarintField(playersProfileResp, 1, reqId);
            PutBytesField(playersProfileResp, 2, profile);
            Reply(9128, playersProfileResp);
        } else if (msg == 9197) { // ClientPartyJoinRelay
            std::vector<uint8_t> relayResp;
            PutVarintField(relayResp, 1, 0);
            Reply(9198, relayResp);
        } else if (msg == 9201) { // GetEventFavorites_Request
            std::vector<uint8_t> favResp;
            Reply(9203, favResp);
        } else if (msg == kMsgGCUnlockCrate) {
            // Unlock container (CS:GO sends non-proto 34-byte struct or proto)
            uint64_t crateId = 0, keyId = 0;
            if (!proto && cubData >= 34) {
                // Non-proto struct format: 18-byte GC header + 8 bytes crateId + 8 bytes keyId
                memcpy(&crateId, bytes + 18, sizeof(crateId));
                memcpy(&keyId, bytes + 26, sizeof(keyId));
                ReFixLog("[GC] UnlockCrate (non-proto): crateId=%llu keyId=%llu",
                         (unsigned long long)crateId, (unsigned long long)keyId);
            } else if (proto && body) {
                const uint8_t* p = body;
                const uint8_t* end = body + bodySize;
                while (p < end) {
                    uint64_t tag = 0, val = 0;
                    if (!GetVarint(p, end, tag)) break;
                    uint32_t f = (uint32_t)(tag >> 3);
                    uint32_t w = tag & 7;
                    if (w == 0) {
                        if (!GetVarint(p, end, val)) break;
                        if (f == 1) crateId = val;
                        else if (f == 2) keyId = val;
                    } else if (w == 1) { if (end - p < 8) break; p += 8; }
                    else if (w == 5) { if (end - p < 4) break; p += 4; }
                    else if (w == 2) {
                        if (!GetVarint(p, end, val) || (size_t)(end - p) < val) break;
                        p += val;
                    }
                }
            }

            uint64_t awardedId = CSGOInventoryManager::Get().UnlockCrate(crateId, keyId);

            if (awardedId != 0) {
                // 1. Resend full SOCache welcome so Panorama knows all inventory items
                Reply(kMsgClientWelcome, ReFixGC::BuildClientWelcome(1, UnrealSteamEmu::GetLocalSteamID()));

                // 2. Dispatch k_ESOMsg_Create (21) CMsgSOSingleObject so active SOCache adds it directly
                auto allItems = CSGOInventoryManager::Get().GetItems();
                auto itAwarded = std::find_if(allItems.begin(), allItems.end(), [awardedId](const EconItem& i) {
                    return i.id == awardedId;
                });
                if (itAwarded != allItems.end()) {
                    uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
                    std::vector<uint8_t> itBytes = SerializeEconItem(*itAwarded, accountId, (uint32_t)allItems.size());

                    std::vector<uint8_t> soid;
                    PutVarintField(soid, 1, kSOTypeSteamId);
                    PutVarintField(soid, 2, UnrealSteamEmu::GetLocalSteamID());

                    static uint64_t s_soVersion = 100;
                    std::vector<uint8_t> soSingle;
                    PutVarintField(soSingle, 2, 1); // type_id = 1 (CSOEconItem)
                    PutBytesField(soSingle, 3, itBytes);
                    PutFixed64Field(soSingle, 4, ++s_soVersion);
                    PutBytesField(soSingle, 5, soid);
                    Reply(21 /* k_ESOMsg_Create */, soSingle);
                }

                // 3. Dispatch UnlockCrateResponse (1008)
                std::vector<uint8_t> unlockResp;
                PutVarintField(unlockResp, 1, 1); // 1 = k_EResultOK
                PutVarintField(unlockResp, 2, awardedId);
                Reply(kMsgGCUnlockCrateResponse, unlockResp);

                // 4. Dispatch CMsgGCItemCustomizationNotification (EMsg 1090)
                std::vector<uint8_t> notif;
                PutVarintField(notif, 1, awardedId);
                PutVarintField(notif, 2, 1007 /* k_EGCItemCustomizationNotification_UnlockCrate */);
                Reply(1090, notif);
            }
        } else if (msg == kMsgGCStorePurchaseInit || msg == kMsgGCStorePurchaseInitAlt) {
            // Purchase keys or items -> Intercept with ReFix Overlay Purchase Listener!
            uint32_t defIndex = 0, count = 1;
            const uint8_t* p = body;
            const uint8_t* end = body + bodySize;
            while (p < end) {
                uint64_t tag = 0, val = 0;
                if (!GetVarint(p, end, tag)) break;
                uint32_t f = (tag >> 3);
                uint32_t w = tag & 7;
                if (w == 0) {
                    if (!GetVarint(p, end, val)) break;
                    if (f == 1) defIndex = (uint32_t)val;
                    else if (f == 2) count = (uint32_t)val;
                } else if (w == 1) { if (end - p < 8) break; p += 8; }
                else if (w == 5) { if (end - p < 4) break; p += 4; }
                else if (w == 2) {
                    if (!GetVarint(p, end, val) || (size_t)(end - p) < val) break;
                    p += val;
                }
            }
            if (defIndex == 0) defIndex = 1203; // Default CS:GO Case Key

            char itemName[128];
            sprintf_s(itemName, "CS:GO Item #%u (Qty: %u)", defIndex, count);
            ReFixLog("[GC] Store purchase intercepted: %s. Prompting ReFix Overlay modal...", itemName);

            uint64_t orderId = GetTickCount64();
            ReFixOverlay::PromptPurchase(730, orderId, itemName, "CS:GO In-Game Store Purchase", true,
                [defIndex, count, this](bool approved) {
                    if (approved) {
                        CSGOInventoryManager::Get().GrantKeys(defIndex, count);
                        ReFixLog("[GC] Purchase approved by player! Granted %u of def %u", count, defIndex);
                        std::vector<uint8_t> resp;
                        PutVarintField(resp, 1, 1); // Success
                        Reply(kMsgGCStorePurchaseInitResponse, resp);
                        Reply(kMsgClientWelcome, ReFixGC::BuildClientWelcome(1, UnrealSteamEmu::GetLocalSteamID()));
                    } else {
                        ReFixLog("[GC] Purchase declined or cancelled in overlay.");
                        std::vector<uint8_t> resp;
                        PutVarintField(resp, 1, 2); // Cancelled
                        Reply(kMsgGCStorePurchaseInitResponse, resp);
                    }
                });
        }

        return k_EGCResultOK;
    }

    bool IsMessageAvailable(uint32* pcubMsgSize) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (pcubMsgSize) *pcubMsgSize = m_queue.empty() ? 0 : (uint32)m_queue.front().data.size();
        return !m_queue.empty();
    }

    EGCResults RetrieveMessage(uint32* punMsgType, void* pubDest, uint32 cubDest, uint32* pcubMsgSize) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            if (pcubMsgSize) *pcubMsgSize = 0;
            return k_EGCResultNoMessage;
        }
        const Message& front = m_queue.front();
        if (pcubMsgSize) *pcubMsgSize = (uint32)front.data.size();
        if (!pubDest || cubDest < front.data.size()) return k_EGCResultBufferTooSmall;
        memcpy(pubDest, front.data.data(), front.data.size());
        if (punMsgType) *punMsgType = front.type;
        m_queue.pop_front();
        return k_EGCResultOK;
    }

public:
    void Reply(uint32_t msg, const std::vector<uint8_t>& body) {
        Message out{ msg | kProtoFlag, {} };
        const uint32_t header[2] = { msg | kProtoFlag, 0 };
        out.data.resize(sizeof(header));
        memcpy(out.data.data(), header, sizeof(header));
        out.data.insert(out.data.end(), body.begin(), body.end());

        GCMessageAvailable_t available = {};
        available.m_nMessageSize = (uint32)out.data.size();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(std::move(out));
        }
        if (m_gameServer)
            UnrealSteamEmu::PostGameServerCallback(GCMessageAvailable_t::k_iCallback, &available, sizeof(available));
        else
            UnrealSteamEmu::PostCallback(GCMessageAvailable_t::k_iCallback, &available, sizeof(available));
        ReFixLog("[GC] %s <- %u (%u bytes)", Role(), msg, available.m_nMessageSize);
    }

    const bool m_gameServer;
    std::atomic<int> m_logged{ 0 };
    std::mutex m_mutex;
    std::deque<Message> m_queue;
};

} // namespace

namespace ReFixGC {

ISteamGameCoordinator* Get(bool gameServer) {
    static CGameCoordinatorLan client(false);
    static CGameCoordinatorLan server(true);
    return gameServer ? static_cast<ISteamGameCoordinator*>(&server) : &client;
}

// CMsgClientWelcome with fully populated CSOEconItem (type 1) and CSOEconCoupon (type 45)
std::vector<uint8_t> BuildClientWelcome(uint32_t version, uint64_t steamId) {
    std::vector<uint8_t> owner;
    PutVarintField(owner, 1, kSOTypeSteamId);
    PutVarintField(owner, 2, steamId);

    // Get profile-isolated inventory items and coupons
    auto items = CSGOInventoryManager::Get().GetItems();
    auto coupons = CSGOInventoryManager::Get().GetCoupons();
    uint32_t accountId = (uint32_t)(steamId & 0xFFFFFFFF);

    // SubscribedType for CSOEconItem (type_id = 1)
    std::vector<uint8_t> itemType;
    PutVarintField(itemType, 1, 1); // type_id = 1
    uint32_t pos = 1;
    for (const auto& it : items) {
        std::vector<uint8_t> itBytes = SerializeEconItem(it, accountId, pos++);
        PutBytesField(itemType, 2, itBytes);
    }

    // SubscribedType for CSOEconCoupon (type_id = 45)
    std::vector<uint8_t> couponType;
    PutVarintField(couponType, 1, 45); // type_id = 45
    for (const auto& cp : coupons) {
        std::vector<uint8_t> cpBytes = SerializeEconCoupon(cp);
        PutBytesField(couponType, 2, cpBytes);
    }

    // SubscribedType for CSOPersonaDataPublic (type_id = 2) - Prime status, level 40, commendations
    std::vector<uint8_t> personaData;
    PutVarintField(personaData, 1, 40); // player_level = 40
    std::vector<uint8_t> commends;
    PutVarintField(commends, 1, 100);
    PutVarintField(commends, 2, 100);
    PutVarintField(commends, 4, 100);
    PutBytesField(personaData, 2, commends);
    PutVarintField(personaData, 3, 1); // elevated_state = 1 (Prime Status!)

    std::vector<uint8_t> personaType;
    PutVarintField(personaType, 1, 2); // type_id = 2
    PutBytesField(personaType, 2, personaData);

    // CMsgSOCacheSubscribed
    std::vector<uint8_t> cache;
    PutBytesField(cache, 2, itemType);
    PutBytesField(cache, 2, personaType);
    if (!coupons.empty()) PutBytesField(cache, 2, couponType);
    PutFixed64Field(cache, 3, 1); // version
    PutBytesField(cache, 4, owner);

    // CMsgClientWelcome
    std::vector<uint8_t> welcome;
    if (version) PutVarintField(welcome, 1, version);
    PutBytesField(welcome, 3, cache);
    return welcome;
}

std::vector<uint8_t> BuildConnectionStatus() {
    std::vector<uint8_t> status;
    PutVarintField(status, 1, 0);
    return status;
}

uint32_t ParseHelloVersion(const uint8_t* body, size_t size) {
    const uint8_t* p = body;
    const uint8_t* end = body + size;
    while (p < end) {
        uint64_t key = 0, value = 0;
        if (!GetVarint(p, end, key)) return 0;
        const uint32_t field = (uint32_t)(key >> 3);
        switch (key & 7) {
        case 0:
            if (!GetVarint(p, end, value)) return 0;
            if (field == 1) return (uint32_t)value;
            break;
        case 1: if (end - p < 8) return 0; p += 8; break;
        case 5: if (end - p < 4) return 0; p += 4; break;
        case 2:
            if (!GetVarint(p, end, value) || (uint64_t)(end - p) < value) return 0;
            p += value;
            break;
        default: return 0;
        }
    }
    return 0;
}

} // namespace ReFixGC

extern "C" __declspec(dllexport) void CSGO_GrantItemToLocal(uint32_t defIndex, uint32_t count, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed) {
    uint64_t id = CSGOInventoryManager::Get().AddItemWithAttributes(defIndex, count, quality, rarity, paintKit, wear, seed);
    if (id != 0) {
        auto allItems = CSGOInventoryManager::Get().GetItems();
        auto itGranted = std::find_if(allItems.begin(), allItems.end(), [id](const EconItem& i) { return i.id == id; });
        if (itGranted != allItems.end()) {
            uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            std::vector<uint8_t> itBytes = SerializeEconItem(*itGranted, accountId, (uint32_t)allItems.size());

            std::vector<uint8_t> soid;
            PutVarintField(soid, 1, kSOTypeSteamId);
            PutVarintField(soid, 2, UnrealSteamEmu::GetLocalSteamID());

            static uint64_t s_soVersion = 200;
            std::vector<uint8_t> soSingle;
            PutVarintField(soSingle, 2, 1);
            PutBytesField(soSingle, 3, itBytes);
            PutFixed64Field(soSingle, 4, ++s_soVersion);
            PutBytesField(soSingle, 5, soid);

            CGameCoordinatorLan* gc = (CGameCoordinatorLan*)ReFixGC::Get(false);
            if (gc) gc->Reply(21 /* k_ESOMsg_Create */, soSingle);
        }
    }
    UnrealSteamEmu::PostCallback(4004 /* kMsgClientWelcome */, nullptr, 0);
}
