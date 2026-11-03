// Profile editor importers: a picture file, the Steam account signed in on this PC, or the identity another
// ReFix game uses (identity_import.cpp).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "avatar_image.h"
#include "identity_import.h"
#include "overlay_i18n.h"
#include "overlay_text.h"
#include "refix_overlay.h"

#include <shobjidl.h>
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
        for (const Profile& p : profiles_) {
            if (p.steamId == s->steamId) {
                editor_.error = std::string(T("You already have a profile with this Steam account (",
                                              "Ya tienes un perfil con esta cuenta de Steam (")) + ToUtf8(p.name) + ").";
                return;
            }
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

void App::ImportFromReFixGame() {
    editor_.error.clear();
    editor_.info.clear();
    const std::wstring folder = PickPath(true,
                                         i18n::TW(L"Choose the folder of a game patched with ReFix",
                                                  L"Elige la carpeta de un juego parcheado con ReFix"),
                                         editor_.error);
    if (folder.empty()) return;   // cancelled
    std::wstring err;
    const auto id = profiles::ReadReFixGame(folder, UnrealSteamEmu::Overlay_MachineSteamID(), err);
    if (!id) {
        editor_.error = ToUtf8(err);
        return;
    }
    if (editor_.creating && id->steamId) {
        for (const Profile& p : profiles_) {
            if (p.steamId == id->steamId) {
                editor_.error = std::string(T("You already have a profile with this SteamID (",
                                              "Ya tienes un perfil con este SteamID (")) + ToUtf8(p.name) + ").";
                return;
            }
        }
        editor_.steamId = id->steamId;
        editor_.steamIdFromSteam = false;
    }
    if (!id->name.empty()) strncpy_s(editor_.name, ToUtf8(id->name.substr(0, kMaxNameChars)).c_str(), _TRUNCATE);
    bool avatar = false;
    if (!id->avatarPath.empty()) {
        std::wstring imgErr;
        avatar = image::ImportAvatar(id->avatarPath, EditorTempAvatarPath(), imgErr);
        if (avatar) {
            editor_.custom = true;
            editor_.imagePath = EditorTempAvatarPath();
            editor_.imageVersion = NextEditorImageSerial();
        }
    }
    std::string msg = std::string(T("Imported from ", "Importado de ")) + ToUtf8(id->gameLabel) + ": ";
    msg += T("name", "nombre");
    if (editor_.creating && id->steamId) msg += T(", SteamID", ", SteamID");
    if (avatar) msg += T(", picture", ", imagen");
    msg += ".";
    if (editor_.creating && id->machineId)
        msg += T(" That game uses this PC's automatic SteamID.", " Ese juego usa el SteamID automático de este PC.");
    if (!avatar) msg += T(" It has no picture: pick one above.", " No tiene imagen: elige una arriba.");
    editor_.info = msg;
}

} // namespace refix::overlay
