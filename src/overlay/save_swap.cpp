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
    // Fast path: check environment variables first to avoid Shell/COM RPC hangs
    if (IsEqualGUID(id, FOLDERID_RoamingAppData)) {
        wchar_t buf[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH) > 0) return buf;
    } else if (IsEqualGUID(id, FOLDERID_LocalAppData)) {
        wchar_t buf[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH) > 0) return buf;
    } else if (IsEqualGUID(id, FOLDERID_LocalAppDataLow)) {
        wchar_t buf[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH) > 0) {
            return std::wstring(buf) + L"\\AppData\\LocalLow";
        }
    } else if (IsEqualGUID(id, FOLDERID_SavedGames)) {
        wchar_t buf[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH) > 0) {
            return std::wstring(buf) + L"\\Saved Games";
        }
    } else if (IsEqualGUID(id, FOLDERID_Documents)) {
        wchar_t buf[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH) > 0) {
            return std::wstring(buf) + L"\\Documents";
        }
    }

    PWSTR p = nullptr;
    std::wstring out;
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &p)) && p) out = p;
    if (p) CoTaskMemFree(p);
    if (hrCo == S_OK) CoUninitialize();
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
    out.live = local + L"\\" + project;
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

// Intercepts entire application root under %APPDATA%\<Product> or %APPDATA%\<Company>\<Product>
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

    if (IsDir(roaming + L"\\" + wpNoSpace)) {
        targetSaveDir = roaming + L"\\" + wpNoSpace;
        labelPart = wpNoSpace;
    } else if (IsDir(roaming + L"\\" + wp)) {
        targetSaveDir = roaming + L"\\" + wp;
        labelPart = wp;
    } else if (!wc.empty() && IsDir(roaming + L"\\" + wc + L"\\" + wp)) {
        targetSaveDir = roaming + L"\\" + wc + L"\\" + wp;
        labelPart = wc + L"_" + wp;
    } else {
        targetSaveDir = roaming + L"\\" + wp;
        labelPart = wp;
    }

    out.label = L"Unity_Roaming_" + Sanitize(labelPart);
    out.live = targetSaveDir;
    out.keepLive = { L"*.lock", L"*.log", L"*.tmp" };
    return true;
}

// Detects %USERPROFILE%\Saved Games\<Game>
bool DetectSavedGames(Location& out) {
    std::string company, product;
    std::wstring gameName;
    if (FindUnityAppInfo(company, product)) {
        gameName = FromUtf8(product);
    } else {
        const std::wstring exe = ExePath();
        std::wstring name = Leaf(exe);
        const size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos) name.resize(dot);
        gameName = name;
    }
    if (gameName.empty()) return false;

    const std::wstring savedGames = KnownFolder(FOLDERID_SavedGames);
    if (savedGames.empty()) return false;

    std::wstring targetDir = savedGames + L"\\" + gameName;
    if (!IsDir(targetDir)) return false;

    out.label = L"SavedGames_" + Sanitize(gameName);
    out.live = targetDir;
    out.keepLive = { L"*.lock", L"*.log", L"*.tmp" };
    return true;
}

// Detects Documents\My Games\<Game>
bool DetectMyGames(Location& out) {
    std::string company, product;
    std::wstring gameName;
    if (FindUnityAppInfo(company, product)) {
        gameName = FromUtf8(product);
    } else {
        const std::wstring exe = ExePath();
        std::wstring name = Leaf(exe);
        const size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos) name.resize(dot);
        gameName = name;
    }
    if (gameName.empty()) return false;

    const std::wstring docs = KnownFolder(FOLDERID_Documents);
    if (docs.empty()) return false;

    std::wstring targetDir = docs + L"\\My Games\\" + gameName;
    if (!IsDir(targetDir)) return false;

    out.label = L"MyGames_" + Sanitize(gameName);
    out.live = targetDir;
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
    if (DetectSavedGames(loc)) out.push_back(loc);
    loc = Location{};
    if (DetectMyGames(loc)) out.push_back(loc);
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
static HANDLE g_profileGateEvent = nullptr;
static std::atomic<bool> g_gateActive{ false };
static std::wstring g_activeProfileId;

