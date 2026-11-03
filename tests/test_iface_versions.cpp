// Verifies version-correct interface resolution in the ReFix steam_api(64).dll:
// old SDK interface versions must resolve to adapters whose vtable matches the OLD header,
// not to the latest layout. Builds for x64 (steam_api64.dll) and x86 (steam_api.dll).
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <string>

#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "include/steam/steam_api.h"
#include "include/steam/isteamcontroller003.h"
#include "include/steam/isteamgamecoordinator.h"
#include "include/steam/isteamunifiedmessages.h"

static int g_fail = 0;
static void Check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

static std::string ExeDir() {
    char p[MAX_PATH];
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s(p);
    return s.substr(0, s.find_last_of("\\/") + 1);
}

#ifdef _WIN64
static const char* kDllName = "steam_api64.dll";
static const char* kDefaultSrc = "lan\\steam_api64.dll";
#else
static const char* kDllName = "steam_api.dll";
static const char* kDefaultSrc = "lan\\x86\\steam_api.dll";
#endif

typedef bool (*InitFn)();
typedef void* (*CreateIfaceFn)(const char*);
typedef void* (*FindUserIfaceFn)(HSteamUser, const char*);
typedef void* (*PtrFn)();
typedef uint64 (*GetIdFn)(void*);

int main(int argc, char** argv) {
    std::string src = argc > 1 ? argv[1] : ExeDir() + kDefaultSrc;
    std::string dir = ExeDir() + "iface_test_tmp";
    CreateDirectoryA(dir.c_str(), nullptr);
    std::string dll = dir + "\\" + kDllName;
    printf("test_iface_versions: source %s\n", src.c_str());
    if (!CopyFileA(src.c_str(), dll.c_str(), FALSE)) { printf("FAIL: cannot stage dll\n"); return 2; }
    { std::ofstream ini(dir + "\\ReFix.ini"); ini << "[Network]\nMode=force_lan\n"; }
    { std::ofstream id(dir + "\\steam_appid.txt"); id << "480\n"; }
    SetEnvironmentVariableA("SteamAppId", "480");
    SetEnvironmentVariableA("SteamGameId", "480");

    HMODULE h = LoadLibraryA(dll.c_str());
    Check(h != nullptr, "dll loaded");
    if (!h) return 1;
    auto init = (InitFn)GetProcAddress(h, "SteamAPI_Init");
    auto create = (CreateIfaceFn)GetProcAddress(h, "SteamInternal_CreateInterface");
    auto findUser = (FindUserIfaceFn)GetProcAddress(h, "SteamInternal_FindOrCreateUserInterface");
    auto flatUser = (PtrFn)GetProcAddress(h, "SteamAPI_SteamUser_v023");
    auto flatId = (GetIdFn)GetProcAddress(h, "SteamAPI_ISteamUser_GetSteamID");
    Check(init && create && findUser && flatUser && flatId, "exports resolved");
    if (!(init && create && findUser && flatUser && flatId)) return 1;
    Check(init(), "SteamAPI_Init");

    void* latestUser = flatUser();
    uint64 flatSteamId = flatId(latestUser);
    printf("  flat SteamID=%llu\n", (unsigned long long)flatSteamId);
    Check(flatSteamId != 0, "flat SteamID non-zero");

    // --- ISteamClient018 layout (SDK ~1.42, e.g. Overcooked! 2) ---
    ISteamClient018* c18 = (ISteamClient018*)create("SteamClient018");
    Check(c18 != nullptr, "SteamClient018 resolves");
    Check(c18 != (void*)create("SteamClient023"), "SteamClient018 is not the latest-layout client object");
    HSteamPipe pipe = 0; HSteamUser user = 0;
    if (c18) {
        pipe = c18->CreateSteamPipe();
        user = c18->ConnectToGlobalUser(pipe);
        printf("  pipe=%d user=%d\n", (int)pipe, (int)user);
        ISteamUser019* u19 = (ISteamUser019*)c18->GetISteamUser(user, pipe, "SteamUser019");
        Check(u19 != nullptr && (void*)u19 != latestUser, "client018->GetISteamUser(SteamUser019) is a versioned adapter");
        if (u19) {
            Check(u19->GetSteamID().ConvertToUint64() == flatSteamId, "ISteamUser019::GetSteamID == flat SteamID (via client018)");
            Check(u19->BLoggedOn() || true, "ISteamUser019::BLoggedOn no crash");
        }
        ISteamFriends015* f15 = (ISteamFriends015*)c18->GetISteamFriends(user, pipe, "SteamFriends015");
        Check(f15 != nullptr, "client018->GetISteamFriends(SteamFriends015)");
        if (f15) {
            const char* name = f15->GetPersonaName();
            printf("  persona=%s\n", name ? name : "(null)");
            Check(name != nullptr, "ISteamFriends015::GetPersonaName non-null (via client018)");
        }
        ISteamUtils009* ut9 = (ISteamUtils009*)c18->GetISteamUtils(pipe, "SteamUtils009");
        Check(ut9 != nullptr && ut9->GetAppID() != 0, "ISteamUtils009::GetAppID != 0 (via client018)");
        ISteamMatchmaking008* mm = (ISteamMatchmaking008*)c18->GetISteamMatchmaking(user, pipe, "SteamMatchMaking008");
        Check(mm != nullptr, "client018->GetISteamMatchmaking(SteamMatchMaking008)");
    }

    // --- direct SteamInternal_FindOrCreateUserInterface / CreateInterface lookups ---
    ISteamUser019* u19 = (ISteamUser019*)findUser(0, "SteamUser019");
    Check(u19 != nullptr && u19->GetSteamID().ConvertToUint64() == flatSteamId, "ISteamUser019::GetSteamID == flat SteamID");
    ISteamUser012* u12 = (ISteamUser012*)findUser(0, "SteamUser012");
    Check(u12 != nullptr && u12->GetSteamID().ConvertToUint64() == flatSteamId, "ISteamUser012::GetSteamID == flat SteamID");
    ISteamFriends015* f15 = (ISteamFriends015*)findUser(0, "SteamFriends015");
    Check(f15 != nullptr && f15->GetPersonaName() != nullptr, "ISteamFriends015::GetPersonaName non-null");
    ISteamUtils009* ut9 = (ISteamUtils009*)findUser(0, "SteamUtils009");
    Check(ut9 != nullptr && ut9->GetAppID() != 0, "ISteamUtils009::GetAppID != 0");
    ISteamMatchmaking008* mm8 = (ISteamMatchmaking008*)findUser(0, "SteamMatchMaking008");
    Check(mm8 != nullptr, "SteamMatchMaking008 resolves");
    if (mm8) { int n = mm8->GetFavoriteGameCount(); printf("  favorites=%d\n", n); Check(n >= 0, "ISteamMatchmaking008::GetFavoriteGameCount no crash"); }
    ISteamNetworking005* nw5 = (ISteamNetworking005*)findUser(0, "SteamNetworking005");
    Check(nw5 != nullptr, "SteamNetworking005 resolves");
    if (nw5) { uint32 sz = 0; bool avail = nw5->IsP2PPacketAvailable(&sz, 0); Check(!avail, "ISteamNetworking005::IsP2PPacketAvailable no crash (none pending)"); }
    ISteamApps008* ap8 = (ISteamApps008*)findUser(0, "STEAMAPPS_INTERFACE_VERSION008");
    Check(ap8 != nullptr, "STEAMAPPS_INTERFACE_VERSION008 resolves");
    ISteamUserStats011* us11 = (ISteamUserStats011*)findUser(0, "STEAMUSERSTATS_INTERFACE_VERSION011");
    Check(us11 != nullptr, "STEAMUSERSTATS_INTERFACE_VERSION011 resolves");
    ISteamRemoteStorage014* rs14 = (ISteamRemoteStorage014*)findUser(0, "STEAMREMOTESTORAGE_INTERFACE_VERSION014");
    Check(rs14 != nullptr, "STEAMREMOTESTORAGE_INTERFACE_VERSION014 resolves");

    // --- ISteamClient017 tree (SDK ~1.39, e.g. CS:GO 1.37) ---
    // CSteamAPIContext refuses to start without UnifiedMessages, and SteamController003 has its own vtable
    // (serving ISteamInput there crashed inputsystem.dll).
    ISteamClient017* c17 = (ISteamClient017*)create("SteamClient017");
    Check(c17 != nullptr, "SteamClient017 resolves");
    if (c17) {
        void* um = c17->DEPRECATED_GetISteamUnifiedMessages(user, pipe, "STEAMUNIFIEDMESSAGES_INTERFACE_VERSION001");
        Check(um != nullptr, "client017->GetISteamUnifiedMessages(001) non-null");
        if (um) Check(((ISteamUnifiedMessages*)um)->SendMethod("Test.Method#1", nullptr, 0, 0) == 0, "ISteamUnifiedMessages::SendMethod fails cleanly");
        ISteamController003* sc3 = (ISteamController003*)c17->GetISteamController(user, pipe, "SteamController003");
        Check(sc3 != nullptr, "client017->GetISteamController(SteamController003) non-null");
        if (sc3) {
            Check(sc3->Init(), "ISteamController003::Init");
            ControllerHandle_t handles[STEAM_CONTROLLER_MAX_COUNT] = {};
            Check(sc3->GetConnectedControllers(handles) == 0, "ISteamController003::GetConnectedControllers == 0");
            sc3->RunFrame();
        }
        auto* gc = (ISteamGameCoordinator*)c17->GetISteamGenericInterface(user, pipe, "SteamGameCoordinator001");
        Check(gc != nullptr, "client017->GetISteamGenericInterface(SteamGameCoordinator001) non-null");
        if (gc) {
            uint32 size = 1;
            Check(!gc->IsMessageAvailable(&size) && size == 0, "ISteamGameCoordinator::IsMessageAvailable false");
            Check(gc->SendMessage_(4006, "x", 1) == k_EGCResultOK, "ISteamGameCoordinator::SendMessage accepted");
        }
    }
    Check(GetProcAddress(h, "SteamGameServerInternal_CreateInterface") != nullptr, "SteamGameServerInternal_CreateInterface exported");

    // Native (latest) versions keep returning the native object.
    void* n23 = findUser(0, "SteamUser023");
    Check(n23 != nullptr && n23 == create("SteamUser023") && n23 != (void*)u19, "SteamUser023 stays native (not a versioned adapter)");

    printf(g_fail ? "FAIL (%d)\n" : "PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
