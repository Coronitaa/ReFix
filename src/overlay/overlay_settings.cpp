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

} // namespace

const Values& Get() {
    if (!g_loaded) {
        g_loaded = true;
        const std::wstring ini = Ini();
        for (const Key& k : kKeys) {
            const int v = ReadBool(ini, k.name);
            if (v >= 0) g_values.*k.field = v == 1;
        }
    }
    return g_values;
}

void Set(const Values& v) {
    Get();
    g_values = v;
    CreateDirectoryW(profiles::RootDir().c_str(), nullptr);
    const std::wstring ini = Ini();
    for (const Key& k : kKeys) WritePrivateProfileStringW(kSection, k.name, (v.*k.field) ? L"1" : L"0", ini.c_str());
    sound::SetEnabled(v.uiSounds);
}

bool BlockReflexEarly(bool iniDefault) {
    wchar_t appdata[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) return iniDefault;
    const int v = ReadBool(std::wstring(appdata) + L"\\ReFix\\profiles.ini", L"BlockNvidiaReflex");
    return v < 0 ? iniDefault : v == 1;
}

} // namespace refix::overlay::settings
