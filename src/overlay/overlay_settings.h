// Player settings of the overlay, per PC, in %APPDATA%\ReFix\profiles.ini [Overlay] (next to the language).
// ReFix.ini [Overlay] keeps the defaults a game folder ships with; a value set here wins over it.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace refix::overlay::settings {

enum class GameOpenBehavior {
    KeepActive = 0,   // Game continues ticking and audio keeps playing
    Mute = 1,         // Game continues ticking, but audio is muted while overlay is open
    Pause = 2         // Game pauses and audio is muted while overlay is open
};

enum class DisplayMode {
    FullscreenWindowed = 0,   // Fullscreen borderless window (default)
    Fullscreen = 1,           // Exclusive fullscreen
    Windowed = 2              // Windowed mode with selectable resolution
};

enum class OverlayShortcut {
    ShiftTab = 0,     // Shift + Tab (default)
    CtrlTab = 1,      // Ctrl + Tab
    ShiftF1 = 2,      // Shift + F1
    ShiftF2 = 3,      // Shift + F2
    F11 = 4,          // F11
    F12 = 5,          // F12
    Home = 6,         // Home
    Insert = 7,       // Insert
};

struct ShortcutInfo {
    OverlayShortcut id;
    uint32_t mod;     // MOD_SHIFT, MOD_CONTROL, 0
    uint32_t vk;      // VK_TAB, VK_F1, etc.
    const char* label;
    const char* labelEs;
};

struct Values {
    bool achievementToasts = true;   // corner notification when an achievement unlocks
    bool progressToasts = true;      // ... and when one makes progress
    bool achievementSounds = true;   // chime of those notifications
    bool inviteToasts = true;        // invitations (the game still receives them)
    bool welcomeToast = true;        // "Press Shift+Tab" when the game starts
    bool otherToasts = true;         // other notices that arrive while playing (overlay closed)
    bool uiSounds = true;            // clicks, open/close
    bool blockReflex = true;         // NVIDIA Reflex never runs in the game (from the next start)
    GameOpenBehavior openBehavior = GameOpenBehavior::KeepActive;
    OverlayShortcut shortcut = OverlayShortcut::ShiftTab;
    DisplayMode displayMode = DisplayMode::FullscreenWindowed;
    int windowWidth = 1920;
    int windowHeight = 1080;
    bool perGameDisplay = false;     // keep display mode/resolution only for the current game
};

struct ResolutionOption {
    int width = 0;
    int height = 0;
    std::string label;
};

// Host thread (UI). Loaded on first use.
const Values& Get();
void Set(const Values& v);           // saves at once
bool HasSavedDisplay();              // true once the user explicitly saved/customized display settings
void MarkDisplayCustomized(bool customized);
bool ApplyDisplayToWindow(HWND gameHwnd, const Values& v); // applies DisplayMode + WindowWidth/Height at runtime
bool WindowMatchesDisplay(HWND gameHwnd, const Values& v); // checks if gameHwnd style/rect already matches v
std::vector<ResolutionOption> GetSupportedResolutions(HWND gameHwnd);

ShortcutInfo GetShortcutInfo(OverlayShortcut sc);
const char* GetShortcutName(OverlayShortcut sc);
const char* GetShortcutNameEs(OverlayShortcut sc);

// Steam online services (ReFix Online). Auto = the player never chose: ReFix.ini decides.
enum class SteamOnline { Auto, On, Off };
SteamOnline SteamOnlineChoice();
void SetSteamOnline(bool on);

// Bridge AppID used when connecting to Steam Online Services (default: Spacewar 480).
enum class BridgeMode {
    Spacewar = 0,
    CurrentGame = 1,
    Custom = 2,
};

struct BridgeSettings {
    BridgeMode mode = BridgeMode::Spacewar;
    uint32_t customAppId = 0;
    bool officialServers = false;
};

BridgeSettings GetBridgeSettings();
void SetBridgeSettings(const BridgeSettings& b);
uint32_t EffectiveBridgeAppId(uint32_t currentGameAppId);
bool IsOfficialServersActive();
void SetOfficialServersActive(bool active);

// DllMain-safe read of the Reflex setting (no shell APIs): the player's choice, else `iniDefault`.
bool BlockReflexEarly(bool iniDefault);

// Switch overlay to Steam: writes Enabled=false, BlockSteamOverlay=false to ReFix.ini
bool ReplaceWithSteamOverlay();

// Disable overlay completely: writes Enabled=false, BlockSteamOverlay=true to ReFix.ini
bool DisableOverlayCompletely();

} // namespace refix::overlay::settings
