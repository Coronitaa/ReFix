#define _CRT_RAND_S
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "profile_store.h"
#include "overlay_i18n.h"
#include "overlay_text.h"

#include <shlobj.h>
#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::profiles {
namespace {

constexpr uint64_t kSteamIdBase = 76561197960265728ULL;   // individual account, public universe
constexpr uint32_t kGeneratedAccountMin = 100000000;
constexpr uint32_t kGeneratedAccountSpan = 0x1FFFFFFF;
constexpr DWORD kLockPollMs = 200;

HANDLE g_lock = nullptr;
HANDLE g_anyLock = nullptr;
std::wstring g_lockedId;

std::wstring IniPath(const std::wstring& id) { return ProfileDir(id) + L"\\profile.ini"; }
std::wstring IndexPath() { return RootDir() + L"\\profiles.ini"; }

std::wstring LockName(const std::wstring& id) {
    return L"Local\\ReFixLAN_Profile_" + ExeKey() + L"_" + id;
}

// Held next to the per-exe lock: tells every game that the profile is in use somewhere.
std::wstring AnyLockName(const std::wstring& id) { return L"Local\\ReFixLAN_ProfileAny_" + id; }

std::wstring GuestRoot() {
    wchar_t tmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmp);
    return std::wstring(tmp) + L"ReFix";
}

constexpr wchar_t kRootName[] = L"\\ReFix";
constexpr wchar_t kOldRootName[] = L"\\ReFix LAN";


void DeleteTree(const std::wstring& path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((path + L"\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) DeleteTree(path + L"\\" + fd.cFileName);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        RemoveDirectoryW(path.c_str());
    } else {
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(path.c_str());
    }
}

void EnsureDir(const std::wstring& path) {
    for (size_t i = 3; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == L'\\') CreateDirectoryW(path.substr(0, i).c_str(), nullptr);
    }
}

// WritePrivateProfileStringW keeps Unicode only when the file already starts with a UTF-16 BOM.
void EnsureUnicodeIni(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    const WORD bom = 0xFEFF;
    DWORD written = 0;
    WriteFile(h, &bom, sizeof(bom), &written, nullptr);
    CloseHandle(h);
}

std::wstring ReadIni(const std::wstring& file, const wchar_t* section, const wchar_t* key) {
    wchar_t buf[2048] = {};
    GetPrivateProfileStringW(section, key, L"", buf, 2048, file.c_str());
    return buf;
}

void WriteIni(const std::wstring& file, const wchar_t* section, const wchar_t* key, const std::wstring& value) {
    WritePrivateProfileStringW(section, key, value.c_str(), file.c_str());
}

uint64_t Now() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

uint32_t Random32() {
    unsigned int v = 0;
    if (rand_s(&v) != 0) v = (unsigned int)(GetTickCount64() * 2654435761u);
    return v;
}

std::wstring Hex8(uint32_t v) {
    wchar_t buf[16];
    swprintf_s(buf, L"%08x", v);
    return buf;
}

std::wstring JoinIds(const std::vector<uint64_t>& ids) {
    std::wstring out;
    for (uint64_t id : ids) {
        if (!out.empty()) out += L',';
        out += std::to_wstring(id);
    }
    return out;
}

std::vector<uint64_t> SplitIds(const std::wstring& s) {
    std::vector<uint64_t> out;
    std::wstringstream ss(s);
    std::wstring tok;
    while (std::getline(ss, tok, L',')) {
        uint64_t v = _wcstoui64(tok.c_str(), nullptr, 10);
        if (v != 0 && std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
    }
    return out;
}

bool IsValidId(const std::wstring& id) {
    if (id.size() != 8) return false;
    return std::all_of(id.begin(), id.end(), [](wchar_t c) { return iswxdigit(c) != 0; });
}

std::wstring SteamPath() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &key) != ERROR_SUCCESS) return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    LONG res = RegQueryValueExW(key, L"SteamPath", nullptr, &type, (LPBYTE)buf, &size);
    RegCloseKey(key);
    if (res != ERROR_SUCCESS) return {};
    std::wstring p = buf;
    std::replace(p.begin(), p.end(), L'/', L'\\');
    return p;
}

