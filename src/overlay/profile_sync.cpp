// Profile sync: what goes into a profile's package, when it is taken and when it is uploaded (profile_sync.h).
//
// Package layout: "Profile/<path>" = the profile folder (Cloud, Stats, Inventory, avatar; profile.ini stays per
// PC), "Saves/<label>/<path>" = each save folder the profile owns, live or stashed. A package is taken at game
// start when it differs from what this PC last synced and this PC has not changed since (or, when both
// changed, when it is the newer one); whatever it replaces is kept in %APPDATA%\ReFix\SyncBackups first.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "profile_sync.h"
#include "cloud_account.h"
#include "overlay_text.h"
#include "save_swap.h"

#include "../mods/crypto/monocypher.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::sync {
namespace {

constexpr DWORD kCheckEveryMs = 60000;
constexpr DWORD kShutdownWaitMs = 8000;
constexpr ULONGLONG kAutoSyncQuietMs = 5000;        // the game's save has finished writing
constexpr ULONGLONG kAutoSyncCooldownMs = 300000;   // at most one autosync upload every 5 minutes
constexpr int kBackupsKept = 3;

std::mutex g_mu;                 // everything below
std::optional<Profile> g_profile;
Status g_status;
std::atomic<bool> g_busy{ false };
std::atomic<bool> g_started{ false };
std::atomic<bool> g_seenRemote{ false };   // this session read the destination (or knows nothing newer is there)
HANDLE g_wake = nullptr;
uint8_t g_lastPending[32] = {};  // state seen by the previous check (uploads wait until it holds still)

std::wstring Ini() { return profiles::RootDir() + L"\\profiles.ini"; }
std::wstring StateIni() { return profiles::RootDir() + L"\\sync.ini"; }

std::wstring Hex(const uint8_t* b, size_t n) {
    static const wchar_t* d = L"0123456789abcdef";
    std::wstring s;
    for (size_t i = 0; i < n; ++i) { s += d[b[i] >> 4]; s += d[b[i] & 15]; }
    return s;
}

bool Syncs(const Profile& p) { return !p.guest && (profiles::IsSteamProfile(p) || SyncLocalProfiles()); }

std::wstring PackageName(const Profile& p) {
    return profiles::IsSteamProfile(p) ? L"steam_" + std::to_wstring(p.steamId) + L".rfxp" : L"local_" + p.id + L".rfxp";
}

// --- files of a profile ------------------------------------------------------------------------------------
struct Entry { std::string name; std::wstring path; uint64_t size; uint64_t mtime; };

void Walk(const std::wstring& dir, const std::string& prefix, std::vector<Entry>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   // never follow links out of the folder
        const std::string name = prefix + ToUtf8(n);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            Walk(dir + L"\\" + n, name + "/", out);
        } else {
            out.push_back({ name, dir + L"\\" + n, ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow,
                            ((uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime });
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

std::vector<Entry> Collect(const Profile& p) {
    std::vector<Entry> out;
    const std::wstring dir = profiles::ProfileDir(p.id);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring n = fd.cFileName;
            if (n == L"." || n == L".." || _wcsicmp(n.c_str(), L"profile.ini") == 0) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Saves\<label> is the stash; it goes under Saves/ like the live folders below.
                Walk(dir + L"\\" + n, (_wcsicmp(n.c_str(), L"Saves") == 0 ? std::string("Saves/") : "Profile/" + ToUtf8(n) + "/"), out);
            } else {
                out.push_back({ "Profile/" + ToUtf8(n), dir + L"\\" + n, ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow,
                                ((uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime });
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    for (const saves::Location& loc : saves::OwnedBy(p.id)) Walk(loc.live, "Saves/" + ToUtf8(loc.label) + "/", out);
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
    return out;
}

void StateHash(const std::vector<Entry>& files, uint8_t out[32]) {
    crypto_blake2b_ctx ctx;
    crypto_blake2b_general_init(&ctx, 32, nullptr, 0);
    for (const Entry& e : files) {
        crypto_blake2b_update(&ctx, (const uint8_t*)e.name.data(), e.name.size() + 1);
        crypto_blake2b_update(&ctx, (const uint8_t*)&e.size, sizeof(e.size));
        crypto_blake2b_update(&ctx, (const uint8_t*)&e.mtime, sizeof(e.mtime));
    }
    crypto_blake2b_final(&ctx, out);
}

bool ReadAll(const std::wstring& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);   // shared read: works while the game has the file open
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

// A package entry may only land inside its folder: no "..", no drive or root, no stream names.
bool SafeName(const std::string& n) {
    if (n.empty() || n[0] == '/' || n[0] == '\\' || n.find(':') != std::string::npos) return false;
    size_t start = 0;
    while (start <= n.size()) {
        const size_t end = n.find_first_of("/\\", start);
        const std::string part = n.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

void EnsureParents(const std::wstring& file) {
    for (size_t i = 3; i < file.size(); ++i) {
        if (file[i] == L'\\') CreateDirectoryW(file.substr(0, i).c_str(), nullptr);
    }
}

void ClearDir(const std::wstring& dir) {
    std::vector<Entry> files;
    Walk(dir, "", files);
    for (const Entry& e : files) DeleteFileW(e.path.c_str());
}

std::string LastHash(const Profile& p, uint64_t* when = nullptr) {
    wchar_t buf[128] = {};
    GetPrivateProfileStringW(p.id.c_str(), L"LastHash", L"", buf, 128, StateIni().c_str());
    if (when) {
        wchar_t t[32] = {};
        GetPrivateProfileStringW(p.id.c_str(), L"LastSync", L"0", t, 32, StateIni().c_str());
        *when = _wcstoui64(t, nullptr, 10);
    }
    return ToUtf8(buf);
}

void SetLastHash(const Profile& p, const uint8_t hash[32]) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const uint64_t now = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    WritePrivateProfileStringW(p.id.c_str(), L"LastHash", Hex(hash, 32).c_str(), StateIni().c_str());
    WritePrivateProfileStringW(p.id.c_str(), L"LastSync", std::to_wstring(now).c_str(), StateIni().c_str());
    std::lock_guard<std::mutex> lock(g_mu);
    g_status.lastSync = now;
}

std::unique_ptr<ISyncTarget> Target() {
    const std::wstring where = EffectiveFolder();
    cloud::Provider p;
    return cloud::FromChoice(where, p) ? cloud::MakeTarget(p) : MakeFolderTarget(where);
}

std::wstring Destination() {   // what the player reads
    const std::wstring where = EffectiveFolder();
    cloud::Provider p;
    return cloud::FromChoice(where, p) ? FromUtf8(cloud::Label(p)) : where;
}

void SetState(State s, const std::string& error = {}) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_status.state = s;
    g_status.error = error;
    g_status.destination = Destination();
}

bool BuildPackage(const Profile& p, const std::vector<Entry>& files, const uint8_t hash[32], std::vector<uint8_t>& pkg) {
    FileList list;
    const std::string name = ToUtf8(p.name);   // for "Import": the new profile takes this name
    list.emplace_back("Meta/name.txt", std::vector<uint8_t>(name.begin(), name.end()));
    for (const Entry& e : files) {
        std::vector<uint8_t> data;
        if (!ReadAll(e.path, data)) {
            ReFixLog("[Sync] Skipped %s (locked)", e.name.c_str());
            continue;
        }
        list.emplace_back(e.name, std::move(data));
    }
    return Pack(list, profiles::IsSteamProfile(p) ? p.steamId : 0, hash, pkg);
}

void Backup(const Profile& p, const std::vector<Entry>& files, const uint8_t hash[32]) {
    if (files.empty()) return;
    const std::wstring dir = profiles::RootDir() + L"\\SyncBackups\\" + p.id;
    EnsureParents(dir + L"\\x");
    std::vector<uint8_t> pkg;
    if (!BuildPackage(p, files, hash, pkg)) return;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const std::wstring file = dir + L"\\" + std::to_wstring(((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) + L".rfxp";
    MakeFolderTarget(dir)->Write(file.substr(dir.size() + 1), pkg);
    std::vector<Entry> old;
    Walk(dir, "", old);
    std::sort(old.begin(), old.end(), [](const Entry& a, const Entry& b) { return a.name > b.name; });
    for (size_t i = kBackupsKept; i < old.size(); ++i) DeleteFileW(old[i].path.c_str());
}

// Writes a package's files over the profile on this PC (save folders it owns get them live).
bool Apply(const Profile& p, const FileList& files) {
    for (const auto& f : files) if (!SafeName(f.first)) return false;
    std::map<std::wstring, std::wstring> live;   // label -> live folder owned by p on this PC
    for (const saves::Location& loc : saves::OwnedBy(p.id)) live[loc.label] = loc.live;
    const std::wstring dir = profiles::ProfileDir(p.id);
    for (const wchar_t* sub : { L"\\Cloud", L"\\Stats", L"\\Inventory", L"\\Saves" }) ClearDir(dir + sub);
    for (const auto& [label, path] : live) ClearDir(path);
    for (const auto& [name, data] : files) {
        std::wstring target;
        if (name.rfind("Profile/", 0) == 0) {
            target = dir + L"\\" + FromUtf8(name.substr(8));
        } else if (name.rfind("Saves/", 0) == 0) {
            const std::string rest = name.substr(6);
            const size_t slash = rest.find('/');
            if (slash == std::string::npos) continue;
            const std::wstring label = FromUtf8(rest.substr(0, slash)), file = FromUtf8(rest.substr(slash + 1));
            auto it = live.find(label);
            target = (it != live.end() ? it->second : saves::StashOf(p.id, label)) + L"\\" + file;
        } else {
            continue;
        }
        std::replace(target.begin(), target.end(), L'/', L'\\');
        EnsureParents(target);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out || !out.write((const char*)data.data(), (std::streamsize)data.size())) return false;
    }
    return true;
}

// Game start: take the destination's package when it is the one to keep.
void Pull(const Profile& p) {
    auto target = Target();
    if (!target->Ready()) { SetState(State::Unavailable, "Destination not available"); return; }
    std::vector<uint8_t> pkg;
    const std::vector<Entry> local = Collect(p);
    uint8_t localHash[32];
    StateHash(local, localHash);
    const bool read = target->Read(PackageName(p), pkg);
    if (read || !target->Failed()) g_seenRemote = true;
    if (!read) {
        // Unreachable is not "missing": never let a later upload replace a package this PC could not see.
        if (target->Failed()) SetState(State::Unavailable, "Destination not available");
        else SetState(local.empty() ? State::Synced : State::Pending);
        return;
    }
    FileList files;
    uint64_t steamId = 0;
    uint8_t remoteHash[32];
    if (!Unpack(pkg, files, &steamId, remoteHash) || (profiles::IsSteamProfile(p) && steamId != p.steamId)) {
        SetState(State::Failed, "The saved package is damaged");
        return;
    }
    const std::string last = LastHash(p), localHex = ToUtf8(Hex(localHash, 32)), remoteHex = ToUtf8(Hex(remoteHash, 32));
    if (remoteHex == localHex || remoteHex == last) {   // nothing new over there
        if (remoteHex == localHex) SetLastHash(p, localHash);
        SetState(remoteHex == localHex ? State::Synced : State::Pending);
        return;
    }
    // New over there. Here unchanged since the last sync (or never had anything): take it. Both changed:
    // the package wins too, since it was uploaded after this PC's last sync; this PC's copy is backed up.
    Backup(p, local, localHash);
    if (!Apply(p, files)) { SetState(State::Failed, "Could not write the downloaded saves"); return; }
    uint8_t now[32];
    StateHash(Collect(p), now);
    SetLastHash(p, now);
    SetState(State::Synced);
    ReFixLog("[Sync] Took the newer package of %s (%zu files)%s", ToUtf8(p.id).c_str(), files.size(),
             last.empty() || last == localHex ? "" : "; this PC's changes kept in SyncBackups");
}

// Before the first upload of a session whose start could not reach the destination: only when what is there is
// what this PC last synced (taking a newer package mid-game would overwrite live saves; the next start takes it).
bool RemoteUnchanged(const Profile& p, ISyncTarget& target) {
    if (g_seenRemote) return true;
    std::vector<uint8_t> pkg;
    if (!target.Read(PackageName(p), pkg)) {
        if (target.Failed()) { SetState(State::Unavailable, "Destination not available"); return false; }
        return g_seenRemote = true;
    }
    FileList files;
    uint8_t remoteHash[32];
    if (Unpack(pkg, files, nullptr, remoteHash) && ToUtf8(Hex(remoteHash, 32)) == LastHash(p)) return g_seenRemote = true;
    SetState(State::Failed, "Newer saves from another PC: restart the game to take them");
    return false;
}

// Upload when the profile changed since the last sync. `force`: now, without waiting for it to hold still.
void Push(const Profile& p, bool force) {
    const std::vector<Entry> files = Collect(p);
    uint8_t hash[32];
    StateHash(files, hash);
    if (ToUtf8(Hex(hash, 32)) == LastHash(p)) { SetState(State::Synced); return; }
    const bool steady = memcmp(hash, g_lastPending, 32) == 0;
    memcpy(g_lastPending, hash, 32);
    auto target = Target();
    if (!target->Ready()) { SetState(State::Unavailable, "Destination not available"); return; }
    if (!force && !steady) { SetState(State::Pending); return; }
    if (!RemoteUnchanged(p, *target)) return;
    SetState(State::Syncing);
    std::vector<uint8_t> pkg;
    if (!BuildPackage(p, files, hash, pkg)) { SetState(State::Failed, "Could not build the package"); return; }
    if (!target->Write(PackageName(p), pkg)) { SetState(State::Failed, "Could not write to the destination"); return; }
    SetLastHash(p, hash);
    SetState(State::Synced);
    ReFixLog("[Sync] Uploaded %s (%zu files, %zu KB)", ToUtf8(p.id).c_str(), files.size(), pkg.size() / 1024);
}

void Run(bool force) {
    if (g_busy.exchange(true)) return;
    std::optional<Profile> p;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        p = g_profile;
    }
    if (p && Syncs(*p)) Push(*p, force);
    g_busy = false;
}

// Folders the game writes its saves to (the profile's emulated Steam Cloud and the live save folders).
std::vector<std::wstring> WatchedDirs() {
    std::lock_guard<std::mutex> lock(g_mu);
    std::vector<std::wstring> dirs;
    if (!g_profile || !Syncs(*g_profile)) return dirs;
    dirs.push_back(profiles::ProfileDir(g_profile->id));
    for (const saves::Location& loc : saves::OwnedBy(g_profile->id)) dirs.push_back(loc.live);
    return dirs;
}

DWORD WINAPI Loop(LPVOID) {
    std::vector<std::wstring> dirs;
    std::vector<HANDLE> watches;
    ULONGLONG savedAt = 0, lastAuto = 0;   // last save seen, last autosync upload (GetTickCount64)
    ULONGLONG nextCheck = GetTickCount64() + kCheckEveryMs;
    for (;;) {
        if (std::vector<std::wstring> now = AutoSync() ? WatchedDirs() : std::vector<std::wstring>{}; now != dirs) {
            for (HANDLE h : watches) FindCloseChangeNotification(h);
            watches.clear();
            dirs = std::move(now);
            for (const std::wstring& d : dirs) {
                HANDLE h = FindFirstChangeNotificationW(d.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
                if (h != INVALID_HANDLE_VALUE && watches.size() < MAXIMUM_WAIT_OBJECTS - 1) watches.push_back(h);
                else if (h != INVALID_HANDLE_VALUE) FindCloseChangeNotification(h);
            }
        }
        const ULONGLONG t = GetTickCount64();
        ULONGLONG due = nextCheck;
        if (savedAt) due = (std::min)(due, (std::max)(savedAt + kAutoSyncQuietMs, lastAuto + kAutoSyncCooldownMs));
        std::vector<HANDLE> waitOn{ g_wake };
        waitOn.insert(waitOn.end(), watches.begin(), watches.end());
        const DWORD r = WaitForMultipleObjects((DWORD)waitOn.size(), waitOn.data(), FALSE, due > t ? (DWORD)(due - t) : 0);
        const ULONGLONG now = GetTickCount64();
        if (r == WAIT_OBJECT_0) { Run(true); nextCheck = now + kCheckEveryMs; continue; }
        if (r > WAIT_OBJECT_0 && r < WAIT_OBJECT_0 + waitOn.size()) {
            FindNextChangeNotification(waitOn[r - WAIT_OBJECT_0]);
            savedAt = now;   // saves come in bursts: wait until they stop
            continue;
        }
        if (savedAt && now >= savedAt + kAutoSyncQuietMs && (lastAuto == 0 || now >= lastAuto + kAutoSyncCooldownMs)) {
            ReFixLog("[Sync] Autosync after the game saved");
            Run(true);
            savedAt = 0;
            lastAuto = now;
            nextCheck = now + kCheckEveryMs;
        } else if (now >= nextCheck) {
            Run(false);
            nextCheck = now + kCheckEveryMs;
        }
    }
}

} // namespace

std::wstring ChosenFolder() {
    wchar_t buf[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Sync", L"Folder", L"", buf, MAX_PATH, Ini().c_str());
    return buf;
}

std::wstring EffectiveFolder() {
    const std::wstring chosen = ChosenFolder();
    if (chosen == kLocalOnly) return {};
    if (!chosen.empty()) return chosen;
    const std::vector<CloudFolder> clouds = CloudFolders();
    return clouds.empty() ? std::wstring() : clouds.front().path + L"\\ReFix Sync";
}

void SetFolder(const std::wstring& folder) {
    WritePrivateProfileStringW(L"Sync", L"Folder", folder.empty() ? nullptr : folder.c_str(), Ini().c_str());
    SyncNow();
}

bool AutoSync() { return GetPrivateProfileIntW(L"Sync", L"AutoSync", 1, Ini().c_str()) != 0; }

void SetAutoSync(bool on) {
    WritePrivateProfileStringW(L"Sync", L"AutoSync", on ? L"1" : L"0", Ini().c_str());
    SyncNow();
}

bool SyncLocalProfiles() { return GetPrivateProfileIntW(L"Sync", L"LocalProfiles", 0, Ini().c_str()) != 0; }

void SetSyncLocalProfiles(bool on) {
    WritePrivateProfileStringW(L"Sync", L"LocalProfiles", on ? L"1" : L"0", Ini().c_str());
    SyncNow();
}

bool ExportTo(const Profile& p, const std::wstring& file, std::string& error) {
    const std::vector<Entry> files = Collect(p);
    uint8_t hash[32];
    StateHash(files, hash);
    std::vector<uint8_t> pkg;
    const size_t slash = file.find_last_of(L"\\/");
    if (slash == std::wstring::npos || !BuildPackage(p, files, hash, pkg) ||
        !MakeFolderTarget(file.substr(0, slash))->Write(file.substr(slash + 1), pkg)) {
        error = "Could not write the file";
        return false;
    }
    ReFixLog("[Sync] Exported %s (%zu files)", ToUtf8(p.id).c_str(), files.size());
    return true;
}

std::optional<Profile> ImportAsNewProfile(const std::wstring& file, std::string& error) {
    std::vector<uint8_t> pkg;
    const size_t slash = file.find_last_of(L"\\/");
    if (slash == std::wstring::npos || !MakeFolderTarget(file.substr(0, slash))->Read(file.substr(slash + 1), pkg)) {
        error = "Could not read the file";
        return std::nullopt;
    }
    FileList files;
    uint8_t hash[32];
    if (!Unpack(pkg, files, nullptr, hash)) {
        error = "This is not a ReFix profile file, or it is damaged";
        return std::nullopt;
    }
    std::wstring name = file.substr(slash + 1);
    bool avatar = false;
    for (const auto& [n, data] : files) {
        if (n == "Meta/name.txt" && !data.empty()) name = FromUtf8(std::string(data.begin(), data.end()));
        avatar |= n == "Profile/avatar.png";
    }
    std::vector<Profile> all = profiles::LoadAll();
    Profile p = profiles::MakeNew(name, all);   // fresh id and SteamID: it never takes over a profile here
    p.customAvatar = avatar;
    if (!profiles::Save(p) || !Apply(p, files)) {
        error = "Could not write the imported saves";
        return std::nullopt;
    }
    ReFixLog("[Sync] Imported %ls as new profile %s (%zu files)", file.c_str(), ToUtf8(p.id).c_str(), files.size());
    return p;
}

void Begin(const Profile& active) {
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_profile = active;
        g_status = Status{};
        g_seenRemote = false;
        LastHash(active, &g_status.lastSync);
    }
    if (Syncs(active)) Pull(active);
    if (!g_started.exchange(true)) {
        g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HANDLE t = CreateThread(nullptr, 0, Loop, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
}

Status Get() {
    std::lock_guard<std::mutex> lock(g_mu);
    Status s = g_status;
    if (!g_profile || !Syncs(*g_profile)) s.state = State::Off;
    if (g_busy && s.state != State::Off) s.state = State::Syncing;
    return s;
}

void SyncNow() {
    if (g_wake) SetEvent(g_wake);
}

void Shutdown() {
    if (!g_started) return;
    SyncNow();
    for (DWORD waited = 0; waited < kShutdownWaitMs; waited += 100) {
        Sleep(100);
        if (!g_busy && Get().state != State::Pending) break;
    }
}

} // namespace refix::overlay::sync
