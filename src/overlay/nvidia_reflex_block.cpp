#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "nvidia_reflex_block.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>
#include "../minhook/MinHook.h"

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
namespace {

// NVAPI interface ids (nvapi_QueryInterface) of NVIDIA Reflex.
constexpr unsigned int kIdSetSleepMode = 0xAC789736;          // NvAPI_D3D_SetSleepMode
constexpr unsigned int kIdSleep = 0x852CD1D2;                 // NvAPI_D3D_Sleep
constexpr unsigned int kIdGetSleepStatus = 0xAEF96CA1;        // NvAPI_D3D_GetSleepStatus
constexpr unsigned int kIdGetLatency = 0x1A587F9C;            // NvAPI_D3D_GetLatency
constexpr unsigned int kIdSetLatencyMarker = 0xD9984C05;      // NvAPI_D3D_SetLatencyMarker
constexpr unsigned int kIdD3D12SetLatencyMarker = 0xD22BDD7E; // NvAPI_D3D12_SetLatencyMarker
constexpr unsigned int kReflexIds[] = { kIdSetSleepMode, kIdSleep, kIdGetSleepStatus, kIdGetLatency,
                                        kIdSetLatencyMarker, kIdD3D12SetLatencyMarker };

// NV_SET_SLEEP_MODE_PARAMS_V1: version, bLowLatencyMode @4, bLowLatencyBoost @5, minimumIntervalUs @8.
constexpr size_t kSleepParamsMax = 64;
constexpr ULONGLONG kPollMs = 2000;

using QueryInterface_t = void* (*)(unsigned int);
using SetSleepMode_t = int (*)(void* dev, void* params);
using Sleep_t = int (*)(void* dev);
using SetLatencyMarker_t = int (*)(void* dev, void* params);

std::atomic<bool> g_armed{false};
std::atomic<bool> g_hooked{false};
ULONGLONG g_lastPoll = 0;

QueryInterface_t g_origQuery = nullptr;
SetSleepMode_t g_origSetSleepMode = nullptr;
Sleep_t g_origSleep = nullptr;
SetLatencyMarker_t g_origMarker = nullptr;
SetLatencyMarker_t g_origMarker12 = nullptr;

size_t ParamsSize(const void* params) {
    const uint32_t version = *static_cast<const uint32_t*>(params);
    const size_t size = version & 0xFFFF;
    return size >= 12 && size <= kSleepParamsMax ? size : 0;
}

// NVAPI no longer offers Reflex to the game: it sees a driver without it and never turns it on.
void* HookQuery(unsigned int id) {
    for (unsigned int r : kReflexIds) if (id == r) return nullptr;
    return g_origQuery(id);
}

// For a game that looked the functions up before the hooks: low latency always off, no frame pacing, no markers.
int HookSetSleepMode(void* dev, void* params) {
    const size_t size = params ? ParamsSize(params) : 0;
    if (!size) return 0;
    uint8_t off[kSleepParamsMax];
    memcpy(off, params, size);
    off[4] = 0;                                   // bLowLatencyMode
    off[5] = 0;                                   // bLowLatencyBoost
    memset(off + 8, 0, sizeof(uint32_t));         // minimumIntervalUs
    return g_origSetSleepMode(dev, off);          // switches it off in the driver if the game had it on
}

int HookSleep(void*) { return 0; }                // NVAPI_OK, no wait
int HookMarker(void*, void*) { return 0; }

template <typename T>
void Hook(unsigned int id, T detour, T* orig, const char* name) {
    void* target = g_origQuery(id);
    if (!target) return;
    if (MH_CreateHook(target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(orig)) == MH_OK &&
        MH_EnableHook(target) == MH_OK)
        ReFixLog("[NvidiaReflexBlock] Disabled %s", name);
}

void TryHook() {
    if (!g_armed || g_hooked) return;
#ifdef _WIN64
    const wchar_t* dll = L"nvapi64.dll";
#else
    const wchar_t* dll = L"nvapi.dll";
#endif
    // Loaded now (not when the game gets to it) so the game's first Reflex lookup already finds nothing.
    // No NVIDIA driver: the DLL does not exist and there is no Reflex to turn off.
    HMODULE nvapi = GetModuleHandleW(dll);
    if (!nvapi) nvapi = LoadLibraryW(dll);
    if (!nvapi) return;
    auto query = reinterpret_cast<QueryInterface_t>(GetProcAddress(nvapi, "nvapi_QueryInterface"));
    if (!query || g_hooked.exchange(true)) return;
    MH_Initialize();
    g_origQuery = query;
    Hook(kIdSetSleepMode, &HookSetSleepMode, &g_origSetSleepMode, "NvAPI_D3D_SetSleepMode");
    Hook(kIdSleep, &HookSleep, &g_origSleep, "NvAPI_D3D_Sleep");
    Hook(kIdSetLatencyMarker, &HookMarker, &g_origMarker, "NvAPI_D3D_SetLatencyMarker");
    Hook(kIdD3D12SetLatencyMarker, &HookMarker, &g_origMarker12, "NvAPI_D3D12_SetLatencyMarker");
    if (MH_CreateHook(reinterpret_cast<void*>(query), reinterpret_cast<void*>(&HookQuery),
                      reinterpret_cast<void**>(&g_origQuery)) == MH_OK &&
        MH_EnableHook(reinterpret_cast<void*>(query)) == MH_OK)
        ReFixLog("[NvidiaReflexBlock] NVIDIA Reflex hidden from the game");
}

} // namespace

