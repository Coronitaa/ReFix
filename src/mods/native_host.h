#pragma once

#include "mod_types.h"
#include <string>
#include <windows.h>

namespace refix::mods {

// Function pointer signatures for native ReFix Mod ABI
typedef int (*RefixModInitFn)(const void* host_api);
typedef void (*RefixModShutdownFn)();
typedef void (*RefixModOnEventFn)(int event_id, const void* event_data);

class NativeModInstance {
public:
    NativeModInstance(const ModManifest& manifest, const std::string& dll_path);
    ~NativeModInstance();

    // Verify official signature and load DLL
    bool Load(const void* host_api, std::string& out_error);

    // Call mod shutdown and unload DLL
    void Unload();

    // Trigger event callback
    void DispatchEvent(int event_id, const void* event_data);

    bool IsLoaded() const { return hModule_ != NULL; }
    const ModManifest& GetManifest() const { return manifest_; }

private:
    ModManifest manifest_;
    std::string dll_path_;
    HMODULE hModule_ = NULL;

    RefixModInitFn init_fn_ = nullptr;
    RefixModShutdownFn shutdown_fn_ = nullptr;
    RefixModOnEventFn on_event_fn_ = nullptr;
};

} // namespace refix::mods
