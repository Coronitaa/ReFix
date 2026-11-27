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
#include "achievements.h"
#include "cloud_account.h"
#include "overlay_app.h"
#include "overlay_i18n.h"
#include "overlay_text.h"
#include "save_swap.h"
#include "../mods/mod_host.h"
#include "../unreal_steam_emu.h"

#include "../mods/crypto/monocypher.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::sync {
namespace {

constexpr DWORD kCheckEveryMs = 60000;
constexpr DWORD kShutdownWaitMs = 15000;
constexpr ULONGLONG kAutoSyncQuietMs = 5000;        // the game's save has finished writing
constexpr ULONGLONG kAutoSyncCooldownMs = 300000;   // at most one autosync upload every 5 minutes
constexpr int kBackupsKept = 3;

std::mutex g_mu;                 // everything below
std::optional<Profile> g_profile;
Status g_status;
std::atomic<bool> g_busy{ false };
std::atomic<bool> g_manualPushPending{ false };
std::atomic<bool> g_started{ false };
std::atomic<bool> g_stopLoop{ false };
std::atomic<bool> g_seenRemote{ false };   // this session read the destination (or knows nothing newer is there)
HANDLE g_wake = nullptr;
HANDLE g_loopThread = nullptr;
uint8_t g_lastPending[32] = {};  // state seen by the previous check (uploads wait until it holds still)

std::mutex g_conflictMu;
ConflictInfo g_conflict;
Profile g_conflictProfile;
FileList g_conflictCloudFiles;
uint8_t g_conflictRemoteHash[32] = {};
uint8_t g_conflictLocalHash[32] = {};
std::string g_conflictRemoteTag;

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

bool IsVolatileSyncEntry(const std::wstring& n, bool isDir) {
    if (isDir) {
        return _wcsicmp(n.c_str(), L"Sentry") == 0 ||
               _wcsicmp(n.c_str(), L"SentryNative") == 0 ||
               _wcsicmp(n.c_str(), L"CrashDumps") == 0 ||
               _wcsicmp(n.c_str(), L"eventcache") == 0 ||
               _wcsicmp(n.c_str(), L"Unity") == 0 ||
               _wcsicmp(n.c_str(), L"ShaderCache") == 0 ||
               _wcsicmp(n.c_str(), L"RfsCache") == 0 ||
               _wcsicmp(n.c_str(), L"RfsPlayerCache") == 0;
    }
    if (_wcsicmp(n.c_str(), L"desktop.ini") == 0 ||
        _wcsicmp(n.c_str(), L"IconCache.db") == 0 ||
        _wcsicmp(n.c_str(), L"RemoteConfigCache.json") == 0 ||
        _wcsicmp(n.c_str(), L"Player.log") == 0 ||
        _wcsicmp(n.c_str(), L"Player-prev.log") == 0 ||
        _wcsicmp(n.c_str(), L"output_log.txt") == 0) {
        return true;
    }
    const size_t dot = n.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        const std::wstring ext = n.substr(dot);
        if (_wcsicmp(ext.c_str(), L".log") == 0 ||
            _wcsicmp(ext.c_str(), L".tmp") == 0 ||
            _wcsicmp(ext.c_str(), L".lock") == 0 ||
            _wcsicmp(ext.c_str(), L".dmp") == 0) {
            return true;
        }
    }
    return false;
}

