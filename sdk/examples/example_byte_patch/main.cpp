#include "../../bindings/cpp/refix_mod.hpp"

// Sandboxed WASM entrypoint called by ReFix during GameInit phase
extern "C" __attribute__((visibility("default"))) int refix_init() {
    refix::Mod::LogInfo("Example Byte Patch Mod initializing...");

    // Replace a NOP or check instruction in GameAssembly.dll at RVA 0x1234
    // Expected original instruction: 75 08 (JNZ +8)
    // Replacement instruction: 90 90 (NOP NOP)
    std::vector<uint8_t> expected = { 0x75, 0x08 };
    std::vector<uint8_t> replacement = { 0x90, 0x90 };

    bool ok = refix::Mod::ApplyPatch("GameAssembly.dll", 0x1234, expected, replacement);
    if (ok) {
        refix::Mod::LogInfo("Successfully applied declarative byte patch!");
    } else {
        refix::Mod::LogError("Failed to apply declarative patch (version mismatch or module protected)");
    }

    return 0;
}
