#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include "csgo_handlers.h"

static refix::IRefixServices* g_modServices = nullptr;

void ReFixLog(const char* fmt, ...) {
    if (!g_modServices || !g_modServices->Log) return;
    va_list args;
    va_start(args, fmt);
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    g_modServices->Log("%s", buf);
}

extern "C" {

__declspec(dllexport) int refix_mod_init(const void* host_api) {
    if (!host_api) return -1;
    g_modServices = const_cast<refix::IRefixServices*>(static_cast<const refix::IRefixServices*>(host_api));
    if (g_modServices->version < 1) return -2;

    if (g_modServices->Log) {
        g_modServices->Log("[CSGO Mod] Initializing CS:GO LAN & Economy Overhaul (Native Tier)...");
    }

    // Register GC Handlers
    refix::csgo::RegisterCSGOHandlers(g_modServices);

    // The CS:GO overlay UI (LAN card, Market & Skins, LAN server) is drawn by the host overlay (overlay/csgo/).

    if (g_modServices->Log) {
        g_modServices->Log("[CSGO Mod] CS:GO LAN & Economy Overhaul native mod initialized successfully");
    }

    return 0; // Success
}

__declspec(dllexport) void refix_mod_shutdown() {
    if (g_modServices && g_modServices->Log) {
        g_modServices->Log("[CSGO Mod] CS:GO LAN & Economy Overhaul shutting down");
    }
    g_modServices = nullptr;
}

__declspec(dllexport) void refix_mod_on_event(int event_id, const void* event_data) {
    // Lifecycle and engine event dispatch
}

} // extern "C"
