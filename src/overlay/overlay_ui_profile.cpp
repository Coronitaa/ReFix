// Profile screens: the chip menu (switch / create / edit / delete), the restart and delete confirmations, the
// language switch, the profile editor (overlay modal and first-run dialog) and the "Who's playing?" picker shown to
// a second window of the same game or when every profile is already in use.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "avatar_image.h"
#include "overlay_i18n.h"
#include "overlay_text.h"
#include "refix_overlay.h"
#include "save_swap.h"

#include <shobjidl.h>
#include <algorithm>
#include <cstring>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
using i18n::T;
namespace {

constexpr float kMenuWidth = 320.0f;
constexpr float kMenuRow = 42.0f;
constexpr float kEditorWidth = 540.0f;
constexpr int kGridColumns = 6;
constexpr int kPickerColumns = 3;
constexpr float kPickerCardHeight = 150.0f;

int g_imageSerial = 0;

std::wstring TempAvatarPath() {
    return profiles::RootDir() + L"\\avatar_import_" + std::to_wstring(GetCurrentProcessId()) + L".png";
}

void ClearTempAvatar() { DeleteFileW(TempAvatarPath().c_str()); }

enum class RowAction { None, Open, Delete };

// Full-width clickable row: avatar or glyph, label, optional note and an optional trash button on the right.
RowAction MenuRow(const char* id, const char* glyph, const ui::AvatarRef* avatar, const std::string& label,
                  const char* note, bool enabled, bool deletable = false) {
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = ui::S(kMenuRow);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    if (deletable) ImGui::SetNextItemAllowOverlap();
    const bool clicked = ui::Clicked(ui::NavButton("##row", ImVec2(w, h)) && enabled);
    const bool hover = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_AllowWhenOverlappedByItem);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hover && enabled) {
        dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h), ui::color::RowHover, ui::S(10));
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    const ImU32 ink = enabled ? ui::color::Text : ui::color::Faint;
    const float cy = p0.y + h * 0.5f;
    const ui::Fonts& f = ui::F();
    if (avatar) {
        ui::DrawAvatar(dl, ImVec2(p0.x + ui::S(10 + 14), cy), ui::S(14), *avatar);
    } else if (glyph) {
        const ImVec2 gs = f.body->CalcTextSizeA(f.body->FontSize, FLT_MAX, 0, glyph);
        dl->AddText(f.body, f.body->FontSize, ImVec2(p0.x + ui::S(24) - gs.x * 0.5f, cy - gs.y * 0.5f),
                    enabled ? ui::color::Accent : ui::color::Faint, glyph);
    }
    float right = p0.x + w - ui::S(12);
    RowAction action = clicked ? RowAction::Open : RowAction::None;
    if (deletable) {
        const float d = ui::S(28);
        right -= d;
        ImGui::SetCursorScreenPos(ImVec2(right, cy - d * 0.5f));
        if (ui::IconButton("del", ui::icon::Delete, T("Delete profile", "Eliminar perfil"), ui::color::Danger, true, d))
            action = RowAction::Delete;
        right -= ui::S(8);
    }
    if (note) {
        const ImVec2 ns = f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, 0, note);
        right -= ns.x;
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(right, cy - ns.y * 0.5f), ui::color::Muted, note);
        right -= ui::S(8);
    }
    ImGui::PushFont(f.body);
    const std::string text = ui::Ellipsize(label, right - (p0.x + ui::S(48)));
    const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
    ImGui::PopFont();
    dl->AddText(f.body, f.body->FontSize, ImVec2(p0.x + ui::S(48), cy - ts.y * 0.5f), ink, text.c_str());
    ImGui::PopID();
    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(w, h));
    return action;
}

void CenteredText(ImFont* font, ImU32 col, const char* text, float width) {
    ImGui::PushFont(font);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
    const float tw = std::min(ImGui::CalcTextSize(text).x, width);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - tw) * 0.5f);
    ui::TextColored(col, text);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
}

void Message(ImU32 col, const char* glyph, const std::string& text) {
    if (text.empty()) return;
    ImGui::PushFont(ui::F().caption);
    ui::TextColored(col, glyph);
    ImGui::SameLine(0, ui::S(8));
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(col, text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
}

bool BeginCenteredModal(const char* id, float width) {
    if (!ImGui::IsPopupOpen(id)) ImGui::OpenPopup(id);
    const ImVec2 vp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(ui::S(width), vp.x - ui::S(24)), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(22)));
    const bool open = ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    return open;
}

