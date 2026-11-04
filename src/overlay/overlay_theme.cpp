#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_theme.h"
#include "overlay_sound.h"
#include "../imgui/imgui_internal.h"

#include <windows.h>
#include <algorithm>
#include <cmath>

namespace refix::overlay::ui {

static bool g_tooltipsAbove = false;   // see SetTooltipsAbove
namespace {

Fonts g_fonts;

struct Stock {
    ImU32 from;
    ImU32 to;
    const char* glyph;
};
// Gradient badge + white glyph. Same order on every PC: only the index travels over the network.
const Stock kStock[] = {
    { IM_COL32(255, 107, 107, 255), IM_COL32(196, 69, 105, 255), "\xEE\x9F\xBC" },  // controller
    { IM_COL32(79, 172, 254, 255), IM_COL32(0, 200, 230, 255), "\xEE\xA6\x9A" },    // robot
    { IM_COL32(247, 151, 30, 255), IM_COL32(255, 210, 0, 255), "\xEE\xA5\x85" },    // bolt
    { IM_COL32(246, 180, 80, 255), IM_COL32(253, 120, 100, 255), "\xEE\x9C\x86" },  // sun
    { IM_COL32(102, 126, 234, 255), IM_COL32(118, 75, 162, 255), "\xEE\x9C\x88" },  // moon
    { IM_COL32(40, 200, 110, 255), IM_COL32(40, 210, 190, 255), "\xEE\xA4\x89" },   // world
    { IM_COL32(250, 112, 154, 255), IM_COL32(240, 190, 60, 255), "\xEE\xAA\x86" },  // puzzle
    { IM_COL32(48, 207, 208, 255), IM_COL32(51, 8, 103, 255), "\xEE\x9D\xAE" },     // smile
    { IM_COL32(161, 140, 209, 255), IM_COL32(230, 150, 210, 255), "\xEE\xA3\x96" }, // music
    { IM_COL32(255, 154, 68, 255), IM_COL32(252, 96, 118, 255), "\xEE\xB5\x96" },   // pizza
    { IM_COL32(220, 110, 250, 255), IM_COL32(245, 87, 108, 255), "\xEE\xAD\x91" },  // heart
    { IM_COL32(11, 163, 96, 255), IM_COL32(60, 186, 146, 255), "\xEE\xA3\xA1" },    // thumbs up
};
static_assert(sizeof(kStock) / sizeof(kStock[0]) == 12, "kStockAvatarCount in profile_store.h");

std::string FontPath(const char* file) {
    char dir[MAX_PATH] = {};
    GetWindowsDirectoryA(dir, MAX_PATH);
    return std::string(dir) + "\\Fonts\\" + file;
}

bool Exists(const std::string& p) { return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

const ImWchar* TextRanges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 (á é í ó ú ñ ¿ ¡)
        0x0100, 0x024F, // Latin Extended-A/B
        0x0370, 0x03FF, // Greek
        0x0400, 0x04FF, // Cyrillic
        0x2000, 0x206F, // punctuation (…, –, ·)
        0x20AC, 0x20AC,
        0,
    };
    return ranges;
}

const ImWchar* IconRanges() {
    static const ImWchar ranges[] = { 0xE700, 0xF8FF, 0 };
    return ranges;
}

ImFont* AddFont(const std::string& text, const std::string& icons, float px, float iconPx) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.PixelSnapH = true;
    ImFont* f = Exists(text) ? io.Fonts->AddFontFromFileTTF(text.c_str(), px, &cfg, TextRanges())
                             : io.Fonts->AddFontDefault();
    if (!icons.empty()) {
        ImFontConfig ic;
        ic.MergeMode = true;
        ic.PixelSnapH = true;
        ic.GlyphOffset = ImVec2(0, iconPx * 0.12f);
        ic.GlyphMinAdvanceX = iconPx;
        io.Fonts->AddFontFromFileTTF(icons.c_str(), iconPx, &ic, IconRanges());
    }
    return f;
}

ImU32 Alpha(ImU32 c, float a) {
    const int alpha = (int)(((c >> IM_COL32_A_SHIFT) & 0xFF) * a);
    return (c & ~IM_COL32_A_MASK) | ((ImU32)alpha << IM_COL32_A_SHIFT);
}

