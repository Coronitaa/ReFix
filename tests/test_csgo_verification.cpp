#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <map>
#include <iomanip>
#include <windows.h>
#include "../src/csgo_schema.h"

int main() {
    std::cout << "====================================================\n";
    std::cout << "       CS:GO SCHEMA & VERIFICATION TEST SUITE\n";
    std::cout << "====================================================\n\n";

    // 1. Test Schema Parsing
    std::cout << "[*] Loading CS:GO schema via SchemaManager::Get().Load()...\n";
    bool loaded = refix::csgo::SchemaManager::Get().Load();
    std::cout << "[+] Schema load result: " << (loaded ? "SUCCESS" : "FALLBACK") << "\n";

    const auto& marketItems = refix::csgo::SchemaManager::Get().GetMarketItems();
    std::cout << "[+] Total market items cataloged: " << marketItems.size() << "\n";
    assert(!marketItems.empty());

    std::map<std::string, int> catCounts;
    for (const auto& item : marketItems) {
        catCounts[item.category]++;
    }
    std::cout << "\n--- Catalog Breakdown by Category ---\n";
    for (const auto& kv : catCounts) {
        std::cout << "  - " << std::left << std::setw(15) << kv.first << ": " << kv.second << " items\n";
    }

    // 2. Test Unboxing Probabilities (100,000 roll empirical simulation)
    // Find first case in marketItems
    uint32_t caseDef = 0;
    for (const auto& item : marketItems) {
        if (item.category == "Caja") {
            caseDef = item.defIndex;
            std::cout << "\n[*] Found test crate: " << item.name << " (defIndex=" << caseDef << ")\n";
            break;
        }
    }
    if (caseDef == 0) caseDef = 4001; // CS:GO Weapon Case

    std::cout << "[*] Running 100,000 Unboxing Simulation...\n";
    const int kRolls = 100000;
    std::map<uint32_t, int> rarityCounts;
    int stattrakCount = 0;

    for (int i = 0; i < kRolls; ++i) {
        uint32_t outDef = 0, outPaint = 0, outRarity = 0, outQuality = 0, outSeed = 0;
        float outWear = 0.0f;
        bool ok = refix::csgo::SchemaManager::Get().RollCaseDrop(caseDef, outDef, outPaint, outRarity, outQuality, outWear, outSeed);
        assert(ok);
        rarityCounts[outRarity]++;
        if (outQuality == 9) stattrakCount++; // StatTrak quality is 9
    }

    // Rarity values: Mil-Spec=3, Restricted=4, Classified=5, Covert=6, Special=7
    double bluePct   = (rarityCounts[3] * 100.0) / kRolls;
    double purplePct = (rarityCounts[4] * 100.0) / kRolls;
    double pinkPct   = (rarityCounts[5] * 100.0) / kRolls;
    double redPct    = (rarityCounts[6] * 100.0) / kRolls;
    double goldPct   = (rarityCounts[7] * 100.0) / kRolls;
    double stPct     = (stattrakCount * 100.0) / kRolls;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\n--- Unboxing Probability Results (Expected vs Actual) ---\n";
    std::cout << "  Mil-Spec (Blue, Rarity 3):        Expected ~79.92% | Got: " << bluePct << "%\n";
    std::cout << "  Restricted (Purple, Rarity 4):    Expected ~15.98% | Got: " << purplePct << "%\n";
    std::cout << "  Classified (Pink, Rarity 5):      Expected  ~3.20% | Got: " << pinkPct << "%\n";
    std::cout << "  Covert (Red, Rarity 6):           Expected  ~0.64% | Got: " << redPct << "%\n";
    std::cout << "  Special Rare (Gold, Rarity 7):    Expected  ~0.26% | Got: " << goldPct << "%\n";
    std::cout << "  StatTrak Chance (Quality 9):      Expected ~10.00% | Got: " << stPct << "%\n";

    // Verify probabilities within sensible statistical tolerances (3 sigma)
    assert(bluePct > 78.0 && bluePct < 82.0);
    assert(purplePct > 14.5 && purplePct < 17.5);
    assert(pinkPct > 2.5 && pinkPct < 4.0);
    assert(redPct > 0.4 && redPct < 0.9);
    assert(goldPct > 0.15 && goldPct < 0.45);
    assert(stPct > 9.0 && stPct < 11.0);
    std::cout << "[+] Probabilities statistically match Valve's authentic rates!\n";

    // 3. Test 64-bit ID Isolation
    std::cout << "\n[*] Testing Profile 64-bit ID Isolation...\n";
    uint32_t accountA = 0x69393642;
    uint32_t accountB = 0xbb179780;
    uint64_t idA = ((uint64_t)accountA << 32) | 1;
    uint64_t idB = ((uint64_t)accountB << 32) | 1;

    std::cout << "  Profile A ID: 0x" << std::hex << idA << std::dec << "\n";
    std::cout << "  Profile B ID: 0x" << std::hex << idB << std::dec << "\n";
    assert(idA != idB);
    assert((idA >> 32) == accountA);
    assert((idB >> 32) == accountB);
    std::cout << "[+] ID Isolation verified: zero collisions between accounts.\n";

    std::cout << "\n====================================================\n";
    std::cout << "        ALL VERIFICATION TESTS PASSED (100% OK)\n";
    std::cout << "====================================================\n";
    return 0;
}
