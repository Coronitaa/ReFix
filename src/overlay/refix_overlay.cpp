#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "refix_overlay.h"
#include "overlay_app.h"
#include "overlay_text.h"
#include "profile_store.h"
#include "save_swap.h"
#include "steam_overlay_block.h"
#include "../unreal_steam_emu.h"

#include <windows.h>
#include <atomic>
#include <mutex>
#include <optional>

extern void ReFixLog(const char* fmt, ...);

using namespace refix::overlay;

namespace {

constexpr DWORD kForcedProfileWaitMs = 30000;
constexpr DWORD kRestartParentWaitMs = 30000;

struct Config {
    bool enabled = true;
    bool blockSteamOverlay = true;
    bool isolateSaves = true;
};

Config g_cfg;
std::mutex g_mu;
std::optional<Profile> g_active;
std::wstring g_cloudFolder;
uint32_t g_cloudApp = 0;
std::atomic<bool> g_started{false};

// Identity variables LoadConfig exports; the profile restart puts back what the game was launched with,
// or the new process would inherit this profile's SteamID.
const wchar_t* const kIdentityEnv[] = {
    L"REFIX_STEAM_ID", L"REFIX_STEAMID", L"SteamID", L"SteamId",
    L"REFIX_STEAM_PERSONA_NAME", L"REFIX_USER_NAME", L"REFIX_USERNAME", L"SteamPersonaName",
};
struct EnvValue {
    bool present = false;
    std::wstring value;
};
EnvValue g_launchEnv[sizeof(kIdentityEnv) / sizeof(kIdentityEnv[0])];
bool g_envCaptured = false;

void CaptureLaunchEnvironment() {
    if (g_envCaptured) return;
    g_envCaptured = true;
    for (size_t i = 0; i < sizeof(kIdentityEnv) / sizeof(kIdentityEnv[0]); ++i) {
        wchar_t buf[512];
        const DWORD n = GetEnvironmentVariableW(kIdentityEnv[i], buf, 512);
        g_launchEnv[i].present = n > 0 && n < 512;
        if (g_launchEnv[i].present) g_launchEnv[i].value = buf;
    }
}

bool ParseBool(const char* v, bool def) {
    if (!v[0]) return def;
    if (!_stricmp(v, "true") || !_stricmp(v, "1") || !_stricmp(v, "yes") || !_stricmp(v, "on")) return true;
    if (!_stricmp(v, "false") || !_stricmp(v, "0") || !_stricmp(v, "no") || !_stricmp(v, "off")) return false;
    return def;
}

void LoadConfig(const std::string& iniPath) {
    auto read = [&](const char* key, bool def) {
        char buf[16] = {};
        GetPrivateProfileStringA("Overlay", key, "", buf, sizeof(buf), iniPath.c_str());
        return ParseBool(buf, def);
    };
    g_cfg.enabled = read("Enabled", true);
    g_cfg.blockSteamOverlay = read("BlockSteamOverlay", true);
    g_cfg.isolateSaves = read("IsolateSaves", true);
}

// Same search as the emulator's LoadConfig: next to the exe and up to four folders above.
std::string FindIni() {
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

bool EnvIs(const wchar_t* name, const wchar_t* value) {
    wchar_t buf[16] = {};
    return GetEnvironmentVariableW(name, buf, 16) > 0 && _wcsicmp(buf, value) == 0;
}

bool IsGuiProcess() {
    const auto* base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE && nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI;
}

// Headless runs: Unity -batchmode / -nographics, Source/Unreal -server.
bool HasHeadlessSwitch() {
    std::wstring cmd = GetCommandLineW();
    for (wchar_t& c : cmd) c = (wchar_t)towlower(c);
    for (const wchar_t* sw : { L"-batchmode", L"-nographics", L"-server" }) {
        const size_t len = wcslen(sw);
        for (size_t at = cmd.find(sw); at != std::wstring::npos; at = cmd.find(sw, at + 1)) {
            const bool startOk = at == 0 || cmd[at - 1] == L' ' || cmd[at - 1] == L'"' || cmd[at - 1] == L'\t';
            const wchar_t next = at + len < cmd.size() ? cmd[at + len] : L' ';
            if (startOk && (next == L' ' || next == L'"' || next == L'\t')) return true;
        }
    }
    return false;
}

// Whether this process may show the overlay and its profile window.
bool Allowed() {
    if (EnvIs(L"REFIX_NO_OVERLAY", L"1")) return false;
    if (!g_cfg.enabled) return false;
    return IsGuiProcess() && !HasHeadlessSwitch();
}

// `otherInstance`: another window of this game is open, so the player of this one has to say who they are.
std::optional<Profile> PickProfile(const std::vector<Profile>& all, bool otherInstance) {
    wchar_t forced[32] = {};
    if (GetEnvironmentVariableW(L"REFIX_PROFILE_ID", forced, 32) > 0) {
        SetEnvironmentVariableW(L"REFIX_PROFILE_ID", nullptr);   // not for child processes of the game
        if (auto p = profiles::Load(forced)) {
            if (profiles::WaitLock(p->id, kForcedProfileWaitMs)) return p;
            ReFixLog("[Overlay] Profile %s from REFIX_PROFILE_ID is still in use", ToUtf8(p->id).c_str());
        }
    }
    if (otherInstance) return std::nullopt;
    if (auto last = profiles::Load(profiles::LastProfileId())) {
        if (profiles::TryLock(last->id)) return last;
    }
    for (const Profile& p : all) {   // most recently used first
        if (profiles::TryLock(p.id)) return p;
    }
    return std::nullopt;
}

} // namespace

namespace refix::overlay {

void RestoreLaunchEnvironment() {
    for (size_t i = 0; i < sizeof(kIdentityEnv) / sizeof(kIdentityEnv[0]); ++i) {
        SetEnvironmentVariableW(kIdentityEnv[i], g_launchEnv[i].present ? g_launchEnv[i].value.c_str() : nullptr);
    }
}

} // namespace refix::overlay

namespace ReFixOverlay {

void ArmEarly() {
    CaptureLaunchEnvironment();
    LoadConfig(FindIni());
    if (EnvIs(L"REFIX_NO_OVERLAY", L"1") || !g_cfg.enabled || !g_cfg.blockSteamOverlay) return;
    ArmSteamOverlayBlock();
}

void WaitForRestartParent() {
    wchar_t buf[16] = {};
    if (GetEnvironmentVariableW(L"REFIX_RESTART_PID", buf, 16) == 0) return;
    SetEnvironmentVariableW(L"REFIX_RESTART_PID", nullptr);
    const DWORD pid = wcstoul(buf, nullptr, 10);
    HANDLE h = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    if (!h) return;
    const DWORD r = WaitForSingleObject(h, kRestartParentWaitMs);
    CloseHandle(h);
    ReFixLog("[Overlay] Restart: previous process %lu %s", pid, r == WAIT_OBJECT_0 ? "exited" : "still running");
}

bool ResolveIdentity(uint32_t appId, const std::string& iniPath, Identity& out) {
    CaptureLaunchEnvironment();
    LoadConfig(iniPath);
    if (!Allowed()) {
        ReFixLog("[Overlay] Profiles off for this process (disabled, console or headless)");
        return false;
    }
    profiles::CleanupGuestFolders();
    std::vector<Profile> all = profiles::LoadAll();
    const bool otherInstance = saves::OtherInstanceRunning();
    std::optional<Profile> chosen = PickProfile(all, otherInstance);
    if (!chosen) {
        const DialogKind kind = all.empty() && !otherInstance ? DialogKind::FirstRun : DialogKind::Picker;
        ReFixLog("[Overlay] %s (%zu profiles on this PC): showing the %s",
                 otherInstance ? "Another instance of this game is running" : "No free profile", all.size(),
                 kind == DialogKind::FirstRun ? "first-run dialog" : "profile picker");
        chosen = App::Get().RunDialog(kind);
        all = profiles::LoadAll();
    }
    if (!chosen) {
        ReFixLog("[Overlay] Playing without a profile (machine identity)");
        return false;
    }
    Profile p = *chosen;
    if (!p.guest) {
        profiles::Touch(p);
        profiles::Save(p);
        profiles::SetLastProfileId(p.id);
        if (g_cfg.isolateSaves) saves::SwitchTo(p, all);
    }
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_active = p;
    }
    out.steamId = p.steamId;
    out.name = ToUtf8(p.name);
    ReFixLog("[Overlay] %s %s '%s' -> SteamID %llu (app %u)", p.guest ? "Guest" : "Profile", ToUtf8(p.id).c_str(), out.name.c_str(),
             (unsigned long long)p.steamId, appId);
    return true;
}

std::wstring CloudFolder(uint32_t appId) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) return {};
    if (g_cloudFolder.empty() || g_cloudApp != appId) {
        g_cloudApp = appId;
        g_cloudFolder = g_active->guest ? profiles::GuestCloudRoot(appId) : profiles::CloudRoot(g_active->id, appId);
        for (size_t i = 3; i <= g_cloudFolder.size(); ++i) {
            if (i == g_cloudFolder.size() || g_cloudFolder[i] == L'\\')
                CreateDirectoryW(g_cloudFolder.substr(0, i).c_str(), nullptr);
        }
    }
    return g_cloudFolder;
}

