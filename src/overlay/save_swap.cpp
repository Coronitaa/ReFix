#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "save_swap.h"
#include "overlay_text.h"

#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <algorithm>
#include <fstream>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::saves {
namespace {

constexpr wchar_t kPcOwner[] = L"_pc";

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &p)) && p) out = p;
    if (p) CoTaskMemFree(p);
    return out;
}

bool IsDir(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

void EnsureDir(const std::wstring& path) {
    for (size_t i = 3; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == L'\\') CreateDirectoryW(path.substr(0, i).c_str(), nullptr);
    }
}

std::wstring Sanitize(std::wstring s) {
    for (wchar_t& c : s) {
        if (wcschr(L"<>:\"/\\|?*", c) || c < 32) c = L'_';
    }
    return s;
}

std::vector<std::wstring> Entries(const std::wstring& dir) {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) out.push_back(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

bool Kept(const std::wstring& name, const std::vector<std::wstring>& keep) {
    return std::any_of(keep.begin(), keep.end(), [&](const std::wstring& k) { return _wcsicmp(k.c_str(), name.c_str()) == 0; });
}

bool CopyTree(const std::wstring& from, const std::wstring& to) {
    if (!IsDir(from)) return CopyFileW(from.c_str(), to.c_str(), TRUE) != 0;
    if (!CreateDirectoryW(to.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    for (const std::wstring& e : Entries(from)) {
        if (!CopyTree(from + L"\\" + e, to + L"\\" + e)) return false;
    }
    return true;
}

void DeleteTree(const std::wstring& path) {
    if (IsDir(path)) {
        for (const std::wstring& e : Entries(path)) DeleteTree(path + L"\\" + e);
        RemoveDirectoryW(path.c_str());
    } else {
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(path.c_str());
    }
}

// Rename when both sides are on one volume; copy + delete otherwise.
bool MoveOne(const std::wstring& from, const std::wstring& to) {
    if (MoveFileExW(from.c_str(), to.c_str(), 0)) return true;
    if (GetLastError() != ERROR_NOT_SAME_DEVICE) return false;
    if (!CopyTree(from, to)) {
        DeleteTree(to);
        return false;
    }
    DeleteTree(from);
    return true;
}

// Moves every entry of `from` (except `keep`) into `to`. All or nothing: refuses when a name already
// exists in `to`, and moves back what it moved when an entry is locked.
bool MoveEntries(const std::wstring& from, const std::wstring& to, const std::vector<std::wstring>& keep) {
    std::vector<std::wstring> names;
    for (const std::wstring& e : Entries(from)) {
        if (!Kept(e, keep)) names.push_back(e);
    }
    if (names.empty()) return true;
    EnsureDir(to);
    for (const std::wstring& e : names) {
        if (GetFileAttributesW((to + L"\\" + e).c_str()) != INVALID_FILE_ATTRIBUTES) {
            ReFixLog("[Saves] '%s' already exists in the destination; nothing moved", ToUtf8(to + L"\\" + e).c_str());
            return false;
        }
    }
    std::vector<std::wstring> moved;
    for (const std::wstring& e : names) {
        if (!MoveOne(from + L"\\" + e, to + L"\\" + e)) {
            ReFixLog("[Saves] Could not move '%s' (error %lu); rolling back", ToUtf8(from + L"\\" + e).c_str(), GetLastError());
            for (const std::wstring& m : moved) MoveOne(to + L"\\" + m, from + L"\\" + m);
            return false;
        }
        moved.push_back(e);
    }
    return true;
}

std::wstring OwnersIni() { return profiles::RootDir() + L"\\saves.ini"; }

std::wstring ReadOwner(const std::wstring& label) {
    wchar_t buf[64] = {};
    GetPrivateProfileStringW(L"Owners", label.c_str(), L"", buf, 64, OwnersIni().c_str());
    return buf;
}

void WriteOwner(const std::wstring& label, const std::wstring& owner) {
    const std::wstring ini = OwnersIni();
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
        EnsureDir(profiles::RootDir());
        // UTF-16 BOM so non-ASCII labels survive WritePrivateProfileStringW.
        HANDLE h = CreateFileW(ini.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            const WORD bom = 0xFEFF;
            DWORD written = 0;
            WriteFile(h, &bom, sizeof(bom), &written, nullptr);
            CloseHandle(h);
        }
    }
    WritePrivateProfileStringW(L"Owners", label.c_str(), owner.c_str(), ini.c_str());
}

std::wstring StashDir(const std::wstring& owner, const std::wstring& label) {
    if (owner == kPcOwner) return profiles::RootDir() + L"\\Saves\\_pc\\" + label;
    return profiles::ProfileDir(owner) + L"\\Saves\\" + label;
}

std::wstring ExePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

std::wstring Parent(const std::wstring& p) {
    const size_t s = p.find_last_of(L'\\');
    return s == std::wstring::npos ? std::wstring() : p.substr(0, s);
}

std::wstring Leaf(const std::wstring& p) {
    const size_t s = p.find_last_of(L'\\');
    return s == std::wstring::npos ? p : p.substr(s + 1);
}

// <Root>\<Project>\Binaries\Win64\<exe>, with <Root>\Engine next to the project.
bool DetectUnreal(Location& out) {
    const std::wstring bin = Parent(ExePath());                  // ...\Binaries\Win64
    const std::wstring binaries = Parent(bin);
    if (_wcsicmp(Leaf(binaries).c_str(), L"Binaries") != 0) return false;
    const std::wstring projectDir = Parent(binaries);
    const std::wstring project = Leaf(projectDir);
    if (project.empty() || _wcsicmp(project.c_str(), L"Engine") == 0) return false;
    if (!IsDir(Parent(projectDir) + L"\\Engine") && !IsDir(projectDir + L"\\Content")) return false;
    const std::wstring local = KnownFolder(FOLDERID_LocalAppData);
    if (local.empty()) return false;
    out.label = L"UE_" + Sanitize(project);
    out.live = local + L"\\" + project + L"\\Saved\\SaveGames";
    return true;
}

// <exe dir>\<exe name>_Data\app.info: company on the first line, product on the second (UTF-8).
bool DetectUnity(Location& out) {
    const std::wstring exe = ExePath();
    std::wstring name = Leaf(exe);
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name.resize(dot);
    std::ifstream f(Parent(exe) + L"\\" + name + L"_Data\\app.info", std::ios::binary);
    if (!f) return false;
    std::string company, product;
    std::getline(f, company);
    std::getline(f, product);
    auto clean = [](std::string& s) {
        if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s.erase(0, 3);
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    };
    clean(company);
    clean(product);
    if (company.empty() || product.empty()) return false;
    const std::wstring low = KnownFolder(FOLDERID_LocalAppDataLow);
    if (low.empty()) return false;
    const std::wstring wc = FromUtf8(company), wp = FromUtf8(product);
    out.label = L"Unity_" + Sanitize(wc) + L"_" + Sanitize(wp);
    out.live = low + L"\\" + wc + L"\\" + wp;
    // Unity has its log open from the first frame; its analytics cache is not a save.
    out.keepLive = { L"Player.log", L"Player-prev.log", L"output_log.txt", L"Unity" };
    return true;
}

} // namespace

bool OtherInstanceRunning() {
    const std::wstring self = Leaf(ExePath());
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) {
        found = pe.th32ProcessID != GetCurrentProcessId() && _wcsicmp(pe.szExeFile, self.c_str()) == 0;
    }
    CloseHandle(snap);
    return found;
}