std::string ProfileName(const std::vector<Profile>& all, const std::wstring& id, const char* fallback) {
    for (const Profile& p : all) {
        if (p.id == id) return ToUtf8(p.name);
    }
    return fallback;
}

} // namespace

std::wstring EditorTempAvatarPath() { return TempAvatarPath(); }
int NextEditorImageSerial() { return ++g_imageSerial; }

// ---------------------------------------------------------------------------------------------------------------
// Language switch: "EN | ES" pill whose right edge sits at `rightEdgeCenter`.
// ---------------------------------------------------------------------------------------------------------------

void App::DrawLanguageSwitch(ImVec2 rightEdgeCenter) {
    const float h = ui::S(30), segW = ui::S(40);
    const ImVec2 p0(rightEdgeCenter.x - segW * 2, rightEdgeCenter.y - h * 0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + segW * 2, p0.y + h), ui::color::PanelRaised, h * 0.5f);
    dl->AddRect(p0, ImVec2(p0.x + segW * 2, p0.y + h), ui::color::Border, h * 0.5f);
    const struct { const char* label; i18n::Lang lang; } segs[] = { { "EN", i18n::Lang::En }, { "ES", i18n::Lang::Es } };
    for (int i = 0; i < 2; ++i) {
        const ImVec2 a(p0.x + segW * i, p0.y), b(a.x + segW, p0.y + h);
        const bool active = i18n::Current() == segs[i].lang;
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        if (ui::Clicked(ui::NavButton("##lang", ImVec2(segW, h)) && !active)) i18n::Set(segs[i].lang);
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hover && !active) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (active) dl->AddRectFilled(ImVec2(a.x + ui::S(3), a.y + ui::S(3)), ImVec2(b.x - ui::S(3), b.y - ui::S(3)), ui::color::Accent, h * 0.5f);
        ImFont* font = ui::F().caption;
        const ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, segs[i].label);
        dl->AddText(font, font->FontSize, ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f),
                    active ? ui::color::AccentInk : (hover ? ui::color::Text : ui::color::Muted), segs[i].label);
    }
    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(segW * 2, h));
    ui::Tooltip(T("Language", "Idioma"));
}

// ---------------------------------------------------------------------------------------------------------------
// Chip menu
// ---------------------------------------------------------------------------------------------------------------