bool ReadActiveProcess(DWORD& pid, DWORD& accountId) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    DWORD size = sizeof(DWORD);
    pid = 0;
    accountId = 0;
    RegQueryValueExW(key, L"pid", nullptr, nullptr, (LPBYTE)&pid, &size);
    size = sizeof(DWORD);
    RegQueryValueExW(key, L"ActiveUser", nullptr, nullptr, (LPBYTE)&accountId, &size);
    RegCloseKey(key);
    return true;
}

bool IsProcessAlive(DWORD pid) {
    if (pid == 0) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code = 0;
    bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
}

// "PersonaName" of `steamId` in Steam's config\loginusers.vdf.
std::wstring PersonaFromLoginUsers(const std::wstring& steamPath, uint64_t steamId) {
    std::ifstream f(steamPath + L"\\config\\loginusers.vdf", std::ios::binary);
    if (!f) return {};
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t at = text.find("\"" + std::to_string(steamId) + "\"");
    if (at == std::string::npos) return {};
    size_t blockEnd = text.find('}', at);
    size_t key = text.find("\"PersonaName\"", at);
    if (key == std::string::npos || (blockEnd != std::string::npos && key > blockEnd)) return {};
    size_t open = text.find('"', key + 13);
    size_t close = open == std::string::npos ? open : text.find('"', open + 1);
    if (close == std::string::npos) return {};
    return FromUtf8(text.substr(open + 1, close - open - 1));
}

// Moves `from` into `to` without replacing anything: missing entries are moved, folders present on both
// sides are merged one level further down. Returns the number of entries that could not move.
int MergeInto(const std::wstring& from, const std::wstring& to) {
    CreateDirectoryW(to.c_str(), nullptr);
    int left = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((from + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        const std::wstring src = from + L"\\" + fd.cFileName, dst = to + L"\\" + fd.cFileName;
        const DWORD dstAttr = GetFileAttributesW(dst.c_str());
        if (dstAttr == INVALID_FILE_ATTRIBUTES) {
            if (!MoveFileExW(src.c_str(), dst.c_str(), 0)) ++left;
        } else if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (dstAttr & FILE_ATTRIBUTE_DIRECTORY)) {
            if (MergeInto(src, dst) == 0) RemoveDirectoryW(src.c_str());
            else ++left;
        } else {
            ++left;   // same file on both sides: the one already in place wins
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return left;
}

// saves.ini owners the new file does not have yet (the new file wins for the rest).
void MergeOwners(const std::wstring& from, const std::wstring& to) {
    std::vector<wchar_t> sec(32768, L'\0');
    const DWORD n = GetPrivateProfileSectionW(L"Owners", sec.data(), (DWORD)sec.size(), from.c_str());
    for (const wchar_t* e = sec.data(); e < sec.data() + n && *e; e += wcslen(e) + 1) {
        const wchar_t* eq = wcschr(e, L'=');
        if (!eq || eq == e) continue;
        const std::wstring key(e, eq);
        wchar_t cur[64] = {};
        if (GetPrivateProfileStringW(L"Owners", key.c_str(), L"", cur, 64, to.c_str()) == 0)
            WritePrivateProfileStringW(L"Owners", key.c_str(), eq + 1, to.c_str());
    }
}

// %APPDATA%\ReFix LAN became %APPDATA%\ReFix. That folder can already exist (ReFix Online's EOS identity
// keeps user_profile.json there, or an earlier start moved part of the data), so the old folder is merged
// into it without replacing anything: profiles and stashed saves are moved, saves.ini owners combined,
// profiles.ini and the browser cache of the new folder kept. What is in use right now (the browser data of a
// running game) stays and moves on a later start.
void MigrateOldRoot(const std::wstring& roaming) {
    const std::wstring oldRoot = roaming + kOldRootName, newRoot = roaming + kRootName;
    if (GetFileAttributesW(oldRoot.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    HANDLE mu = CreateMutexW(nullptr, FALSE, L"Local\\ReFix_RootMigration");
    if (mu) WaitForSingleObject(mu, 5000);
    CreateDirectoryW(newRoot.c_str(), nullptr);
    const std::wstring oldOwners = oldRoot + L"\\saves.ini", newOwners = newRoot + L"\\saves.ini";
    if (GetFileAttributesW(oldOwners.c_str()) != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW(newOwners.c_str()) != INVALID_FILE_ATTRIBUTES) {
        MergeOwners(oldOwners, newOwners);
        DeleteFileW(oldOwners.c_str());
    }
    for (const wchar_t* dup : { L"\\profiles.ini", L"\\WebView2" }) {   // settings and cache: the new ones win
        if (GetFileAttributesW((newRoot + dup).c_str()) != INVALID_FILE_ATTRIBUTES) DeleteTree(oldRoot + dup);
    }
    const int left = MergeInto(oldRoot, newRoot);
    if (left == 0) RemoveDirectoryW(oldRoot.c_str());
    ReFixLog("[Profiles] %%APPDATA%%\\ReFix LAN merged into %%APPDATA%%\\ReFix%s",
             left ? " (some files are in use or exist on both sides; they stay for a later start)" : "");
    if (mu) {
        ReleaseMutex(mu);
        CloseHandle(mu);
    }
}

} // namespace

std::wstring RootDir() {
    static std::once_flag once;
    PWSTR roaming = nullptr;
    std::wstring root;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &roaming))) root = roaming;
    if (roaming) CoTaskMemFree(roaming);
    if (root.empty()) {
        wchar_t buf[MAX_PATH] = {};
        GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
        root = buf;
    }
    std::call_once(once, [&] { MigrateOldRoot(root); });
    return root + kRootName;
}