void ArmNvidiaReflexBlock() {
    if (!g_armed.exchange(true)) {
        // Switches some NVIDIA/DXVK layers read for the latency flash indicator and the GeForce overlay.
        SetEnvironmentVariableW(L"NV_LOWLATENCY_FLASH_INDICATOR", L"0");
        SetEnvironmentVariableW(L"NV_LOW_LATENCY_FLASH_INDICATOR", L"0");
        SetEnvironmentVariableW(L"NV_REFLEX_DISABLE_OVERLAY", L"1");
        SetEnvironmentVariableW(L"NV_REFLEX_DISABLE", L"1");
        SetEnvironmentVariableW(L"DXVK_NVAPI_VKREFLEX", L"0");
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

namespace {
std::mutex g_nvOverlayMu;
std::vector<HWND> g_nvHidden;   // NVIDIA overlay windows we hid, to show again

BOOL CALLBACK HideNvidiaWindow(HWND w, LPARAM) {
    wchar_t cls[64] = {};
    if (!IsWindowVisible(w) || !GetClassNameW(w, cls, 64) || wcscmp(cls, L"CEF-OSC-WIDGET") != 0) return TRUE;
    wchar_t title[64] = {};
    InternalGetWindowText(w, title, 64);   // no WM_GETTEXT round trip to another process
    if (wcsncmp(title, L"NVIDIA GeForce Overlay", 22) != 0) return TRUE;
    ShowWindowAsync(w, SW_HIDE);
    if (std::find(g_nvHidden.begin(), g_nvHidden.end(), w) == g_nvHidden.end()) {
        g_nvHidden.push_back(w);
        ReFixLog("[NvidiaReflexBlock] NVIDIA overlay window hidden while the game is in front");
    }
    return TRUE;
}
} // namespace

void SetNvidiaOverlayHidden(bool hidden) {
    std::lock_guard<std::mutex> lock(g_nvOverlayMu);
    if (hidden) {
        EnumWindows(HideNvidiaWindow, 0);   // every time: NVIDIA shows it again by itself
        return;
    }
    for (HWND w : g_nvHidden) if (IsWindow(w)) ShowWindowAsync(w, SW_SHOWNA);
    g_nvHidden.clear();
}

} // namespace refix::overlay