void App::DrawProfileMenu() {
    ImGui::SetNextWindowPos(ImVec2(chipMax_.x, chipMax_.y + ui::S(8)), ImGuiCond_Appearing, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(ui::S(kMenuWidth), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(10), ui::S(10)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ui::S(6), ui::S(2)));
    if (!ImGui::BeginPopup("##profilemenu")) {
        ImGui::PopStyleVar(2);
        return;
    }
    const ui::Fonts& f = ui::F();
    const std::wstring activeId = active_ ? active_->id : L"";
    const bool guest = active_ && active_->guest;

    // Header: who is playing.
    {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float r = ui::S(22);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ui::DrawAvatar(dl, ImVec2(p0.x + ui::S(4) + r, p0.y + ui::S(6) + r), r, active_ ? ProfileAvatar(*active_) : ui::AvatarRef{});
        const float tx = p0.x + ui::S(4) + r * 2 + ui::S(12);
        const float tw = ImGui::GetContentRegionAvail().x - (tx - p0.x);
        ImGui::PushFont(f.strong);
        const std::string name = ui::Ellipsize(LocalName(), tw);
        ImGui::PopFont();
        dl->AddText(f.strong, f.strong->FontSize, ImVec2(tx, p0.y + ui::S(9)), ui::color::Text, name.c_str());
        const char* sub = !active_ ? T("No profile: this PC's identity", "Sin perfil: identidad de este PC")
                        : guest    ? T("Guest: saves are temporary", "Invitado: partidas temporales")
                                   : T("Active profile", "Perfil activo");
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(tx, p0.y + ui::S(9) + f.strong->FontSize + ui::S(2)),
                    ui::color::Muted, sub);
        ImGui::Dummy(ImVec2(0, r * 2 + ui::S(14)));
    }
    if (active_ && !guest && MenuRow("edit", ui::icon::Edit, nullptr, T("Edit profile", "Editar perfil"), nullptr, true) == RowAction::Open) {
        ImGui::CloseCurrentPopup();
        BeginEditor(false);
    }
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    ui::SectionLabel(T("Switch profile", "Cambiar de perfil"));
    ImGui::Dummy(ImVec2(0, ui::S(2)));
    int others = 0;
    for (const Profile& p : profiles_) {
        if (p.id == activeId) continue;
        ++others;
        const bool inUse = profiles::IsLockedByOther(p.id, activeId);
        const bool busyElsewhere = !inUse && profiles::IsInUseAnywhere(p.id);
        const ui::AvatarRef av = ProfileAvatar(p);
        const RowAction a = MenuRow(ToUtf8(p.id).c_str(), nullptr, &av, ToUtf8(p.name),
                                    inUse ? T("Playing", "En uso") : nullptr, !inUse, !inUse && !busyElsewhere);
        if (a == RowAction::Open) {
            pendingSwitch_ = p.id;
            ImGui::CloseCurrentPopup();
        } else if (a == RowAction::Delete) {
            pendingDelete_ = p.id;
            deleteUnderstood_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (inUse) ui::Tooltip(T("Open in another window of this game", "Se está usando en otra ventana de este juego"));
    }
    if (others == 0) {
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Faint, T("  No other profiles on this PC.", "  No hay otros perfiles en este PC."));
        ImGui::PopFont();
    }
    const bool canCreate = profiles_.size() < (size_t)kMaxProfiles;
    if (MenuRow("new", ui::icon::Add, nullptr, T("Create new profile", "Crear perfil nuevo"),
                canCreate ? nullptr : T("Max. 10", "Máximo 10"), canCreate) == RowAction::Open) {
        ImGui::CloseCurrentPopup();
        BeginEditor(true);
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------------------------------------------
// Confirmations
// ---------------------------------------------------------------------------------------------------------------

void App::DrawSwitchConfirm() {
    if (pendingSwitch_.empty()) return;
    if (!BeginCenteredModal("##switch", 460)) return;
    const std::string name = ProfileName(profiles_, pendingSwitch_, T("this profile", "este perfil"));
    ImGui::PushFont(ui::F().heading);
    ImGui::TextUnformatted((T("Switch to ", "Cambiar a ") + name).c_str());
    ImGui::PopFont();
    ImGui::PushFont(ui::F().body);
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(ui::color::Muted, T("The game has to restart to switch profiles. Save your game before you continue.",
                                        "Para cambiar de perfil el juego tiene que reiniciarse. Guarda tu partida antes de continuar."));
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(8)));
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 bs((w - ui::S(10)) * 0.5f, ui::S(42));
    const std::wstring id = pendingSwitch_;
    if (ui::PrimaryButton(T("Restart now", "Reiniciar ahora"), bs)) {
        ImGui::CloseCurrentPopup();
        pendingSwitch_.clear();
        RestartWithProfile(id);
    }
    ImGui::SameLine(0, ui::S(10));
    if (ui::SecondaryButton(T("Next time I start", "Al próximo inicio"), bs)) {
        ImGui::CloseCurrentPopup();
        DeferProfile(id);
    }
    ImGui::Dummy(ImVec2(0, ui::S(2)));
    const char* cancel = T("Cancel", "Cancelar");
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - ImGui::CalcTextSize(cancel).x) * 0.5f);
    if (ui::LinkButton(cancel) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
        pendingSwitch_.clear();
    }
    ImGui::EndPopup();
}

