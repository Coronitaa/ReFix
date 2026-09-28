// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// unae_types.h - Core Data Structures and Enums
// =============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace UNAE {

// -----------------------------------------------------------------------------
// Network Topologies (UNAE Topology Classifier)
// -----------------------------------------------------------------------------
enum class NetworkTopology : uint8_t {
    Unknown = 0,
    TopologyA_DirectP2P       = 1, // Direct sockets (P2P Puro / LAN / Steam SDR)
    TopologyB_HybridMultiplex = 2, // Hybrid multiplex (EOS P2P / Mirror / Fusion)
    TopologyC_CloudRelayStrict= 3  // Strict Cloud-Relay (Photon PUN 2 / Photon Voice)
};

inline const char* TopologyToString(NetworkTopology topo) {
    switch (topo) {
        case NetworkTopology::TopologyA_DirectP2P:       return "Topologia A: Direct Sockets (P2P Puro / LAN / SDR)";
        case NetworkTopology::TopologyB_HybridMultiplex: return "Topologia B: Multiplexado Hibrido (EOS P2P / Mirror / Fusion)";
        case NetworkTopology::TopologyC_CloudRelayStrict:return "Topologia C: Cloud-Relay Estricto (PUN 2 / Photon Voice)";
        default:                                         return "Desconocida";
    }
}

// -----------------------------------------------------------------------------
// Global Arbitration Modes (ReFix.ini [Network] Mode)
// -----------------------------------------------------------------------------
enum class ArbitrationMode : uint8_t {
    Auto      = 0, // Detección automática y cascada inteligente (Recomendado)
    ForceP2P  = 1, // Fuerza transporte P2P directo (Tier 1 LAN / Tier 2 SDR)
    ForceRelay= 2, // Salta P2P directo y enruta a nubes de relevo (EOS / Photon)
    ForceLAN  = 3, // Aísla conectividad a la subred local (Tier 1 exclusivo)
    Offline   = 4  // Desactiva toda actividad de red externa; emulación local
};

inline ArbitrationMode StringToArbitrationMode(const std::string& str) {
    if (str == "force_p2p")   return ArbitrationMode::ForceP2P;
    if (str == "force_relay") return ArbitrationMode::ForceRelay;
    if (str == "force_lan")   return ArbitrationMode::ForceLAN;
    if (str == "offline")     return ArbitrationMode::Offline;
    return ArbitrationMode::Auto;
}

// -----------------------------------------------------------------------------
// 4-Tier Connectivity Cascade
// -----------------------------------------------------------------------------
enum class ConnectivityTier : uint8_t {
    Tier1_LAN         = 1, // LAN Autónoma (Multicast UDP 239.255.71.84:47584)
    Tier2_WAN_P2P     = 2, // WAN P2P Directo (Winsock Hooks + Steam SDR AppID 480)
    Tier3_EOS_DualMode= 3, // Dual-Mode EOS (DeviceId Passthrough / WebRTC)
    Tier4_Photon_Cloud= 4  // Photon Cloud Fallback (Realtime / Voice / Fusion)
};

inline const char* TierToString(ConnectivityTier tier) {
    switch (tier) {
        case ConnectivityTier::Tier1_LAN:         return "Tier 1: LAN Autonoma (Multicast UDP + Direct Sockets)";
        case ConnectivityTier::Tier2_WAN_P2P:     return "Tier 2: WAN P2P Directo (Winsock MinHook + Steam SDR)";
        case ConnectivityTier::Tier3_EOS_DualMode:return "Tier 3: Dual-Mode EOS (DeviceIdAuth / WebRTC P2P)";
        case ConnectivityTier::Tier4_Photon_Cloud:return "Tier 4: Photon Cloud Fallback (Realtime / Voice / Fusion)";
        default:                                  return "Tier Desconocido";
    }
}

// -----------------------------------------------------------------------------
// Photon Ecosystem Settings
// -----------------------------------------------------------------------------
enum class PhotonBackend : uint8_t {
    OfficialPhoton = 0,
    ReFixCloud     = 1,
    CustomPhoton   = 2
};

enum class RegionMode : uint8_t {
    Dynamic = 0, // Preserva selección in-game (DRPI)
    Auto    = 1, // Sondeo de ping automático
    Fixed   = 2  // Forzado estático en DefaultRegion
};

struct PhotonConfig {
    PhotonBackend backend = PhotonBackend::OfficialPhoton;
    RegionMode    regionMode = RegionMode::Dynamic;
    std::string   appIdRealtime;
    std::string   appIdFusion;
    std::string   appIdVoice;
    std::string   defaultRegion = "sa";
    bool          preserveGameUiRegion = true;
};

// -----------------------------------------------------------------------------
// Detected Runtime Capabilities
// -----------------------------------------------------------------------------
struct GameCapabilities {
    // Phase 1 (PE/IAT)
    bool hasWinsock           = false; // ws2_32.dll
    bool hasNanosockets       = false; // nanosockets.dll
    bool hasSteamApi          = false; // steam_api64.dll
    bool hasEOS               = false; // EOSSDK-Win64-Shipping.dll

    // Phase 3 (Unity Managed Assemblies)
    bool hasPhoton3Unity      = false; // Photon3Unity3D.dll
    bool hasPhotonRealtime    = false; // PhotonRealtime.dll (PUN 2)
    bool hasPhotonVoice       = false; // PhotonVoice.dll
    bool hasPhotonFusion      = false; // Fusion.Runtime.dll
    bool hasMirror            = false; // Mirror.dll
    bool hasKcp               = false; // kcp2k.dll

    // Phase 4 (IL2CPP Metadata Scan)
    bool isIL2CPP             = false;
    bool il2cppHasPhotonPUN   = false;
    bool il2cppHasFusion      = false;
    bool il2cppHasMirror      = false;

    // Derived Classification
    NetworkTopology topology  = NetworkTopology::Unknown;
    std::string engineName    = "Unknown";
    std::string detectedGame  = "Generic";
};

// -----------------------------------------------------------------------------
// Anti-Flapping Hysteresis State
// -----------------------------------------------------------------------------
struct AntiFlappingState {
    const uint32_t lockingWindowMs   = 15000; // 15s stability before tier change
    const uint32_t failureThreshold  = 5;     // 5 consecutive heartbeat failures
    const uint32_t recoveryCooldownMs= 60000; // 60s cooldown before tier elevation

    uint32_t consecutiveFailures     = 0;
    uint32_t lastTierSwitchTime      = 0;
    uint32_t lastEvaluationTime      = 0;
    bool     isLocked                = false;
};

// -----------------------------------------------------------------------------
// Global UNAE Configuration
// -----------------------------------------------------------------------------
struct UNAEConfig {
    ArbitrationMode mode = ArbitrationMode::Auto;
    uint16_t discoveryPort = 47584;
    std::string discoveryGroup = "239.255.71.84";
    uint32_t lanProbeTimeoutMs = 2500;
    std::string customBroadcasts;
    bool verboseLog = true;

    // P2P
    bool enableWinsockHooks = true;
    bool enableSteamSDR = true;
    bool allowRelay = true;
    uint16_t p2pPort = 7777;
    uint32_t peerHandshakeTimeoutMs = 5000;
    uint32_t maxPacketLossTolerance = 40;

    // Photon & Voice
    PhotonConfig photon;
};

} // namespace UNAE
