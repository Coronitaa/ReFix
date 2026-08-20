#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include "unreal_detect.h"

// Forward-declare logger if available
extern void ReFixLog(const char* fmt, ...);

static bool FileOrDirExists(const std::string& path) {
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

static bool DirectoryExists(const std::string& path) {
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)::tolower(c); });
    return s;
}

const UnrealDetectionResult& UnrealDetect_Get() {
    static UnrealDetectionResult result = {};
    static bool evaluated = false;
    if (evaluated) return result;
    evaluated = true;

    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string fullPath(exePath);
    std::string lowerPath = ToLower(fullPath);

    // Extract directory & file name
    size_t lastSlash = fullPath.find_last_of("\\/");
    std::string exeDir = (lastSlash != std::string::npos) ? fullPath.substr(0, lastSlash + 1) : "";
    std::string exeName = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;
    std::string lowerExeName = ToLower(exeName);

    // Read ReFix.ini configuration overrides
    std::string iniPath = exeDir + "ReFix.ini";
    if (!FileOrDirExists(iniPath)) {
        // Try one level up (if in Binaries/Win64)
        iniPath = exeDir + "..\\..\\ReFix.ini";
        if (!FileOrDirExists(iniPath)) {
            iniPath = exeDir + "ReFix.ini";
        }
    }

    char bufEngine[64] = { 0 };
    GetPrivateProfileStringA("Game", "EngineType", "", bufEngine, sizeof(bufEngine), iniPath.c_str());
    
    char bufUnrealEnabled[32] = { 0 };
    GetPrivateProfileStringA("Unreal", "EnableReGoldberg", "true", bufUnrealEnabled, sizeof(bufUnrealEnabled), iniPath.c_str());
    if (bufUnrealEnabled[0] == '\0') {
        GetPrivateProfileStringA("Unreal", "Enabled", "true", bufUnrealEnabled, sizeof(bufUnrealEnabled), iniPath.c_str());
    }

    char bufUnrealForce[32] = { 0 };
    GetPrivateProfileStringA("Unreal", "ForceMode", "false", bufUnrealForce, sizeof(bufUnrealForce), iniPath.c_str());

    char bufUnrealBackend[64] = { 0 };
    GetPrivateProfileStringA("Unreal", "Backend", "auto", bufUnrealBackend, sizeof(bufUnrealBackend), iniPath.c_str());

    // 1. Structural heuristics
    bool hasWin64Binaries = (lowerPath.find("\\binaries\\win64") != std::string::npos ||
                             lowerPath.find("/binaries/win64") != std::string::npos);
    bool hasWin32Binaries = (lowerPath.find("\\binaries\\win32") != std::string::npos ||
                             lowerPath.find("/binaries/win32") != std::string::npos);

    bool hasShippingSuffix = (lowerExeName.find("-win64-shipping.exe") != std::string::npos ||
                              lowerExeName.find("-win64-development.exe") != std::string::npos ||
                              lowerExeName.find("-win64-test.exe") != std::string::npos ||
                              lowerExeName.find("-win64-debug.exe") != std::string::npos ||
                              lowerExeName.find("-win32-shipping.exe") != std::string::npos);

    // Check sibling/parent folders:
    // Typical UE path: <GameRoot>/<ProjectName>/Binaries/Win64/<Project>-Win64-Shipping.exe
    // From Win64: ..\..\Content, ..\..\Config, ..\..\..\Engine
    bool hasContentDir = DirectoryExists(exeDir + "..\\..\\Content") ||
                         DirectoryExists(exeDir + "..\\Content") ||
                         DirectoryExists(exeDir + "Content");
    bool hasConfigDir  = DirectoryExists(exeDir + "..\\..\\Config") ||
                         DirectoryExists(exeDir + "..\\Config");
    bool hasEngineDir  = DirectoryExists(exeDir + "..\\..\\..\\Engine") ||
                         DirectoryExists(exeDir + "..\\..\\Engine") ||
                         DirectoryExists(exeDir + "Engine");

    bool hasUFSManifest = FileOrDirExists(exeDir + "..\\..\\Manifest_NonUFSFiles_Win64.txt") ||
                          FileOrDirExists(exeDir + "..\\..\\Manifest_UFSFiles_Win64.txt") ||
                          FileOrDirExists(exeDir + "Manifest_NonUFSFiles_Win64.txt");

    // 2. Module checks (typical UE runtime DLLs)
    bool hasTBB = (GetModuleHandleA("tbb12.dll") != NULL || GetModuleHandleA("tbb.dll") != NULL ||
                   FileOrDirExists(exeDir + "tbb12.dll") || FileOrDirExists(exeDir + "tbb.dll"));
    bool hasNVAftermath = (GetModuleHandleA("GFSDK_Aftermath_Lib.x64.dll") != NULL ||
                           FileOrDirExists(exeDir + "GFSDK_Aftermath_Lib.x64.dll"));
    bool hasXAudio2Redist = (GetModuleHandleA("XAudio2_9Redist.dll") != NULL ||
                             FileOrDirExists(exeDir + "XAudio2_9Redist.dll"));

    // 3. Steamworks presence
    bool hasSteamworksInEngine = DirectoryExists(exeDir + "..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks") ||
                                 DirectoryExists(exeDir + "..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks");
    bool hasSteamDll = FileOrDirExists(exeDir + "steam_api64.dll") ||
                       FileOrDirExists(exeDir + "steam_api.dll") ||
                       hasSteamworksInEngine;
    result.hasSteamworks = hasSteamDll;

    // 4. RedpointEOS / EOS presence
    bool hasRedpointDir = DirectoryExists(exeDir + "RedpointEOS") ||
                          DirectoryExists(exeDir + "..\\RedpointEOS") ||
                          FileOrDirExists(exeDir + "RedpointEOS\\EOSSDK-Win64-Shipping.dll") ||
                          FileOrDirExists(exeDir + "RedpointEOS\\x64\\EOSSDK-Win64-Shipping.dll");
    bool hasEOSSDK = (GetModuleHandleA("EOSSDK-Win64-Shipping.dll") != NULL ||
                      FileOrDirExists(exeDir + "EOSSDK-Win64-Shipping.dll") ||
                      hasRedpointDir);
    result.hasRedpointEOS = hasRedpointDir;

    // Score calculation
    int score = 0;
    if (hasWin64Binaries || hasWin32Binaries) score += 3;
    if (hasShippingSuffix) score += 4;
    if (hasContentDir) score += 3;
    if (hasConfigDir) score += 2;
    if (hasEngineDir) score += 4;
    if (hasUFSManifest) score += 3;
    if (hasTBB) score += 2;
    if (hasNVAftermath) score += 1;
    if (hasXAudio2Redist) score += 1;
    if (hasSteamworksInEngine) score += 3;
    if (hasRedpointDir) score += 3;

    // Configuration explicit override
    if (_stricmp(bufUnrealForce, "true") == 0 || _stricmp(bufUnrealForce, "1") == 0 || _stricmp(bufEngine, "Unreal") == 0) {
        score += 100;
    } else if (_stricmp(bufUnrealEnabled, "false") == 0 || _stricmp(bufUnrealEnabled, "0") == 0 || _stricmp(bufUnrealEnabled, "no") == 0) {
        score = -100; // explicitly disabled
    } else if (_stricmp(bufEngine, "Unity") == 0 || _stricmp(bufEngine, "Godot") == 0 || _stricmp(bufEngine, "Native") == 0) {
        // If EngineType was set to another engine but Unreal structural signals are clearly present, don't block
        if (score < 8) {
            score = -100;
        }
    }

    result.isUnreal = (score >= 5);

    // Backend classification
    if (_stricmp(bufUnrealBackend, "steam+eos") == 0) {
        result.backend = UnrealBackendType::SteamAndRedpointEOS;
    } else if (_stricmp(bufUnrealBackend, "eos") == 0) {
        result.backend = result.hasRedpointEOS ? UnrealBackendType::RedpointEOS : UnrealBackendType::EOSOnly;
    } else if (_stricmp(bufUnrealBackend, "steam") == 0 || _stricmp(bufUnrealBackend, "goldberg") == 0) {
        result.backend = UnrealBackendType::SteamOnly;
    } else {
        // Auto-detection
        if (result.hasSteamworks && (result.hasRedpointEOS || hasEOSSDK)) {
            result.backend = UnrealBackendType::SteamAndRedpointEOS;
        } else if (result.hasRedpointEOS) {
            result.backend = UnrealBackendType::RedpointEOS;
        } else if (hasEOSSDK) {
            result.backend = UnrealBackendType::EOSOnly;
        } else if (result.hasSteamworks) {
            result.backend = UnrealBackendType::SteamOnly;
        } else {
            result.backend = UnrealBackendType::SteamOnly; // default fallback
        }
    }

    // Version & Game name hints
    if (DirectoryExists(exeDir + "..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv157")) {
        result.engineVersionHint = "UE5 (Steamworks v1.57)";
    } else if (DirectoryExists(exeDir + "..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv153")) {
        result.engineVersionHint = "UE5 (Steamworks v1.53)";
    } else if (DirectoryExists(exeDir + "..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv147")) {
        result.engineVersionHint = "UE4 (Steamworks v1.47)";
    } else {
        result.engineVersionHint = "Unreal Engine (Generic)";
    }

    result.gameNameHint = exeName;
    size_t dashPos = result.gameNameHint.find("-Win64");
    if (dashPos == std::string::npos) dashPos = result.gameNameHint.find("-Win32");
    if (dashPos == std::string::npos) dashPos = result.gameNameHint.find(".exe");
    if (dashPos != std::string::npos) {
        result.gameNameHint = result.gameNameHint.substr(0, dashPos);
    }

    return result;
}

