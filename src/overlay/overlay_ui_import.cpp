// Profile editor importers: a picture file, the Steam account signed in on this PC, or a profile another
// ReFix game of this PC already uses (all of them live in %APPDATA%\ReFix).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "avatar_image.h"
#include "overlay_i18n.h"
#include "overlay_text.h"
#include "profile_sync.h"
#include "refix_overlay.h"
#include "../refix_wine.h"
#if REFIX_STEAM_LINK
#include "../steam_link/steam_link.h"
#endif

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

std::wstring PickFolder(const wchar_t* title, std::string& error) { return PickPath(true, title, error); }

std::wstring PickProfileFile(bool save, const wchar_t* title, const std::wstring& suggested, std::string& error) {
    IFileDialog* dlg = nullptr;
    const HRESULT hr = save ? CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS((IFileSaveDialog**)&dlg))
                            : CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS((IFileOpenDialog**)&dlg));
    if (FAILED(hr)) {
        error = T("Could not open the file picker.", "No se pudo abrir el selector de archivos.");
        return L"";
    }
    const COMDLG_FILTERSPEC types[] = { { i18n::TW(L"ReFix profile", L"Perfil de ReFix"), L"*.rfxp" } };
    dlg->SetFileTypes(1, types);
    dlg->SetDefaultExtension(L"rfxp");
    dlg->SetTitle(title);
    if (save && !suggested.empty()) dlg->SetFileName(suggested.c_str());
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
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

