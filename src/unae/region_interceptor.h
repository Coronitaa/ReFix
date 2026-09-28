// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// region_interceptor.h - Dynamic Region Preservation Interceptor (DRPI)
// =============================================================================
#pragma once
#include "unae_types.h"
#include <string>
#include <mutex>

namespace UNAE {

class RegionInterceptor {
public:
    static RegionInterceptor& Instance();

    void Initialize(const PhotonConfig& config);

    // Records the region chosen by the player in-game (US / EU / SA / Asia)
    void SetUserSelectedRegion(const std::string& region);

    // Retrieves the currently active user region
    std::string GetUserSelectedRegion() const;

    // Resolves effective cluster domain or region token according to configured backend
    std::string ResolveTargetEndpoint(const std::string& requestedRegion);

    // Simulates / responds to ping probe queries for UI ping bars
    uint32_t GetEchoProbePingMs(const std::string& region);

    bool IsPreservationActive() const;

private:
    RegionInterceptor() = default;

    mutable std::mutex m_mutex;
    PhotonConfig m_config;
    std::string  m_userSelectedRegion;
    bool         m_initialized = false;
};

} // namespace UNAE

extern "C" {
    __declspec(dllexport) void UNAE_SetSelectedRegion(const char* regionStr);
    __declspec(dllexport) const char* UNAE_GetSelectedRegion();
}