bool UnrealDetect_IsUnrealProcess() {
    return UnrealDetect_Get().isUnreal;
}

UnrealBackendType UnrealDetect_GetBackendType() {
    return UnrealDetect_Get().backend;
}

bool UnrealDetect_IsReGoldbergActive() {
    const auto& res = UnrealDetect_Get();
    if (!res.isUnreal) return false;

    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string fullPath(exePath);
    size_t lastSlash = fullPath.find_last_of("\\/");
    std::string exeDir = (lastSlash != std::string::npos) ? fullPath.substr(0, lastSlash + 1) : "";

    std::string iniPath = exeDir + "ReFix.ini";
    if (!FileOrDirExists(iniPath)) iniPath = exeDir + "..\\..\\ReFix.ini";

    char bufOnlineMode[64] = "goldberg";
    GetPrivateProfileStringA("Online", "Mode", "goldberg", bufOnlineMode, sizeof(bufOnlineMode), iniPath.c_str());

    char bufUnrealSteamBackend[64] = "";
    GetPrivateProfileStringA("Unreal.Steam", "Backend", "", bufUnrealSteamBackend, sizeof(bufUnrealSteamBackend), iniPath.c_str());

    if (_stricmp(bufUnrealSteamBackend, "valve") == 0) {
        return false; // explicitly requested valve proxy
    }
    if (_stricmp(bufUnrealSteamBackend, "goldberg") == 0) {
        return true;
    }

    // Default: in goldberg/offline/lan online mode, or if steam_api64_valve.dll does not exist
    bool isGoldbergMode = (_stricmp(bufOnlineMode, "goldberg") == 0 ||
                           _stricmp(bufOnlineMode, "offline") == 0 ||
                           _stricmp(bufOnlineMode, "lan") == 0);
    return isGoldbergMode;
}

