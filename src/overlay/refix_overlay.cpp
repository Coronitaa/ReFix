#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "refix_overlay.h"
#include "overlay_ui_mods.h"
#include "overlay_app.h"
#include "overlay_sound.h"
#include "overlay_text.h"
#include "profile_store.h"
#include "save_swap.h"
#include "steam_overlay_block.h"
#include "nvidia_reflex_block.h"
#include "avatar_image.h"
#include "presence_net.h"
#include "../unreal_steam_emu.h"

#include <windows.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <map>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>

extern void ReFixLog(const char* fmt, ...);
extern "C" void CSGO_SetPrimeStatus(bool enabled);

using namespace refix::overlay;

namespace {

constexpr DWORD kForcedProfileWaitMs = 30000;
constexpr DWORD kRestartParentWaitMs = 30000;

struct Config {
    bool enabled = true;
    bool blockSteamOverlay = true;
    bool isolateSaves = true;
    bool sounds = true;
    bool firstRunDialog = false;
    bool blockNvidiaReflex = true;
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
    g_cfg.sounds = read("Sounds", true);
    g_cfg.firstRunDialog = read("FirstRunDialog", false);
    g_cfg.blockNvidiaReflex = read("BlockNvidiaReflex", true);
    sound::SetEnabled(g_cfg.sounds);
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
    wchar_t forced[32] = {};
    if (GetEnvironmentVariableW(L"REFIX_PROFILE_ID", forced, 32) > 0 || EnvIs(L"REFIX_FORCE_GUI", L"1")) {
        return true;
    }
    return IsGuiProcess() && !HasHeadlessSwitch();
}

// `otherInstance`: another window of this game is open, so the player of this one has to say who they are.
// `all`: the profiles of this game.
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
    if (auto last = profiles::Load(profiles::LastProfileId(profiles::GameKey()))) {
        if (profiles::UsedIn(*last, profiles::GameKey()) && profiles::TryLock(last->id)) return last;
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
    // Independent of the ReFix overlay: Enabled=false + BlockSteamOverlay=true means no overlay at all,
    // Enabled=false + BlockSteamOverlay=false lets Steam's own overlay draw.
    if (g_cfg.blockSteamOverlay) ArmSteamOverlayBlock();
    if (g_cfg.blockNvidiaReflex) ArmNvidiaReflexBlock();
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
    const std::wstring game = profiles::GameKey();
    const std::vector<Profile> mine = profiles::ForGame(all, game);
    const bool otherInstance = saves::OtherInstanceRunning();
    std::optional<Profile> chosen = PickProfile(mine, otherInstance);
    if (!chosen) {
        if (otherInstance) {
            ReFixLog("[Overlay] Another instance is running; showing profile picker");
            chosen = App::Get().RunDialog(DialogKind::Picker);
            all = profiles::LoadAll();
        } else if (mine.empty()) {
            if (g_cfg.firstRunDialog) {
                ReFixLog("[Overlay] First run with FirstRunDialog=true; showing first-run dialog");
                chosen = App::Get().RunDialog(DialogKind::FirstRun);
                all = profiles::LoadAll();
            } else {
                ReFixLog("[Overlay] First run for this game: booting directly with machine identity (Shift+Tab to configure profile)");
                return false;
            }
        } else {
            ReFixLog("[Overlay] No free profile; showing profile picker");
            chosen = App::Get().RunDialog(DialogKind::Picker);
            all = profiles::LoadAll();
        }
    }
    if (!chosen) {
        ReFixLog("[Overlay] Playing without a profile (machine identity)");
        return false;
    }
    Profile p = *chosen;
    if (!p.guest) {
        profiles::Touch(p);
        profiles::AddGame(p, game, profiles::DefaultGameName(), false);
        profiles::Save(p);
        profiles::SetLastProfileId(p.id, game);
        if (g_cfg.isolateSaves) saves::SwitchTo(p, profiles::ForGame(all, game));
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

std::wstring ActiveProfileId() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_active ? g_active->id : std::wstring();
}

std::wstring StatsFolder(uint32_t appId) {
    std::lock_guard<std::mutex> lock(g_mu);
    std::wstring folder;
    if (g_active) {
        folder = g_active->guest ? profiles::GuestStatsRoot(appId) : profiles::StatsRoot(g_active->id, appId);
    } else {
        folder = profiles::RootDir() + L"\\Saves\\_pc\\Stats\\" + std::to_wstring(appId);
    }
    for (size_t i = 3; i <= folder.size(); ++i) {
        if (i == folder.size() || folder[i] == L'\\')
            CreateDirectoryW(folder.substr(0, i).c_str(), nullptr);
    }
    return folder;
}

std::wstring InventoryFolder(uint32_t appId) {
    std::lock_guard<std::mutex> lock(g_mu);
    std::wstring folder;
    if (g_active) {
        folder = g_active->guest ? profiles::GuestInventoryRoot(appId) : profiles::InventoryRoot(g_active->id, appId);
    } else {
        folder = profiles::RootDir() + L"\\Saves\\_pc\\Inventory\\" + std::to_wstring(appId);
    }
    for (size_t i = 3; i <= folder.size(); ++i) {
        if (i == folder.size() || folder[i] == L'\\')
            CreateDirectoryW(folder.substr(0, i).c_str(), nullptr);
    }
    return folder;
}

void Start() {
    if (g_started.exchange(true)) return;
    LoadConfig(FindIni());
    if (g_cfg.blockNvidiaReflex) ArmNvidiaReflexBlock();
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
    if (_stricmp(d.c_str(), "Community") == 0 || _stricmp(d.c_str(), "Market") == 0) {
        OpenMarketGiveMenu();
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
    if (strstr(url, "market") || strstr(url, "community") || strstr(url, "give")) {
        OpenMarketGiveMenu();
        return;
    }
    if (strstr(url, "54029") || strstr(url, "prime") || strstr(url, "sub/")) {
        PromptPurchase(730, 54029, "CS:GO Prime Status Upgrade",
            "Mejora tu cuenta a Estado Prime. Emparejamiento exclusivo, obtencion de cajas semanales y medallas de servicio.",
            false, [](bool approved) {
                if (approved) {
                    ReFixLog("[Overlay] Prime Status Upgrade approved by player!");
                    CSGO_SetPrimeStatus(true);
                }
            });
        return;
    }
    const std::wstring u = FromUtf8(url);
    App::Get().Post([u] { App::Get().OpenWeb(u); });
}

void OpenMarketGiveMenu() {
    if (!IsEnabled()) return;
    App::Get().Post([] {
        App::Get().OpenOverlay();
        refix::overlay::OpenMarketGiveMenu();
    });
}

void OpenMatchmakerModal() {
    if (!IsEnabled()) return;
    App::Get().Post([] {
        App::Get().OpenOverlay();
        refix::overlay::OpenHostMatchModal();
    });
}

void ActivateStore(uint32_t appId) {
    if (!IsEnabled() || appId == 0) return;
    PromptPurchase(appId, 0, "", "", false);
}

void PromptPurchase(uint32_t appId, uint64_t orderId, const std::string& itemName, const std::string& itemDesc, bool isMicroTxn, std::function<void(bool)> onComplete) {
    if (!IsEnabled()) return;
    ReFixLog("[Overlay] PromptPurchase requested: appId=%u, orderId=%llu, item='%s'", appId, (unsigned long long)orderId, itemName.c_str());
    App::Get().Post([appId, orderId, itemName, itemDesc, isMicroTxn, onComplete] {
        App::Get().OpenPurchasePrompt(appId, orderId, itemName, itemDesc, isMicroTxn, onComplete);
    });
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

struct CachedImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
};

static std::mutex g_avatarMu;
static std::unordered_map<int, CachedImage> g_avatarImages;
static std::map<std::pair<uint64_t, int>, int> g_avatarHandles;
static int g_nextAvatarHandle = 1;

struct ComInitScope {
    HRESULT hr;
    ComInitScope() {
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == RPC_E_CHANGED_MODE) hr = S_FALSE;
    }
    ~ComInitScope() {
        if (SUCCEEDED(hr) && hr != S_FALSE) CoUninitialize();
    }
};

static void GenerateStockAvatar(int stockIdx, int width, int height, std::vector<uint8_t>& outRgba) {
    if (width <= 0 || height <= 0) return;
    outRgba.resize((size_t)width * height * 4);
    stockIdx = (stockIdx % 12 + 12) % 12;

    struct Color { uint8_t r, g, b; };
    static const Color kFrom[12] = {
        { 255, 107, 107 }, {  79, 172, 254 }, { 247, 151,  30 }, { 246, 180,  80 },
        { 102, 126, 234 }, {  40, 200, 110 }, { 250, 112, 154 }, {  48, 207, 208 },
        { 161, 140, 209 }, { 255, 154,  68 }, { 220, 110, 250 }, {  11, 163,  96 },
    };
    static const Color kTo[12] = {
        { 196,  69, 105 }, {   0, 200, 230 }, { 255, 210,   0 }, { 253, 120, 100 },
        { 118,  75, 162 }, {  40, 210, 190 }, { 240, 190,  60 }, {  51,   8, 103 },
        { 230, 150, 210 }, { 252,  96, 118 }, { 245,  87, 108 }, {  60, 186, 146 },
    };
    static const wchar_t kGlyphs[12] = {
        0xE7FC, 0xE99A, 0xE945, 0xE706, 0xE708, 0xE909,
        0xEA86, 0xE76E, 0xE8D6, 0xED56, 0xEB51, 0xE8E1
    };

    const Color c1 = kFrom[stockIdx];
    const Color c2 = kTo[stockIdx];
    const wchar_t glyphChar = kGlyphs[stockIdx];

    const float cx = (width - 1) * 0.5f;
    const float cy = (height - 1) * 0.5f;

    std::vector<uint8_t> glyphMask((size_t)width * height, 0);
    int offsetX = 0, offsetY = 0;

    HDC hdc = CreateCompatibleDC(nullptr);
    if (hdc) {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (hbm && bits) {
            HGDIOBJ oldBm = SelectObject(hdc, hbm);
            int fontSize = (int)std::round(width * 0.52f);
            if (fontSize < 10) fontSize = 10;

            bool isMdl2 = false;
            HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                      L"Segoe Fluent Icons");
            if (!hFont) {
                hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                    L"Segoe MDL2 Assets");
                isMdl2 = true;
            }

            HGDIOBJ oldFont = hFont ? SelectObject(hdc, hFont) : nullptr;
            memset(bits, 0, (size_t)width * height * 4);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            RECT rc = { 0, 0, width, height };
            DrawTextW(hdc, &glyphChar, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            GdiFlush();

            const uint8_t* raw = reinterpret_cast<const uint8_t*>(bits);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    glyphMask[y * width + x] = raw[(y * width + x) * 4];
                }
            }

            // On Windows 10 Segoe MDL2 Assets, the pizza glyph (0xED56) was drawn facing left.
            // Flip it horizontally so it consistently faces right as in Segoe Fluent Icons / overlay.
            if (isMdl2 && stockIdx == 9) {
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width / 2; ++x) {
                        std::swap(glyphMask[y * width + x], glyphMask[y * width + (width - 1 - x)]);
                    }
                }
            }

            int minX = width, maxX = -1;
            int minY = height, maxY = -1;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    uint8_t v = glyphMask[y * width + x];
                    if (v > 15) {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
            }

            if (maxX >= minX && maxY >= minY) {
                float curCx = (minX + maxX) * 0.5f;
                float curCy = (minY + maxY) * 0.5f;
                offsetX = (int)std::round(cx - curCx);
                offsetY = (int)std::round(cy - curCy);
            }

            if (oldFont) SelectObject(hdc, oldFont);
            if (hFont) DeleteObject(hFont);
            SelectObject(hdc, oldBm);
            DeleteObject(hbm);
        }
        DeleteDC(hdc);
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcX = x - offsetX;
            int srcY = y - offsetY;
            uint8_t maskVal = 0;
            if (srcX >= 0 && srcX < width && srcY >= 0 && srcY < height) {
                maskVal = glyphMask[srcY * width + srcX];
            }
            float glyphA = ((float)maskVal / 255.0f) * 0.92f;

            float t = (float)(x + y) / (float)((width - 1) + (height - 1));
            float gr = c1.r + t * (c2.r - c1.r);
            float gg = c1.g + t * (c2.g - c1.g);
            float gb = c1.b + t * (c2.b - c1.b);

            float finalR = gr * (1.0f - glyphA) + 255.0f * glyphA;
            finalR = std::min(finalR, 255.0f);
            float finalG = gg * (1.0f - glyphA) + 255.0f * glyphA;
            finalG = std::min(finalG, 255.0f);
            float finalB = gb * (1.0f - glyphA) + 255.0f * glyphA;
            finalB = std::min(finalB, 255.0f);

            size_t idx = ((size_t)y * width + x) * 4;
            outRgba[idx + 0] = (uint8_t)std::clamp((int)std::round(finalR), 0, 255);
            outRgba[idx + 1] = (uint8_t)std::clamp((int)std::round(finalG), 0, 255);
            outRgba[idx + 2] = (uint8_t)std::clamp((int)std::round(finalB), 0, 255);
            outRgba[idx + 3] = 255;
        }
    }
}

