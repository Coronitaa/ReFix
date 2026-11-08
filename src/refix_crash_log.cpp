#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <atomic>
#include <cstdio>
#include <string>
#include "refix_crash_log.h"

#pragma comment(lib, "psapi.lib")

extern void ReFixLog(const char* fmt, ...);

namespace {

constexpr int kMaxLoggedCrashes = 3;  // a game that catches and retries must not flood the log
constexpr int kStackSlots = 512;      // pointer-sized stack slots scanned for return addresses
constexpr int kMaxFrames = 24;

std::atomic<int> g_logged{ 0 };
HMODULE g_self = nullptr;
bool g_logForeign = true;

bool IsFatal(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_PRIV_INSTRUCTION:
    case 0xC0000409: // STATUS_STACK_BUFFER_OVERRUN (fail-fast)
        return true;
    default:
        return false;
    }
}

// "module+0xoffset" for an address inside a loaded image, empty otherwise.
std::string Describe(const void* address) {
    HMODULE mod = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(address), &mod) || !mod)
        return {};
    char path[MAX_PATH] = {};
    GetModuleFileNameA(mod, path, MAX_PATH);
    const char* name = strrchr(path, '\\');
    name = name ? name + 1 : path;
    char buf[MAX_PATH + 32];
    snprintf(buf, sizeof(buf), "%s+0x%llx", name,
             (unsigned long long)((const char*)address - (const char*)mod));
    return buf;
}

bool InSelf(const void* address) {
    MODULEINFO info = {};
    if (!g_self || !GetModuleInformation(GetCurrentProcess(), g_self, &info, sizeof(info))) return false;
    const char* base = (const char*)info.lpBaseOfDll;
    return (const char*)address >= base && (const char*)address < base + info.SizeOfImage;
}

// Separate function: __try cannot share a frame with objects that need unwinding (std::string).
bool ReadSlot(const uintptr_t* slot, uintptr_t& value) {
    __try { value = *slot; return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Printable C string at p (>= 6 chars), for spotting error messages pushed on the stack.
bool ReadText(const char* p, char* out, size_t cap) {
    __try {
        size_t n = 0;
        for (; n + 1 < cap && p[n]; n++) {
            unsigned char c = (unsigned char)p[n];
            if (c < 0x20 && c != '\n' && c != '\t') return false;
            if (c > 0x7E) return false;
            out[n] = (char)(c == '\n' ? ' ' : c);
        }
        out[n] = 0;
        return n >= 6;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

LONG CALLBACK OnException(PEXCEPTION_POINTERS ep) {
    const EXCEPTION_RECORD* rec = ep->ExceptionRecord;
    // Probing the stack faults by design (guarded by __try); the vectored handler sees those first.
    static thread_local bool inHandler = false;
    if (inHandler || !IsFatal(rec->ExceptionCode)) return EXCEPTION_CONTINUE_SEARCH;
    if (!g_logForeign && !InSelf(rec->ExceptionAddress)) return EXCEPTION_CONTINUE_SEARCH;
    if (g_logged.fetch_add(1) >= kMaxLoggedCrashes) return EXCEPTION_CONTINUE_SEARCH;
    inHandler = true;

#ifdef _WIN64
    const uintptr_t sp = ep->ContextRecord->Rsp;
#else
    const uintptr_t sp = ep->ContextRecord->Esp;
#endif
    const uintptr_t target = rec->NumberParameters >= 2 ? (uintptr_t)rec->ExceptionInformation[1] : 0;
    ReFixLog("[Crash] Exception 0x%08lX at %s (thread %lu, %s address %p)", rec->ExceptionCode,
             Describe(rec->ExceptionAddress).c_str(), GetCurrentThreadId(),
             rec->NumberParameters >= 1 && rec->ExceptionInformation[0] ? "write" : "read", (void*)target);

    // Stack scan: every slot that points into a loaded image is a candidate return address. Not an exact
    // unwind, but enough to see which ReFix function and which game call led here.
    const uintptr_t* slot = reinterpret_cast<const uintptr_t*>(sp);
    int frames = 0;
    for (int i = 0; i < kStackSlots && frames < kMaxFrames; i++) {
        uintptr_t value = 0;
        if (!ReadSlot(slot + i, value)) break;
        if (value < 0x10000) continue;
        std::string where = Describe(reinterpret_cast<const void*>(value));
        if (g_logForeign) {
            char text[96] = {};
            if (ReadText(reinterpret_cast<const char*>(value), text, sizeof(text)))
                ReFixLog("[Crash]   stack[%d] \"%s\"", i, text);
        }
        if (where.empty()) continue;
        ReFixLog("[Crash]   stack[%d] %s", i, where.c_str());
        frames++;
    }
    inHandler = false;
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void ReFix_InstallCrashLog() {
    static std::atomic<bool> installed{ false };
    if (installed.exchange(true)) return;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&ReFix_InstallCrashLog), &g_self);
    char flag[8] = {};
    g_logForeign = GetEnvironmentVariableA("REFIX_CRASH_ALL", flag, sizeof(flag)) > 0 && flag[0] == '1';
    AddVectoredExceptionHandler(1, OnException);
}
