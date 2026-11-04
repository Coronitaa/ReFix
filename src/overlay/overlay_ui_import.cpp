// Profile editor importers: a picture file, the Steam account signed in on this PC, or a profile another
// ReFix game of this PC already uses (all of them live in %APPDATA%\ReFix).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "avatar_image.h"
#include "overlay_i18n.h"
#include "overlay_text.h"
#include "refix_overlay.h"

#include <shobjidl.h>
#include <algorithm>
#include <cstring>

namespace refix::overlay {
using i18n::T;
namespace {

// Folder or file chosen in the shell picker, empty when cancelled.
std::wstring PickPath(bool folders, const wchar_t* title, std::string& error) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        error = T("Could not open the file picker.", "No se pudo abrir el selector de archivos.");
        return L"";
    }
    if (!folders) {
        const COMDLG_FILTERSPEC types[] = {
            { i18n::TW(L"Images", L"Imágenes"), L"*.png;*.jpg;*.jpeg;*.jfif;*.bmp;*.gif;*.tif;*.tiff;*.ico;*.webp;*.heic;*.heif" },
            { i18n::TW(L"All files", L"Todos los archivos"), L"*.*" },
        };
        dlg->SetFileTypes(2, types);
    }
    dlg->SetTitle(title);
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | (folders ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
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
    return path;
}

} // namespace

void App::ImportImage() {
    editor_.error.clear();
    editor_.info.clear();
    const std::wstring path =
        PickPath(false, i18n::TW(L"Choose a picture for your avatar", L"Elige una imagen para tu avatar"), editor_.error);
    if (path.empty()) return;   // cancelled
    std::wstring err;
    if (!image::ImportAvatar(path, EditorTempAvatarPath(), err)) {
        editor_.error = ToUtf8(err);
        return;
    }
    editor_.custom = true;
    editor_.imagePath = EditorTempAvatarPath();
    editor_.imageVersion = NextEditorImageSerial();
    editor_.info = T("Picture ready: it was cropped to a square.", "Imagen lista: se ha recortado en cuadrado.");
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
        for (const Profile& p : allProfiles_) {
            if (p.steamId != s->steamId) continue;
            editor_.error = profiles::UsedIn(p, profiles::GameKey())
                                ? std::string(T("You already have a profile with this Steam account (",
                                                "Ya tienes un perfil con esta cuenta de Steam (")) + ToUtf8(p.name) + ")."
                                : std::string(T("Another game already has a profile with this Steam account (",
                                                "Otro juego ya tiene un perfil con esta cuenta de Steam (")) +
                                      ToUtf8(p.name) + T("): import it from that game.", "): impórtalo de ese juego.");
            return;
        }
        editor_.steamId = s->steamId;
        editor_.steamIdFromSteam = true;
    }
    strncpy_s(editor_.name, ToUtf8(s->name.substr(0, kMaxNameChars)).c_str(), _TRUNCATE);
    bool avatar = false;
    if (!s->avatarPath.empty()) {
        std::wstring imgErr;
        avatar = image::ImportAvatar(s->avatarPath, EditorTempAvatarPath(), imgErr);
        if (avatar) {
            editor_.custom = true;
            editor_.imagePath = EditorTempAvatarPath();
            editor_.imageVersion = NextEditorImageSerial();
        }
    }
    if (editor_.creating)
        editor_.info = avatar ? T("Imported your Steam name, avatar and SteamID.", "Importados tu nombre, tu avatar y tu SteamID de Steam.")
                              : T("Imported your Steam name and SteamID (Steam has no cached avatar).",
                                  "Importados tu nombre y tu SteamID (Steam no tiene tu avatar en caché).");
    else
        editor_.info = avatar ? T("Imported your Steam name and avatar.", "Importados tu nombre y tu avatar de Steam.")
                              : T("Imported your Steam name.", "Importado tu nombre de Steam.");
}