static bool ResolveAvatarRgba(uint64_t steamId, int size, std::vector<uint8_t>& outRgba) {
    if (size <= 0) size = 64;
    ComInitScope com;

    Profile localProfile;
    bool hasLocalProfile = false;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_active) {
            localProfile = *g_active;
            hasLocalProfile = true;
        }
    }

    const uint64_t localSid = UnrealSteamEmu::GetLocalSteamID();
    bool isLocal = (steamId == 0 || (hasLocalProfile && steamId == localProfile.steamId) || (localSid != 0 && steamId == localSid));

    if (isLocal) {
        if (hasLocalProfile) {
            std::wstring path = profiles::AvatarPath(localProfile.id);
            if (localProfile.customAvatar || GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                image::Rgba img;
                if (image::LoadFile(path, img, size) && !img.pixels.empty()) {
                    outRgba = std::move(img.pixels);
                    return true;
                }
            }
        }

        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string exeDir = exePath;
        exeDir = exeDir.substr(0, exeDir.find_last_of("\\/") + 1);
        std::string candPaths[] = {
            exeDir + "steam_settings\\avatar.png",
            exeDir + "steam_settings\\avatar.jpg",
            exeDir + "avatar.png",
            exeDir + "avatar.jpg"
        };
        for (const auto& cp : candPaths) {
            if (GetFileAttributesA(cp.c_str()) != INVALID_FILE_ATTRIBUTES) {
                image::Rgba img;
                if (image::LoadFile(FromUtf8(cp), img, size) && !img.pixels.empty()) {
                    outRgba = std::move(img.pixels);
                    return true;
                }
            }
        }

        int stock = hasLocalProfile ? localProfile.stockAvatar : (int)(steamId ? (steamId % 12) : 0);
        GenerateStockAvatar(stock, size, size, outRgba);
        return true;
    }

    std::vector<Profile> allProfiles = profiles::LoadAll();
    for (const Profile& p : allProfiles) {
        if (p.steamId == steamId) {
            std::wstring path = profiles::AvatarPath(p.id);
            if (p.customAvatar || GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                image::Rgba img;
                if (image::LoadFile(path, img, size) && !img.pixels.empty()) {
                    outRgba = std::move(img.pixels);
                    return true;
                }
            }
            GenerateStockAvatar(p.stockAvatar, size, size, outRgba);
            return true;
        }
    }

    std::vector<net::RemotePlayer> peers = net::Players();
    for (const net::RemotePlayer& peer : peers) {
        if (peer.steamId == steamId) {
            if (peer.avatarKind == net::AvatarKind::Custom) {
                std::vector<uint8_t> jpeg;
                if (net::FetchAvatar(steamId, peer.avatarKey, jpeg)) {
                    image::Rgba img;
                    if (image::DecodeMemory(jpeg.data(), jpeg.size(), img, size) && !img.pixels.empty()) {
                        outRgba = std::move(img.pixels);
                        return true;
                    }
                }
            }
            GenerateStockAvatar((int)peer.avatarKey, size, size, outRgba);
            return true;
        }
    }

    GenerateStockAvatar((int)(steamId % 12), size, size, outRgba);
    return true;
}