// Two steps: the "I understand" box has to be ticked before the red button works.
void App::DrawDeleteConfirm() {
    if (pendingDelete_.empty()) return;
    if (!BeginCenteredModal("##delete", 480)) return;
    const std::string name = ProfileName(profiles_, pendingDelete_, T("this profile", "este perfil"));
    ImGui::PushFont(ui::F().heading);
    ui::TextColored(ui::color::Danger, ui::icon::Warning);
    ImGui::SameLine();
    ImGui::TextUnformatted((T("Delete ", "Eliminar ") + name + "?").c_str());
    ImGui::PopFont();
    ImGui::PushFont(ui::F().body);
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(ui::color::Muted,
                    T("Its name, avatar, SteamID and favourites are deleted, together with the saves ReFix LAN keeps for "
                      "this profile. It cannot be undone.",
                      "Se borran su nombre, su avatar, su SteamID y sus favoritos, junto con las partidas que ReFix LAN "
                      "guarda para este perfil. No se puede deshacer."));
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    ImGui::Checkbox(T("I understand, delete it", "Lo entiendo, quiero eliminarlo"), &deleteUnderstood_);
    ImGui::Dummy(ImVec2(0, ui::S(8)));
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 bs((w - ui::S(10)) * 0.5f, ui::S(42));
    bool close = false;
    if (ui::SecondaryButton(T("Cancel", "Cancelar"), bs) || ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;
    ImGui::SameLine(0, ui::S(10));
    if (ui::DangerButton(T("Delete profile", "Eliminar perfil"), bs, deleteUnderstood_)) {
        const std::wstring id = pendingDelete_;
        if (DeleteProfile(id)) ShowInfo(T("Profile deleted", "Perfil eliminado"), name);
        close = true;
    }
    if (close) {
        ImGui::CloseCurrentPopup();
        pendingDelete_.clear();
        deleteUnderstood_ = false;
    }
    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------------------------------------------

void App::BeginEditor(bool creating) {
    ClearTempAvatar();
    editor_ = EditorState{};
    editor_.open = true;
    editor_.creating = creating;
    editor_.imageVersion = ++g_imageSerial;
    if (!creating && active_) {
        strncpy_s(editor_.name, ToUtf8(active_->name).c_str(), _TRUNCATE);
        editor_.stock = active_->stockAvatar;
        editor_.custom = active_->customAvatar;
        if (editor_.custom) editor_.imagePath = profiles::AvatarPath(active_->id);
    } else {
        editor_.stock = (int)(GetTickCount64() % (ULONGLONG)ui::StockCount());
        editor_.offerNative = saves::HasOriginalSaves();
        editor_.native = editor_.offerNative && profiles_.empty();
    }
}

bool App::SaveEditor() {
    editor_.error.clear();
    std::wstring name = Trim(FromUtf8(editor_.name));
    if (name.empty()) {
        editor_.error = T("Type a name for the profile.", "Escribe un nombre para el perfil.");
        return false;
    }
    name = name.substr(0, kMaxNameChars);
    const bool customFromFile = editor_.custom && editor_.imagePath == TempAvatarPath();

    if (editor_.creating) {
        if (profiles_.size() >= (size_t)kMaxProfiles || allProfiles_.size() >= (size_t)kMaxStoredProfiles) {
            editor_.error = T("This PC already has 10 profiles, the maximum.", "Ya hay 10 perfiles en este PC, el máximo.");
            return false;
        }
        Profile p = profiles::MakeNew(name, allProfiles_);
        if (editor_.steamId) {
            p.steamId = editor_.steamId;
            p.importedFromSteam = editor_.steamIdFromSteam;
        }
        p.stockAvatar = editor_.stock;
        p.nativeSaves = editor_.offerNative && editor_.native;
        if (!profiles::Save(p)) {
            editor_.error = T("Could not save the profile in %APPDATA%\\ReFix.", "No se pudo guardar el perfil en %APPDATA%\\ReFix.");
            return false;
        }
        if (p.nativeSaves) {
            saves::InheritOriginalSaves(p.id);
        }
        if (customFromFile && CopyFileW(editor_.imagePath.c_str(), profiles::AvatarPath(p.id).c_str(), FALSE)) {
            p.customAvatar = true;
            profiles::Save(p);
        }
        editor_.savedId = p.id;
        ReFixLog("[Overlay] Profile created: %s (%llu)", ToUtf8(p.id).c_str(), (unsigned long long)p.steamId);
    } else {
        if (!active_ || active_->guest) return false;
        Profile p = *active_;
        const bool renamed = p.name != name;
        p.name = name;
        p.stockAvatar = editor_.stock;
        if (customFromFile) {
            p.customAvatar = CopyFileW(editor_.imagePath.c_str(), profiles::AvatarPath(p.id).c_str(), FALSE) != 0;
        } else if (!editor_.custom) {
            p.customAvatar = false;
        }
        if (!profiles::Save(p)) {
            editor_.error = T("Could not save the profile.", "No se pudo guardar el perfil.");
            return false;
        }
        active_ = p;
        ++profileVersion_;
        if (renamed) UnrealSteamEmu::Overlay_SetPersonaName(ToUtf8(name));
        editor_.savedId = p.id;
        lastRefresh_ = 0;
    }
    ReloadProfiles();
    ClearTempAvatar();
    return true;
}

bool App::DrawEditorBody(bool firstRun) {
    const ui::Fonts& f = ui::F();
    const float w = ImGui::GetContentRegionAvail().x;
    const char* title = firstRun ? T("Welcome to ReFix", "Te damos la bienvenida a ReFix")
                                 : editor_.creating ? T("New profile", "Nuevo perfil") : T("Edit profile", "Editar perfil");
    CenteredText(f.title, ui::color::Text, title, w);
    if (firstRun) {
        CenteredText(f.caption, ui::color::Muted,
                     T("Create your profile to play on the LAN. Every profile has its own name, avatar, SteamID and saves.",
                       "Crea tu perfil para jugar en la LAN. Cada perfil tiene su nombre, su avatar, su SteamID y sus "
                       "partidas guardadas."),
                     w);
    }
    ImGui::Dummy(ImVec2(0, ui::S(10)));

    // Preview with carousel arrows.
    const float r = ui::S(52);
    const float d = ui::S(36);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const ImVec2 c(top.x + w * 0.5f, top.y + r + ui::S(4));
    dl->AddCircleFilled(c, r + ui::S(6), ui::Fade(ui::color::Accent, 0.12f), 64);
    ui::DrawAvatar(dl, c, r, EditorAvatar());
    const int count = ui::StockCount();
    ImGui::SetCursorScreenPos(ImVec2(c.x - r - ui::S(28) - d, c.y - d * 0.5f));
    if (ui::IconButton("prev", ui::icon::ChevronLeft, T("Previous avatar", "Avatar anterior"), ui::color::Text, true, d)) {
        editor_.stock = editor_.custom ? editor_.stock : (editor_.stock + count - 1) % count;
        editor_.custom = false;
    }
    ImGui::SetCursorScreenPos(ImVec2(c.x + r + ui::S(28), c.y - d * 0.5f));
    if (ui::IconButton("next", ui::icon::ChevronRight, T("Next avatar", "Avatar siguiente"), ui::color::Text, true, d)) {
        editor_.stock = editor_.custom ? editor_.stock : (editor_.stock + 1) % count;
        editor_.custom = false;
    }
    ImGui::SetCursorScreenPos(ImVec2(top.x, top.y + r * 2 + ui::S(20)));

    // Stock grid.
    const float cell = std::min(ui::S(52), w / kGridColumns);
    const float gridW = cell * kGridColumns;
    const ImVec2 g0(top.x + (w - gridW) * 0.5f, ImGui::GetCursorScreenPos().y);
    for (int i = 0; i < count; ++i) {
        const ImVec2 cc(g0.x + (i % kGridColumns + 0.5f) * cell, g0.y + (i / kGridColumns + 0.5f) * cell);
        ImGui::SetCursorScreenPos(ImVec2(cc.x - cell * 0.5f, cc.y - cell * 0.5f));
        ImGui::PushID(i);
        if (ui::Clicked(ui::NavButton("##stock", ImVec2(cell, cell)))) {
            editor_.stock = i;
            editor_.custom = false;
        }
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const float sr = cell * 0.5f - ui::S(6);
        const bool selected = !editor_.custom && editor_.stock == i;
        if (selected) dl->AddCircle(cc, sr + ui::S(3.5f), ui::color::Accent, 48, ui::S(2.5f));
        else if (hover) dl->AddCircle(cc, sr + ui::S(3), IM_COL32(255, 255, 255, 60), 48, ui::S(1.5f));
        ui::AvatarRef a;
        a.stock = i;
        ui::DrawAvatar(dl, cc, sr, a);
    }
    const int rows = (count + kGridColumns - 1) / kGridColumns;
    ImGui::SetCursorScreenPos(ImVec2(top.x, g0.y + rows * cell + ui::S(10)));

    // Import buttons.
    const float bw = (w - ui::S(10)) * 0.5f;
    const std::string imgLabel = std::string(ui::icon::Folder) + "  " + T("Import picture…", "Importar imagen…") + "##img";
    const std::string steamLabel = std::string(ui::icon::Import) + "  " + T("Import from Steam", "Importar desde Steam") + "##steam";
    if (ui::SecondaryButton(imgLabel.c_str(), ImVec2(bw, ui::S(40)))) ImportImage();
    ImGui::SameLine(0, ui::S(10));
    if (ui::SecondaryButton(steamLabel.c_str(), ImVec2(bw, ui::S(40)))) ImportFromSteam();
    ui::Tooltip(T("Uses the name, avatar and SteamID of the account signed in to Steam",
                  "Usa el nombre, el avatar y el SteamID de la cuenta abierta en Steam"));
    if (editor_.creating) {
        const bool others = !profiles::FromOtherGames(allProfiles_, profiles::GameKey()).empty();
        const std::string otherLabel = std::string(ui::icon::Game) + "  " +
                                       T("Import from another game…", "Importar de otro juego…") + "##othergame";
        if (ui::SecondaryButton(otherLabel.c_str(), ImVec2(w, ui::S(40)), others)) importRequested_ = true;
        ui::Tooltip(others ? T("Play with a profile you made in another ReFix game: same name, picture and SteamID",
                               "Juega con un perfil que creaste en otro juego con ReFix: mismo nombre, imagen y SteamID")
                           : T("Your other ReFix games have no profiles yet", "Tus otros juegos con ReFix aún no tienen perfiles"));
        DrawImportPopup();
    }
    ImGui::Dummy(ImVec2(0, ui::S(6)));

    // Name.
    ImGui::PushFont(f.caption);
    ui::TextColored(ui::color::Muted, T("Name", "Nombre"));
    ImGui::PopFont();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool enter = ImGui::InputTextWithHint("##name", T("Your name on the LAN", "Tu nombre en la LAN"), editor_.name,
                                                sizeof(editor_.name), ImGuiInputTextFlags_EnterReturnsTrue);
    if (editor_.offerNative) {
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        ImGui::Checkbox(T("Use the saves already on this PC##native", "Usar las partidas guardadas que ya hay en este PC##native"),
                        &editor_.native);
        ui::Tooltip(T("If you untick it, this profile starts with no saves and the current ones are kept aside",
                      "Si lo desmarcas, este perfil empieza sin partidas y las actuales se guardan aparte"));
    }
    if (editor_.steamId) {
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Faint, (std::string(editor_.steamIdFromSteam ? T("Steam SteamID: ", "SteamID de Steam: ")
                                                                  : T("Imported SteamID: ", "SteamID importado: ")) +
                         std::to_string(editor_.steamId)).c_str());
        ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    Message(ui::color::Danger, ui::icon::Warning, editor_.error);
    Message(ui::color::Accent, ui::icon::Check, editor_.info);

    // Footer pinned to the bottom of the available area.
    const float footerH = ui::S(44);
    const float remaining = ImGui::GetContentRegionAvail().y;
    if (remaining > footerH + ui::S(8)) ImGui::Dummy(ImVec2(0, remaining - footerH - ui::S(4)));
    else ImGui::Dummy(ImVec2(0, ui::S(10)));
    bool done = false;
    const bool hasName = !Trim(FromUtf8(editor_.name)).empty();
    const ImVec2 bs(ui::S(150), footerH);
    const float rowY = ImGui::GetCursorPosY();
    if (firstRun) {
        ImGui::SetCursorPosY(rowY + (footerH - ImGui::GetTextLineHeight()) * 0.5f);
        if (ui::LinkButton(T("Not now", "Ahora no"))) {
            editor_.savedId.clear();
            done = true;
        }
        ui::Tooltip(T("Play with this PC's identity; you can create the profile later with Shift+Tab",
                      "Juega con la identidad de este PC; podrás crear el perfil más tarde con Shift+Tab"));
    } else {
        if (ui::SecondaryButton(T("Cancel##editor", "Cancelar##editor"), bs) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            editor_.savedId.clear();
            done = true;
        }
    }
    ImGui::SameLine();
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - bs.x, rowY));
    const char* primary = editor_.creating ? T("Create profile##save", "Crear perfil##save") : T("Save##save", "Guardar##save");
    if ((ui::PrimaryButton(primary, bs, hasName) || (enter && hasName)) && SaveEditor()) done = true;
    if (done) ClearTempAvatar();
    return done;
}