void Walk(const std::wstring& dir, const std::string& prefix, std::vector<Entry>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   // never follow links out of the folder
        const bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (IsVolatileSyncEntry(n, isDir)) continue;
        const std::string name = prefix + ToUtf8(n);
        if (isDir) {
            Walk(dir + L"\\" + n, name + "/", out);
        } else {
            out.push_back({ name, dir + L"\\" + n, ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow,
                            ((uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime });
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

std::vector<Entry> Collect(const Profile& p) {
    saves::ScopedBypass fsBypass;
    std::vector<Entry> out;
    const std::wstring dir = profiles::ProfileDir(p.id);
    std::set<std::string> stashedLabels;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring n = fd.cFileName;
            if (n == L"." || n == L".." || _wcsicmp(n.c_str(), L"profile.ini") == 0) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Saves\<label> is the canonical redirected save stash for the profile.
                if (_wcsicmp(n.c_str(), L"Saves") == 0) {
                    const size_t before = out.size();
                    Walk(dir + L"\\" + n, "Saves/", out);
                    for (size_t i = before; i < out.size(); ++i) {
                        const size_t s1 = out[i].name.find('/', 6);
                        if (s1 != std::string::npos) stashedLabels.insert(out[i].name.substr(6, s1 - 6));
                    }
                } else {
                    Walk(dir + L"\\" + n, "Profile/" + ToUtf8(n) + "/", out);
                }
            } else if (!IsVolatileSyncEntry(n, false)) {
                out.push_back({ "Profile/" + ToUtf8(n), dir + L"\\" + n, ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow,
                                ((uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime });
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    // Pre-redirection legacy fallback: only pull from live folder if this profile's Saves\<label> stash has no files yet.
    for (const saves::Location& loc : saves::OwnedBy(p.id)) {
        const std::string labelUtf8 = ToUtf8(loc.label);
        if (stashedLabels.count(labelUtf8)) continue;
        Walk(loc.live, "Saves/" + labelUtf8 + "/", out);
    }
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
    out.erase(std::unique(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.name == b.name; }), out.end());
    return out;
}

bool ReadAll(const std::wstring& path, std::vector<uint8_t>& out);

void StateHash(const std::vector<Entry>& files, uint8_t out[32]) {
    saves::ScopedBypass fsBypass;
    crypto_blake2b_ctx ctx;
    crypto_blake2b_general_init(&ctx, 32, nullptr, 0);
    for (const Entry& e : files) {
        crypto_blake2b_update(&ctx, (const uint8_t*)e.name.data(), e.name.size() + 1);
        crypto_blake2b_update(&ctx, (const uint8_t*)&e.size, sizeof(e.size));
        std::vector<uint8_t> data;
        if (ReadAll(e.path, data)) {
            uint8_t fh[32] = {};
            crypto_blake2b_general(fh, 32, nullptr, 0, data.data(), data.size());
            crypto_blake2b_update(&ctx, fh, sizeof(fh));
        } else {
            crypto_blake2b_update(&ctx, (const uint8_t*)&e.mtime, sizeof(e.mtime));
        }
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

std::string LastRemoteHash(const Profile& p) {
    wchar_t buf[128] = {};
    GetPrivateProfileStringW(p.id.c_str(), L"LastRemoteHash", L"", buf, 128, StateIni().c_str());
    if (buf[0]) return ToUtf8(buf);
    return LastHash(p);
}

std::string LastRemoteTag(const Profile& p) {
    wchar_t buf[512] = {};
    GetPrivateProfileStringW(p.id.c_str(), L"LastRemoteTag", L"", buf, 512, StateIni().c_str());
    return ToUtf8(buf);
}

void SetRemoteTag(const Profile& p, const std::string& tag) {
    const std::wstring ini = StateIni();
    const std::wstring wtag = FromUtf8(tag);
    WritePrivateProfileStringW(p.id.c_str(), L"LastRemoteTag", wtag.empty() ? nullptr : wtag.c_str(), ini.c_str());
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, ini.c_str());
}

void SetSyncHashes(const Profile& p, const uint8_t localHash[32], const uint8_t remoteHash[32]) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const uint64_t now = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    const std::wstring ini = StateIni();
    WritePrivateProfileStringW(p.id.c_str(), L"LastHash", Hex(localHash, 32).c_str(), ini.c_str());
    WritePrivateProfileStringW(p.id.c_str(), L"LastRemoteHash", Hex(remoteHash, 32).c_str(), ini.c_str());
    WritePrivateProfileStringW(p.id.c_str(), L"LastSync", std::to_wstring(now).c_str(), ini.c_str());
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, ini.c_str());
    std::lock_guard<std::mutex> lock(g_mu);
    g_status.lastSync = now;
}

void MarkSyncedAfterApply(const Profile& p, const uint8_t remoteHash[32], const std::string& remoteTag = {}) {
    uint8_t localNow[32];
    StateHash(Collect(p), localNow);
    SetSyncHashes(p, localNow, remoteHash);
    if (!remoteTag.empty()) SetRemoteTag(p, remoteTag);
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
    saves::ScopedBypass fsBypass;
    FileList list;
    const std::string name = ToUtf8(p.name);   // for "Import": the new profile takes this name
    list.emplace_back("Meta/name.txt", std::vector<uint8_t>(name.begin(), name.end()));
    const std::string orig = ToUtf8(p.originGame);
    list.emplace_back("Meta/origin_game.txt", std::vector<uint8_t>(orig.begin(), orig.end()));
    std::string gamesStr;
    for (const auto& g : p.games) {
        gamesStr += ToUtf8(g.key) + "=" + ToUtf8(g.name) + "\n";
    }
    list.emplace_back("Meta/games.txt", std::vector<uint8_t>(gamesStr.begin(), gamesStr.end()));
    const std::string stockAv = std::to_string(p.stockAvatar);
    list.emplace_back("Meta/stock_avatar.txt", std::vector<uint8_t>(stockAv.begin(), stockAv.end()));
    const std::string ptStr = std::to_string(p.playtimeSeconds);
    list.emplace_back("Meta/playtime.txt", std::vector<uint8_t>(ptStr.begin(), ptStr.end()));
    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);
    const std::string savedAt = std::to_string(((uint64_t)ftNow.dwHighDateTime << 32) | ftNow.dwLowDateTime);
    list.emplace_back("Meta/saved_at.txt", std::vector<uint8_t>(savedAt.begin(), savedAt.end()));
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

// Writes a package's files over the profile's canonical stash (SaveSwap redirects live paths here).
bool Apply(const Profile& p, const FileList& files) {
    saves::ScopedBypass fsBypass;
    for (const auto& f : files) if (!SafeName(f.first)) return false;
    const std::wstring dir = profiles::ProfileDir(p.id);
    for (const wchar_t* sub : { L"\\Cloud", L"\\Stats", L"\\Inventory", L"\\Saves" }) ClearDir(dir + sub);
    for (const auto& [name, data] : files) {
        std::wstring target;
        if (name.rfind("Profile/", 0) == 0) {
            target = dir + L"\\" + FromUtf8(name.substr(8));
        } else if (name.rfind("Saves/", 0) == 0) {
            const std::string rest = name.substr(6);
            const size_t slash = rest.find('/');
            if (slash == std::string::npos) continue;
            const std::wstring label = FromUtf8(rest.substr(0, slash)), file = FromUtf8(rest.substr(slash + 1));
            target = saves::StashOf(p.id, label) + L"\\" + file;
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
    const std::string lastLocal = LastHash(p), lastRemote = LastRemoteHash(p);
    const std::string localHex = ToUtf8(Hex(localHash, 32));

    // Fast path: ask destination metadata (etag/md5/mtime) before downloading multi-MB packages on every launch.
    std::string rawTag;
    const bool gotTag = target->RemoteTag(PackageName(p), rawTag);
    const std::string remoteTag = (gotTag && !rawTag.empty()) ? (ToUtf8(target->Name()) + "|" + rawTag) : std::string();
    if (gotTag) {
        if (rawTag.empty()) {
            g_seenRemote = true;
            SetRemoteTag(p, "");
            SetState(local.empty() ? State::Synced : State::Pending);
            return;
        }
        if (!remoteTag.empty() && remoteTag == LastRemoteTag(p) && !lastRemote.empty()) {
            g_seenRemote = true;
            const bool localMatches = (localHex == lastRemote || localHex == lastLocal);
            if (localMatches && lastLocal != localHex) {
                uint8_t rhBytes[32] = {};
                if (lastRemote.size() == 64) {
                    for (size_t i = 0; i < 32; ++i)
                        rhBytes[i] = (uint8_t)strtoul(lastRemote.substr(i * 2, 2).c_str(), nullptr, 16);
                    SetSyncHashes(p, localHash, rhBytes);
                }
            }
            SetState(localMatches ? State::Synced : State::Pending);
            return;
        }
    }

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
    const std::string remoteHex = ToUtf8(Hex(remoteHash, 32));
    if (remoteHex == localHex || remoteHex == lastRemote) {   // nothing new over there
        const bool localMatches = (remoteHex == localHex || lastLocal == localHex);
        if (localMatches) SetSyncHashes(p, localHash, remoteHash);
        if (!remoteTag.empty()) SetRemoteTag(p, remoteTag);
        SetState(localMatches ? State::Synced : State::Pending);
        return;
    }
    // Both local and remote have diverged with un-synced data: prompt user to resolve conflict
    if (!local.empty() && !lastLocal.empty() && lastLocal != localHex) {
        std::lock_guard<std::mutex> lock(g_conflictMu);
        g_conflict.active = true;
        g_conflict.profileId = p.id;
        g_conflict.profileName = p.name;
        g_conflict.steamId = p.steamId;
        g_conflict.localFiles = local.size();
        g_conflict.cloudFiles = files.size();
        g_conflictProfile = p;
        g_conflictCloudFiles = std::move(files);
        memcpy(g_conflictRemoteHash, remoteHash, 32);
        memcpy(g_conflictLocalHash, localHash, 32);
        g_conflictRemoteTag = remoteTag;
        SetState(State::Pending, "Sync conflict detected");
        ReFixLog("[Sync] Conflict detected for %s: local=%zu files, cloud=%zu files", ToUtf8(p.name).c_str(), local.size(), g_conflictCloudFiles.size());
        return;
    }
    // Unchanged locally or local had nothing: take remote
    Backup(p, local, localHash);
    if (!Apply(p, files)) { SetState(State::Failed, "Could not write the downloaded saves"); return; }
    MarkSyncedAfterApply(p, remoteHash, remoteTag);
    SetState(State::Synced);
    ReFixLog("[Sync] Took the newer package of %s (%zu files)%s", ToUtf8(p.id).c_str(), files.size(),
             lastLocal.empty() || lastLocal == localHex ? "" : "; this PC's changes kept in SyncBackups");
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
    if (Unpack(pkg, files, nullptr, remoteHash) && ToUtf8(Hex(remoteHash, 32)) == LastRemoteHash(p)) return g_seenRemote = true;
    SetState(State::Failed, "Newer saves from another PC: restart the game to take them");
    return false;
}

// Upload when the profile changed since the last sync. `force`: now, without waiting for it to hold still.
// `overwriteRemote`: explicit user conflict resolution ("Local data") that must overwrite the remote package
// even if local hashes match LastHash or RemoteUnchanged would reject it.
void Push(const Profile& p, bool force, bool overwriteRemote = false) {
    const std::vector<Entry> files = Collect(p);
    uint8_t hash[32];
    StateHash(files, hash);
    if (!overwriteRemote && ToUtf8(Hex(hash, 32)) == LastHash(p)) { SetState(State::Synced); return; }
    const bool steady = memcmp(hash, g_lastPending, 32) == 0;
    memcpy(g_lastPending, hash, 32);
    auto target = Target();
    if (!target->Ready()) { SetState(State::Unavailable, "Destination not available"); return; }
    if (!force && !steady) { SetState(State::Pending); return; }
    if (!overwriteRemote && !RemoteUnchanged(p, *target)) return;
    SetState(State::Syncing);
    std::vector<uint8_t> pkg;
    if (!BuildPackage(p, files, hash, pkg)) { SetState(State::Failed, "Could not build the package"); return; }
    if (!target->Write(PackageName(p), pkg)) { SetState(State::Failed, "Could not write to the destination"); return; }
    g_seenRemote = true;
    SetSyncHashes(p, hash, hash);
    std::string newRawTag;
    if (target->RemoteTag(PackageName(p), newRawTag) && !newRawTag.empty()) {
        SetRemoteTag(p, ToUtf8(target->Name()) + "|" + newRawTag);
    } else {
        SetRemoteTag(p, "");
    }
    SetState(State::Synced);
    ReFixLog("[Sync] Uploaded %s (%zu files, %zu KB)%s", ToUtf8(p.id).c_str(), files.size(), pkg.size() / 1024,
             overwriteRemote ? " [local conflict overwrite]" : "");
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
    for (; !g_stopLoop.load(std::memory_order_acquire);) {
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
        if (g_stopLoop.load(std::memory_order_acquire)) break;
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
    for (HANDLE h : watches) FindCloseChangeNotification(h);
    return 0;
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
    if (!folder.empty() && folder != kLocalOnly) {
        CheckCloudForProfiles();
    }
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
        g_stopLoop.store(false, std::memory_order_release);
        g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_loopThread = CreateThread(nullptr, 0, Loop, nullptr, 0, nullptr);
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
    if (!g_started.exchange(false)) return;
    if (Get().state != State::Off || g_manualPushPending.load()) {
        SyncNow();
        for (DWORD waited = 0; waited < kShutdownWaitMs; waited += 50) {
            Sleep(50);
            if (!g_busy && !g_manualPushPending.load() && Get().state != State::Pending) break;
        }
    }
    g_stopLoop.store(true, std::memory_order_release);
    if (g_wake) SetEvent(g_wake);
    if (g_loopThread) {
        WaitForSingleObject(g_loopThread, 1500);
        CloseHandle(g_loopThread);
        g_loopThread = nullptr;
    }
}

bool HasConflict() {
    std::lock_guard<std::mutex> lock(g_conflictMu);
    return g_conflict.active;
}

ConflictInfo GetConflict() {
    std::lock_guard<std::mutex> lock(g_conflictMu);
    return g_conflict;
}

void ResolveConflictUseCloud() {
    Profile p;
    FileList files;
    uint8_t remoteHash[32];
    std::string remoteTag;
    {
        std::lock_guard<std::mutex> lock(g_conflictMu);
        if (!g_conflict.active) return;
        p = g_conflictProfile;
        files = std::move(g_conflictCloudFiles);
        memcpy(remoteHash, g_conflictRemoteHash, 32);
        remoteTag = g_conflictRemoteTag;
        g_conflict.active = false;
    }
    const std::vector<Entry> local = Collect(p);
    uint8_t localHash[32];
    StateHash(local, localHash);
    Backup(p, local, localHash);
    if (Apply(p, files)) {
        MarkSyncedAfterApply(p, remoteHash, remoteTag);
        SetState(State::Synced);
        ReFixLog("[Sync] Conflict resolved using cloud data for profile %s", ToUtf8(p.id).c_str());
        // Skip flushing old in-memory stats so the downloaded cloud stats.ini stays intact on restart.
        App::Get().RestartWithProfile(p.id, false);
    } else {
        SetState(State::Failed, "Could not write the downloaded saves");
    }
}

void ResolveConflictUseLocal() {
    Profile p;
    {
        std::lock_guard<std::mutex> lock(g_conflictMu);
        if (!g_conflict.active) return;
        p = g_conflictProfile;
        g_conflictCloudFiles.clear();
        g_conflict.active = false;
    }
    g_seenRemote = true;
    g_manualPushPending = true;
    SetState(State::Syncing);
    std::thread([p] {
        struct Guard {
            ~Guard() { g_manualPushPending = false; }
        } guard;
        while (g_busy.exchange(true)) Sleep(20);
        Push(p, true, true);
        g_busy = false;
        ReFixLog("[Sync] Conflict resolved using local data for profile %s", ToUtf8(p.id).c_str());
    }).detach();
}

void ResolveConflictKeepBoth() {
    Profile p;
    FileList files;
    uint8_t remoteHash[32];
    {
        std::lock_guard<std::mutex> lock(g_conflictMu);
        if (!g_conflict.active) return;
        p = g_conflictProfile;
        files = std::move(g_conflictCloudFiles);
        memcpy(remoteHash, g_conflictRemoteHash, 32);
        g_conflict.active = false;
    }
    std::vector<Profile> all = profiles::LoadAll();
    const std::wstring suffix = i18n::Spanish() ? L" (Nube)" : L" (Cloud)";
    Profile newP = profiles::MakeNew(p.name + suffix, all);
    newP.originGame = p.originGame;
    newP.games = p.games;
    newP.customAvatar = p.customAvatar;
    if (profiles::Save(newP)) {
        if (p.customAvatar) {
            CopyFileW(profiles::AvatarPath(p.id).c_str(), profiles::AvatarPath(newP.id).c_str(), FALSE);
        }
        Apply(newP, files);
        MarkSyncedAfterApply(newP, remoteHash);
        ReFixLog("[Sync] Conflict resolved keeping both: created profile %s (%s)", ToUtf8(newP.id).c_str(), ToUtf8(newP.name).c_str());
        App::Get().Post([] { App::Get().ReloadProfiles(); });
    }
}

void SyncAllProfilesFromCloud() {
    auto target = Target();
    if (!target || !target->Ready()) return;

    const std::vector<std::wstring> files = target->ListFiles();
    if (files.empty()) return;

    std::vector<Profile> allProfiles = profiles::LoadAll();
    const std::wstring curGame = profiles::GameKey();
    const std::wstring curGameName = profiles::DefaultGameName();
    const std::wstring stateIni = StateIni();

    bool anyAdded = false;
    bool activeUpdated = false;
    std::wstring activeIdToRestart;

    for (const std::wstring& file : files) {
        if (GetPrivateProfileIntW(L"Deleted", file.c_str(), 0, stateIni.c_str()) != 0) continue;

        uint64_t fileSteamId = 0;
        std::wstring fileLocalId;
        if (file.rfind(L"steam_", 0) == 0 && file.size() > 11) {
            fileSteamId = _wcstoui64(file.substr(6, file.size() - 11).c_str(), nullptr, 10);
        } else if (file.rfind(L"local_", 0) == 0 && file.size() > 11) {
            fileLocalId = file.substr(6, file.size() - 11);
        }

        if (fileSteamId != 0 && GetPrivateProfileIntW(L"Deleted", (L"steam_" + std::to_wstring(fileSteamId)).c_str(), 0, stateIni.c_str()) != 0) {
            continue;
        }
        if (!fileLocalId.empty() && GetPrivateProfileIntW(L"Deleted", fileLocalId.c_str(), 0, stateIni.c_str()) != 0) {
            continue;
        }

        Profile* match = nullptr;
        for (auto& p : allProfiles) {
            if (fileSteamId != 0 && p.steamId == fileSteamId) {
                match = &p;
                break;
            }
            if (!fileLocalId.empty() && _wcsicmp(p.id.c_str(), fileLocalId.c_str()) == 0) {
                match = &p;
                break;
            }
        }

        std::vector<uint8_t> pkg;
        if (!target->Read(file, pkg)) continue;

        FileList pkgFiles;
        uint64_t pkgSteamId = 0;
        uint8_t remoteHash[32] = {};
        if (!Unpack(pkg, pkgFiles, &pkgSteamId, remoteHash)) continue;

        if (match) {
            bool changed = profiles::AddGame(*match, curGame, curGameName, false);
            if (changed) profiles::Save(*match);

            const std::vector<Entry> localEntries = Collect(*match);
            uint8_t localHash[32];
            StateHash(localEntries, localHash);
            const std::string lastLocal = LastHash(*match);
            const std::string lastRemote = LastRemoteHash(*match);
            const std::string localHex = ToUtf8(Hex(localHash, 32));
            const std::string remoteHex = ToUtf8(Hex(remoteHash, 32));

            if (remoteHex == localHex || remoteHex == lastRemote) {
                if (remoteHex == localHex && lastLocal != localHex) {
                    SetSyncHashes(*match, localHash, remoteHash);
                }
                continue;
            }

            if (!localEntries.empty() && !lastLocal.empty() && lastLocal != localHex) {
                std::lock_guard<std::mutex> lock(g_conflictMu);
                if (!g_conflict.active) {
                    g_conflict.active = true;
                    g_conflict.profileId = match->id;
                    g_conflict.profileName = match->name;
                    g_conflict.steamId = match->steamId;
                    g_conflict.localFiles = localEntries.size();
                    g_conflict.cloudFiles = pkgFiles.size();
                    g_conflictProfile = *match;
                    g_conflictCloudFiles = std::move(pkgFiles);
                    memcpy(g_conflictRemoteHash, remoteHash, 32);
                    memcpy(g_conflictLocalHash, localHash, 32);
                    std::string rTag;
                    if (target->RemoteTag(file, rTag) && !rTag.empty()) g_conflictRemoteTag = ToUtf8(target->Name()) + "|" + rTag;
                    else g_conflictRemoteTag.clear();
                    SetState(State::Pending, "Sync conflict detected");
                    ReFixLog("[Sync] Cloud sync conflict for existing profile %s", ToUtf8(match->name).c_str());
                }
            } else {
                Backup(*match, localEntries, localHash);
                if (Apply(*match, pkgFiles)) {
                    std::string rTag;
                    if (target->RemoteTag(file, rTag) && !rTag.empty()) rTag = ToUtf8(target->Name()) + "|" + rTag;
                    MarkSyncedAfterApply(*match, remoteHash, rTag);
                    ReFixLog("[Sync] Installed newer cloud saves for %s (%zu files)", ToUtf8(match->name).c_str(), pkgFiles.size());
                    if (g_profile && g_profile->id == match->id) {
                        activeUpdated = true;
                        activeIdToRestart = match->id;
                    }
                }
            }
        } else {
            // New un-matched cloud profile found: will prompt via modal
            anyAdded = true;
        }
    }

    if (anyAdded) {
        CheckCloudForProfiles();
    }

    if (activeUpdated && !activeIdToRestart.empty()) {
        App::Get().RestartWithProfile(activeIdToRestart, false);
    }
}

static std::mutex g_cloudImportMu;
static bool g_cloudImportActive = false;
static std::vector<CloudProfileItem> g_cloudImportItems;
static std::atomic<bool> g_checkingCloud{ false };

bool HasCloudImportModal() {
    std::lock_guard<std::mutex> lock(g_cloudImportMu);
    return g_cloudImportActive && !g_cloudImportItems.empty();
}

std::vector<CloudProfileItem>& GetCloudImportItems() {
    return g_cloudImportItems;
}

void DismissCloudImport() {
    std::lock_guard<std::mutex> lock(g_cloudImportMu);
    g_cloudImportActive = false;
    g_cloudImportItems.clear();
}

void CheckCloudForProfiles() {
    if (HasCloudImportModal() || g_checkingCloud.exchange(true)) return;
    std::thread([] {
        struct Guard {
            ~Guard() { g_checkingCloud = false; }
        } guard;
        auto target = Target();
        if (!target || !target->Ready()) return;

        const std::vector<std::wstring> files = target->ListFiles();
        if (files.empty()) return;

        std::vector<Profile> allProfiles = profiles::LoadAll();
        const std::wstring curGame = profiles::GameKey();
        const std::wstring curGameName = profiles::DefaultGameName();
        const std::wstring stateIni = StateIni();

        std::vector<CloudProfileItem> candidates;

        for (const std::wstring& file : files) {
            if (GetPrivateProfileIntW(L"Deleted", file.c_str(), 0, stateIni.c_str()) != 0) continue;

            uint64_t fileSteamId = 0;
            std::wstring fileLocalId;
            if (file.rfind(L"steam_", 0) == 0 && file.size() > 11) {
                fileSteamId = _wcstoui64(file.substr(6, file.size() - 11).c_str(), nullptr, 10);
            } else if (file.rfind(L"local_", 0) == 0 && file.size() > 11) {
                fileLocalId = file.substr(6, file.size() - 11);
            }

            std::vector<uint8_t> pkg;
            if (!target->Read(file, pkg)) continue;

            FileList pkgFiles;
            uint64_t pkgSteamId = 0;
            uint8_t remoteHash[32] = {};
            if (!Unpack(pkg, pkgFiles, &pkgSteamId, remoteHash)) continue;

            CloudProfileItem item;
            item.file = file;
            item.steamId = fileSteamId != 0 ? fileSteamId : pkgSteamId;
            memcpy(item.remoteHash, remoteHash, 32);
            std::string rawTag;
            if (target->RemoteTag(file, rawTag) && !rawTag.empty()) {
                item.remoteTag = ToUtf8(target->Name()) + "|" + rawTag;
            }
            item.selected = true; // All selected by default

            // Parse Meta info
            for (const auto& [n, data] : pkgFiles) {
                if (n == "Meta/name.txt" && !data.empty()) {
                    item.name = FromUtf8(std::string(data.begin(), data.end()));
                } else if (n == "Meta/origin_game.txt" && !data.empty()) {
                    item.originGameKey = FromUtf8(std::string(data.begin(), data.end()));
                } else if (n == "Meta/games.txt" && !data.empty()) {
                    std::string str(data.begin(), data.end());
                    size_t pos = 0;
                    while (pos < str.size()) {
                        size_t next = str.find('\n', pos);
                        if (next == std::string::npos) next = str.size();
                        std::string line = str.substr(pos, next - pos);
                        pos = next + 1;
                        size_t eq = line.find('=');
                        if (eq != std::string::npos) {
                            GameUse g{ FromUtf8(line.substr(0, eq)), FromUtf8(line.substr(eq + 1)) };
                            item.games.push_back(g);
                        }
                    }
                } else if (n == "Meta/stock_avatar.txt" && !data.empty()) {
                    item.stockAvatar = atoi(std::string(data.begin(), data.end()).c_str());
                } else if (n == "Meta/playtime.txt" && !data.empty()) {
                    item.playtimeSeconds = _strtoui64(std::string(data.begin(), data.end()).c_str(), nullptr, 10);
                } else if (n == "Meta/saved_at.txt" && !data.empty()) {
                    item.cloudSavedAt = _strtoui64(std::string(data.begin(), data.end()).c_str(), nullptr, 10);
                } else if (n == "Profile/avatar.png" && !data.empty()) {
                    item.customAvatar = true;
                    item.avatarBytes = data;
                }
            }
            if (item.cloudSavedAt == 0) {
                item.cloudSavedAt = target->ModifiedTime(file);
            }

            if (item.name.empty()) {
                item.name = file;
            }

            // Check if this profile already exists locally (by SteamID, local ID, or profile name)
            const Profile* match = nullptr;
            for (const auto& p : allProfiles) {
                if (item.steamId != 0 && p.steamId == item.steamId) {
                    match = &p;
                    break;
                }
                if (!fileLocalId.empty() && _wcsicmp(p.id.c_str(), fileLocalId.c_str()) == 0) {
                    match = &p;
                    break;
                }
                if (!p.guest && !item.name.empty() && _wcsicmp(p.name.c_str(), item.name.c_str()) == 0) {
                    match = &p;
                    break;
                }
            }

            if (match) {
                const std::vector<Entry> localFiles = Collect(*match);
                uint8_t localHash[32] = {};
                StateHash(localFiles, localHash);
                const std::string localHex = ToUtf8(Hex(localHash, 32));
                const std::string remoteHex = ToUtf8(Hex(remoteHash, 32));
                if ((memcmp(localHash, remoteHash, 32) == 0 || remoteHex == LastRemoteHash(*match)) &&
                    LastHash(*match) == localHex) {
                    continue; // Identical and already synced
                }
                item.hasLocalMatch = true;
                item.localProfileId = match->id;
                uint64_t localTime = match->lastUsed;
                for (const Entry& e : localFiles) {
                    if (e.mtime > localTime) localTime = e.mtime;
                }
                item.localSavedAt = localTime;
                item.action = (item.cloudSavedAt >= item.localSavedAt) ? CloudImportAction::UseCloud : CloudImportAction::UseLocal;
            } else {
                item.action = CloudImportAction::ImportNew;
            }

            auto cleanGameDisplay = [&](std::wstring raw) -> std::wstring {
                if (raw.empty()) return {};
                auto stripPrefix = [&](const wchar_t* pfx) {
                    const size_t n = wcslen(pfx);
                    if (raw.size() > n && _wcsnicmp(raw.c_str(), pfx, n) == 0) {
                        raw = raw.substr(n);
                        return true;
                    }
                    return false;
                };
                if (stripPrefix(L"Unity_Roaming_")) {
                    if (raw.size() > 6 && _wcsicmp(raw.c_str() + raw.size() - 6, L"_Saves") == 0) raw.resize(raw.size() - 6);
                    else if (raw.size() > 11 && _wcsicmp(raw.c_str() + raw.size() - 11, L"_SavesLocal") == 0) raw.resize(raw.size() - 11);
                } else if (stripPrefix(L"Unity_")) {
                    const size_t sep = raw.find(L'_');
                    if (sep != std::wstring::npos && sep + 1 < raw.size()) {
                        raw = raw.substr(sep + 1);
                    }
                } else {
                    stripPrefix(L"UE_") || stripPrefix(L"SavedGames_") || stripPrefix(L"MyGames_") ||
                    stripPrefix(L"LocalLow_") || stripPrefix(L"Local_") || stripPrefix(L"Roaming_");
                }
                for (wchar_t& c : raw) if (c == L'_') c = L' ';
                while (!raw.empty() && raw.back() == L' ') raw.pop_back();
                return raw;
            };

            // Fallback for older packages
            if (item.originGameKey.empty() && !item.games.empty()) {
                item.originGameKey = item.games.front().key;
                item.originGameName = item.games.front().name;
            }

            if (item.originGameName.empty()) {
                for (const auto& g : item.games) {
                    if (g.key == item.originGameKey) {
                        item.originGameName = g.name;
                        break;
                    }
                }
            }

            if (item.originGameKey.empty()) {
                for (const auto& [n, _] : pkgFiles) {
                    if (n.rfind("Saves/", 0) == 0 && n.size() > 6) {
                        size_t nextSlash = n.find('/', 6);
                        std::string label = (nextSlash != std::string::npos) ? n.substr(6, nextSlash - 6) : n.substr(6);
                        item.originGameName = FromUtf8(label);
                        item.originGameKey = FromUtf8(label);
                        break;
                    } else if (n.rfind("Profile/Cloud/", 0) == 0 && n.size() > 14) {
                        size_t nextSlash = n.find('/', 14);
                        std::string appIdStr = (nextSlash != std::string::npos) ? n.substr(14, nextSlash - 14) : n.substr(14);
                        if (appIdStr == "730") {
                            item.originGameKey = L"csgo";
                            item.originGameName = L"Counter-Strike: Global Offensive";
                        } else if (appIdStr == "1966720") {
                            item.originGameKey = L"lethalcompany";
                            item.originGameName = L"Lethal Company";
                        } else {
                            item.originGameKey = FromUtf8(appIdStr);
                            item.originGameName = FromUtf8(appIdStr);
                        }
                        break;
                    }
                }
            }

            if (item.originGameName.empty()) {
                item.originGameName = i18n::Spanish() ? L"Juego desconocido" : L"Unknown Game";
            }

            // Current game AppIDs and save labels for accurate per-game breakdown
            std::set<uint32_t> curAppIds;
            if (uint32_t aid = UnrealSteamEmu::GetAppID()) curAppIds.insert(aid);
            for (uint32_t aid : refix::mods::GameAppIds()) if (aid != 0) curAppIds.insert(aid);
            std::set<std::string> curSaveLabels;
            for (const saves::Location& loc : saves::DetectLocations()) curSaveLabels.insert(ToUtf8(loc.label));

            // Compute total weight, save counts, achievements, and stats/inventory counts
            item.totalBytes = pkg.size();
            for (const auto& [n, data] : pkgFiles) {
                item.totalBytes += data.size();
                if (n.rfind("Meta/", 0) == 0 || n == "Profile/avatar.png") continue;
                if (n.rfind("Saves/", 0) == 0 && n.size() > 6) {
                    ++item.totalSaves;
                    const size_t sl = n.find('/', 6);
                    const std::string lbl = (sl != std::string::npos) ? n.substr(6, sl - 6) : n.substr(6);
                    bool isCur = curSaveLabels.count(lbl) > 0;
                    if (!isCur) {
                        std::string lowerLbl = lbl, lowerKey = ToUtf8(curGame);
                        for (char& c : lowerLbl) c = (char)tolower((unsigned char)c);
                        for (char& c : lowerKey) c = (char)tolower((unsigned char)c);
                        if (!lowerKey.empty() && lowerLbl.find(lowerKey) != std::string::npos) isCur = true;
                    }
                    if (isCur) ++item.currentGameSaves;
                } else if (n.rfind("Profile/Cloud/", 0) == 0 && n.size() > 14) {
                    ++item.totalSaves;
                    const size_t sl = n.find('/', 14);
                    const uint32_t aid = (uint32_t)strtoul(n.substr(14, sl == std::string::npos ? std::string::npos : sl - 14).c_str(), nullptr, 10);
                    if (aid != 0 && curAppIds.count(aid)) ++item.currentGameSaves;
                } else if (n.rfind("Profile/Stats/", 0) == 0 && n.size() > 14) {
                    const size_t sl = n.find('/', 14);
                    const uint32_t aid = (uint32_t)strtoul(n.substr(14, sl == std::string::npos ? std::string::npos : sl - 14).c_str(), nullptr, 10);
                    const bool isCurApp = (aid != 0 && curAppIds.count(aid) > 0);
                    std::string text(data.begin(), data.end()), line, sec;
                    size_t pos = 0;
                    while (pos < text.size()) {
                        size_t nl = text.find('\n', pos);
                        if (nl == std::string::npos) nl = text.size();
                        line = text.substr(pos, nl - pos);
                        pos = nl + 1;
                        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
                        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
                        if (line[0] == '[') {
                            size_t rb = line.find(']');
                            sec = (rb != std::string::npos) ? line.substr(1, rb - 1) : "";
                            continue;
                        }
                        const size_t eq = line.find('=');
                        if (eq == std::string::npos || eq == 0) continue;
                        if (sec == "Achievements") {
                            std::string v = line.substr(eq + 1);
                            if (!v.empty() && v != "0") {
                                ++item.totalAchievements;
                                if (isCurApp) ++item.currentGameAchievements;
                            }
                        } else if (sec == "IntStats" || sec == "FloatStats" || sec == "AchievementProgress") {
                            ++item.otherDataCount;
                        }
                    }
                } else if (n.rfind("Profile/Inventory/", 0) == 0 || n.rfind("Profile/Data/", 0) == 0) {
                    ++item.otherDataCount;
                }
            }

            // Check if profile has data for current game
            item.hasCurrentGame = (item.currentGameSaves > 0 || item.currentGameAchievements > 0);
            std::wstring currentGameDisplay = curGameName;
            if (!item.originGameKey.empty() && _wcsicmp(item.originGameKey.c_str(), curGame.c_str()) == 0) {
                item.hasCurrentGame = true;
            }
            for (const auto& g : item.games) {
                if (_wcsicmp(g.key.c_str(), curGame.c_str()) == 0) {
                    item.hasCurrentGame = true;
                    if (!g.name.empty() && _wcsicmp(g.name.c_str(), curGame.c_str()) != 0) {
                        currentGameDisplay = g.name;
                    }
                    break;
                }
            }
            if (!item.hasCurrentGame) {
                for (const auto& [n, _] : pkgFiles) {
                    if (n.find(ToUtf8(curGame)) != std::string::npos) {
                        item.hasCurrentGame = true;
                        break;
                    }
                }
            }
            if (item.playtimeSeconds == 0 && match && match->playtimeSeconds > 0) {
                item.playtimeSeconds = match->playtimeSeconds;
            }

            std::set<std::wstring> distinctGames;
            for (const auto& g : item.games) {
                std::wstring k = g.key;
                for (wchar_t& c : k) c = (wchar_t)towlower(c);
                if (!k.empty()) distinctGames.insert(k);
            }
            if (item.hasCurrentGame && !curGame.empty()) {
                std::wstring k = curGame;
                for (wchar_t& c : k) c = (wchar_t)towlower(c);
                distinctGames.insert(k);
            }
            item.extraGamesCount = distinctGames.size() > 1 ? (int)distinctGames.size() - 1 : 0;

            const std::wstring primaryName = item.hasCurrentGame
                ? cleanGameDisplay(currentGameDisplay.empty() ? curGameName : currentGameDisplay)
                : cleanGameDisplay(item.originGameName);
            std::string disp = ToUtf8(primaryName.empty() ? item.originGameName : primaryName);
            if (item.extraGamesCount > 0) {
                disp += " +" + std::to_string(item.extraGamesCount);
            }
            item.originDisplay = disp;
            item.pkgFiles = std::move(pkgFiles);

            candidates.push_back(std::move(item));
        }

        if (candidates.empty()) return;

        // Sort: current game profiles first, then largest weight (bytes) descending, then name
        std::stable_sort(candidates.begin(), candidates.end(), [](const CloudProfileItem& a, const CloudProfileItem& b) {
            if (a.hasCurrentGame != b.hasCurrentGame) return a.hasCurrentGame > b.hasCurrentGame;
            if (a.totalBytes != b.totalBytes) return a.totalBytes > b.totalBytes;
            return a.name < b.name;
        });

        {
            std::lock_guard<std::mutex> lock(g_cloudImportMu);
            if (g_cloudImportActive) return;
            g_cloudImportItems = std::move(candidates);
            g_cloudImportActive = true;
        }
        App::Get().Post([] {});
    }).detach();
}

void ConfirmCloudImport(const std::vector<CloudProfileItem>& items) {
    const std::vector<CloudProfileItem> selected = items;
    std::vector<Profile> allProfiles = profiles::LoadAll();
    const std::wstring curGame = profiles::GameKey();
    const std::wstring curGameName = profiles::DefaultGameName();

    struct UploadJob {
        std::wstring file;
        Profile profile;
    };
    std::vector<UploadJob> uploadJobs;

    for (const auto& item : selected) {
        if (!item.selected || item.action == CloudImportAction::Skip) continue;

        if (item.action == CloudImportAction::UseLocal && item.hasLocalMatch) {
            auto it = std::find_if(allProfiles.begin(), allProfiles.end(), [&](const Profile& p) {
                return p.id == item.localProfileId;
            });
            if (it != allProfiles.end()) {
                uploadJobs.push_back({ item.file, *it });
            }
            continue;
        }

        if (item.action == CloudImportAction::UseCloud && item.hasLocalMatch) {
            auto it = std::find_if(allProfiles.begin(), allProfiles.end(), [&](const Profile& p) {
                return p.id == item.localProfileId;
            });
            if (it != allProfiles.end()) {
                Profile updated = *it;
                updated.name = item.name;
                updated.stockAvatar = item.stockAvatar;
                updated.customAvatar = item.customAvatar;
                for (const auto& g : item.games) {
                    profiles::AddGame(updated, g.key, g.name, false);
                }
                if (item.hasCurrentGame) {
                    profiles::AddGame(updated, curGame, curGameName, false);
                }
                if (profiles::Save(updated)) {
                    Apply(updated, item.pkgFiles);
                    MarkSyncedAfterApply(updated, item.remoteHash, item.remoteTag);
                    *it = updated;
                    ReFixLog("[Sync] Cloud overwrote local profile %s (%s)",
                             ToUtf8(updated.id).c_str(), ToUtf8(updated.name).c_str());
                    if (g_profile && g_profile->id == updated.id) {
                        ach::ReloadFromDisk();
                        App::Get().Post([id = updated.id] { App::Get().RestartWithProfile(id, false); });
                    }
                }
                continue;
            }
        }

        // ImportNew or KeepBoth
        const std::wstring newName = (item.action == CloudImportAction::KeepBoth && item.hasLocalMatch)
                                         ? (item.name + L" (Cloud)")
                                         : item.name;
        Profile newP = profiles::MakeNew(newName, allProfiles);
        if (item.steamId != 0 && !(item.action == CloudImportAction::KeepBoth && item.hasLocalMatch)) {
            newP.steamId = item.steamId;
        }
        newP.stockAvatar = item.stockAvatar;
        newP.customAvatar = item.customAvatar;
        newP.originGame = item.originGameKey.empty() ? curGame : item.originGameKey;

        if (!item.games.empty()) {
            newP.games = item.games;
        } else {
            newP.games = { { newP.originGame, item.originGameName } };
        }

        if (item.hasCurrentGame) {
            profiles::AddGame(newP, curGame, curGameName, false);
        }

        if (profiles::Save(newP)) {
            Apply(newP, item.pkgFiles);
            MarkSyncedAfterApply(newP, item.remoteHash, item.remoteTag);
            allProfiles.push_back(newP);
            ReFixLog("[Sync] Cloud imported profile %s (%s, origin: %s, hasCurrent: %d)",
                     ToUtf8(newP.id).c_str(), ToUtf8(newP.name).c_str(), ToUtf8(newP.originGame).c_str(), (int)item.hasCurrentGame);
        }
    }

    if (!uploadJobs.empty()) {
        g_manualPushPending = true;
        std::thread([jobs = std::move(uploadJobs)] {
            struct Guard {
                ~Guard() { g_manualPushPending = false; }
            } guard;
            auto target = Target();
            if (!target || !target->Ready()) return;
            for (const auto& job : jobs) {
                const std::vector<Entry> local = Collect(job.profile);
                uint8_t localHash[32] = {};
                StateHash(local, localHash);
                std::vector<uint8_t> pkg;
                if (BuildPackage(job.profile, local, localHash, pkg) && target->Write(job.file, pkg)) {
                    SetSyncHashes(job.profile, localHash, localHash);
                    std::string rTag;
                    if (target->RemoteTag(job.file, rTag) && !rTag.empty()) {
                        SetRemoteTag(job.profile, ToUtf8(target->Name()) + "|" + rTag);
                    }
                    ReFixLog("[Sync] Local overwrote cloud profile %s (%s)",
                             ToUtf8(job.profile.id).c_str(), ToUtf8(job.file).c_str());
                }
            }
        }).detach();
    }

    DismissCloudImport();
    App::Get().Post([] { App::Get().ReloadProfiles(); });
}

} // namespace refix::overlay::sync
