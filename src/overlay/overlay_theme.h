// Look of the ReFix LAN overlay: fonts, colours, icons and the small widgets every screen shares.
#pragma once
#include "../imgui/imgui.h"
#include <cstdint>
#include <functional>
#include <string>

namespace refix::overlay::ui {

// Segoe Fluent Icons (Windows 11) / Segoe MDL2 Assets (Windows 10) code points.
namespace icon {
constexpr const char* Close = "\xEE\x9C\x91";        // E711
constexpr const char* Back = "\xEE\x9C\xAB";         // E72B
constexpr const char* Star = "\xEE\x9C\xB4";         // E734
constexpr const char* StarFill = "\xEE\x9C\xB5";     // E735
constexpr const char* Join = "\xEE\x9D\xA8";         // E768 play
constexpr const char* Invite = "\xEE\x9C\xA4";       // E724 send
constexpr const char* People = "\xEE\x9C\x96";       // E716
constexpr const char* Store = "\xEE\x9C\x99";        // E719
constexpr const char* Chat = "\xEE\xA3\xB2";         // E8F2
constexpr const char* Guides = "\xEE\x9C\xB6";       // E736
constexpr const char* Workshop = "\xEE\xA4\x8F";     // E90F
constexpr const char* News = "\xEE\x9E\x89";         // E789
constexpr const char* Globe = "\xEE\x9D\xB4";        // E774
constexpr const char* Trophy = "\xEE\x9F\x81";       // E7C1 flag
constexpr const char* Add = "\xEE\x9C\x90";          // E710
constexpr const char* Edit = "\xEE\x9C\x8F";         // E70F
constexpr const char* Switch = "\xEE\xA2\xAB";       // E8AB
constexpr const char* Folder = "\xEE\xA2\xB7";       // E8B7
constexpr const char* Import = "\xEE\xA2\xB5";       // E8B5
constexpr const char* Check = "\xEE\x9C\xBE";        // E73E
constexpr const char* Info = "\xEE\xA5\x86";         // E946
constexpr const char* Warning = "\xEE\x9E\xBA";      // E7BA
constexpr const char* Lan = "\xEE\xA0\xB9";          // E839 ethernet
constexpr const char* Game = "\xEE\x9F\xBC";         // E7FC
constexpr const char* ChevronLeft = "\xEE\x9D\xAB";  // E76B
constexpr const char* ChevronRight = "\xEE\x9D\xAC"; // E76C
constexpr const char* ChevronDown = "\xEE\x9C\x8D";  // E70D
constexpr const char* Search = "\xEE\x9C\xA1";       // E721
constexpr const char* Delete = "\xEE\x9D\x8D";       // E74D trash
constexpr const char* Lock = "\xEE\x9C\xAE";         // E72E
constexpr const char* Language = "\xEE\x9D\xBD";     // E775 globe/keyboard
constexpr const char* Refresh = "\xEE\x9C\xAC";      // E72C
constexpr const char* OpenExternal = "\xEE\xA2\xA7"; // E8A7 open in new window
} // namespace icon

namespace color {
constexpr ImU32 Backdrop = IM_COL32(6, 9, 14, 235);
constexpr ImU32 BackdropSolid = IM_COL32(8, 11, 16, 250);
constexpr ImU32 Panel = IM_COL32(18, 22, 29, 242);
constexpr ImU32 PanelRaised = IM_COL32(26, 31, 40, 255);
constexpr ImU32 Row = IM_COL32(28, 34, 44, 255);
constexpr ImU32 RowHover = IM_COL32(36, 43, 55, 255);
constexpr ImU32 Border = IM_COL32(255, 255, 255, 22);
constexpr ImU32 Text = IM_COL32(232, 237, 244, 255);
constexpr ImU32 Muted = IM_COL32(140, 150, 165, 255);
constexpr ImU32 Faint = IM_COL32(90, 100, 114, 255);
constexpr ImU32 Accent = IM_COL32(43, 212, 164, 255);
constexpr ImU32 AccentHover = IM_COL32(72, 227, 182, 255);
constexpr ImU32 AccentInk = IM_COL32(6, 37, 28, 255);
constexpr ImU32 Invite = IM_COL32(90, 169, 255, 255);
constexpr ImU32 Star = IM_COL32(255, 192, 70, 255);
constexpr ImU32 Danger = IM_COL32(255, 107, 107, 255);
constexpr ImU32 Online = IM_COL32(43, 212, 164, 255);
constexpr ImU32 InGame = IM_COL32(90, 169, 255, 255);
} // namespace color

struct Fonts {
    ImFont* body = nullptr;      // 16 px
    ImFont* caption = nullptr;   // 14.5 px
    ImFont* strong = nullptr;    // 16 px semibold
    ImFont* heading = nullptr;   // 20 px semibold
    ImFont* title = nullptr;     // 28 px semibold
    ImFont* glyph = nullptr;     // 44 px icons only (stock avatars)
    float scale = 1.0f;
};

Fonts& F();
// DPI Scaling Convention:
// - ui::S(px) scales a point/pixel dimension by g_fonts.scale.
// - RULE: Either the caller scales, or the callee scales - NEVER both.
//   * BeginCard(id, pos, size, padding): 'padding' is unscaled points (e.g. 12.0f, 20.0f);
//     BeginCard calls ui::S(padding) internally. Do NOT pass ui::S(12.0f) to BeginCard.
//   * Widget sizes (ImVec2) passed to Button, Dummy, etc. must be scaled by caller via ui::S().
float S(float px);               // scaled pixels
void BuildFonts(float scale);    // rebuilds the atlas; call before NewFrame
void ApplyStyle(float scale);

// Avatar picture: a stock gradient badge or a texture.
struct AvatarRef {
    int stock = 0;
    ImTextureID texture = 0;     // custom picture when non-zero
};
void DrawAvatar(ImDrawList* dl, ImVec2 center, float radius, const AvatarRef& a);
int StockCount();

// Widgets. Ids are taken from the label/icon unless `id` is given.
bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
bool SecondaryButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
bool DangerButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
bool LinkButton(const char* label);
// Round icon button with a tooltip; `tint` colours the glyph when enabled.
bool IconButton(const char* id, const char* glyph, const char* tooltip, ImU32 tint, bool enabled = true,
                float diameter = 0.0f);
void Tooltip(const char* text);
// Pass-through for "was it clicked": plays the select sound when it was.
bool Clicked(bool clicked);
// Invisible button that keyboard/controller navigation can reach.
bool NavButton(const char* id, ImVec2 size);
// Tooltips of the next items open above them (the browser page, a separate window, covers anything below).
void SetTooltipsAbove(bool above);
void GlyphCentered(ImDrawList* dl, ImFont* font, float size, ImVec2 center, ImU32 col, const char* glyph);
void Keycap(const char* text);   // inline "Shift" / "Tab" chip
// Keycap drawn at `leftCenter` (left edge, vertical centre of the line); returns its width.
float KeycapAt(ImDrawList* dl, ImVec2 leftCenter, const char* text, float alpha = 1.0f);
float KeycapHeight();
// Card background for the current window region [min, max].
void Card(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 fill = color::Panel, float rounding = 14.0f);
void TextColored(ImU32 col, const char* text);
// Child window drawn as a Card at screen `pos`; the content area is inset by `padding` (unscaled px).
void BeginCard(const char* id, ImVec2 pos, ImVec2 size, float padding = 20.0f);
void EndCard();
bool BeginCenteredModal(const char* id, float width);
// Small upper-case muted heading, with an optional count ("FAVORITOS Â· 2").
void SectionLabel(const char* text, int count = -1);
ImU32 Fade(ImU32 col, float alpha);
std::string Ellipsize(const std::string& text, float maxWidth);

} // namespace refix::overlay::ui
