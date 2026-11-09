// NVIDIA Reflex off in the game. On a PC with an NVIDIA driver it hooks the real NVAPI and checks that the game can
// no longer look Reflex up, and that NvAPI_D3D_Sleep found earlier returns at once without the driver.
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
    using namespace refix::overlay;
    printf("[*] NVIDIA Reflex block tests\n");

    PollNvidiaReflexHooks();   // not armed: nothing happens
    assert(!NvidiaReflexBlockArmed());

    ArmNvidiaReflexBlock();
    ArmNvidiaReflexBlock();    // idempotent
    assert(NvidiaReflexBlockArmed());
    assert(GetEnv(L"NV_LOW_LATENCY_FLASH_INDICATOR") == L"0");
    assert(GetEnv(L"NV_REFLEX_DISABLE") == L"1");
    printf("  [PASS] armed; Reflex switches set in the environment\n");

#ifdef _WIN64
    HMODULE nvapi = LoadLibraryW(L"nvapi64.dll");
#else
    HMODULE nvapi = LoadLibraryW(L"nvapi.dll");
#endif
    if (!nvapi) {
        printf("  [SKIP] no NVIDIA driver on this PC\n");
        return 0;
    }
    using Query = void* (*)(unsigned int);
    using SleepFn = int (*)(void*);
    auto query = reinterpret_cast<Query>(GetProcAddress(nvapi, "nvapi_QueryInterface"));
    assert(query);
    auto sleepFn = reinterpret_cast<SleepFn>(query(0x852CD1D2));   // looked up before the hooks, like an early game
    assert(sleepFn);
    const int before = sleepFn(nullptr);   // reaches the driver: an error for a null device
    PollNvidiaReflexHooks();
    assert(sleepFn(nullptr) == 0);                  // hooked in place: NVAPI_OK without the driver
    assert(query(0x852CD1D2) == nullptr);           // NvAPI_D3D_Sleep is no longer offered
    assert(query(0xAC789736) == nullptr);           // NvAPI_D3D_SetSleepMode either
    assert(query(0x0150E828) != nullptr);           // other NVAPI functions still are (NvAPI_Initialize)
    printf("  [PASS] Reflex hidden from NVAPI lookups; early Sleep pointer returns at once (driver said %d)\n", before);
    printf("[SUCCESS]\n");
    return 0;
}
