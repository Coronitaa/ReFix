#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_settings.h"
#include "overlay_sound.h"
#include "profile_store.h"

#include <windows.h>
#include <string>

namespace refix::overlay::settings {
namespace {

constexpr const wchar_t* kSection = L"Overlay";

struct Key {
    const wchar_t* name;
    bool Values::*field;
};
const Key kKeys[] = {
    { L"AchievementToasts", &Values::achievementToasts },
    { L"ProgressToasts", &Values::progressToasts },
    { L"AchievementSounds", &Values::achievementSounds },
    { L"InviteToasts", &Values::inviteToasts },
    { L"WelcomeToast", &Values::welcomeToast },
    { L"OtherToasts", &Values::otherToasts },
    { L"Sounds", &Values::uiSounds },
    { L"BlockNvidiaReflex", &Values::blockReflex },
};

const ShortcutInfo kShortcuts[] = {
    { OverlayShortcut::ShiftTab, MOD_SHIFT, VK_TAB, "Shift + Tab", "Shift + Tab" },
    { OverlayShortcut::CtrlTab, MOD_CONTROL, VK_TAB, "Ctrl + Tab", "Ctrl + Tab" },
    { OverlayShortcut::ShiftF1, MOD_SHIFT, VK_F1, "Shift + F1", "Shift + F1" },
    { OverlayShortcut::ShiftF2, MOD_SHIFT, VK_F2, "Shift + F2", "Shift + F2" },
    { OverlayShortcut::F11, 0, VK_F11, "F11", "F11" },
    { OverlayShortcut::F12, 0, VK_F12, "F12", "F12" },
    { OverlayShortcut::Home, 0, VK_HOME, "Home", "Inicio (Home)" },
    { OverlayShortcut::Insert, 0, VK_INSERT, "Insert", "Insert (Insertar)" },
};

Values g_values;
bool g_loaded = false;

std::wstring Ini() { return profiles::RootDir() + L"\\profiles.ini"; }

// -1 = not set by the player.
int ReadBool(const std::wstring& ini, const wchar_t* key) {
    wchar_t buf[8] = {};
    GetPrivateProfileStringW(kSection, key, L"", buf, 8, ini.c_str());
    if (!buf[0]) return -1;
    return (buf[0] == L'1' || _wcsicmp(buf, L"true") == 0) ? 1 : 0;
}

int ReadInt(const std::wstring& ini, const wchar_t* key, int def) {
    return (int)GetPrivateProfileIntW(kSection, key, def, ini.c_str());
}

std::string FindReFixIni() {
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    dir = dir.substr(0, dir.find_last_of("\\/") + 1);
    std::string up;
    for (int i = 0; i < 5; ++i, up += "..\\") {
        const std::string cand = dir + up + "ReFix.ini";
        if (GetFileAttributesA(cand.c_str()) != INVALID_FILE_ATTRIBUTES) return cand;
    }
    return dir + "ReFix.ini";
}

} // namespace

const Values& Get() {
    if (!g_loaded) {
        g_loaded = true;
        const std::wstring ini = Ini();
        for (const Key& k : kKeys) {
            const int v = ReadBool(ini, k.name);
            if (v >= 0) g_values.*k.field = v == 1;
        }
        int beh = ReadInt(ini, L"OpenBehavior", (int)GameOpenBehavior::KeepActive);
        if (beh >= 0 && beh <= 2) g_values.openBehavior = static_cast<GameOpenBehavior>(beh);
        int sc = ReadInt(ini, L"Shortcut", (int)OverlayShortcut::ShiftTab);
        if (sc >= 0 && sc <= (int)OverlayShortcut::Insert) g_values.shortcut = static_cast<OverlayShortcut>(sc);
    }
    return g_values;
}

void Set(const Values& v) {
    Get();
    g_values = v;
    CreateDirectoryW(profiles::RootDir().c_str(), nullptr);
    const std::wstring ini = Ini();
    for (const Key& k : kKeys) WritePrivateProfileStringW(kSection, k.name, (v.*k.field) ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(kSection, L"OpenBehavior", std::to_wstring((int)v.openBehavior).c_str(), ini.c_str());
    WritePrivateProfileStringW(kSection, L"Shortcut", std::to_wstring((int)v.shortcut).c_str(), ini.c_str());
    sound::SetEnabled(v.uiSounds);
}

ShortcutInfo GetShortcutInfo(OverlayShortcut sc) {
    int idx = static_cast<int>(sc);
    if (idx >= 0 && idx < (int)(sizeof(kShortcuts) / sizeof(kShortcuts[0]))) {
        return kShortcuts[idx];
    }
    return kShortcuts[0];
}

const char* GetShortcutName(OverlayShortcut sc) {
    return GetShortcutInfo(sc).label;
}

const char* GetShortcutNameEs(OverlayShortcut sc) {
    return GetShortcutInfo(sc).labelEs;
}

SteamOnline SteamOnlineChoice() {
    const int v = ReadBool(Ini(), L"SteamOnline");
    return v < 0 ? SteamOnline::Auto : v == 1 ? SteamOnline::On : SteamOnline::Off;
}

void SetSteamOnline(bool on) {
    CreateDirectoryW(profiles::RootDir().c_str(), nullptr);
    WritePrivateProfileStringW(kSection, L"SteamOnline", on ? L"1" : L"0", Ini().c_str());
}

bool BlockReflexEarly(bool iniDefault) {
    wchar_t appdata[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) return iniDefault;
    const int v = ReadBool(std::wstring(appdata) + L"\\ReFix\\profiles.ini", L"BlockNvidiaReflex");
    return v < 0 ? iniDefault : v == 1;
}

bool ReplaceWithSteamOverlay() {
    const std::string ini = FindReFixIni();
    WritePrivateProfileStringA("Overlay", "Enabled", "false", ini.c_str());
    WritePrivateProfileStringA("Overlay", "BlockSteamOverlay", "false", ini.c_str());
    return true;
}

} // namespace refix::overlay::settings
