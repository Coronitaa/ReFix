// Names of Steam games by AppID, for the friends list ("Playing Portal 2"). Asked to Steam's public store API
// (no key) in the background; cached in memory and in %APPDATA%\ReFix\AppNames.ini.
#pragma once
#include <cstdint>
#include <string>

namespace refix::overlay::apps {

// UTF-8 name, or "" while it is being looked up (call again on a later frame) or when Steam does not know it.
std::string Name(uint32_t appId);

} // namespace refix::overlay::apps
