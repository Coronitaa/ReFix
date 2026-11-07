#include "inventory_store.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace stdfs = std::filesystem;

namespace refix::econ {

InventoryStore& InventoryStore::Get() {
    static InventoryStore s_instance;
    return s_instance;
}

InventoryStore::InventoryStore() {
    refix::GetRefixServices().inventory = this;
}

uint64_t InventoryStore::ComposeItemId(uint32_t account_id, uint32_t low) {
    return (static_cast<uint64_t>(account_id) << 32) | low;
}

std::string InventoryStore::GetInventoryFilePath(uint64_t steam_id) const {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s\\ReFix\\profiles\\%08llx", appdata, (unsigned long long)(steam_id & 0xFFFFFFFF));
        CreateDirectoryA(dir, NULL);
        char file[MAX_PATH];
        snprintf(file, sizeof(file), "%s\\inventory.json", dir);
        return file;
    }
    return "inventory.json";
}

uint64_t InventoryStore::AllocateItemId(uint64_t steam_id) {
    uint32_t accountId = static_cast<uint32_t>(steam_id & 0xFFFFFFFF);
    uint32_t low = ++m_lastLowIds[steam_id];
    return ComposeItemId(accountId, low);
}

bool InventoryStore::Load(uint64_t steam_id) {
    std::lock_guard<std::mutex> lock(m_mu);
    std::string path = GetInventoryFilePath(steam_id);
    std::ifstream f(path);
    if (!f.is_open()) return false;

    // Load items JSON structure
    m_profiles[steam_id].clear();
    uint32_t maxLow = 0;

    std::string line;
    InventoryItem current;
    bool inItem = false;

    while (std::getline(f, line)) {
        if (line.find("\"id\":") != std::string::npos) {
            uint64_t id = 0;
            if (sscanf_s(line.c_str(), " \"id\": %llu", &id) == 1) {
                current.id = id;
                uint32_t low = static_cast<uint32_t>(id & 0xFFFFFFFF);
                if (low > maxLow) maxLow = low;
            }
        } else if (line.find("\"defIndex\":") != std::string::npos) {
            sscanf_s(line.c_str(), " \"defIndex\": %u", &current.defIndex);
        } else if (line.find("\"quality\":") != std::string::npos) {
            sscanf_s(line.c_str(), " \"quality\": %u", &current.quality);
        } else if (line.find("\"rarity\":") != std::string::npos) {
            sscanf_s(line.c_str(), " \"rarity\": %u", &current.rarity);
        } else if (line.find("\"paintKit\":") != std::string::npos) {
            sscanf_s(line.c_str(), " \"paintKit\": %u", &current.paintKit);
        } else if (line.find("\"wear\":") != std::string::npos) {
            sscanf_s(line.c_str(), " \"wear\": %f", &current.wear);
        } else if (line.find("\"seed\":") != std::string::npos) {
            sscanf_s(line.c_str(), " \"seed\": %u", &current.seed);
        } else if (line.find("}") != std::string::npos) {
            if (current.id != 0 && current.defIndex != 0) {
                m_profiles[steam_id].push_back(current);
                current = InventoryItem();
            }
        }
    }

    m_lastLowIds[steam_id] = (maxLow > 0) ? maxLow : 1;
    return true;
}

bool InventoryStore::Save(uint64_t steam_id) {
    std::lock_guard<std::mutex> lock(m_mu);
    std::string path = GetInventoryFilePath(steam_id);
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return false;

    f << "{\n  \"items\": [\n";
    const auto& items = m_profiles[steam_id];
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& itm = items[i];
        f << "    {\n";
        f << "      \"id\": " << itm.id << ",\n";
        f << "      \"defIndex\": " << itm.defIndex << ",\n";
        f << "      \"quality\": " << itm.quality << ",\n";
        f << "      \"rarity\": " << itm.rarity << ",\n";
        f << "      \"paintKit\": " << itm.paintKit << ",\n";
        f << "      \"wear\": " << itm.wear << ",\n";
        f << "      \"seed\": " << itm.seed << "\n";
        f << "    }" << (i + 1 < items.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

std::vector<InventoryItem> InventoryStore::GetItems(uint64_t steam_id) {
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = m_profiles.find(steam_id);
    if (it != m_profiles.end()) {
        return it->second;
    }
    return {};
}

uint64_t InventoryStore::GrantItem(uint64_t steam_id, uint32_t defIndex, uint32_t level, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed, const char* customName) {
    std::lock_guard<std::mutex> lock(m_mu);
    uint32_t accountId = static_cast<uint32_t>(steam_id & 0xFFFFFFFF);
    uint32_t low = ++m_lastLowIds[steam_id];
    uint64_t newId = ComposeItemId(accountId, low);

    InventoryItem itm;
    itm.id = newId;
    itm.defIndex = defIndex;
    itm.level = level;
    itm.quality = quality;
    itm.rarity = rarity;
    itm.paintKit = paintKit;
    itm.wear = wear;
    itm.seed = seed;
    if (customName) itm.customName = customName;

    m_profiles[steam_id].push_back(itm);
    return newId;
}

bool InventoryStore::SetEquipped(uint64_t steam_id, uint64_t item_id, uint32_t team, uint32_t slot) {
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = m_profiles.find(steam_id);
    if (it == m_profiles.end()) return false;

    // Remove existing item in that slot for that team
    for (auto& itm : it->second) {
        itm.equipped.erase(std::remove_if(itm.equipped.begin(), itm.equipped.end(),
            [team, slot](const EquippedState& eq) {
                return eq.team == team && eq.slot == slot;
            }), itm.equipped.end());
    }

    // Equip the new item
    for (auto& itm : it->second) {
        if (itm.id == item_id) {
            itm.equipped.push_back({ team, slot });
            return true;
        }
    }
    return false;
}

} // namespace refix::econ
