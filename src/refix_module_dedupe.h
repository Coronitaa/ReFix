// One ReFix emulator per process: when a game loads several copies of the ReFix
// steam_api(64).dll (different folders => different modules), only the first copy
// ("primary") runs the emulator. Later copies ("secondary") redirect their exports
// to the primary's same-named exports and skip all other initialization.
#pragma once
#include <windows.h>

// Call first thing in DLL_PROCESS_ATTACH. Returns true if this module is a secondary copy
// (exports redirected; the caller must return TRUE from DllMain without any other init).
bool ReFix_ModuleDedupeAttach(HMODULE self);

// Call in DLL_PROCESS_DETACH. Returns true if this module was a secondary (caller skips its
// normal teardown). Primary: clears the process-wide primary marker.
bool ReFix_ModuleDedupeDetach(HMODULE self, bool processTerminating);
