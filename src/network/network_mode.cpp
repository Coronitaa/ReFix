#include "network_mode.h"
#include <windows.h>
#include <algorithm>

#include <tlhelp32.h>

namespace ReFix {

std::atomic<ReFixNetworkMode> NetworkModeManager::s_mode{ ReFixNetworkMode::Online };

bool NetworkModeManager::IsSteamClientRunning() {
    bool running = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, L"steam.exe") == 0) {
                    running = true;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return running;
}

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
    } else if (mode == "force_online") {
        s_mode = ReFixNetworkMode::Online;
        return;
    } else if (!mode.empty() && mode != "auto") {
        s_mode = ReFixNetworkMode::Online;
        return;
    }
    
    // Check legacy [Online] Mode
    GetPrivateProfileStringA("Online", "Mode", "", buf, sizeof(buf), iniPath.c_str());
    std::string onlineMode = buf;
    std::transform(onlineMode.begin(), onlineMode.end(), onlineMode.begin(), ::tolower);
    
    if (onlineMode == "goldberg" || onlineMode == "lan") {
        s_mode = ReFixNetworkMode::Lan;
        return;
    } else if (onlineMode == "offline") {
        s_mode = ReFixNetworkMode::Offline;
        return;
    } else if (onlineMode == "valve" && (mode != "auto" && !mode.empty())) {
        s_mode = ReFixNetworkMode::Online;
        return;
    }
    
    // Mode is "auto" or unspecified:
    // If Steam is running, attempt Online mode; otherwise autonomously enter LAN mode!
    if (IsSteamClientRunning()) {
        s_mode = ReFixNetworkMode::Online;
    } else {
        s_mode = ReFixNetworkMode::Lan;
    }
}

ReFixNetworkMode NetworkModeManager::GetMode() {
    return s_mode.load(std::memory_order_acquire);
}

void NetworkModeManager::SetMode(ReFixNetworkMode mode) {
    s_mode.store(mode, std::memory_order_release);
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
    return s_mode.load(std::memory_order_acquire) == ReFixNetworkMode::Online;
}

} // namespace ReFix
