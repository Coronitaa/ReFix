#include "entitlements_store.h"
#include "../overlay/refix_overlay.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>

namespace refix::entitlements {

EntitlementsStore& EntitlementsStore::Get() {
    static EntitlementsStore s_instance;
    return s_instance;
}

EntitlementsStore::EntitlementsStore() {
    refix::GetRefixServices().entitlements = this;
}

std::string EntitlementsStore::GetFilePath(uint64_t steam_id) const {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s\\ReFix\\profiles\\%08llx", appdata, (unsigned long long)(steam_id & 0xFFFFFFFF));
        CreateDirectoryA(dir, NULL);
        char file[MAX_PATH];
        snprintf(file, sizeof(file), "%s\\entitlements.txt", dir);
        return file;
    }
    return "entitlements.txt";
}

bool EntitlementsStore::Load(uint64_t steam_id) {
    std::lock_guard<std::mutex> lock(m_mu);
    std::string path = GetFilePath(steam_id);
    std::ifstream f(path);
    if (!f.is_open()) return false;

    m_entitlements[steam_id].clear();
    uint32_t ent = 0;
    while (f >> ent) {
        m_entitlements[steam_id].insert(ent);
    }
    return true;
}

bool EntitlementsStore::Save(uint64_t steam_id) {
    std::lock_guard<std::mutex> lock(m_mu);
    std::string path = GetFilePath(steam_id);
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return false;

    for (uint32_t ent : m_entitlements[steam_id]) {
        f << ent << "\n";
    }
    return true;
}

bool EntitlementsStore::HasEntitlement(uint64_t steam_id, uint32_t entitlement_id) {
    std::lock_guard<std::mutex> lock(m_mu);
    auto it = m_entitlements.find(steam_id);
    if (it != m_entitlements.end()) {
        return it->second.count(entitlement_id) > 0;
    }
    return false;
}

void EntitlementsStore::GrantEntitlement(uint64_t steam_id, uint32_t entitlement_id) {
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_entitlements[steam_id].insert(entitlement_id);
    }
    Save(steam_id);
}

void EntitlementsStore::PromptPurchase(uint32_t app_id, uint32_t item_id, const char* name, const char* description, bool requires_elevation, EntitlementCallbackFn callback) {
    std::string itemName = name ? name : "Item";
    std::string itemDesc = description ? description : "";
    ReFixOverlay::PromptPurchase(app_id, item_id, itemName, itemDesc, requires_elevation, [callback](bool approved) {
        if (callback) callback(approved);
    });
}

} // namespace refix::entitlements
