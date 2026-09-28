// =============================================================================
// ReFix - Universal Network Arbitration Engine (UNAE)
// unae.cpp - Main UNAE Subsystem Implementation
// =============================================================================
#include "unae.h"
#include <windows.h>
#include <cstdio>
#include <string>
#include <algorithm>

extern void ReFixLog(const char* fmt, ...);

namespace UNAE {

static UNAEConfig s_config;
static bool       s_initialized = false;

static std::string GetIniPath() {
    char path[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string s(path);
    size_t pos = s.find_last_of("\\/");
    return (pos != std::string::npos ? s.substr(0, pos + 1) : ".\\") + "ReFix.ini";
}

static void LoadConfigFromIni(UNAEConfig& cfg) {
    std::string ini = GetIniPath();

    char buf[256] = { 0 };

    // [Network]
    GetPrivateProfileStringA("Network", "Mode", "auto", buf, sizeof(buf), ini.c_str());
    cfg.mode = StringToArbitrationMode(buf);
    cfg.discoveryPort = (uint16_t)GetPrivateProfileIntA("Network", "DiscoveryPort", 47584, ini.c_str());
    GetPrivateProfileStringA("Network", "DiscoveryGroup", "239.255.71.84", buf, sizeof(buf), ini.c_str());
    cfg.discoveryGroup = buf;
    cfg.lanProbeTimeoutMs = (uint32_t)GetPrivateProfileIntA("Network", "LanProbeTimeoutMs", 2500, ini.c_str());
    cfg.verboseLog = (GetPrivateProfileIntA("Network", "VerboseArbitrationLog", 1, ini.c_str()) != 0);

    // [P2P]
    cfg.enableWinsockHooks = (GetPrivateProfileIntA("P2P", "EnableWinsockHooks", 1, ini.c_str()) != 0);
    cfg.enableSteamSDR = (GetPrivateProfileIntA("P2P", "EnableSteamSDR", 1, ini.c_str()) != 0);
    cfg.allowRelay = (GetPrivateProfileIntA("P2P", "AllowRelay", 1, ini.c_str()) != 0);
    cfg.p2pPort = (uint16_t)GetPrivateProfileIntA("P2P", "P2PPort", 7777, ini.c_str());
    cfg.peerHandshakeTimeoutMs = (uint32_t)GetPrivateProfileIntA("P2P", "PeerHandshakeTimeoutMs", 5000, ini.c_str());
    cfg.maxPacketLossTolerance = (uint32_t)GetPrivateProfileIntA("P2P", "MaxPacketLossTolerance", 40, ini.c_str());

    // [Photon]
    GetPrivateProfileStringA("Photon", "Backend", "OfficialPhoton", buf, sizeof(buf), ini.c_str());
    if (_stricmp(buf, "ReFixCloud") == 0) cfg.photon.backend = PhotonBackend::ReFixCloud;
    else if (_stricmp(buf, "CustomPhoton") == 0) cfg.photon.backend = PhotonBackend::CustomPhoton;
    else cfg.photon.backend = PhotonBackend::OfficialPhoton;

    GetPrivateProfileStringA("Photon", "AppIdRealtime", "", buf, sizeof(buf), ini.c_str());
    cfg.photon.appIdRealtime = buf;

    GetPrivateProfileStringA("Photon", "AppIdFusion", "", buf, sizeof(buf), ini.c_str());
    cfg.photon.appIdFusion = buf;

    GetPrivateProfileStringA("Photon", "RegionMode", "dynamic", buf, sizeof(buf), ini.c_str());
    if (_stricmp(buf, "auto") == 0) cfg.photon.regionMode = RegionMode::Auto;
    else if (_stricmp(buf, "fixed") == 0) cfg.photon.regionMode = RegionMode::Fixed;
    else cfg.photon.regionMode = RegionMode::Dynamic;

    GetPrivateProfileStringA("Photon", "DefaultRegion", "sa", buf, sizeof(buf), ini.c_str());
    cfg.photon.defaultRegion = buf;

    cfg.photon.preserveGameUiRegion = (GetPrivateProfileIntA("Photon", "PreserveGameUiRegion", 1, ini.c_str()) != 0);

    // [Voice]
    GetPrivateProfileStringA("Voice", "AppIdVoice", "", buf, sizeof(buf), ini.c_str());
    cfg.photon.appIdVoice = buf;
}

void Initialize() {
    if (s_initialized) return;

    ReFixLog("====================================================================");
    ReFixLog("[UNAE] Universal Network Arbitration Engine v3.0 Initializing...");
    ReFixLog("====================================================================");

    // 1. Load UNAE configuration
    LoadConfigFromIni(s_config);

    // 2. Scan Runtime Capabilities & Classify Topology
    const GameCapabilities& caps = CapabilityDetector::Instance().ScanCapabilities();

    // 3. Initialize Cascade Arbiter
    CascadeArbiter::Instance().Initialize(caps, s_config);

    // 4. Initialize DRPI (Dynamic Region Preservation Interceptor)
    RegionInterceptor::Instance().Initialize(s_config.photon);

    s_initialized = true;

    ReFixLog("[UNAE] Engine Ready: Active Route -> %s", TierToString(CascadeArbiter::Instance().GetActiveTier()));
    ReFixLog("====================================================================");
}

const GameCapabilities& GetCapabilities() {
    return CapabilityDetector::Instance().GetCapabilities();
}

NetworkTopology GetTopology() {
    return CapabilityDetector::Instance().GetCapabilities().topology;
}

ConnectivityTier GetActiveTier() {
    return CascadeArbiter::Instance().GetActiveTier();
}

bool IsDirectP2PAllowed() {
    return CascadeArbiter::Instance().IsDirectP2PAllowed();
}

const UNAEConfig& GetConfig() {
    return s_config;
}

} // namespace UNAE

extern "C" {
__declspec(dllexport) void UNAE_Initialize() {
    UNAE::Initialize();
}

__declspec(dllexport) int UNAE_GetTopology() {
    return static_cast<int>(UNAE::GetTopology());
}

__declspec(dllexport) int UNAE_GetActiveTier() {
    return static_cast<int>(UNAE::GetActiveTier());
}

__declspec(dllexport) bool UNAE_IsDirectP2PAllowed() {
    return UNAE::IsDirectP2PAllowed();
}
}
