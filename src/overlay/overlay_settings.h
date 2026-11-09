// Player settings of the overlay, per PC, in %APPDATA%\ReFix\profiles.ini [Overlay] (next to the language).
// ReFix.ini [Overlay] keeps the defaults a game folder ships with; a value set here wins over it.
#pragma once

namespace refix::overlay::settings {

struct Values {
    bool achievementToasts = true;   // corner notification when an achievement unlocks
    bool progressToasts = true;      // ... and when one makes progress
    bool achievementSounds = true;   // chime of those notifications
    bool inviteToasts = true;        // invitations (the game still receives them)
    bool welcomeToast = true;        // "Press Shift+Tab" when the game starts
    bool otherToasts = true;         // other notices that arrive while playing (overlay closed)
    bool uiSounds = true;            // clicks, open/close
    bool blockReflex = true;         // NVIDIA Reflex paused while the overlay is open
};

// Host thread (UI). Loaded on first use.
const Values& Get();
void Set(const Values& v);           // saves at once

// Steam online services (ReFix Online). Auto = the player never chose: ReFix.ini decides.
enum class SteamOnline { Auto, On, Off };
SteamOnline SteamOnlineChoice();
void SetSteamOnline(bool on);

// DllMain-safe read of the Reflex setting (no shell APIs): the player's choice, else `iniDefault`.
bool BlockReflexEarly(bool iniDefault);

} // namespace refix::overlay::settings
