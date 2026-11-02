// Verifies that ReFix LAN closes the process's own handle to known singleton mutexes (so a 2nd game
// instance can start) and leaves every other named mutex alone.
//   parent: holds "hl2_singleton_mutex" + "ValveHalfLifeLauncherMutex" + an unrelated mutex, loads the
//           ReFix LAN steam_api64.dll from a temp copy, SteamAPI_Init, waits 3 s, then spawns children.
//   child "--probe-singleton": CreateMutex(hl2_singleton_mutex) must NOT report ALREADY_EXISTS.
//   child "--probe-other":     CreateMutex(unrelated) MUST report ALREADY_EXISTS (it was not closed).
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

static const char* kSingleton = "hl2_singleton_mutex";
static const char* kGoldSrc = "ValveHalfLifeLauncherMutex";
static const char* kOther = "ReFixMultiInstanceTest_Unrelated_Mutex";

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

// Returns the child's exit code, or -1 on spawn failure.
static int RunChild(const char* mode) {
    char self[MAX_PATH];
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::string cmd = std::string("\"") + self + "\" " + mode;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, 20000);
    DWORD code = 0xFFFFFFFF;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

static int Probe(const char* name, bool expectExists) {
    HANDLE h = CreateMutexA(nullptr, TRUE, name);
    DWORD err = GetLastError();
    bool exists = (h != nullptr && err == ERROR_ALREADY_EXISTS);
    printf("  child: CreateMutex('%s') -> handle=%p already_exists=%d\n", name, (void*)h, exists ? 1 : 0);
    return (h != nullptr && exists == expectExists) ? 0 : 1;
}

typedef bool (*InitFn)();

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "--probe-singleton") == 0) return Probe(kSingleton, false);
    if (argc > 1 && strcmp(argv[1], "--probe-goldsrc") == 0) return Probe(kGoldSrc, false);
    if (argc > 1 && strcmp(argv[1], "--probe-other") == 0) return Probe(kOther, true);

    std::string dll = argc > 1 ? argv[1] : ExeDir() + "lan\\steam_api64.dll";
    std::string dir = ExeDir() + "multi_instance_test_tmp";
    CreateDirectoryA(dir.c_str(), nullptr);
    printf("test_multi_instance: source dll %s\n", dll.c_str());
    if (!CopyFileA(dll.c_str(), (dir + "\\steam_api64.dll").c_str(), FALSE)) { printf("FAIL: cannot stage dll\n"); return 2; }
    { std::ofstream ini(dir + "\\ReFix.ini"); ini << "[Network]\nMode=force_lan\n"; }
    DeleteFileA((ExeDir() + "ReFix.log").c_str());

    HANDLE hSingleton = CreateMutexA(nullptr, TRUE, kSingleton);
    HANDLE hGoldSrc = CreateMutexA(nullptr, TRUE, kGoldSrc);
    HANDLE hOther = CreateMutexA(nullptr, TRUE, kOther);
    Check(hSingleton && hGoldSrc && hOther, "parent created the three named mutexes");
    Check(RunChild("--probe-singleton") == 0 ? false : true, "before ReFix: child sees the singleton as taken");

    HMODULE h = LoadLibraryA((dir + "\\steam_api64.dll").c_str());
    Check(h != nullptr, "ReFix LAN steam_api64.dll loaded");
    auto init = h ? (InitFn)GetProcAddress(h, "SteamAPI_Init") : nullptr;
    Check(init && init(), "SteamAPI_Init returned true");
    Sleep(3000);

    Check(RunChild("--probe-singleton") == 0, "after ReFix: child can create hl2_singleton_mutex (2nd instance allowed)");
    Check(RunChild("--probe-goldsrc") == 0, "after ReFix: child can create ValveHalfLifeLauncherMutex");
    Check(RunChild("--probe-other") == 0, "unrelated named mutex still held (not closed)");
    DWORD s = WaitForSingleObject(hOther, 0);
    Check(s == WAIT_OBJECT_0, "parent still owns the unrelated mutex handle");

    printf(g_fail ? "FAIL (%d)\n" : "PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
