#include "native_host.h"
#include "mod_signature.h"
#include "seh_guard.h"
#include "../overlay/game_actions/mod_panels.h"

#include <iostream>

extern void ReFixLog(const char* fmt, ...);

namespace refix::mods {

NativeModInstance::NativeModInstance(const ModManifest& manifest, const std::string& dll_path)
    : manifest_(manifest), dll_path_(dll_path) {
}

NativeModInstance::~NativeModInstance() {
    Unload();
}

bool NativeModInstance::Load(const void* host_api, std::string& out_error) {
    if (manifest_.execution_tier != ExecutionTier::Native) {
        out_error = "Manifest execution tier is not Native";
        return false;
    }

    // Strict security check: Native mods MUST be signed by ReFix Official Root/Release key!
    uint8_t root_pubkey[32];
    memcpy(root_pubkey, g_RefixRootPublicKey, 32);
    std::string root_pubkey_hex = ToHex(root_pubkey, 32);

    if (_stricmp(manifest_.author_pubkey.c_str(), root_pubkey_hex.c_str()) != 0) {
        out_error = "Security Policy Violation: Native DLL execution is strictly restricted to ReFix Official signed binaries. Community mods must use the Sandboxed (WASM) tier.";
        return false;
    }

    // Verify file integrity
    std::string actual_hash = ComputeFileSha256(dll_path_);
    auto it = manifest_.files.find(manifest_.entrypoint);
    if (it != manifest_.files.end()) {
        if (_stricmp(actual_hash.c_str(), it->second.c_str()) != 0) {
            out_error = "Native DLL hash mismatch on disk: " + dll_path_;
            return false;
        }
    }

    // Load library safely
    hModule_ = LoadLibraryExA(dll_path_.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hModule_) {
        DWORD err = GetLastError();
        out_error = "LoadLibraryEx failed for '" + dll_path_ + "' with Win32 error " + std::to_string(err);
        return false;
    }

    // Resolve exports
    init_fn_ = reinterpret_cast<RefixModInitFn>(GetProcAddress(hModule_, "refix_mod_init"));
    shutdown_fn_ = reinterpret_cast<RefixModShutdownFn>(GetProcAddress(hModule_, "refix_mod_shutdown"));
    on_event_fn_ = reinterpret_cast<RefixModOnEventFn>(GetProcAddress(hModule_, "refix_mod_on_event"));

    if (init_fn_) {
        int res = -1;
        unsigned long code = 0;
        if (!SehGuard([&] { res = init_fn_(host_api); }, &code)) {
            Unload();
            out_error = "refix_mod_init crashed (exception " + std::to_string(code) + ")";
            return false;
        }
        if (res != 0) {
            Unload();
            out_error = "refix_mod_init returned error code " + std::to_string(res);
            return false;
        }
    }

    // Overlay panels: only with the capability, and the manifest's target_games were already matched.
    auto register_overlay = reinterpret_cast<refix::overlay::mods::RegisterFn>(GetProcAddress(hModule_, "refix_mod_register_overlay"));
    if (register_overlay && manifest_.capabilities.count(Capability::OverlayPanel)) {
        refix::overlay::mods::Registry::Get().RegisterModule(manifest_.id, register_overlay);
    }

    ReFixLog("[NativeHost] Successfully loaded official native mod: %s", manifest_.id.c_str());
    std::cout << "[NativeHost] Successfully loaded official native mod: " << manifest_.id << std::endl;
    return true;
}

void NativeModInstance::Unload() {
    if (hModule_) {
        refix::overlay::mods::Registry::Get().RemoveOwner(manifest_.id);  // no panel may point into the DLL
        if (shutdown_fn_) {
            if (!SehGuard([&] { shutdown_fn_(); }, nullptr)) ReFixLog("[NativeHost] %s: shutdown crashed", manifest_.id.c_str());
            shutdown_fn_ = nullptr;
        }
        init_fn_ = nullptr;
        on_event_fn_ = nullptr;
        FreeLibrary(hModule_);
        hModule_ = NULL;
    }
}

void NativeModInstance::DispatchEvent(int event_id, const void* event_data) {
    if (!on_event_fn_) return;
    unsigned long code = 0;
    if (!SehGuard([&] { on_event_fn_(event_id, event_data); }, &code)) {
        ReFixLog("[NativeHost] %s: event %d crashed (exception 0x%08lX), events turned off", manifest_.id.c_str(), event_id, code);
        on_event_fn_ = nullptr;  // fail closed
    }
}

} // namespace refix::mods
