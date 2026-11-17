// Overlay modules compiled into ReFix. Each one lists the games it belongs to; the registry registers it
// only when the running game matches, exactly like a mod package's target_games.
#include "mod_panels.h"
#include "../csgo/csgo_overlay.h"

namespace refix::overlay::mods {

const std::vector<BuiltinModule>& BuiltinModules() {
    static const std::vector<BuiltinModule> s_modules = {
        {"refix.csgo.overlay", {{730, "csgo.exe", "Source"}, {4465480, "csgo.exe", "Source"}}, csgo_ui::RegisterOverlay},
    };
    return s_modules;
}

} // namespace refix::overlay::mods
