#pragma once
#include "../imgui/imgui.h"

namespace refix::overlay {
    // The overlay home's mod grid: one cell per panel the game's mods registered (0 height without any).
    float GetModGridHeight(float width);
    void DrawModGrid(ImVec2 pos, float width, float height);
    // Mod HUD panels opened over the game (e.g. CS:GO's Market & Skins).
    void DrawModHuds(ImVec2 viewport);
    bool AnyModHudOpen();
    void ExecuteEngineClientCommand(const char* cmd);
}
