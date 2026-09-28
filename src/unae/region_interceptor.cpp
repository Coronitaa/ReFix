// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// region_interceptor.cpp - DRPI Implementation
// =============================================================================
#include "region_interceptor.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

extern void ReFixLog(const char* fmt, ...);

namespace UNAE {

RegionInterceptor& RegionInterceptor::Instance() {
    static RegionInterceptor s_instance;
    return s_instance;
}

void RegionInterceptor::Initialize(const PhotonConfig& config) {
    std::lock_guard<std::mutex> lg(m_mutex);
    m_config = config;
    m_userSelectedRegion.clear(); // Empty until UI or user explicitly selects a region
    m_initialized = true;

    ReFixLog("[UNAE:DRPI] Initialized Dynamic Region Preservation Interceptor:");
    ReFixLog("  -> Mode: %s, Default: %s, Preserve UI: %s",
        (m_config.regionMode == RegionMode::Dynamic ? "Dynamic" :
         m_config.regionMode == RegionMode::Auto ? "Auto" : "Fixed"),
        m_config.defaultRegion.c_str(),
        m_config.preserveGameUiRegion ? "TRUE" : "FALSE");
}

void RegionInterceptor::SetUserSelectedRegion(const std::string& region) {
    if (region.empty()) return;

    std::string clean = region;
    // Strip trailing '/' or whitespace
    clean.erase(std::remove_if(clean.begin(), clean.end(), [](char c) {
        return c == '/' || c == '\\' || c == ' ' || c == '\r' || c == '\n';
    }), clean.end());
    std::transform(clean.begin(), clean.end(), clean.begin(), ::tolower);

    std::lock_guard<std::mutex> lg(m_mutex);
    if (m_config.regionMode == RegionMode::Fixed) {
        ReFixLog("[UNAE:DRPI] Ignored UI region '%s' (FixedRegion=%s enforced in ReFix.ini)",
            clean.c_str(), m_config.defaultRegion.c_str());
        return;
    }

    m_userSelectedRegion = clean;
    ReFixLog("[UNAE:DRPI] Intercepted user-selected region: '%s' (preservation active, no INI overwrite)",
        clean.c_str());
}

std::string RegionInterceptor::GetUserSelectedRegion() const {
    std::lock_guard<std::mutex> lg(m_mutex);
    return m_userSelectedRegion.empty() ? m_config.defaultRegion : m_userSelectedRegion;
}

std::string RegionInterceptor::ResolveTargetEndpoint(const std::string& requestedRegion) {
    std::lock_guard<std::mutex> lg(m_mutex);

    std::string effRegion = requestedRegion;
    if (m_config.regionMode == RegionMode::Fixed) {
        effRegion = m_config.defaultRegion;
    } else if (!m_userSelectedRegion.empty()) {
        effRegion = m_userSelectedRegion;
    }

    if (m_config.backend == PhotonBackend::OfficialPhoton) {
        // Enrutamiento directo al NameServer oficial con token regional preservado
        return effRegion;
    } else if (m_config.backend == PhotonBackend::ReFixCloud) {
        // Mapea dinámicamente al cluster ReFix correspondiente
        return effRegion + ".refixcloud.net";
    }

    return effRegion;
}

uint32_t RegionInterceptor::GetEchoProbePingMs(const std::string& region) {
    std::string reg = region;
    std::transform(reg.begin(), reg.end(), reg.begin(), ::tolower);

    // Realistic baseline pings for standard clusters
    if (reg.find("sa") != std::string::npos)   return 25;
    if (reg.find("us") != std::string::npos)   return 115;
    if (reg.find("eu") != std::string::npos)   return 165;
    if (reg.find("asia") != std::string::npos) return 275;
    if (reg.find("jp") != std::string::npos)   return 290;
    if (reg.find("ru") != std::string::npos)   return 210;

    return 75;
}

bool RegionInterceptor::IsPreservationActive() const {
    return (m_config.regionMode == RegionMode::Dynamic && m_config.preserveGameUiRegion);
}

} // namespace UNAE

extern "C" {
__declspec(dllexport) void UNAE_SetSelectedRegion(const char* regionStr) {
    if (regionStr && *regionStr) {
        UNAE::RegionInterceptor::Instance().SetUserSelectedRegion(regionStr);
    }
}

__declspec(dllexport) const char* UNAE_GetSelectedRegion() {
    static char s_regionBuf[64];
    std::string reg = UNAE::RegionInterceptor::Instance().GetUserSelectedRegion();
    strncpy_s(s_regionBuf, sizeof(s_regionBuf), reg.c_str(), _TRUNCATE);
    return s_regionBuf;
}
}
