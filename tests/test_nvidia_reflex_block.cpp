// NVIDIA Reflex pause for the overlay. On a PC with an NVIDIA driver it hooks the real NVAPI and checks that
// NvAPI_D3D_Sleep returns at once (NVAPI_OK) while the overlay is open and reaches the driver again after.
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
    printf("[*] NVIDIA Reflex pause tests\n");

    // Not armed: pausing does nothing (and must not crash).
    SetReflexPausedForOverlay(true);
    SetReflexPausedForOverlay(false);

    ArmNvidiaReflexBlock();
    ArmNvidiaReflexBlock();   // idempotent
    assert(GetEnv(L"NV_LOW_LATENCY_FLASH_INDICATOR") == L"0");
    assert(GetEnv(L"NV_REFLEX_DISABLE") == L"");   // Reflex itself is not switched off for the game
    printf("  [PASS] armed; flash indicator switches set, Reflex left on for the game\n");

#ifdef _WIN64
    HMODULE nvapi = LoadLibraryW(L"nvapi64.dll");
#else
    HMODULE nvapi = LoadLibraryW(L"nvapi.dll");
#endif
    if (!nvapi) {
        printf("  [SKIP] no NVIDIA driver on this PC\n");
        return 0;
    }
    PollNvidiaReflexHooks();
    using Query = void* (*)(unsigned int);
    using SleepFn = int (*)(void*);
    auto query = reinterpret_cast<Query>(GetProcAddress(nvapi, "nvapi_QueryInterface"));
    assert(query);
    auto sleepFn = reinterpret_cast<SleepFn>(query(0x852CD1D2));   // NvAPI_D3D_Sleep (hooked in place)
    assert(sleepFn);

    const int before = sleepFn(nullptr);   // reaches the driver: an error for a null device
    SetReflexPausedForOverlay(true);
    assert(sleepFn(nullptr) == 0);         // paused: returns NVAPI_OK without calling the driver
    SetReflexPausedForOverlay(false);
    assert(sleepFn(nullptr) == before);    // back to the driver
    printf("  [PASS] NvAPI_D3D_Sleep paused while the overlay is open (driver result %d otherwise)\n", before);
    printf("[SUCCESS]\n");
    return 0;
}
