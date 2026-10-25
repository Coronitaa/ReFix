#pragma once

// Forwards a game's managed logger (static Log/LogWarning/LogError/LogWithoutStack(object) methods)
// into ReFix.log. Useful when the game encrypts or suppresses its own log output.
// Requires MinHook to be initialized and the IL2CPP domain to be attached.
namespace ReFixIL2CPPLogForward {
    // Hooks the one-argument logging methods of ns.className. Returns the number of methods hooked.
    int Install(const char* ns, const char* className);
}
