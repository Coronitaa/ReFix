#pragma once
#include <string>

enum class UnrealBackendType {
    Unknown = 0,
    SteamOnly,
    EOSOnly,
    RedpointEOS,
    SteamAndRedpointEOS
};

struct UnrealDetectionResult {
    bool isUnreal;
    UnrealBackendType backend;
    std::string engineVersionHint;
    std::string gameNameHint;
    bool hasRedpointEOS;
    bool hasSteamworks;
    bool hasOnlineSubsystem;
};

// Main detection functions
const UnrealDetectionResult& UnrealDetect_Get();
bool UnrealDetect_IsUnrealProcess();
UnrealBackendType UnrealDetect_GetBackendType();
bool UnrealDetect_IsReGoldbergActive();
void UnrealDetect_LogInfo();
inline void UnrealDetect_Init() { UnrealDetect_LogInfo(); }
