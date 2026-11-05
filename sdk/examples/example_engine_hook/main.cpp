#include "../../bindings/cpp/refix_mod.hpp"

// Callback invoked by ReFix mediated MinHook trampoline when game function is hit
extern "C" __attribute__((visibility("default"))) int on_engine_function_hook(int param1, int param2) {
    refix::Mod::LogInfo("Mediated engine hook triggered from game thread!");
    // Return modified result
    return param1 + param2;
}

extern "C" __attribute__((visibility("default"))) int refix_init() {
    refix::Mod::LogInfo("Registering mediated hook on engine function...");
    return 0;
}