void UnrealDetect_LogInfo() {
    const auto& res = UnrealDetect_Get();
    const char* backendStr = "Unknown";
    switch (res.backend) {
        case UnrealBackendType::SteamOnly: backendStr = "SteamOnly (OnlineSubsystemSteam)"; break;
        case UnrealBackendType::EOSOnly: backendStr = "EOSOnly (EOS SDK)"; break;
        case UnrealBackendType::RedpointEOS: backendStr = "RedpointEOS"; break;
        case UnrealBackendType::SteamAndRedpointEOS: backendStr = "Steam + RedpointEOS"; break;
        default: break;
    }

    ReFixLog("[UnrealDetect] Detected Unreal Engine Process: %s", res.isUnreal ? "YES" : "NO");
    if (res.isUnreal) {
        ReFixLog("[UnrealDetect]   Engine Hint: %s | Game Hint: %s", res.engineVersionHint.c_str(), res.gameNameHint.c_str());
        ReFixLog("[UnrealDetect]   Backend: %s (RedpointEOS=%d, Steamworks=%d)", backendStr, res.hasRedpointEOS, res.hasSteamworks);
        ReFixLog("[UnrealDetect]   Re:Goldberg for Unreal Engine Active: %s", UnrealDetect_IsReGoldbergActive() ? "YES" : "NO");
    }
}
