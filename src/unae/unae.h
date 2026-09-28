// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// unae.h - Main UNAE Subsystem Interface
// =============================================================================
#pragma once
#include "unae_types.h"
#include "capability_detector.h"
#include "cascade_arbiter.h"
#include "region_interceptor.h"

namespace UNAE {

// Initializes UNAE: parses ReFix.ini, executes 4-phase scan, classifies topology,
// arms the 4-tier cascade arbiter, and initializes DRPI.
void Initialize();

// Query current UNAE status
const GameCapabilities& GetCapabilities();
NetworkTopology GetTopology();
ConnectivityTier GetActiveTier();
bool IsDirectP2PAllowed();
const UNAEConfig& GetConfig();

} // namespace UNAE

extern "C" {
    __declspec(dllexport) void UNAE_Initialize();
    __declspec(dllexport) int  UNAE_GetTopology();
    __declspec(dllexport) int  UNAE_GetActiveTier();
    __declspec(dllexport) bool UNAE_IsDirectP2PAllowed();
}
