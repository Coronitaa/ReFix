// Keeps the Steam overlay out of games running with ReFix LAN, including games started from Big Picture,
// without touching any code: its Vulkan layer is disabled through the loader variable
// DISABLE_VK_LAYER_VALVE_steam_overlay_1, and SteamNoOverlayUIDrawing tells the D3D/GL renderer not to draw
// its UI. The ReFix overlay answers Shift+Tab and every overlay request the game makes.
#pragma once

namespace refix::overlay {

// Sets the environment switches. Idempotent and DllMain-safe.
void ArmSteamOverlayBlock();

} // namespace refix::overlay
