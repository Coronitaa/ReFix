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
    
    // Convenience checkers
    static bool IsOnline();
    static bool IsLanOnly();
    static bool IsOffline();
    
private:
    static ReFixNetworkMode s_mode;
};

} // namespace ReFix