std::wstring NormalizePath(const wchar_t* in) {
    if (!in || !*in) return L"";
    std::wstring s = in;
    if (s.rfind(L"\\\\?\\", 0) == 0) s = s.substr(4);
    for (auto& c : s) {
        if (c == L'/') c = L'\\';
    }
    return s;
}

static std::wstring GetRoamingBase() {
    static std::wstring s = NormalizePath(KnownFolder(FOLDERID_RoamingAppData).c_str());
    return s;
}
static std::wstring GetLocalBase() {
    static std::wstring s = NormalizePath(KnownFolder(FOLDERID_LocalAppData).c_str());
    return s;
}
static std::wstring GetLocalLowBase() {
    static std::wstring s = NormalizePath(KnownFolder(FOLDERID_LocalAppDataLow).c_str());
    return s;
}
static std::wstring GetSavedGamesBase() {
    static std::wstring s = NormalizePath(KnownFolder(FOLDERID_SavedGames).c_str());
    return s;
}
static std::wstring GetDocumentsBase() {
    static std::wstring s = NormalizePath(KnownFolder(FOLDERID_Documents).c_str());
    return s;
}

static bool IsExcludedRoot(const std::wstring& folder) {
    static const wchar_t* const exclusions[] = {
        L"refix", L"steam", L"valve", L"epic", L"epic games", L"nvidia", L"microsoft",
        L"temp", L"packages", L"crashdumps", L"d3dscache", L"amd", L"intel", L"discord",
        L"unity", L"windows", nullptr
    };
    for (int i = 0; exclusions[i]; ++i) {
        if (_wcsicmp(folder.c_str(), exclusions[i]) == 0) return true;
    }
    return false;
}

} // namespace

void SetGateActive(bool active) {
    g_gateActive.store(active);
    if (active) {
        if (!g_profileGateEvent) {
            g_profileGateEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        } else {
            ResetEvent(g_profileGateEvent);
        }
    } else {
        if (g_profileGateEvent) SetEvent(g_profileGateEvent);
    }
}

void SignalProfileGate() {
    g_gateActive.store(false);
    if (g_profileGateEvent) {
        SetEvent(g_profileGateEvent);
    }
}

void WaitForProfileGate() {
    if (g_gateActive.load() && g_profileGateEvent) {
        ReFixLog("[Saves] Pre-Game Profile Gate: waiting for profile selection...");
        WaitForSingleObject(g_profileGateEvent, 30000);
        ReFixLog("[Saves] Pre-Game Profile Gate: unblocked, proceeding with file access");
    }
}

namespace {

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

    // 1. Fast path: check already registered rules
    {
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
    }

    // 2. Generic application root check under known base folders
    struct BaseFolderInfo {
        std::wstring base;
        const wchar_t* label;
    };
    const BaseFolderInfo bases[] = {
        { GetRoamingBase(), L"Roaming" },
        { GetLocalLowBase(), L"LocalLow" },
        { GetSavedGamesBase(), L"SavedGames" },
        { GetLocalBase(), L"Local" },
        { GetDocumentsBase(), L"Documents" },
    };

