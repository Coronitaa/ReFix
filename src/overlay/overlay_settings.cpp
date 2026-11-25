#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_settings.h"
#include "overlay_sound.h"
#include "profile_store.h"

#include <windows.h>
#include <algorithm>
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
bool g_displayCustomized = false;

std::wstring Ini() { return profiles::RootDir() + L"\\profiles.ini"; }

std::wstring DisplaySectionForGame() {
    const std::wstring gk = profiles::GameKey();
    if (gk.empty()) return kSection;
    return std::wstring(L"OverlayDisplay.") + gk;
}

// -1 = not set by the player.
int ReadBool(const std::wstring& ini, const wchar_t* sec, const wchar_t* key) {
    wchar_t buf[8] = {};
    GetPrivateProfileStringW(sec, key, L"", buf, 8, ini.c_str());
    if (!buf[0]) return -1;
    return (buf[0] == L'1' || _wcsicmp(buf, L"true") == 0) ? 1 : 0;
}

int ReadInt(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, int def) {
    return (int)GetPrivateProfileIntW(sec, key, def, ini.c_str());
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
            const int v = ReadBool(ini, kSection, k.name);
            if (v >= 0) g_values.*k.field = v == 1;
        }
        int beh = ReadInt(ini, kSection, L"OpenBehavior", (int)GameOpenBehavior::KeepActive);
        if (beh >= 0 && beh <= 2) g_values.openBehavior = static_cast<GameOpenBehavior>(beh);
        int sc = ReadInt(ini, kSection, L"Shortcut", (int)OverlayShortcut::ShiftTab);
        if (sc >= 0 && sc <= (int)OverlayShortcut::Insert) g_values.shortcut = static_cast<OverlayShortcut>(sc);
        int dm = ReadInt(ini, kSection, L"DisplayMode", (int)DisplayMode::FullscreenWindowed);
        if (dm >= 0 && dm <= 2) g_values.displayMode = static_cast<DisplayMode>(dm);
        g_values.windowWidth = ReadInt(ini, kSection, L"WindowWidth", 1920);
        if (g_values.windowWidth <= 0) g_values.windowWidth = 1920;
        g_values.windowHeight = ReadInt(ini, kSection, L"WindowHeight", 1080);
        if (g_values.windowHeight <= 0) g_values.windowHeight = 1080;
        const int customDisp = ReadBool(ini, kSection, L"DisplayCustomized");
        g_displayCustomized = (customDisp >= 0)
            ? (customDisp == 1)
            : (g_values.displayMode != DisplayMode::FullscreenWindowed ||
               g_values.windowWidth != 1920 || g_values.windowHeight != 1080);

        const std::wstring gameSec = DisplaySectionForGame();
        if (gameSec != kSection && ReadBool(ini, gameSec.c_str(), L"PerGame") == 1) {
            g_values.perGameDisplay = true;
            const int gdm = ReadInt(ini, gameSec.c_str(), L"DisplayMode", (int)g_values.displayMode);
            if (gdm >= 0 && gdm <= 2) g_values.displayMode = static_cast<DisplayMode>(gdm);
            const int gw = ReadInt(ini, gameSec.c_str(), L"WindowWidth", g_values.windowWidth);
            if (gw > 0) g_values.windowWidth = gw;
            const int gh = ReadInt(ini, gameSec.c_str(), L"WindowHeight", g_values.windowHeight);
            if (gh > 0) g_values.windowHeight = gh;
            const int gCustom = ReadBool(ini, gameSec.c_str(), L"DisplayCustomized");
            g_displayCustomized = (gCustom >= 0) ? (gCustom == 1) : true;
        }
    }
    return g_values;
}

bool HasSavedDisplay() {
    Get();
    return g_displayCustomized;
}

void MarkDisplayCustomized(bool customized) {
    Get();
    g_displayCustomized = customized;
    CreateDirectoryW(profiles::RootDir().c_str(), nullptr);
    const std::wstring ini = Ini();
    const std::wstring sec = g_values.perGameDisplay ? DisplaySectionForGame() : kSection;
    WritePrivateProfileStringW(sec.c_str(), L"DisplayCustomized", customized ? L"1" : L"0", ini.c_str());
}

