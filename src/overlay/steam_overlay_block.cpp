#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "steam_overlay_block.h"

#include <windows.h>
#include <atomic>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
namespace {
std::atomic<bool> g_armed{false};
} // namespace

void ArmSteamOverlayBlock() {
    bool expected = false;
    if (!g_armed.compare_exchange_strong(expected, true)) return;
    SetEnvironmentVariableW(L"DISABLE_VK_LAYER_VALVE_steam_overlay_1", L"1");
    SetEnvironmentVariableW(L"SteamNoOverlayUIDrawing", L"1");
    if (GetModuleHandleW(L"GameOverlayRenderer64.dll") || GetModuleHandleW(L"GameOverlayRenderer.dll"))
        ReFixLog("[SteamOverlayBlock] Steam overlay renderer is loaded; its Shift+Tab is taken by the ReFix overlay");
}

} // namespace refix::overlay
