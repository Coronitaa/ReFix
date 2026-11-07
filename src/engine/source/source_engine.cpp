#include "source_engine.h"
#include "../../source_main_thread_queue.h"
#include <string>

typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

namespace refix::source {

SourceEngine& SourceEngine::Get() {
    static SourceEngine s_instance;
    return s_instance;
}

SourceEngine::SourceEngine() {
    refix::GetRefixServices().engine = this;
}

void SourceEngine::EnqueueCommand(const char* command) {
    if (!command || !command[0]) return;
    std::string cmd = command;
    refix::source::MainThreadQueue::Enqueue([cmd]() {
        // Find engine client and execute
        HMODULE hEngine = GetModuleHandleA("engine.dll");
        if (!hEngine) return;

        auto createInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hEngine, "CreateInterface"));
        if (!createInterface) return;

        void* engineClient = createInterface("VEngineClient014", nullptr);
        if (!engineClient) engineClient = createInterface("VEngineClient013", nullptr);
        if (!engineClient) return;

        // VEngineClient vtable index 108: ExecuteClientCmd
        void** vtable = *reinterpret_cast<void***>(engineClient);
        if (vtable && vtable[108]) {
            typedef void(__thiscall* ExecuteClientCmdFn)(void*, const char*);
            auto fn = reinterpret_cast<ExecuteClientCmdFn>(vtable[108]);
            fn(engineClient, cmd.c_str());
        }
    });
}

void* SourceEngine::FindInterface(const char* module_name, const char* interface_name) {
    if (!module_name || !interface_name) return nullptr;
    HMODULE hMod = GetModuleHandleA(module_name);
    if (!hMod) hMod = LoadLibraryA(module_name);
    if (!hMod) return nullptr;

    auto createInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hMod, "CreateInterface"));
    if (!createInterface) return nullptr;

    return createInterface(interface_name, nullptr);
}

} // namespace refix::source
