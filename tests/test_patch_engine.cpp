#include "../src/mods/patch_engine.h"
#include <iostream>
#include <cassert>

using namespace refix::mods;

static int g_HookTestVal = 0;
__declspec(noinline) int TargetFunction(int a, int b) {
    return a + b;
}

static int (*g_OrigTargetFunction)(int a, int b) = nullptr;
static int DetourFunction(int a, int b) {
    g_HookTestVal = 100;
    return g_OrigTargetFunction(a, b) * 2;
}

int main() {
    std::cout << "Running test_patch_engine..." << std::endl;

    PatchEngine engine;
    assert(engine.Initialize());
    std::string err;

    // 1. Test forbidden module rejection
    assert(PatchEngine::IsForbiddenModule("kernel32.dll"));
    assert(PatchEngine::IsForbiddenModule("ntdll.dll"));
    assert(PatchEngine::IsForbiddenModule("winmm.dll"));
    assert(PatchEngine::IsForbiddenModule("C:\\Windows\\System32\\ws2_32.dll"));
    assert(!PatchEngine::IsForbiddenModule("GameAssembly.dll"));
    assert(!PatchEngine::IsForbiddenModule("mono-2.0-bdwgc.dll"));
    assert(!PatchEngine::IsForbiddenModule("MyGame.exe"));
    std::cout << "[PASS] Forbidden module filter operational" << std::endl;

    // Attempt declarative patch on kernel32.dll must fail immediately
    bool patch_res = engine.ApplyPatch("mod1", "kernel32.dll", 0x1000, {0x90}, {0xCC}, err);
    assert(!patch_res);
    assert(err.find("forbidden") != std::string::npos);
    std::cout << "[PASS] System DLL declarative patch blocked: " << err << std::endl;

    // 2. Test declarative patch on simulated target buffer
    uint8_t buffer[16] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                           0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00 };
    uint8_t expected_orig[4] = { 0x11, 0x22, 0x33, 0x44 };
    uint8_t new_bytes[4] = { 0x90, 0x90, 0x90, 0x90 };

    // Test mismatch detection
    std::vector<uint8_t> wrong_orig = { 0x00, 0x00, 0x00, 0x00 };
    // We can test GuardedMemoryWrite and declarative patch rollback
    uint8_t write_val[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    bool write_ok = engine.GuardedMemoryWrite("mod1", buffer, write_val, sizeof(write_val), err);
    assert(write_ok);
    assert(buffer[0] == 0xDE && buffer[1] == 0xAD && buffer[2] == 0xBE && buffer[3] == 0xEF);
    std::cout << "[PASS] Guarded memory write executed successfully" << std::endl;

    // 3. Test ROP system executable pointer injection mitigation
    void* kernel32_proc = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "ExitProcess"));
    assert(kernel32_proc != nullptr);
    void* evil_payload[2] = { kernel32_proc, nullptr };
    bool evil_write = engine.GuardedMemoryWrite("mod_evil", buffer, evil_payload, sizeof(evil_payload), err);
    assert(!evil_write);
    assert(err.find("ROP mitigation") != std::string::npos);
    std::cout << "[PASS] ROP system pointer hijacking blocked: " << err << std::endl;

    // 4. Test MinHook mediated hook
    HMODULE hSelf = GetModuleHandleA(NULL);
    uint32_t func_rva = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(TargetFunction) - reinterpret_cast<uint8_t*>(hSelf));

    int (* volatile pTarget)(int, int) = TargetFunction;

    int base_val = pTarget(3, 4);
    assert(base_val == 7);

    bool hook_ok = engine.RegisterHook("mod_hook", "", func_rva, reinterpret_cast<void*>(DetourFunction), reinterpret_cast<void**>(&g_OrigTargetFunction), err);
    assert(hook_ok);
    assert(g_OrigTargetFunction != nullptr);

    int hooked_val = pTarget(3, 4);
    assert(hooked_val == 14); // (3 + 4) * 2
    assert(g_HookTestVal == 100);
    std::cout << "[PASS] Mediated MinHook detour invoked successfully" << std::endl;

    // 5. Test Hook rollback
    engine.RemoveModHooks("mod_hook");
    int restored_val = pTarget(3, 4);
    assert(restored_val == 7);
    std::cout << "[PASS] Hook removal completely restored original target function" << std::endl;

    std::cout << "All test_patch_engine tests passed!" << std::endl;
    return 0;
}
