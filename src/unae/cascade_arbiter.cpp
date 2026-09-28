// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// cascade_arbiter.cpp - Implementation
// =============================================================================
#include "cascade_arbiter.h"
#include <windows.h>
#include <cstdio>

extern void ReFixLog(const char* fmt, ...);

namespace UNAE {

CascadeArbiter& CascadeArbiter::Instance() {
    static CascadeArbiter s_instance;
    return s_instance;
}

void CascadeArbiter::Initialize(const GameCapabilities& caps, const UNAEConfig& config) {
    std::lock_guard<std::mutex> lg(m_mutex);
    m_caps = caps;
    m_config = config;
    m_uptimeMs = 0;
    m_afState.consecutiveFailures = 0;
    m_afState.lastTierSwitchTime = GetTickCount();
    m_afState.lastEvaluationTime = GetTickCount();
    m_afState.isLocked = false;

    // Determine initial tier based on configuration and detected topology
    if (m_config.mode == ArbitrationMode::ForceLAN || m_config.mode == ArbitrationMode::Offline) {
        m_activeTier = ConnectivityTier::Tier1_LAN;
    } else if (m_config.mode == ArbitrationMode::ForceP2P) {
        m_activeTier = ConnectivityTier::Tier2_WAN_P2P;
    } else if (m_config.mode == ArbitrationMode::ForceRelay) {
        if (m_caps.topology == NetworkTopology::TopologyC_CloudRelayStrict) {
            m_activeTier = ConnectivityTier::Tier4_Photon_Cloud;
        } else if (m_caps.hasEOS) {
            m_activeTier = ConnectivityTier::Tier3_EOS_DualMode;
        } else {
            m_activeTier = ConnectivityTier::Tier4_Photon_Cloud;
        }
    } else { // ArbitrationMode::Auto
        if (m_caps.topology == NetworkTopology::TopologyC_CloudRelayStrict) {
            // Topología C: PUN 2 / Photon Voice NO soporta P2P directo (desincroniza Actor IDs)
            m_activeTier = ConnectivityTier::Tier4_Photon_Cloud;
        } else if (m_caps.topology == NetworkTopology::TopologyB_HybridMultiplex) {
            // Topología B: Comienza en Tier 1 LAN, listo para escalar a Tier 2 SDR o Tier 3 EOS
            m_activeTier = ConnectivityTier::Tier1_LAN;
        } else {
            // Topología A: LAN / Sockets directos
            m_activeTier = ConnectivityTier::Tier1_LAN;
        }
    }

    m_initialized = true;

    ReFixLog("[UNAE:Arbiter] Initialized 4-Tier Connectivity Cascade:");
    ReFixLog("  -> Active Route: %s", TierToString(m_activeTier));
    ReFixLog("  -> Direct P2P Permitted: %s", IsDirectP2PAllowed() ? "YES" : "NO (Protected)");
}

bool CascadeArbiter::IsDirectP2PAllowed() const {
    // Si el juego es Topología C estricta (Phasmophobia, R.E.P.O.), P2P directo está deshabilitado
    // para evitar el vector de ruptura V-05 (desincronización de Actor Numbers y Room Event Cache).
    if (m_caps.topology == NetworkTopology::TopologyC_CloudRelayStrict) {
        return false;
    }
    if (m_config.mode == ArbitrationMode::Offline) {
        return false;
    }
    return (m_activeTier == ConnectivityTier::Tier1_LAN ||
            m_activeTier == ConnectivityTier::Tier2_WAN_P2P);
}

void CascadeArbiter::TransitionToTier(ConnectivityTier newTier, const char* reason) {
    if (m_activeTier == newTier) return;

    ConnectivityTier oldTier = m_activeTier;
    m_activeTier = newTier;
    m_afState.lastTierSwitchTime = GetTickCount();
    m_afState.consecutiveFailures = 0;
    m_afState.isLocked = true; // Engage 15s stability lock

    ReFixLog("[UNAE:Arbiter] TIER TRANSITION: %s -> %s", TierToString(oldTier), TierToString(newTier));
    ReFixLog("  -> Reason: %s", reason ? reason : "Unspecified");
    ReFixLog("  -> Anti-Flapping: Locking window engaged (15000 ms stability)");
}

void CascadeArbiter::RecordHeartbeat(bool success) {
    std::lock_guard<std::mutex> lg(m_mutex);
    if (!m_initialized) return;

    DWORD now = GetTickCount();

    if (success) {
        m_afState.consecutiveFailures = 0;
        // Evaluate 60s recovery cooldown to ascend tier if network is stable
        if (now - m_afState.lastTierSwitchTime >= m_afState.recoveryCooldownMs) {
            if (m_caps.topology != NetworkTopology::TopologyC_CloudRelayStrict) {
                if (m_activeTier == ConnectivityTier::Tier4_Photon_Cloud) {
                    TransitionToTier(m_caps.hasEOS ? ConnectivityTier::Tier3_EOS_DualMode : ConnectivityTier::Tier2_WAN_P2P,
                                     "60s stable heartbeat recovery -> Ascending Tier");
                } else if (m_activeTier == ConnectivityTier::Tier3_EOS_DualMode) {
                    TransitionToTier(ConnectivityTier::Tier2_WAN_P2P, "60s stable heartbeat recovery -> Ascending to WAN P2P");
                } else if (m_activeTier == ConnectivityTier::Tier2_WAN_P2P) {
                    TransitionToTier(ConnectivityTier::Tier1_LAN, "60s stable heartbeat recovery -> Ascending to Tier 1 LAN");
                }
            }
        }
        return;
    }

    // Heartbeat failed
    m_afState.consecutiveFailures++;

    // Check if locking window active (do not flap if within 15s of last switch)
    if (now - m_afState.lastTierSwitchTime < m_afState.lockingWindowMs) {
        return;
    }

    // Degrade tier if threshold reached
    if (m_afState.consecutiveFailures >= m_afState.failureThreshold) {
        if (m_activeTier == ConnectivityTier::Tier1_LAN) {
            TransitionToTier(ConnectivityTier::Tier2_WAN_P2P, "5 consecutive LAN heartbeats failed -> Falling back to WAN SDR P2P");
        } else if (m_activeTier == ConnectivityTier::Tier2_WAN_P2P) {
            if (m_caps.hasEOS) {
                TransitionToTier(ConnectivityTier::Tier3_EOS_DualMode, "WAN P2P handshake failed -> Falling back to Epic WebRTC Dual-Mode");
            } else {
                TransitionToTier(ConnectivityTier::Tier4_Photon_Cloud, "WAN P2P failed and no EOS detected -> Falling back to Tier 4 Photon Cloud");
            }
        } else if (m_activeTier == ConnectivityTier::Tier3_EOS_DualMode) {
            TransitionToTier(ConnectivityTier::Tier4_Photon_Cloud, "EOS WebRTC connection degraded -> Falling back to Tier 4 Photon Cloud");
        }
    }
}

void CascadeArbiter::Update(uint32_t deltaMs) {
    std::lock_guard<std::mutex> lg(m_mutex);
    if (!m_initialized) return;

    m_uptimeMs += deltaMs;
    DWORD now = GetTickCount();

    // Update locking status
    if (now - m_afState.lastTierSwitchTime < m_afState.lockingWindowMs) {
        m_afState.isLocked = true;
    } else {
        m_afState.isLocked = false;
    }
}

void CascadeArbiter::ForceTier(ConnectivityTier tier) {
    std::lock_guard<std::mutex> lg(m_mutex);
    TransitionToTier(tier, "Admin / Manual Override");
}

ConnectivityTier CascadeArbiter::EvaluateRoute() {
    std::lock_guard<std::mutex> lg(m_mutex);
    return m_activeTier;
}

} // namespace UNAE
