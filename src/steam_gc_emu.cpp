#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "include/steam/steam_api.h"
#include "include/steam/isteamgamecoordinator.h"
#include "steam_gc_emu.h"
#include "unreal_steam_emu.h"
#include "overlay/refix_overlay.h"
#include "csgo_item_defs.h"
#include "csgo_schema.h"

namespace refix::overlay {
    void ExecuteEngineClientCommand(const char* cmd);
    void OpenMatchmakingToast();
}

extern void ReFixLog(const char* fmt, ...);

namespace {

static bool IsGCTraceEnabled() {
    static int s_gcTrace = -1;
    if (s_gcTrace == -1) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string dir(exePath);
        size_t p = dir.find_last_of("\\/");
        std::string ini = (p != std::string::npos ? dir.substr(0, p + 1) : "") + "ReFix.ini";
        if (GetFileAttributesA(ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
            std::string cand = (p != std::string::npos ? dir.substr(0, p + 1) : "") + "..\\ReFix.ini";
            if (GetFileAttributesA(cand.c_str()) != INVALID_FILE_ATTRIBUTES) ini = cand;
        }
        s_gcTrace = GetPrivateProfileIntA("Debug", "GCTrace", 0, ini.c_str()) ? 1 : 0;
    }
    return s_gcTrace == 1;
}

static void LogGCTrace(const char* role, const char* dir, uint32_t msg, bool proto, const void* data, size_t size) {
    if (!IsGCTraceEnabled()) return;

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string logPath(exePath);
    size_t pos = logPath.find_last_of("\\/");
    char filename[64];
    sprintf_s(filename, "ReFix_gc_%lu.log", GetCurrentProcessId());
    if (pos != std::string::npos) logPath = logPath.substr(0, pos + 1) + filename;
    else logPath = filename;

    SYSTEMTIME st;
    GetLocalTime(&st);

    std::string hex;
    hex.reserve(size * 2 + 1);
    static const char hexChars[] = "0123456789abcdef";
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < size; ++i) {
        hex.push_back(hexChars[(p[i] >> 4) & 0x0F]);
        hex.push_back(hexChars[p[i] & 0x0F]);
    }
    if (hex.empty()) hex = "-";

    char line[256];
    int n = sprintf_s(line, "%04d-%02d-%02d %02d:%02d:%02d.%03d %lu %s %s %u %d ",
                      st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                      GetCurrentProcessId(), role, dir, msg, proto ? 1 : 0);
    std::string fullLine;
    if (n > 0) fullLine.append(line, n);
    fullLine.append(hex);
    fullLine.push_back('\n');