    for (const auto& bf : bases) {
        if (bf.base.empty()) continue;
        if (s.size() > bf.base.size() && _wcsnicmp(s.c_str(), bf.base.c_str(), bf.base.size()) == 0) {
            if (s[bf.base.size()] == L'\\') {
                std::wstring sub = s.substr(bf.base.size() + 1);
                size_t slash = sub.find(L'\\');
                std::wstring firstPart = (slash == std::wstring::npos) ? sub : sub.substr(0, slash);
                if (firstPart.empty() || IsExcludedRoot(firstPart)) continue;

                // Candidate save path: wait for profile gate if active
                if (g_gateActive.load()) {
                    refix::overlay::saves::WaitForProfileGate();
                }

                // Query active profile
                std::wstring profileId = g_activeProfileId;
                if (profileId.empty()) {
                    profileId = ReFixOverlay::ActiveProfileId();
                }
                if (profileId.empty()) {
                    wchar_t envId[64] = {};
                    if (GetEnvironmentVariableW(L"REFIX_PROFILE_ID", envId, 64) > 0) profileId = envId;
                }
                if (profileId.empty()) {
                    wchar_t slotBuf[16] = {};
                    int slot = 0;
                    if (GetEnvironmentVariableW(L"REFIX_INSTANCE_SLOT", slotBuf, 16) > 0) {
                        slot = wcstol(slotBuf, nullptr, 10);
                    }
                    profileId = L"Guest_Slot" + std::to_wstring(slot);
                }

                std::wstring livePrefix = bf.base + L"\\" + firstPart;
                std::wstring stashPrefix = profiles::ProfileDir(profileId) + L"\\Saves\\" + bf.label + L"_" + Sanitize(firstPart);
                EnsureDir(stashPrefix);

                SaveRedirectRule rule;
                rule.livePrefix = livePrefix;
                rule.stashPrefix = stashPrefix;

                {
                    std::lock_guard<std::mutex> lock(g_redirectMutex);
                    g_redirectRules.push_back(rule);
                }

                ReFixLog("[Saves] Generic root redirection activated: '%s' -> '%s' (profile: %s)",
                         ToUtf8(livePrefix).c_str(), ToUtf8(stashPrefix).c_str(), ToUtf8(profileId).c_str());

                std::wstring rest = s.substr(livePrefix.size());
                out = (hasLong ? L"\\\\?\\" : L"") + stashPrefix + rest;
                return true;
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

static BOOL (WINAPI* TrueCreateDirectoryExW)(LPCWSTR, LPCWSTR, LPSECURITY_ATTRIBUTES) = nullptr;
static BOOL WINAPI HookCreateDirectoryExW(LPCWSTR lpTemplateDirectory, LPCWSTR lpNewDirectory, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    std::wstring redirTemplate, redirNew;
    LPCWSTR templ = lpTemplateDirectory;
    LPCWSTR newDir = lpNewDirectory;
    if (lpTemplateDirectory && CheckAndRedirect(lpTemplateDirectory, redirTemplate)) templ = redirTemplate.c_str();
    if (CheckAndRedirect(lpNewDirectory, redirNew)) {
        EnsureDir(Parent(redirNew));
        newDir = redirNew.c_str();
    }
    return TrueCreateDirectoryExW(templ, newDir, lpSecurityAttributes);
}

static BOOL (WINAPI* TrueCreateDirectoryExA)(LPCSTR, LPCSTR, LPSECURITY_ATTRIBUTES) = nullptr;
static BOOL WINAPI HookCreateDirectoryExA(LPCSTR lpTemplateDirectory, LPCSTR lpNewDirectory, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    std::string redirTemplate, redirNew;
    LPCSTR templ = lpTemplateDirectory;
    LPCSTR newDir = lpNewDirectory;
    if (lpTemplateDirectory && CheckAndRedirectA(lpTemplateDirectory, redirTemplate)) templ = redirTemplate.c_str();
    if (CheckAndRedirectA(lpNewDirectory, redirNew)) {
        EnsureDir(Parent(FromUtf8(redirNew)));
        newDir = redirNew.c_str();
    }
    return TrueCreateDirectoryExA(templ, newDir, lpSecurityAttributes);
}

static BOOL (WINAPI* TrueReplaceFileW)(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID) = nullptr;
static BOOL WINAPI HookReplaceFileW(LPCWSTR lpReplacedFileName, LPCWSTR lpReplacementFileName, LPCWSTR lpBackupFileName, DWORD dwReplaceFlags, LPVOID lpExclude, LPVOID lpReserved) {
    std::wstring redirReplaced, redirReplacement, redirBackup;
    LPCWSTR replaced = lpReplacedFileName;
    LPCWSTR replacement = lpReplacementFileName;
    LPCWSTR backup = lpBackupFileName;
    if (CheckAndRedirect(lpReplacedFileName, redirReplaced)) replaced = redirReplaced.c_str();
    if (CheckAndRedirect(lpReplacementFileName, redirReplacement)) replacement = redirReplacement.c_str();
    if (lpBackupFileName && CheckAndRedirect(lpBackupFileName, redirBackup)) backup = redirBackup.c_str();
    return TrueReplaceFileW(replaced, replacement, backup, dwReplaceFlags, lpExclude, lpReserved);
}

static BOOL (WINAPI* TrueReplaceFileA)(LPCSTR, LPCSTR, LPCSTR, DWORD, LPVOID, LPVOID) = nullptr;
static BOOL WINAPI HookReplaceFileA(LPCSTR lpReplacedFileName, LPCSTR lpReplacementFileName, LPCSTR lpBackupFileName, DWORD dwReplaceFlags, LPVOID lpExclude, LPVOID lpReserved) {
    std::string redirReplaced, redirReplacement, redirBackup;
    LPCSTR replaced = lpReplacedFileName;
    LPCSTR replacement = lpReplacementFileName;
    LPCSTR backup = lpBackupFileName;
    if (CheckAndRedirectA(lpReplacedFileName, redirReplaced)) replaced = redirReplaced.c_str();
    if (CheckAndRedirectA(lpReplacementFileName, redirReplacement)) replacement = redirReplacement.c_str();
    if (lpBackupFileName && CheckAndRedirectA(lpBackupFileName, redirBackup)) backup = redirBackup.c_str();
    return TrueReplaceFileA(replaced, replacement, backup, dwReplaceFlags, lpExclude, lpReserved);
}

static DWORD (WINAPI* TrueGetFullPathNameW)(LPCWSTR, DWORD, LPWSTR, LPWSTR*) = nullptr;
static DWORD WINAPI HookGetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart) {
    std::wstring redir;
    if (CheckAndRedirect(lpFileName, redir)) {
        return TrueGetFullPathNameW(redir.c_str(), nBufferLength, lpBuffer, lpFilePart);
    }
    return TrueGetFullPathNameW(lpFileName, nBufferLength, lpBuffer, lpFilePart);
}

static DWORD (WINAPI* TrueGetFullPathNameA)(LPCSTR, DWORD, LPSTR, LPSTR*) = nullptr;
static DWORD WINAPI HookGetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart) {
    std::string redir;
    if (CheckAndRedirectA(lpFileName, redir)) {
        return TrueGetFullPathNameA(redir.c_str(), nBufferLength, lpBuffer, lpFilePart);
    }
    return TrueGetFullPathNameA(lpFileName, nBufferLength, lpBuffer, lpFilePart);
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

} // namespace

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
    HOOK_API(CreateDirectoryExW);
    HOOK_API(CreateDirectoryExA);
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
    HOOK_API(ReplaceFileW);
    HOOK_API(ReplaceFileA);
    HOOK_API(SetFileAttributesW);
    HOOK_API(SetFileAttributesA);
    HOOK_API(GetFullPathNameW);
    HOOK_API(GetFullPathNameA);
#undef HOOK_API

    g_hooksInstalled = true;
    ReFixLog("[Saves] Multi-instance filesystem redirection hooks installed successfully");
}

void InitEarly() {
    if (!g_profileGateEvent) {
        g_profileGateEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    InstallFileSystemHooks();
}

void EnableMultiInstanceRedirection(const std::vector<Location>& locations, const Profile& active) {
    g_activeProfileId = active.id;
    {
        std::lock_guard<std::mutex> lock(g_redirectMutex);
        g_redirectRules.clear();
        for (const auto& loc : locations) {
            std::wstring stash = StashDir(active.id, loc.label);
            EnsureDir(stash);

            SaveRedirectRule rule;
            rule.livePrefix = NormalizePath(loc.live.c_str());
            rule.stashPrefix = stash;
            g_redirectRules.push_back(rule);
            ReFixLog("[Saves] Profile save redirect: '%s' -> '%s' (profile: %s)",
                     ToUtf8(rule.livePrefix).c_str(), ToUtf8(rule.stashPrefix).c_str(), ToUtf8(active.id).c_str());
        }
    }
    InstallFileSystemHooks();
}

void SwitchTo(const Profile& active, const std::vector<Profile>& all) {
    (void)all;
    g_activeProfileId = active.id;
    const std::vector<Location> locations = DetectLocations();
    EnableMultiInstanceRedirection(locations, active);
    SignalProfileGate();
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
    return stash;
}

} // namespace refix::overlay::saves