namespace {

struct ImportCardItem {
    bool isExistingProfile = false;
    bool enabled = true;
    std::wstring profileId;
    uint64_t steamId = 0;
    std::wstring name;
    std::string subtitle;
    ImU32 subtitleColor = ui::color::Accent;
    std::wstring avatarPath;
    int stockAvatar = 0;
    bool isSteam = false;
};

std::string ProfileOriginLabel(const Profile& p, bool steamFallback) {
    std::wstring from;
    for (const GameUse& g : p.games) {
        if (g.key == p.originGame) from = g.name;
    }
    if (from.empty() && !p.games.empty()) from = p.games.front().name;
    if (from.empty()) {
        return steamFallback ? T("Steam profile", "Perfil de Steam") : T("Another game", "Otro juego");
    }
    const std::string more = p.games.size() > 1 ? " +" + std::to_string(p.games.size() - 1) : "";
    return ToUtf8(from) + more;
}

template <typename AvatarFn>
int DrawProfileGridModal(const char* popupId, const char* title, const char* subtitle,
                         const std::vector<ImportCardItem>& items, const std::string& emptyMessage,
                         AvatarFn&& avatarForItem) {
    constexpr float kWidth = 640.0f, kCardH = 150.0f, kGap = 12.0f;
    constexpr int kColumns = 3;
    const ImVec2 vp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(ui::S(kWidth), vp.x - ui::S(24)), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(22)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(8));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::color::PanelRaised);
    const bool open = ImGui::BeginPopupModal(popupId, nullptr,
                                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                                 ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!open) return -1;

    const ui::Fonts& f = ui::F();
    ImGui::PushFont(f.heading);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::PushFont(f.caption);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ui::TextColored(ui::color::Muted, subtitle);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, ui::S(10)));

    const float avail = ImGui::GetContentRegionAvail().x;
    int pickedIdx = -1;

    if (items.empty()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cardMin = ImGui::GetCursorScreenPos();
        const float pad = ui::S(14);
        dl->ChannelsSplit(2);
        dl->ChannelsSetCurrent(1);
        ImGui::SetCursorScreenPos(ImVec2(cardMin.x + pad, cardMin.y + pad));
        ImGui::BeginGroup();
        ImGui::PushTextWrapPos(cardMin.x + avail - pad);
        ImGui::PushFont(f.strong);
        ui::TextColored(ui::color::Text, emptyMessage.empty()
                                             ? T("Nothing available to import.", "No hay elementos disponibles para importar.")
                                             : emptyMessage.c_str());
        ImGui::PopFont();
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
        const ImVec2 cardMax(cardMin.x + avail, ImGui::GetItemRectMax().y + pad);
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled(cardMin, cardMax, ui::color::Row, ui::S(8));
        dl->AddRect(cardMin, cardMax, ui::color::Border, ui::S(8), 0, 1.0f);
        dl->ChannelsMerge();
        ImGui::SetCursorScreenPos(ImVec2(cardMin.x, cardMax.y));
    } else {
        const float gap = ui::S(kGap), cardH = ui::S(kCardH);
        const float cardW = (avail - gap * (kColumns - 1)) / kColumns;
        const int rows = std::max(1, ((int)items.size() + kColumns - 1) / kColumns);
        const float gridH = std::min(rows * (cardH + gap) - gap, vp.y * 0.6f);
        std::string childId = std::string(popupId) + "_grid";
        ImGui::BeginChild(childId.c_str(), ImVec2(avail, gridH), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        for (int i = 0; i < (int)items.size(); ++i) {
            const ImportCardItem& item = items[i];
            const ImVec2 p0(origin.x + (i % kColumns) * (cardW + gap), origin.y + (i / kColumns) * (cardH + gap));
            const ImVec2 p1(p0.x + cardW, p0.y + cardH);
            ImGui::SetCursorScreenPos(p0);
            ImGui::PushID(i);
            if (ui::Clicked(ui::NavButton("##card", ImVec2(cardW, cardH))) && item.enabled) pickedIdx = i;
            const bool hover = ImGui::IsItemHovered() || ImGui::IsItemFocused();
            ImGui::PopID();
            if (ImGui::IsItemHovered() && item.enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            const ImU32 fill = (hover && item.enabled) ? ui::color::RowHover : ui::color::Row;
            dl->AddRectFilled(p0, p1, fill, ui::S(14));
            if (hover && item.enabled) dl->AddRect(p0, p1, ui::Fade(ui::color::Accent, 0.7f), ui::S(14), 0, ui::S(1.5f));
            else dl->AddRect(p0, p1, ui::color::Border, ui::S(14), 0, 1.0f);
            const ImVec2 ac(p0.x + cardW * 0.5f, p0.y + ui::S(18 + 34));
            ui::DrawAvatar(dl, ac, ui::S(34), avatarForItem(item));
            ImGui::PushFont(f.strong);
            const std::string name = ui::Ellipsize(ToUtf8(item.name), cardW - ui::S(20));
            const ImVec2 ns = ImGui::CalcTextSize(name.c_str());
            ImGui::PopFont();
            dl->AddText(f.strong, f.strong->FontSize, ImVec2(ac.x - ns.x * 0.5f, p0.y + ui::S(98)),
                        item.enabled ? ui::color::Text : ui::color::Muted, name.c_str());
            ImGui::PushFont(f.caption);
            const std::string sub = ui::Ellipsize(item.subtitle, cardW - ui::S(20));
            const ImVec2 os = ImGui::CalcTextSize(sub.c_str());
            ImGui::PopFont();
            dl->AddText(f.caption, f.caption->FontSize,
                        ImVec2(ac.x - os.x * 0.5f, p0.y + ui::S(98) + f.strong->FontSize + ui::S(4)),
                        item.enabled ? item.subtitleColor : ui::color::Muted, sub.c_str());
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rows * (cardH + gap) - gap));
        ImGui::Dummy(ImVec2(avail, 0));
        ImGui::EndChild();
    }

    ImGui::Dummy(ImVec2(0, ui::S(10)));
    const float bw = ui::S(140);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - bw);
    const bool cancel = ui::SecondaryButton(T("Cancel", "Cancelar"), ImVec2(bw, ui::S(40))) ||
                        ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
    if (cancel || pickedIdx >= 0) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return pickedIdx;
}

} // namespace

void App::ImportFromSteam() {
    editor_.error.clear();
    editor_.info.clear();
#if REFIX_STEAM_LINK
    if (ReFixIsWine() && steamlink::GetState() == steamlink::State::Off) {
        steamlink::Enable();
        editor_.info = T("Connecting to Steam. Try importing again once connected.",
                         "Conectando a Steam. Intenta importar de nuevo cuando se conecte.");
        return;
    }
#endif
    steamImportRequested_ = true;
}

