// Verifies "one emulator per process": two copies of the ReFix steam_api64.dll loaded from
// different folders must share a single emulator (same ISteamUser pointer, one instance slot).
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static int g_fail = 0;
static void Check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++g_fail;
}

static std::string ExeDir() {
    char p[MAX_PATH];
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s(p);
    return s.substr(0, s.find_last_of("\\/") + 1);
}

static bool Stage(const std::string& src, const std::string& dir) {
    CreateDirectoryA(dir.c_str(), nullptr);
    if (!CopyFileA(src.c_str(), (dir + "\\steam_api64.dll").c_str(), FALSE)) return false;
    std::ofstream ini(dir + "\\ReFix.ini");
    ini << "[Network]\nMode=force_lan\n";
    return ini.good();
}

static std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static int CountOf(const std::string& hay, const std::string& needle) {
    int n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) ++n;
    return n;
}

typedef bool (*InitFn)();
typedef void* (*GetUserFn)();
typedef unsigned long long (*GetIdFn)(void*);

int main(int argc, char** argv) {
    std::string dll = argc > 1 ? argv[1] : ExeDir() + "lan\\steam_api64.dll";
    std::string root = ExeDir() + "dedupe_test_tmp";
    std::string a = root + "\\A", b = root + "\\B";
    CreateDirectoryA(root.c_str(), nullptr);
    printf("test_module_dedupe: source dll %s\n", dll.c_str());
    if (!Stage(dll, a) || !Stage(dll, b)) { printf("FAIL: cannot stage dll copies\n"); return 2; }

    std::string logPath = ExeDir() + "ReFix.log";
    DeleteFileA(logPath.c_str());

    HMODULE hA = LoadLibraryA((a + "\\steam_api64.dll").c_str());
    HMODULE hB = LoadLibraryA((b + "\\steam_api64.dll").c_str());
    Check(hA && hB, "both copies loaded");
    Check(hA != hB, "distinct HMODULEs");
    if (!hA || !hB) { printf("FAIL\n"); return 1; }

    auto initB = (InitFn)GetProcAddress(hB, "SteamAPI_Init");
    Check(initB != nullptr, "SteamAPI_Init export present in B");
    bool inited = initB && initB();
    Check(inited, "SteamAPI_Init (via B) returned true");

    auto userA = (GetUserFn)GetProcAddress(hA, "SteamAPI_SteamUser_v023");
    auto userB = (GetUserFn)GetProcAddress(hB, "SteamAPI_SteamUser_v023");
    auto idA = (GetIdFn)GetProcAddress(hA, "SteamAPI_ISteamUser_GetSteamID");
    auto idB = (GetIdFn)GetProcAddress(hB, "SteamAPI_ISteamUser_GetSteamID");
    Check(userA && userB && idA && idB, "SteamUser exports resolved in A and B");
    if (userA && userB && idA && idB) {
        void* ua = userA();
        void* ub = userB();
        printf("  SteamUser A=%p B=%p\n", ua, ub);
        Check(ua != nullptr && ua == ub, "ISteamUser pointers identical (single emulator)");
        unsigned long long sa = idA(ua), sb = idB(ub);
        printf("  SteamID A=%llu B=%llu\n", sa, sb);
        Check(sa != 0 && sa == sb, "SteamIDs equal and non-zero");
    }

    std::string log = ReadFile(logPath);
    int slots = CountOf(log, "Multi-Instance Slot:");
    int dedupe = CountOf(log, "[ModuleDedupe] Secondary ReFix copy");
    printf("  log: Multi-Instance Slot lines=%d, ModuleDedupe lines=%d\n", slots, dedupe);
    Check(slots == 1, "exactly one 'Multi-Instance Slot:' logged");
    Check(dedupe == 1, "exactly one secondary redirect logged");

    printf(g_fail ? "FAIL: test_module_dedupe\n" : "PASS: test_module_dedupe\n");
    return g_fail ? 1 : 0;
}
