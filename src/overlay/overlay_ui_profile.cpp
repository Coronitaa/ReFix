// Profile screens: the chip menu (switch / create / edit), the restart confirmation, the profile editor
// (overlay modal and first-run dialog) and the picker shown when every profile is already in use.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "avatar_image.h"
#include "overlay_text.h"
#include "refix_overlay.h"

#include <shobjidl.h>
#include <algorithm>
#include <cstring>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay {
namespace {

constexpr float kMenuWidth = 320.0f;
constexpr float kMenuRow = 42.0f;
constexpr float kEditorWidth = 540.0f;
constexpr int kGridColumns = 6;

int g_imageSerial = 0;

std::wstring TempAvatarPath() {
    return profiles::RootDir() + L"\\avatar_import_" + std::to_wstring(GetCurrentProcessId()) + L".png";
}

void ClearTempAvatar() { DeleteFileW(TempAvatarPath().c_str()); }

// Full-width clickable row: avatar or glyph, label, optional right-hand note.
bool MenuRow(const char* id, const char* glyph, const ui::AvatarRef* avatar, const std::string& label,
             const char* note, bool enabled) {
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = ui::S(kMenuRow);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    const bool clicked = ImGui::InvisibleButton("##row", ImVec2(w, h)) && enabled;
    const bool hover = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    ImGui::PopID();
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
    float noteW = 0;
    if (note) {
        const ImVec2 ns = f.caption->CalcTextSizeA(f.caption->FontSize, FLT_MAX, 0, note);
        noteW = ns.x + ui::S(12);
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(p0.x + w - ui::S(12) - ns.x, cy - ns.y * 0.5f),
                    ui::color::Muted, note);
    }
    ImGui::PushFont(f.body);
    const std::string text = ui::Ellipsize(label, w - ui::S(48) - noteW - ui::S(8));
    const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
    ImGui::PopFont();
    dl->AddText(f.body, f.body->FontSize, ImVec2(p0.x + ui::S(48), cy - ts.y * 0.5f), ink, text.c_str());
    return clicked;
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

} // namespace

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
        const char* sub = active_ ? "Perfil activo" : "Sin perfil: identidad de este PC";
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(tx, p0.y + ui::S(9) + f.strong->FontSize + ui::S(2)),
                    ui::color::Muted, sub);
        ImGui::Dummy(ImVec2(0, r * 2 + ui::S(14)));
    }
    if (active_ && MenuRow("edit", ui::icon::Edit, nullptr, "Editar perfil", nullptr, true)) {
        ImGui::CloseCurrentPopup();
        BeginEditor(false);
    }
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    ui::SectionLabel("Cambiar de perfil");
    ImGui::Dummy(ImVec2(0, ui::S(2)));
    int others = 0;
    for (const Profile& p : profiles_) {
        if (p.id == activeId) continue;
        ++others;
        const bool inUse = profiles::IsLockedByOther(p.id, activeId);
        const ui::AvatarRef av = ProfileAvatar(p);
        if (MenuRow(ToUtf8(p.id).c_str(), nullptr, &av, ToUtf8(p.name), inUse ? "En uso" : nullptr, !inUse)) {
            pendingSwitch_ = p.id;
            ImGui::CloseCurrentPopup();
        }
        if (inUse) ui::Tooltip("Se está usando en otra ventana de este juego");
    }
    if (others == 0) {
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Faint, "  No hay otros perfiles en este PC.");
        ImGui::PopFont();
    }
    const bool canCreate = profiles_.size() < (size_t)kMaxProfiles;
    if (MenuRow("new", ui::icon::Add, nullptr, "Crear perfil nuevo", canCreate ? nullptr : "Máximo 10", canCreate)) {
        ImGui::CloseCurrentPopup();
        BeginEditor(true);
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------------------------------------------
// Restart confirmation
// ---------------------------------------------------------------------------------------------------------------

void App::DrawSwitchConfirm() {
    if (pendingSwitch_.empty()) return;
    if (!ImGui::IsPopupOpen("##switch")) ImGui::OpenPopup("##switch");
    const ImVec2 vp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(ui::S(460), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(22)));
    if (!ImGui::BeginPopupModal("##switch", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PopStyleVar();
        return;
    }
    std::string name = "este perfil";
    for (const Profile& p : profiles_) {
        if (p.id == pendingSwitch_) name = ToUtf8(p.name);
    }
    ImGui::PushFont(ui::F().heading);
    ImGui::TextUnformatted(("Cambiar a " + name).c_str());
    ImGui::PopFont();
    ImGui::PushFont(ui::F().body);
    ImGui::PushTextWrapPos(0.0f);
    ui::TextColored(ui::color::Muted,
                    "Para cambiar de perfil el juego tiene que reiniciarse. Guarda tu partida antes de continuar.");
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(8)));
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 bs((w - ui::S(10)) * 0.5f, ui::S(42));
    const std::wstring id = pendingSwitch_;
    if (ui::PrimaryButton("Reiniciar ahora", bs)) {
        ImGui::CloseCurrentPopup();
        pendingSwitch_.clear();
        RestartWithProfile(id);
    }
    ImGui::SameLine(0, ui::S(10));
    if (ui::SecondaryButton("Al próximo inicio", bs)) {
        ImGui::CloseCurrentPopup();
        DeferProfile(id);
    }
    ImGui::Dummy(ImVec2(0, ui::S(2)));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - ImGui::CalcTextSize("Cancelar").x) * 0.5f);
    if (ui::LinkButton("Cancelar") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
        pendingSwitch_.clear();
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar();
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
        editor_.offerNative = profiles_.empty();
        editor_.native = editor_.offerNative;
    }
}