int GetAvatarHandle(uint64_t steamId, int size) {
    if (size <= 0) size = 64;
    std::lock_guard<std::mutex> lock(g_avatarMu);
    auto key = std::make_pair(steamId, size);
    auto it = g_avatarHandles.find(key);
    if (it != g_avatarHandles.end()) {
        return it->second;
    }
    int handle = g_nextAvatarHandle++;
    g_avatarHandles[key] = handle;
    CachedImage ci;
    ci.width = size;
    ci.height = size;
    ResolveAvatarRgba(steamId, size, ci.rgba);
    g_avatarImages[handle] = std::move(ci);

    struct AvatarImageLoaded_t {
        enum { k_iCallback = 334 };
        uint64_t m_steamID;
        int m_iImage;
        int m_iWide;
        int m_iTall;
    };
    AvatarImageLoaded_t cb{};
    cb.m_steamID = steamId;
    cb.m_iImage = handle;
    cb.m_iWide = size;
    cb.m_iTall = size;
    UnrealSteamEmu::PostCallback(AvatarImageLoaded_t::k_iCallback, &cb, sizeof(cb));

    return handle;
}

bool GetImageSize(int iImage, uint32_t* pnWidth, uint32_t* pnHeight) {
    if (iImage <= 0) return false;
    std::lock_guard<std::mutex> lock(g_avatarMu);
    if (g_avatarImages.empty() && iImage == 1) {
        CachedImage ci;
        ci.width = 64;
        ci.height = 64;
        ResolveAvatarRgba(0, 64, ci.rgba);
        g_avatarImages[1] = std::move(ci);
        g_avatarHandles[{0, 64}] = 1;
        if (g_nextAvatarHandle <= 1) g_nextAvatarHandle = 2;
    }
    auto it = g_avatarImages.find(iImage);
    if (it == g_avatarImages.end()) return false;
    if (pnWidth) *pnWidth = (uint32_t)it->second.width;
    if (pnHeight) *pnHeight = (uint32_t)it->second.height;
    return true;
}