void App::DrawEditorModal() {
    if (!editor_.open) return;
    if (!ImGui::IsPopupOpen("##editor")) ImGui::OpenPopup("##editor");
    const ImVec2 vp = ImGui::GetIO().DisplaySize;
    const ImVec2 size(std::min(ui::S(kEditorWidth), vp.x - ui::S(32)), std::min(ui::S(640), vp.y - ui::S(32)));
    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(28), ui::S(24)));
    if (!ImGui::BeginPopupModal("##editor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                                         ImGuiWindowFlags_NoScrollbar)) {
        ImGui::PopStyleVar();
        return;
    }
    const bool creating = editor_.creating;
    if (DrawEditorBody(false)) {
        editor_.open = false;
        ImGui::CloseCurrentPopup();
        // A new profile only takes effect after a restart: ask now.
        if (creating && !editor_.savedId.empty()) pendingSwitch_ = editor_.savedId;
        else if (!creating && !editor_.savedId.empty())
            ShowInfo(T("Profile saved", "Perfil guardado"), T("The changes are already applied.", "Los cambios ya están aplicados."));
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------------------------------------------
// Dialog window (before the game window exists)
// ---------------------------------------------------------------------------------------------------------------

void App::DrawDialog(ImVec2 viewport) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(viewport);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(32), ui::S(28)));
    ImGui::Begin("##dialog", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddRectFilled(ImVec2(0, 0), viewport, ui::color::Panel, ui::S(14));
    bg->AddRect(ImVec2(0.5f, 0.5f), ImVec2(viewport.x - 0.5f, viewport.y - 0.5f), ui::color::Border, ui::S(14));
    bg->AddRectFilled(ImVec2(ui::S(14), 0), ImVec2(viewport.x - ui::S(14), ui::S(3)), ui::color::Accent);

    // Language first, on a row of its own above the title.
    const ImVec2 body = ImGui::GetCursorScreenPos();
    DrawLanguageSwitch(ImVec2(viewport.x - ui::S(18), ui::S(26)));
    ImGui::SetCursorScreenPos(ImVec2(body.x, body.y + ui::S(18)));

    if (dialog_ == DialogKind::FirstRun || dialogEditing_) {
        if (!editor_.open) BeginEditor(true);
        if (DrawEditorBody(dialog_ == DialogKind::FirstRun)) {
            std::optional<Profile> saved;
            if (!editor_.savedId.empty()) saved = profiles::Load(editor_.savedId);
            if (saved && profiles::TryLock(saved->id)) {
                FinishDialog(saved);
            } else if (dialog_ == DialogKind::FirstRun) {
                FinishDialog(std::nullopt);
            } else {
                dialogEditing_ = false;
                editor_.open = false;
                ReloadProfiles();
            }
        }
    } else {
        DrawPicker();
    }
    DrawDeleteConfirm();
    ImGui::End();
}