void App::DrawSteamImportPopup() {
    static std::vector<ImportCardItem> s_items;
    static std::string s_emptyMsg;
    if (steamImportRequested_) {
        steamImportRequested_ = false;
        s_items.clear();
        s_emptyMsg.clear();

        auto hasSteamId = [&](uint64_t sid) {
            return std::any_of(s_items.begin(), s_items.end(), [&](const ImportCardItem& c) { return c.steamId == sid; });
        };

        // 1. Existing Steam profiles created across ReFix games on this PC.
        for (const Profile& p : allProfiles_) {
            if (!profiles::IsSteamProfile(p)) continue;
            const bool usedHere = profiles::UsedIn(p, profiles::GameKey());
            ImportCardItem item;
            item.isExistingProfile = true;
            item.profileId = p.id;
            item.steamId = p.steamId;
            item.name = p.name;
            item.stockAvatar = p.stockAvatar;
            item.isSteam = true;
            item.enabled = !editor_.creating || !usedHere;
            item.subtitle = (!item.enabled)
                                ? std::string(T("Already in this game", "Ya en este juego"))
                                : ProfileOriginLabel(p, true);
            item.subtitleColor = ui::color::Accent;
            s_items.push_back(std::move(item));
        }

        // 2. Signed-in Steam session + remembered local Steam accounts (deduplicated against ReFix Steam profiles).
        std::wstring sessionErr;
        const std::optional<SteamSession> activeSession = profiles::ReadSteamSession(sessionErr);
        auto addSession = [&](const SteamSession& s, bool active) {
            if (s.steamId == 0 || hasSteamId(s.steamId)) return;
            const bool usedHere = std::any_of(allProfiles_.begin(), allProfiles_.end(), [&](const Profile& p) {
                return p.steamId == s.steamId && profiles::UsedIn(p, profiles::GameKey());
            });
            ImportCardItem item;
            item.isExistingProfile = false;
            item.steamId = s.steamId;
            item.name = s.name;
            item.avatarPath = s.avatarPath;
            item.stockAvatar = (int)(s.steamId % (uint64_t)ui::StockCount());
            item.isSteam = true;
            item.enabled = !editor_.creating || !usedHere;
            item.subtitle = (!item.enabled)
                                ? std::string(T("Already in this game", "Ya en este juego"))
                                : (active ? std::string(T("Signed in on Steam", "Sesión activa en Steam"))
                                          : std::string(T("Steam account", "Cuenta de Steam")));
            item.subtitleColor = active ? ui::color::SteamOnline : ui::color::Muted;
            s_items.push_back(std::move(item));
        };
        if (activeSession) addSession(*activeSession, true);
        for (const SteamSession& acc : profiles::ReadSteamAccounts()) {
            addSession(acc, activeSession && activeSession->steamId == acc.steamId);
        }

        if (s_items.empty()) {
            s_emptyMsg = !sessionErr.empty()
                             ? ToUtf8(sessionErr)
                             : std::string(T("No Steam profiles or accounts were found on this PC.",
                                             "No se encontraron perfiles ni cuentas de Steam en este PC."));
        }
        ImGui::OpenPopup("##importsteam");
    }
    if (!ImGui::IsPopupOpen("##importsteam")) return;

    const char* title = T("Import from Steam", "Importar desde Steam");
    const char* subtitle = editor_.creating
                               ? T("Pick a Steam profile you already imported in another game or a Steam account on this PC.",
                                   "Elige un perfil de Steam ya importado en otro juego o una cuenta de Steam de este PC.")
                               : T("Pick a Steam profile or Steam account to copy its name and avatar.",
                                   "Elige un perfil o cuenta de Steam para copiar su nombre y avatar.");

    const int picked = DrawProfileGridModal("##importsteam", title, subtitle, s_items, s_emptyMsg, [&](const ImportCardItem& item) {
        if (item.isExistingProfile) {
            auto it = std::find_if(allProfiles_.begin(), allProfiles_.end(), [&](const Profile& p) { return p.id == item.profileId; });
            if (it != allProfiles_.end()) return ProfileAvatar(*it);
        }
        ui::AvatarRef a;
        a.stock = item.stockAvatar;
        a.steam = true;
        if (!item.avatarPath.empty()) {
            const std::string key = "sc:" + std::to_string(item.steamId);
            const std::wstring path = item.avatarPath;
            a.texture = host::Texture(key, [path](image::Rgba& img) { return image::LoadFile(path, img); });
        }
        return a;
    });

    if (picked < 0 || picked >= (int)s_items.size()) return;
    const ImportCardItem& c = s_items[picked];
    if (c.isExistingProfile && editor_.creating) {
        ImportProfile(c.profileId);
        return;
    }

    strncpy_s(editor_.name, ToUtf8(c.name.substr(0, kMaxNameChars)).c_str(), _TRUNCATE);
    if (editor_.creating) {
        editor_.steamId = c.steamId;
        editor_.steamIdFromSteam = true;
    }
    bool avatar = false;
    std::wstring srcAvatar = c.avatarPath;
    if (c.isExistingProfile) {
        auto it = std::find_if(allProfiles_.begin(), allProfiles_.end(), [&](const Profile& p) { return p.id == c.profileId; });
        if (it != allProfiles_.end() && it->customAvatar) srcAvatar = profiles::AvatarPath(it->id);
        else if (it != allProfiles_.end()) {
            editor_.custom = false;
            editor_.stock = it->stockAvatar;
        }
    }
    if (!srcAvatar.empty()) {
        std::wstring imgErr;
        avatar = image::ImportAvatar(srcAvatar, EditorTempAvatarPath(), imgErr);
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
        editor_.info = avatar ? T("Imported Steam name and avatar.", "Importados el nombre y el avatar de Steam.")
                              : T("Imported Steam name.", "Importado el nombre de Steam.");
}

// "Import from another game": non-Steam profiles on this PC that this game does not list yet.
void App::DrawImportPopup() {
    if (importRequested_) {
        ImGui::OpenPopup("##importprofile");
        importRequested_ = false;
    }
    if (!ImGui::IsPopupOpen("##importprofile")) return;

    const std::vector<Profile> others = profiles::FromOtherGames(allProfiles_, profiles::GameKey());
    std::vector<ImportCardItem> items;
    items.reserve(others.size());
    for (const Profile& p : others) {
        ImportCardItem item;
        item.isExistingProfile = true;
        item.profileId = p.id;
        item.steamId = p.steamId;
        item.name = p.name;
        item.subtitle = ProfileOriginLabel(p, false);
        item.subtitleColor = ui::color::Accent;
        item.stockAvatar = p.stockAvatar;
        items.push_back(std::move(item));
    }

    const int picked = DrawProfileGridModal(
        "##importprofile",
        T("Import a profile from another game", "Importar un perfil de otro juego"),
        T("It is the same player in both games: same name, picture and SteamID.",
          "Es el mismo jugador en los dos juegos: mismo nombre, imagen y SteamID."),
        items,
        T("Your other ReFix games have no local profiles to import.",
          "Tus otros juegos con ReFix no tienen perfiles locales para importar."),
        [&](const ImportCardItem& item) {
            auto it = std::find_if(others.begin(), others.end(), [&](const Profile& p) { return p.id == item.profileId; });
            return it != others.end() ? ProfileAvatar(*it) : ui::AvatarRef{};
        });
    if (picked >= 0 && picked < (int)items.size()) ImportProfile(items[picked].profileId);
}

void App::DrawCloudImportModal() {
    if (!sync::HasCloudImportModal()) return;
    auto& items = sync::GetCloudImportItems();
    if (items.empty()) return;

    if (!ImGui::IsPopupOpen("##cloud_import_modal")) {
        ImGui::OpenPopup("##cloud_import_modal");
    }

    const ImVec2 vp = ImGui::GetIO().DisplaySize;
    constexpr float kWidth = 720.0f;
    constexpr float kCardH = 145.0f;
    constexpr float kGap = 12.0f;
    constexpr int kColumns = 3;

    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(ui::S(kWidth), vp.x - ui::S(24)), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(20)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(14));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::color::PanelRaised);

    const bool open = ImGui::BeginPopupModal("##cloud_import_modal", nullptr,
                                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!open) return;

    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Header
    ImGui::PushFont(f.heading);
    ui::TextColored(ui::color::Accent, ui::icon::Cloud);
    ImGui::SameLine();
    ImGui::TextUnformatted(T("Cloud Profiles Detected", "Perfiles detectados en la nube"));
    ImGui::PopFont();

    ImGui::PushFont(f.caption);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ui::TextColored(ui::color::Muted,
                    T("Existing profiles were detected in your cloud sync. Select which profiles you want to import to this PC:",
                      "Se detectaron perfiles existentes en la nube. Selecciona cuáles deseas importar a este equipo:"));
    ImGui::PopTextWrapPos();
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, ui::S(10)));

    // Grid of cards
    const float avail = ImGui::GetContentRegionAvail().x;
    const float gap = ui::S(kGap);
    const float cardH = ui::S(kCardH);
    const float cardW = (avail - gap * (kColumns - 1)) / kColumns;
    const int rows = std::max(1, ((int)items.size() + kColumns - 1) / kColumns);
    const float gridH = std::min(rows * (cardH + gap) - gap, vp.y * 0.55f);

    ImGui::BeginChild("##cloudgrid", ImVec2(avail, gridH), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    for (int i = 0; i < (int)items.size(); ++i) {
        auto& item = items[i];
        const ImVec2 p0(origin.x + (i % kColumns) * (cardW + gap), origin.y + (i / kColumns) * (cardH + gap));
        const ImVec2 p1(p0.x + cardW, p0.y + cardH);

        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        if (ui::Clicked(ui::NavButton("##cloudcard", ImVec2(cardW, cardH)))) {
            item.selected = !item.selected;
        }
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        // Card background
        dl->AddRectFilled(p0, p1, item.selected ? ui::color::AccentSoft : (hover ? ui::color::RowHover : ui::color::Row), ui::S(14));
        if (item.selected) {
            dl->AddRect(p0, p1, ui::color::Accent, ui::S(14), 0, ui::S(1.5f));
        } else if (hover) {
            dl->AddRect(p0, p1, ui::Fade(ui::color::Accent, 0.5f), ui::S(14), 0, ui::S(1.0f));
        }

        // Selection badge / check circle at top right
        const ImVec2 checkC(p1.x - ui::S(18), p0.y + ui::S(18));
        if (item.selected) {
            dl->AddCircleFilled(checkC, ui::S(10), ui::color::Accent, 24);
            dl->AddText(f.caption, f.caption->FontSize, ImVec2(checkC.x - ui::S(6), checkC.y - ui::S(7)), ui::color::AccentInk, ui::icon::Check);
        } else {
            dl->AddCircle(checkC, ui::S(10), ui::color::Faint, 24, ui::S(1.5f));
        }

        // Avatar
        const ImVec2 ac(p0.x + cardW * 0.5f, p0.y + ui::S(18 + 30));
        ui::AvatarRef av;
        av.stock = item.stockAvatar;
        av.steam = (item.steamId != 0);
        ui::DrawAvatar(dl, ac, ui::S(28), av);

        // Name
        ImGui::PushFont(f.strong);
        const std::string name = ui::Ellipsize(ToUtf8(item.name), cardW - ui::S(24));
        const ImVec2 ns = ImGui::CalcTextSize(name.c_str());
        ImGui::PopFont();
        dl->AddText(f.strong, f.strong->FontSize, ImVec2(ac.x - ns.x * 0.5f, p0.y + ui::S(86)), ui::color::Text, name.c_str());

        // Origin game display: "Lethal Company +5 más"
        ImGui::PushFont(f.caption);
        std::string disp = ui::Ellipsize(item.originDisplay, cardW - ui::S(24));
        const ImVec2 ds = ImGui::CalcTextSize(disp.c_str());
        ImGui::PopFont();
        dl->AddText(f.caption, f.caption->FontSize, ImVec2(ac.x - ds.x * 0.5f, p0.y + ui::S(86) + f.strong->FontSize + ui::S(4)),
                    item.hasCurrentGame ? ui::color::Accent : ui::color::Muted, disp.c_str());
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rows * (cardH + gap) - gap));
    ImGui::Dummy(ImVec2(avail, 0));
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, ui::S(12)));

    // Actions footer
    int selectedCount = 0;
    for (const auto& item : items) if (item.selected) ++selectedCount;

    const char* toggleAllText = (selectedCount == (int)items.size()) ?
        T("Deselect all", "Desmarcar todos") : T("Select all", "Seleccionar todos");
    if (ui::LinkButton(toggleAllText)) {
        const bool selectAll = (selectedCount != (int)items.size());
        for (auto& item : items) item.selected = selectAll;
    }

    ImGui::SameLine();
    const float btnGap = ui::S(10);
    const float totalBtnW = ui::S(190) + ui::S(110) + btnGap;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - totalBtnW);

    char impBuf[64];
    snprintf(impBuf, sizeof(impBuf), "%s (%d)", T("Import selected", "Importar seleccionados"), selectedCount);
    if (ui::PrimaryButton(impBuf, ImVec2(ui::S(190), ui::S(38)), selectedCount > 0)) {
        sync::ConfirmCloudImport(items);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine(0, btnGap);
    if (ui::SecondaryButton(T("Cancel", "Cancelar"), ImVec2(ui::S(110), ui::S(38))) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
        sync::DismissCloudImport();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

} // namespace refix::overlay
