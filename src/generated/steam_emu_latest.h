// Accessors to the emulator's latest-version interface objects, used by the
// generated versioned adapters (tools/gen_steam_iface_adapters.py).
#pragma once
#include <cstdint>

class ISteamClient; class ISteamUser; class ISteamFriends; class ISteamUtils;
class ISteamMatchmaking; class ISteamNetworking; class ISteamUserStats;
class ISteamApps; class ISteamRemoteStorage; class ISteamUGC; class ISteamHTTP;
class ISteamScreenshots; class ISteamGameServer; class ISteamController;

struct ReFixIfaceEntry {
    const char* version;
    void* iface;
};

// Exact-version lookup: returns the generated adapter for pchVersion or nullptr
// (nullptr also for versions the emulator implements natively).
void* ReFixVersionedInterface(const char* pchVersion);

namespace UnrealSteamEmu {
    void* GetGenericInterface(const char* pchVersion);
    int32_t GetHSteamUser();

    ISteamClient* LatestClient();
    ISteamUser* LatestUser();
    ISteamFriends* LatestFriends();
    ISteamUtils* LatestUtils();
    ISteamMatchmaking* LatestMatchmaking();
    ISteamNetworking* LatestNetworking();
    ISteamUserStats* LatestUserStats();
    ISteamApps* LatestApps();
    ISteamRemoteStorage* LatestRemoteStorage();
    ISteamUGC* LatestUGC();
    ISteamHTTP* LatestHTTP();
    ISteamScreenshots* LatestScreenshots();
    ISteamController* LatestController();
    ISteamGameServer* LatestGameServer();
}