void ForgetOwner(const std::wstring& profileId) {
    std::vector<wchar_t> buf(32 * 1024);
    const DWORD n = GetPrivateProfileSectionW(L"Owners", buf.data(), (DWORD)buf.size(), OwnersIni().c_str());
    for (const wchar_t* p = buf.data(); p < buf.data() + n && *p; p += wcslen(p) + 1) {
        const std::wstring entry = p;
        const size_t eq = entry.find(L'=');
        if (eq != std::wstring::npos && entry.substr(eq + 1) == profileId)
            WritePrivateProfileStringW(L"Owners", entry.substr(0, eq).c_str(), nullptr, OwnersIni().c_str());
    }
}

namespace {

bool HasEntries(const std::wstring& dir, const std::vector<std::wstring>& keep) {
    for (const std::wstring& e : Entries(dir)) {
        if (!Kept(e, keep)) return true;
    }
    return false;
}

void SwitchOne(const Location& loc, const Profile& active, const std::vector<Profile>& all) {
    std::wstring owner = ReadOwner(loc.label);
    if (owner.empty()) {
        if (!HasEntries(loc.live, loc.keepLive)) {
            WriteOwner(loc.label, active.id);
            return;
        }
        // Saves from before profiles: they belong to the profile that kept the PC's saves, if any.
        owner = kPcOwner;
        for (const Profile& p : all) {
            if (p.nativeSaves) owner = p.id;
        }
        if (active.nativeSaves) owner = active.id;
        WriteOwner(loc.label, owner);
        ReFixLog("[Saves] %s: existing saves assigned to %s", ToUtf8(loc.label).c_str(), ToUtf8(owner).c_str());
    }
    if (owner == active.id) return;

    const std::wstring out = StashDir(owner, loc.label);
    const std::wstring in = StashDir(active.id, loc.label);
    if (!MoveEntries(loc.live, out, loc.keepLive)) {
        ReFixLog("[Saves] %s: could not stash the saves of %s; keeping them in place", ToUtf8(loc.label).c_str(),
                 ToUtf8(owner).c_str());
        return;
    }
    EnsureDir(loc.live);
    if (!MoveEntries(in, loc.live, {})) {
        MoveEntries(out, loc.live, {});
        ReFixLog("[Saves] %s: could not restore the saves of %s; previous saves put back", ToUtf8(loc.label).c_str(),
                 ToUtf8(active.id).c_str());
        return;
    }
    WriteOwner(loc.label, active.id);
    ReFixLog("[Saves] %s: switched from %s to %s", ToUtf8(loc.label).c_str(), ToUtf8(owner).c_str(),
             ToUtf8(active.id).c_str());
}

} // namespace

std::vector<Location> DetectLocations() {
    std::vector<Location> out;
    Location loc;
    if (DetectUnreal(loc)) out.push_back(loc);
    loc = Location{};
    if (DetectUnity(loc)) out.push_back(loc);
    return out;
}

void SwitchTo(const Profile& active, const std::vector<Profile>& all) {
    const std::vector<Location> locations = DetectLocations();
    if (locations.empty()) return;
    if (OtherInstanceRunning()) {
        ReFixLog("[Saves] Another instance of this game is running: save folders left as they are");
        return;
    }
    for (const Location& loc : locations) SwitchOne(loc, active, all);
}

} // namespace refix::overlay::saves