// "Import from another game": every profile of the PC that this game does not list yet, with its picture,
// its name and the game it comes from ("Lethal Company +2" when two more games used it too).
void App::DrawImportPopup() {
    constexpr float kWidth = 640.0f, kCardH = 150.0f, kGap = 12.0f;
    constexpr int kColumns = 3;
    if (importRequested_) {
        ImGui::OpenPopup("##importprofile");
        importRequested_ = false;
    }
    if (!ImGui::IsPopupOpen("##importprofile")) return;
    const ImVec2 vp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(ui::S(kWidth), vp.x - ui::S(24)), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(22)));
    const bool open = ImGui::BeginPopupModal("##importprofile", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                                                         ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleVar();
    if (!open) return;
    const ui::Fonts& f = ui::F();
    ImGui::PushFont(f.heading);
    ImGui::TextUnformatted(T("Import a profile from another game", "Importar un perfil de otro juego"));
    ImGui::PopFont();
    ImGui::PushFont(f.caption);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ui::TextColored(ui::color::Muted, T("It is the same player in both games: same name, picture and SteamID.",
                                        "Es el mismo jugador en los dos juegos: mismo nombre, imagen y SteamID."));
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(8)));

    const std::vector<Profile> others = profiles::FromOtherGames(allProfiles_, profiles::GameKey());
    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = ui::S(kGap), cardH = ui::S(kCardH);
    const float cardW = (avail - gap * (kColumns - 1)) / kColumns;
    const int rows = std::max(1, ((int)others.size() + kColumns - 1) / kColumns);
    const float gridH = std::min(rows * (cardH + gap) - gap, vp.y * 0.6f);
    std::wstring picked;
    ImGui::BeginChild("##importgrid", ImVec2(avail, gridH), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    for (int i = 0; i < (int)others.size(); ++i) {
        const Profile& p = others[i];
        const ImVec2 p0(origin.x + (i % kColumns) * (cardW + gap), origin.y + (i / kColumns) * (cardH + gap));
        const ImVec2 p1(p0.x + cardW, p0.y + cardH);
        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        if (ui::Clicked(ui::NavButton("##other", ImVec2(cardW, cardH)))) picked = p.id;
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        dl->AddRectFilled(p0, p1, hover ? ui::color::RowHover : ui::color::Row, ui::S(14));
        if (hover) dl->AddRect(p0, p1, ui::Fade(ui::color::Accent, 0.7f), ui::S(14), 0, ui::S(1.5f));
        const ImVec2 ac(p0.x + cardW * 0.5f, p0.y + ui::S(18 + 34));
        ui::DrawAvatar(dl, ac, ui::S(34), ProfileAvatar(p));
        ImGui::PushFont(f.strong);
        const std::string name = ui::Ellipsize(ToUtf8(p.name), cardW - ui::S(20));
        const ImVec2 ns = ImGui::CalcTextSize(name.c_str());
        ImGui::PopFont();
        dl->AddText(f.strong, f.strong->FontSize, ImVec2(ac.x - ns.x * 0.5f, p0.y + ui::S(98)), ui::color::Text, name.c_str());
        // Where it comes from: the game it was created in, "+N" for the other games that use it too.
        std::wstring from;
        for (const GameUse& g : p.games) {
            if (g.key == p.originGame) from = g.name;
        }
        if (from.empty() && !p.games.empty()) from = p.games.front().name;
        std::string origin = from.empty() ? std::string(T("Another game", "Otro juego")) : ToUtf8(from);
        ImGui::PushFont(f.caption);
        const std::string more = p.games.size() > 1 ? " +" + std::to_string(p.games.size() - 1) : "";
        origin = ui::Ellipsize(origin, cardW - ui::S(20) - ImGui::CalcTextSize(more.c_str()).x) + more;
        const ImVec2 os = ImGui::CalcTextSize(origin.c_str());
        ImGui::PopFont();
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(ac.x - os.x * 0.5f, p0.y + ui::S(98) + f.strong->FontSize + ui::S(4)),
                    ui::color::Accent, origin.c_str());
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rows * (cardH + gap) - gap));
    ImGui::Dummy(ImVec2(avail, 0));
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, ui::S(10)));
    const float bw = ui::S(140);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - bw);
    const bool cancel = ui::SecondaryButton(T("Cancel", "Cancelar"), ImVec2(bw, ui::S(40))) ||
                        ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
    if (cancel || !picked.empty()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    if (!picked.empty()) ImportProfile(picked);
}

} // namespace refix::overlay