bool StyledButton(const char* label, ImVec2 size, bool enabled, ImU32 fill, ImU32 hover, ImU32 ink, ImU32 border) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(fill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(hover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(hover));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ink));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(border));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, border ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(18), S(10)));
    ImGui::PushFont(F().strong);
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::Button(label, size);
    ImGui::EndDisabled();
    ImGui::PopFont();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
    return Clicked(clicked && enabled);
}

} // namespace

Fonts& F() { return g_fonts; }
float S(float px) { return std::round(px * g_fonts.scale); }
int StockCount() { return (int)(sizeof(kStock) / sizeof(kStock[0])); }

void BuildFonts(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    g_fonts.scale = scale;
    const std::string regular = FontPath("segoeui.ttf");
    const std::string semibold = Exists(FontPath("seguisb.ttf")) ? FontPath("seguisb.ttf") : FontPath("segoeuib.ttf");
    std::string icons = FontPath("SegoeIcons.ttf");
    if (!Exists(icons)) icons = FontPath("segmdl2.ttf");
    if (!Exists(icons)) icons.clear();
    g_fonts.body = AddFont(regular, icons, 16.0f * scale, 16.0f * scale);
    g_fonts.caption = AddFont(regular, icons, 14.5f * scale, 14.0f * scale);
    g_fonts.strong = AddFont(semibold, icons, 16.0f * scale, 16.0f * scale);
    g_fonts.heading = AddFont(semibold, icons, 20.0f * scale, 19.0f * scale);
    g_fonts.title = AddFont(semibold, icons, 28.0f * scale, 26.0f * scale);
    if (!icons.empty()) {
        ImFontConfig cfg;
        cfg.PixelSnapH = true;
        g_fonts.glyph = io.Fonts->AddFontFromFileTTF(icons.c_str(), 44.0f * scale, &cfg, IconRanges());
    }
    if (!g_fonts.glyph) g_fonts.glyph = g_fonts.title;
    io.FontDefault = g_fonts.body;
    io.Fonts->Build();
}

void ApplyStyle(float scale) {
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.WindowRounding = 14.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 10.0f;
    style.PopupRounding = 12.0f;
    style.GrabRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(20, 20);
    style.FramePadding = ImVec2(12, 9);
    style.ItemSpacing = ImVec2(10, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 10.0f;
    style.SeparatorTextBorderSize = 1.0f;
    ImVec4* c = style.Colors;
    auto v = [](ImU32 u) { return ImGui::ColorConvertU32ToFloat4(u); };
    c[ImGuiCol_Text] = v(color::Text);
    c[ImGuiCol_TextDisabled] = v(color::Faint);
    c[ImGuiCol_WindowBg] = v(color::Panel);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = v(color::PanelRaised);
    c[ImGuiCol_Border] = v(color::Border);
    c[ImGuiCol_FrameBg] = v(IM_COL32(10, 13, 18, 255));
    c[ImGuiCol_FrameBgHovered] = v(IM_COL32(14, 18, 25, 255));
    c[ImGuiCol_FrameBgActive] = v(IM_COL32(14, 18, 25, 255));
    c[ImGuiCol_Button] = v(color::Row);
    c[ImGuiCol_ButtonHovered] = v(color::RowHover);
    c[ImGuiCol_ButtonActive] = v(color::RowHover);
    c[ImGuiCol_Header] = v(color::Row);
    c[ImGuiCol_HeaderHovered] = v(color::RowHover);
    c[ImGuiCol_HeaderActive] = v(color::RowHover);
    c[ImGuiCol_CheckMark] = v(color::Accent);
    c[ImGuiCol_SliderGrab] = v(color::Accent);
    c[ImGuiCol_TextSelectedBg] = v(Alpha(color::Accent, 0.35f));
    c[ImGuiCol_NavCursor] = v(color::Accent);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = v(IM_COL32(255, 255, 255, 40));
    c[ImGuiCol_ScrollbarGrabHovered] = v(IM_COL32(255, 255, 255, 70));
    c[ImGuiCol_Separator] = v(color::Border);
    c[ImGuiCol_ModalWindowDimBg] = v(IM_COL32(0, 0, 0, 120));
    style.ScaleAllSizes(scale);
    ImGui::GetStyle() = style;
}

void DrawAvatar(ImDrawList* dl, ImVec2 c, float r, const AvatarRef& a) {
    if (a.texture) {
        dl->AddImageRounded(a.texture, ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), ImVec2(0, 0), ImVec2(1, 1),
                            IM_COL32_WHITE, r);
        dl->AddCircle(c, r, IM_COL32(255, 255, 255, 30), 0, 1.0f);
        return;
    }
    const Stock& s = kStock[std::clamp(a.stock, 0, StockCount() - 1)];
    const int first = dl->VtxBuffer.Size;
    dl->AddCircleFilled(c, r, IM_COL32_WHITE, 48);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, first, dl->VtxBuffer.Size, ImVec2(c.x - r, c.y - r),
                                                  ImVec2(c.x + r, c.y + r), s.from, s.to);
    GlyphCentered(dl, F().glyph, r * 1.05f, c, IM_COL32(255, 255, 255, 235), s.glyph);
}

