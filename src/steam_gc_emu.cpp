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
        }

        if (m_items.empty()) {
            // First time setup: Grant all cases & keys to test the inventory!
            uint64_t nextId = 1000000000ULL;
            // Grant starter skins and knives first
            for (size_t i = 0; i < ReFixGC::CSGO::kNumWeaponSkinDefs; ++i) {
                EconItem itm;
                itm.id = ++nextId;
                itm.defIndex = ReFixGC::CSGO::kWeaponSkinDefs[i];
                itm.quantity = 1;
                itm.quality = 4;
                itm.rarity = 6;
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

        static unsigned int s_seed = 12345;
        s_seed = s_seed * 1103515245 + 12347;
        uint32_t skinDef = ReFixGC::CSGO::kWeaponSkinDefs[s_seed % ReFixGC::CSGO::kNumWeaponSkinDefs];

        EconItem unboxed;
        unboxed.id = maxId + 1;
        unboxed.defIndex = skinDef;
        unboxed.quantity = 1;
        unboxed.quality = 4;
        unboxed.rarity = (s_seed % 5) + 3; // Classified / Covert
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
                << ", \"rarity\": " << it.rarity << " }" << (i + 1 < m_items.size() ? ",\n" : "\n");
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
    PutVarintField(out, 7, item.quality);
    PutVarintField(out, 8, 0); // flags
    PutVarintField(out, 9, 0); // origin
    if (!item.customName.empty()) {
        PutStringField(out, 10, item.customName);
    }
    if (item.rarity) PutVarintField(out, 18, item.rarity);
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
    explicit CGameCoordinatorLan(bool gameServer) : m_gameServer(gameServer) {}

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
            Reply(kMsgClientWelcome, ReFixGC::BuildClientWelcome(version, UnrealSteamEmu::GetLocalSteamID()));
            Reply(kMsgClientConnectionStatus, ReFixGC::BuildConnectionStatus());
        } else if (m_gameServer && msg == kMsgServerHello) {
            uint32_t version = ReFixGC::ParseHelloVersion(body, bodySize);
            if (version == 0) version = 13884;
            std::vector<uint8_t> welcome;
            PutVarintField(welcome, 1, version);
            PutVarintField(welcome, 2, version);
            Reply(kMsgServerWelcome, welcome);
            Reply(kMsgServerConnectionStatus, ReFixGC::BuildConnectionStatus());
        } else if (msg == kMsgGCCStrike15_v2_MatchmakingClient2GCHello) {
            // Reply with MatchmakingGC2ClientHello populated with Rank, Commendations, and VAC-clear status
            uint32_t accountId = (uint32_t)(UnrealSteamEmu::GetLocalSteamID() & 0xFFFFFFFF);
            std::vector<uint8_t> ranking;
            PutVarintField(ranking, 1, accountId);
            PutVarintField(ranking, 2, 18); // Global Elite
            PutVarintField(ranking, 3, 150); // 150 wins
            PutVarintField(ranking, 6, 6);   // 5v5 Competitive

            std::vector<uint8_t> commendation;
            PutVarintField(commendation, 1, 100);
            PutVarintField(commendation, 2, 100);
            PutVarintField(commendation, 4, 100);

            std::vector<uint8_t> medals;
            PutVarintField(medals, 1, 1024); // 5 Year Veteran Coin
            PutVarintField(medals, 2, 1024); // featured display item

            std::vector<uint8_t> resp;
            PutVarintField(resp, 1, accountId);
            PutVarintField(resp, 6, 0); // vac_banned = 0
            PutBytesField(resp, 7, ranking);
            PutBytesField(resp, 8, commendation);
            PutBytesField(resp, 9, medals);
            PutVarintField(resp, 17, 40);   // player_level = 40 (General de rango 40)
            PutVarintField(resp, 18, 3500); // player_cur_xp = 3500
            PutVarintField(resp, 19, 1);    // player_xp_bonus_flags = 1
            Reply(kMsgGCCStrike15_v2_MatchmakingGC2ClientHello, resp);
        } else if (msg == kMsgGCCStrike15_v2_MatchmakingStart) {
            ReFixLog("[GC] kMsgGCCStrike15_v2_MatchmakingStart: Stopping queue cleanly and opening ReFix Host Match Menu");
            // Reply with 9104 (MatchmakingGC2ClientUpdate): matchmaking = 0 (stops queue cleanly with no error modal)
            std::vector<uint8_t> updateResp;
            PutVarintField(updateResp, 1, 0); // matchmaking status = 0 (idle)
            Reply(9104, updateResp);

            // Open ReFix Host Match modal so the player can customize mode, map, bots, etc.
            ReFixOverlay::OpenMatchmakerModal();
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

            // 1. Resend full SOCache welcome FIRST so Panorama knows the item exists before notification
            Reply(kMsgClientWelcome, ReFixGC::BuildClientWelcome(1, UnrealSteamEmu::GetLocalSteamID()));

            // 2. Dispatch UnlockCrateResponse
            std::vector<uint8_t> unlockResp;
            PutVarintField(unlockResp, 1, awardedId != 0 ? 1 : 2); // 1 = k_EResultOK
            if (awardedId != 0) PutVarintField(unlockResp, 2, awardedId);
            Reply(kMsgGCUnlockCrateResponse, unlockResp);

            // 3. Dispatch CMsgGCItemCustomizationNotification (EMsg 1090):
            // field 1: item_id (awardedId), field 2: request (1007 = UnlockCrate)
            if (awardedId != 0) {
                std::vector<uint8_t> notif;
                PutVarintField(notif, 1, awardedId);
                PutVarintField(notif, 2, 1007);
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

private:
    struct Message {
        uint32_t type;
        std::vector<uint8_t> data;
    };

    const char* Role() const { return m_gameServer ? "server" : "client"; }

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

    // CMsgSOCacheSubscribed
    std::vector<uint8_t> cache;
    PutBytesField(cache, 2, itemType);
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

extern "C" __declspec(dllexport) void CSGO_GrantItemToLocal(uint32_t defIndex, uint32_t count, uint32_t quality, uint32_t rarity) {
    CSGOInventoryManager::Get().AddItem(defIndex, count, quality, rarity);
    UnrealSteamEmu::PostCallback(4004 /* kMsgClientWelcome */, nullptr, 0);
}
