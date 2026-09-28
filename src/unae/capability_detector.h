// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// capability_detector.h - Runtime Capability Scanner & Topology Classifier
// =============================================================================
#pragma once
#include "unae_types.h"
#include <string>

namespace UNAE {

class CapabilityDetector {
public:
    static CapabilityDetector& Instance();

    // Executes 4-phase runtime detection scan (PE/IAT, Loaded Modules, Unity Managed, IL2CPP)
    const GameCapabilities& ScanCapabilities();

    // Returns previously scanned capabilities
    const GameCapabilities& GetCapabilities() const { return m_caps; }

    // Evaluates and returns optimal NetworkTopology based on detected capabilities
    NetworkTopology ClassifyTopology();

private:
    CapabilityDetector() = default;

    void ScanPhase1_IAT();
    void ScanPhase2_LoadedModules();
    void ScanPhase3_UnityManaged();
    void ScanPhase4_IL2CPPMetadata();

    std::string GetExecutableDirectory() const;
    std::string GetExecutableBaseName() const;

    GameCapabilities m_caps;
    bool m_scanned = false;
};

} // namespace UNAE
