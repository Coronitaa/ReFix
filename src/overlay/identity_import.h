// Reads the identity (name, SteamID, picture) another ReFix game uses, for "Import from a ReFix game" in the
// profile editor. Read-only: nothing in the other game's folder is changed.
#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace refix::overlay::profiles {

struct ImportedIdentity {
    std::wstring name;
    uint64_t steamId = 0;
    std::wstring avatarPath;     // picture file, empty when the game has none
    std::wstring gameLabel;      // folder name, for the messages
    bool machineId = false;      // the game derives its SteamID from this PC (IdentityMode=auto)
};

// `folder`: the game folder (any level above its ReFix.ini), or a ReFix LAN profile folder (profile.ini).
// `machineSteamId`: what IdentityMode=auto gives on this PC.
std::optional<ImportedIdentity> ReadReFixGame(const std::wstring& folder, uint64_t machineSteamId, std::wstring& err);

} // namespace refix::overlay::profiles
