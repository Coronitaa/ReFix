// Settings modal of the overlay (gear in the top bar). A sidebar of sections (notifications, sound, language,
// performance, profile sync) and the chosen section on the right. Each row is one switch with a plain
// explanation; changes save at once (overlay_settings.cpp).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "nvidia_reflex_block.h"
#include "overlay_i18n.h"
#include "overlay_settings.h"
#include "overlay_text.h"
#include "profile_sync.h"
#include "cloud_account.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace refix::overlay {
namespace {

constexpr float kWidth = 920.0f;
constexpr float kMaxHeight = 680.0f;
constexpr float kHeaderH = 64.0f;
constexpr float kSidebarW = 224.0f;
constexpr float kNavItemH = 40.0f;
constexpr float kMarginX = 56.0f;
constexpr float kMarginY = 40.0f;
constexpr float kRowPad = 14.0f;
constexpr float kRowGap = 6.0f;
constexpr float kSwitchW = 46.0f;
constexpr float kSwitchH = 26.0f;
constexpr float kKnobSpeed = 10.0f;   // switch animation, fraction per second
constexpr ULONGLONG kIntroMs = 180;

float EaseOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

ImU32 Mix(ImU32 a, ImU32 b, float t) {
    const ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}

// Pill switch whose knob slides to its new side.
void Switch(ImDrawList* dl, ImGuiID id, ImVec2 rightCenter, bool on, bool enabled) {
    float* t = ImGui::GetStateStorage()->GetFloatRef(id, on ? 1.0f : 0.0f);
    const float step = ImGui::GetIO().DeltaTime * kKnobSpeed;
    *t = on ? std::min(1.0f, *t + step) : std::max(0.0f, *t - step);
    const float w = ui::S(kSwitchW), h = ui::S(kSwitchH), r = h * 0.5f;
    const ImVec2 p0(rightCenter.x - w, rightCenter.y - r), p1(rightCenter.x, rightCenter.y + r);
    const ImU32 offTrack = IM_COL32(58, 66, 80, 255);
    dl->AddRectFilled(p0, p1, enabled ? Mix(offTrack, ui::color::Accent, *t) : IM_COL32(40, 46, 56, 255), r);
    const float knobR = r - ui::S(3);
    const ImVec2 kc(p0.x + r + (w - h) * *t, rightCenter.y);
    dl->AddCircleFilled(kc, knobR, enabled ? IM_COL32(245, 248, 252, 255) : IM_COL32(120, 128, 140, 255), 24);
}

// One setting: glyph, title, explanation, switch. The whole row toggles (mouse, Enter, controller A).
bool SettingRow(const char* id, const char* title, const char* desc, bool* value, bool enabled = true, const char* note = nullptr) {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float pad = ui::S(kRowPad);
    const float textW = width - pad * 3 - ui::S(kSwitchW);
    const ImVec2 descSize = f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, textW, desc);
    const ImVec2 noteSize = note ? f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, textW, note) : ImVec2(0, 0);
    const float h = pad * 2 + f.strong->FontSize + ui::S(4) + descSize.y + (note ? ui::S(6) + noteSize.y : 0.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + h);
    bool changed = false;
    ImGui::PushID(id);
    if (ui::Clicked(ui::NavButton("##row", ImVec2(width, h))) && enabled) {
        *value = !*value;
        changed = true;
    }
    const bool hover = ImGui::IsItemHovered();
    const ImGuiID switchId = ImGui::GetID("##switch");
    ImGui::PopID();
    if (hover && enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    dl->AddRectFilled(p0, p1, hover && enabled ? ui::color::RowHover : ui::color::Row, ui::S(12));
    float y = p0.y + pad;
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(p0.x + pad, y), enabled ? ui::color::Text : ui::color::Muted, title);
    y += f.strong->FontSize + ui::S(4);
    dl->AddText(f.caption, f.caption->FontSize, ImVec2(p0.x + pad, y), ui::color::Muted, desc, nullptr, textW);
    if (note) {
        y += descSize.y + ui::S(6);
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(p0.x + pad, y), ui::color::Star, note, nullptr, textW);
    }
    Switch(dl, switchId, ImVec2(p1.x - pad, p0.y + h * 0.5f), *value, enabled);
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + ui::S(kRowGap)));
    ImGui::Dummy(ImVec2(0, 0));
    return changed;
}

// One option of a set (a radio row): the chosen one carries a check.
bool ChoiceRow(const char* id, const char* title, const char* desc, bool selected) {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x, pad = ui::S(kRowPad);
    const float textW = width - pad * 3 - ui::S(24);
    const ImVec2 descSize = f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, textW, desc);
    const float h = pad * 2 + f.strong->FontSize + ui::S(4) + descSize.y;
    const ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + width, p0.y + h);
    ImGui::PushID(id);
    const bool clicked = ui::Clicked(ui::NavButton("##choice", ImVec2(width, h)));
    const bool hover = ImGui::IsItemHovered();
    ImGui::PopID();
    if (hover && !selected) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    dl->AddRectFilled(p0, p1, selected ? ui::color::AccentSoft : (hover ? ui::color::RowHover : ui::color::Row), ui::S(12));
    if (selected) dl->AddRect(p0, p1, ui::color::Accent, ui::S(12), 0, ui::S(1.5f));
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(p0.x + pad, p0.y + pad), selected ? ui::color::Accent : ui::color::Text, title);
    dl->AddText(f.caption, f.caption->FontSize, ImVec2(p0.x + pad, p0.y + pad + f.strong->FontSize + ui::S(4)), ui::color::Muted,
                desc, nullptr, textW);
    const ImVec2 c(p1.x - pad - ui::S(10), p0.y + h * 0.5f);
    dl->AddCircle(c, ui::S(10), selected ? ui::color::Accent : ui::color::Faint, 24, ui::S(2));
    if (selected) dl->AddCircleFilled(c, ui::S(5), ui::color::Accent, 24);
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + ui::S(kRowGap)));
    ImGui::Dummy(ImVec2(0, 0));
    return clicked && !selected;
}

void SectionHeader(const char* title, const char* subtitle) {
    ImGui::PushFont(ui::F().heading);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::PushFont(ui::F().caption);
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(ui::color::Muted, subtitle);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(10)));
}

struct NavEntry { int id; const char* glyph; const char* en; const char* es; };
struct NavGroup { const char* en; const char* es; NavEntry items[4]; int count; };

// Sidebar entry: icon and label; the open section is tinted with the accent.
bool NavItem(const NavEntry& e, bool active, float width) {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ui::S(kNavItemH);
    const ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + width, p0.y + h);
    ImGui::PushID(e.id);
    const bool clicked = ui::Clicked(ui::NavButton("##nav", ImVec2(width, h)));
    const bool hover = ImGui::IsItemHovered();
    ImGui::PopID();
    if (hover && !active) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (active) {
        dl->AddRectFilled(p0, p1, ui::color::AccentSoft, ui::S(10));
        dl->AddRect(p0, p1, ui::color::AccentLine, ui::S(10));
    } else if (hover) {
        dl->AddRectFilled(p0, p1, ui::color::RowHover, ui::S(10));
    }
    const ImU32 ink = active ? ui::color::Accent : (hover ? ui::color::Text : ui::color::Muted);
    const float cy = p0.y + h * 0.5f;
    dl->AddText(f.body, f.body->FontSize, ImVec2(p0.x + ui::S(14), cy - f.body->FontSize * 0.5f), ink, e.glyph);
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(p0.x + ui::S(44), cy - f.strong->FontSize * 0.55f), active ? ui::color::Accent : ui::color::Text,
                i18n::T(e.en, e.es));
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + ui::S(4)));
    ImGui::Dummy(ImVec2(0, 0));
    return clicked;
}