bool GetImageRGBA(int iImage, uint8_t* pubDest, int nDestBufferSize) {
    if (iImage <= 0 || !pubDest) return false;
    std::lock_guard<std::mutex> lock(g_avatarMu);
    if (g_avatarImages.empty() && iImage == 1) {
        CachedImage ci;
        ci.width = 64;
        ci.height = 64;
        ResolveAvatarRgba(0, 64, ci.rgba);
        g_avatarImages[1] = std::move(ci);
        g_avatarHandles[{0, 64}] = 1;
        if (g_nextAvatarHandle <= 1) g_nextAvatarHandle = 2;
    }
    auto it = g_avatarImages.find(iImage);
    if (it == g_avatarImages.end()) return false;
    size_t needed = (size_t)it->second.width * it->second.height * 4;
    if ((size_t)nDestBufferSize < needed) return false;
    memcpy(pubDest, it->second.rgba.data(), needed);
    return true;
}

void InvalidateAvatar(uint64_t steamId) {
    std::lock_guard<std::mutex> lock(g_avatarMu);
    for (int size : { 32, 64, 184 }) {
        auto key = std::make_pair(steamId, size);
        auto it = g_avatarHandles.find(key);
        if (it != g_avatarHandles.end()) {
            int handle = it->second;
            CachedImage ci;
            ci.width = size;
            ci.height = size;
            ResolveAvatarRgba(steamId, size, ci.rgba);
            g_avatarImages[handle] = std::move(ci);

            struct AvatarImageLoaded_t {
                enum { k_iCallback = 334 };
                uint64_t m_steamID;
                int m_iImage;
                int m_iWide;
                int m_iTall;
            };
            AvatarImageLoaded_t cb{};
            cb.m_steamID = steamId;
            cb.m_iImage = handle;
            cb.m_iWide = size;
            cb.m_iTall = size;
            UnrealSteamEmu::PostCallback(AvatarImageLoaded_t::k_iCallback, &cb, sizeof(cb));
        }
    }
}

} // namespace ReFixOverlay