void Set(const Values& v) {
    Get();
    g_values = v;
    CreateDirectoryW(profiles::RootDir().c_str(), nullptr);
    const std::wstring ini = Ini();
    for (const Key& k : kKeys) WritePrivateProfileStringW(kSection, k.name, (v.*k.field) ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(kSection, L"OpenBehavior", std::to_wstring((int)v.openBehavior).c_str(), ini.c_str());
    WritePrivateProfileStringW(kSection, L"Shortcut", std::to_wstring((int)v.shortcut).c_str(), ini.c_str());
    const std::wstring gameSec = DisplaySectionForGame();
    if (gameSec != kSection) {
        WritePrivateProfileStringW(gameSec.c_str(), L"PerGame", v.perGameDisplay ? L"1" : L"0", ini.c_str());
    }
    const std::wstring dispSec = (v.perGameDisplay && gameSec != kSection) ? gameSec : kSection;
    WritePrivateProfileStringW(dispSec.c_str(), L"DisplayMode", std::to_wstring((int)v.displayMode).c_str(), ini.c_str());
    WritePrivateProfileStringW(dispSec.c_str(), L"WindowWidth", std::to_wstring(v.windowWidth).c_str(), ini.c_str());
    WritePrivateProfileStringW(dispSec.c_str(), L"WindowHeight", std::to_wstring(v.windowHeight).c_str(), ini.c_str());
    sound::SetEnabled(v.uiSounds);
}

bool ApplyDisplayToWindow(HWND gameHwnd, const Values& v) {
    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    HMONITOR mon = MonitorFromWindow(gameHwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(mon, &mi)) return false;

    if (IsIconic(gameHwnd) || (v.displayMode == DisplayMode::Windowed && IsZoomed(gameHwnd))) {
        ShowWindowAsync(gameHwnd, SW_RESTORE);
    }

    const LONG_PTR curStyle = GetWindowLongPtrW(gameHwnd, GWL_STYLE);
    const LONG_PTR curExStyle = GetWindowLongPtrW(gameHwnd, GWL_EXSTYLE);
    const UINT swpFlags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS;

    if (v.displayMode == DisplayMode::Windowed) {
        const LONG_PTR newStyle = (curStyle & ~(WS_POPUP | WS_MAXIMIZE)) | WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        const LONG_PTR newExStyle = curExStyle | WS_EX_WINDOWEDGE;
        SetWindowLongPtrW(gameHwnd, GWL_STYLE, newStyle);
        SetWindowLongPtrW(gameHwnd, GWL_EXSTYLE, newExStyle);

        int reqW = (v.windowWidth >= 320) ? v.windowWidth : 1280;
        int reqH = (v.windowHeight >= 240) ? v.windowHeight : 720;
        RECT rc = { 0, 0, reqW, reqH };
        AdjustWindowRectEx(&rc, (DWORD)newStyle, FALSE, (DWORD)newExStyle);

        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;
        const int workW = mi.rcWork.right - mi.rcWork.left;
        const int workH = mi.rcWork.bottom - mi.rcWork.top;
        if (winW > workW) winW = workW;
        if (winH > workH) winH = workH;

        const int x = mi.rcWork.left + (workW - winW) / 2;
        const int y = mi.rcWork.top + (workH - winH) / 2;
        SetWindowPos(gameHwnd, nullptr, x, y, winW, winH, swpFlags);
    } else {
        const LONG_PTR newStyle = (curStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU)) | WS_POPUP | WS_VISIBLE;
        const LONG_PTR newExStyle = curExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
        SetWindowLongPtrW(gameHwnd, GWL_STYLE, newStyle);
        SetWindowLongPtrW(gameHwnd, GWL_EXSTYLE, newExStyle);

        const int monW = mi.rcMonitor.right - mi.rcMonitor.left;
        const int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
        SetWindowPos(gameHwnd, nullptr, mi.rcMonitor.left, mi.rcMonitor.top, monW, monH, swpFlags);
    }
    return true;
}

bool WindowMatchesDisplay(HWND gameHwnd, const Values& v) {
    if (!gameHwnd || !IsWindow(gameHwnd)) return true;

    HMONITOR mon = MonitorFromWindow(gameHwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(mon, &mi)) return true;

    RECT wr{};
    if (!GetWindowRect(gameHwnd, &wr)) return true;
    const int curW = wr.right - wr.left;
    const int curH = wr.bottom - wr.top;
    const LONG_PTR style = GetWindowLongPtrW(gameHwnd, GWL_STYLE);

    if (v.displayMode == DisplayMode::Windowed) {
        if ((style & WS_CAPTION) == 0 || (style & WS_POPUP) != 0) return false;
        const int reqW = (v.windowWidth >= 320) ? v.windowWidth : 1280;
        const int reqH = (v.windowHeight >= 240) ? v.windowHeight : 720;
        RECT rc = { 0, 0, reqW, reqH };
        const LONG_PTR exStyle = GetWindowLongPtrW(gameHwnd, GWL_EXSTYLE);
        AdjustWindowRectEx(&rc, (DWORD)style, FALSE, (DWORD)exStyle);
        const int expW = std::min<int>(rc.right - rc.left, mi.rcWork.right - mi.rcWork.left);
        const int expH = std::min<int>(rc.bottom - rc.top, mi.rcWork.bottom - mi.rcWork.top);
        return (curW >= expW - 8 && curW <= expW + 8) && (curH >= expH - 8 && curH <= expH + 8);
    }

    if ((style & WS_CAPTION) != 0) return false;
    const int monW = mi.rcMonitor.right - mi.rcMonitor.left;
    const int monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
    return (curW >= monW - 4 && curW <= monW + 4) && (curH >= monH - 4 && curH <= monH + 4);
}

