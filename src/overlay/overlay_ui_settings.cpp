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
struct NavGroup { const char* en; const char* es; NavEntry items[3]; int count; };

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

enum SettingsSection { kNotifications, kSound, kLanguage, kPerformance, kSync };   // App::settingsSection_

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
        { "OVERLAY", "OVERLAY", { { kNotifications, ui::icon::Bell, "Notifications", "Notificaciones" },
                                  { kSound, ui::icon::Volume, "Sound", "Sonido" },
                                  { kLanguage, ui::icon::Globe, "Language", "Idioma" } }, 3 },
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

namespace {

// Sub-heading of the Syncing page: accent glyph, label, hairline to the right.
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

// Provider mark: a coloured disc with its initial.
void ProviderBadge(ImDrawList* dl, ImVec2 c, float r, cloud::Provider p) {
    const ImU32 col = p == cloud::Provider::Google ? IM_COL32(66, 133, 244, 255) : IM_COL32(0, 120, 212, 255);
    dl->AddCircleFilled(c, r, ui::Fade(col, 0.18f), 32);
    dl->AddCircle(c, r, ui::Fade(col, 0.7f), 32, ui::S(1.5f));
    ImFont* font = ui::F().heading;
    const char* letter = p == cloud::Provider::Google ? "G" : "O";
    const ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, letter);
    dl->AddText(font, font->FontSize, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), col, letter);
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
        } else {
            place(ui::S(150));
            const bool busy = cloud::SigningIn();
            if (ui::PrimaryButton(busy ? i18n::T("In the browser...", "En el navegador...") : i18n::T("Sign in", "Iniciar sesión"),
                                  ImVec2(ui::S(150), bs.y), !busy))
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
            const bool live = inUse && sync::Get().state == sync::State::Syncing;
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
