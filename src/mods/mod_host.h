#pragma once

#include "mod_types.h"
#include "mod_manifest.h"
#include "mod_signature.h"
#include "mod_package.h"
#include "mod_trust_store.h"
#include "mod_scheduler.h"
#include "sandbox_wasm.h"
#include "native_host.h"
#include "patch_engine.h"
#include "privacy_gate.h"
#include "adapters/engine/engine_adapter.h"
#include "adapters/protocol/protocol_adapter.h"

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace refix::mods {

struct LoadedMod {
    ModManifest manifest;
    std::string package_path;
    std::string extracted_path;
    TrustLevel trust_level = TrustLevel::Untrusted;
    bool is_approved = false;
    bool is_active = false;
    bool is_faulted = false;
    std::string status_message;

    std::unique_ptr<WasmSandbox> wasm_instance;
    std::unique_ptr<NativeModInstance> native_instance;
};

class ModHost {
public:
    static ModHost& GetInstance();

    ModHost();
    ~ModHost();

    // Initialize the mod subsystem
    bool Initialize(const std::string& mods_root_dir = "", bool dev_mode = false);

    // Discover and load mods from disk
    void DiscoverAndLoad();

    // Load a single .rfmod package file
    bool LoadPackage(const std::string& rfmod_path, std::string& out_error);

    // Trigger lifecycle phase dispatch
    void OnPhase(ModPhase phase);

    // Fault handler (called if a mod traps or crashes)
    void OnModFault(const std::string& mod_id, const std::string& fault_reason);

    // Unload all mods and clean up hooks/patches
    void Shutdown();

    // Overlay UI query access
    const std::map<std::string, LoadedMod>& GetLoadedMods() const { return loaded_mods_; }
    bool IsDevMode() const { return dev_mode_; }
    bool IsEnabled() const { return is_enabled_; }
    void SetEnabled(bool enabled) { is_enabled_ = enabled; }

    // User manual approval action from overlay
    bool ApproveMod(const std::string& mod_id, bool approved);

    // Subsystem accessors
    PatchEngine& GetPatchEngine() { return patch_engine_; }
    PrivacyGate& GetPrivacyGate() { return privacy_gate_; }
    ModTrustStore& GetTrustStore() { return trust_store_; }
    ModScheduler& GetScheduler() { return scheduler_; }

private:
    bool is_initialized_ = false;
    bool is_enabled_ = true;
    bool dev_mode_ = false;
    std::string mods_root_dir_;

    PatchEngine patch_engine_;
    PrivacyGate privacy_gate_;
    ModTrustStore trust_store_;
    ModScheduler scheduler_;

    std::map<std::string, LoadedMod> loaded_mods_;

    std::string GetDefaultModsDirectory() const;
};

} // namespace refix::mods
