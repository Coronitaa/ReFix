// See identity_import.h. Sources, best first:
//   1. ReFix.log next to ReFix.ini: the SteamID and name the emulator really used ("[ConfigDiag] ... Source").
//   2. ReFix.ini [User] SteamId / Name.
//   3. steam_settings (force_steamid.txt, user_steam_id.txt, force_account_name.txt, configs.user.ini) and
//      its account_avatar picture.
//   4. IdentityMode=auto (the default) without a fixed SteamId: the SteamID this PC derives.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "identity_import.h"
#include "overlay_i18n.h"

#include <windows.h>
#include <deque>
#include <utility>
#include <vector>

namespace refix::overlay::profiles {
namespace {

constexpr int kMaxDepth = 4;                 // game root -> <Game>_Data/Plugins/x86_64 and the like
constexpr size_t kMaxDirsScanned = 4000;
constexpr size_t kMaxTextBytes = 8u << 20;   // logs are read whole up to this size (tail kept)
constexpr uint64_t kSteamIdMin = 76561197960265729ULL;
constexpr uint64_t kSteamIdMax = 76561202255233023ULL;

bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

bool IsDir(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring Trim(std::wstring s) {
    const wchar_t* ws = L" \t\r\n\"";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::wstring::npos) return L"";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

// Text file as UTF-8 (falls back to the ANSI code page, understands UTF-16 with a BOM).
std::wstring ReadText(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    LARGE_INTEGER size{};
    GetFileSizeEx(h, &size);
    if (size.QuadPart > (LONGLONG)kMaxTextBytes) {
        LARGE_INTEGER pos{};
        pos.QuadPart = size.QuadPart - (LONGLONG)kMaxTextBytes;
        SetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
    }
    std::vector<char> buf((size_t)std::min<LONGLONG>(size.QuadPart, (LONGLONG)kMaxTextBytes));
    DWORD got = 0;
    if (!buf.empty()) ReadFile(h, buf.data(), (DWORD)buf.size(), &got, nullptr);
    CloseHandle(h);
    buf.resize(got);
    if (buf.size() >= 2 && (unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE)
        return std::wstring((const wchar_t*)(buf.data() + 2), (buf.size() - 2) / sizeof(wchar_t));
    size_t skip = (buf.size() >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB &&
                   (unsigned char)buf[2] == 0xBF) ? 3 : 0;
    const int n = (int)(buf.size() - skip);
    if (n <= 0) return L"";
    UINT cp = CP_UTF8;
    int len = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, buf.data() + skip, n, nullptr, 0);
    if (len <= 0) { cp = CP_ACP; len = MultiByteToWideChar(cp, 0, buf.data() + skip, n, nullptr, 0); }
    std::wstring out(len, L'\0');
    MultiByteToWideChar(cp, 0, buf.data() + skip, n, out.data(), len);
    return out;
}

std::vector<std::wstring> Lines(const std::wstring& text) {
    std::vector<std::wstring> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) end = text.size();
        out.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

// Value of `key` in `[section]` of an ini text; empty when missing.
std::wstring IniValue(const std::wstring& text, const wchar_t* section, const wchar_t* key) {
    bool inSection = false;
    for (std::wstring line : Lines(text)) {
        line = Trim(line);
        if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
        if (line[0] == L'[') {
            const size_t close = line.find(L']');
            inSection = close != std::wstring::npos && _wcsicmp(line.substr(1, close - 1).c_str(), section) == 0;
            continue;
        }
        const size_t eq = line.find(L'=');
        if (inSection && eq != std::wstring::npos && _wcsicmp(Trim(line.substr(0, eq)).c_str(), key) == 0)
            return Trim(line.substr(eq + 1));
    }
    return L"";
}

uint64_t ParseSteamId(const std::wstring& s) {
    const std::wstring t = Trim(s);
    if (t.empty() || t.find_first_not_of(L"0123456789") != std::wstring::npos) return 0;
    const uint64_t id = _wcstoui64(t.c_str(), nullptr, 10);
    return id >= kSteamIdMin && id <= kSteamIdMax ? id : 0;
}

// Breadth-first search under `root` for a file or folder called `name` (shallowest wins).
std::wstring FindUnder(const std::wstring& root, const wchar_t* name, bool wantDir) {
    std::deque<std::pair<std::wstring, int>> queue{ { root, 0 } };
    size_t scanned = 0;
    while (!queue.empty() && scanned < kMaxDirsScanned) {
        auto [dir, depth] = queue.front();
        queue.pop_front();
        ++scanned;
        const std::wstring direct = dir + L"\\" + name;
        if (wantDir ? IsDir(direct) : (Exists(direct) && !IsDir(direct))) return direct;
        if (depth >= kMaxDepth) continue;
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories,
                                    nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                continue;
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            queue.emplace_back(dir + L"\\" + fd.cFileName, depth + 1);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return L"";
}

std::wstring DirOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(0, slash);
}

std::wstring LeafOf(std::wstring path) {
    while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

// "...[ConfigDiag] <what> Source: '<source>' -> <value>": last occurrence in the log.
bool LastDiag(const std::vector<std::wstring>& lines, const wchar_t* what, std::wstring& source, std::wstring& value) {
    const std::wstring tag = std::wstring(L"[ConfigDiag] ") + what + L" Source: '";
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        const size_t at = it->find(tag);
        if (at == std::wstring::npos) continue;
        const size_t s0 = at + tag.size();
        const size_t s1 = it->find(L"' -> ", s0);
        if (s1 == std::wstring::npos) continue;
        source = it->substr(s0, s1 - s0);
        value = Trim(it->substr(s1 + 5));
        if (value.size() >= 2 && value.front() == L'\'' && value.back() == L'\'') value = value.substr(1, value.size() - 2);
        return true;
    }
    return false;
}

std::optional<ImportedIdentity> FromProfileFolder(const std::wstring& folder) {
    const std::wstring ini = ReadText(folder + L"\\profile.ini");
    ImportedIdentity id;
    id.name = IniValue(ini, L"Profile", L"Name");
    id.steamId = ParseSteamId(IniValue(ini, L"Profile", L"SteamId"));
    if (_wcsicmp(IniValue(ini, L"Profile", L"Avatar").c_str(), L"custom") == 0 && Exists(folder + L"\\avatar.png"))
        id.avatarPath = folder + L"\\avatar.png";
    id.gameLabel = LeafOf(folder);
    if (id.name.empty() && !id.steamId) return std::nullopt;
    return id;
}

void ReadSteamSettings(const std::wstring& dir, ImportedIdentity& id) {
    if (dir.empty()) return;
    const std::wstring user = ReadText(dir + L"\\configs.user.ini");
    if (!id.steamId) id.steamId = ParseSteamId(ReadText(dir + L"\\force_steamid.txt"));
    if (!id.steamId) id.steamId = ParseSteamId(ReadText(dir + L"\\user_steam_id.txt"));
    if (!id.steamId) id.steamId = ParseSteamId(IniValue(user, L"user::general", L"account_steamid"));
    if (id.name.empty()) id.name = Trim(ReadText(dir + L"\\force_account_name.txt"));
    if (id.name.empty()) id.name = IniValue(user, L"user::general", L"account_name");
    for (const wchar_t* pic : { L"account_avatar.png", L"account_avatar.jpg", L"account_avatar.jpeg",
                                L"settings\\account_avatar.png", L"settings\\account_avatar.jpg" }) {
        if (Exists(dir + L"\\" + pic)) { id.avatarPath = dir + L"\\" + pic; break; }
    }
}

} // namespace

std::optional<ImportedIdentity> ReadReFixGame(const std::wstring& folderIn, uint64_t machineSteamId, std::wstring& err) {
    std::wstring folder = folderIn;
    while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/')) folder.pop_back();
    if (!IsDir(folder)) {
        err = i18n::TW(L"That folder does not exist.", L"Esa carpeta no existe.");
        return std::nullopt;
    }
    if (Exists(folder + L"\\profile.ini")) {
        if (auto p = FromProfileFolder(folder)) return p;
        err = i18n::TW(L"That ReFix LAN profile has no name or SteamID.", L"Ese perfil de ReFix LAN no tiene nombre ni SteamID.");
        return std::nullopt;
    }

    const std::wstring iniPath = FindUnder(folder, L"ReFix.ini", false);
    const std::wstring settings = FindUnder(iniPath.empty() ? folder : DirOf(iniPath), L"steam_settings", true);
    if (iniPath.empty() && settings.empty()) {
        err = i18n::TW(L"No ReFix was found in that folder. Choose the folder of a game patched with ReFix.",
                       L"No se encontró ReFix en esa carpeta. Elige la carpeta de un juego parcheado con ReFix.");
        return std::nullopt;
    }

    ImportedIdentity id;
    id.gameLabel = LeafOf(iniPath.empty() ? folder : DirOf(iniPath));
    std::wstring identityMode;
    if (!iniPath.empty()) {
        // What the emulator really used last time, when that game keeps a log.
        const std::vector<std::wstring> log = Lines(ReadText(DirOf(iniPath) + L"\\ReFix.log"));
        std::wstring src, value;
        if (LastDiag(log, L"SteamID", src, value)) {
            if (src == L"profile") {
                err = i18n::TW(L"That game already uses your ReFix LAN profiles: they are the same in every game of this PC.",
                               L"Ese juego ya usa tus perfiles de ReFix LAN: son los mismos en todos los juegos de este PC.");
                return std::nullopt;
            }
            id.steamId = ParseSteamId(value);
            id.machineId = id.steamId != 0 && id.steamId == machineSteamId;
        }
        if (LastDiag(log, L"Persona Name", src, value) && src != L"profile") id.name = value;

        const std::wstring ini = ReadText(iniPath);
        identityMode = IniValue(ini, L"User", L"IdentityMode");
        if (!id.steamId) id.steamId = ParseSteamId(IniValue(ini, L"User", L"SteamId"));
        if (id.name.empty()) id.name = IniValue(ini, L"User", L"Name");
    }
    ReadSteamSettings(settings, id);
    if (!id.steamId && _wcsicmp(identityMode.c_str(), L"fixed") != 0 && machineSteamId) {
        id.steamId = machineSteamId;
        id.machineId = true;
    }
    if (id.name.empty() && !id.steamId) {
        err = i18n::TW(L"That game's ReFix has no name or SteamID to import.",
                       L"El ReFix de ese juego no tiene nombre ni SteamID que importar.");
        return std::nullopt;
    }
    return id;
}

} // namespace refix::overlay::profiles