bool PrimaryButton(const char* label, ImVec2 size, bool enabled) {
    return StyledButton(label, size, enabled, color::Accent, color::AccentHover, color::AccentInk, 0);
}

bool SecondaryButton(const char* label, ImVec2 size, bool enabled) {
    return StyledButton(label, size, enabled, color::Row, color::RowHover, color::Text, color::Border);
}

bool DangerButton(const char* label, ImVec2 size, bool enabled) {
    return StyledButton(label, size, enabled, color::Danger, IM_COL32(255, 135, 135, 255), IM_COL32(40, 8, 8, 255), 0);
}

bool LinkButton(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::Muted));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::CalcTextSize(label);
    const bool clicked = NavButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::GetWindowDrawList()->AddText(pos, hovered ? color::Text : color::Muted, label);
    if (hovered) {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + size.y), ImVec2(pos.x + size.x, pos.y + size.y), color::Muted);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImGui::PopStyleColor();
    return Clicked(clicked);
}

void SetTooltipsAbove(bool above) { g_tooltipsAbove = above; }

// InvisibleButton is left out of keyboard/controller navigation unless asked (ImGui 1.91): every
// clickable surface of the overlay goes through here so the D-pad reaches it.
bool NavButton(const char* id, ImVec2 size) { return ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav); }

bool Clicked(bool clicked) {
    if (clicked) sound::Play(sound::Cue::Select);
    return clicked;
}

// Centres a single glyph on its drawn box. CalcTextSize uses the line height of the font the icons are
// merged into, which put icons below and to the side of the centre.
void GlyphCentered(ImDrawList* dl, ImFont* font, float size, ImVec2 center, ImU32 col, const char* glyph) {
    unsigned int cp = 0;
    ImTextCharFromUtf8(&cp, glyph, nullptr);
    const ImFontGlyph* g = font->FindGlyphNoFallback((ImWchar)cp);
    if (!g) {
        const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph);
        dl->AddText(font, size, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), col, glyph);
        return;
    }
    const float k = size / font->FontSize;
    const ImVec2 mid((g->X0 + g->X1) * 0.5f * k, (g->Y0 + g->Y1) * 0.5f * k);
    dl->AddText(font, size, ImVec2(std::round(center.x - mid.x), std::round(center.y - mid.y)), col, glyph);
}

void Tooltip(const char* text) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayShort)) return;
    if (g_tooltipsAbove) {
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        ImGui::SetNextWindowPos(ImVec2((mn.x + mx.x) * 0.5f, mn.y - S(6)), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(10), S(6)));
    ImGui::PushFont(F().caption);
    ImGui::SetTooltip("%s", text);
    ImGui::PopFont();
    ImGui::PopStyleVar();
}

bool IconButton(const char* id, const char* glyph, const char* tooltip, ImU32 tint, bool enabled, float diameter) {
    const float d = diameter > 0 ? diameter : S(34);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    const bool clicked = NavButton("##icon", ImVec2(d, d)) && enabled;
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    const bool held = ImGui::IsItemActive();
    Tooltip(tooltip);
    ImGui::PopID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 c(pos.x + d * 0.5f, pos.y + d * 0.5f);
    if (enabled) {
        dl->AddCircleFilled(c, d * 0.5f, hovered ? Alpha(tint, held ? 0.32f : 0.22f) : Alpha(tint, 0.10f), 32);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    } else {
        dl->AddCircle(c, d * 0.5f - 0.5f, IM_COL32(255, 255, 255, 18), 32, 1.0f);
    }
    GlyphCentered(dl, F().body, d * 0.48f, c, enabled ? tint : color::Faint, glyph);
    return Clicked(clicked);
}

