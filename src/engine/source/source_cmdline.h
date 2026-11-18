// Source engine command line (tier0.dll CommandLine()): read it and add parameters before the engine reads them.
// Shared by ReFix (multi-instance window mode) and game mods (e.g. -maxplayers_override). Header-only so a mod DLL
// can use it without linking ReFix.
#pragma once
#include <windows.h>

namespace refix::source {

// ICommandLine vtable: [2] GetCmdLine, [6] AppendParm(parm, values). Same layout in every Source build since 2013.
inline void* Tier0CommandLine() {
    HMODULE tier0 = GetModuleHandleA("tier0.dll");
    using CommandLineFn = void* (__cdecl*)();
    auto getCmdLine = tier0 ? reinterpret_cast<CommandLineFn>(GetProcAddress(tier0, "CommandLine")) : nullptr;
    void* cmdLine = getCmdLine ? getCmdLine() : nullptr;
    void** vtable = cmdLine ? *reinterpret_cast<void***>(cmdLine) : nullptr;
    return vtable && vtable[2] && vtable[6] ? cmdLine : nullptr;
}

// The game's full command line, or null outside Source.
inline const char* GetGameCommandLine() {
    void* cmdLine = Tier0CommandLine();
    if (!cmdLine) return nullptr;
    using GetCmdLineFn = const char*(__thiscall*)(void*);
    return reinterpret_cast<GetCmdLineFn>((*reinterpret_cast<void***>(cmdLine))[2])(cmdLine);
}

// Adds (or replaces) "parm value" on the game's command line. False outside Source.
inline bool AppendGameParm(const char* parm, const char* value) {
    void* cmdLine = Tier0CommandLine();
    if (!cmdLine) return false;
    using AppendParmFn = void(__thiscall*)(void*, const char*, const char*);
    reinterpret_cast<AppendParmFn>((*reinterpret_cast<void***>(cmdLine))[6])(cmdLine, parm, value);
    return true;
}

} // namespace refix::source
