// Multi-instance support: several copies of the same game on one PC.
// Some Source/GoldSrc launchers (hl.exe, bin\launcher.dll) refuse a 2nd instance by holding a named
// "singleton" mutex. Once ReFix is running in this process we close our own handle to that mutex so a
// later launch's CreateMutex no longer reports ERROR_ALREADY_EXISTS.
#pragma once
#include <windows.h>

// Config from ReFix.ini [User] AllowMultipleInstances (the REFIX_ALLOW_MULTI_INSTANCE env var still
// overrides: "0" disables, anything else enables). Call before ReFix_MultiInstanceOnSteamInit.
void ReFix_MultiInstanceConfigure(bool allowFromIni);

// Call after a successful SteamAPI init. Releases known singleton mutexes now and again ~2 s later
// from a short-lived background thread. Safe to call repeatedly (only the first call arms the thread).
void ReFix_MultiInstanceOnSteamInit();

// Scans this process's handles once and closes Mutant handles whose name ends in a known singleton
// name. Returns the number of handles closed. Exposed for tests.
int ReFix_MultiInstanceReleaseNow();
