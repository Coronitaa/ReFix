#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <fstream>
#include <nlohmann/json.hpp>

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

class TestInventoryManager {
public:
    uint32_t m_lastLowItemId = 0;
    uint32_t m_accountId = 0x69393642;
    std::vector<EconItem> m_items;
    std::vector<EconCoupon> m_coupons;

    uint64_t ComposeItemId(uint32_t accountId, uint32_t lowId) const {
        return ((uint64_t)accountId << 32) | lowId;
    }

    uint32_t GetLowItemId(uint32_t lowItemId = 0) {
        if (!lowItemId) {
            m_lastLowItemId++;
            lowItemId = m_lastLowItemId;
        }

        for (;; lowItemId++) {
            if (!lowItemId) {
                m_lastLowItemId = 1;
                lowItemId = m_lastLowItemId;
            }

            uint64_t candidateId = ComposeItemId(m_accountId, lowItemId);
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
        uint32_t low = GetLowItemId(0);
        return ComposeItemId(m_accountId, low);
    }

    std::string SaveToJson() {
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
        return j.dump(2);
    }

    void LoadFromJson(const std::string& str) {
        nlohmann::json j = nlohmann::json::parse(str);
        m_items.clear();
        m_coupons.clear();

        if (j.contains("last_low_item_id") && j["last_low_item_id"].is_number()) {
            m_lastLowItemId = j["last_low_item_id"].get<uint32_t>();
        } else {
            m_lastLowItemId = 0;
        }

        if (j.contains("items") && j["items"].is_array()) {
            for (const auto& itmJson : j["items"]) {
                EconItem itm;
                if (itmJson.contains("id")) itm.id = itmJson["id"].get<uint64_t>();
                if (itmJson.contains("defIndex")) itm.defIndex = itmJson["defIndex"].get<uint32_t>();
                if (itmJson.contains("quantity")) itm.quantity = itmJson["quantity"].get<uint32_t>();
                if (itmJson.contains("quality")) itm.quality = itmJson["quality"].get<uint32_t>();
                if (itmJson.contains("rarity")) itm.rarity = itmJson["rarity"].get<uint32_t>();
                if (itmJson.contains("paintKit")) itm.paintKit = itmJson["paintKit"].get<uint32_t>();
                if (itmJson.contains("wear")) itm.wear = itmJson["wear"].get<float>();
                if (itmJson.contains("seed")) itm.seed = itmJson["seed"].get<uint32_t>();
                if (itmJson.contains("customName")) itm.customName = itmJson["customName"].get<std::string>();

                if (itm.wear <= 0.0f) itm.wear = 0.001f;

                if (itmJson.contains("equipped") && itmJson["equipped"].is_array()) {
                    for (const auto& eqJson : itmJson["equipped"]) {
                        uint32_t cl = 0, sl = 0;
                        if (eqJson.contains("class")) cl = eqJson["class"].get<uint32_t>();
                        if (eqJson.contains("slot")) sl = eqJson["slot"].get<uint32_t>();
                        // Critical fix: class must be valid (>0), slot 0 (knife) is fully valid!
                        if (cl > 0) {
                            itm.equipped.push_back({ cl, sl });
                        }
                    }
                }
                m_items.push_back(itm);

                uint32_t lowId = (uint32_t)(itm.id & 0xFFFFFFFFULL);
                if (lowId > m_lastLowItemId) {
                    m_lastLowItemId = lowId;
                }
            }
        }

        if (j.contains("coupons") && j["coupons"].is_array()) {
            for (const auto& cpJson : j["coupons"]) {
                EconCoupon cp;
                if (cpJson.contains("entryId")) cp.entryId = cpJson["entryId"].get<uint32_t>();
                if (cpJson.contains("defidx")) cp.defidx = cpJson["defidx"].get<uint32_t>();
                if (cpJson.contains("expirationDate")) cp.expirationDate = cpJson["expirationDate"].get<uint32_t>();
                m_coupons.push_back(cp);
            }
        }
    }
};

int main() {
    std::cout << "====================================================\n";
    std::cout << "       CS:GO INVENTORY JSON & ID REGRESSION TEST\n";
    std::cout << "====================================================\n\n";

    TestInventoryManager inv1;

    // 1. Knife (Karambit defIndex 507) in slot 0 for T (class 2) and CT (class 3)
    EconItem knife;
    knife.id = inv1.GenerateNewItemId();
    knife.defIndex = 507;
    knife.paintKit = 417; // Doppler
    knife.wear = 0.015f;
    knife.seed = 1337;
    knife.equipped.push_back({ 2, 0 }); // T Knife (slot 0)
    knife.equipped.push_back({ 3, 0 }); // CT Knife (slot 0)
    inv1.m_items.push_back(knife);

    // 2. Gloves (Sport Gloves defIndex 5030) in slot 41 for T (2) and CT (3)
    EconItem gloves;
    gloves.id = inv1.GenerateNewItemId();
    gloves.defIndex = 5030;
    gloves.paintKit = 10037;
    gloves.wear = 0.12f;
    gloves.seed = 42;
    gloves.equipped.push_back({ 2, 41 }); // T Gloves (slot 41)
    gloves.equipped.push_back({ 3, 41 }); // CT Gloves (slot 41)
    inv1.m_items.push_back(gloves);

    // 3. Rifle AK-47 (defIndex 7) in slot 2 for T (class 2)
    EconItem ak;
    ak.id = inv1.GenerateNewItemId();
    ak.defIndex = 7;
    ak.paintKit = 180; // Fire Serpent
    ak.wear = 0.08f;
    ak.seed = 777;
    ak.equipped.push_back({ 2, 2 }); // T Rifle
    inv1.m_items.push_back(ak);

    // 4. Rifle M4A4 (defIndex 16) in slot 3 for CT (class 3)
    EconItem m4;
    m4.id = inv1.GenerateNewItemId();
    m4.defIndex = 16;
    m4.paintKit = 309; // Howl
    m4.wear = 0.03f;
    m4.seed = 888;
    m4.equipped.push_back({ 3, 3 }); // CT Rifle
    inv1.m_items.push_back(m4);

    std::cout << "[*] Saving inventory with Knife (slot 0), Gloves (slot 41), AK (class 2 slot 2), M4 (class 3 slot 3)...\n";
    std::string jsonStr = inv1.SaveToJson();
    std::cout << "[+] Serialized JSON length: " << jsonStr.size() << " bytes\n";

    // Reload into a fresh manager
    TestInventoryManager inv2;
    inv2.LoadFromJson(jsonStr);

    std::cout << "[*] Verifying reloaded inventory...\n";
    assert(inv2.m_items.size() == 4);
    assert(inv2.m_lastLowItemId == inv1.m_lastLowItemId);

    // Check Knife
    const auto& k2 = inv2.m_items[0];
    assert(k2.id == knife.id);
    assert(k2.defIndex == 507);
    assert(k2.paintKit == 417);
    assert(k2.equipped.size() == 2);
    assert(k2.equipped[0].new_class == 2 && k2.equipped[0].new_slot == 0);
    assert(k2.equipped[1].new_class == 3 && k2.equipped[1].new_slot == 0);
    std::cout << "  [+] Knife slot 0 for T (2) and CT (3) preserved perfectly!\n";

    // Check Gloves
    const auto& g2 = inv2.m_items[1];
    assert(g2.id == gloves.id);
    assert(g2.defIndex == 5030);
    assert(g2.equipped.size() == 2);
    assert(g2.equipped[0].new_class == 2 && g2.equipped[0].new_slot == 41);
    assert(g2.equipped[1].new_class == 3 && g2.equipped[1].new_slot == 41);
    std::cout << "  [+] Gloves slot 41 for T (2) and CT (3) preserved perfectly!\n";

    // Check AK
    const auto& ak2 = inv2.m_items[2];
    assert(ak2.id == ak.id);
    assert(ak2.defIndex == 7);
    assert(ak2.equipped.size() == 1);
    assert(ak2.equipped[0].new_class == 2 && ak2.equipped[0].new_slot == 2);
    std::cout << "  [+] AK-47 slot 2 for T (2) preserved perfectly!\n";

    // Check M4
    const auto& m42 = inv2.m_items[3];
    assert(m42.id == m4.id);
    assert(m42.defIndex == 16);
    assert(m42.equipped.size() == 1);
    assert(m42.equipped[0].new_class == 3 && m42.equipped[0].new_slot == 3);
    std::cout << "  [+] M4A4 slot 3 for CT (3) preserved perfectly!\n";

    // Test Item ID Generator monotonicity and collision resistance
    std::cout << "[*] Stress testing ID generator (1000 consecutive items)...\n";
    uint32_t prevLow = inv2.m_lastLowItemId;
    for (int i = 0; i < 1000; ++i) {
        uint64_t newId = inv2.GenerateNewItemId();
        uint32_t hi = (uint32_t)(newId >> 32);
        uint32_t lo = (uint32_t)(newId & 0xFFFFFFFF);
        assert(hi == inv2.m_accountId);
        assert(lo > prevLow);
        prevLow = lo;

        EconItem dummy;
        dummy.id = newId;
        dummy.defIndex = 1;
        inv2.m_items.push_back(dummy);
    }
    assert(inv2.m_items.size() == 1004);
    std::cout << "  [+] 1000 IDs generated with zero collisions and strict monotonicity!\n";

    std::cout << "\n====================================================\n";
    std::cout << "       ALL INVENTORY & ID TESTS PASSED (100% OK)\n";
    std::cout << "====================================================\n";
    return 0;
}