void App::DrawPicker() {
    const ui::Fonts& f = ui::F();
    const float w = ImGui::GetContentRegionAvail().x;
    CenteredText(f.title, ui::color::Text, T("Who's playing?", "¿Quién juega?"), w);
    CenteredText(f.caption, ui::color::Muted,
                 T("Profiles already playing in another window of this game are locked. Pick yours, create one or play "
                   "as a guest.",
                   "Los perfiles que ya están jugando en otra ventana de este juego aparecen bloqueados. Elige el tuyo, "
                   "crea uno o juega como invitado."),
                 w);
    ImGui::Dummy(ImVec2(0, ui::S(12)));

    const float footer = ui::S(56);
    ImGui::BeginChild("##profiles", ImVec2(0, ImGui::GetContentRegionAvail().y - footer), ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoBackground);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float gap = ui::S(12);
    const float avail = ImGui::GetContentRegionAvail().x;
    const float cardW = (avail - gap * (kPickerColumns - 1)) / kPickerColumns;
    const float cardH = ui::S(kPickerCardHeight);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const bool canCreate = profiles_.size() < (size_t)kMaxProfiles;
    const bool canImport = !profiles::FromOtherGames(allProfiles_, profiles::GameKey()).empty();
    const int cards = (int)profiles_.size() + 1 + (canImport ? 1 : 0);   // + "new profile" (+ "from another game")
    std::optional<Profile> chosen;
    for (int i = 0; i < cards; ++i) {
        const ImVec2 p0(origin.x + (i % kPickerColumns) * (cardW + gap), origin.y + (i / kPickerColumns) * (cardH + gap));
        const ImVec2 p1(p0.x + cardW, p0.y + cardH);
        const bool isNew = i == (int)profiles_.size();
        const bool isImport = i == (int)profiles_.size() + 1;
        const Profile* p = (isNew || isImport) ? nullptr : &profiles_[i];
        const bool locked = p && profiles::IsLockedByOther(p->id, L"");
        const bool busyElsewhere = p && !locked && profiles::IsInUseAnywhere(p->id);
        const bool enabled = isImport ? true : isNew ? canCreate : !locked;
        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        ImGui::SetNextItemAllowOverlap();
        const bool clicked = ui::Clicked(ui::NavButton("##card", ImVec2(cardW, cardH)) && enabled);
        const bool hover = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);
        if (hover && enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        dl->AddRectFilled(p0, p1, hover && enabled ? ui::color::RowHover : ui::color::Row, ui::S(14));
        if (hover && enabled) dl->AddRect(p0, p1, ui::Fade(ui::color::Accent, 0.7f), ui::S(14), 0, ui::S(1.5f));
        const ImVec2 ac(p0.x + cardW * 0.5f, p0.y + ui::S(18 + 34));
        if (isNew || isImport) {
            dl->AddCircle(ac, ui::S(34), enabled ? ui::color::Accent : ui::color::Faint, 48, ui::S(2));
            ui::GlyphCentered(dl, f.title, f.title->FontSize, ac, enabled ? ui::color::Accent : ui::color::Faint,
                              isImport ? ui::icon::Import : ui::icon::Add);
        } else {
            ui::DrawAvatar(dl, ac, ui::S(34), ProfileAvatar(*p));
            if (locked) {
                dl->AddCircleFilled(ac, ui::S(34), IM_COL32(10, 13, 18, 170), 48);
                const ImVec2 gs = f.heading->CalcTextSizeA(f.heading->FontSize, FLT_MAX, 0, ui::icon::Lock);
                dl->AddText(f.heading, f.heading->FontSize, ImVec2(ac.x - gs.x * 0.5f, ac.y - gs.y * 0.5f), ui::color::Text,
                            ui::icon::Lock);
            }
        }
        const std::string label = isImport ? T("From another game", "De otro juego")
                                  : isNew  ? (canCreate ? T("New profile", "Nuevo perfil") : T("Max. 10 profiles", "Máximo 10 perfiles"))
                                           : ToUtf8(p->name);
        ImGui::PushFont(f.strong);
        const std::string name = ui::Ellipsize(label, cardW - ui::S(20));
        const ImVec2 ns = ImGui::CalcTextSize(name.c_str());
        ImGui::PopFont();
        dl->AddText(f.strong, f.strong->FontSize, ImVec2(ac.x - ns.x * 0.5f, p0.y + ui::S(98)), enabled ? ui::color::Text : ui::color::Faint,
                    name.c_str());
        const char* status = (isNew || isImport) ? "" : locked ? T("Playing now", "Jugando ahora") : T("Available", "Disponible");
        const ImVec2 ss = f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, 0, status);
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(ac.x - ss.x * 0.5f, p0.y + ui::S(98) + f.strong->FontSize + ui::S(4)),
                    locked ? ui::color::Faint : ui::color::Accent, status);
        // Delete: only for profiles no game is using.
        // Hover by rectangle: while the trash button is held the card itself does not count as hovered.
        if (p && !locked && !busyElsewhere && ImGui::IsMouseHoveringRect(p0, p1) &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            const float d = ui::S(28);
            ImGui::SetCursorScreenPos(ImVec2(p1.x - d - ui::S(8), p0.y + ui::S(8)));
            if (ui::IconButton("del", ui::icon::Delete, T("Delete profile", "Eliminar perfil"), ui::color::Danger, true, d)) {
                pendingDelete_ = p->id;
                deleteUnderstood_ = false;
            } else if (clicked && pendingDelete_.empty()) {
                chosen = *p;
            }
        } else if (clicked) {
            if (isImport) {
                importRequested_ = true;
            } else if (isNew) {
                BeginEditor(true);
                dialogEditing_ = true;
            } else {
                chosen = *p;
            }
        }
        ImGui::PopID();
    }
    const int rowsUsed = (cards + kPickerColumns - 1) / kPickerColumns;
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rowsUsed * (cardH + gap)));
    ImGui::Dummy(ImVec2(avail, 0));
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, ui::S(10)));
    const char* guestLabel = T("I'd rather play as a guest", "Prefiero jugar como invitado");
    ImGui::PushFont(f.strong);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - ImGui::CalcTextSize(guestLabel).x) * 0.5f);
    const bool guest = ui::LinkButton(guestLabel);
    ImGui::PopFont();
    ui::Tooltip(T("Standard name and avatar, temporary saves that are deleted when the game closes",
                  "Nombre y avatar estándar, partidas temporales que se borran al cerrar el juego"));

    DrawImportPopup();
    if (chosen && profiles::TryLock(chosen->id)) {
        FinishDialog(chosen);
    } else if (guest) {
        Profile g = profiles::MakeGuest(i18n::TW(L"Guest", L"Invitado"), allProfiles_);
        ReFixLog("[Overlay] Playing as a guest (SteamID %llu)", (unsigned long long)g.steamId);
        FinishDialog(g);
    }
}

} // namespace refix::overlay
