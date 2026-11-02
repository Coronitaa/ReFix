// Generic flat-API dispatch for the LAN emulator (tools/gen_steam_flat_dispatch.py).
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Latest-version emulator object for an SDK class name ("ISteamRemoteStorage"), or nullptr.
void* ReFixLatestInterface(const char* className);

// Generated thunk for a flat export name, or nullptr when there is none.
FARPROC ReFixFlatDispatchLookup(const char* name);
