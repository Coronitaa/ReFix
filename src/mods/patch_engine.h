#pragma once

#include "mod_types.h"
#include <string>
#include <vector>
#include <map>
#include <windows.h>

namespace refix::mods {

struct DeclarativePatch {
    std::string mod_id;
    std::string target_module;
    uint32_t rva = 0;
    std::vector<uint8_t> expected_original_bytes;
    std::vector<uint8_t> replacement_bytes;
    void* applied_address = nullptr;
    std::vector<uint8_t> backed_up_bytes;
    bool is_applied = false;
};

struct MediatedHook {
    std::string mod_id;
    std::string target_module;
    uint32_t rva = 0;
    void* target_func = nullptr;
    void* original_trampoline = nullptr;
    std::string wasm_callback_name;
    bool is_enabled = false;
};

class PatchEngine {
public:
    PatchEngine();
    ~PatchEngine();

    // Initialize MinHook subsystem
    bool Initialize();

    // Declarative byte patch
    bool ApplyPatch(const std::string& mod_id,
                    const std::string& target_module,
                    uint32_t rva,
                    const std::vector<uint8_t>& expected_orig,
                    const std::vector<uint8_t>& replacement,
                    std::string& out_error);

    // Roll back all patches for a specific mod
    void RollbackModPatches(const std::string& mod_id);

    // Roll back all patches across all mods
    void RollbackAll();

    // Mediated MinHook registration
    bool RegisterHook(const std::string& mod_id,
                      const std::string& target_module,
                      uint32_t rva,
                      void* detour_func,
                      void** out_orig_trampoline,
                      std::string& out_error);

    // Remove all hooks installed by a mod
    void RemoveModHooks(const std::string& mod_id);

    // Remove all hooks
    void RemoveAllHooks();

    // Guarded memory write (requires memory.write capability)
    bool GuardedMemoryWrite(const std::string& mod_id,
                           void* target_address,
                           const void* data,
                           size_t size,
                           std::string& out_error);

    // Validation: checks whether module is a forbidden system or ReFix DLL
    static bool IsForbiddenModule(const std::string& module_name);

    // Check if target pointer lies inside code/IAT/EAT/system space
    static bool IsProtectedMemoryRange(void* address, size_t size);

private:
    bool minhook_initialized_ = false;
    std::vector<DeclarativePatch> patches_;
    std::vector<MediatedHook> hooks_;

    static HMODULE ResolveTargetModule(const std::string& module_name);
};

} // namespace refix::mods
