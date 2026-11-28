# ReFix v2.6.6

ReFix v2.6.6 fixes a critical recursive process-spawning loop on Microsoft Store / GDK titles, resolves exit deadlocks that left game and helper processes lingering in Task Manager, extends the `0x800705b4` launch error watchdog to WinGDK game executables, and expands XStore DLC hook detection.

---

## Key Changes

### Process Isolation & Anti-Recursion Guards
* **Proxy Exclusion (`winhttp.dll` / `winmm.dll`)**: Added `IsHelperOrToolProcess` filtering for `refix_steamhost*`, `refixfusionshared.exe`, `refixmod.exe`, `gamelaunchhelper.exe`, `eosbootstrapper.exe`, and crash handlers, plus `REFIX_HELPER_PROCESS=1` / `REFIX_NO_BOOTSTRAP=1` environment checks so helper processes act strictly as passthroughs without loading `steam_api64.dll`.
* **Emulator Guard (`steam_api64.dll` / `steam_api.dll`)**: Added early exit guards in `DllMain`, `ReFixInitializePre`, `SteamAPI_Init`, and `SteamAPI_InitSafe` when running inside a helper or tool process.
* **Host Environment & Overlay (`steam_link.cpp` / `steam_host.cpp` / `overlay_app.cpp`)**: Injected `REFIX_HELPER_PROCESS=1`, `REFIX_NO_BOOTSTRAP=1`, and `REFIX_NO_OVERLAY=1` into helper environments and stopped triggering redundant license probes when Steam is signed out (`newSteamId == 0`).

### Clean Process Exit (Task Manager & `0x800705b4` Watchdog)
* **Exit Deadlock Fix (`refix_lan_transport.cpp` / `steam_proxy.cpp`)**: Replaced blocking `Stop()` / `.join()` calls inside `LanTransport::~LanTransport()` with non-blocking `detach()` and socket closure to prevent Windows OS loader-lock deadlocks during `ExitProcess`. Moved `ForceTerminateHelpersOnExit()` to run first in `DLL_PROCESS_DETACH`.
* **WinGDK / Microsoft Store `0x800705b4` Watchdog (`winhttp_proxy.cpp`)**: Extended `MsStoreLaunchWatchdogThread` to launch from WinGDK / Microsoft Store game processes (in addition to `gamelaunchhelper.exe`), scanning `GamingServicesUI.exe`, `ApplicationFrameHost.exe`, `XboxPcApp.exe`, `XboxPcAppFT.exe`, and `XboxAppServices.exe` to automatically dismiss or terminate lingering "already running" error dialogs.

### Microsoft Store DLC Entitlements (`xstore_hook.cpp`)
* Added `XGameRuntime.dll` alongside `XGamingRuntimeThunks.dll` and `XGamingRuntime.dll` for `XStore*` API hooking, and added diagnostic logging when `xgameruntime.dll` is loaded with static GDK thunks.
