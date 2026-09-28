// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// cascade_arbiter.h - 4-Tier Connectivity Cascade with Anti-Flapping
// =============================================================================
#pragma once
#include "unae_types.h"
#include <mutex>

namespace UNAE {

class CascadeArbiter {
public:
    static CascadeArbiter& Instance();

    void Initialize(const GameCapabilities& caps, const UNAEConfig& config);
    void Update(uint32_t deltaMs);

    // Records peer heartbeat success/failure to manage anti-flapping transitions
    void RecordHeartbeat(bool success);

    // Evaluates current optimal connectivity route
    ConnectivityTier EvaluateRoute();

    // Force transition to specific Tier (admin or failover override)
    void ForceTier(ConnectivityTier tier);

    // Status queries
    ConnectivityTier GetActiveTier() const { return m_activeTier; }
    bool IsDirectP2PAllowed() const;
    const AntiFlappingState& GetAntiFlappingState() const { return m_afState; }

private:
    CascadeArbiter() = default;

    void TransitionToTier(ConnectivityTier newTier, const char* reason);

    mutable std::mutex m_mutex;
    GameCapabilities m_caps;
    UNAEConfig       m_config;
    ConnectivityTier m_activeTier = ConnectivityTier::Tier1_LAN;
    AntiFlappingState m_afState;
    uint32_t         m_uptimeMs = 0;
    bool             m_initialized = false;
};

} // namespace UNAE
