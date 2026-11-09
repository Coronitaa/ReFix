#include "network_mode.h"
#include <windows.h>
#include <algorithm>

#include <tlhelp32.h>

namespace ReFix {

#ifdef REFIX_LAN_EDITION
// ReFix LAN is a standalone product: it never forwards to Valve's DLL nor talks to Steam.
// Online mode cannot be selected from ReFix.ini, the environment or a test hook.
static ReFixNetworkMode ClampMode(ReFixNetworkMode mode) {
    return mode == ReFixNetworkMode::Online ? ReFixNetworkMode::Lan : mode;
}
std::atomic<ReFixNetworkMode> NetworkModeManager::s_mode{ ReFixNetworkMode::Lan };
#else
static ReFixNetworkMode ClampMode(ReFixNetworkMode mode) { return mode; }
std::atomic<ReFixNetworkMode> NetworkModeManager::s_mode{ ReFixNetworkMode::Online };
#endif

std::atomic<bool> NetworkModeManager::s_lanInternet{ true };

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
#ifdef REFIX_LAN_EDITION
    char lanBuf[64] = {0};
    GetPrivateProfileStringA("Network", "Mode", "", lanBuf, sizeof(lanBuf), iniPath.c_str());
    bool offline = (_stricmp(lanBuf, "offline") == 0);
    GetPrivateProfileStringA("Online", "Mode", "", lanBuf, sizeof(lanBuf), iniPath.c_str());
    offline = offline || (_stricmp(lanBuf, "offline") == 0);
    s_mode = offline ? ReFixNetworkMode::Offline : ReFixNetworkMode::Lan;
    GetPrivateProfileStringA("Network", "InternetAccess", "allow", lanBuf, sizeof(lanBuf), iniPath.c_str());
    s_lanInternet = _stricmp(lanBuf, "block") != 0;
    return;
#endif
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
    s_mode.store(ClampMode(mode), std::memory_order_release);
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

bool NetworkModeManager::IsInternetEgressAllowed() {
    switch (s_mode.load(std::memory_order_acquire)) {
    case ReFixNetworkMode::Online: return true;
    case ReFixNetworkMode::Lan: return s_lanInternet.load(std::memory_order_acquire);
    default: return false;
    }
}

bool NetworkModeManager::IsExternalNetworkingAllowed() {
    return s_mode.load(std::memory_order_acquire) == ReFixNetworkMode::Online;
}

} // namespace ReFix
