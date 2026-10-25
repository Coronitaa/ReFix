#include "refix_pa_offline.h"
#include "refix_il2cpp_scene.h"
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <string>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixPAOffline {

namespace {

// Recreate.StaticGameInfoAot.ModeType values (RecreateAot.dll)
constexpr int32_t kModeUnknown = 0;
constexpr int32_t kModeLan = 3;
constexpr int32_t kModeP2P = 4;
constexpr int32_t kModeDisabled = -1;

// Keep re-asserting the mode for a while in case a later init step writes it back.
constexpr DWORD kMonitorWindowMs = 120000;

std::atomic<int32_t> g_targetMode{ kModeDisabled };
std::atomic<bool> g_engaged{ false };
std::atomic<bool> g_finished{ false };
std::atomic<DWORD> g_engagedTick{ 0 };
std::atomic<bool> g_lanFlagApplied{ false };
std::atomic<bool> g_profileIdDone{ false };
std::string g_profileId;
void* g_klassInjection = nullptr;
void* g_klassGameInfo = nullptr;
void* g_klassAccountManager = nullptr;

bool IsLoginFreeMode(int32_t mode) {
    return mode == kModeUnknown || mode == kModeLan || mode == kModeP2P;
}

std::string IniPath() {
    char buf[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix.ini";
}

int32_t ParseMode(const char* v) {
    if (_stricmp(v, "lan") == 0) return kModeLan;
    if (_stricmp(v, "p2p") == 0) return kModeP2P;
    if (_stricmp(v, "unknown") == 0) return kModeUnknown;
    return kModeDisabled;
}

// Without login the account id is AccountManager.GetFakeId(), which defaults to "fake_<timestamp>": a new
// local profile (and tutorial) on every launch, and the same kind of id for every player on the network.
// Seed it before first use with a stable per-player value (ReFix.ini override, else fake_<SteamID64>).
void SeedStableFakeId() {
    if (g_profileIdDone.load()) return;
    if (!g_klassAccountManager) {
        g_klassAccountManager = ReFixIL2CPPScene::FindClassByName("Network.Account", "AccountManager");
        if (!g_klassAccountManager) return;
    }
    std::string id = g_profileId;
    if (id.empty()) {
        char steamId[32] = { 0 };
        if (!GetEnvironmentVariableA("REFIX_STEAM_ID", steamId, sizeof(steamId))) return; // Steam not initialized yet
        id = std::string("fake_") + steamId;
    }
    g_profileIdDone.store(true);

    void* existing = nullptr;
    ReFixIL2CPPScene::GetStaticFieldValue(g_klassAccountManager, "s_FakeId", &existing);
    if (existing) {
        ReFixLog("[PA-Offline] AccountManager.s_FakeId already generated (%s); profile id not changed",
                 ReFixIL2CPPScene::StringToUtf8(existing).c_str());
        return;
    }
    void* str = ReFixIL2CPPScene::NewPinnedString(id.c_str());
    bool ok = str && ReFixIL2CPPScene::SetStaticReference(g_klassAccountManager, "s_FakeId", str);
    void* readback = nullptr;
    ReFixIL2CPPScene::GetStaticFieldValue(g_klassAccountManager, "s_FakeId", &readback);
    ok = ok && readback == str;
    if (!ok && readback) {
        // Never leave a value we cannot verify: the game would dereference it as a string.
        ReFixIL2CPPScene::SetStaticReference(g_klassAccountManager, "s_FakeId", nullptr);
    }
    ReFixLog("[PA-Offline] Offline profile id %s -> %s", ok ? "set" : "FAILED (game default kept)", id.c_str());
}

bool ResolveClasses() {
    if (!g_klassInjection) g_klassInjection = ReFixIL2CPPScene::FindClassByName("Recreate.LoginFlow", "InjectionNode");
    if (!g_klassGameInfo) g_klassGameInfo = ReFixIL2CPPScene::FindClassByName("Recreate", "StaticGameInfoAot");
    return g_klassInjection && g_klassGameInfo;
}

void ApplyMode(int32_t current, int32_t target) {
    int32_t value = target;
    if (!ReFixIL2CPPScene::SetStaticFieldValue(g_klassGameInfo, "m_ModeType", &value)) {
        ReFixLog("[PA-Offline] Failed to write StaticGameInfoAot.m_ModeType");
        return;
    }
    int32_t check = -1;
    ReFixIL2CPPScene::GetStaticFieldValue(g_klassGameInfo, "m_ModeType", &check);
    ReFixLog("[PA-Offline] StaticGameInfoAot.modeType %d -> %d (readback %d): login-free TrailBlazer path enabled",
             current, target, check);
}

// AutoCreateSingleton<T> keeps its instance in the static s_Singleton of the generic
// instantiation, which is the parent class of T.
void* AutoSingleton(const char* ns, const char* className) {
    void* klass = ReFixIL2CPPScene::FindClassByName(ns, className);
    void* parent = klass ? ReFixIL2CPPScene::GetParentClass(klass) : nullptr;
    void* instance = nullptr;
    if (parent) ReFixIL2CPPScene::GetStaticFieldValue(parent, "s_Singleton", &instance);
    return instance;
}

int ReadBool(void* obj, const char* getter) {
    bool v = false;
    return (obj && ReFixIL2CPPScene::CallBoolGetter(obj, getter, &v)) ? (v ? 1 : 0) : -1;
}

int ReadInt(void* obj, const char* getter) {
    int32_t v = 0;
    return (obj && ReFixIL2CPPScene::CallInt32Getter(obj, getter, &v)) ? v : -1;
}

} // namespace

void LogLoadingGate(void* menuSceneLoader) {
    // Mirrors LoadingNode.<Execute>b__6_2 (PartyAnimals.Gameplay.dll), the last wait before NemoLand loads.
    void* tb = AutoSingleton("Recreate.Scripts.Network.Account", "TrailBlazer");
    void* aa = AutoSingleton("", "AAInitializer");
    void* phase = AutoSingleton("Recreate", "LaunchUiPhase");
    int32_t mode = -1;
    if (g_klassGameInfo) ReFixIL2CPPScene::GetStaticFieldValue(g_klassGameInfo, "m_ModeType", &mode);
    ReFixLog("[PA-Offline] LoadingGate: modeType=%d | TrailBlazer(%p) world=%d ping=%d config=%d auth=%d aux=%d "
             "isSuccess=%d busy=%d | AAInitializer(%p) state=%d initialized=%d hotUpdateComplete=%d hotUpdateSuccess=%d "
             "| existInterruption=%d | uiPhase=%d (need >=4)",
             mode, tb, ReadInt(tb, "get_worldResult"), ReadInt(tb, "get_pingResult"), ReadInt(tb, "get_configResult"),
             ReadInt(tb, "get_authResult"), ReadInt(tb, "get_auxResult"), ReadBool(tb, "get_isSuccess"),
             ReadBool(tb, "get_isBusy"), aa, ReadInt(aa, "get_state"), ReadBool(aa, "get_initialized"),
             ReadBool(aa, "get_hotUpdateComplete"), ReadBool(aa, "get_hotUpdateSuccess"),
             ReadBool(menuSceneLoader, "get_existInterruption"), ReadInt(phase, "get_uiPhase"));

    // CreateNemoLandUi waits for RootCanvas.initializeDone, which itself waits for three Addressables fonts.
    void* rootCanvas = ReFixIL2CPPScene::FindFirstObjectOfType("HeoUi", "RootCanvas");
    if (!rootCanvas) return;
    void* heavy = nullptr;
    void* black = nullptr;
    void* bold = nullptr;
    ReFixIL2CPPScene::GetFieldValue(rootCanvas, "m_FontHeavy", &heavy);
    ReFixIL2CPPScene::GetFieldValue(rootCanvas, "m_FontBlack", &black);
    ReFixIL2CPPScene::GetFieldValue(rootCanvas, "m_FontBold", &bold);
    ReFixLog("[PA-Offline] RootCanvas(%p) initializeDone=%d fontHeavy=%p fontBlack=%p fontBold=%p", rootCanvas,
             ReadBool(rootCanvas, "get_initializeDone"), heavy, black, bold);
}

void Initialize() {
    char val[32] = { 0 };
    GetPrivateProfileStringA("P2P", "OfflineModeType", "lan", val, sizeof(val), IniPath().c_str());
    int32_t mode = ParseMode(val);
    g_targetMode.store(mode);
    g_finished.store(mode == kModeDisabled);
    char profile[128] = { 0 };
    GetPrivateProfileStringA("P2P", "OfflineProfileId", "", profile, sizeof(profile), IniPath().c_str());
    g_profileId = profile;
    ReFixLog("[PA-Offline] OfflineModeType=%s (%s), OfflineProfileId=%s", val, mode == kModeDisabled ? "disabled" : "enabled",
             g_profileId.empty() ? "<fake_SteamID64>" : g_profileId.c_str());
}

void Tick() {
    if (g_finished.load()) return;
    int32_t target = g_targetMode.load();
    SeedStableFakeId();
    if (!ResolveClasses()) return;

    // Hot-update DLLs register the login subtree here; before that, PlatformInitNode may still need Steam mode.
    void* getter = nullptr;
    if (!ReFixIL2CPPScene::GetStaticFieldValue(g_klassInjection, "flowNodeGetter", &getter) || !getter) return;

    int32_t current = -1;
    if (!ReFixIL2CPPScene::GetStaticFieldValue(g_klassGameInfo, "m_ModeType", &current)) return;

    if (!g_engaged.load()) {
        if (IsLoginFreeMode(current)) {
            ReFixLog("[PA-Offline] modeType already login-free (%d); nothing to change", current);
        } else {
            ApplyMode(current, target);
        }
        g_engagedTick.store(GetTickCount());
        g_engaged.store(true);
        return;
    }

    if (current != target && !IsLoginFreeMode(current)) {
        ReFixLog("[PA-Offline] modeType was reset to %d; re-applying", current);
        ApplyMode(current, target);
    }
    if (GetTickCount() - g_engagedTick.load() > kMonitorWindowMs) g_finished.store(true);
}

static void ApplyLanFlag() {
    // Lan mode also needs StaticGameInfo.isLanMode (as the studio's own LAN test path sets it): with a fake
    // account id, LocalPlayerData only creates the Player scope when isEditorOrLanMode is true. The setter
    // re-initializes ServerAgent, so it must run on the Unity main thread.
    if (g_targetMode.load() != kModeLan || g_lanFlagApplied.exchange(true)) return;
    void* klass = ReFixIL2CPPScene::FindClassByName("", "StaticGameInfo");
    bool applied = ReFixIL2CPPScene::InvokeStaticBoolSetter(klass, "set_isLanMode", true);
    bool readback = false;
    ReFixIL2CPPScene::InvokeStaticBoolGetter(klass, "get_isLanMode", &readback);
    ReFixLog("[PA-Offline] StaticGameInfo.set_isLanMode(true) %s (readback %d)", applied ? "invoked" : "FAILED", readback);
}

// First-launch profile steps of LaunchSequenceManager (PartyAnimals.Ui.dll) open mandatory modals
// ("choose your name", "choose avatar") unless the account reports them as done. Confirming them calls
// the backend, which Lan mode refuses, so they could never close. Both flags are local-only.
struct ProfileStep {
    const char* ns;
    const char* className;
    const char* markMethod;
    const char* doneGetter;
    bool done;
};

static ProfileStep g_profileSteps[] = {
    { "Recreate.Service.Account", "AccountNameManager", "MarkDisplayNameChanged", "get_isDisplayNameChanged", false },
    { "Recreate.Service.Personalization", "AvatarSettingManager", "MarkPortraitChanged", "get_isPortraitChanged", false },
};

static void SkipFirstLaunchProfileSteps() {
    for (ProfileStep& step : g_profileSteps) {
        if (step.done) continue;
        void* manager = AutoSingleton(step.ns, step.className);
        if (!manager) continue; // PartyAnimals.Service.dll not initialized yet
        step.done = true;
        ReFixIL2CPPScene::CallObjectGetter(manager, step.markMethod);
        ReFixLog("[PA-Offline] %s.%s() -> %s=%d", step.className, step.markMethod, step.doneGetter + 4,
                 ReadBool(manager, step.doneGetter));
    }
}

void MainThreadTick() {
    if (!g_engaged.load()) return;
    ApplyLanFlag();
    SkipFirstLaunchProfileSteps();
}

bool IsEngaged() {
    return g_engaged.load();
}

bool IsFinished() {
    return g_finished.load();
}

} // namespace ReFixPAOffline