void Start() {
    if (g_started.exchange(true)) return;
    LoadConfig(FindIni());
    if (!Allowed()) return;
    std::optional<Profile> active;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        active = g_active;
    }
    App::Get().Begin(UnrealSteamEmu::GetAppID(), active);
    ReFixLog("[Overlay] Started (%s)", active ? "profile" : "no profile");
}

bool IsEnabled() { return g_started && host::IsRunning(); }

void ActivateOverlay(const char* dialog) {
    if (!IsEnabled()) return;
    const std::string d = dialog ? dialog : "";
    ReFixLog("[Overlay] ActivateGameOverlay('%s')", d.c_str());
    if (_stricmp(d.c_str(), "LobbyInvite") == 0) {
        const uint64_t lobby = UnrealSteamEmu::GetActiveLobbyID();
        App::Get().Post([lobby] { App::Get().OpenOverlay(true, lobby); });
        return;
    }
    App::Get().Post([] { App::Get().OpenOverlay(); });
}

void ActivateOverlayToUser(const char* dialog, uint64_t steamId) {
    if (!IsEnabled()) return;
    ReFixLog("[Overlay] ActivateGameOverlayToUser('%s', %llu)", dialog ? dialog : "", (unsigned long long)steamId);
    App::Get().Post([steamId] {
        App::Get().OpenOverlay();
        App::Get().highlight_ = steamId;
    });
}

void ActivateWebPage(const char* url) {
    if (!IsEnabled() || !url) return;
    const std::wstring u = FromUtf8(url);
    App::Get().Post([u] { App::Get().OpenWeb(u); });
}

void ActivateStore(uint32_t appId) {
    if (!IsEnabled() || appId == 0) return;
    const std::wstring u = L"https://store.steampowered.com/app/" + std::to_wstring(appId) + L"/";
    App::Get().Post([u] { App::Get().OpenWeb(u); });
}

void ActivateInviteDialog(uint64_t lobbyId, const char* connect) {
    if (!IsEnabled()) return;
    const std::string c = connect ? connect : "";
    ReFixLog("[Overlay] Invite dialog (lobby %llu, connect '%s')", (unsigned long long)lobbyId, c.c_str());
    App::Get().Post([lobbyId, c] { App::Get().OpenOverlay(true, lobbyId, c); });
}

bool InviteUser(uint64_t steamId, uint64_t lobbyId, const char* connect) {
    if (!g_started || steamId == 0) return false;
    if (!lobbyId && !(connect && connect[0])) lobbyId = UnrealSteamEmu::GetActiveLobbyID();
    return net::SendInvite(steamId, lobbyId, connect ? connect : "");
}

} // namespace ReFixOverlay
