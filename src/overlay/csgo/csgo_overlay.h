// The CS:GO overlay module: its UI registers through the overlay mod ABI and exists only in CS:GO.
#pragma once

#include "../../../sdk/include/refix_overlay_api.h"
#include <functional>

namespace refix::overlay::csgo_ui {

constexpr const char* kLanCard = "csgo.lan";
constexpr const char* kMarketHud = "csgo.market";
constexpr const char* kServerHud = "csgo.server";

// csgo_overlay_module.cpp
int RegisterOverlay(const RefixOverlayHost* host);
void OpenHud(const char* id);   // closes the overlay and shows the HUD over the game
void CloseHud(const char* id);
bool HandleRequest(uint32_t kind, const char* text);  // the dialogs and URLs only CS:GO sends

// Implemented by the UI files (csgo_lan_card.cpp, csgo_market_ui.cpp, csgo_server_ui.cpp).
float LanCardHeight(float width);
void DrawLanCard(float width, float height);
void DrawMarketHud(float viewportW, float viewportH);
void DrawServerHud(float viewportW, float viewportH);
void SetServerMode(const char* modeId);
void RunOnOverlayThread(std::function<void()> fn);  // App::Post: the overlay thread owns UI state
void CloseOverlayNow();                             // on the overlay thread

} // namespace refix::overlay::csgo_ui
