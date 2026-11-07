// CS:GO item artwork for the overlay, read from the game's own files (loose files first, then pak01 VPKs) so it
// matches whatever build is installed:
//   - painted items: resource/flash/econ/default_generated/<weapon>_<paintkit>_light_large.png when the build ships
//     it; otherwise the paint kit pattern texture (materials/.../customization/paints/*/<pattern>.vtf) as a swatch;
//   - everything else: resource/flash/<image_inventory>.png (or panorama/images/<image_inventory>_png.png).
// Textures are created on the render thread, a few per frame, and cached by the overlay host.
#pragma once
#include <cstdint>
#include <string>
#include "../../imgui/imgui.h"

namespace refix::csgo { struct MarketItem; }

namespace refix::overlay::csgo_icons {

void BeginFrame();                                        // resets the per-frame load budget
ImTextureID ItemImage(const refix::csgo::MarketItem& item); // pre-rendered skin or the base item image
ImTextureID BaseImage(const std::string& imageInventory); // e.g. "econ/weapons/base_weapons/weapon_ak47"
ImTextureID PaintSwatch(const std::string& pattern);      // paint kit pattern texture, 0 when unavailable
bool HasRenderedSkin(const refix::csgo::MarketItem& item);  // the build ships a pre-rendered icon for it

// Draws the item art fitted into [min, max]: rendered icon, or swatch + base image, or nothing.
void DrawItemArt(ImDrawList* dl, const refix::csgo::MarketItem& item, ImVec2 min, ImVec2 max, float rounding);

} // namespace refix::overlay::csgo_icons
