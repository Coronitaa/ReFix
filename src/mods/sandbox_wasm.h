#pragma once

#include "mod_types.h"
#include "wasm3/wasm3.h"

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>

namespace refix::mods {

class PatchEngine;
class PrivacyGate;

// Callback type for rolling back patches/hooks when a sandbox traps
using SandboxFaultCallback = std::function<void(const std::string& mod_id, const std::string& fault_reason)>;

class WasmSandbox {
public:
    WasmSandbox(const ModManifest& manifest,
                SandboxFaultCallback on_fault,
                PatchEngine* patch_engine = nullptr,
                PrivacyGate* privacy_gate = nullptr);
    ~WasmSandbox();

    // Load and compile bytecode into isolated runtime
    bool LoadBytecode(const std::vector<uint8_t>& wasm_bytecode, std::string& out_error);

    // Call an exported WASM function with watchdog protection
    bool CallExport(const std::string& export_name, std::string& out_error);
    bool CallExportArgs(const std::string& export_name,
                        int argc,
                        const void* argv[],
                        std::string& out_error);

    // Query status
    bool IsFaulted() const { return is_faulted_; }
    const std::string& GetFaultReason() const { return fault_reason_; }
    const ModManifest& GetManifest() const { return manifest_; }

    // Direct memory access within WASM linear memory bounds
    bool ReadMemory(uint32_t wasm_offset, void* out_buffer, size_t size);
    bool WriteMemory(uint32_t wasm_offset, const void* in_buffer, size_t size);
    uint32_t GetLinearMemorySize() const;

    // Trigger explicit fault/trap and cleanup
    void TriggerFault(const std::string& reason);

    // Internal host call dispatch helpers (used by raw wasm3 wrappers)
    bool CheckCapability(const std::string& cap);
    void HostLog(int level, const char* msg);
    int HostStorageRead(const char* key, void* out_buf, size_t max_len);
    int HostStorageWrite(const char* key, const void* val, size_t len);

private:
    ModManifest manifest_;
    SandboxFaultCallback fault_callback_;
    PatchEngine* patch_engine_ = nullptr;
    PrivacyGate* privacy_gate_ = nullptr;

    IM3Environment env_ = nullptr;
    IM3Runtime runtime_ = nullptr;
    IM3Module module_ = nullptr;

    bool is_faulted_ = false;
    std::string fault_reason_;
    std::vector<uint8_t> wasm_copy_; // Retain copy for wasm3 lifetime

    // Persistent mod storage cache (isolated per mod)
    std::map<std::string, std::vector<uint8_t>> storage_;

    void LinkHostFunctions();
};

} // namespace refix::mods
