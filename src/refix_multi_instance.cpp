#include "refix_multi_instance.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

extern void ReFixLog(const char* fmt, ...);

namespace {

// Object-name suffixes (after the last '\') of the known singleton mutexes. Only these are ever closed.
//  - ValveHalfLifeLauncherMutex: GoldSrc hl.exe (Counter-Strike 1.6, HL1 mods)
//  - hl2_singleton_mutex:        Source bin\launcher.dll (L4D, Garry's Mod, HL2 family)
constexpr const wchar_t* kSingletonNames[] = {
    L"ValveHalfLifeLauncherMutex",
    L"hl2_singleton_mutex",
};
// Substrings of singleton mutex names that embed per-game text.
//  - -SingleInstanceMutex-: UnityPlayer.dll with Player Settings "Force Single Instance"
//                           (name is built from the company and product names)
constexpr const wchar_t* kSingletonFragments[] = {
    L"-SingleInstanceMutex-",
};
constexpr DWORD kRescanDelayMs = 2000;

constexpr LONG kStatusInfoLengthMismatch = (LONG)0xC0000004L;
constexpr LONG kStatusBufferOverflow = (LONG)0x80000005L;
constexpr LONG kStatusBufferTooSmall = (LONG)0xC0000023L;
constexpr ULONG kProcessHandleInformation = 51;
constexpr ULONG kObjectNameInformation = 1;
constexpr ULONG kObjectTypeInformation = 2;

struct NtUnicodeString { USHORT Length; USHORT MaximumLength; PWSTR Buffer; };
// OBJECT_NAME_INFORMATION and the head of OBJECT_TYPE_INFORMATION both start with a UNICODE_STRING.
struct NtNameInfo { NtUnicodeString Name; };

struct NtProcessHandleEntry {
    HANDLE HandleValue;
    ULONG_PTR HandleCount;
    ULONG_PTR PointerCount;
    ULONG GrantedAccess;
    ULONG ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
};
struct NtProcessHandleSnapshot {
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    NtProcessHandleEntry Handles[1];
};

typedef LONG(NTAPI* NtQueryObject_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef LONG(NTAPI* NtQueryInformationProcess_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);

std::atomic<bool> g_armed{false};
std::atomic<int> g_configured{-1};   // -1 = unset, 0/1 = ini value

bool EnvOverride(bool& value) {
    char buf[16];
    DWORD n = GetEnvironmentVariableA("REFIX_ALLOW_MULTI_INSTANCE", buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) return false;
    value = !(strcmp(buf, "0") == 0 || _stricmp(buf, "false") == 0 || _stricmp(buf, "no") == 0 || _stricmp(buf, "off") == 0);
    return true;
}

bool IsEnabled() {
    bool v = true;
    if (EnvOverride(v)) return v;
    int c = g_configured.load();
    if (c >= 0) return c != 0;
#ifdef REFIX_LAN_EDITION
    return true;
#else
    return false;
#endif
}

bool QueryObjectUnicode(NtQueryObject_t fn, HANDLE h, ULONG cls, std::wstring& out) {
    std::vector<BYTE> buf(512);
    for (int attempt = 0; attempt < 3; ++attempt) {
        ULONG need = 0;
        LONG st = fn(h, cls, buf.data(), (ULONG)buf.size(), &need);
        if (st >= 0) {
            const NtNameInfo* ni = reinterpret_cast<const NtNameInfo*>(buf.data());
            if (!ni->Name.Buffer || ni->Name.Length == 0) { out.clear(); return true; }
            out.assign(ni->Name.Buffer, ni->Name.Length / sizeof(wchar_t));
            return true;
        }
        if (st != kStatusInfoLengthMismatch && st != kStatusBufferOverflow && st != kStatusBufferTooSmall) return false;
        buf.resize(need > buf.size() ? need + 64 : buf.size() * 2);
    }
    return false;
}

bool IsKnownSingleton(const std::wstring& objectName, const wchar_t** matched) {
    size_t slash = objectName.find_last_of(L'\\');
    const wchar_t* leaf = objectName.c_str() + (slash == std::wstring::npos ? 0 : slash + 1);
    for (const wchar_t* known : kSingletonNames) {
        if (_wcsicmp(leaf, known) == 0) { *matched = known; return true; }
    }
    for (const wchar_t* fragment : kSingletonFragments) {
        if (wcsstr(leaf, fragment)) { *matched = fragment; return true; }
    }
    return false;
}

struct Resolver {
    NtQueryObject_t queryObject = nullptr;
    NtQueryInformationProcess_t queryProcess = nullptr;
    bool Init() {
        HMODULE nt = GetModuleHandleW(L"ntdll.dll");
        if (!nt) return false;
        queryObject = reinterpret_cast<NtQueryObject_t>(GetProcAddress(nt, "NtQueryObject"));
        queryProcess = reinterpret_cast<NtQueryInformationProcess_t>(GetProcAddress(nt, "NtQueryInformationProcess"));
        return queryObject != nullptr;
    }
    // Type names are safe to query for any handle (only name queries can block on pipes/files).
    bool IsMutantHandle(HANDLE h) {
        std::wstring type;
        return QueryObjectUnicode(queryObject, h, kObjectTypeInformation, type) && type == L"Mutant";
    }
};

// Handles of this process via ProcessHandleInformation (Windows 8+). Empty on failure.
bool SnapshotHandles(Resolver& r, std::vector<NtProcessHandleEntry>& out) {
    if (!r.queryProcess) return false;
    std::vector<BYTE> buf(64 * 1024);
    for (int attempt = 0; attempt < 8; ++attempt) {
        ULONG need = 0;
        LONG st = r.queryProcess(GetCurrentProcess(), kProcessHandleInformation, buf.data(), (ULONG)buf.size(), &need);
        if (st >= 0) {
            const NtProcessHandleSnapshot* snap = reinterpret_cast<const NtProcessHandleSnapshot*>(buf.data());
            out.assign(snap->Handles, snap->Handles + snap->NumberOfHandles);
            return true;
        }
        if (st != kStatusInfoLengthMismatch) return false;
        buf.resize(need > buf.size() ? need + 4096 : buf.size() * 2);
    }
    return false;
}

DWORD WINAPI RescanThread(LPVOID module) {
    Sleep(kRescanDelayMs);
    int n = ReFix_MultiInstanceReleaseNow();
    if (n == 0) ReFixLog("[MultiInstance] Delayed rescan: no singleton mutex found");
    FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
}

} // namespace

void ReFix_MultiInstanceConfigure(bool allowFromIni) {
    g_configured.store(allowFromIni ? 1 : 0);
}

int ReFix_MultiInstanceReleaseNow() {
    if (!IsEnabled()) return 0;
    Resolver r;
    if (!r.Init()) { ReFixLog("[MultiInstance] ntdll unavailable"); return 0; }
    int closed = 0;
    auto tryClose = [&](HANDLE h) {
        if (!r.IsMutantHandle(h)) return;
        std::wstring name;
        if (!QueryObjectUnicode(r.queryObject, h, kObjectNameInformation, name)) return;
        const wchar_t* matched = nullptr;
        if (!IsKnownSingleton(name, &matched)) return;
        if (CloseHandle(h)) {
            ++closed;
            ReFixLog("[MultiInstance] Released singleton mutex '%ls' (rule '%ls')", name.c_str(), matched);
        }
    };
    std::vector<NtProcessHandleEntry> handles;
    if (SnapshotHandles(r, handles)) {
        for (const auto& e : handles) tryClose(e.HandleValue);
    } else {
        // Pre-Win8 fallback: probe handle values (multiples of 4) directly.
        DWORD flags = 0;
        for (ULONG_PTR v = 4; v < 0x40000; v += 4) {
            HANDLE h = reinterpret_cast<HANDLE>(v);
            if (GetHandleInformation(h, &flags)) tryClose(h);
        }
    }
    if (closed == 0) ReFixLog("[MultiInstance] No singleton mutex found");
    return closed;
}

void ReFix_MultiInstanceOnSteamInit() {
    if (!IsEnabled()) {
        ReFixLog("[MultiInstance] Disabled (AllowMultipleInstances=false or REFIX_ALLOW_MULTI_INSTANCE=0)");
        return;
    }
    ReFix_MultiInstanceReleaseNow();
    bool expected = false;
    if (!g_armed.compare_exchange_strong(expected, true)) return;
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCSTR>(&RescanThread), &self)) return;
    HANDLE t = CreateThread(nullptr, 0, RescanThread, self, 0, nullptr);
    if (t) CloseHandle(t);
    else FreeLibrary(self);
}
