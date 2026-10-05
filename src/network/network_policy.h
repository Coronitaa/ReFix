#pragma once

#include <string>

namespace ReFix {

enum class NetworkPolicy {
    Online,
    LanOnly,
    Offline
};

class NetworkPolicyManager {
public:
    static void LoadPolicy(const std::string& iniPath);
    static NetworkPolicy GetPolicy();
    static bool IsInternetAllowed();
    static bool IsLanAllowed();
    
private:
    static NetworkPolicy s_policy;
};

} // namespace ReFix
