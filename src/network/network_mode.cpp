#include "network_mode.h"
#include <windows.h>
#include <algorithm>

namespace ReFix {

ReFixNetworkMode NetworkModeManager::s_mode = ReFixNetworkMode::Online;

void NetworkModeManager::LoadMode(const std::string& iniPath) {
    char buf[64] = {0};
    
    // Modern UNAE config (highest priority)
    GetPrivateProfileStringA("Network", "Mode", "", buf, sizeof(buf), iniPath.c_str());
    std::string mode = buf;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
    
    if (mode == "force_lan") {
        s_mode = ReFixNetworkMode::Lan;
        return;
    } else if (mode == "offline") {
        s_mode = ReFixNetworkMode::Offline;
        return;
    } else if (!mode.empty() && mode != "auto") {
        s_mode = ReFixNetworkMode::Online;
        return;
    }
    
    // Legacy fallback to [Online] Mode
    GetPrivateProfileStringA("Online", "Mode", "valve", buf, sizeof(buf), iniPath.c_str());
    mode = buf;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
    
    if (mode == "goldberg" || mode == "lan") {
        s_mode = ReFixNetworkMode::Lan;
    } else if (mode == "offline") {
        s_mode = ReFixNetworkMode::Offline;
    } else {
        s_mode = ReFixNetworkMode::Online;
    }
}

ReFixNetworkMode NetworkModeManager::GetMode() {
    return s_mode;
}

void NetworkModeManager::SetMode(ReFixNetworkMode mode) {
    s_mode = mode;
}

bool NetworkModeManager::IsOnline() {
    return s_mode == ReFixNetworkMode::Online;
}

bool NetworkModeManager::IsLanOnly() {
    return s_mode == ReFixNetworkMode::Lan;
}

bool NetworkModeManager::IsOffline() {
    return s_mode == ReFixNetworkMode::Offline;
}

bool NetworkModeManager::IsGoldbergBackendActive() {
    // Currently, both Lan and Offline modes use the Goldberg emulator backend
    return s_mode == ReFixNetworkMode::Lan || s_mode == ReFixNetworkMode::Offline;
}

bool NetworkModeManager::IsExternalNetworkingAllowed() {
    return s_mode == ReFixNetworkMode::Online;
}

} // namespace ReFix
