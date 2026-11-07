#include "source_main_thread_queue.h"
#include <windows.h>

namespace refix::source {

void MainThreadQueue::ExecuteClientCmdImmediate(const char* cmd) {
    if (!cmd || !*cmd) return;
    HMODULE hEngine = GetModuleHandleA("engine.dll");
    if (!hEngine) return;
    typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
    auto factory = (CreateInterfaceFn)GetProcAddress(hEngine, "CreateInterface");
    if (!factory) return;
    void* engineClient = factory("VEngineClient014", nullptr);
    if (!engineClient) return;

    typedef void(__thiscall* ExecuteClientCmdFn)(void*, const char*);
    void** vtable = *(void***)engineClient;
    if (vtable && vtable[108]) {
        auto fn = (ExecuteClientCmdFn)vtable[108];
        fn(engineClient, cmd);
    }
}

} // namespace refix::source