enum SettingsSection { kGeneral, kNotifications, kSound, kLanguage, kPerformance, kSync };   // App::settingsSection_

void SyncSection(const char* glyph, const char* text) {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::Dummy(ImVec2(0, ui::S(10)));
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x, h = f.strong->FontSize;
    dl->AddText(f.body, f.body->FontSize, ImVec2(p0.x, p0.y + (h - f.body->FontSize) * 0.5f), ui::color::Accent, glyph);
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(p0.x + ui::S(26), p0.y), ui::color::Text, text);
    const float tx = p0.x + ui::S(26) + f.strong->CalcTextSizeA(f.strong->FontSize, FLT_MAX, 0, text).x + ui::S(12);
    dl->AddLine(ImVec2(tx, p0.y + h * 0.55f), ImVec2(p0.x + w, p0.y + h * 0.55f), ui::color::Border);
    ImGui::Dummy(ImVec2(w, h + ui::S(8)));
}

enum class OverlayAction { None, Steam, DisableAll };
static OverlayAction g_pendingOverlayAction = OverlayAction::None;
static bool g_overlayWarn1Open = false;
static bool g_overlayWarn2Open = false;
static float g_overlayHoldTime = 0.0f;

bool ComboRow(const char* id, const char* title, const char* desc, const char* currentLabel,
              const std::vector<std::string>& items, int* selectedIndex) {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float pad = ui::S(kRowPad);
    const float comboW = ui::S(220);
    const float textW = width - comboW - pad * 3;
    const ImVec2 descSize = f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, textW, desc);
    const float textH = pad * 2 + f.strong->FontSize + ui::S(4) + descSize.y;
    const float comboH = ui::S(36);
    const float h = std::max(textH, pad * 2 + comboH);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + h);

    dl->AddRectFilled(p0, p1, ui::color::Row, ui::S(12));
    float y = p0.y + pad;
    dl->AddText(f.strong, f.strong->FontSize, ImVec2(p0.x + pad, y), ui::color::Text, title);
    y += f.strong->FontSize + ui::S(4);
    dl->AddText(f.caption, f.caption->FontSize, ImVec2(p0.x + pad, y), ui::color::Muted, desc, nullptr, textW);

    bool changed = false;
    const float comboY = p0.y + (h - comboH) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(p1.x - pad - comboW, comboY));
    ImGui::SetNextItemWidth(comboW);

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ui::color::PanelRaised);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ui::color::RowHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ui::color::AccentSoft);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::color::PanelRaised);
    ImGui::PushStyleColor(ImGuiCol_Border, ui::color::Border);
    ImGui::PushStyleColor(ImGuiCol_Header, ui::color::AccentSoft);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ui::color::RowHover);
    ImGui::PushStyleColor(ImGuiCol_Text, ui::color::Text);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::S(8));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(8));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ui::S(12), ui::S(7)));

    if (ImGui::BeginCombo("##combo", currentLabel)) {
        for (int i = 0; i < (int)items.size(); ++i) {
            const bool isSelected = (*selectedIndex == i);
            if (ImGui::Selectable(items[i].c_str(), isSelected)) {
                *selectedIndex = i;
                changed = true;
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(8);
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + ui::S(kRowGap)));
    ImGui::Dummy(ImVec2(0, 0));
    return changed;
}

void DrawFormattedNotice(const char* text) {
    const ui::Fonts& f = ui::F();
    std::string str = text;
    size_t lineStart = 0;
    while (lineStart < str.size()) {
        size_t lineEnd = str.find('\n', lineStart);
        if (lineEnd == std::string::npos) lineEnd = str.size();
        std::string line = str.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;

        if (line.empty()) {
            ImGui::Dummy(ImVec2(0, ui::S(4)));
            continue;
        }

        size_t tokenStart = 0;
        bool isFirst = true;
        while (tokenStart < line.size()) {
            size_t bPos = line.find("**", tokenStart);
            if (bPos == std::string::npos) {
                std::string chunk = line.substr(tokenStart);
                if (!chunk.empty()) {
                    if (!isFirst) ImGui::SameLine(0, 0);
                    ImGui::PushFont(f.body);
                    ui::TextColored(ui::color::Text, chunk.c_str());
                    ImGui::PopFont();
                    isFirst = false;
                }
                break;
            } else {
                if (bPos > tokenStart) {
                    std::string chunk = line.substr(tokenStart, bPos - tokenStart);
                    if (!chunk.empty()) {
                        if (!isFirst) ImGui::SameLine(0, 0);
                        ImGui::PushFont(f.body);
                        ui::TextColored(ui::color::Text, chunk.c_str());
                        ImGui::PopFont();
                        isFirst = false;
                    }
                }
                size_t closePos = line.find("**", bPos + 2);
                if (closePos == std::string::npos) {
                    std::string chunk = line.substr(bPos);
                    if (!isFirst) ImGui::SameLine(0, 0);
                    ImGui::PushFont(f.body);
                    ui::TextColored(ui::color::Text, chunk.c_str());
                    ImGui::PopFont();
                    break;
                }
                std::string boldChunk = line.substr(bPos + 2, closePos - (bPos + 2));
                if (!boldChunk.empty()) {
                    if (!isFirst) ImGui::SameLine(0, 0);
                    ImGui::PushFont(f.strong);
                    ui::TextColored(ui::color::Star, boldChunk.c_str());
                    ImGui::PopFont();
                    isFirst = false;
                }
                tokenStart = closePos + 2;
            }
        }
    }
}

} // namespace

