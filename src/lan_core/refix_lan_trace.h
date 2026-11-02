#pragma once
// Env-gated diagnostics for LanCore (REFIX_LAN_TRACE=1): one atomic append per line to ReFix.log next to the exe.
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace refix::lan {

inline bool LanTraceEnabled() {
    static const bool on = []() {
        char v[8] = {};
        return GetEnvironmentVariableA("REFIX_LAN_TRACE", v, sizeof(v)) > 0 && v[0] == '1';
    }();
    return on;
}

inline void LanTrace(const char* fmt, ...) {
    if (!LanTraceEnabled()) return;
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char* slash = strrchr(exePath, '\\');
    if (slash) slash[1] = 0;
    strcat_s(exePath, "ReFix.log");
    char line[640];
    int n = snprintf(line, sizeof(line), "[LanTrace pid=%lu tid=%lu] %s\n", GetCurrentProcessId(), GetCurrentThreadId(), msg);
    if (n <= 0) return;
    HANDLE hf = CreateFileA(exePath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(hf, line, static_cast<DWORD>(n), &w, nullptr);
    CloseHandle(hf);
}

} // namespace refix::lan