void Keycap(const char* text) {
    ImGui::PushFont(F().caption);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    const ImVec2 pad(S(7), S(2));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 max(pos.x + ts.x + pad.x * 2, pos.y + ts.y + pad.y * 2);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(pos.x, pos.y + S(2)), ImVec2(max.x, max.y + S(2)), IM_COL32(0, 0, 0, 90), S(6));
    dl->AddRectFilled(pos, max, IM_COL32(48, 56, 70, 255), S(6));
    dl->AddRect(pos, max, IM_COL32(255, 255, 255, 30), S(6));
    dl->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y), color::Text, text);
    ImGui::Dummy(ImVec2(max.x - pos.x, max.y - pos.y + S(2)));
    ImGui::PopFont();
}

float KeycapHeight() { return F().caption->FontSize + S(4); }

float KeycapAt(ImDrawList* dl, ImVec2 leftCenter, const char* text, float alpha) {
    ImFont* font = F().caption;
    const ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, text);
    const float padX = S(7);
    const float h = KeycapHeight();
    const ImVec2 pos(leftCenter.x, leftCenter.y - h * 0.5f);
    const ImVec2 max(pos.x + ts.x + padX * 2, pos.y + h);
    dl->AddRectFilled(ImVec2(pos.x, pos.y + S(2)), ImVec2(max.x, max.y + S(2)), Fade(IM_COL32(0, 0, 0, 90), alpha), S(6));
    dl->AddRectFilled(pos, max, Fade(IM_COL32(48, 56, 70, 255), alpha), S(6));
    dl->AddRect(pos, max, Fade(IM_COL32(255, 255, 255, 30), alpha), S(6));
    dl->AddText(font, font->FontSize, ImVec2(pos.x + padX, leftCenter.y - ts.y * 0.5f), Fade(color::Text, alpha), text);
    return max.x - pos.x;
}

void Card(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 fill, float rounding) {
    const float r = S(rounding);
    for (int i = 3; i >= 1; --i) {
        const float o = S(3.0f * i);
        dl->AddRectFilled(ImVec2(min.x - o * 0.3f, min.y + o * 0.2f), ImVec2(max.x + o * 0.3f, max.y + o),
                          IM_COL32(0, 0, 0, 22), r + o);
    }
    dl->AddRectFilled(min, max, fill, r);
    dl->AddRect(min, max, color::Border, r, 0, 1.0f);
    // Light catch on the top edge.
    dl->AddLine(ImVec2(min.x + r, min.y + 0.5f), ImVec2(max.x - r, min.y + 0.5f), IM_COL32(255, 255, 255, 18));
}

void TextColored(ImU32 col, const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void BeginCard(const char* id, ImVec2 pos, ImVec2 size, float padding) {
    Card(ImGui::GetWindowDrawList(), pos, ImVec2(pos.x + size.x, pos.y + size.y));
    const float pad = S(padding);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, pos.y + pad));
    ImGui::BeginChild(id, ImVec2(size.x - pad * 2, size.y - pad * 2), ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void EndCard() { ImGui::EndChild(); }

void SectionLabel(const char* text, int count) {
    std::string label = text;
    for (char& c : label) c = (char)toupper((unsigned char)c);
    if (count >= 0) label += "  ·  " + std::to_string(count);
    ImGui::PushFont(F().caption);
    TextColored(color::Faint, label.c_str());
    ImGui::PopFont();
}

ImU32 Fade(ImU32 col, float alpha) { return Alpha(col, std::clamp(alpha, 0.0f, 1.0f)); }

std::string Ellipsize(const std::string& text, float maxWidth) {
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;
    std::string s = text;
    const char* dots = "\xE2\x80\xA6";
    while (!s.empty()) {
        // Drop one UTF-8 code point.
        size_t cut = s.size() - 1;
        while (cut > 0 && ((unsigned char)s[cut] & 0xC0) == 0x80) --cut;
        s.resize(cut);
        if (ImGui::CalcTextSize((s + dots).c_str()).x <= maxWidth) break;
    }
    return s + dots;
}

} // namespace refix::overlay::ui
