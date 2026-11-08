#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "nvidia_reflex_block.h"

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "../minhook/MinHook.h"

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
namespace {

// NVAPI interface ids (nvapi_QueryInterface).
constexpr unsigned int kIdSetSleepMode = 0xAC789736;          // NvAPI_D3D_SetSleepMode
constexpr unsigned int kIdSleep = 0x852CD1D2;                 // NvAPI_D3D_Sleep
constexpr unsigned int kIdSetLatencyMarker = 0xD9984C05;      // NvAPI_D3D_SetLatencyMarker
constexpr unsigned int kIdD3D12SetLatencyMarker = 0xD22BDD7E; // NvAPI_D3D12_SetLatencyMarker

// NV_LATENCY_MARKER_PARAMS: markerType at offset 12; 7 = TRIGGER_FLASH (the latency flash square).
constexpr uint32_t kMarkerTriggerFlash = 7;
// NV_SET_SLEEP_MODE_PARAMS_V1: version, bLowLatencyMode @4, bLowLatencyBoost @5, minimumIntervalUs @8.
constexpr size_t kSleepParamsMax = 64;
constexpr ULONGLONG kPollMs = 2000;

using QueryInterface_t = void* (*)(unsigned int);
using SetSleepMode_t = int (*)(void* dev, void* params);
using Sleep_t = int (*)(void* dev);
using SetLatencyMarker_t = int (*)(void* dev, void* params);

std::atomic<bool> g_armed{false};
std::atomic<bool> g_hooked{false};
std::atomic<bool> g_paused{false};
ULONGLONG g_lastPoll = 0;

SetSleepMode_t g_origSetSleepMode = nullptr;
Sleep_t g_origSleep = nullptr;
SetLatencyMarker_t g_origMarker = nullptr;
SetLatencyMarker_t g_origMarker12 = nullptr;

// Last sleep mode the game asked for, to restore it when the overlay closes.
std::mutex g_mu;
void* g_gameDevice = nullptr;
uint8_t g_gameParams[kSleepParamsMax] = {};
size_t g_gameParamsSize = 0;

size_t ParamsSize(const void* params) {
    const uint32_t version = *static_cast<const uint32_t*>(params);
    const size_t size = version & 0xFFFF;
    return size >= 12 && size <= kSleepParamsMax ? size : 0;
}

// Copy of the game's parameters with Reflex low latency switched off.
bool PausedParams(const void* params, size_t size, uint8_t* out) {
    if (!size) return false;
    memcpy(out, params, size);
    out[4] = 0;                                   // bLowLatencyMode
    out[5] = 0;                                   // bLowLatencyBoost
    memset(out + 8, 0, sizeof(uint32_t));         // minimumIntervalUs
    return true;
}

int HookSetSleepMode(void* dev, void* params) {
    const size_t size = params ? ParamsSize(params) : 0;
    if (size) {
        std::lock_guard<std::mutex> lock(g_mu);
        g_gameDevice = dev;
        memcpy(g_gameParams, params, size);
        g_gameParamsSize = size;
    }
    uint8_t paused[kSleepParamsMax];
    if (g_paused && PausedParams(params, size, paused)) return g_origSetSleepMode(dev, paused);
    return g_origSetSleepMode(dev, params);
}

int HookSleep(void* dev) {
    if (g_paused) return 0;   // NVAPI_OK: no frame pacing while the overlay is open
    return g_origSleep(dev);
}

bool IsFlash(const void* params) {
    return params && *reinterpret_cast<const uint32_t*>(static_cast<const char*>(params) + 12) == kMarkerTriggerFlash;
}

int HookMarker(void* dev, void* params) { return IsFlash(params) ? 0 : g_origMarker(dev, params); }
int HookMarker12(void* dev, void* params) { return IsFlash(params) ? 0 : g_origMarker12(dev, params); }

template <typename T>
void Hook(QueryInterface_t query, unsigned int id, T detour, T* orig, const char* name) {
    void* target = query(id);
    if (!target) return;
    if (MH_CreateHook(target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(orig)) == MH_OK &&
        MH_EnableHook(target) == MH_OK)
        ReFixLog("[NvidiaReflexBlock] Hooked %s", name);
}

void TryHook() {
    if (!g_armed || g_hooked) return;
#ifdef _WIN64
    HMODULE nvapi = GetModuleHandleW(L"nvapi64.dll");
#else
    HMODULE nvapi = GetModuleHandleW(L"nvapi.dll");
#endif
    if (!nvapi) return;   // not loaded (yet): PollNvidiaReflexHooks tries again
    auto query = reinterpret_cast<QueryInterface_t>(GetProcAddress(nvapi, "nvapi_QueryInterface"));
    if (!query || g_hooked.exchange(true)) return;
    // The functions themselves, not nvapi_QueryInterface: the game may have looked them up already.
    MH_Initialize();
    Hook(query, kIdSetSleepMode, &HookSetSleepMode, &g_origSetSleepMode, "NvAPI_D3D_SetSleepMode");
    Hook(query, kIdSleep, &HookSleep, &g_origSleep, "NvAPI_D3D_Sleep");
    Hook(query, kIdSetLatencyMarker, &HookMarker, &g_origMarker, "NvAPI_D3D_SetLatencyMarker");
    Hook(query, kIdD3D12SetLatencyMarker, &HookMarker12, &g_origMarker12, "NvAPI_D3D12_SetLatencyMarker");
}

} // namespace

void ArmNvidiaReflexBlock() {
    if (!g_armed.exchange(true)) {
        // Switches some NVIDIA/DXVK layers read for the latency flash indicator and the GeForce overlay.
        SetEnvironmentVariableW(L"NV_LOWLATENCY_FLASH_INDICATOR", L"0");
        SetEnvironmentVariableW(L"NV_LOW_LATENCY_FLASH_INDICATOR", L"0");
        SetEnvironmentVariableW(L"NV_REFLEX_DISABLE_OVERLAY", L"1");
        SetEnvironmentVariableW(L"GFE_DISABLE_OVERLAY", L"1");
    }
}

void PollNvidiaReflexHooks() {
    if (!g_armed || g_hooked) return;
    const ULONGLONG now = GetTickCount64();
    if (now - g_lastPoll < kPollMs) return;
    g_lastPoll = now;
    TryHook();
}

bool NvidiaReflexBlockArmed() { return g_armed; }

void SetReflexPausedForOverlay(bool paused) {
    if (!g_armed || g_paused == paused) return;
    g_paused = paused;
    // Apply right away on the game's device: low latency off now, the game's own settings back on close.
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_origSetSleepMode || !g_gameDevice || !g_gameParamsSize) return;
    uint8_t params[kSleepParamsMax];
    if (paused) PausedParams(g_gameParams, g_gameParamsSize, params);
    else memcpy(params, g_gameParams, g_gameParamsSize);
    g_origSetSleepMode(g_gameDevice, params);
    ReFixLog("[NvidiaReflexBlock] Reflex %s for the overlay", paused ? "paused" : "restored");
}

} // namespace refix::overlay