std::wstring ProfileDir(const std::wstring& id) { return RootDir() + L"\\Profiles\\" + id; }
std::wstring AvatarPath(const std::wstring& id) { return ProfileDir(id) + L"\\avatar.png"; }
std::wstring DataRoot(const std::wstring& id) { return ProfileDir(id) + L"\\Data"; }
std::wstring CloudRoot(const std::wstring& id, uint32_t appId) {
    return ProfileDir(id) + L"\\Cloud\\" + std::to_wstring(appId);
}

std::optional<Profile> Load(const std::wstring& id) {
    if (!IsValidId(id)) return std::nullopt;
    const std::wstring ini = IniPath(id);
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) return std::nullopt;
    Profile p;
    p.id = id;
    p.name = ReadIni(ini, L"Profile", L"Name");
    p.steamId = _wcstoui64(ReadIni(ini, L"Profile", L"SteamId").c_str(), nullptr, 10);
    if (p.name.empty() || p.steamId == 0) return std::nullopt;
    const std::wstring avatar = ReadIni(ini, L"Profile", L"Avatar");
    p.customAvatar = avatar == L"custom" && GetFileAttributesW(AvatarPath(id).c_str()) != INVALID_FILE_ATTRIBUTES;
    p.stockAvatar = std::clamp(_wtoi(avatar.c_str()), 0, kStockAvatarCount - 1);
    p.nativeSaves = ReadIni(ini, L"Profile", L"NativeSaves") == L"1";
    p.importedFromSteam = ReadIni(ini, L"Profile", L"ImportedFromSteam") == L"1";
    p.lastUsed = _wcstoui64(ReadIni(ini, L"Profile", L"LastUsed").c_str(), nullptr, 10);
    p.favorites = SplitIds(ReadIni(ini, L"Favorites", L"List"));
    p.originGame = ReadIni(ini, L"Profile", L"OriginGame");
    // [Games] key=name, one line per game that used the profile.
    std::vector<wchar_t> sec(16384, L'\0');
    const DWORD n = GetPrivateProfileSectionW(L"Games", sec.data(), (DWORD)sec.size(), ini.c_str());
    for (const wchar_t* e = sec.data(); e < sec.data() + n && *e; e += wcslen(e) + 1) {
        const wchar_t* eq = wcschr(e, L'=');
        if (!eq || eq == e) continue;
        GameUse g{ std::wstring(e, eq), eq + 1 };
        if (g.key == p.originGame) p.games.insert(p.games.begin(), g);
        else p.games.push_back(g);
    }
    return p;
}

