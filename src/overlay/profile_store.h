// ReFix LAN player profiles.
// Profiles live in %APPDATA%\ReFix LAN\Profiles\<id>\ (never in the game folder), so a patched game copied
// to another PC carries no identity: each PC creates its own profiles, each with its own SteamID.
#pragma once
#include <windows.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace refix::overlay {

constexpr int kMaxProfiles = 10;
constexpr int kStockAvatarCount = 12;
constexpr size_t kMaxNameChars = 32;

struct Profile {
    std::wstring id;                 // 8 hex chars, also the folder name
    std::wstring name;
    uint64_t steamId = 0;
    int stockAvatar = 0;             // used when customAvatar is false
    bool customAvatar = false;       // avatar.png (184x184) in the profile folder
    bool nativeSaves = false;        // plays with the PC's real save folders (data from before profiles)
    bool importedFromSteam = false;
    uint64_t lastUsed = 0;           // FILETIME ticks
    std::vector<uint64_t> favorites; // SteamIDs, in the order they were starred
};

struct SteamSession {
    uint64_t steamId = 0;
    std::wstring name;
    std::wstring avatarPath;         // Steam's cached avatar PNG, empty when Steam has none cached
};

namespace profiles {

std::wstring RootDir();                          // %APPDATA%\ReFix LAN
std::wstring ProfileDir(const std::wstring& id);
std::wstring AvatarPath(const std::wstring& id);
std::wstring DataRoot(const std::wstring& id);   // redirected user folders (isolated saves)
std::wstring CloudRoot(const std::wstring& id, uint32_t appId); // emulated Steam Cloud files

std::vector<Profile> LoadAll();                  // most recently used first
std::optional<Profile> Load(const std::wstring& id);
bool Save(const Profile& p);
bool Remove(const std::wstring& id);
// New profile with a fresh id and SteamID (not saved yet).
Profile MakeNew(const std::wstring& name, const std::vector<Profile>& existing);
void Touch(Profile& p);                          // lastUsed = now

std::wstring LastProfileId();
void SetLastProfileId(const std::wstring& id);

// Locks are per game executable: two instances of one game never share a profile, while the same
// profile can still be used by two different games at once.
bool TryLock(const std::wstring& id);            // held for the rest of the process
bool WaitLock(const std::wstring& id, DWORD timeoutMs);
bool IsLockedByOther(const std::wstring& id, const std::wstring& activeId);

// Session of the Steam client running on this PC. Empty with `error` set when Steam is closed or no
// account is logged in.
std::optional<SteamSession> ReadSteamSession(std::wstring& error);

} // namespace profiles
} // namespace refix::overlay
