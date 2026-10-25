#include "refix_il2cpp_logfwd.h"
#include "refix_il2cpp_scene.h"
#include "minhook/MinHook.h"
#include <windows.h>
#include <string>

extern void ReFixLog(const char* fmt, ...);

namespace ReFixIL2CPPLogForward {

namespace {

typedef void (*fn_Log_t)(void* message, void* method);

struct LogTarget {
    const char* ns;         // nullptr: the game logger class passed to Install()
    const char* className;
    const char* methodName;
    const char* level;
    fn_Log_t original;
};

LogTarget g_targets[] = {
    { nullptr, nullptr, "Log", "info", nullptr },
    { nullptr, nullptr, "LogWithoutStack", "info", nullptr },
    { nullptr, nullptr, "LogWarning", "warn", nullptr },
    { nullptr, nullptr, "LogError", "error", nullptr },
    { nullptr, nullptr, "LogException", "exception", nullptr },
    // Unobserved async (UniTask) exceptions end up here even when the game logger is bypassed.
    { "UnityEngine", "Debug", "LogException", "exception", nullptr },
};

constexpr size_t kMaxForwardedChars = 960; // ReFixLog formats into a 1024-byte buffer

// One log line per message: flatten newlines (HTTP bodies, stack traces) and cap the length.
std::string Flatten(std::string text) {
    for (char& c : text) {
        if (c == '\r' || c == '\n') c = ' ';
    }
    if (text.size() > kMaxForwardedChars) text = text.substr(0, kMaxForwardedChars) + "...";
    return text;
}

template <int Index>
void Hooked_Log(void* message, void* method) {
    ReFixLog("[Game:%s] %s", g_targets[Index].level, Flatten(ReFixIL2CPPScene::ObjectToDisplayString(message)).c_str());
    if (g_targets[Index].original) g_targets[Index].original(message, method);
}

void* const g_hooks[] = {
    (void*)&Hooked_Log<0>,
    (void*)&Hooked_Log<1>,
    (void*)&Hooked_Log<2>,
    (void*)&Hooked_Log<3>,
    (void*)&Hooked_Log<4>,
    (void*)&Hooked_Log<5>,
};

static_assert(sizeof(g_hooks) / sizeof(g_hooks[0]) == sizeof(g_targets) / sizeof(g_targets[0]),
              "one hook per log target");

} // namespace

int Install(const char* ns, const char* className) {
    int hooked = 0;
    for (size_t i = 0; i < sizeof(g_targets) / sizeof(g_targets[0]); i++) {
        LogTarget& t = g_targets[i];
        const char* targetNs = t.ns ? t.ns : ns;
        const char* targetClass = t.className ? t.className : className;
        void* klass = ReFixIL2CPPScene::FindClassByName(targetNs, targetClass);
        void* code = klass ? ReFixIL2CPPScene::GetMethodCode(klass, t.methodName, 1) : nullptr;
        if (!code) continue;
        if (MH_CreateHook(code, g_hooks[i], (void**)&t.original) == MH_OK && MH_EnableHook(code) == MH_OK) {
            hooked++;
        } else {
            ReFixLog("[LogForward] Could not hook %s.%s::%s at %p", targetNs, targetClass, t.methodName, code);
        }
    }
    ReFixLog("[LogForward] Forwarding %d logging methods (%s.%s + UnityEngine.Debug.LogException) into ReFix.log",
             hooked, ns, className);
    return hooked;
}

} // namespace ReFixIL2CPPLogForward
