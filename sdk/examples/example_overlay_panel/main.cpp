#include "../../bindings/cpp/refix_mod.hpp"

// Callback triggered when rendering custom overlay tab in ImGui
extern "C" __attribute__((visibility("default"))) void render_custom_panel() {
    // In Sandboxed WASM, high-level declarative widgets or events update state
    refix::Mod::LogInfo("Rendering custom mod settings in overlay");
}

extern "C" __attribute__((visibility("default"))) int refix_init() {
    refix::Mod::LogInfo("Custom overlay panel registered");
    return 0;
}
