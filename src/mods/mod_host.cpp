#include "mod_host.h"
#include "../include/refix_services.h"
#include "../unreal_steam_emu.h"

#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <iostream>

namespace stdfs = std::filesystem;

namespace refix::mods {

ModHost& ModHost::GetInstance() {
    static ModHost s_instance;
    return s_instance;
}

ModHost::ModHost() {
}

ModHost::~ModHost() {
    Shutdown();
}

std::string ModHost::GetDefaultModsDirectory() const {
    char path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
        stdfs::path p(path);
        stdfs::path mod_dir = p.parent_path() / "mods";
        if (stdfs::exists(mod_dir)) return mod_dir.string();
        stdfs::path refix_mod_dir = p.parent_path() / "refix_mods";
        if (stdfs::exists(refix_mod_dir)) return refix_mod_dir.string();
        return mod_dir.string();
    }
    return "mods";
}

bool ModHost::Initialize(const std::string& mods_root_dir, bool dev_mode) {
    if (is_initialized_) return true;

    dev_mode_ = dev_mode;
    if (!mods_root_dir.empty()) {
        mods_root_dir_ = mods_root_dir;
    } else {
        mods_root_dir_ = GetDefaultModsDirectory();
    }

    // Initialize MinHook patch engine
    patch_engine_.Initialize();

    // Initialize session salts for privacy gate
    privacy_gate_.InitializeSession();

    // Load Trust Store
    std::string err;
    if (!trust_store_.Load(err)) {
        if (dev_mode_) {
            std::cerr << "[ModHost] Warning loading trust store: " << err << std::endl;
        }
    }

    is_initialized_ = true;
    if (dev_mode_) {
        std::cout << "[ModHost] Subsystem initialized (DevMode=1, Dir=" << mods_root_dir_ << ")" << std::endl;
    }
    return true;
}

bool ModHost::LoadPackage(const std::string& rfmod_path, std::string& out_error) {
    PackageInspectInfo inspect_info;
    std::string inspect_err;

    if (!ModPackage::InspectPackage(rfmod_path, nullptr, &GetGlobalRevocationList(), inspect_info, inspect_err)) {
        if (!dev_mode_) {
            out_error = "Package inspect failed: " + inspect_err;
            return false;
        }
    }

    LoadedMod loaded;
    loaded.manifest = inspect_info.manifest;
    loaded.package_path = rfmod_path;
    loaded.trust_level = inspect_info.trust_level;
    loaded.is_active = false;
    loaded.is_faulted = false;

    // Validate target AppID compatibility (treating CS:GO classic 730 and standalone legacy 4465480 as equivalent).
    if (!loaded.manifest.target_games.empty()) {
        const uint32_t curApp = UnrealSteamEmu::GetAppID();
        if (curApp != 0 && curApp != 480) {
            bool appMatch = false;
            const bool curIsCsgo = (curApp == 730 || curApp == 4465480);
            for (const auto& tg : loaded.manifest.target_games) {
                const uint32_t targetId = tg.app_id;
                if (targetId == 0 || targetId == curApp || (curIsCsgo && (targetId == 730 || targetId == 4465480))) {
                    appMatch = true;
                    break;
                }
            }
            if (!appMatch) {
                out_error = "Mod target_games does not match current AppID " + std::to_string(curApp);
                return false;
            }
        }
    }

    // Check revocation
    if (loaded.trust_level == TrustLevel::Revoked) {
        out_error = "Mod package or author key is REVOKED";
        loaded.status_message = out_error;
        loaded_mods_[loaded.manifest.id] = std::move(loaded);
        return false;
    }

    // Check signature policy
    if (!inspect_info.signature_valid && !dev_mode_) {
        out_error = "Unsigned or invalidly signed mod rejected (DevMode=0)";
        loaded.status_message = out_error;
        loaded_mods_[loaded.manifest.id] = std::move(loaded);
        return false;
    }

    // Check trust level and user approval
    std::vector<std::string> new_caps;
    if (loaded.trust_level == TrustLevel::Official) {
        loaded.is_approved = true;
        loaded.status_message = "Official ReFix Mod (Trusted)";
    } else if (dev_mode_) {
        loaded.is_approved = true;
        loaded.status_message = "Loaded in Developer Mode";
    } else {
        bool needs_approval = trust_store_.NeedsUserApproval(loaded.manifest, new_caps);
        if (needs_approval) {
            loaded.is_approved = false;
            loaded.status_message = "Requires user approval in Overlay UI";
            loaded_mods_[loaded.manifest.id] = std::move(loaded);
            std::cout << "[ModHost] Mod " << inspect_info.manifest.id << " requires manual approval" << std::endl;
            return true; // Staged for UI approval
        } else {
            loaded.is_approved = true;
            loaded.status_message = "Approved by user";
        }
    }

    // If approved, extract to safe temp cache
    stdfs::path extract_dir = stdfs::temp_directory_path() / "refix" / "mods" / loaded.manifest.id;
    loaded.extracted_path = extract_dir.string();

    std::string extract_err;
    if (!ModPackage::VerifyAndExtract(rfmod_path, loaded.extracted_path, nullptr, &GetGlobalRevocationList(), inspect_info, extract_err)) {
        out_error = "Extraction and hash verification failed: " + extract_err;
        loaded.status_message = out_error;
        loaded_mods_[loaded.manifest.id] = std::move(loaded);
        return false;
    }

    // Instantiate execution runtime based on tier
    if (loaded.manifest.execution_tier == ExecutionTier::Sandboxed) {
        std::string wasm_file = (extract_dir / loaded.manifest.entrypoint).string();
        std::vector<uint8_t> bytecode;
        if (!ModPackage::ReadDiskFile(wasm_file, bytecode)) {
            out_error = "Failed to read WASM entrypoint: " + wasm_file;
            return false;
        }

        auto fault_cb = [this](const std::string& mod_id, const std::string& reason) {
            this->OnModFault(mod_id, reason);
        };

        loaded.wasm_instance = std::make_unique<WasmSandbox>(loaded.manifest, fault_cb, &patch_engine_, &privacy_gate_);
        if (!loaded.wasm_instance->LoadBytecode(bytecode, out_error)) {
            return false;
        }
        loaded.is_active = true;
    } else if (loaded.manifest.execution_tier == ExecutionTier::Native) {
        std::string dll_file = (extract_dir / loaded.manifest.entrypoint).string();
        loaded.native_instance = std::make_unique<NativeModInstance>(loaded.manifest, dll_file);
        if (!loaded.native_instance->Load(&refix::GetRefixServices(), out_error)) {
            return false;
        }
        loaded.is_active = true;
    }

    // Register with scheduler
    std::string sched_err;
    scheduler_.RegisterMod(loaded.manifest, sched_err);

    loaded_mods_[loaded.manifest.id] = std::move(loaded);
    return true;
}

