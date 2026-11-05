#include "sandbox_wasm.h"

#include <windows.h>
#include <iostream>
#include <chrono>

namespace refix::mods {

// -----------------------------------------------------------------------------
// Host API Functions exposed to WASM modules
// -----------------------------------------------------------------------------

// host: refix_log(i32 level, i32 msg_offset, i32 msg_len) -> void
m3ApiRawFunction(Host_refix_log) {
    m3ApiGetArg(int32_t, level);
    m3ApiGetArg(uint32_t, msg_offset);
    m3ApiGetArg(uint32_t, msg_len);

    auto* sandbox = static_cast<WasmSandbox*>(_ctx->userdata);
    if (!sandbox) m3ApiTrap("Missing sandbox context");

    if (!sandbox->CheckCapability(Capability::Log)) {
        m3ApiTrap("Missing capability: log");
    }

    m3ApiCheckMem(m3ApiOffsetToPtr(msg_offset), msg_len);
    const char* str = static_cast<const char*>(m3ApiOffsetToPtr(msg_offset));
    std::string msg(str, msg_len);
    sandbox->HostLog(level, msg.c_str());

    m3ApiSuccess();
}

// host: refix_get_time_ms() -> i64
m3ApiRawFunction(Host_refix_get_time_ms) {
    m3ApiReturnType(int64_t);

    uint64_t ms = GetTickCount64();
    m3ApiReturn(static_cast<int64_t>(ms));
}

// host: refix_storage_read(i32 key_offset, i32 key_len, i32 out_offset, i32 max_len) -> i32
m3ApiRawFunction(Host_refix_storage_read) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, key_offset);
    m3ApiGetArg(uint32_t, key_len);
    m3ApiGetArg(uint32_t, out_offset);
    m3ApiGetArg(uint32_t, max_len);

    auto* sandbox = static_cast<WasmSandbox*>(_ctx->userdata);
    if (!sandbox) m3ApiTrap("Missing sandbox context");

    if (!sandbox->CheckCapability(Capability::Storage)) {
        m3ApiTrap("Missing capability: storage");
    }

    m3ApiCheckMem(m3ApiOffsetToPtr(key_offset), key_len);
    m3ApiCheckMem(m3ApiOffsetToPtr(out_offset), max_len);

    const char* key_str = static_cast<const char*>(m3ApiOffsetToPtr(key_offset));
    std::string key(key_str, key_len);
    void* out_buf = m3ApiOffsetToPtr(out_offset);

    int read_bytes = sandbox->HostStorageRead(key.c_str(), out_buf, max_len);
    m3ApiReturn(read_bytes);
}

// host: refix_storage_write(i32 key_offset, i32 key_len, i32 val_offset, i32 val_len) -> i32
m3ApiRawFunction(Host_refix_storage_write) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, key_offset);
    m3ApiGetArg(uint32_t, key_len);
    m3ApiGetArg(uint32_t, val_offset);
    m3ApiGetArg(uint32_t, val_len);

    auto* sandbox = static_cast<WasmSandbox*>(_ctx->userdata);
    if (!sandbox) m3ApiTrap("Missing sandbox context");

    if (!sandbox->CheckCapability(Capability::Storage)) {
        m3ApiTrap("Missing capability: storage");
    }

    m3ApiCheckMem(m3ApiOffsetToPtr(key_offset), key_len);
    m3ApiCheckMem(m3ApiOffsetToPtr(val_offset), val_len);

    const char* key_str = static_cast<const char*>(m3ApiOffsetToPtr(key_offset));
    std::string key(key_str, key_len);
    const void* val_buf = m3ApiOffsetToPtr(val_offset);

    int status = sandbox->HostStorageWrite(key.c_str(), val_buf, val_len);
    m3ApiReturn(status);
}

// -----------------------------------------------------------------------------
// WasmSandbox Lifecycle & Implementation
// -----------------------------------------------------------------------------

WasmSandbox::WasmSandbox(const ModManifest& manifest,
                         SandboxFaultCallback on_fault,
                         PatchEngine* patch_engine,
                         PrivacyGate* privacy_gate)
    : manifest_(manifest),
      fault_callback_(std::move(on_fault)),
      patch_engine_(patch_engine),
      privacy_gate_(privacy_gate) {
    env_ = m3_NewEnvironment();
}