std::vector<ResolutionOption> GetSupportedResolutions(HWND gameHwnd) {
    struct Pair { int w; int h; };
    std::vector<Pair> modes;
    auto addUnique = [&](int w, int h) {
        if (w < 640 || h < 480) return;
        for (const auto& m : modes) {
            if (m.w == w && m.h == h) return;
        }
        modes.push_back({ w, h });
    };

#if defined(_WIN32)
    // Enumerate OS display modes (on Linux under Wine/Proton, EnumDisplaySettingsW exposes XRandR/Wayland modes)
    HMONITOR mon = (gameHwnd && IsWindow(gameHwnd))
        ? MonitorFromWindow(gameHwnd, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    const wchar_t* device = nullptr;
    if (GetMonitorInfoW(mon, &mi)) {
        device = mi.szDevice;
    }

    DEVMODEW curDm = {};
    curDm.dmSize = sizeof(curDm);
    if (EnumDisplaySettingsW(device, ENUM_CURRENT_SETTINGS, &curDm)) {
        addUnique((int)curDm.dmPelsWidth, (int)curDm.dmPelsHeight);
    }

    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(device, i, &dm); ++i) {
        if (dm.dmBitsPerPel == 0 || dm.dmBitsPerPel >= 24) {
            addUnique((int)dm.dmPelsWidth, (int)dm.dmPelsHeight);
        }
        dm.dmSize = sizeof(dm);
    }
#endif

    if (modes.empty()) {
        const int maxW = GetSystemMetrics(SM_CXSCREEN);
        const int maxH = GetSystemMetrics(SM_CYSCREEN);
        static const Pair kFallback[] = {
            { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1366, 768 },
            { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 }
        };
        for (const auto& f : kFallback) {
            if ((maxW <= 0 || f.w <= maxW) && (maxH <= 0 || f.h <= maxH)) {
                addUnique(f.w, f.h);
            }
        }
        if (maxW >= 640 && maxH >= 480) addUnique(maxW, maxH);
    }

    std::sort(modes.begin(), modes.end(), [](const Pair& a, const Pair& b) {
        if (a.w != b.w) return a.w < b.w;
        return a.h < b.h;
    });

    std::vector<ResolutionOption> out;
    out.reserve(modes.size());
    for (const auto& m : modes) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d x %d", m.w, m.h);
        out.push_back({ m.w, m.h, buf });
    }
    return out;
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
    const int v = ReadBool(Ini(), kSection, L"SteamOnline");
    return v < 0 ? SteamOnline::Auto : v == 1 ? SteamOnline::On : SteamOnline::Off;
}

void SetSteamOnline(bool on) {
    CreateDirectoryW(profiles::RootDir().c_str(), nullptr);
    WritePrivateProfileStringW(kSection, L"SteamOnline", on ? L"1" : L"0", Ini().c_str());
}

bool BlockReflexEarly(bool iniDefault) {
    wchar_t appdata[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) return iniDefault;
    const int v = ReadBool(std::wstring(appdata) + L"\\ReFix\\profiles.ini", kSection, L"BlockNvidiaReflex");
    return v < 0 ? iniDefault : v == 1;
}

bool ReplaceWithSteamOverlay() {
    const std::string ini = FindReFixIni();
    WritePrivateProfileStringA("Overlay", "Enabled", "false", ini.c_str());
    WritePrivateProfileStringA("Overlay", "BlockSteamOverlay", "false", ini.c_str());
    return true;
}

bool DisableOverlayCompletely() {
    const std::string ini = FindReFixIni();
    WritePrivateProfileStringA("Overlay", "Enabled", "false", ini.c_str());
    WritePrivateProfileStringA("Overlay", "BlockSteamOverlay", "true", ini.c_str());
    return true;
}

} // namespace refix::overlay::settings
