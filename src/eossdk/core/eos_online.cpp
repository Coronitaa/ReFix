#include "eos_online.h"
#include "eos_platform.h"
#include "refix_config.h"
#include "refix_log.h"
#include "../net/lobby_directory.h"

namespace refix {

const std::string& OnlineScope() {
    static std::string scope = [] {
        // Prefer what the title itself declared; fall back to configuration and
        // finally to the executable name, so the scope is always defined.
        if (PlatformImpl* p = CurrentPlatform()) {
            if (!p->ProductId.empty()) return p->ProductId;
            if (!p->DeploymentId.empty()) return p->DeploymentId;
        }
        auto& cfg = Config::Get();
        std::string s = cfg.GetString("EOS", "ProductId", "");
        if (s.empty()) s = cfg.GetString("Steam", "RealAppId", "");
        if (s.empty()) s = cfg.GetString("Game", "GameName", "refix");
        return s;
    }();
    return scope;
}

bool EnsureOnline() {
    static bool started = false;
    static bool ok = false;
    if (started) return ok;
    started = true;
    ok = LobbyDirectory::Get().Start(OnlineScope());
    if (!ok) RFLOG(Net, "online layer unavailable - lobby and session calls will fail cleanly");
    return ok;
}

} // namespace refix
