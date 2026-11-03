// Per-profile save folders without hooks.
// The game's known save folder on disk always belongs to one profile (its "owner", recorded in
// %APPDATA%\ReFix LAN\saves.ini). When a different profile starts the game, the owner's files are moved
// into its stash (Profiles\<id>\Saves\<label>) and the new profile's stash is moved back in, before the
// game reads anything. Covered folders:
//   Unreal: %LOCALAPPDATA%\<Project>\Saved\SaveGames      (<Project>\Binaries\Win64\<exe>)
//   Unity:  %USERPROFILE%\AppData\LocalLow\<Company>\<Product>   (from <exe>_Data\app.info)
// Data found there before any profile owned it goes to the profile marked nativeSaves, or to
// %APPDATA%\ReFix LAN\Saves\_pc, so nothing is ever deleted.
#pragma once
#include "profile_store.h"

#include <string>
#include <vector>

namespace refix::overlay::saves {

struct Location {
    std::wstring label;                 // stash folder name, e.g. "UE_Pal" or "Unity_Company_Product"
    std::wstring live;                  // the folder the game uses
    std::vector<std::wstring> keepLive; // entries that stay in place (open logs)
};

std::vector<Location> DetectLocations();

// Makes `active` the owner of every detected location. Skipped (and logged) while another process of the
// same executable is running, since its files would be moved from under it.
void SwitchTo(const Profile& active, const std::vector<Profile>& all);

// Another process of this game's executable is running on this PC.
bool OtherInstanceRunning();

// A deleted profile no longer owns any folder; whatever is in place is treated as data from before profiles.
void ForgetOwner(const std::wstring& profileId);

} // namespace refix::overlay::saves