void App::ImportImage() {
    editor_.error.clear();
    editor_.info.clear();
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        editor_.error = "No se pudo abrir el selector de archivos.";
        return;
    }
    const COMDLG_FILTERSPEC types[] = {
        { L"Imágenes", L"*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.gif;*.tif;*.tiff;*.ico;*.webp;*.heic;*.heif" },
        { L"Todos los archivos", L"*.*" },
    };
    dlg->SetFileTypes(2, types);
    dlg->SetTitle(L"Elige una imagen para tu avatar");
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM);
    std::wstring path;
    if (SUCCEEDED(dlg->Show(host::Window()))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                path = p;
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    dlg->Release();
    if (path.empty()) return;   // cancelled
    std::wstring err;
    if (!image::ImportAvatar(path, TempAvatarPath(), err)) {
        editor_.error = ToUtf8(err);
        return;
    }
    editor_.custom = true;
    editor_.imagePath = TempAvatarPath();
    editor_.imageVersion = ++g_imageSerial;
    editor_.info = "Imagen lista: se ha recortado en cuadrado.";
}

void App::ImportFromSteam() {
    editor_.error.clear();
    editor_.info.clear();
    std::wstring err;
    const std::optional<SteamSession> s = profiles::ReadSteamSession(err);
    if (!s) {
        editor_.error = ToUtf8(err);
        return;
    }
    if (editor_.creating) {
        for (const Profile& p : profiles_) {
            if (p.steamId == s->steamId) {
                editor_.error = "Ya tienes un perfil con esta cuenta de Steam (" + ToUtf8(p.name) + ").";
                return;
            }
        }
        editor_.steamId = s->steamId;
    }
    strncpy_s(editor_.name, ToUtf8(s->name.substr(0, kMaxNameChars)).c_str(), _TRUNCATE);
    bool avatar = false;
    if (!s->avatarPath.empty()) {
        std::wstring imgErr;
        avatar = image::ImportAvatar(s->avatarPath, TempAvatarPath(), imgErr);
        if (avatar) {
            editor_.custom = true;
            editor_.imagePath = TempAvatarPath();
            editor_.imageVersion = ++g_imageSerial;
        }
    }
    if (editor_.creating)
        editor_.info = avatar ? "Importados tu nombre, tu avatar y tu SteamID de Steam."
                              : "Importados tu nombre y tu SteamID (Steam no tiene tu avatar en caché).";
    else
        editor_.info = avatar ? "Importados tu nombre y tu avatar de Steam." : "Importado tu nombre de Steam.";
}

bool App::SaveEditor() {
    editor_.error.clear();
    std::wstring name = Trim(FromUtf8(editor_.name));
    if (name.empty()) {
        editor_.error = "Escribe un nombre para el perfil.";
        return false;
    }
    name = name.substr(0, kMaxNameChars);
    const bool customFromFile = editor_.custom && editor_.imagePath == TempAvatarPath();

    if (editor_.creating) {
        if (profiles_.size() >= (size_t)kMaxProfiles) {
            editor_.error = "Ya hay 10 perfiles en este PC, el máximo.";
            return false;
        }
        Profile p = profiles::MakeNew(name, profiles_);
        if (editor_.steamId) {
            p.steamId = editor_.steamId;
            p.importedFromSteam = true;
        }
        p.stockAvatar = editor_.stock;
        p.nativeSaves = editor_.offerNative && editor_.native;
        if (!profiles::Save(p)) {
            editor_.error = "No se pudo guardar el perfil en %APPDATA%\\ReFix LAN.";
            return false;
        }
        if (customFromFile && CopyFileW(editor_.imagePath.c_str(), profiles::AvatarPath(p.id).c_str(), FALSE)) {
            p.customAvatar = true;
            profiles::Save(p);
        }
        editor_.savedId = p.id;
        ReFixLog("[Overlay] Profile created: %s (%llu)", ToUtf8(p.id).c_str(), (unsigned long long)p.steamId);
    } else {
        if (!active_) return false;
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
            editor_.error = "No se pudo guardar el perfil.";
            return false;
        }
        active_ = p;
        ++profileVersion_;
        if (renamed) UnrealSteamEmu::Overlay_SetPersonaName(ToUtf8(name));
        editor_.savedId = p.id;
        lastRefresh_ = 0;
    }
    profiles_ = profiles::LoadAll();
    ClearTempAvatar();
    return true;
}

