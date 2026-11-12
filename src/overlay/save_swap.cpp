#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "save_swap.h"
#include "overlay_text.h"
#include "refix_overlay.h"

#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <algorithm>
#include <fstream>
#include <mutex>
#include <vector>
#include "../minhook/MinHook.h"

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
    return std::any_of(keep.begin(), keep.end(), [&](const std::wstring& k) {
        if (k.find(L'*') != std::wstring::npos || k.find(L'?') != std::wstring::npos) {
            auto WildcardMatch = [](auto& self, const wchar_t* pat, const wchar_t* str) -> bool {
                while (*pat) {
                    if (*pat == L'*') {
                        pat++;
                        if (!*pat) return true;
                        while (*str) {
                            if (self(self, pat, str)) return true;
                            str++;
                        }
                        return false;
                    } else if (*pat == L'?' || towlower(*pat) == towlower(*str)) {
                        if (!*str) return false;
                        pat++;
                        str++;
                    } else {
                        return false;
                    }
                }
                return *str == L'\0';
            };
            return WildcardMatch(WildcardMatch, k.c_str(), name.c_str());
        }
        return _wcsicmp(k.c_str(), name.c_str()) == 0;
    });
}

bool CopyTree(const std::wstring& from, const std::wstring& to) {
    if (!IsDir(from)) return CopyFileW(from.c_str(), to.c_str(), TRUE) != 0;
    if (!CreateDirectoryW(to.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    for (const std::wstring& e : Entries(from)) {
        if (!CopyTree(from + L"\\" + e, to + L"\\" + e)) return false;
    }
    return true;
}

bool CopyTreeWithKeep(const std::wstring& from, const std::wstring& to, const std::vector<std::wstring>& keep) {
    if (!IsDir(from)) return CopyFileW(from.c_str(), to.c_str(), TRUE) != 0;
    EnsureDir(to);
    for (const std::wstring& e : Entries(from)) {
        if (!Kept(e, keep)) {
            if (!CopyTree(from + L"\\" + e, to + L"\\" + e)) return false;
        }
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
    if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
    if (GetLastError() != ERROR_NOT_SAME_DEVICE) {
        if (CopyTree(from, to)) {
            DeleteTree(from);
            return true;
        }
        return false;
    }
    if (!CopyTree(from, to)) {
        DeleteTree(to);
        return false;
    }
    DeleteTree(from);
    return true;
}

// Moves every entry of `from` (except `keep`) into `to`.
bool MoveEntries(const std::wstring& from, const std::wstring& to, const std::vector<std::wstring>& keep) {
    std::vector<std::wstring> names;
    for (const std::wstring& e : Entries(from)) {
        if (!Kept(e, keep)) names.push_back(e);
    }
    if (names.empty()) return true;
    EnsureDir(to);
    for (const std::wstring& e : names) {
        std::wstring dst = to + L"\\" + e;
        if (GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES) {
            DeleteTree(dst);
        }
    }
    std::vector<std::wstring> moved;
    for (const std::wstring& e : names) {
        if (!MoveOne(from + L"\\" + e, to + L"\\" + e)) {
            DWORD err = GetLastError();
            std::wstring lower = e;
            for (auto& c : lower) c = towlower(c);
            bool isLockOrLog = lower.find(L"lock") != std::wstring::npos || lower.find(L"log") != std::wstring::npos ||
                               lower.find(L"tmp") != std::wstring::npos || lower.find(L"sentry") != std::wstring::npos ||
                               lower.find(L"cache") != std::wstring::npos;
            if (isLockOrLog) {
                ReFixLog("[Saves] Skipping runtime/diagnostic file '%s' (error %lu)", ToUtf8(from + L"\\" + e).c_str(), err);
                continue;
            }
            ReFixLog("[Saves] Could not move '%s' (error %lu); rolling back", ToUtf8(from + L"\\" + e).c_str(), err);
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

// Locate and read <exe dir>\<exe name>_Data\app.info (with fallback to any *_Data\app.info)
bool FindUnityAppInfo(std::string& company, std::string& product) {
    const std::wstring exe = ExePath();
    std::wstring name = Leaf(exe);
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name.resize(dot);

    std::wstring appInfoPath = Parent(exe) + L"\\" + name + L"_Data\\app.info";
    if (GetFileAttributesW(appInfoPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((Parent(exe) + L"\\*_Data").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    std::wstring cand = Parent(exe) + L"\\" + fd.cFileName + L"\\app.info";
                    if (GetFileAttributesW(cand.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        appInfoPath = cand;
                        break;
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    std::ifstream f(appInfoPath, std::ios::binary);
    if (!f) return false;
    std::getline(f, company);
    std::getline(f, product);
    auto clean = [](std::string& s) {
        if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s.erase(0, 3);
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    };
    clean(company);
    clean(product);
    return !product.empty();
}

// <exe dir>\<exe name>_Data\app.info: company on the first line, product on the second (UTF-8).
bool DetectUnity(Location& out) {
    std::string company, product;
    if (!FindUnityAppInfo(company, product)) return false;
    if (company.empty() || product.empty()) return false;
    const std::wstring low = KnownFolder(FOLDERID_LocalAppDataLow);
    if (low.empty()) return false;
    const std::wstring wc = FromUtf8(company), wp = FromUtf8(product);
    out.label = L"Unity_" + Sanitize(wc) + L"_" + Sanitize(wp);
    out.live = low + L"\\" + wc + L"\\" + wp;
    // Unity has its log open from the first frame; its analytics cache is not a save.
    out.keepLive = { L"Player.log", L"Player-prev.log", L"output_log.txt", L"Unity", L"sentry-unity.lock", L"*.lock", L"*.log", L"*.tmp", L"Sentry", L"SentryNative", L"eventcache" };
    return true;
}

// Many Unity games (such as 7 Days to Die, RimWorld, etc.) store world saves in %APPDATA%\<Product>\Saves.
bool DetectUnityRoaming(Location& out) {
    std::string company, product;
    if (!FindUnityAppInfo(company, product)) return false;
    if (product.empty()) return false;

    const std::wstring roaming = KnownFolder(FOLDERID_RoamingAppData);
    if (roaming.empty()) return false;

    const std::wstring wc = FromUtf8(company);
    const std::wstring wp = FromUtf8(product);

    std::wstring wpNoSpace = wp;
    wpNoSpace.erase(std::remove_if(wpNoSpace.begin(), wpNoSpace.end(), iswspace), wpNoSpace.end());

    std::wstring targetSaveDir;
    std::wstring labelPart;

    if (IsDir(roaming + L"\\" + wpNoSpace + L"\\Saves")) {
        targetSaveDir = roaming + L"\\" + wpNoSpace + L"\\Saves";
        labelPart = wpNoSpace;
    } else if (IsDir(roaming + L"\\" + wp + L"\\Saves")) {
        targetSaveDir = roaming + L"\\" + wp + L"\\Saves";
        labelPart = wp;
    } else if (!wc.empty() && IsDir(roaming + L"\\" + wc + L"\\" + wp + L"\\Saves")) {
        targetSaveDir = roaming + L"\\" + wc + L"\\" + wp + L"\\Saves";
        labelPart = wc + L"_" + wp;
    } else if (IsDir(roaming + L"\\" + wpNoSpace)) {
        targetSaveDir = roaming + L"\\" + wpNoSpace + L"\\Saves";
        labelPart = wpNoSpace;
    } else if (IsDir(roaming + L"\\" + wp)) {
        targetSaveDir = roaming + L"\\" + wp + L"\\Saves";
        labelPart = wp;
    } else {
        return false;
    }

    if (!IsDir(targetSaveDir) && !IsDir(Parent(targetSaveDir))) {
        return false;
    }

    out.label = L"Unity_Roaming_" + Sanitize(labelPart) + L"_Saves";
    out.live = targetSaveDir;
    out.keepLive = { L"*.lock", L"*.log", L"*.tmp" };
    return true;
}

bool DetectUnityRoamingLocal(Location& out) {
    std::string company, product;
    if (!FindUnityAppInfo(company, product)) return false;
    if (product.empty()) return false;

    const std::wstring roaming = KnownFolder(FOLDERID_RoamingAppData);
    if (roaming.empty()) return false;

    const std::wstring wc = FromUtf8(company);
    const std::wstring wp = FromUtf8(product);

    std::wstring wpNoSpace = wp;
    wpNoSpace.erase(std::remove_if(wpNoSpace.begin(), wpNoSpace.end(), iswspace), wpNoSpace.end());

    std::wstring targetSaveDir;
    std::wstring labelPart;

    if (IsDir(roaming + L"\\" + wpNoSpace + L"\\SavesLocal")) {
        targetSaveDir = roaming + L"\\" + wpNoSpace + L"\\SavesLocal";
        labelPart = wpNoSpace;
    } else if (IsDir(roaming + L"\\" + wp + L"\\SavesLocal")) {
        targetSaveDir = roaming + L"\\" + wp + L"\\SavesLocal";
        labelPart = wp;
    } else if (!wc.empty() && IsDir(roaming + L"\\" + wc + L"\\" + wp + L"\\SavesLocal")) {
        targetSaveDir = roaming + L"\\" + wc + L"\\" + wp + L"\\SavesLocal";
        labelPart = wc + L"_" + wp;
    } else {
        return false;
    }

    out.label = L"Unity_Roaming_" + Sanitize(labelPart) + L"_SavesLocal";
    out.live = targetSaveDir;
    out.keepLive = { L"*.lock", L"*.log", L"*.tmp" };
    return true;
}

} // namespace

bool OtherInstanceRunning() {
    wchar_t restartPidBuf[16] = {};
    DWORD restartPid = 0;
    if (GetEnvironmentVariableW(L"REFIX_RESTART_PID", restartPidBuf, 16) > 0) {
        restartPid = wcstoul(restartPidBuf, nullptr, 10);
    }

    const std::wstring self = Leaf(ExePath());
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) {
        if (pe.th32ProcessID == GetCurrentProcessId() || pe.th32ProcessID == restartPid) continue;
        if (_wcsicmp(pe.szExeFile, self.c_str()) == 0) {
            HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (proc) {
                DWORD code = 0;
                if (GetExitCodeProcess(proc, &code) && code == STILL_ACTIVE) {
                    found = true;
                }
                CloseHandle(proc);
            } else if (GetLastError() == ERROR_ACCESS_DENIED) {
                found = true;
            }
        }
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
    const std::wstring pcStash = StashDir(kPcOwner, loc.label);

    if (owner.empty()) {
        const bool liveHasData = HasEntries(loc.live, loc.keepLive);
        if (liveHasData && !HasEntries(pcStash, {})) {
            CopyTreeWithKeep(loc.live, pcStash, loc.keepLive);
            ReFixLog("[Saves] %s: backed up pristine pre-ReFix saves to _pc", ToUtf8(loc.label).c_str());
        }

        if (!liveHasData) {
            WriteOwner(loc.label, active.id);
            return;
        }

        if (active.nativeSaves) {
            WriteOwner(loc.label, active.id);
            ReFixLog("[Saves] %s: active profile %s started with original saves", ToUtf8(loc.label).c_str(), ToUtf8(active.id).c_str());
            return;
        }

        owner = kPcOwner;
        WriteOwner(loc.label, owner);
        if (!MoveEntries(loc.live, pcStash, loc.keepLive)) {
            for (const std::wstring& e : Entries(loc.live)) {
                if (!Kept(e, loc.keepLive)) DeleteTree(loc.live + L"\\" + e);
            }
        }
        WriteOwner(loc.label, active.id);
        ReFixLog("[Saves] %s: pre-ReFix saves preserved in _pc, active %s starts clean", ToUtf8(loc.label).c_str(), ToUtf8(active.id).c_str());
        return;
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
    if (!MoveEntries(in, loc.live, loc.keepLive)) {
        MoveEntries(out, loc.live, loc.keepLive);
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
    loc = Location{};
    if (DetectUnityRoaming(loc)) out.push_back(loc);
    loc = Location{};
    if (DetectUnityRoamingLocal(loc)) out.push_back(loc);
    return out;
}

bool HasOriginalSaves() {
    const std::vector<Location> locations = DetectLocations();
    if (locations.empty()) return false;
    for (const Location& loc : locations) {
        const std::wstring pcStash = StashDir(kPcOwner, loc.label);
        if (HasEntries(pcStash, {})) return true;
        const std::wstring owner = ReadOwner(loc.label);
        if (owner.empty() && HasEntries(loc.live, loc.keepLive)) return true;
        if (!owner.empty() && owner != kPcOwner) {
            const std::wstring ownerStash = StashDir(owner, loc.label);
            if (HasEntries(ownerStash, {})) {
                if (auto p = profiles::Load(owner)) {
                    if (p->nativeSaves) return true;
                }
            } else if (HasEntries(loc.live, loc.keepLive)) {
                if (auto p = profiles::Load(owner)) {
                    if (p->nativeSaves) return true;
                }
            }
        }
    }
    return false;
}

bool InheritOriginalSaves(const std::wstring& profileId) {
    const std::vector<Location> locations = DetectLocations();
    if (locations.empty() || profileId.empty()) return false;
    bool anyInherited = false;
    for (const Location& loc : locations) {
        const std::wstring pcStash = StashDir(kPcOwner, loc.label);
        if (!HasEntries(pcStash, {})) {
            const std::wstring owner = ReadOwner(loc.label);
            if (owner.empty() && HasEntries(loc.live, loc.keepLive)) {
                CopyTreeWithKeep(loc.live, pcStash, loc.keepLive);
            } else if (!owner.empty() && owner != kPcOwner) {
                const std::wstring ownerStash = StashDir(owner, loc.label);
                if (HasEntries(ownerStash, {})) {
                    CopyTree(ownerStash, pcStash);
                } else if (HasEntries(loc.live, loc.keepLive)) {
                    CopyTreeWithKeep(loc.live, pcStash, loc.keepLive);
                }
            }
        }
        if (HasEntries(pcStash, {})) {
            const std::wstring profileStash = StashDir(profileId, loc.label);
            EnsureDir(profileStash);
            if (CopyTree(pcStash, profileStash)) {
                anyInherited = true;
                ReFixLog("[Saves] Profile %s inherited original saves for %s",
                         ToUtf8(profileId).c_str(), ToUtf8(loc.label).c_str());
            }
        }
    }
    return anyInherited;
}

namespace {

struct SaveRedirectRule {
    std::wstring livePrefix;
    std::wstring stashPrefix;
};

static std::vector<SaveRedirectRule> g_redirectRules;
static std::mutex g_redirectMutex;
static bool g_hooksInstalled = false;

std::wstring NormalizePath(const wchar_t* in) {
    if (!in || !*in) return L"";
    std::wstring s = in;
    if (s.rfind(L"\\\\?\\", 0) == 0) s = s.substr(4);
    for (auto& c : s) {
        if (c == L'/') c = L'\\';
    }
    return s;
}

bool CheckAndRedirect(const wchar_t* originalPath, std::wstring& out) {
    if (!originalPath || !*originalPath) return false;
    std::wstring s = originalPath;
    bool hasLong = false;
    if (s.rfind(L"\\\\?\\", 0) == 0) {
        s = s.substr(4);
        hasLong = true;
    }
    for (auto& c : s) {
        if (c == L'/') c = L'\\';
    }
    std::lock_guard<std::mutex> lock(g_redirectMutex);
    for (const auto& rule : g_redirectRules) {
        if (s.size() >= rule.livePrefix.size()) {
            if (_wcsnicmp(s.c_str(), rule.livePrefix.c_str(), rule.livePrefix.size()) == 0) {
                if (s.size() == rule.livePrefix.size() || s[rule.livePrefix.size()] == L'\\') {
                    std::wstring rest = s.substr(rule.livePrefix.size());
                    out = (hasLong ? L"\\\\?\\" : L"") + rule.stashPrefix + rest;
                    return true;
                }
            }
        }
    }
    return false;
}

bool CheckAndRedirectA(const char* originalPath, std::string& out) {
    if (!originalPath || !*originalPath) return false;
    std::wstring w = FromUtf8(originalPath);
    std::wstring wOut;
    if (CheckAndRedirect(w.c_str(), wOut)) {
        out = ToUtf8(wOut);
        return true;
    }
    return false;
}

static HANDLE (WINAPI* TrueCreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = nullptr;
static HANDLE WINAPI HookCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    std::wstring redir;
    if (CheckAndRedirect(lpFileName, redir)) {
        if (dwCreationDisposition == CREATE_NEW || dwCreationDisposition == CREATE_ALWAYS || dwCreationDisposition == OPEN_ALWAYS) {
            EnsureDir(Parent(redir));
        }
        return TrueCreateFileW(redir.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    return TrueCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HANDLE (WINAPI* TrueCreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = nullptr;
static HANDLE WINAPI HookCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    std::string redir;
    if (CheckAndRedirectA(lpFileName, redir)) {
        if (dwCreationDisposition == CREATE_NEW || dwCreationDisposition == CREATE_ALWAYS || dwCreationDisposition == OPEN_ALWAYS) {
            EnsureDir(Parent(FromUtf8(redir)));
        }
        return TrueCreateFileA(redir.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    return TrueCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HANDLE (WINAPI* TrueFindFirstFileW)(LPCWSTR, LPWIN32_FIND_DATAW) = nullptr;
static HANDLE WINAPI HookFindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
    std::wstring redir;
    if (CheckAndRedirect(lpFileName, redir)) {
        return TrueFindFirstFileW(redir.c_str(), lpFindFileData);
    }
    return TrueFindFirstFileW(lpFileName, lpFindFileData);
}

static HANDLE (WINAPI* TrueFindFirstFileA)(LPCSTR, LPWIN32_FIND_DATAA) = nullptr;
static HANDLE WINAPI HookFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    std::string redir;
    if (CheckAndRedirectA(lpFileName, redir)) {
        return TrueFindFirstFileA(redir.c_str(), lpFindFileData);
    }
    return TrueFindFirstFileA(lpFileName, lpFindFileData);
}

static HANDLE (WINAPI* TrueFindFirstFileExW)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD) = nullptr;
static HANDLE WINAPI HookFindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags) {
    std::wstring redir;
    if (CheckAndRedirect(lpFileName, redir)) {
        return TrueFindFirstFileExW(redir.c_str(), fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
    }
    return TrueFindFirstFileExW(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

static HANDLE (WINAPI* TrueFindFirstFileExA)(LPCSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD) = nullptr;
static HANDLE WINAPI HookFindFirstFileExA(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags) {
    std::string redir;
    if (CheckAndRedirectA(lpFileName, redir)) {
        return TrueFindFirstFileExA(redir.c_str(), fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
    }
    return TrueFindFirstFileExA(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

static DWORD (WINAPI* TrueGetFileAttributesW)(LPCWSTR) = nullptr;
static DWORD WINAPI HookGetFileAttributesW(LPCWSTR lpFileName) {
    std::wstring redir;
    if (CheckAndRedirect(lpFileName, redir)) {
        return TrueGetFileAttributesW(redir.c_str());
    }
    return TrueGetFileAttributesW(lpFileName);
}

static DWORD (WINAPI* TrueGetFileAttributesA)(LPCSTR) = nullptr;
static DWORD WINAPI HookGetFileAttributesA(LPCSTR lpFileName) {
    std::string redir;
    if (CheckAndRedirectA(lpFileName, redir)) {
        return TrueGetFileAttributesA(redir.c_str());
    }
    return TrueGetFileAttributesA(lpFileName);
}

static BOOL (WINAPI* TrueGetFileAttributesExW)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID) = nullptr;
static BOOL WINAPI HookGetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {
    std::wstring redir;
    if (CheckAndRedirect(lpFileName, redir)) {
        return TrueGetFileAttributesExW(redir.c_str(), fInfoLevelId, lpFileInformation);
    }
    return TrueGetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
}

static BOOL (WINAPI* TrueGetFileAttributesExA)(LPCSTR, GET_FILEEX_INFO_LEVELS, LPVOID) = nullptr;
static BOOL WINAPI HookGetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {
    std::string redir;
    if (CheckAndRedirectA(lpFileName, redir)) {
        return TrueGetFileAttributesExA(redir.c_str(), fInfoLevelId, lpFileInformation);
    }
    return TrueGetFileAttributesExA(lpFileName, fInfoLevelId, lpFileInformation);
}

static BOOL (WINAPI* TrueCreateDirectoryW)(LPCWSTR, LPSECURITY_ATTRIBUTES) = nullptr;
static BOOL WINAPI HookCreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    std::wstring redir;
    if (CheckAndRedirect(lpPathName, redir)) {
        EnsureDir(Parent(redir));
        return TrueCreateDirectoryW(redir.c_str(), lpSecurityAttributes);
    }
    return TrueCreateDirectoryW(lpPathName, lpSecurityAttributes);
}

static BOOL (WINAPI* TrueCreateDirectoryA)(LPCSTR, LPSECURITY_ATTRIBUTES) = nullptr;
static BOOL WINAPI HookCreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    std::string redir;
    if (CheckAndRedirectA(lpPathName, redir)) {
        EnsureDir(Parent(FromUtf8(redir)));
        return TrueCreateDirectoryA(redir.c_str(), lpSecurityAttributes);
    }
    return TrueCreateDirectoryA(lpPathName, lpSecurityAttributes);
}

static BOOL (WINAPI* TrueRemoveDirectoryW)(LPCWSTR) = nullptr;
static BOOL WINAPI HookRemoveDirectoryW(LPCWSTR lpPathName) {
    std::wstring redir;
    if (CheckAndRedirect(lpPathName, redir)) {
        return TrueRemoveDirectoryW(redir.c_str());
    }
    return TrueRemoveDirectoryW(lpPathName);
}

static BOOL (WINAPI* TrueRemoveDirectoryA)(LPCSTR) = nullptr;
static BOOL WINAPI HookRemoveDirectoryA(LPCSTR lpPathName) {
    std::string redir;
    if (CheckAndRedirectA(lpPathName, redir)) {
        return TrueRemoveDirectoryA(redir.c_str());
    }
    return TrueRemoveDirectoryA(lpPathName);
}

static BOOL (WINAPI* TrueDeleteFileW)(LPCWSTR) = nullptr;
static BOOL WINAPI HookDeleteFileW(LPCWSTR lpFileName) {
    std::wstring redir;
    if (CheckAndRedirect(lpFileName, redir)) {
        return TrueDeleteFileW(redir.c_str());
    }
    return TrueDeleteFileW(lpFileName);
}

static BOOL (WINAPI* TrueDeleteFileA)(LPCSTR) = nullptr;
static BOOL WINAPI HookDeleteFileA(LPCSTR lpFileName) {
    std::string redir;
    if (CheckAndRedirectA(lpFileName, redir)) {
        return TrueDeleteFileA(redir.c_str());
    }
    return TrueDeleteFileA(lpFileName);
}

static BOOL (WINAPI* TrueMoveFileExW)(LPCWSTR, LPCWSTR, DWORD) = nullptr;
static BOOL WINAPI HookMoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags) {
    std::wstring redirExist, redirNew;
    LPCWSTR existPath = lpExistingFileName;
    LPCWSTR newPath = lpNewFileName;
    if (CheckAndRedirect(lpExistingFileName, redirExist)) existPath = redirExist.c_str();
    if (CheckAndRedirect(lpNewFileName, redirNew)) {
        EnsureDir(Parent(redirNew));
        newPath = redirNew.c_str();
    }
    return TrueMoveFileExW(existPath, newPath, dwFlags);
}

static BOOL (WINAPI* TrueMoveFileW)(LPCWSTR, LPCWSTR) = nullptr;
static BOOL WINAPI HookMoveFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName) {
    return HookMoveFileExW(lpExistingFileName, lpNewFileName, 0);
}

static BOOL (WINAPI* TrueCopyFileW)(LPCWSTR, LPCWSTR, BOOL) = nullptr;
static BOOL WINAPI HookCopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists) {
    std::wstring redirExist, redirNew;
    LPCWSTR existPath = lpExistingFileName;
    LPCWSTR newPath = lpNewFileName;
    if (CheckAndRedirect(lpExistingFileName, redirExist)) existPath = redirExist.c_str();
    if (CheckAndRedirect(lpNewFileName, redirNew)) {
        EnsureDir(Parent(redirNew));
        newPath = redirNew.c_str();
    }
    return TrueCopyFileW(existPath, newPath, bFailIfExists);
}

static BOOL (WINAPI* TrueCopyFileExW)(LPCWSTR, LPCWSTR, LPPROGRESS_ROUTINE, LPVOID, LPBOOL, DWORD) = nullptr;
static BOOL WINAPI HookCopyFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, LPPROGRESS_ROUTINE lpProgressRoutine, LPVOID lpData, LPBOOL pbCancel, DWORD dwCopyFlags) {
    std::wstring redirExist, redirNew;
    LPCWSTR existPath = lpExistingFileName;
    LPCWSTR newPath = lpNewFileName;
    if (CheckAndRedirect(lpExistingFileName, redirExist)) existPath = redirExist.c_str();
    if (CheckAndRedirect(lpNewFileName, redirNew)) {
        EnsureDir(Parent(redirNew));
        newPath = redirNew.c_str();
    }
    return TrueCopyFileExW(existPath, newPath, lpProgressRoutine, lpData, pbCancel, dwCopyFlags);
}

static BOOL (WINAPI* TrueMoveFileExA)(LPCSTR, LPCSTR, DWORD) = nullptr;
static BOOL WINAPI HookMoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags) {
    std::string redirExist, redirNew;
    LPCSTR existPath = lpExistingFileName;
    LPCSTR newPath = lpNewFileName;
    if (CheckAndRedirectA(lpExistingFileName, redirExist)) existPath = redirExist.c_str();
    if (CheckAndRedirectA(lpNewFileName, redirNew)) {
        EnsureDir(Parent(FromUtf8(redirNew)));
        newPath = redirNew.c_str();
    }
    return TrueMoveFileExA(existPath, newPath, dwFlags);
}

static BOOL (WINAPI* TrueMoveFileA)(LPCSTR, LPCSTR) = nullptr;
static BOOL WINAPI HookMoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName) {
    return HookMoveFileExA(lpExistingFileName, lpNewFileName, 0);
}

static BOOL (WINAPI* TrueCopyFileA)(LPCSTR, LPCSTR, BOOL) = nullptr;
static BOOL WINAPI HookCopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists) {
    std::string redirExist, redirNew;
    LPCSTR existPath = lpExistingFileName;
    LPCSTR newPath = lpNewFileName;
    if (CheckAndRedirectA(lpExistingFileName, redirExist)) existPath = redirExist.c_str();
    if (CheckAndRedirectA(lpNewFileName, redirNew)) {
        EnsureDir(Parent(FromUtf8(redirNew)));
        newPath = redirNew.c_str();
    }
    return TrueCopyFileA(existPath, newPath, bFailIfExists);
}

static BOOL (WINAPI* TrueCopyFileExA)(LPCSTR, LPCSTR, LPPROGRESS_ROUTINE, LPVOID, LPBOOL, DWORD) = nullptr;
static BOOL WINAPI HookCopyFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, LPPROGRESS_ROUTINE lpProgressRoutine, LPVOID lpData, LPBOOL pbCancel, DWORD dwCopyFlags) {
    std::string redirExist, redirNew;
    LPCSTR existPath = lpExistingFileName;
    LPCSTR newPath = lpNewFileName;
    if (CheckAndRedirectA(lpExistingFileName, redirExist)) existPath = redirExist.c_str();
    if (CheckAndRedirectA(lpNewFileName, redirNew)) {
        EnsureDir(Parent(FromUtf8(redirNew)));
        newPath = redirNew.c_str();
    }
    return TrueCopyFileExA(existPath, newPath, lpProgressRoutine, lpData, pbCancel, dwCopyFlags);
}

static BOOL (WINAPI* TrueSetFileAttributesW)(LPCWSTR, DWORD) = nullptr;
static BOOL WINAPI HookSetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes) {
    std::wstring redir;
    if (CheckAndRedirect(lpFileName, redir)) {
        return TrueSetFileAttributesW(redir.c_str(), dwFileAttributes);
    }
    return TrueSetFileAttributesW(lpFileName, dwFileAttributes);
}

static BOOL (WINAPI* TrueSetFileAttributesA)(LPCSTR, DWORD) = nullptr;
static BOOL WINAPI HookSetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes) {
    std::string redir;
    if (CheckAndRedirectA(lpFileName, redir)) {
        return TrueSetFileAttributesA(redir.c_str(), dwFileAttributes);
    }
    return TrueSetFileAttributesA(lpFileName, dwFileAttributes);
}

static void HookExport(const char* name, void* hookFn, void** trueFn) {
    HMODULE hK32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE hKBase = GetModuleHandleW(L"kernelbase.dll");
    void* pK32 = hK32 ? (void*)GetProcAddress(hK32, name) : nullptr;
    void* pKBase = hKBase ? (void*)GetProcAddress(hKBase, name) : nullptr;

    void* chosen = pKBase ? pKBase : pK32;
    if (chosen) {
        MH_CreateHook(chosen, hookFn, trueFn);
        MH_EnableHook(chosen);
    }
    if (pK32 && pKBase && pK32 != pKBase) {
        void* dummy = nullptr;
        MH_CreateHook(pK32, hookFn, &dummy);
        MH_EnableHook(pK32);
    }
}

void InstallFileSystemHooks() {
    if (g_hooksInstalled) return;
    MH_Initialize();

#define HOOK_API(fn) HookExport(#fn, (void*)Hook##fn, (void**)&True##fn)
    HOOK_API(CreateFileW);
    HOOK_API(CreateFileA);
    HOOK_API(FindFirstFileW);
    HOOK_API(FindFirstFileA);
    HOOK_API(FindFirstFileExW);
    HOOK_API(FindFirstFileExA);
    HOOK_API(GetFileAttributesW);
    HOOK_API(GetFileAttributesA);
    HOOK_API(GetFileAttributesExW);
    HOOK_API(GetFileAttributesExA);
    HOOK_API(CreateDirectoryW);
    HOOK_API(CreateDirectoryA);
    HOOK_API(RemoveDirectoryW);
    HOOK_API(RemoveDirectoryA);
    HOOK_API(DeleteFileW);
    HOOK_API(DeleteFileA);
    HOOK_API(MoveFileExW);
    HOOK_API(MoveFileExA);
    HOOK_API(MoveFileW);
    HOOK_API(MoveFileA);
    HOOK_API(CopyFileW);
    HOOK_API(CopyFileA);
    HOOK_API(CopyFileExW);
    HOOK_API(CopyFileExA);
    HOOK_API(SetFileAttributesW);
    HOOK_API(SetFileAttributesA);
#undef HOOK_API

    g_hooksInstalled = true;
    ReFixLog("[Saves] Multi-instance filesystem redirection hooks installed successfully");
}

} // namespace

void EnableMultiInstanceRedirection(const std::vector<Location>& locations, const Profile& active) {
    {
        std::lock_guard<std::mutex> lock(g_redirectMutex);
        g_redirectRules.clear();
        for (const auto& loc : locations) {
            std::wstring stash = StashDir(active.id, loc.label);
            EnsureDir(stash);

            if (active.nativeSaves) {
                std::wstring pcStash = StashDir(kPcOwner, loc.label);
                if (!HasEntries(stash, {}) && HasEntries(pcStash, {})) {
                    CopyTree(pcStash, stash);
                    ReFixLog("[Saves] Profile %s inherited original saves in multi-instance stash", ToUtf8(active.id).c_str());
                }
            }

            SaveRedirectRule rule;
            rule.livePrefix = NormalizePath(loc.live.c_str());
            rule.stashPrefix = stash;
            g_redirectRules.push_back(rule);
            ReFixLog("[Saves] Multi-instance redirect: '%s' -> '%s'",
                     ToUtf8(rule.livePrefix).c_str(), ToUtf8(rule.stashPrefix).c_str());
        }
    }
    InstallFileSystemHooks();
}

void SwitchTo(const Profile& active, const std::vector<Profile>& all) {
    const std::vector<Location> locations = DetectLocations();
    if (locations.empty()) return;
    if (OtherInstanceRunning()) {
        ReFixLog("[Saves] Another instance of this game is running: enabling filesystem redirection for profile %s",
                 ToUtf8(active.id).c_str());
        EnableMultiInstanceRedirection(locations, active);
        return;
    }
    for (const Location& loc : locations) {
        WritePrivateProfileStringW(L"Paths", loc.label.c_str(), loc.live.c_str(), OwnersIni().c_str());
        SwitchOne(loc, active, all);
    }
}

std::wstring StashOf(const std::wstring& profileId, const std::wstring& label) { return StashDir(profileId, label); }

// Folder of a label recorded before [Paths] existed: UE_<Project>, Unity_<Company>_<Product>, Unity_Roaming_<Product>_Saves.
static std::wstring DerivePath(const std::wstring& label) {
    wchar_t buf[MAX_PATH] = {};
    if (label.rfind(L"UE_", 0) == 0) {
        if (!GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH)) return {};
        return std::wstring(buf) + L"\\" + label.substr(3) + L"\\Saved\\SaveGames";
    }
    if (label.rfind(L"Unity_Roaming_", 0) == 0 && GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH)) {
        std::wstring rest = label.substr(14);
        if (rest.size() > 6 && rest.substr(rest.size() - 6) == L"_Saves") {
            rest = rest.substr(0, rest.size() - 6);
            return std::wstring(buf) + L"\\" + rest + L"\\Saves";
        }
        if (rest.size() > 11 && rest.substr(rest.size() - 11) == L"_SavesLocal") {
            rest = rest.substr(0, rest.size() - 11);
            return std::wstring(buf) + L"\\" + rest + L"\\SavesLocal";
        }
        return std::wstring(buf) + L"\\" + rest + L"\\Saves";
    }
    if (label.rfind(L"Unity_", 0) == 0 && GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH)) {
        const std::wstring rest = label.substr(6), base = std::wstring(buf) + L"\\AppData\\LocalLow\\";
        for (size_t at = rest.find(L'_'); at != std::wstring::npos; at = rest.find(L'_', at + 1)) {
            const std::wstring dir = base + rest.substr(0, at) + L"\\" + rest.substr(at + 1);
            if (GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES) return dir;
        }
    }
    return {};
}

std::vector<Location> OwnedBy(const std::wstring& profileId) {
    std::vector<Location> out;
    std::vector<wchar_t> buf(32768);
    const DWORD n = GetPrivateProfileSectionW(L"Owners", buf.data(), (DWORD)buf.size(), OwnersIni().c_str());
    for (const wchar_t* p = buf.data(); p < buf.data() + n && *p; p += wcslen(p) + 1) {
        const std::wstring entry = p;
        const size_t eq = entry.find(L'=');
        if (eq == std::wstring::npos || entry.substr(eq + 1) != profileId) continue;
        Location loc;
        loc.label = entry.substr(0, eq);
        wchar_t path[MAX_PATH] = {};
        GetPrivateProfileStringW(L"Paths", loc.label.c_str(), L"", path, MAX_PATH, OwnersIni().c_str());
        loc.live = path[0] ? std::wstring(path) : DerivePath(loc.label);
        if (!loc.live.empty()) out.push_back(loc);
    }
    return out;
}

std::wstring GetActiveSaveDir(const std::wstring& label) {
    std::wstring targetLabel = label;
    std::wstring livePath;
    std::vector<std::wstring> keepList;

    const std::vector<Location> locations = DetectLocations();
    for (const auto& loc : locations) {
        if (targetLabel.empty() || _wcsicmp(loc.label.c_str(), targetLabel.c_str()) == 0) {
            targetLabel = loc.label;
            livePath = loc.live;
            keepList = loc.keepLive;
            break;
        }
    }

    if (targetLabel.empty()) return L"";

    std::wstring activeId = ReFixOverlay::ActiveProfileId();
    if (activeId.empty()) {
        wchar_t envId[64] = {};
        if (GetEnvironmentVariableW(L"REFIX_PROFILE_ID", envId, 64) > 0) activeId = envId;
    }
    if (activeId.empty()) activeId = profiles::LastProfileId();
    if (activeId.empty()) activeId = kPcOwner;

    std::wstring stash = StashDir(activeId, targetLabel);
    EnsureDir(stash);

    bool allowNative = false;
    if (activeId == kPcOwner) {
        allowNative = true;
    } else if (auto p = profiles::Load(activeId)) {
        allowNative = p->nativeSaves;
    }

    if (allowNative && !HasEntries(stash, {}) && !livePath.empty() && HasEntries(livePath, keepList)) {
        CopyTreeWithKeep(livePath, stash, keepList);
        ReFixLog("[Saves] Seeded isolated saves for profile %s from %s", ToUtf8(activeId).c_str(), ToUtf8(livePath).c_str());
    }

    return stash;
}

} // namespace refix::overlay::saves
