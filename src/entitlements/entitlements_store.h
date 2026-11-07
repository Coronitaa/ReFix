// Game Entitlements: Prime, operations, DLC, and overlay purchase flow
#pragma once

#include "../include/refix_services.h"
#include <cstdint>
#include <unordered_map>
#include <set>
#include <mutex>
#include <string>

namespace refix::entitlements {

class EntitlementsStore : public refix::IEntitlements {
public:
    static EntitlementsStore& Get();

    EntitlementsStore();
    virtual ~EntitlementsStore() override = default;

    virtual bool HasEntitlement(uint64_t steam_id, uint32_t entitlement_id) override;
    virtual void GrantEntitlement(uint64_t steam_id, uint32_t entitlement_id) override;
    virtual void PromptPurchase(uint32_t app_id, uint32_t item_id, const char* name, const char* description, bool requires_elevation, EntitlementCallbackFn callback) override;

    bool Load(uint64_t steam_id);
    bool Save(uint64_t steam_id);

private:
    std::string GetFilePath(uint64_t steam_id) const;

    std::mutex m_mu;
    std::unordered_map<uint64_t, std::set<uint32_t>> m_entitlements;
};

} // namespace refix::entitlements
