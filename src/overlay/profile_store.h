// ReFix LAN player profiles.
// Profiles live in %APPDATA%\ReFix\Profiles\<id>\ (never in the game folder), so a patched game copied
// to another PC carries no identity: each PC creates its own profiles, each with its own SteamID.
// Every profile remembers the games it was used in: a game lists its own profiles, and the others can be
// brought in with "Import from another game" (same profile: name, picture and SteamID).
#pragma once
#include <windows.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace refix::overlay {

constexpr int kMaxProfiles = 10;          // per game
constexpr int kMaxStoredProfiles = 100;   // on the PC, every game together
constexpr int kStockAvatarCount = 12;
constexpr size_t kMaxNameChars = 32;

struct GameUse {
    std::wstring key;                // ExeKey() of the game
    std::wstring name;               // shown in "Import from another game" (window title once known)
};

struct Profile {
    std::wstring id;                 // 8 hex chars, also the folder name
    std::wstring name;
    uint64_t steamId = 0;
    int stockAvatar = 0;             // used when customAvatar is false
    bool customAvatar = false;       // avatar.png (184x184) in the profile folder
    bool nativeSaves = false;        // plays with the PC's real save folders (data from before profiles)
    bool importedFromSteam = false;
    bool guest = false;              // "play as a guest": never saved, temporary Cloud folder, no lock
    uint64_t lastUsed = 0;           // FILETIME ticks
    std::vector<uint64_t> favorites; // SteamIDs, in the order they were starred
    std::wstring originGame;         // key of the game it was created in; empty = from before games were
                                     // tracked, shown in every game
    std::vector<GameUse> games;      // every game that used it, the origin first
};

struct SteamSession {
    uint64_t steamId = 0;
    std::wstring name;
    std::wstring avatarPath;         // Steam's cached avatar PNG, empty when Steam has none cached
};

namespace profiles {

std::wstring RootDir();                          // %APPDATA%\ReFix (moved from %APPDATA%\ReFix LAN)
std::wstring ProfileDir(const std::wstring& id);
std::wstring AvatarPath(const std::wstring& id);
std::wstring DataRoot(const std::wstring& id);   // redirected user folders (isolated saves)
std::wstring CloudRoot(const std::wstring& id, uint32_t appId); // emulated Steam Cloud files
std::wstring StatsRoot(const std::wstring& id, uint32_t appId); // emulated Steam UserStats / achievements
std::wstring InventoryRoot(const std::wstring& id, uint32_t appId); // emulated Steam Inventory

std::vector<Profile> LoadAll();                  // most recently used first
std::optional<Profile> Load(const std::wstring& id);
bool Save(const Profile& p);
// Deletes the profile folder (definition, avatar, Cloud files and stashed saves). Refused while any game uses it.
bool Remove(const std::wstring& id);
// New profile with a fresh id and SteamID (not saved yet).
Profile MakeNew(const std::wstring& name, const std::vector<Profile>& existing);
// Guest for this session: standard name, stock avatar, fresh SteamID; Cloud files under %TEMP%.
Profile MakeGuest(const std::wstring& name, const std::vector<Profile>& existing);
std::wstring GuestCloudRoot(uint32_t appId);
std::wstring GuestStatsRoot(uint32_t appId);
std::wstring GuestInventoryRoot(uint32_t appId);
void CleanupGuestFolders();                      // removes the temporary folders of guests no longer running
void Touch(Profile& p);                          // lastUsed = now

// This game: key and default name (file name of the exe; the overlay replaces it by the window title).
std::wstring GameKey();
std::wstring DefaultGameName();
bool UsedIn(const Profile& p, const std::wstring& gameKey);
// Records that `p` is used in this game (does not save). Returns true when something changed.
// `replaceName` false keeps a name already recorded (the exe name must not undo the window title).
bool AddGame(Profile& p, const std::wstring& gameKey, const std::wstring& gameName, bool replaceName = true);
std::vector<Profile> ForGame(const std::vector<Profile>& all, const std::wstring& gameKey);
std::vector<Profile> FromOtherGames(const std::vector<Profile>& all, const std::wstring& gameKey);

std::wstring LastProfileId(const std::wstring& gameKey = GameKey());
void SetLastProfileId(const std::wstring& id, const std::wstring& gameKey = GameKey());

// Locks are per game executable: two instances of one game never share a profile, while the same
// profile can still be used by two different games at once.
bool TryLock(const std::wstring& id);            // held for the rest of the process
bool WaitLock(const std::wstring& id, DWORD timeoutMs);
bool IsLockedByOther(const std::wstring& id, const std::wstring& activeId);
bool IsInUseAnywhere(const std::wstring& id);    // by any game on this PC (this process included)

// Session of the Steam client running on this PC. Empty with `error` set when Steam is closed or no
// account is logged in.
std::optional<SteamSession> ReadSteamSession(std::wstring& error);

} // namespace profiles
} // namespace refix::overlay