std::vector<Profile> LoadAll() {
    std::vector<Profile> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((RootDir() + L"\\Profiles\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (auto p = Load(fd.cFileName)) out.push_back(std::move(*p));
        } while (FindNextFileW(h, &fd) && out.size() < (size_t)kMaxStoredProfiles);
        FindClose(h);
    }
    std::sort(out.begin(), out.end(), [](const Profile& a, const Profile& b) { return a.lastUsed > b.lastUsed; });
    return out;
}

bool Save(const Profile& p) {
    if (!IsValidId(p.id)) return false;
    EnsureDir(ProfileDir(p.id));
    const std::wstring ini = IniPath(p.id);
    EnsureUnicodeIni(ini);
    WriteIni(ini, L"Profile", L"Name", p.name);
    WriteIni(ini, L"Profile", L"SteamId", std::to_wstring(p.steamId));
    WriteIni(ini, L"Profile", L"Avatar", p.customAvatar ? L"custom" : std::to_wstring(p.stockAvatar));
    WriteIni(ini, L"Profile", L"NativeSaves", p.nativeSaves ? L"1" : L"0");
    WriteIni(ini, L"Profile", L"ImportedFromSteam", p.importedFromSteam ? L"1" : L"0");
    WriteIni(ini, L"Profile", L"LastUsed", std::to_wstring(p.lastUsed));
    WriteIni(ini, L"Favorites", L"List", JoinIds(p.favorites));
    WriteIni(ini, L"Profile", L"OriginGame", p.originGame);
    std::wstring games;
    for (const GameUse& g : p.games) games += g.key + L"=" + g.name + L'\0';
    games += L'\0';
    WritePrivateProfileSectionW(L"Games", games.c_str(), ini.c_str());
    return GetFileAttributesW(ini.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool Remove(const std::wstring& id) {
    if (!IsValidId(id) || IsInUseAnywhere(id)) return false;
    // The definition goes first: a half-deleted folder never shows up as a profile again.
    if (!DeleteFileW(IniPath(id).c_str())) return false;
    DeleteTree(ProfileDir(id));
    if (LastProfileId() == id) SetLastProfileId(L"");
    ReFixLog("[Profiles] Profile %s deleted", ToUtf8(id).c_str());
    return true;
}

Profile MakeGuest(const std::wstring& name, const std::vector<Profile>& existing) {
    Profile p = MakeNew(name, existing);
    p.id = L"guest";
    p.guest = true;
    return p;
}

std::wstring GuestCloudRoot(uint32_t appId) {
    return GuestRoot() + L"\\Guest-" + std::to_wstring(GetCurrentProcessId()) + L"\\Cloud\\" + std::to_wstring(appId);
}

void CleanupGuestFolders() {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((GuestRoot() + L"\\Guest-*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const DWORD pid = wcstoul(fd.cFileName + 6, nullptr, 10);
        if (pid == GetCurrentProcessId()) continue;
        HANDLE proc = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) : nullptr;
        DWORD code = 0;
        const bool alive = proc && GetExitCodeProcess(proc, &code) && code == STILL_ACTIVE;
        if (proc) CloseHandle(proc);
        if (!alive) DeleteTree(GuestRoot() + L"\\" + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

Profile MakeNew(const std::wstring& name, const std::vector<Profile>& existing) {
    Profile p;
    do {
        p.id = Hex8(Random32());
    } while (std::any_of(existing.begin(), existing.end(), [&](const Profile& e) { return e.id == p.id; }) ||
             GetFileAttributesW(ProfileDir(p.id).c_str()) != INVALID_FILE_ATTRIBUTES);
    do {
        p.steamId = kSteamIdBase + kGeneratedAccountMin + (Random32() % kGeneratedAccountSpan);
    } while (std::any_of(existing.begin(), existing.end(), [&](const Profile& e) { return e.steamId == p.steamId; }));
    p.name = name.substr(0, kMaxNameChars);
    p.stockAvatar = (int)(Random32() % kStockAvatarCount);
    p.lastUsed = Now();
    p.originGame = GameKey();
    p.games = { { p.originGame, DefaultGameName() } };
    return p;
}

void Touch(Profile& p) { p.lastUsed = Now(); }

std::wstring GameKey() { return ExeKey(); }

std::wstring DefaultGameName() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const wchar_t* base = wcsrchr(path, L'\\');
    std::wstring name = base ? base + 1 : path;
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name.resize(dot);
    return name;
}

bool UsedIn(const Profile& p, const std::wstring& gameKey) {
    if (p.originGame.empty()) return true;   // from before games were tracked: every game
    return std::any_of(p.games.begin(), p.games.end(), [&](const GameUse& g) { return g.key == gameKey; });
}

bool AddGame(Profile& p, const std::wstring& gameKey, const std::wstring& gameName, bool replaceName) {
    const std::wstring name = gameName.empty() ? gameKey : gameName.substr(0, 64);
    for (GameUse& g : p.games) {
        if (g.key != gameKey) continue;
        if (g.name == name || !replaceName) return false;
        g.name = name;
        return true;
    }
    p.games.push_back({ gameKey, name });
    return true;
}

std::vector<Profile> ForGame(const std::vector<Profile>& all, const std::wstring& gameKey) {
    std::vector<Profile> out;
    for (const Profile& p : all) {
        if (UsedIn(p, gameKey)) out.push_back(p);
    }
    return out;
}

std::vector<Profile> FromOtherGames(const std::vector<Profile>& all, const std::wstring& gameKey) {
    std::vector<Profile> out;
    for (const Profile& p : all) {
        if (!UsedIn(p, gameKey)) out.push_back(p);
    }
    return out;
}

std::wstring LastProfileId() { return ReadIni(IndexPath(), L"General", L"LastProfile"); }

void SetLastProfileId(const std::wstring& id) {
    EnsureDir(RootDir());
    EnsureUnicodeIni(IndexPath());
    WriteIni(IndexPath(), L"General", L"LastProfile", id);
}

bool TryLock(const std::wstring& id) {
    if (g_lock) return g_lockedId == id;
    HANDLE h = CreateMutexW(nullptr, FALSE, LockName(id).c_str());
    if (!h) return false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(h);
        return false;
    }
    g_lock = h;
    g_lockedId = id;
    g_anyLock = CreateMutexW(nullptr, FALSE, AnyLockName(id).c_str());
    return true;
}

bool WaitLock(const std::wstring& id, DWORD timeoutMs) {
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    while (!TryLock(id)) {
        if (GetTickCount64() >= deadline) return false;
        Sleep(kLockPollMs);
    }
    return true;
}

bool IsInUseAnywhere(const std::wstring& id) {
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, AnyLockName(id).c_str());
    if (!h) return false;
    CloseHandle(h);
    return true;
}

bool IsLockedByOther(const std::wstring& id, const std::wstring& activeId) {
    if (id == activeId) return false;
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, LockName(id).c_str());
    if (!h) return false;
    CloseHandle(h);
    return true;
}

std::optional<SteamSession> ReadSteamSession(std::wstring& error) {
    DWORD pid = 0, accountId = 0;
    if (!ReadActiveProcess(pid, accountId) || !IsProcessAlive(pid)) {
        error = i18n::TW(L"Steam is not running. Open it and sign in to import your account.",
                         L"Steam no está abierto. Ábrelo e inicia sesión para importar tu cuenta.");
        return std::nullopt;
    }
    if (accountId == 0) {
        error = i18n::TW(L"Steam is open but no account is signed in.",
                         L"Steam está abierto pero no hay ninguna sesión iniciada.");
        return std::nullopt;
    }
    SteamSession s;
    s.steamId = kSteamIdBase + accountId;
    const std::wstring steamPath = SteamPath();
    if (!steamPath.empty()) {
        s.name = PersonaFromLoginUsers(steamPath, s.steamId);
        const std::wstring avatar = steamPath + L"\\config\\avatarcache\\" + std::to_wstring(s.steamId) + L".png";
        if (GetFileAttributesW(avatar.c_str()) != INVALID_FILE_ATTRIBUTES) s.avatarPath = avatar;
    }
    if (s.name.empty()) {
        wchar_t buf[256] = {};
        DWORD size = sizeof(buf);
        if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"LastGameNameUsed", RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS)
            s.name = buf;
    }
    if (s.name.empty()) s.name = L"Steam " + std::to_wstring(accountId);
    ReFixLog("[Profiles] Steam session found: %llu '%s'", (unsigned long long)s.steamId, ToUtf8(s.name).c_str());
    return s;
}

} // namespace refix::overlay::profiles