    HANDLE hf = CreateFileA(logPath.c_str(), FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hf, fullLine.data(), (DWORD)fullLine.size(), &written, nullptr);
        CloseHandle(hf);
    }
}

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
    struct EquippedSlot {
        uint32_t new_class = 0;
        uint32_t new_slot = 0;
    };

    uint64_t id = 0;
    uint32_t defIndex = 0;
    uint32_t quantity = 1;
    uint32_t quality = 4;
    uint32_t rarity = 0;
    uint32_t paintKit = 0;
    float wear = 0.001f;
    uint32_t seed = 0;
    std::string customName;
    std::vector<EquippedSlot> equipped;
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

    uint64_t ComposeItemId(uint32_t accountId, uint32_t lowId) const {
        return ((uint64_t)accountId << 32) | lowId;
    }

    uint32_t GetLowItemId(uint32_t lowItemId = 0) {
        if (!lowItemId) {
            m_lastLowItemId++;
            lowItemId = m_lastLowItemId;
        }

        uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);

        for (;; lowItemId++) {
            if (!lowItemId) {
                m_lastLowItemId = 1;
                lowItemId = m_lastLowItemId;
            }

            uint64_t candidateId = ComposeItemId(accountId, lowItemId);
            bool collision = false;
            for (const auto& itm : m_items) {
                if (itm.id == candidateId) {
                    collision = true;
                    break;
                }
            }
            if (collision) continue;

            if (lowItemId > m_lastLowItemId) {
                m_lastLowItemId = lowItemId;
            }
            return lowItemId;
        }
    }

    uint64_t GenerateNewItemId() {
        uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
        uint32_t low = GetLowItemId(0);
        return ComposeItemId(accountId, low);
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
        refix::csgo::SchemaManager::Get().Load();

        std::wstring path = GetInventoryPath();
        std::ifstream f(path, std::ios::binary);
        if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            f.close();
            ParseJson(content);

            uint64_t accountId = (UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            for (auto& itm : m_items) {
                if (itm.id < 0x100000000ULL && accountId != 0) {
                    itm.id = (accountId << 32) | (itm.id & 0xFFFFFFFFULL);
                }
            }

            // Sanitize equipped loadout: guarantee strictly ONE item per (class, slot)
            std::set<std::pair<uint32_t, uint32_t>> seenEquipped;
            bool changedEquipped = false;
            for (auto& itm : m_items) {
                auto it = itm.equipped.begin();
                while (it != itm.equipped.end()) {
                    auto key = std::make_pair(it->new_class, it->new_slot);
                    if (seenEquipped.count(key)) {
                        it = itm.equipped.erase(it);
                        changedEquipped = true;
                    } else {
                        seenEquipped.insert(key);
                        ++it;
                    }
                }
            }
            if (changedEquipped) SaveLocked();

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
            uint64_t accountId = (UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            uint32_t nextId = 1;
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
                itm.id = GenerateNewItemId();
                itm.defIndex = s.defIndex;
                itm.quantity = 1;
                itm.quality = 4;
                itm.rarity = s.rarity;
                itm.paintKit = s.paintKit;
                itm.wear = 0.001f;
                itm.seed = 420;
                if (itm.defIndex == 9) { itm.equipped.push_back({2, 18}); itm.equipped.push_back({3, 18}); }
                else if (itm.defIndex == 7) { itm.equipped.push_back({2, 15}); }
                else if (itm.defIndex == 16) { itm.equipped.push_back({3, 15}); }
                else if (itm.defIndex == 61) { itm.equipped.push_back({3, 2}); }
                else if (itm.defIndex == 4) { itm.equipped.push_back({2, 2}); }
                else if (itm.defIndex == 1) { itm.equipped.push_back({2, 6}); itm.equipped.push_back({3, 6}); }
                else if (itm.defIndex == 507) { itm.equipped.push_back({2, 0}); itm.equipped.push_back({3, 0}); }
                else if (itm.defIndex == 5030) { itm.equipped.push_back({2, 41}); itm.equipped.push_back({3, 41}); }
                m_items.push_back(itm);
            }
            // Then keys
            for (size_t i = 0; i < ReFixGC::CSGO::kNumKeys; ++i) {
                EconItem itm;
                itm.id = GenerateNewItemId();
                itm.defIndex = ReFixGC::CSGO::kKeys[i];
                itm.quantity = 20; // 20 keys each for easy testing
                itm.quality = 4;
                m_items.push_back(itm);
            }
            // Then 36 cases
            for (size_t i = 0; i < ReFixGC::CSGO::kNumCases; ++i) {
                EconItem itm;
                itm.id = GenerateNewItemId();
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
        auto itKey = std::find_if(m_items.begin(), m_items.end(), [keyId](const EconItem& it) {
            return it.id == keyId || (keyId != 0 && it.defIndex == (uint32_t)keyId);
        });

        // Bidirectional case vs key sanity check: if parameters were reversed in packet
        if (itCrate != m_items.end() && itKey != m_items.end()) {
            if (itCrate->defIndex < 4000 && itKey->defIndex >= 4000) {
                std::swap(itCrate, itKey);
                std::swap(crateId, keyId);
            }
        }
        if (itCrate == m_items.end()) {
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

        uint32_t crateDef = itCrate->defIndex;

        // If key was not found by exact ID, fallback search
        if (itKey == m_items.end()) {
            itKey = std::find_if(m_items.begin(), m_items.end(), [keyId](const EconItem& it) {
                return it.id == keyId || (keyId != 0 && it.defIndex == (uint32_t)keyId);
            });
        }
        if (itKey == m_items.end()) {
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

        // Roll authentic drop from collection
        uint32_t outDef = 0, outPaintKit = 0, outRarity = 0, outQuality = 4;
        float outWear = 0.001f;
        uint32_t outSeed = 420;
        refix::csgo::SchemaManager::Get().RollCaseDrop(crateDef, outDef, outPaintKit, outRarity, outQuality, outWear, outSeed);

        EconItem unboxed;
        unboxed.id = GenerateNewItemId();
        unboxed.defIndex = outDef;
        unboxed.quantity = 1;
        unboxed.quality = outQuality;
        unboxed.rarity = outRarity;
        unboxed.paintKit = outPaintKit;
        unboxed.wear = outWear;
        unboxed.seed = outSeed;
        m_items.push_back(unboxed);

        SaveLocked();
        ReFixLog("[GC] CSGOInventory: Unlocked crate %u! Awarded item %llu (def %u, pk %u, rarity %u, quality %u)",
                 crateDef, (unsigned long long)unboxed.id, unboxed.defIndex, unboxed.paintKit, unboxed.rarity, unboxed.quality);
        return unboxed.id;
    }

    uint64_t AddItem(uint32_t defIndex, uint32_t count = 1, uint32_t quality = 4, uint32_t rarity = 0) {
        EnsureLoaded();
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t accountId = (UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);

        auto it = std::find_if(m_items.begin(), m_items.end(), [defIndex](const EconItem& it) {
            return it.defIndex == defIndex && it.paintKit == 0;
        });
        if (it != m_items.end()) {
            it->quantity += count;
            SaveLocked();
            return it->id;
        }

        EconItem itm;
        itm.id = GenerateNewItemId();
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
        uint64_t accountId = (UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);

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
        itm.id = GenerateNewItemId();
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

    void AdjustEquippedState(uint64_t itemId, uint32_t newClass, uint32_t newSlot) {
        EnsureLoaded();
        std::lock_guard<std::mutex> lock(m_mutex);

        bool isEquipping = (newSlot < 65535);

        if (isEquipping) {
            for (auto& itm : m_items) {
                if (itm.id != itemId) {
                    itm.equipped.erase(std::remove_if(itm.equipped.begin(), itm.equipped.end(),
                        [newClass, newSlot](const EconItem::EquippedSlot& es) {
                            return es.new_class == newClass && es.new_slot == newSlot;
                        }), itm.equipped.end());
                }
            }
        }

        for (auto& itm : m_items) {
            if (itm.id == itemId) {
                itm.equipped.erase(std::remove_if(itm.equipped.begin(), itm.equipped.end(),
                    [newClass, newSlot, isEquipping](const EconItem::EquippedSlot& es) {
                        if (!isEquipping) {
                            return es.new_class == newClass;
                        }
                        return es.new_class == newClass && es.new_slot == newSlot;
                    }), itm.equipped.end());

                if (isEquipping) {
                    EconItem::EquippedSlot es;
                    es.new_class = newClass;
                    es.new_slot = newSlot;
                    itm.equipped.push_back(es);
                }
                break;
            }
        }
        SaveLocked();
        ReFixLog("[GC] CSGOInventory: Adjusted equipped state for item %llu (class %u, slot %u, equipping=%d)",
                 (unsigned long long)itemId, newClass, newSlot, isEquipping ? 1 : 0);
    }

    void GrantKeys(uint32_t defIndex, uint32_t count) {
        AddItem(defIndex, count, 4, 0);
    }

private:
    std::mutex m_mutex;
    bool m_loaded = false;
    uint32_t m_lastLowItemId = 0;
    std::vector<EconItem> m_items;
    std::vector<EconCoupon> m_coupons;

    void SaveLocked() {
        std::wstring path = GetInventoryPath();
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) return;

        nlohmann::json j;
        j["last_low_item_id"] = m_lastLowItemId;
        j["items"] = nlohmann::json::array();
        for (const auto& it : m_items) {
            nlohmann::json itm;
            itm["id"] = it.id;
            itm["defIndex"] = it.defIndex;
            itm["quantity"] = it.quantity;
            itm["quality"] = it.quality;
            itm["rarity"] = it.rarity;
            itm["paintKit"] = it.paintKit;
            itm["wear"] = it.wear;
            itm["seed"] = it.seed;
            if (!it.customName.empty()) {
                itm["customName"] = it.customName;
            }
            if (!it.equipped.empty()) {
                nlohmann::json eqArr = nlohmann::json::array();
                for (const auto& es : it.equipped) {
                    nlohmann::json eqObj;
                    eqObj["class"] = es.new_class;
                    eqObj["slot"] = es.new_slot;
                    eqArr.push_back(eqObj);
                }
                itm["equipped"] = eqArr;
            }
            j["items"].push_back(itm);
        }
        j["coupons"] = nlohmann::json::array();
        for (const auto& cp : m_coupons) {
            nlohmann::json c;
            c["entryId"] = cp.entryId;
            c["defidx"] = cp.defidx;
            c["expirationDate"] = cp.expirationDate;
            j["coupons"].push_back(c);
        }

        std::string dumped = j.dump(2);
        ofs.write(dumped.data(), dumped.size());
    }

    void ParseJson(const std::string& str) {
        m_items.clear();
        m_coupons.clear();
        if (str.empty()) return;

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(str);
        } catch (const std::exception& e) {
            ReFixLog("[GC] CSGOInventory: Failed to parse inventory JSON: %s", e.what());
            return;
        }

        if (j.contains("last_low_item_id") && j["last_low_item_id"].is_number()) {
            m_lastLowItemId = j["last_low_item_id"].get<uint32_t>();
        } else {
            m_lastLowItemId = 0;
        }

        if (j.contains("items") && j["items"].is_array()) {
            for (const auto& itmJson : j["items"]) {
                if (!itmJson.is_object()) continue;
                EconItem itm;
                if (itmJson.contains("id") && itmJson["id"].is_number()) itm.id = itmJson["id"].get<uint64_t>();
                if (itmJson.contains("defIndex") && itmJson["defIndex"].is_number()) itm.defIndex = itmJson["defIndex"].get<uint32_t>();
                if (itmJson.contains("quantity") && itmJson["quantity"].is_number()) itm.quantity = itmJson["quantity"].get<uint32_t>();
                if (itmJson.contains("quality") && itmJson["quality"].is_number()) itm.quality = itmJson["quality"].get<uint32_t>();
                if (itmJson.contains("rarity") && itmJson["rarity"].is_number()) itm.rarity = itmJson["rarity"].get<uint32_t>();
                if (itmJson.contains("paintKit") && itmJson["paintKit"].is_number()) itm.paintKit = itmJson["paintKit"].get<uint32_t>();
                if (itmJson.contains("wear") && itmJson["wear"].is_number()) itm.wear = itmJson["wear"].get<float>();
                if (itmJson.contains("seed") && itmJson["seed"].is_number()) itm.seed = itmJson["seed"].get<uint32_t>();
                if (itmJson.contains("customName") && itmJson["customName"].is_string()) itm.customName = itmJson["customName"].get<std::string>();

                if (itm.wear <= 0.0f) itm.wear = 0.001f;

                if (itmJson.contains("equipped") && itmJson["equipped"].is_array()) {
                    for (const auto& eqJson : itmJson["equipped"]) {
                        if (!eqJson.is_object()) continue;
                        uint32_t cl = 0, sl = 0;
                        if (eqJson.contains("class") && eqJson["class"].is_number()) cl = eqJson["class"].get<uint32_t>();
                        if (eqJson.contains("slot") && eqJson["slot"].is_number()) sl = eqJson["slot"].get<uint32_t>();
                        // Support all valid equip slots, including slot 0 (knife)
                        if (cl > 0) {
                            itm.equipped.push_back({ cl, sl });
                        }
                    }
                }
                if (itm.id != 0 && itm.defIndex != 0) {
                    m_items.push_back(itm);
                }

                uint32_t lowId = (uint32_t)(itm.id & 0xFFFFFFFFULL);
                if (lowId > m_lastLowItemId) {
                    m_lastLowItemId = lowId;
                }
            }
        }

        if (j.contains("coupons") && j["coupons"].is_array()) {
            for (const auto& cpJson : j["coupons"]) {
                if (!cpJson.is_object()) continue;
                EconCoupon cp;
                if (cpJson.contains("entryId") && cpJson["entryId"].is_number()) cp.entryId = cpJson["entryId"].get<uint32_t>();
                if (cpJson.contains("defidx") && cpJson["defidx"].is_number()) cp.defidx = cpJson["defidx"].get<uint32_t>();
                if (cpJson.contains("expirationDate") && cpJson["expirationDate"].is_number()) cp.expirationDate = cpJson["expirationDate"].get<uint32_t>();
                if (cp.defidx != 0) {
                    m_coupons.push_back(cp);
                }
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
        // Attribute 6: paint kit (set item texture prefab) - provide BOTH value bits and value_bytes
        float fpk = (float)item.paintKit;
        uint32_t ufpk = *(uint32_t*)&fpk;
        std::vector<uint8_t> attr6;
        PutVarintField(attr6, 1, 6);
        PutVarintField(attr6, 2, ufpk);
        PutBytesField(attr6, 3, std::vector<uint8_t>((uint8_t*)&fpk, (uint8_t*)&fpk + 4));
        PutBytesField(out, 12, attr6);

        // Attribute 8: wear (set item texture wear) - provide BOTH value bits and value_bytes
        float fwear = item.wear > 0.0f ? item.wear : 0.001f;
        uint32_t ufwear = *(uint32_t*)&fwear;
        std::vector<uint8_t> attr8;
        PutVarintField(attr8, 1, 8);
        PutVarintField(attr8, 2, ufwear);
        PutBytesField(attr8, 3, std::vector<uint8_t>((uint8_t*)&fwear, (uint8_t*)&fwear + 4));
        PutBytesField(out, 12, attr8);

        // Attribute 7: seed (set item texture seed) - provide BOTH value bits and value_bytes
        uint32_t seedVal = item.seed > 0 ? item.seed : 420;
        float fseed = (float)seedVal;
        uint32_t ufseed = *(uint32_t*)&fseed;
        std::vector<uint8_t> attr7;
        PutVarintField(attr7, 1, 7);
        PutVarintField(attr7, 2, ufseed);
        PutBytesField(attr7, 3, std::vector<uint8_t>((uint8_t*)&fseed, (uint8_t*)&fseed + 4));
        PutBytesField(out, 12, attr7);

        // StatTrak: Attribute 80 (kill eater) & 81 (kill eater score type)
        // Stored as integer = 1, so MUST be varint field 2 (value), NO value_bytes!
        if (quality == 9) {
            std::vector<uint8_t> attr80;
            PutVarintField(attr80, 1, 80);
            PutVarintField(attr80, 2, 0); // 0 kills
            PutBytesField(out, 12, attr80);

            std::vector<uint8_t> attr81;
            PutVarintField(attr81, 1, 81);
            PutVarintField(attr81, 2, 0); // score type 0 = kills
            PutBytesField(out, 12, attr81);
        }
    }

    // Contains equipped state - ALWAYS emit field 17 so Panorama properly updates equipped vs unequipped states
    PutVarintField(out, 17, 1); // contains_equipped_state_v1 = true
    for (const auto& eq : item.equipped) {
        std::vector<uint8_t> eqBytes;
        PutVarintField(eqBytes, 1, eq.new_class);
        PutVarintField(eqBytes, 2, eq.new_slot);
        PutBytesField(out, 18, eqBytes); // repeated CSOEconItemEquipped equipped_state = 18;
    }

    // CSOEconItem field 19: optional uint32 rarity = 19;
    uint32_t finalRarity = item.rarity;
    if (finalRarity == 0) {
        if (item.defIndex >= 500 && item.defIndex <= 530) finalRarity = 7;
        else if (item.defIndex >= 5030 && item.defIndex <= 5035) finalRarity = 7;
        else if (item.defIndex >= 5038 && item.defIndex <= 5601) finalRarity = (item.defIndex >= 5300 ? 5 : 3);
        else if (item.paintKit > 0) finalRarity = 6;
    }
    if (finalRarity > 0) {
        PutVarintField(out, 19, finalRarity);
    }

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
        } else if (!proto && cubData >= 18) {
            body = bytes + 18;
            bodySize = cubData - 18;
        } else {
            body = bytes;
            bodySize = cubData;
        }

        LogGCTrace(Role(), "in", msg, proto, body, bodySize);

        if (!m_gameServer && msg == kMsgClientHello) {
            uint32_t version = ReFixGC::ParseHelloVersion(body, bodySize);
            uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            Reply(kMsgClientWelcome, ReFixGC::BuildClientWelcome(version, UnrealSteamEmu::GetLocalSteamID()));
            Reply(kMsgClientConnectionStatus, ReFixGC::BuildConnectionStatus());
            Reply(kMsgGCCStrike15_v2_MatchmakingGC2ClientHello, BuildMatchmakingClientHello(accountId));
        } else if (m_gameServer && msg == kMsgServerHello) {
            uint32_t version = ReFixGC::ParseHelloVersion(body, bodySize);
            if (version == 0) version = 13884;
            ReFixLog("[GC] Handling ServerHello v%u: Sending ServerWelcome with active SOCache", version);
            Reply(kMsgServerWelcome, ReFixGC::BuildServerWelcome(version, UnrealSteamEmu::GetLocalSteamID()));
            Reply(kMsgServerConnectionStatus, ReFixGC::BuildConnectionStatus());
            Reply(24 /* k_ESOMsg_CacheSubscribed */, ReFixGC::BuildCacheSubscribed(UnrealSteamEmu::GetLocalSteamID()));
        } else if (m_gameServer && (msg == 27 || msg == 28)) { // k_ESOMsg_CacheSubscriptionCheck / Refresh
            Reply(24 /* k_ESOMsg_CacheSubscribed */, ReFixGC::BuildCacheSubscribed(UnrealSteamEmu::GetLocalSteamID()));
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
            // Do NOT reply with 9198 (CMsgGCCStrike15_v2_ClientPartyWarning) to avoid rejecting lobby joins
            std::vector<uint8_t> relayResp;
            PutVarintField(relayResp, 1, 0);
            // Non-error response
        } else if (msg == 1059) { // k_EMsgGCAdjustItemEquippedState
            uint64_t itemId = 0;
            uint32_t newClass = 0;
            uint32_t newSlot = 0;
            if (body && bodySize > 0) {
                const uint8_t* p = body;
                const uint8_t* end = body + bodySize;
                while (p < end) {
                    uint64_t tag = 0, val = 0;
                    if (!GetVarint(p, end, tag)) break;
                    uint32_t f = (uint32_t)(tag >> 3);
                    uint32_t w = tag & 7;
                    if (w == 0) {
                        if (!GetVarint(p, end, val)) break;
                        if (f == 1) itemId = val;
                        else if (f == 2) newClass = (uint32_t)val;
                        else if (f == 3) newSlot = (uint32_t)val;
                    } else if (w == 1) { if (end - p < 8) break; p += 8; }
                    else if (w == 5) { if (end - p < 4) break; p += 4; }
                    else if (w == 2) {
                        if (!GetVarint(p, end, val) || (size_t)(end - p) < val) break;
                        p += val;
                    }
                }
            }
            ReFixLog("[GC] AdjustItemEquippedState: itemId=%llu class=%u slot=%u",
                     (unsigned long long)itemId, newClass, newSlot);

            // Collect all items whose equipped slot changes so Panorama updates cleanly
            std::vector<uint64_t> affectedItemIds;
            affectedItemIds.push_back(itemId);
            {
                auto currentItems = CSGOInventoryManager::Get().GetItems();
                if (newSlot < 65535) {
                    for (const auto& itm : currentItems) {
                        if (itm.id != itemId) {
                            for (const auto& es : itm.equipped) {
                                if (es.new_class == newClass && es.new_slot == newSlot) {
                                    affectedItemIds.push_back(itm.id);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            CSGOInventoryManager::Get().AdjustEquippedState(itemId, newClass, newSlot);

            // Send k_ESOMsg_Update (22) for EACH affected item to update Panorama loadout without destroying SOCache
            auto allItems = CSGOInventoryManager::Get().GetItems();
            uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            std::vector<uint8_t> soid;
            PutVarintField(soid, 1, kSOTypeSteamId);
            PutVarintField(soid, 2, UnrealSteamEmu::GetLocalSteamID());

            static uint64_t s_soUpdateVer = 1000;
            std::vector<uint8_t> soMult;
            for (uint64_t affId : affectedItemIds) {
                auto itm = std::find_if(allItems.begin(), allItems.end(), [affId](const EconItem& i) { return i.id == affId; });
                if (itm != allItems.end()) {
                    uint32_t itemPos = (uint32_t)(std::distance(allItems.begin(), itm) + 1);
                    std::vector<uint8_t> itBytes = SerializeEconItem(*itm, accountId, itemPos);
                    std::vector<uint8_t> singleObj;
                    PutVarintField(singleObj, 1, 1); // type_id = 1 (CSOEconItem)
                    PutBytesField(singleObj, 2, itBytes); // object_data = 2
                    PutBytesField(soMult, 2, singleObj); // repeated SingleObject objects_modified = 2
                }
            }
            PutFixed64Field(soMult, 3, ++s_soUpdateVer); // version = 3
            PutBytesField(soMult, 6, soid);              // owner_soid = 6
            Reply(26 /* k_ESOMsg_UpdateMultiple */, soMult);
            // CRITICAL: DO NOT send kMsgClientWelcome here!
            // Sending kMsgClientWelcome destroys all CEconItem pointers in Panorama JS heap,
            // causing immediate 0xC0000005 access violation crash!
        } else if (msg == 9156) { // Client2GCEconPreviewDataBlockRequest (Inspect item)
            uint64_t param_s = 0, param_a = 0;
            if (body && bodySize > 0) {
                const uint8_t* p = body;
                const uint8_t* end = body + bodySize;
                while (p < end) {
                    uint64_t tag = 0, val = 0;
                    if (!GetVarint(p, end, tag)) break;
                    uint32_t f = (uint32_t)(tag >> 3);
                    uint32_t w = tag & 7;
                    if (w == 0) {
                        if (!GetVarint(p, end, val)) break;
                        if (f == 1) param_s = val;
                        else if (f == 2) param_a = val;
                    } else if (w == 1) { if (end - p < 8) break; p += 8; }
                    else if (w == 5) { if (end - p < 4) break; p += 4; }
                    else if (w == 2) {
                        if (!GetVarint(p, end, val) || (size_t)(end - p) < val) break;
                        p += val;
                    }
                }
            }
            auto allItems = CSGOInventoryManager::Get().GetItems();
            auto itm = std::find_if(allItems.begin(), allItems.end(), [param_a](const EconItem& i) { return i.id == param_a; });
            uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);

            std::vector<uint8_t> previewBlock;
            PutVarintField(previewBlock, 1, accountId);
            PutVarintField(previewBlock, 2, param_a);
            if (itm != allItems.end()) {
                PutVarintField(previewBlock, 3, itm->defIndex);
                PutVarintField(previewBlock, 4, itm->paintKit);
                PutVarintField(previewBlock, 5, itm->rarity);
                PutVarintField(previewBlock, 6, itm->quality);
                PutVarintField(previewBlock, 7, itm->seed);
                float fwear = itm->wear;
                PutFixed32Field(previewBlock, 8, *(uint32_t*)&fwear);
                if (itm->quality == 9) {
                    PutVarintField(previewBlock, 9, 0);
                    PutVarintField(previewBlock, 10, 0);
                }
            }
            std::vector<uint8_t> resp;
            PutBytesField(resp, 1, previewBlock);
            Reply(9157, resp); // Client2GCEconPreviewDataBlockResponse
        } else if (msg == 9201) { // GetEventFavorites_Request
            std::vector<uint8_t> favResp;
            Reply(9203, favResp);
        } else if (msg == kMsgGCUnlockCrate) {
            // Unlock container (CS:GO sends non-proto 34-byte struct or proto)
            uint64_t crateId = 0, keyId = 0;
            if (!proto && cubData >= 34) {
                // Valve MsgGCUnlockCrate_t: 18-byte GC header + 8 bytes m_ulKeyID + 8 bytes m_ulCrateID
                memcpy(&keyId, bytes + 18, sizeof(keyId));
                memcpy(&crateId, bytes + 26, sizeof(crateId));
                ReFixLog("[GC] UnlockCrate (non-proto): keyId=%llu crateId=%llu",
                         (unsigned long long)keyId, (unsigned long long)crateId);
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
                // 1. Dispatch k_ESOMsg_Create (21) CMsgSOSingleObject directly to active SOCache
                auto allItems = CSGOInventoryManager::Get().GetItems();
                auto itAwarded = std::find_if(allItems.begin(), allItems.end(), [awardedId](const EconItem& i) {
                    return i.id == awardedId;
                });
                if (itAwarded != allItems.end()) {
                    uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
                    uint32_t itemPos = (uint32_t)(std::distance(allItems.begin(), itAwarded) + 1);
                    std::vector<uint8_t> itBytes = SerializeEconItem(*itAwarded, accountId, itemPos);

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
                refix::overlay::ExecuteEngineClientCommand("econ_clear_inventory_images");
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
        LogGCTrace(Role(), "out", msg, true, body.data(), body.size());
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

std::vector<uint8_t> BuildServerWelcome(uint32_t version, uint64_t steamId) {
    // k_EMsgGCServerWelcome (4005) is a CMsgClientWelcome protobuf:
    // field 1: optional uint32 version = 1 (0)
    // field 2: optional bytes game_data = 2 (CMsgCStrike15Welcome { gscookieid = 0x293A206F6C6C6548 })
    // field 7: optional uint32 rtime32_gc_welcome_timestamp = 7 (current time)
    std::vector<uint8_t> gameData;
    // CMsgCStrike15Welcome: field 18 = gscookieid (uint64)
    PutVarintField(gameData, 18, 0x293A206F6C6C6548ULL);

    std::vector<uint8_t> welcome;
    PutVarintField(welcome, 1, 0); // version = 0
    PutBytesField(welcome, 2, gameData); // game_data = serialized CMsgCStrike15Welcome
    PutVarintField(welcome, 7, (uint32_t)::time(nullptr)); // rtime32_gc_welcome_timestamp
    return welcome;
}

std::vector<uint8_t> BuildCacheSubscribed(uint64_t steamId) {
    std::vector<uint8_t> owner;
    PutVarintField(owner, 1, kSOTypeSteamId);
    PutVarintField(owner, 2, steamId);

    auto items = CSGOInventoryManager::Get().GetItems();
    uint32_t accountId = (uint32_t)(steamId & 0xFFFFFFFF);

    std::vector<uint8_t> itemType;
    PutVarintField(itemType, 1, 1); // type_id = 1 (CSOEconItem)
    uint32_t pos = 1;
    for (const auto& it : items) {
        std::vector<uint8_t> itBytes = SerializeEconItem(it, accountId, pos++);
        PutBytesField(itemType, 2, itBytes);
    }

    std::vector<uint8_t> cache;
    PutBytesField(cache, 2, itemType);
    PutFixed64Field(cache, 3, 1); // version
    PutBytesField(cache, 4, owner);
    return cache;
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
            if (gc) {
                gc->Reply(21 /* k_ESOMsg_Create */, soSingle);
            }
            refix::overlay::ExecuteEngineClientCommand("econ_clear_inventory_images");
        }
    }
}
