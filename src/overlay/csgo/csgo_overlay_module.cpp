// Registers the CS:GO LAN card, Market & Skins and the LAN server dialog through the overlay mod ABI.
// builtin_modules.cpp lists this module for CS:GO only, so none of it exists in another game.
#include "csgo_overlay.h"

#include <cstring>
#include <string>

namespace refix::overlay::csgo_ui {

namespace {

RefixOverlayHost g_host{};  // function pointers and token copied from the registration call

void DrawLanCardCb(void*, const RefixPanelDrawArgs* a) { DrawLanCard(a->width, a->height); }
float LanCardHeightCb(void*, float width) { return LanCardHeight(width); }
void DrawMarketCb(void*, const RefixPanelDrawArgs* a) { DrawMarketHud(a->width, a->height); }
void DrawServerCb(void*, const RefixPanelDrawArgs* a) { DrawServerHud(a->width, a->height); }
int OnRequestCb(void*, uint32_t kind, const char* text) { return HandleRequest(kind, text) ? 1 : 0; }

bool StartsWith(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

void OpenServer(const std::string& mode) {
    RunOnOverlayThread([mode] {
        if (!mode.empty()) SetServerMode(mode.c_str());
        CloseOverlayNow();
        if (g_host.open_panel) g_host.open_panel(g_host.token, kServerHud);
    });
}

} // namespace

int RegisterOverlay(const RefixOverlayHost* host) {
    if (!host || host->struct_size < sizeof(RefixOverlayHost)) return -1;
    g_host = *host;
    const RefixPanelDesc panels[] = {
        {sizeof(RefixPanelDesc), REFIX_OVERLAY_ABI_VERSION, kLanCard, "CS:GO LAN", nullptr, REFIX_PANEL_SLOT_GRID,
         DrawLanCardCb, LanCardHeightCb, nullptr},
        {sizeof(RefixPanelDesc), REFIX_OVERLAY_ABI_VERSION, kMarketHud, nullptr, nullptr, REFIX_PANEL_SLOT_HUD,
         DrawMarketCb, nullptr, nullptr},
        {sizeof(RefixPanelDesc), REFIX_OVERLAY_ABI_VERSION, kServerHud, nullptr, nullptr, REFIX_PANEL_SLOT_HUD,
         DrawServerCb, nullptr, nullptr},
    };
    for (const RefixPanelDesc& p : panels) {
        if (host->register_panel(host->token, &p) != REFIX_OK) return -2;
    }
    const RefixActionDesc action{sizeof(RefixActionDesc), REFIX_OVERLAY_ABI_VERSION, "csgo.requests", OnRequestCb, nullptr};
    return host->register_action(host->token, &action) == REFIX_OK ? 0 : -3;
}

void OpenHud(const char* id) {
    const std::string panel = id;
    RunOnOverlayThread([panel] {
        CloseOverlayNow();
        if (g_host.open_panel) g_host.open_panel(g_host.token, panel.c_str());
    });
}

void CloseHud(const char* id) {
    if (g_host.close_panel) g_host.close_panel(g_host.token, id);
}

bool HandleRequest(uint32_t kind, const char* text) {
    if (!text) return false;
    if (kind == REFIX_REQUEST_DIALOG) {
        if (_stricmp(text, "Community") != 0 && _stricmp(text, "Market") != 0) return false;
        OpenHud(kMarketHud);
        return true;
    }
    if (kind != REFIX_REQUEST_WEBPAGE) return false;
    // The play menu's "LAN server" buttons (src/panorama/refix_play_menu_js.h).
    if (strcmp(text, "refix://server") == 0) { OpenServer(""); return true; }
    if (StartsWith(text, "refix://server/")) { OpenServer(text + strlen("refix://server/")); return true; }
    if (StartsWith(text, "refix://mode/")) {
        const std::string mode = text + strlen("refix://mode/");
        if (!mode.empty()) RunOnOverlayThread([mode] { SetServerMode(mode.c_str()); });
        return true;
    }
    if (strstr(text, "market") || strstr(text, "community") || strstr(text, "give")) {
        OpenHud(kMarketHud);
        return true;
    }
    return false;
}

} // namespace refix::overlay::csgo_ui
