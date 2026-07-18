// =============================================================================
// ReFix - winmm.dll Proxy (Loader + Config)
// =============================================================================
// Clean DLL proxy for winmm.dll. This is the first ReFix DLL loaded by Windows
// (due to DLL search order - local directory before System32).
//
// Responsibilities:
//   1. Load the real winmm.dll from System32 and populate the jump table
//   2. Read ReFix.ini and set Steam/EOS environment variables
//   3. Provide the ReFix marker export
//
// The actual function forwarding is done by assembly trampolines in winmm_fwd.asm
// that jump through the g_winmmProcs table populated here.
//
// This DLL contains NO:
//   - CreateToolhelp32Snapshot, OpenThread, SuspendThread, ResumeThread
//   - GetThreadContext, SetThreadContext
//   - VirtualProtect, FlushInstructionCache
//   - Any network, shell execution, or code injection APIs
//
// Build: ml64 /c winmm_fwd.asm
//        cl /LD /O2 /EHsc winmm_proxy.cpp winmm_fwd.obj /Fe:winmm.dll /link /DEF:winmm_proxy.def
// =============================================================================

#include <windows.h>
#include <string>

// =============================================================================
// FUNCTION POINTER TABLE — shared with the ASM trampolines
// =============================================================================
// The ASM trampolines (winmm_fwd.asm) jump through this table.
// We populate it at load time with addresses from the real system winmm.dll.
// Must be 'extern "C"' so the ASM can reference it by name.

#define WINMM_EXPORT_COUNT 180

extern "C" {
    __declspec(dllexport) FARPROC g_winmmProcs[WINMM_EXPORT_COUNT] = { 0 };
}

// Names of all forwarded functions (matching the order in the ASM file)
static const char* g_exportNames[WINMM_EXPORT_COUNT] = {
    "CloseDriver",
    "DefDriverProc",
    "DriverCallback",
    "DrvGetModuleHandle",
    "GetDriverModuleHandle",
    "OpenDriver",
    "PlaySound",
    "PlaySoundA",
    "PlaySoundW",
    "SendDriverMessage",
    "WOWAppExit",
    "auxGetDevCapsA",
    "auxGetDevCapsW",
    "auxGetNumDevs",
    "auxGetVolume",
    "auxOutMessage",
    "auxSetVolume",
    "joyConfigChanged",
    "joyGetDevCapsA",
    "joyGetDevCapsW",
    "joyGetNumDevs",
    "joyGetPos",
    "joyGetPosEx",
    "joyGetThreshold",
    "joyReleaseCapture",
    "joySetCapture",
    "joySetThreshold",
    "mciDriverNotify",
    "mciDriverYield",
    "mciExecute",
    "mciFreeCommandResource",
    "mciGetCreatorTask",
    "mciGetDeviceIDA",
    "mciGetDeviceIDFromElementIDA",
    "mciGetDeviceIDFromElementIDW",
    "mciGetDeviceIDW",
    "mciGetDriverData",
    "mciGetErrorStringA",
    "mciGetErrorStringW",
    "mciGetYieldProc",
    "mciLoadCommandResource",
    "mciSendCommandA",
    "mciSendCommandW",
    "mciSendStringA",
    "mciSendStringW",
    "mciSetDriverData",
    "mciSetYieldProc",
    "midiConnect",
    "midiDisconnect",
    "midiInAddBuffer",
    "midiInClose",
    "midiInGetDevCapsA",
    "midiInGetDevCapsW",
    "midiInGetErrorTextA",
    "midiInGetErrorTextW",
    "midiInGetID",
    "midiInGetNumDevs",
    "midiInMessage",
    "midiInOpen",
    "midiInPrepareHeader",
    "midiInReset",
    "midiInStart",
    "midiInStop",
    "midiInUnprepareHeader",
    "midiOutCacheDrumPatches",
    "midiOutCachePatches",
    "midiOutClose",
    "midiOutGetDevCapsA",
    "midiOutGetDevCapsW",
    "midiOutGetErrorTextA",
    "midiOutGetErrorTextW",
    "midiOutGetID",
    "midiOutGetNumDevs",
    "midiOutGetVolume",
    "midiOutLongMsg",
    "midiOutMessage",
    "midiOutOpen",
    "midiOutPrepareHeader",
    "midiOutReset",
    "midiOutSetVolume",
    "midiOutShortMsg",
    "midiOutUnprepareHeader",
    "midiStreamClose",
    "midiStreamOpen",
    "midiStreamOut",
    "midiStreamPause",
    "midiStreamPosition",
    "midiStreamProperty",
    "midiStreamRestart",
    "midiStreamStop",
    "mixerClose",
    "mixerGetControlDetailsA",
    "mixerGetControlDetailsW",
    "mixerGetDevCapsA",
    "mixerGetDevCapsW",
    "mixerGetID",
    "mixerGetLineControlsA",
    "mixerGetLineControlsW",
    "mixerGetLineInfoA",
    "mixerGetLineInfoW",
    "mixerGetNumDevs",
    "mixerMessage",
    "mixerOpen",
    "mixerSetControlDetails",
    "mmDrvInstall",
    "mmGetCurrentTask",
    "mmTaskBlock",
    "mmTaskCreate",
    "mmTaskSignal",
    "mmTaskYield",
    "mmioAdvance",
    "mmioAscend",
    "mmioClose",
    "mmioCreateChunk",
    "mmioDescend",
    "mmioFlush",
    "mmioGetInfo",
    "mmioInstallIOProcA",
    "mmioInstallIOProcW",
    "mmioOpenA",
    "mmioOpenW",
    "mmioRead",
    "mmioRenameA",
    "mmioRenameW",
    "mmioSeek",
    "mmioSendMessage",
    "mmioSetBuffer",
    "mmioSetInfo",
    "mmioStringToFOURCCA",
    "mmioStringToFOURCCW",
    "mmioWrite",
    "mmsystemGetVersion",
    "sndPlaySoundA",
    "sndPlaySoundW",
    "timeBeginPeriod",
    "timeEndPeriod",
    "timeGetDevCaps",
    "timeGetSystemTime",
    "timeGetTime",
    "timeKillEvent",
    "timeSetEvent",
    "waveInAddBuffer",
    "waveInClose",
    "waveInGetDevCapsA",
    "waveInGetDevCapsW",
    "waveInGetErrorTextA",
    "waveInGetErrorTextW",
    "waveInGetID",
    "waveInGetNumDevs",
    "waveInGetPosition",
    "waveInMessage",
    "waveInOpen",
    "waveInPrepareHeader",
    "waveInReset",
    "waveInStart",
    "waveInStop",
    "waveInUnprepareHeader",
    "waveOutBreakLoop",
    "waveOutClose",
    "waveOutGetDevCapsA",
    "waveOutGetDevCapsW",
    "waveOutGetErrorTextA",
    "waveOutGetErrorTextW",
    "waveOutGetID",
    "waveOutGetNumDevs",
    "waveOutGetPitch",
    "waveOutGetPlaybackRate",
    "waveOutGetPosition",
    "waveOutGetVolume",
    "waveOutMessage",
    "waveOutOpen",
    "waveOutPause",
    "waveOutPrepareHeader",
    "waveOutReset",
    "waveOutRestart",
    "waveOutSetPitch",
    "waveOutSetPlaybackRate",
    "waveOutSetVolume",
    "waveOutUnprepareHeader",
    "waveOutWrite"
};

