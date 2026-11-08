#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "overlay_ui_mods.h"
#include "game_actions/overlay_actions.h"
#include "overlay_theme.h"
#include "overlay_i18n.h"
#include "overlay_app.h"
#include "refix_overlay.h"
#include "../mods/mod_host.h"
#include "../steam_gc_emu.h"
#include "../unreal_steam_emu.h"
#include "presence_net.h"
#include "csgo_schema.h"
#include "../lan_core/refix_lan_core.h"
#include "../steam_lobby_mapping.h"
#include <algorithm>
#include <vector>
#include <string>
#include "../source_main_thread_queue.h"

// Exports already declared in steam_gc_emu.h

namespace refix::overlay {

void ExecuteEngineClientCommand(const char* cmd) {
    if (!cmd || !*cmd) return;
    refix::source::MainThreadQueue::ExecuteClientCmd(cmd);
}

extern "C" {
__declspec(dllexport) void CSGO_OpenMarketHUD() {
    App::Get().OpenMarketHUD();
}
__declspec(dllexport) void CSGO_CloseMarketHUD() {
    App::Get().CloseMarketHUD();
}
__declspec(dllexport) void CSGO_OpenOverlay() {
    App::Get().Post([] { App::Get().OpenOverlay(); });
}
__declspec(dllexport) void CSGO_CloseOverlay() {
    App::Get().Post([] { App::Get().CloseOverlay(); });
}
__declspec(dllexport) void CSGO_ToggleOverlay() {
    App::Get().Post([] {
        if (App::Get().IsOverlayOpen()) App::Get().CloseOverlay();
        else App::Get().OpenOverlay();
    });
}
}

void OpenMarketGiveMenu() {
    App::Get().CloseOverlay();
    App::Get().OpenMarketHUD();
}

void OpenHostMatchModal() {
    App::Get().CloseOverlay();
    App::Get().OpenMatchmakingHUD();
}

void OpenMatchmakingToast() {
    App::Get().CloseOverlay();
    App::Get().OpenMatchmakingHUD();
}

// Market & Skins, the LAN server settings and the CS:GO LAN card live in overlay/csgo/.

void DrawModsApprovalModal() {}

} // namespace refix::overlay
