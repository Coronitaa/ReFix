// Which game a mod is for: the single rule both the mod host and the overlay's mod registry use.
#pragma once

#include "mod_types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace refix::mods {

// 0 and 480 (Spacewar, the emulator's fallback) say nothing about the game.
bool IsKnownAppId(uint32_t appId);

// True when one target matches the running game. A target needs an app_id or an exe_name; with both, both
// must match (the exe alone decides while the AppID is unknown). Builds of one game under different AppIDs are
// listed as separate targets.
// An empty target list matches nothing: a mod has to say which game it is for.
bool MatchesGame(const std::vector<TargetGame>& targets, uint32_t appId, const std::string& exeName);

// File name of the running executable, e.g. "csgo.exe".
std::string CurrentExeName();

} // namespace refix::mods