bool App::DrawEditorBody(bool firstRun) {
    const ui::Fonts& f = ui::F();
    const float w = ImGui::GetContentRegionAvail().x;
    const char* title = firstRun ? "Te damos la bienvenida a ReFix LAN"
                                 : editor_.creating ? "Nuevo perfil" : "Editar perfil";
    CenteredText(f.title, ui::color::Text, title, w);
    if (firstRun) {
        CenteredText(f.caption, ui::color::Muted,
                     "Crea tu perfil para jugar en la LAN. Cada perfil tiene su nombre, su avatar, su SteamID y sus "
                     "partidas guardadas.",
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
    if (ui::IconButton("prev", ui::icon::ChevronLeft, "Avatar anterior", ui::color::Text, true, d)) {
        editor_.stock = editor_.custom ? editor_.stock : (editor_.stock + count - 1) % count;
        editor_.custom = false;
    }
    ImGui::SetCursorScreenPos(ImVec2(c.x + r + ui::S(28), c.y - d * 0.5f));
    if (ui::IconButton("next", ui::icon::ChevronRight, "Avatar siguiente", ui::color::Text, true, d)) {
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
        if (ImGui::InvisibleButton("##stock", ImVec2(cell, cell))) {
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
    const std::string imgLabel = std::string(ui::icon::Folder) + "  Importar imagen…";
    const std::string steamLabel = std::string(ui::icon::Import) + "  Importar desde Steam";
    if (ui::SecondaryButton(imgLabel.c_str(), ImVec2(bw, ui::S(40)))) ImportImage();
    ImGui::SameLine(0, ui::S(10));
    if (ui::SecondaryButton(steamLabel.c_str(), ImVec2(bw, ui::S(40)))) ImportFromSteam();
    ui::Tooltip("Usa el nombre, el avatar y el SteamID de la cuenta abierta en Steam");
    ImGui::Dummy(ImVec2(0, ui::S(6)));

    // Name.
    ImGui::PushFont(f.caption);
    ui::TextColored(ui::color::Muted, "Nombre");
    ImGui::PopFont();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool enter = ImGui::InputTextWithHint("##name", "Tu nombre en la LAN", editor_.name, sizeof(editor_.name),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
    if (editor_.offerNative) {
        ImGui::Dummy(ImVec2(0, ui::S(2)));
        ImGui::Checkbox("Usar las partidas guardadas que ya hay en este PC", &editor_.native);
        ui::Tooltip("Si lo desmarcas, este perfil empieza sin partidas y las actuales se guardan aparte");
    }
    if (editor_.steamId) {
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Faint, ("SteamID de Steam: " + std::to_string(editor_.steamId)).c_str());
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
        if (ui::LinkButton("Ahora no")) {
            editor_.savedId.clear();
            done = true;
        }
        ui::Tooltip("Juega con la identidad de este PC; podrás crear el perfil más tarde con Shift+Tab");
    } else {
        if (ui::SecondaryButton("Cancelar", bs) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            editor_.savedId.clear();
            done = true;
        }
    }
    ImGui::SameLine();
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - bs.x, rowY));
    const char* primary = editor_.creating ? "Crear perfil" : "Guardar";
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
        else if (!creating && !editor_.savedId.empty()) ShowInfo("Perfil guardado", "Los cambios ya están aplicados.");
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
                profiles_ = profiles::LoadAll();
            }
        }
    } else {
        DrawPicker();
    }
    ImGui::End();
}

void App::DrawPicker() {
    const ui::Fonts& f = ui::F();
    const float w = ImGui::GetContentRegionAvail().x;
    CenteredText(f.title, ui::color::Text, "¿Quién juega?", w);
    CenteredText(f.caption, ui::color::Muted,
                 "Todos los perfiles están abiertos en otra ventana de este juego. Elige uno que se haya liberado o "
                 "crea uno nuevo.",
                 w);
    ImGui::Dummy(ImVec2(0, ui::S(12)));
    const float footer = ui::S(110);
    ImGui::BeginChild("##profiles", ImVec2(0, ImGui::GetContentRegionAvail().y - footer), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground);
    for (const Profile& p : profiles_) {
        const bool inUse = profiles::IsLockedByOther(p.id, L"");
        const ui::AvatarRef av = ProfileAvatar(p);
        if (MenuRow(ToUtf8(p.id).c_str(), nullptr, &av, ToUtf8(p.name), inUse ? "En uso" : "Libre", !inUse)) {
            if (profiles::TryLock(p.id)) {
                FinishDialog(p);
                break;
            }
        }
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, ui::S(6)));
    const bool canCreate = profiles_.size() < (size_t)kMaxProfiles;
    if (ui::PrimaryButton(canCreate ? "Crear perfil nuevo" : "Máximo de 10 perfiles", ImVec2(w, ui::S(44)), canCreate)) {
        BeginEditor(true);
        dialogEditing_ = true;
    }
    ImGui::Dummy(ImVec2(0, ui::S(4)));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - ImGui::CalcTextSize("Jugar sin perfil").x) * 0.5f);
    if (ui::LinkButton("Jugar sin perfil")) FinishDialog(std::nullopt);
}

} // namespace refix::overlay
