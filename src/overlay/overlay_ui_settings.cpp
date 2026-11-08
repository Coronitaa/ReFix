// Settings modal of the overlay (gear in the top bar): notifications, sounds, language and the NVIDIA Reflex
// pause. Each row is one switch with a plain explanation; changes save at once (overlay_settings.cpp).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "nvidia_reflex_block.h"
#include "overlay_i18n.h"
#include "overlay_settings.h"

#include <algorithm>

namespace refix::overlay {
namespace {

constexpr float kWidth = 640.0f;
constexpr float kMaxHeight = 760.0f;
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

void SectionTitle(const char* glyph, const char* text) {
    ImGui::Dummy(ImVec2(0, ui::S(6)));
    ImGui::PushFont(ui::F().strong);
    ui::TextColored(ui::color::Accent, glyph);
    ImGui::SameLine();
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(20)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(14));
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
    const float width = ImGui::GetContentRegionAvail().x;
    bool close = false;

    // Header.
    const ImVec2 h0 = ImGui::GetCursorScreenPos();
    ImGui::PushFont(f.title);
    ui::TextColored(ui::color::Accent, ui::icon::Settings);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("Settings", "Ajustes"));
    ImGui::PopFont();
    const float btn = ui::S(36);
    ImGui::SetCursorScreenPos(ImVec2(h0.x + width - btn, h0.y));
    if (ui::IconButton("settingsclose", ui::icon::Close, i18n::T("Close (Esc)", "Cerrar (Esc)"), ui::color::Text, true, btn)) close = true;
    ImGui::SetCursorScreenPos(ImVec2(h0.x, h0.y + f.title->FontSize + ui::S(8)));
    ImGui::PushFont(f.caption);
    ui::TextColored(ui::color::Muted, i18n::T("Changes are saved right away, for every game on this PC.",
                                              "Los cambios se guardan al instante, para todos los juegos de este PC."));
    ImGui::PopFont();

    const float footer = ui::S(40) + ui::S(16);
    ImGui::BeginChild("##settingsbody", ImVec2(0, ImGui::GetContentRegionAvail().y - footer), ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoBackground);
    settings::Values v = settings::Get();
    bool changed = false;

    SectionTitle(ui::icon::Bell, i18n::T("Notifications", "Notificaciones"));
    changed |= SettingRow("ach", i18n::T("Unlocked achievements", "Logros desbloqueados"),
                          i18n::T("A notice in the corner with the achievement and how rare it is.",
                                  "Aviso en la esquina con el logro y su rareza."), &v.achievementToasts);
    changed |= SettingRow("progress", i18n::T("Achievement progress", "Progreso de logros"),
                          i18n::T("A notice when an achievement moves forward, like 3/10.",
                                  "Aviso cuando un logro avanza, por ejemplo 3/10."), &v.progressToasts);
    changed |= SettingRow("achsound", i18n::T("Achievement sounds", "Sonido de los logros"),
                          i18n::T("A soft chime with those notices; rarer achievements sound richer.",
                                  "Una campanilla suave con esos avisos; los más raros suenan más especiales."),
                          &v.achievementSounds, v.achievementToasts || v.progressToasts);
    changed |= SettingRow("invite", i18n::T("Invitations", "Invitaciones"),
                          i18n::T("A notice with a Join button. The game receives the invitation either way.",
                                  "Aviso con el botón Unirse. El juego recibe la invitación igualmente."), &v.inviteToasts);
    changed |= SettingRow("welcome", i18n::T("Welcome when the game starts", "Bienvenida al abrir el juego"),
                          i18n::T("The reminder that Shift+Tab opens ReFix.", "El recordatorio de que Shift+Tab abre ReFix."),
                          &v.welcomeToast);
    changed |= SettingRow("other", i18n::T("Other notices", "Otros avisos"),
                          i18n::T("ReFix messages while you play (with the overlay closed).",
                                  "Mensajes de ReFix mientras juegas (con el overlay cerrado)."), &v.otherToasts);

    SectionTitle(ui::icon::Volume, i18n::T("Sound and language", "Sonido e idioma"));
    changed |= SettingRow("uisound", i18n::T("Interface sounds", "Sonidos de la interfaz"),
                          i18n::T("Clicks and the sound of opening and closing the overlay.",
                                  "Los clics y el sonido al abrir y cerrar el overlay."), &v.uiSounds);
    {
        // Language: same EN / ES switch as the top bar, in a row of its own.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rw = ImGui::GetContentRegionAvail().x, pad = ui::S(kRowPad);
        const float rh = pad * 2 + f.strong->FontSize;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p0, ImVec2(p0.x + rw, p0.y + rh), ui::color::Row, ui::S(12));
        dl->AddText(f.strong, f.strong->FontSize, ImVec2(p0.x + pad, p0.y + pad), ui::color::Text,
                    i18n::T("Overlay language", "Idioma del overlay"));
        DrawLanguageSwitch(ImVec2(p0.x + rw - pad, p0.y + rh * 0.5f));
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + rh + ui::S(kRowGap)));
        ImGui::Dummy(ImVec2(0, 0));
    }

    SectionTitle(ui::icon::Speed, i18n::T("Performance and compatibility", "Rendimiento y compatibilidad"));
    const bool armed = NvidiaReflexBlockArmed();
    const char* reflexNote = nullptr;
    if (v.blockReflex && !armed)
        reflexNote = i18n::T("Takes effect the next time you open the game.", "Se activa la próxima vez que abras el juego.");
    else if (!v.blockReflex && armed)
        reflexNote = i18n::T("Off now. Fully removed the next time you open the game.",
                             "Desactivado ya. Se quita del todo la próxima vez que abras el juego.");
    const bool reflexChanged = SettingRow(
        "reflex", i18n::T("Pause NVIDIA Reflex while the overlay is open", "Pausar NVIDIA Reflex con el overlay abierto"),
        i18n::T("Stops the overlay from flickering or turning see-through in games with Reflex. The game gets Reflex back when "
                "you close the overlay. Turn it off if the game behaves worse with it.",
                "Evita que el overlay parpadee o se vea transparente en juegos con Reflex. El juego recupera Reflex al cerrar "
                "el overlay. Desactívalo si el juego va peor con esta opción."),
        &v.blockReflex, true, reflexNote);
    changed |= reflexChanged;
    ImGui::EndChild();

    if (changed) {
        settings::Set(v);
        if (reflexChanged) SetReflexPausedForOverlay(v.blockReflex);   // the overlay is open right now
    }

    // Footer: restore defaults on the left, done on the right.
    const ImVec2 bs(ui::S(150), ui::S(40));
    ImGui::Dummy(ImVec2(0, ui::S(8)));
    const ImVec2 fpos = ImGui::GetCursorScreenPos();
    if (ui::SecondaryButton(i18n::T("Restore defaults", "Restablecer"), bs)) {
        settings::Set(settings::Values());
        SetReflexPausedForOverlay(true);
    }
    ImGui::SetCursorScreenPos(ImVec2(fpos.x + width - bs.x, fpos.y));
    if (ui::PrimaryButton(i18n::T("Done", "Listo"), bs)) close = true;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) close = true;
    if (close) {
        settingsOpen_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace refix::overlay
