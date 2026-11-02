#include "refix_module_dedupe.h"
#include "minhook/MinHook.h"
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

const char* const kEnvName = "REFIX_PRIMARY_MODULE";
bool g_isSecondary = false;
bool g_isPrimary = false;
HANDLE g_primaryMutex = nullptr;
bool g_hooksInstalled = false;

// Format: "<pid>:<hex HMODULE>". Env is inherited by child processes, so the pid is always compared.
HMODULE ReadValidPrimary() {
    char buf[96] = {};
    DWORD n = GetEnvironmentVariableA(kEnvName, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) return nullptr;
    unsigned long pid = 0;
    unsigned long long hm = 0;
    if (sscanf_s(buf, "%lu:%llx", &pid, &hm) != 2) return nullptr;
    if (pid != GetCurrentProcessId() || hm == 0) return nullptr;
    HMODULE h = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(hm));
    char path[MAX_PATH];
    if (GetModuleFileNameA(h, path, MAX_PATH) == 0) return nullptr;   // unloaded / dead
    if (!GetProcAddress(h, "ReFix_ModuleMarker")) return nullptr;      // not a ReFix module
    return h;
}

// Same file/format as ReFixLog (exe dir ReFix.log); standalone so the secondary never touches config.
void DedupeLog(const char* fmt, ...) {
    char msg[2048];
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);
    va_end(args);
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0) return;
    std::string logPath(exePath);
    size_t pos = logPath.find_last_of("\\/");
    if (pos == std::string::npos) return;
    logPath = logPath.substr(0, pos + 1) + "ReFix.log";
    FILE* f = nullptr;
    fopen_s(&f, logPath.c_str(), "a");
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [steam_api64] %s\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
    fclose(f);
}

bool IsExecutableRva(const IMAGE_NT_HEADERS* nt, DWORD rva) {
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        DWORD size = sec->Misc.VirtualSize > sec->SizeOfRawData ? sec->Misc.VirtualSize : sec->SizeOfRawData;
        if (rva >= sec->VirtualAddress && rva < sec->VirtualAddress + size)
            return (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    }
    return false;
}

// Redirects every named code export of `self` to the primary's same-named export.
void RedirectExports(HMODULE self, HMODULE primary, int& redirected, int& unhookable) {
    auto base = reinterpret_cast<BYTE*>(self);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dd.VirtualAddress == 0) return;
    auto exp = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base + dd.VirtualAddress);
    auto names = reinterpret_cast<DWORD*>(base + exp->AddressOfNames);
    auto ords = reinterpret_cast<WORD*>(base + exp->AddressOfNameOrdinals);
    auto funcs = reinterpret_cast<DWORD*>(base + exp->AddressOfFunctions);

    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return;

    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const char* name = reinterpret_cast<const char*>(base + names[i]);
        if (strcmp(name, "ReFix_ModuleMarker") == 0) continue;
        DWORD rva = funcs[ords[i]];
        if (rva >= dd.VirtualAddress && rva < dd.VirtualAddress + dd.Size) continue;  // forwarded
        if (!IsExecutableRva(nt, rva)) continue;                                      // data export
        FARPROC theirs = GetProcAddress(primary, name);
        void* ours = base + rva;
        if (!theirs || reinterpret_cast<void*>(theirs) == ours) continue;
        MH_STATUS st = MH_CreateHook(ours, reinterpret_cast<void*>(theirs), nullptr);
        if (st == MH_OK || st == MH_ERROR_ALREADY_CREATED) ++redirected;  // ALREADY_CREATED: alias of a hooked function
        else ++unhookable;
    }
    if (redirected > 0 && MH_EnableHook(MH_ALL_HOOKS) == MH_OK) g_hooksInstalled = true;
}

}  // namespace

extern "C" __declspec(dllexport) int ReFix_ModuleMarker() { return 1; }

bool ReFix_ModuleDedupeAttach(HMODULE self) {
    HMODULE primary = ReadValidPrimary();
    if (primary && primary != self) {
        g_isSecondary = true;
        int redirected = 0, unhookable = 0;
        RedirectExports(self, primary, redirected, unhookable);
        char selfPath[MAX_PATH] = {}, primPath[MAX_PATH] = {};
        GetModuleFileNameA(self, selfPath, MAX_PATH);
        GetModuleFileNameA(primary, primPath, MAX_PATH);
        DedupeLog("[ModuleDedupe] Secondary ReFix copy %s redirected %d exports to primary %s (%d unhookable)",
                  selfPath, redirected, primPath, unhookable);
        return true;
    }
    // First ReFix module in this process: become primary.
    char name[64];
    sprintf_s(name, "Local\\ReFix_PrimaryModule_%lu", GetCurrentProcessId());
    g_primaryMutex = CreateMutexA(nullptr, FALSE, name);  // not inheritable; dies with this process
    char val[96];
    sprintf_s(val, "%lu:%llx", GetCurrentProcessId(), static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(self)));
    SetEnvironmentVariableA(kEnvName, val);
    g_isPrimary = true;
    return false;
}

bool ReFix_ModuleDedupeDetach(HMODULE self, bool processTerminating) {
    if (g_isSecondary) {
        if (!processTerminating && g_hooksInstalled) {
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            g_hooksInstalled = false;
        }
        return true;
    }
    if (g_isPrimary) {
        HMODULE cur = ReadValidPrimary();
        if (cur == self) SetEnvironmentVariableA(kEnvName, nullptr);
        if (g_primaryMutex) { CloseHandle(g_primaryMutex); g_primaryMutex = nullptr; }
        g_isPrimary = false;
    }
    return false;
}
