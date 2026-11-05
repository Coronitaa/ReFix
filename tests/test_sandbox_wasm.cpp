#include "../src/mods/sandbox_wasm.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace refix::mods;

int main() {
    std::cout << "Running test_sandbox_wasm..." << std::endl;

    std::string err;

    // 1. Test valid add(a, b) function in WASM
    {
        ModManifest manifest;
        manifest.id = "com.refix.wasmtest";
        manifest.capabilities = { Capability::Log, Capability::Storage };
        manifest.resource_limits.watchdog_timeout_ms = 50;

        bool faulted = false;
        std::string fault_msg;
        auto on_fault = [&](const std::string& mod_id, const std::string& reason) {
            faulted = true;
            fault_msg = reason;
        };

        WasmSandbox sandbox(manifest, on_fault);

        // Bytecode for add(i32, i32) -> i32
        std::vector<uint8_t> add_wasm = {
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
            0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f,
            0x03, 0x02, 0x01, 0x00,
            0x07, 0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00,
            0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
        };

        bool ok = sandbox.LoadBytecode(add_wasm, err);
        if (!ok) {
            std::cerr << "LoadBytecode failed: " << err << std::endl;
            assert(false);
        }

        const char* i32_arg1 = "15";
        const char* i32_arg2 = "27";
        const void* args[] = { i32_arg1, i32_arg2 };

        bool call_ok = sandbox.CallExportArgs("add", 2, args, err);
        assert(call_ok);
        assert(!faulted);
        std::cout << "[PASS] Executed exported add(15, 27) successfully" << std::endl;

        // Test Host Storage Write and Read
        std::string key = "high_score";
        int score = 9999;
        int status = sandbox.HostStorageWrite(key.c_str(), &score, sizeof(score));
        assert(status == 0);

        int read_score = 0;
        int read_bytes = sandbox.HostStorageRead(key.c_str(), &read_score, sizeof(read_score));
        assert(read_bytes == sizeof(int));
        assert(read_score == 9999);
        std::cout << "[PASS] Sandboxed storage read/write isolation functional" << std::endl;

        // Test Storage Quota enforcement (default 1024 KB)
        std::vector<uint8_t> huge_data(1024 * 1024 + 100, 0xAA);
        status = sandbox.HostStorageWrite("huge", huge_data.data(), huge_data.size());
        assert(status == -2); // Quota exceeded
        std::cout << "[PASS] Storage quota limit strictly enforced" << std::endl;
    }

    // 2. Test WASM trap (division by zero) handling and rollback trigger
    {
        ModManifest manifest;
        manifest.id = "com.refix.traptest";

        bool faulted = false;
        std::string fault_msg;
        auto on_fault = [&](const std::string& mod_id, const std::string& reason) {
            faulted = true;
            fault_msg = reason;
        };

        WasmSandbox sandbox(manifest, on_fault);

        // Bytecode for trap() -> i32 (divides 42 by 0)
        std::vector<uint8_t> trap_wasm = {
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
            0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
            0x03, 0x02, 0x01, 0x00,
            0x07, 0x08, 0x01, 0x04, 0x74, 0x72, 0x61, 0x70, 0x00, 0x00,
            0x0a, 0x09, 0x01, 0x07, 0x00, 0x41, 0x2a, 0x41, 0x00, 0x6d, 0x0b
        };

        bool load_ok = sandbox.LoadBytecode(trap_wasm, err);
        if (!load_ok) {
            std::cerr << "trap_wasm LoadBytecode error: " << err << std::endl;
        }
        assert(load_ok);

        bool call_ok = sandbox.CallExport("trap", err);
        assert(!call_ok);
        assert(faulted);
        assert(sandbox.IsFaulted());
        std::cout << "[PASS] WASM division by zero trap caught gracefully without crash: " << fault_msg << std::endl;

        // Any subsequent calls to faulted sandbox must be refused
        bool refused = sandbox.CallExport("trap", err);
        assert(!refused);
        assert(err.find("Sandbox has faulted previously") != std::string::npos);
        std::cout << "[PASS] Subsequent calls on faulted sandbox safely rejected" << std::endl;
    }

    std::cout << "All test_sandbox_wasm tests passed!" << std::endl;
    return 0;
}
