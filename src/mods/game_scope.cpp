#include "game_scope.h"

#include <windows.h>
#include <cstring>

namespace refix::mods {

namespace {

bool IsCsgo(uint32_t appId) { return appId == 730 || appId == 4465480; }

bool SameGame(uint32_t a, uint32_t b) { return a == b || (IsCsgo(a) && IsCsgo(b)); }

bool TargetMatches(const TargetGame& t, uint32_t appId, const std::string& exeName) {
    if (t.app_id == 0 && t.exe_name.empty()) return false;  // a wildcard target would load the mod everywhere
    if (!t.exe_name.empty() && _stricmp(t.exe_name.c_str(), exeName.c_str()) != 0) return false;
    if (t.app_id == 0) return true;
    if (IsKnownAppId(appId)) return SameGame(t.app_id, appId);
    return !t.exe_name.empty();  // unknown AppID: only the exe can vouch for the game
}

} // namespace

bool IsKnownAppId(uint32_t appId) { return appId != 0 && appId != 480; }

bool MatchesGame(const std::vector<TargetGame>& targets, uint32_t appId, const std::string& exeName) {
    for (const TargetGame& t : targets) {
        if (TargetMatches(t, appId, exeName)) return true;
    }
    return false;
}

std::string CurrentExeName() {
    char path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    const char* slash = strrchr(path, '\\');
    return slash ? slash + 1 : path;
}

} // namespace refix::mods
