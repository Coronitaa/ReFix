#include "eos_ids.h"

namespace refix {

IdRegistry& IdRegistry::Get() { static IdRegistry r; return r; }

static std::string Normalize(const std::string& raw, size_t maxLen) {
    std::string s = Trim(raw);
    if (s.size() > maxLen) s.resize(maxLen);
    return s;
}

EOS_ProductUserId IdRegistry::Puid(const std::string& raw) {
    std::string id = Normalize(raw, EOS_PRODUCTUSERID_MAX_LENGTH);
    if (id.empty()) return nullptr;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_puids.find(id);
    if (it != m_puids.end()) return (EOS_ProductUserId)it->second;
    auto* d = new EOS_ProductUserIdDetails();
    std::memset(d, 0, sizeof(*d));
    std::memcpy(d->id, id.c_str(), id.size());
    m_puids[id] = d;
    return (EOS_ProductUserId)d;
}

EOS_EpicAccountId IdRegistry::Eaid(const std::string& raw) {
    std::string id = Normalize(raw, EOS_EPICACCOUNTID_MAX_LENGTH);
    if (id.empty()) return nullptr;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_eaids.find(id);
    if (it != m_eaids.end()) return (EOS_EpicAccountId)it->second;
    auto* d = new EOS_EpicAccountIdDetails();
    std::memset(d, 0, sizeof(*d));
    std::memcpy(d->id, id.c_str(), id.size());
    m_eaids[id] = d;
    return (EOS_EpicAccountId)d;
}

bool IdRegistry::IsValidPuid(EOS_ProductUserId id) {
    if (!id) return false;
    auto& r = Get();
    std::lock_guard<std::mutex> lock(r.m_mutex);
    for (auto& kv : r.m_puids) if ((EOS_ProductUserId)kv.second == id) return true;
    return false;
}

bool IdRegistry::IsValidEaid(EOS_EpicAccountId id) {
    if (!id) return false;
    auto& r = Get();
    std::lock_guard<std::mutex> lock(r.m_mutex);
    for (auto& kv : r.m_eaids) if ((EOS_EpicAccountId)kv.second == id) return true;
    return false;
}

const char* IdRegistry::ToString(EOS_ProductUserId id) {
    return IsValidPuid(id) ? ((EOS_ProductUserIdDetails*)id)->id : nullptr;
}

const char* IdRegistry::ToString(EOS_EpicAccountId id) {
    return IsValidEaid(id) ? ((EOS_EpicAccountIdDetails*)id)->id : nullptr;
}

} // namespace refix