// Handle to the real system winmm.dll
static HMODULE g_hRealWinmm = nullptr;

// =============================================================================
// CONFIGURATION LOADING
// =============================================================================
static void LoadAndApplyConfig() {
    // Find ReFix.ini next to the game executable
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string iniPath(exePath);
    size_t pos = iniPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        iniPath = iniPath.substr(0, pos + 1) + "ReFix.ini";
    }

    char buf[256];

    // --- [Steam] section ---
    // Set the mask AppId (Spacewar) so Steam client authenticates us
    GetPrivateProfileStringA("Steam", "MaskAppId", "480",
        buf, sizeof(buf), iniPath.c_str());
    SetEnvironmentVariableA("SteamAppId", buf);
    SetEnvironmentVariableA("SteamGameId", buf);

    // Real AppId (for internal game checks)
    GetPrivateProfileStringA("Steam", "RealAppId", "4704690",
        buf, sizeof(buf), iniPath.c_str());
    SetEnvironmentVariableA("REFIX_REAL_APPID", buf);

    // Language override
    GetPrivateProfileStringA("Steam", "Language", "english",
        buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') {
        SetEnvironmentVariableA("SteamLanguage", buf);
    }

    // --- [EOS] section ---
    // DeviceID authentication bypass for Epic Online Services
    GetPrivateProfileStringA("EOS", "DeviceIdAuth", "true",
        buf, sizeof(buf), iniPath.c_str());
    if (std::string(buf) == "true") {
        SetEnvironmentVariableA("EOS_LOGIN_CREDENTIAL_TYPE", "DeviceID");
        SetEnvironmentVariableA("EOS_DEVICE_ID_AUTH", "true");
    }

    // Optional Epic Account ID override
    GetPrivateProfileStringA("EOS", "EpicAccountId", "",
        buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') {
        SetEnvironmentVariableA("EOS_EPIC_ACCOUNT_ID", buf);
    }

    // --- [User] section ---
    // Custom display name
    GetPrivateProfileStringA("User", "Name", "",
        buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') {
        SetEnvironmentVariableA("REFIX_USERNAME", buf);
    }

    // Custom Steam64 ID (auto-generated if empty)
    GetPrivateProfileStringA("User", "SteamId", "",
        buf, sizeof(buf), iniPath.c_str());
    if (buf[0] != '\0') {
        SetEnvironmentVariableA("REFIX_STEAMID", buf);
    }
}

// =============================================================================
// ReFix marker export
// =============================================================================
extern "C" __declspec(dllexport) int ReFix() {
    return 1;
}

// =============================================================================
// DLL Entry Point
// =============================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);

            // Step 1: Load the real system winmm.dll and populate the jump table
            {
                char sysDir[MAX_PATH];
                GetSystemDirectoryA(sysDir, MAX_PATH);
                std::string realPath = std::string(sysDir) + "\\winmm.dll";
                g_hRealWinmm = LoadLibraryA(realPath.c_str());

                if (g_hRealWinmm) {
                    for (int i = 0; i < WINMM_EXPORT_COUNT; i++) {
                        g_winmmProcs[i] = GetProcAddress(g_hRealWinmm, g_exportNames[i]);
                    }
                }
            }

            // Step 2: Load configuration and set environment variables
            // This runs BEFORE any Steam/EOS API is initialized
            LoadAndApplyConfig();
            break;

        case DLL_PROCESS_DETACH:
            if (g_hRealWinmm && !lpReserved) {
                FreeLibrary(g_hRealWinmm);
                g_hRealWinmm = nullptr;
            }
            break;
    }
    return TRUE;
}
