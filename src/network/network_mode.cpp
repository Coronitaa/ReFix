#include "network_mode.h"
#include <windows.h>
#include <algorithm>

namespace ReFix {

std::atomic<ReFixNetworkMode> NetworkModeManager::s_mode{ ReFixNetworkMode::Online };
std::atomic<bool> NetworkModeManager::s_externalNetworkingAllowed{ true };

void NetworkModeManager::LoadMode(const std::string& iniPath) {
    char buf[64] = {0};
    
    // Modern UNAE config (highest priority)
    GetPrivateProfileStringA("Network", "Mode", "", buf, sizeof(buf), iniPath.c_str());
    std::string networkMode = buf;
    std::transform(networkMode.begin(), networkMode.end(), networkMode.begin(), ::tolower);
    
    // Legacy fallback to [Online] Mode
    GetPrivateProfileStringA("Online", "Mode", "valve", buf, sizeof(buf), iniPath.c_str());
    std::string onlineMode = buf;
    std::transform(onlineMode.begin(), onlineMode.end(), onlineMode.begin(), ::tolower);
    
    // 1. Strict isolation modes:
    if (networkMode == "force_lan") {
        s_mode.store(ReFixNetworkMode::Lan, std::memory_order_release);
        s_externalNetworkingAllowed.store(false, std::memory_order_release);
        return;
    } else if (networkMode == "offline" || onlineMode == "offline") {
        s_mode.store(ReFixNetworkMode::Offline, std::memory_order_release);
        s_externalNetworkingAllowed.store(false, std::memory_order_release);
        return;
    }
    
    // 2. Steam emulator backend resolution:
    if (onlineMode == "goldberg" || onlineMode == "lan") {
        s_mode.store(ReFixNetworkMode::Lan, std::memory_order_release);
    } else {
        s_mode.store(ReFixNetworkMode::Online, std::memory_order_release);
    }

    // 3. Outbound WAN egress policy:
    // In "auto", "force_relay", "force_p2p" (or unspecified), external networking
    // is allowed so cloud relays (Photon, EOS) can establish sessions.
    // Only "force_lan" and "offline" enforce total Internet-Zero egress blocking.
    bool allowExternal = true;
    if (networkMode == "force_lan" || networkMode == "offline") {
        allowExternal = false;
    } else {
        allowExternal = true;
    }
    s_externalNetworkingAllowed.store(allowExternal, std::memory_order_release);
}

ReFixNetworkMode NetworkModeManager::GetMode() {
    return s_mode.load(std::memory_order_acquire);
}

void NetworkModeManager::SetMode(ReFixNetworkMode mode) {
    s_mode.store(mode, std::memory_order_release);
    if (mode == ReFixNetworkMode::Online) {
        s_externalNetworkingAllowed.store(true, std::memory_order_release);
    } else {
        s_externalNetworkingAllowed.store(false, std::memory_order_release);
    }
}

bool NetworkModeManager::IsOnline() {
    return s_mode.load(std::memory_order_acquire) == ReFixNetworkMode::Online;
}

bool NetworkModeManager::IsLanOnly() {
    return s_mode.load(std::memory_order_acquire) == ReFixNetworkMode::Lan;
}

bool NetworkModeManager::IsOffline() {
    return s_mode.load(std::memory_order_acquire) == ReFixNetworkMode::Offline;
}

bool NetworkModeManager::IsGoldbergBackendActive() {
    auto m = s_mode.load(std::memory_order_acquire);
    return m == ReFixNetworkMode::Lan || m == ReFixNetworkMode::Offline;
}

bool NetworkModeManager::IsExternalNetworkingAllowed() {
    return s_externalNetworkingAllowed.load(std::memory_order_acquire);
}

} // namespace ReFix
