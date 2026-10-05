#include "network_policy.h"
#include <windows.h>
#include <algorithm>

namespace ReFix {

NetworkPolicy NetworkPolicyManager::s_policy = NetworkPolicy::Online;

void NetworkPolicyManager::LoadPolicy(const std::string& iniPath) {
    char buf[64] = {0};
    GetPrivateProfileStringA("Network", "Mode", "auto", buf, sizeof(buf), iniPath.c_str());
    
    std::string mode = buf;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
    
    if (mode == "force_lan") {
        s_policy = NetworkPolicy::LanOnly;
    } else if (mode == "offline") {
        s_policy = NetworkPolicy::Offline;
    } else {
        s_policy = NetworkPolicy::Online;
    }
}

NetworkPolicy NetworkPolicyManager::GetPolicy() {
    return s_policy;
}

bool NetworkPolicyManager::IsInternetAllowed() {
    return s_policy == NetworkPolicy::Online;
}

bool NetworkPolicyManager::IsLanAllowed() {
    return s_policy != NetworkPolicy::Offline;
}

} // namespace ReFix
