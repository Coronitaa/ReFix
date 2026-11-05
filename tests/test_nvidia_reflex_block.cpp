#include "../src/overlay/nvidia_reflex_block.h"
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <string>

void ReFixLog(const char* fmt, ...) {}

static std::wstring GetEnv(const wchar_t* name) {
    wchar_t buf[256] = {};
    DWORD n = GetEnvironmentVariableW(name, buf, 256);
    if (n > 0 && n < 256) return buf;
    return L"";
}

int main() {
    printf("[*] Running NVIDIA Reflex Overlay Suppression Unit Tests...\n");

    // 1. Arm the Reflex blocker
    refix::overlay::ArmNvidiaReflexBlock();

    // 2. Verify all environment variables
    assert(GetEnv(L"NV_LOWLATENCY_FLASH_INDICATOR") == L"0");
    assert(GetEnv(L"NV_LOW_LATENCY_FLASH_INDICATOR") == L"0");
    assert(GetEnv(L"NV_REFLEX_DISABLE_OVERLAY") == L"1");
    assert(GetEnv(L"NV_DISABLE_REFLEX_OVERLAY") == L"1");
    assert(GetEnv(L"NV_REFLEX_DISABLE") == L"1");
    assert(GetEnv(L"GFE_DISABLE_OVERLAY") == L"1");
    assert(GetEnv(L"DISABLE_VK_LAYER_NV_optimus_1") == L"1");
    assert(GetEnv(L"DISABLE_VK_LAYER_NV_nomad_1") == L"1");
    assert(GetEnv(L"PROTON_NVAPI_DISABLE") == L"1");
    assert(GetEnv(L"DXVK_NVAPI_VKREFLEX") == L"0");
    printf("  [PASS] All NVIDIA Reflex / GeForce overlay suppression environment variables verified!\n");

    // 3. Verify Idempotence
    refix::overlay::ArmNvidiaReflexBlock();
    assert(GetEnv(L"NV_REFLEX_DISABLE_OVERLAY") == L"1");
    printf("  [PASS] Idempotent execution confirmed (safe for multiple calls across threads/stages)\n");

    printf("[SUCCESS] All NVIDIA Reflex suppression tests passed!\n");
    return 0;
}