void App::DrawSettingsModal(ImVec2 viewport) {
    if (settingsRequested_) {
        settingsRequested_ = false;
        settingsOpen_ = true;
        settingsOpenedAt_ = GetTickCount64();
    }
    if (!settingsOpen_) return;
    if (!ImGui::IsPopupOpen("##settings")) ImGui::OpenPopup("##settings");
    const ImVec2 size(std::min(ui::S(kWidth), viewport.x - 2 * ui::S(kMarginX)),
                      std::min(ui::S(kMaxHeight), viewport.y - 2 * ui::S(kMarginY)));
    const float intro = EaseOut((float)(GetTickCount64() - settingsOpenedAt_) / kIntroMs);
    ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f, viewport.y * 0.5f + (1 - intro) * ui::S(18)), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.2f + 0.8f * intro);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(16));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::color::Panel);
    const bool open = ImGui::BeginPopupModal("##settings", nullptr,
                                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!open) {
        settingsOpen_ = false;
        return;
    }

    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const ImVec2 full = ImGui::GetContentRegionAvail();
    const float headerH = ui::S(kHeaderH), sideW = ui::S(kSidebarW);
    bool close = false;

    // Header: "<game> > Settings", close button on the right.
    {
        const std::string game = gameTitle_.empty() ? std::string(i18n::T("Game", "Juego")) : ToUtf8(gameTitle_);
        const float cy = o.y + headerH * 0.5f;
        ImGui::PushFont(f.heading);
        const std::string shown = ui::Ellipsize(game, full.x * 0.5f);
        ImGui::PopFont();
        float x = o.x + ui::S(24);
        dl->AddText(f.heading, f.heading->FontSize, ImVec2(x, cy - f.heading->FontSize * 0.55f), ui::color::Muted, shown.c_str());
        x += f.heading->CalcTextSizeA(f.heading->FontSize, FLT_MAX, 0, shown.c_str()).x + ui::S(12);
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(x, cy - f.caption->FontSize * 0.5f), ui::color::Faint, ui::icon::ChevronRight);
        x += ui::S(24);
        dl->AddText(f.heading, f.heading->FontSize, ImVec2(x, cy - f.heading->FontSize * 0.55f), ui::color::Text,
                    i18n::T("Settings", "Ajustes"));
        const float btn = ui::S(36);
        ImGui::SetCursorScreenPos(ImVec2(o.x + full.x - ui::S(20) - btn, cy - btn * 0.5f));
        if (ui::IconButton("settingsclose", ui::icon::Close, i18n::T("Close (Esc)", "Cerrar (Esc)"), ui::color::Text, true, btn)) close = true;
        dl->AddLine(ImVec2(o.x, o.y + headerH), ImVec2(o.x + full.x, o.y + headerH), ui::color::Border);
        dl->AddLine(ImVec2(o.x + sideW, o.y + headerH), ImVec2(o.x + sideW, o.y + full.y), ui::color::Border);
    }

    // Sidebar.
    static const NavGroup kGroups[] = {
        { "OVERLAY", "OVERLAY", { { kGeneral, ui::icon::Settings, "General", "General" },
                                  { kNotifications, ui::icon::Bell, "Notifications", "Notificaciones" },
                                  { kSound, ui::icon::Volume, "Sound", "Sonido" },
                                  { kLanguage, ui::icon::Globe, "Language", "Idioma" } }, 4 },
        { "GAME", "JUEGO", { { kPerformance, ui::icon::Speed, "Performance", "Rendimiento" } }, 1 },
        { "PROFILES", "PERFILES", { { kSync, ui::icon::Refresh, "Syncing", "Sincronización" } }, 1 },
    };
    ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + headerH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(16), ui::S(18)));
    ImGui::BeginChild("##settingsnav", ImVec2(sideW, full.y - headerH), ImGuiChildFlags_NavFlattened | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    const float navW = ImGui::GetContentRegionAvail().x;
    for (const NavGroup& g : kGroups) {
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Faint, i18n::T(g.en, g.es));
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        for (int i = 0; i < g.count; ++i)
            if (NavItem(g.items[i], settingsSection_ == g.items[i].id, navW)) settingsSection_ = g.items[i].id;
        ImGui::Dummy(ImVec2(0, ui::S(10)));
    }
    // Restore defaults sits at the bottom of the sidebar (changes save at once, so there is no "Save").
    const float linkH = f.body->FontSize + ui::S(8);
    if (ImGui::GetCursorPosY() < ImGui::GetWindowHeight() - linkH - ui::S(18))
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - linkH - ui::S(18));
    bool restore = false;
    if (ui::LinkButton(i18n::T("Restore defaults", "Restablecer valores"))) restore = true;
    ImGui::EndChild();

    // Content of the chosen section.
    ImGui::SetCursorScreenPos(ImVec2(o.x + sideW + 1, o.y + headerH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(28), ui::S(22)));
    ImGui::BeginChild("##settingsbody", ImVec2(full.x - sideW - 1, full.y - headerH),
                      ImGuiChildFlags_NavFlattened | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    settings::Values v = settings::Get();
    bool changed = false;
    switch (settingsSection_) {
    case kGeneral: {
        SectionHeader(i18n::T("General", "General"),
                      i18n::T("Overlay shortcut, display mode, game behavior and options.",
                              "Atajo del overlay, modo de pantalla, comportamiento del juego y opciones."));

        SyncSection(ui::icon::Settings, i18n::T("Overlay Shortcut", "Atajo del Overlay"));
        std::vector<std::string> scItems;
        for (int i = 0; i <= (int)settings::OverlayShortcut::Insert; ++i) {
            auto info = settings::GetShortcutInfo((settings::OverlayShortcut)i);
            scItems.push_back(i18n::Current() == i18n::Lang::Es ? info.labelEs : info.label);
        }
        int curSc = (int)v.shortcut;
        auto curInfo = settings::GetShortcutInfo(v.shortcut);
        const char* curScLabel = i18n::Current() == i18n::Lang::Es ? curInfo.labelEs : curInfo.label;
        if (ComboRow("sc_combo", i18n::T("Keyboard shortcut", "Combinación de teclas"),
                     i18n::T("Key combination used to open and close the overlay in-game.",
                             "Combinación de teclas para abrir y cerrar el overlay dentro del juego."),
                     curScLabel, scItems, &curSc)) {
            v.shortcut = static_cast<settings::OverlayShortcut>(curSc);
            changed = true;
        }

        SyncSection(ui::icon::Game, i18n::T("While Overlay is Open", "Mientras el overlay esté abierto"));
        std::vector<std::string> behItems = {
            i18n::T("Keep game active", "Mantener activo"),
            i18n::T("Mute game audio", "Silenciar audio"),
            i18n::T("Pause game", "Pausar juego")
        };
        int curBeh = (int)v.openBehavior;
        const char* behDesc = "";
        if (v.openBehavior == settings::GameOpenBehavior::KeepActive) {
            behDesc = i18n::T("The game keeps running and audio plays normally while the overlay is open.",
                              "El juego sigue corriendo y el audio sigue sonando normalmente mientras el overlay está abierto.");
        } else if (v.openBehavior == settings::GameOpenBehavior::Mute) {
            behDesc = i18n::T("The game keeps running in the background, but audio is muted while the overlay is open.",
                              "El juego sigue corriendo en segundo plano, pero el audio se silencia mientras el overlay está abierto.");
        } else {
            behDesc = i18n::T("Pauses singleplayer games and mutes audio while the overlay is open.",
                              "Pausa las partidas de un jugador y silencia el audio mientras el overlay está abierto.");
        }
        if (ComboRow("beh_combo", i18n::T("Open overlay behavior", "Comportamiento del juego"),
                     behDesc, behItems[curBeh].c_str(), behItems, &curBeh)) {
            v.openBehavior = static_cast<settings::GameOpenBehavior>(curBeh);
            changed = true;
        }

        SyncSection(ui::icon::OpenExternal, i18n::T("Launch Screen Mode", "Modo de pantalla al ejecutar"));
        std::vector<std::string> dmItems = {
            i18n::T("Fullscreen Windowed (Default)", "Ventana sin bordes (Predet.)"),
            i18n::T("Fullscreen", "Pantalla completa"),
            i18n::T("Windowed", "Modo ventana")
        };
        int curDm = (int)v.displayMode;
        const char* dmDesc = "";
        if (v.displayMode == settings::DisplayMode::FullscreenWindowed) {
            dmDesc = i18n::T("Runs the game in a borderless window matching your display resolution.",
                             "Ejecuta el juego en una ventana sin bordes adaptada a la resolución del monitor.");
        } else if (v.displayMode == settings::DisplayMode::Fullscreen) {
            dmDesc = i18n::T("Runs the game in exclusive fullscreen mode.",
                             "Ejecuta el juego en modo de pantalla completa exclusiva.");
        } else {
            dmDesc = i18n::T("Runs the game in a bordered window with selectable resolution.",
                             "Ejecuta el juego en modo ventana permitiendo elegir la resolución deseada.");
        }
        if (ComboRow("dm_combo", i18n::T("Screen mode", "Modo de pantalla"),
                     dmDesc, dmItems[curDm].c_str(), dmItems, &curDm)) {
            v.displayMode = static_cast<settings::DisplayMode>(curDm);
            changed = true;
        }

        if (v.displayMode == settings::DisplayMode::Windowed) {
            struct ResOption { int w; int h; const char* label; };
            static const ResOption kResList[] = {
                { 1280, 720, "1280 x 720 (16:9 HD)" },
                { 1366, 768, "1366 x 768 (16:9)" },
                { 1600, 900, "1600 x 900 (16:9 HD+)" },
                { 1920, 1080, "1920 x 1080 (16:9 Full HD)" },
                { 2560, 1440, "2560 x 1440 (16:9 2K QHD)" },
                { 3840, 2160, "3840 x 2160 (16:9 4K UHD)" }
            };
            std::vector<std::string> resItems;
            int curResIdx = 3;
            for (size_t i = 0; i < sizeof(kResList) / sizeof(kResList[0]); ++i) {
                resItems.push_back(kResList[i].label);
                if (kResList[i].w == v.windowWidth && kResList[i].h == v.windowHeight) {
                    curResIdx = (int)i;
                }
            }
            if (ComboRow("res_combo", i18n::T("Window resolution", "Resolución de ventana"),
                         i18n::T("Target resolution applied when launching or restarting the game.",
                                 "Resolución objetivo aplicada al iniciar o reiniciar el juego."),
                         resItems[curResIdx].c_str(), resItems, &curResIdx)) {
                v.windowWidth = kResList[curResIdx].w;
                v.windowHeight = kResList[curResIdx].h;
                changed = true;
            }
        }

        SyncSection(ui::icon::Switch, i18n::T("Overlay Mode", "Modo del Overlay"));
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Muted,
                        i18n::T("Configure whether to use ReFix overlay, official Steam overlay, or disable in-game overlay entirely.",
                                "Configura si usar el overlay de ReFix, el oficial de Steam o desactivar el overlay por completo."));
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, ui::S(8)));

        // Horizontally aligned action buttons
        const float availW = ImGui::GetContentRegionAvail().x;
        const float gap = ui::S(12);
        const float btnW = (availW - gap) * 0.5f;
        const float btnH = ui::S(40);

        if (ui::PrimaryButton(i18n::T("Replace with Steam Overlay", "Reemplazar por Overlay de Steam"), ImVec2(btnW, btnH))) {
            g_pendingOverlayAction = OverlayAction::Steam;
            g_overlayWarn1Open = true;
            settingsOpen_ = false;
        }
        ImGui::SameLine(0, gap);
        if (ui::DangerButton(i18n::T("Disable overlay completely", "Desactivar overlay completamente"), ImVec2(btnW, btnH))) {
            g_pendingOverlayAction = OverlayAction::DisableAll;
            g_overlayWarn1Open = true;
            settingsOpen_ = false;
        }
        break;
    }
    case kNotifications:
        SectionHeader(i18n::T("Notifications", "Notificaciones"),
                      i18n::T("What ReFix tells you while you play.", "Lo que ReFix te avisa mientras juegas."));
        changed |= SettingRow("ach", i18n::T("Unlocked achievements", "Logros desbloqueados"),
                              i18n::T("A notice in the corner with the achievement and how rare it is.",
                                      "Aviso en la esquina con el logro y su rareza."), &v.achievementToasts);
        changed |= SettingRow("progress", i18n::T("Achievement progress", "Progreso de logros"),
                              i18n::T("A notice when an achievement moves forward, like 3/10.",
                                      "Aviso cuando un logro avanza, por ejemplo 3/10."), &v.progressToasts);
        changed |= SettingRow("invite", i18n::T("Invitations", "Invitaciones"),
                              i18n::T("A notice with a Join button. The game receives the invitation either way.",
                                      "Aviso con el botón Unirse. El juego recibe la invitación igualmente."), &v.inviteToasts);
        changed |= SettingRow("welcome", i18n::T("Welcome when the game starts", "Bienvenida al abrir el juego"),
                              i18n::T("The reminder that Shift+Tab opens ReFix.", "El recordatorio de que Shift+Tab abre ReFix."),
                              &v.welcomeToast);
        changed |= SettingRow("other", i18n::T("Other notices", "Otros avisos"),
                              i18n::T("ReFix messages while you play (with the overlay closed).",
                                      "Mensajes de ReFix mientras juegas (con el overlay cerrado)."), &v.otherToasts);
        break;
    case kSound:
        SectionHeader(i18n::T("Sound", "Sonido"), i18n::T("Sounds of the overlay and its notices.", "Sonidos del overlay y de sus avisos."));
        changed |= SettingRow("uisound", i18n::T("Interface sounds", "Sonidos de la interfaz"),
                              i18n::T("Clicks and the sound of opening and closing the overlay.",
                                      "Los clics y el sonido al abrir y cerrar el overlay."), &v.uiSounds);
        changed |= SettingRow("achsound", i18n::T("Achievement sounds", "Sonido de los logros"),
                              i18n::T("A soft chime with achievement notices; rarer achievements sound richer.",
                                      "Una campanilla suave con los avisos de logros; los más raros suenan más especiales."),
                              &v.achievementSounds, v.achievementToasts || v.progressToasts,
                              v.achievementToasts || v.progressToasts ? nullptr
                                  : i18n::T("Turn on an achievement notice in Notifications to hear it.",
                                            "Activa un aviso de logros en Notificaciones para oírlo."));
        break;
    case kLanguage:
        SectionHeader(i18n::T("Language", "Idioma"),
                      i18n::T("Language of the ReFix overlay on this PC. The game keeps its own.",
                              "Idioma del overlay de ReFix en este PC. El juego mantiene el suyo."));
        if (ChoiceRow("en", "English", "Overlay in English.", i18n::Current() == i18n::Lang::En)) i18n::Set(i18n::Lang::En);
        if (ChoiceRow("es", "Español", "Overlay en español.", i18n::Current() == i18n::Lang::Es)) i18n::Set(i18n::Lang::Es);
        break;
    case kPerformance: {
        SectionHeader(i18n::T("Performance", "Rendimiento"),
                      i18n::T("How the overlay gets along with the game.", "Cómo convive el overlay con el juego."));
        const bool armed = NvidiaReflexBlockArmed();
        const char* reflexNote = nullptr;
        if (v.blockReflex && !armed)
            reflexNote = i18n::T("Takes effect the next time you open the game.", "Se activa la próxima vez que abras el juego.");
        else if (!v.blockReflex && armed)
            reflexNote = i18n::T("Reflex comes back the next time you open the game.",
                                 "Reflex vuelve la próxima vez que abras el juego.");
        changed |= SettingRow(
            "reflex", i18n::T("Turn off NVIDIA Reflex", "Desactivar NVIDIA Reflex"),
            i18n::T("Reflex does not run in the game at all, so the overlay never flickers or turns see-through over it. "
                    "Games with a Reflex option see it as unavailable. Turn this off to let the game use Reflex.",
                    "Reflex no se ejecuta en el juego, así que el overlay nunca parpadea ni se ve transparente encima. "
                    "Los juegos con opción de Reflex la verán como no disponible. Desactiva esto para que el juego use Reflex."),
            &v.blockReflex, true, reflexNote);
        break;
    }
    default:
        DrawSyncSettings();
        break;
    }
    ImGui::EndChild();

    if (restore) {
        v = settings::Values();
        changed = true;
    }
    if (changed) {
        settings::Set(v);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) close = true;
    if (close) {
        settingsOpen_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::DrawOverlayCautionModals(ImVec2 viewport) {
    if (!g_overlayWarn1Open && !g_overlayWarn2Open && !ImGui::IsPopupOpen("##overlay_warn1") && !ImGui::IsPopupOpen("##overlay_warn2")) {
        return;
    }
    const ui::Fonts& f = ui::F();
    const ImVec2 vp = viewport;

    // Confirmation Warning 1 Modal
    if (g_overlayWarn1Open) {
        ImGui::OpenPopup("##overlay_warn1");
        g_overlayWarn1Open = false;
    }
    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(ui::S(520), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(20)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(14));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::color::PanelRaised);
    if (ImGui::BeginPopupModal("##overlay_warn1", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool isSteam = (g_pendingOverlayAction == OverlayAction::Steam);
        const float availW = ImGui::GetContentRegionAvail().x;

        // Header
        ImGui::PushFont(f.heading);
        ui::TextColored(isSteam ? ui::color::Accent : ui::color::Danger, isSteam ? ui::icon::Switch : ui::icon::Warning);
        ImGui::SameLine(0, ui::S(8));
        ui::TextColored(ui::color::Text,
                        isSteam ? i18n::T("Replace with Steam Overlay?", "¿Reemplazar por Overlay de Steam?")
                                : i18n::T("Disable Overlay Completely?", "¿Desactivar Overlay Completamente?"));
        ImGui::PopFont();

        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Muted,
                        isSteam ? i18n::T("Step 1 of 2: Switch overlay provider", "Paso 1 de 2: Cambiar proveedor de overlay")
                                : i18n::T("Step 1 of 2: Disable overlay provider", "Paso 1 de 2: Desactivar proveedor de overlay"));
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, ui::S(8)));
        dl->AddLine(ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetCursorScreenPos().x + availW, ImGui::GetCursorScreenPos().y), ui::color::Border);
        ImGui::Dummy(ImVec2(0, ui::S(10)));

        // Warning card container
        const ImVec2 cardP0 = ImGui::GetCursorScreenPos();
        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);
        ImGui::Dummy(ImVec2(0, ui::S(8)));
        ImGui::Indent(ui::S(12));

        if (isSteam) {
            DrawFormattedNotice(
                i18n::T("Are you sure you want to **replace the ReFix overlay** with the **official Steam overlay**?\n\n"
                        "• The ReFix overlay will stop running and **Steam will take over**.\n"
                        "• You will be asked for a **final confirmation** in the next step.",
                        "¿Estás seguro de que deseas **reemplazar el overlay de ReFix** por el **oficial de Steam**?\n\n"
                        "• El overlay de ReFix dejará de funcionar para este juego y **Steam tomará el control**.\n"
                        "• Se te solicitará una **confirmación final** en el siguiente paso."));
        } else {
            DrawFormattedNotice(
                i18n::T("Are you sure you want to **completely disable the overlay**?\n\n"
                        "• **Neither ReFix nor Steam overlay** will be displayed in this game.\n"
                        "• You will be asked for a **final confirmation** in the next step.",
                        "¿Estás seguro de que deseas **desactivar completamente el overlay**?\n\n"
                        "• **Ni el overlay de ReFix ni el de Steam** se mostrarán en este juego.\n"
                        "• Se te solicitará una **confirmación final** en el siguiente paso."));
        }

        ImGui::Unindent(ui::S(12));
        ImGui::Dummy(ImVec2(0, ui::S(8)));
        const ImVec2 cardP1(cardP0.x + availW, ImGui::GetCursorScreenPos().y);
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled(cardP0, cardP1, ui::color::Row, ui::S(10));
        dl->AddRect(cardP0, cardP1, ui::Fade(isSteam ? ui::color::Accent : ui::color::Danger, 0.4f), ui::S(10), 0, ui::S(1.0f));
        dl->ChannelsMerge();

        ImGui::Dummy(ImVec2(0, ui::S(14)));

        const float gap = ui::S(12);
        const float btnW = (availW - gap) * 0.5f;
        const ImVec2 bs(btnW, ui::S(38));

        if (isSteam ? ui::PrimaryButton(i18n::T("Continue", "Continuar"), bs)
                    : ui::DangerButton(i18n::T("Continue", "Continuar"), bs)) {
            ImGui::CloseCurrentPopup();
            g_overlayWarn2Open = true;
            g_overlayHoldTime = 0.0f;
        }
        ImGui::SameLine(0, gap);
        if (ui::SecondaryButton(i18n::T("Cancel", "Cancelar"), bs) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
            g_pendingOverlayAction = OverlayAction::None;
            settingsOpen_ = true;
            settingsRequested_ = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Confirmation Warning 2 Modal (Click and Hold to confirm with decremental charge)
    if (g_overlayWarn2Open) {
        ImGui::OpenPopup("##overlay_warn2");
        g_overlayWarn2Open = false;
        g_overlayHoldTime = 0.0f;
    }
    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(ui::S(540), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(20)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(14));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::color::PanelRaised);
    if (ImGui::BeginPopupModal("##overlay_warn2", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool isSteam = (g_pendingOverlayAction == OverlayAction::Steam);
        const float availW = ImGui::GetContentRegionAvail().x;

        // Header
        ImGui::PushFont(f.heading);
        ui::TextColored(isSteam ? ui::color::Accent : ui::color::Danger, ui::icon::Warning);
        ImGui::SameLine(0, ui::S(8));
        ui::TextColored(isSteam ? ui::color::Accent : ui::color::Danger,
                        isSteam ? i18n::T("Important Confirmation: Steam Overlay", "Confirmación importante: Overlay de Steam")
                                : i18n::T("Important Confirmation: Disable Overlay", "Confirmación importante: Desactivar Overlay"));
        ImGui::PopFont();

        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Muted,
                        i18n::T("Step 2 of 2: Critical confirmation & automatic game restart",
                                "Paso 2 de 2: Confirmación crítica y reinicio automático del juego"));
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, ui::S(8)));
        dl->AddLine(ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetCursorScreenPos().x + availW, ImGui::GetCursorScreenPos().y), ui::color::Border);
        ImGui::Dummy(ImVec2(0, ui::S(10)));

        // Warning card container with ChannelsSplit
        const ImVec2 cardP0 = ImGui::GetCursorScreenPos();
        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);
        ImGui::Dummy(ImVec2(0, ui::S(8)));
        ImGui::Indent(ui::S(12));

        if (isSteam) {
            DrawFormattedNotice(
                i18n::T("CRITICAL NOTICE: This will write **Enabled=false** and **BlockSteamOverlay=false** in **ReFix.ini**.\n\n"
                        "• Once disabled, **it CANNOT be reactivated from the in-game interface**.\n"
                        "• You will ONLY be able to reactivate it by editing **ReFix.ini** manually in the game directory.\n"
                        "• The game **will restart automatically** immediately upon confirmation.",
                        "AVISO CRÍTICO: Esta acción escribirá **Enabled=false** y **BlockSteamOverlay=false** en **ReFix.ini**.\n\n"
                        "• Una vez desactivado, **NO se podrá reactivar desde la interfaz del juego**.\n"
                        "• ÚNICAMENTE podrás reactivarlo editando **ReFix.ini** manualmente en la carpeta del juego.\n"
                        "• El juego **se reiniciará automáticamente** de inmediato al confirmar."));
        } else {
            DrawFormattedNotice(
                i18n::T("CRITICAL NOTICE: This will write **Enabled=false** and **BlockSteamOverlay=true** in **ReFix.ini**.\n\n"
                        "• Both overlays will be **completely disabled** in this game.\n"
                        "• You will ONLY be able to reactivate it by editing **ReFix.ini** manually in the game directory.\n"
                        "• The game **will restart automatically** immediately upon confirmation.",
                        "AVISO CRÍTICO: Esta acción escribirá **Enabled=false** y **BlockSteamOverlay=true** en **ReFix.ini**.\n\n"
                        "• Ambos overlays quedarán **completamente desactivados** en este juego.\n"
                        "• ÚNICAMENTE podrás reactivarlo editando **ReFix.ini** manualmente en la carpeta del juego.\n"
                        "• El juego **se reiniciará automáticamente** de inmediato al confirmar."));
        }

        ImGui::Unindent(ui::S(12));
        ImGui::Dummy(ImVec2(0, ui::S(8)));
        const ImVec2 cardP1(cardP0.x + availW, ImGui::GetCursorScreenPos().y);
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled(cardP0, cardP1, ui::color::Row, ui::S(10));
        dl->AddRect(cardP0, cardP1, ui::Fade(isSteam ? ui::color::Accent : ui::color::Danger, 0.45f), ui::S(10), 0, ui::S(1.0f));
        dl->ChannelsMerge();

        ImGui::Dummy(ImVec2(0, ui::S(14)));

        // Action buttons: Hold to confirm on left, Cancel on right
        constexpr float kMaxHoldSeconds = 5.0f;
        const float dt = ImGui::GetIO().DeltaTime;
        const float cancelW = ui::S(110);
        const float gap = ui::S(12);
        const float holdW = availW - cancelW - gap;
        const float btnH = ui::S(42);

        const ImVec2 hp0 = ImGui::GetCursorScreenPos();
        const ImVec2 hp1(hp0.x + holdW, hp0.y + btnH);

        ImGui::InvisibleButton("##hold_btn", ImVec2(holdW, btnH));
        const bool isHolding = ImGui::IsItemActive();
        const bool isHovered = ImGui::IsItemHovered();

        if (isHovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        if (isHolding) {
            g_overlayHoldTime = std::min(kMaxHoldSeconds, g_overlayHoldTime + dt);
        } else {
            // Rapid discharge when released
            g_overlayHoldTime = std::max(0.0f, g_overlayHoldTime - dt * 8.0f);
        }

        // Decremental fill curve: p = 1.0 - (1.0 - (t / 5.0))^2.2
        const float u = std::clamp(g_overlayHoldTime / kMaxHoldSeconds, 0.0f, 1.0f);
        const float fillProgress = 1.0f - std::pow(1.0f - u, 2.2f);

        // Draw hold button base
        const ImU32 baseBg = isSteam
            ? (isHovered ? IM_COL32(26, 42, 62, 255) : IM_COL32(18, 30, 46, 255))
            : (isHovered ? IM_COL32(52, 22, 26, 255) : IM_COL32(38, 16, 20, 255));
        const ImU32 borderColor = isSteam
            ? (isHovered ? ui::color::Accent : ui::Fade(ui::color::Accent, 0.6f))
            : (isHovered ? ui::color::Danger : ui::Fade(ui::color::Danger, 0.6f));
        const ImU32 fillBg = isSteam
            ? IM_COL32(40, 140, 230, 240)
            : IM_COL32(230, 50, 60, 240);

        dl->AddRectFilled(hp0, hp1, baseBg, ui::S(8));
        dl->AddRect(hp0, hp1, borderColor, ui::S(8), 0, ui::S(1.5f));

        // Draw fill bar
        if (fillProgress > 0.002f) {
            dl->PushClipRect(hp0, ImVec2(hp0.x + holdW * fillProgress, hp1.y), true);
            dl->AddRectFilled(hp0, hp1, fillBg, ui::S(8));
            dl->PopClipRect();
        }

        // Button label
        ImGui::PushFont(f.strong);
        char holdLabel[128];
        if (g_overlayHoldTime <= 0.05f) {
            snprintf(holdLabel, sizeof(holdLabel), "%s  %s",
                     isSteam ? ui::icon::Switch : ui::icon::Warning,
                     i18n::T("Hold to restart (5s)", "Mantén presionado para reiniciar (5s)"));
        } else {
            float rem = std::max(0.0f, kMaxHoldSeconds - g_overlayHoldTime);
            snprintf(holdLabel, sizeof(holdLabel), "%s  %s (%.1fs)",
                     ui::icon::Refresh,
                     i18n::T("Holding to restart...", "Manteniendo para reiniciar..."), rem);
        }
        const ImVec2 lsize = ImGui::CalcTextSize(holdLabel);
        dl->AddText(f.strong, f.strong->FontSize,
                    ImVec2(hp0.x + (holdW - lsize.x) * 0.5f + 1, hp0.y + (btnH - lsize.y) * 0.5f + 1),
                    IM_COL32(0, 0, 0, 160), holdLabel);
        dl->AddText(f.strong, f.strong->FontSize,
                    ImVec2(hp0.x + (holdW - lsize.x) * 0.5f, hp0.y + (btnH - lsize.y) * 0.5f),
                    ui::color::Text, holdLabel);
        ImGui::PopFont();

        // Cancel button
        ImGui::SameLine(0, gap);
        if (ui::SecondaryButton(i18n::T("Cancel", "Cancelar"), ImVec2(cancelW, btnH)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            g_overlayHoldTime = 0.0f;
            g_pendingOverlayAction = OverlayAction::None;
            ImGui::CloseCurrentPopup();
            settingsOpen_ = true;
            settingsRequested_ = true;
        }

        // Check if hold duration completed
        if (g_overlayHoldTime >= kMaxHoldSeconds) {
            g_overlayHoldTime = 0.0f;
            const auto action = g_pendingOverlayAction;
            g_pendingOverlayAction = OverlayAction::None;
            ImGui::CloseCurrentPopup();
            CloseOverlay();

            if (action == OverlayAction::Steam) {
                settings::ReplaceWithSteamOverlay();
            } else if (action == OverlayAction::DisableAll) {
                settings::DisableOverlayCompletely();
            }
            App::Get().RestartGame();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

namespace {

std::string Clock(uint64_t filetime) {
    FILETIME ft{ (DWORD)filetime, (DWORD)(filetime >> 32) }, local;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, &local);
    FileTimeToSystemTime(&local, &st);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02u/%02u %02u:%02u", st.wDay, st.wMonth, st.wHour, st.wMinute);
    return buf;
}

// Live state of the sync, for the destination in use.
std::pair<std::string, ImU32> SyncLine() {
    const sync::Status s = sync::Get();
    const std::string when = s.lastSync ? Clock(s.lastSync) : std::string();
    switch (s.state) {
    case sync::State::Synced:
        return { std::string(i18n::T("Synced", "Sincronizado")) + (when.empty() ? "" : " \xC2\xB7 " + when), ui::color::SyncOk };
    case sync::State::Syncing: return { i18n::T("Syncing now...", "Sincronizando..."), ui::color::Accent };
    case sync::State::Pending: return { i18n::T("Changes waiting to sync", "Cambios esperando para sincronizar"), ui::color::SyncPending };
    case sync::State::Failed: return { i18n::T("Last sync failed", "Falló la última sincronización"), ui::color::Danger };
    case sync::State::Unavailable: return { i18n::T("Not reachable right now", "No disponible ahora"), ui::color::SyncPending };
    default: return { i18n::T("This profile does not sync", "Este perfil no se sincroniza"), ui::color::Muted };
    }
}

// Service logos drawn with the draw list (no image files): Google Drive's three-colour triangle, OneDrive's cloud.
void DriveLogo(ImDrawList* dl, ImVec2 c, float size) {
    const float w = size, h = size * 0.87f;
    const ImVec2 o(c.x - w * 0.5f, c.y - h * 0.5f);
    auto P = [&](float x, float y) { return ImVec2(o.x + x * w, o.y + y * h); };
    auto quad = [&](ImVec2 a, ImVec2 b, ImVec2 c2, ImVec2 d, ImU32 col) { dl->AddQuadFilled(a, b, c2, d, col); };
    quad(P(0.34f, 0.00f), P(0.50f, 0.28f), P(0.17f, 0.86f), P(0.00f, 0.57f), IM_COL32(15, 157, 88, 255));    // green, left
    quad(P(0.34f, 0.00f), P(0.66f, 0.00f), P(1.00f, 0.57f), P(0.67f, 0.57f), IM_COL32(244, 180, 0, 255));    // yellow, right
    quad(P(0.17f, 0.86f), P(0.33f, 0.57f), P(1.00f, 0.57f), P(0.83f, 0.86f), IM_COL32(66, 133, 244, 255));   // blue, bottom
}

void OneDriveLogo(ImDrawList* dl, ImVec2 c, float size) {
    const ImU32 dark = IM_COL32(0, 120, 212, 255), light = IM_COL32(40, 168, 234, 255);
    const float u = size;
    auto at = [&](float x, float y) { return ImVec2(c.x + (x - 0.5f) * u, c.y + (y - 0.5f) * u); };
    dl->AddCircleFilled(at(0.36f, 0.56f), u * 0.20f, dark, 24);
    dl->AddCircleFilled(at(0.58f, 0.44f), u * 0.27f, light, 32);
    dl->AddCircleFilled(at(0.80f, 0.60f), u * 0.17f, light, 24);
    dl->AddRectFilled(at(0.18f, 0.58f), at(0.97f, 0.77f), light, u * 0.09f);
    dl->AddRectFilled(at(0.18f, 0.58f), at(0.45f, 0.77f), dark, u * 0.09f);
}

// Provider mark: the service's logo on a soft disc.
void ProviderBadge(ImDrawList* dl, ImVec2 c, float r, cloud::Provider p) {
    dl->AddCircleFilled(c, r, IM_COL32(255, 255, 255, 18), 32);
    dl->AddCircle(c, r, ui::color::Border, 32, ui::S(1));
    if (p == cloud::Provider::Google) DriveLogo(dl, ImVec2(c.x, c.y + r * 0.04f), r * 1.15f);
    else OneDriveLogo(dl, c, r * 1.3f);
}

// Card frame shared by the rows of the Syncing page; returns its top-left and leaves the cursor below it.
struct Card { ImVec2 p0, p1; float cy; };
Card BeginCard(float h, bool active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Card c;
    c.p0 = ImGui::GetCursorScreenPos();
    c.p1 = ImVec2(c.p0.x + ImGui::GetContentRegionAvail().x, c.p0.y + h);
    c.cy = c.p0.y + h * 0.5f;
    dl->AddRectFilled(c.p0, c.p1, active ? ui::color::AccentSoft : ui::color::Row, ui::S(12));
    if (active) dl->AddRect(c.p0, c.p1, ui::color::AccentLine, ui::S(12));
    return c;
}
void EndCard(const Card& c) {
    ImGui::SetCursorScreenPos(ImVec2(c.p0.x, c.p1.y + ui::S(kRowGap)));
    ImGui::Dummy(ImVec2(0, 0));
}

} // namespace

// One card per cloud service. Signed in: the account (name, email), the live sync state when it is the
// destination, Use and Disconnect. Not signed in: what it does and Sign in. Only with ReFix's client IDs.
void App::DrawCloudAccounts(const std::wstring& chosen) {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ui::S(78), pad = ui::S(16), r = ui::S(20);
    for (cloud::Provider p : { cloud::Provider::Google, cloud::Provider::OneDrive }) {
        if (!cloud::Available(p)) continue;
        ImGui::PushID(cloud::Label(p));
        const bool connected = cloud::Connected(p), inUse = connected && chosen == cloud::Choice(p);
        const Card c = BeginCard(h, inUse);
        ProviderBadge(dl, ImVec2(c.p0.x + pad + r, c.cy), r, p);
        // Buttons, right to left.
        float x = c.p1.x - pad;
        const ImVec2 bs(ui::S(120), ui::S(34));
        auto place = [&](float w) { x -= w; ImGui::SetCursorScreenPos(ImVec2(x, c.cy - bs.y * 0.5f)); x -= ui::S(8); };
        if (connected) {
            place(bs.x);
            if (ui::DangerButton(i18n::T("Disconnect", "Desconectar"), bs)) {
                if (inUse) sync::SetFolder(L"");
                cloud::Disconnect(p);
            }
            if (!inUse) {
                place(bs.x);
                if (ui::PrimaryButton(i18n::T("Use", "Usar"), bs)) sync::SetFolder(cloud::Choice(p));
            }
        } else if (cloud::SigningIn(p)) {
            // Waiting for the browser: the player may have closed the tab, so this can always be undone.
            place(bs.x);
            if (ui::SecondaryButton(i18n::T("Cancel", "Cancelar"), bs)) cloud::CancelSignIn();
        } else {
            place(ui::S(150));
            if (ui::PrimaryButton(i18n::T("Sign in", "Iniciar sesión"), ImVec2(ui::S(150), bs.y), !cloud::SigningIn()))
                cloud::SignInAsync(p);
        }
        // Text.
        const float tx = c.p0.x + pad + 2 * r + ui::S(14), tw = std::max(ui::S(60), x - tx);
        std::string title = cloud::Label(p), sub, status;
        ImU32 statusCol = ui::color::Muted;
        if (connected) {
            const cloud::Account a = cloud::AccountOf(p);
            if (!a.name.empty()) title = a.name;
            sub = !a.email.empty() ? a.email : std::string(cloud::Label(p));
            if (inUse) std::tie(status, statusCol) = SyncLine();
            else status = i18n::T("Signed in, not in use", "Sesión iniciada, sin usar");
        } else if (cloud::SigningIn(p)) {
            sub = i18n::T("Finish signing in in your browser", "Termina de iniciar sesión en tu navegador");
            status = i18n::T("Waiting for the browser...", "Esperando al navegador...");
            statusCol = ui::color::SyncPending;
        } else {
            sub = i18n::T("Keeps your profiles in your account", "Guarda tus perfiles en tu cuenta");
        }
        ImGui::PushFont(f.strong);
        title = ui::Ellipsize(title, tw);
        ImGui::PopFont();
        ImGui::PushFont(f.caption);
        sub = ui::Ellipsize(sub, tw);
        status = ui::Ellipsize(status, tw - ui::S(14));
        ImGui::PopFont();
        const float lines = status.empty() ? 2.0f : 3.0f;
        float ty = c.cy - (f.strong->FontSize + (lines - 1) * (f.caption->FontSize + ui::S(2))) * 0.5f;
        dl->AddText(f.strong, f.strong->FontSize, ImVec2(tx, ty), ui::color::Text, title.c_str());
        ty += f.strong->FontSize + ui::S(2);
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(tx, ty), ui::color::Muted, sub.c_str());
        if (!status.empty()) {
            ty += f.caption->FontSize + ui::S(2);
            const float dot = ui::S(4);
            const bool live = (inUse && sync::Get().state == sync::State::Syncing) || cloud::SigningIn(p);
            const float pulse = live ? 0.55f + 0.45f * (0.5f + 0.5f * sinf((float)ImGui::GetTime() * 6.0f)) : 1.0f;
            dl->AddCircleFilled(ImVec2(tx + dot, ty + f.caption->FontSize * 0.55f), dot * pulse, statusCol, 16);
            dl->AddText(f.caption, f.caption->FontSize, ImVec2(tx + ui::S(14), ty), statusCol, status.c_str());
        }
        EndCard(c);
        ImGui::PopID();
    }
    const cloud::Msg err = cloud::LastError();
    if (err.en) {
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Danger, i18n::T(err.en, err.es));
        ImGui::PopFont();
    }
}

