#pragma once

#include <string>

namespace ReFix {

enum class ReFixNetworkMode {
    Online,
    Lan,
    Offline
};

class NetworkModeManager {
public:
    static void LoadMode(const std::string& iniPath);
    static ReFixNetworkMode GetMode();
    static void SetMode(ReFixNetworkMode mode);
    
    // Convenience checkers
    static bool IsOnline();
    static bool IsLanOnly();
    static bool IsOffline();
    
    // Semantic queries
    static bool IsGoldbergBackendActive();
    static bool IsExternalNetworkingAllowed();
    
private:
    static ReFixNetworkMode s_mode;
};

} // namespace ReFix