void ModHost::DiscoverAndLoad() {
    if (!is_initialized_) return;
    if (!stdfs::exists(mods_root_dir_)) return;

    std::error_code ec;
    for (const auto& entry : stdfs::directory_iterator(mods_root_dir_, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".rfmod") {
            std::string err;
            if (!LoadPackage(entry.path().string(), err)) {
                std::cerr << "[ModHost] Failed to load mod: " << entry.path().filename().string() << " (" << err << ")" << std::endl;
            }
        }
    }
}

bool ModHost::ApproveMod(const std::string& mod_id, bool approved) {
    auto it = loaded_mods_.find(mod_id);
    if (it == loaded_mods_.end()) return false;

    trust_store_.RecordDecision(it->second.manifest, approved);
    std::string err;
    trust_store_.Save(err);

    if (approved) {
        // Re-attempt loading
        std::string load_err;
        return LoadPackage(it->second.package_path, load_err);
    } else {
        it->second.is_approved = false;
        it->second.status_message = "Rejected by user";
        return true;
    }
}

void ModHost::OnPhase(ModPhase phase) {
    if (!is_enabled_ || !is_initialized_) return;

    if (phase == ModPhase::EngineReady) {
        EngineAdapterManager::GetInstance().DetectAndInitialize();
    }

    std::string err;
    if (!scheduler_.Resolve(err)) {
        std::cerr << "[ModHost] Dependency resolution failed in phase " << ModPhaseToString(phase) << ": " << err << std::endl;
        return;
    }

    const auto& order = scheduler_.GetExecutionOrder(phase);
    for (const auto& mod_id : order) {
        auto it = loaded_mods_.find(mod_id);
        if (it != loaded_mods_.end() && it->second.is_active && !it->second.is_faulted) {
            if (it->second.wasm_instance) {
                std::string call_err;
                it->second.wasm_instance->CallExport("refix_on_phase", call_err);
            } else if (it->second.native_instance) {
                it->second.native_instance->DispatchEvent(static_cast<int>(phase), nullptr);
            }
        }
    }
}

void ModHost::OnModFault(const std::string& mod_id, const std::string& fault_reason) {
    std::cerr << "[ModHost] Fault in mod '" << mod_id << "': " << fault_reason << ". Performing automatic rollback." << std::endl;

    // 1. Rollback patches
    patch_engine_.RollbackModPatches(mod_id);

    // 2. Remove MinHook hooks
    patch_engine_.RemoveModHooks(mod_id);

    // 3. Mark mod as faulted and inactive
    auto it = loaded_mods_.find(mod_id);
    if (it != loaded_mods_.end()) {
        it->second.is_faulted = true;
        it->second.is_active = false;
        it->second.status_message = "Faulted: " + fault_reason;
    }
}

void ModHost::Shutdown() {
    if (!is_initialized_) return;

    OnPhase(ModPhase::Shutdown);

    patch_engine_.RollbackAll();
    patch_engine_.RemoveAllHooks();
    EngineAdapterManager::GetInstance().Shutdown();

    loaded_mods_.clear();
    scheduler_.Clear();
    is_initialized_ = false;
    std::cout << "[ModHost] Shutdown complete." << std::endl;
}

} // namespace refix::mods
