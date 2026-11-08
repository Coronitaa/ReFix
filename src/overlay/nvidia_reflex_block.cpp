#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "nvidia_reflex_block.h"

#include <windows.h>
#include <atomic>
#include <cstdint>
#include "../minhook/MinHook.h"

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
namespace {

std::atomic<bool> g_armed{false};
std::atomic<bool> g_hookInstalled{false};

typedef void* (*NvAPI_QueryInterface_t)(unsigned int offset);
static NvAPI_QueryInterface_t s_origNvAPI_QueryInterface = nullptr;

// NVAPI Latency Marker IDs
constexpr unsigned int NVAPI_ID_D3D_SetLatencyMarker   = 0xD9984C05;
constexpr unsigned int NVAPI_ID_D3D12_SetLatencyMarker = 0xD22BDD7E;
constexpr unsigned int NVAPI_ID_D3D_SetSleepMode       = 0x83210CD2;
constexpr unsigned int NVAPI_ID_D3D12_SetSleepMode     = 0xAC10B6F8;

// NV_LATENCY_MARKER_PARAMS:
// version: uint32_t (4 bytes)
// frameID: uint64_t (8 bytes)
// markerType: uint32_t (4 bytes, offset 12)
// markerType 7 == TRIGGER_FLASH (the latency flash indicator square)
constexpr uint32_t NV_LATENCY_MARKER_TRIGGER_FLASH = 7;

typedef int (*NvAPI_SetLatencyMarker_t)(void* pDev, void* pParams);
static NvAPI_SetLatencyMarker_t s_origD3D_SetLatencyMarker = nullptr;
static NvAPI_SetLatencyMarker_t s_origD3D12_SetLatencyMarker = nullptr;

static int Hook_D3D_SetLatencyMarker(void* pDev, void* pParams) {
    if (pParams) {
        const uint32_t* pMarkerType = reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(pParams) + 12);
        if (*pMarkerType == NV_LATENCY_MARKER_TRIGGER_FLASH) {
            // Drop flash indicator requests so no white box is rendered on screen
            return 0; // NVAPI_OK
        }
    }
    if (s_origD3D_SetLatencyMarker) {
        return s_origD3D_SetLatencyMarker(pDev, pParams);
    }
    return 0;
}

static int Hook_D3D12_SetLatencyMarker(void* pDev, void* pParams) {
    if (pParams) {
        const uint32_t* pMarkerType = reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(pParams) + 12);
        if (*pMarkerType == NV_LATENCY_MARKER_TRIGGER_FLASH) {
            // Drop flash indicator requests so no white box is rendered on screen
            return 0; // NVAPI_OK
        }
    }
    if (s_origD3D12_SetLatencyMarker) {
        return s_origD3D12_SetLatencyMarker(pDev, pParams);
    }
    return 0;
}

static void* Hook_NvAPI_QueryInterface(unsigned int offset) {
    if (!s_origNvAPI_QueryInterface) return nullptr;
    void* pfn = s_origNvAPI_QueryInterface(offset);
    if (!pfn) return nullptr;

    if (offset == NVAPI_ID_D3D_SetLatencyMarker) {
        if (!s_origD3D_SetLatencyMarker) {
            s_origD3D_SetLatencyMarker = reinterpret_cast<NvAPI_SetLatencyMarker_t>(pfn);
        }
        return reinterpret_cast<void*>(&Hook_D3D_SetLatencyMarker);
    }
    if (offset == NVAPI_ID_D3D12_SetLatencyMarker) {
        if (!s_origD3D12_SetLatencyMarker) {
            s_origD3D12_SetLatencyMarker = reinterpret_cast<NvAPI_SetLatencyMarker_t>(pfn);
        }
        return reinterpret_cast<void*>(&Hook_D3D12_SetLatencyMarker);
    }

    return pfn;
}

void TryHookNvApi() {
    bool expected = false;
    if (!g_hookInstalled.compare_exchange_strong(expected, true)) return;

#ifdef _WIN64
    HMODULE hNvApi = GetModuleHandleW(L"nvapi64.dll");
#else
    HMODULE hNvApi = GetModuleHandleW(L"nvapi.dll");
#endif
    if (!hNvApi) return;

    FARPROC pQuery = GetProcAddress(hNvApi, "nvapi_QueryInterface");
    if (!pQuery) return;

    MH_Initialize();
    if (MH_CreateHook(reinterpret_cast<LPVOID>(pQuery),
                      reinterpret_cast<LPVOID>(&Hook_NvAPI_QueryInterface),
                      reinterpret_cast<LPVOID*>(&s_origNvAPI_QueryInterface)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(pQuery));
        ReFixLog("[NvidiaReflexBlock] Successfully hooked nvapi_QueryInterface to suppress Reflex flash overlays");
    }
}

} // namespace

void ArmNvidiaReflexBlock() {
    bool expected = false;
    if (g_armed.compare_exchange_strong(expected, true)) {
        // 1. Environment switches recognized by NVIDIA Reflex SDK, Vulkan loader, Proton and DXVK
        SetEnvironmentVariableW(L"NV_LOWLATENCY_FLASH_INDICATOR", L"0");
        SetEnvironmentVariableW(L"NV_LOW_LATENCY_FLASH_INDICATOR", L"0");
        SetEnvironmentVariableW(L"NV_REFLEX_DISABLE_OVERLAY", L"1");
        SetEnvironmentVariableW(L"NV_DISABLE_REFLEX_OVERLAY", L"1");
        SetEnvironmentVariableW(L"NV_REFLEX_DISABLE", L"1");
        SetEnvironmentVariableW(L"GFE_DISABLE_OVERLAY", L"1");
        SetEnvironmentVariableW(L"DISABLE_VK_LAYER_NV_optimus_1", L"1");
        SetEnvironmentVariableW(L"DISABLE_VK_LAYER_NV_nomad_1", L"1");
        SetEnvironmentVariableW(L"PROTON_NVAPI_DISABLE", L"1");
        SetEnvironmentVariableW(L"DXVK_NVAPI_VKREFLEX", L"0");

        if (GetModuleHandleW(L"nvspcaps64.dll") || GetModuleHandleW(L"nvspcaps.dll")) {
            ReFixLog("[NvidiaReflexBlock] GeForce Experience capture overlay module is loaded; suppressing Reflex flash & stats overlay");
        }
    }

    // 2. In-process interception if NVAPI is present
    TryHookNvApi();
}

} // namespace refix::overlay