// Syncing: where each profile's file goes. In the cloud (accounts, cloud apps' folders), local (USB, any
// folder, only this PC), and a file to move by hand (export / import).
void App::DrawSyncSettings() {
    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushFont(f.heading);
    ImGui::TextUnformatted(i18n::T("Profile sync", "Sincronización de perfiles"));
    ImGui::PopFont();
    ImGui::PushFont(f.caption);
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(ui::color::Muted,
                    i18n::T("Saves, achievements, stats and items of each profile in one file, kept where you choose. "
                            "Your profiles always stay on this PC too. Nothing is sent to ReFix.",
                            "Partidas, logros, estadísticas e inventario de cada perfil en un archivo, guardado donde elijas. "
                            "Tus perfiles siempre se quedan también en este PC. Nada se envía a ReFix."));
    ImGui::PopTextWrapPos();
    ImGui::PopFont();

    const std::wstring chosen = sync::ChosenFolder(), effective = sync::EffectiveFolder();
    const bool localOnly = chosen == sync::kLocalOnly;
    cloud::Provider account;
    const bool isAccount = cloud::FromChoice(chosen, account);
    const float pad = ui::S(16);
    const ImVec2 bs(ui::S(120), ui::S(34));

    // A row: glyph, title, detail, one button on the right (selected rows show a check instead).
    auto choiceRow = [&](const char* id, const char* glyph, const std::string& title, const std::string& detail,
                         bool selected, const char* button) -> bool {
        ImGui::PushID(id);
        const Card c = BeginCard(ui::S(62), selected);
        dl->AddText(f.heading, f.heading->FontSize, ImVec2(c.p0.x + pad, c.cy - f.heading->FontSize * 0.5f),
                    selected ? ui::color::Accent : ui::color::Muted, glyph);
        bool clicked = false;
        float x = c.p1.x - pad;
        if (selected) {
            const char* inUse = i18n::T("In use", "En uso");
            const float w = f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, 0, inUse).x;
            dl->AddText(f.caption, f.caption->FontSize, ImVec2(x - w, c.cy - f.caption->FontSize * 0.5f), ui::color::Accent, inUse);
            x -= w + ui::S(8);
            dl->AddText(f.body, f.body->FontSize, ImVec2(x - ui::S(14), c.cy - f.body->FontSize * 0.5f), ui::color::Accent, ui::icon::Check);
            x -= ui::S(22);
        } else if (button) {
            x -= bs.x;
            ImGui::SetCursorScreenPos(ImVec2(x, c.cy - bs.y * 0.5f));
            clicked = ui::SecondaryButton(button, bs);
            x -= ui::S(8);
        }
        const float tx = c.p0.x + pad + ui::S(38), tw = std::max(ui::S(60), x - tx);
        ImGui::PushFont(f.strong);
        const std::string t = ui::Ellipsize(title, tw);
        ImGui::PopFont();
        ImGui::PushFont(f.caption);
        const std::string d = ui::Ellipsize(detail, tw);
        ImGui::PopFont();
        const float th = f.strong->FontSize + (d.empty() ? 0 : ui::S(2) + f.caption->FontSize);
        dl->AddText(f.strong, f.strong->FontSize, ImVec2(tx, c.cy - th * 0.5f), ui::color::Text, t.c_str());
        if (!d.empty())
            dl->AddText(f.caption, f.caption->FontSize, ImVec2(tx, c.cy - th * 0.5f + f.strong->FontSize + ui::S(2)), ui::color::Muted, d.c_str());
        EndCard(c);
        ImGui::PopID();
        return clicked;
    };

    // In the cloud.
    SyncSection(ui::icon::Cloud, i18n::T("In the cloud", "En la nube"));
    DrawCloudAccounts(chosen);
    for (const sync::CloudFolder& c : sync::CloudFolders()) {
        const std::wstring folder = c.path + L"\\ReFix Sync";
        const std::string title = ToUtf8(c.label) + i18n::T(" (desktop app)", " (app de escritorio)");
        if (choiceRow(ToUtf8(c.path).c_str(), ui::icon::Cloud, title, ToUtf8(folder), folder == effective && !isAccount,
                      i18n::T("Use", "Usar")))
            sync::SetFolder(folder);
    }
    ImGui::PushFont(f.caption);
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(ui::color::Muted,
                    i18n::T("ReFix only gets its own folder in your account: it cannot see your files. The sign-in stays on this PC, "
                            "encrypted for your Windows user.",
                            "ReFix solo obtiene su propia carpeta en tu cuenta: no puede ver tus archivos. La sesión queda en este PC, "
                            "cifrada para tu usuario de Windows."));
    ImGui::PopTextWrapPos();
    ImGui::PopFont();

    // Local.
    SyncSection(ui::icon::Folder, i18n::T("Local", "Local"));
    const bool customFolder = !chosen.empty() && !localOnly && !isAccount && effective == chosen &&
                              [&] { for (const sync::CloudFolder& c : sync::CloudFolders()) if (c.path + L"\\ReFix Sync" == chosen) return false; return true; }();
    if (choiceRow("folder", ui::icon::Usb, i18n::T("USB or other folder", "USB u otra carpeta"),
                  customFolder ? ToUtf8(chosen) : std::string(i18n::T("A USB drive, a NAS or any folder", "Un USB, un NAS o cualquier carpeta")),
                  false, customFolder ? i18n::T("Change...", "Cambiar...") : i18n::T("Choose...", "Elegir..."))) {
        const std::wstring picked = PickFolder(i18n::TW(L"Folder to keep your profiles in", L"Carpeta para guardar tus perfiles"), syncError_);
        if (!picked.empty()) sync::SetFolder(picked);
    }
    if (customFolder) {
        // The chosen folder is the one in use: say so under its row.
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Accent, (std::string(i18n::T("In use: ", "En uso: ")) + SyncLine().first).c_str());
        ImGui::PopFont();
    }
    if (choiceRow("local", ui::icon::Lock, i18n::T("Only on this PC", "Solo en este PC"),
                  i18n::T("No copy anywhere else", "Sin copia en ningún otro lugar"), localOnly, i18n::T("Use", "Usar")))
        sync::SetFolder(sync::kLocalOnly);
    if (!chosen.empty() && ui::LinkButton(i18n::T("Back to automatic", "Volver a automática")))
        sync::SetFolder(L"");
    bool autoSync = sync::AutoSync();
    if (SettingRow("autosync", i18n::T("Autosync", "Autosincronizar"),
                   i18n::T("Syncs a few seconds after the game saves, at most once every 5 minutes.",
                           "Sincroniza unos segundos después de que el juego guarde, como mucho una vez cada 5 minutos."),
                   &autoSync))
        sync::SetAutoSync(autoSync);
    bool localProfiles = sync::SyncLocalProfiles();
    if (SettingRow("synclocal", i18n::T("Sync local profiles too", "Sincronizar también los perfiles locales"),
                   i18n::T("Profiles not linked to Steam stay on this PC unless you turn this on.",
                           "Los perfiles que no son de Steam se quedan en este PC salvo que actives esto."),
                   &localProfiles))
        sync::SetSyncLocalProfiles(localProfiles);

    // Export or import.
    SyncSection(ui::icon::Import, i18n::T("Export or import", "Exportar o importar"));
    const float half = (ImGui::GetContentRegionAvail().x - ui::S(8)) * 0.5f;
    if (ui::SecondaryButton(i18n::T("Export my profile...", "Exportar mi perfil..."), ImVec2(half, ui::S(40)), active_ && !active_->guest)) {
        const std::wstring file = PickProfileFile(true, i18n::TW(L"Export your profile", L"Exportar tu perfil"), active_->name + L".rfxp", syncError_);
        std::string err;
        if (!file.empty())
            ShowInfo(sync::ExportTo(*active_, file, err) ? i18n::T("Profile exported", "Perfil exportado") : i18n::T("Export failed", "Error al exportar"),
                     err.empty() ? ToUtf8(file) : err);
    }
    ImGui::SameLine(0, ui::S(8));
    if (ui::SecondaryButton(i18n::T("Import a profile file...", "Importar archivo de perfil..."), ImVec2(half, ui::S(40)))) {
        const std::wstring file = PickProfileFile(false, i18n::TW(L"Import a ReFix profile", L"Importar un perfil de ReFix"), L"", syncError_);
        std::string err;
        if (!file.empty()) {
            if (auto p = sync::ImportAsNewProfile(file, err)) {
                ReloadProfiles();
                ShowInfo(i18n::T("Profile imported", "Perfil importado"),
                         ToUtf8(p->name) + i18n::T(" is a new profile: switch to it from your profile menu. Nothing was replaced.",
                                                   " es un perfil nuevo: cámbiate a él desde el menú de tu perfil. No se reemplazó nada."));
            } else {
                ShowInfo(i18n::T("Import failed", "Error al importar"), err);
            }
        }
    }
    ImGui::PushFont(f.caption);
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(ui::color::Muted, i18n::T("A .rfxp file to carry by hand. Importing always makes a new profile: nothing is replaced.",
                                              "Un archivo .rfxp para llevar a mano. Importar siempre crea un perfil nuevo: no se reemplaza nada."));
    if (!syncError_.empty()) ui::TextColored(ui::color::Danger, syncError_.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
}

} // namespace refix::overlay
