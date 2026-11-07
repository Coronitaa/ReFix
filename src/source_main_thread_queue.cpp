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


std::string GetConVarString(const char* cvarName) {
    if (!cvarName || !*cvarName) return "";
    HMODULE hVstdlib = GetModuleHandleA("vstdlib.dll");
    if (!hVstdlib) hVstdlib = LoadLibraryA("vstdlib.dll");
    if (!hVstdlib) return "";
    typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
    auto factory = (CreateInterfaceFn)GetProcAddress(hVstdlib, "CreateInterface");
    if (!factory) return "";
    void* pCvar = factory("VEngineCvar007", nullptr);
    if (!pCvar) return "";

    void** vtable = *(void***)pCvar;
    if (!vtable || !vtable[16]) return "";

    typedef void*(__thiscall* FindVarFn)(void*, const char*);
    auto findVar = (FindVarFn)vtable[16];
    void* pConVar = findVar(pCvar, cvarName);
    if (!pConVar) return "";

    void** varVtable = *(void***)pConVar;
    if (varVtable && varVtable[14]) {
        typedef const char*(__thiscall* GetStringFn)(void*);
        auto getString = (GetStringFn)varVtable[14];
        const char* val = getString(pConVar);
        if (val) return std::string(val);
    }
    const char* strPtr = *(const char**)((uint8_t*)pConVar + 0x24);
    if (strPtr) return std::string(strPtr);

    return "";
}

} // namespace refix::source
