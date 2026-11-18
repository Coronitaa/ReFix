// Overlay modules compiled into ReFix. Each one names the feature that turns it on; the registry registers it
// only when the running game's mod lists that feature in its signed manifest.
#include "mod_panels.h"
#include "../csgo/csgo_overlay.h"

namespace refix::overlay::mods {

const std::vector<BuiltinModule>& BuiltinModules() {
    static const std::vector<BuiltinModule> s_modules = {
        {"refix.csgo.overlay", "csgo.overlay", csgo_ui::RegisterOverlay},
    };
    return s_modules;
}

} // namespace refix::overlay::mods