WasmSandbox::~WasmSandbox() {
    if (runtime_) {
        m3_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
    if (env_) {
        m3_FreeEnvironment(env_);
        env_ = nullptr;
    }
}

bool WasmSandbox::CheckCapability(const std::string& cap) {
    return manifest_.capabilities.find(cap) != manifest_.capabilities.end();
}

void WasmSandbox::HostLog(int level, const char* msg) {
    std::cout << "[Mod:" << manifest_.id << "][L" << level << "] " << (msg ? msg : "") << std::endl;
}

int WasmSandbox::HostStorageRead(const char* key, void* out_buf, size_t max_len) {
    auto it = storage_.find(key ? key : "");
    if (it == storage_.end()) return -1;

    size_t copy_len = min(max_len, it->second.size());
    if (copy_len > 0 && out_buf) {
        memcpy(out_buf, it->second.data(), copy_len);
    }
    return static_cast<int>(copy_len);
}

int WasmSandbox::HostStorageWrite(const char* key, const void* val, size_t len) {
    if (!key) return -1;
    size_t quota_bytes = static_cast<size_t>(manifest_.resource_limits.storage_quota_kb) * 1024;
    size_t current_usage = 0;
    for (const auto& [k, v] : storage_) {
        current_usage += k.size() + v.size();
    }
    if (current_usage + strlen(key) + len > quota_bytes) {
        return -2; // Quota exceeded
    }

    std::vector<uint8_t> data;
    if (len > 0 && val) {
        data.assign(static_cast<const uint8_t*>(val), static_cast<const uint8_t*>(val) + len);
    }
    storage_[key] = std::move(data);
    return 0;
}

bool WasmSandbox::LoadBytecode(const std::vector<uint8_t>& wasm_bytecode, std::string& out_error) {
    if (wasm_bytecode.empty()) {
        out_error = "WASM bytecode buffer is empty";
        return false;
    }

    wasm_copy_ = wasm_bytecode;

    uint32_t stack_size = 64 * 1024;
    runtime_ = m3_NewRuntime(env_, stack_size, this);
    if (!runtime_) {
        out_error = "Failed to create wasm3 runtime";
        return false;
    }

    M3Result res = m3_ParseModule(env_, &module_, wasm_copy_.data(), static_cast<uint32_t>(wasm_copy_.size()));
    if (res) {
        out_error = std::string("WASM parse error: ") + res;
        return false;
    }

    res = m3_LoadModule(runtime_, module_);
    if (res) {
        out_error = std::string("WASM load error: ") + res;
        return false;
    }

    // Link host imports
    m3_LinkRawFunctionEx(module_, "env", "refix_log", "v(iii)", Host_refix_log, this);
    m3_LinkRawFunctionEx(module_, "env", "refix_get_time_ms", "i()", Host_refix_get_time_ms, this);
    m3_LinkRawFunctionEx(module_, "env", "refix_storage_read", "i(iiii)", Host_refix_storage_read, this);
    m3_LinkRawFunctionEx(module_, "env", "refix_storage_write", "i(iiii)", Host_refix_storage_write, this);

    res = m3_CompileModule(module_);
    if (res) {
        out_error = std::string("WASM compile error: ") + res;
        return false;
    }

    return true;
}

void WasmSandbox::TriggerFault(const std::string& reason) {
    if (is_faulted_) return;
    is_faulted_ = true;
    fault_reason_ = reason;

    std::cerr << "[CRITICAL][Mod:" << manifest_.id << "] Sandboxed WASM trap/fault triggered: " << reason << std::endl;

    if (fault_callback_) {
        fault_callback_(manifest_.id, reason);
    }
}

bool WasmSandbox::CallExport(const std::string& export_name, std::string& out_error) {
    return CallExportArgs(export_name, 0, nullptr, out_error);
}

bool WasmSandbox::CallExportArgs(const std::string& export_name,
                                int argc,
                                const void* argv[],
                                std::string& out_error) {
    if (is_faulted_) {
        out_error = "Sandbox has faulted previously: " + fault_reason_;
        return false;
    }
    if (!runtime_ || !module_) {
        out_error = "WASM runtime is not initialized";
        return false;
    }

    IM3Function func = nullptr;
    M3Result res = m3_FindFunction(&func, runtime_, export_name.c_str());
    if (res || !func) {
        out_error = "Exported function '" + export_name + "' not found in module";
        return false;
    }

    LARGE_INTEGER freq, start_time, end_time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start_time);

    res = m3_Call(func, argc, argv);

    QueryPerformanceCounter(&end_time);
    double elapsed_ms = (static_cast<double>(end_time.QuadPart - start_time.QuadPart) * 1000.0) / static_cast<double>(freq.QuadPart);

    uint32_t timeout_limit = manifest_.resource_limits.watchdog_timeout_ms;
    if (timeout_limit > 0 && elapsed_ms > static_cast<double>(timeout_limit)) {
        std::string wd_msg = "Watchdog budget exceeded: took " + std::to_string(elapsed_ms) + "ms (limit: " + std::to_string(timeout_limit) + "ms)";
        TriggerFault(wd_msg);
        out_error = wd_msg;
        return false;
    }

    if (res) {
        M3ErrorInfo info;
        m3_GetErrorInfo(runtime_, &info);
        std::string err_desc = res;
        if (info.message) {
            err_desc += std::string(" - ") + info.message;
        }
        TriggerFault("WASM Trap: " + err_desc);
        out_error = err_desc;
        return false;
    }

    return true;
}

uint32_t WasmSandbox::GetLinearMemorySize() const {
    if (!module_) return 0;
    return static_cast<uint32_t>(m3_GetMemorySize(module_, 0));
}

bool WasmSandbox::ReadMemory(uint32_t wasm_offset, void* out_buffer, size_t size) {
    if (!module_ || !out_buffer || size == 0) return false;
    size_t mem_size = m3_GetMemorySize(module_, 0);
    if (static_cast<uint64_t>(wasm_offset) + size > mem_size) {
        return false;
    }

    size_t mem_bytes = 0;
    void* mem_base = m3_GetMemory(module_, &mem_bytes, 0);
    if (!mem_base || (wasm_offset + size > mem_bytes)) return false;

    memcpy(out_buffer, static_cast<const uint8_t*>(mem_base) + wasm_offset, size);
    return true;
}

bool WasmSandbox::WriteMemory(uint32_t wasm_offset, const void* in_buffer, size_t size) {
    if (!module_ || !in_buffer || size == 0) return false;
    size_t mem_bytes = 0;
    void* mem_base = m3_GetMemory(module_, &mem_bytes, 0);
    if (!mem_base || (wasm_offset + size > mem_bytes)) return false;

    memcpy(static_cast<uint8_t*>(mem_base) + wasm_offset, in_buffer, size);
    return true;
}

} // namespace refix::mods
