// =============================================================================
// ReFix - Re:Goldberg for Unreal Engine (Standalone Steam Emulation Layer)
// =============================================================================
// Provides complete, ABI-compatible Steamworks SDK emulation for Unreal Engine
// games without requiring Steam.exe or original steam_api64_valve.dll.
//
// Covers:
// - Callbacks & CallResults Engine (RunCallbacks, RegisterCallback, etc.)
// - Full ISteamClient, ISteamUser, ISteamFriends, ISteamUtils
// - ISteamMatchmaking & LAN UDP Lobby Discovery (UDP port 47584)
// - ISteamNetworking & ISteamNetworkingSockets / SteamSockets NetDriver
// - ISteamUserStats, ISteamApps, ISteamRemoteStorage, ISteamUGC
// - ISteamGameServer & ISteamGameServerStats (Dedicated / Listen servers)
// - RedpointEOS Steam credential authentication ticket provider
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")

#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "include/steam/steam_api.h"
#include "include/steam/isteamfriends017.h"
#include "include/steam/isteamuser021.h"
#include "include/steam/isteamgameserver012.h"
#include "include/steam/isteamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"
#include "include/steam/isteamnetworkingmessages.h"

#include "unreal_steam_emu.h"
#include "unreal_detect.h"
#include "network/fault_injector.h"
#include "network/message_tracker.h"
#include "lan_core/refix_lan_firewall.h"
#include "lan_core/refix_lan_core.h"

class CCallbackMgr {
public:
    static void Register(CCallbackBase* pCallback, int iCallback) {
        if (!pCallback) return;
        pCallback->m_nCallbackFlags |= CCallbackBase::k_ECallbackFlagsRegistered;
        pCallback->m_iCallback = iCallback;
    }
    static void Unregister(CCallbackBase* pCallback) {
        if (!pCallback) return;
        pCallback->m_nCallbackFlags &= ~CCallbackBase::k_ECallbackFlagsRegistered;
    }
    static bool IsGameServer(CCallbackBase* pCallback) {
        if (!pCallback) return false;
        return (pCallback->m_nCallbackFlags & CCallbackBase::k_ECallbackFlagsGameServer) != 0;
    }
    static void RunCallback(CCallbackBase* pCallback, void* pData) {
        if (!pCallback || !pData) return;
        pCallback->Run(pData);
    }
    static void RunCallResult(CCallbackBase* pCallback, void* pData, bool bFailed, uint64_t hCall) {
        if (!pCallback || !pData) return;
        pCallback->Run(pData, bFailed, hCall);
    }
};

extern void ReFixLog(const char* fmt, ...);

namespace UnrealSteamEmu {

    // =========================================================================
    // CONFIGURATION & IDENTITY
    // =========================================================================
    static bool g_bInitialized = false;
    static std::recursive_mutex g_emuMutex;

    static std::atomic<uint64_t> g_blockedEgressCount{ 0 };
    uint64_t GetBlockedEgressCount() { return g_blockedEgressCount.load(); }
    void ResetBlockedEgressCount() { g_blockedEgressCount.store(0); }

    static std::atomic<uint64_t> s_unrealSteamEmuCallCount{ 0 };
    uint64_t GetCallCount() { return s_unrealSteamEmuCallCount.load(std::memory_order_relaxed); }
    void ResetCallCount() { s_unrealSteamEmuCallCount.store(0, std::memory_order_relaxed); }
    inline void RecordCall() { s_unrealSteamEmuCallCount.fetch_add(1, std::memory_order_relaxed); }

    static uint64_t g_localSteamID = 0;
    static std::string g_personaName = "Player";
    static uint32_t g_appID = 480;
    static std::string g_language = "english";
    static uint16_t g_listenPort = 47584;
    static std::string g_customBroadcasts = "";
    static bool g_bypassLicenseCheck = true;
    static std::set<uint32_t> g_unlockedDLCs;

    static std::atomic<uint64_t> g_nextAPICall{ 100000ULL };
    static std::atomic<uint32_t> g_nextAuthTicket{ 1 };
    static std::atomic<uint64_t> g_activeLobbyID{ 0 };

    static int32_t g_hSteamPipe = 1;
    static int32_t g_hSteamUser = 1;
    static int32_t g_hGameServerPipe = 2;
    static int32_t g_hGameServerUser = 2;

    static SOCKET g_udpSocket = INVALID_SOCKET;

    // Helper: generate stable deterministic SteamID
    static uint64_t GenerateDeterministicSteamID() {
        char compName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD cSize = sizeof(compName);
        GetComputerNameA(compName, &cSize);

        char userName[256] = { 0 };
        DWORD uSize = sizeof(userName);
        GetUserNameA(userName, &uSize);

        std::string seed = std::string(compName) + "_" + std::string(userName);
        uint32_t hash = 5381;
        for (char c : seed) {
            hash = ((hash << 5) + hash) + (uint8_t)c;
        }

        uint64_t accountID = (uint64_t)(hash & 0x7FFFFFFF);
        return 0x0110000100000000ULL | accountID;
    }

    static void SyncTicketToEnvironment(const uint8_t* data, size_t size, uint32_t handle) {
        if (!data || size == 0) return;
        static const char hexChars[] = "0123456789abcdef";
        std::string hexStr;
        hexStr.reserve(size * 2);
        for (size_t i = 0; i < size; ++i) {
            hexStr.push_back(hexChars[(data[i] >> 4) & 0x0F]);
            hexStr.push_back(hexChars[data[i] & 0x0F]);
        }
        SetEnvironmentVariableA("REFIX_STEAM_AUTH_TICKET", hexStr.c_str());
        SetEnvironmentVariableA("REFIX_STEAM_AUTH_HANDLE", std::to_string(handle).c_str());
    }

    static void LoadConfig() {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string exeDir(exePath);
        size_t lastSlash = exeDir.find_last_of("\\/");
        if (lastSlash != std::string::npos) exeDir = exeDir.substr(0, lastSlash + 1);

        std::vector<std::string> iniCandidates = {
            exeDir + "ReFix.ini",
            exeDir + "..\\ReFix.ini",
            exeDir + "..\\..\\ReFix.ini",
            exeDir + "..\\..\\..\\ReFix.ini",
            exeDir + "..\\..\\..\\..\\ReFix.ini"
        };
        std::string iniPath = exeDir + "ReFix.ini";
        for (const auto& cand : iniCandidates) {
            if (GetFileAttributesA(cand.c_str()) != INVALID_FILE_ATTRIBUTES) {
                iniPath = cand;
                break;
            }
        }

        std::vector<std::string> nameFiles = {
            exeDir + "steam_settings\\force_account_name.txt",
            exeDir + "..\\steam_settings\\force_account_name.txt",
            exeDir + "..\\..\\steam_settings\\force_account_name.txt",
            exeDir + "..\\..\\..\\steam_settings\\force_account_name.txt"
        };
        std::string goldbergName = "";
        for (const auto& nf : nameFiles) {
            std::ifstream file(nf);
            if (file.is_open()) {
                std::string line;
                if (std::getline(file, line)) {
                    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
                    while (!line.empty() && line.front() == ' ') line.erase(line.begin());
                    if (!line.empty()) {
                        goldbergName = line;
                        break;
                    }
                }
            }
        }

        std::vector<std::string> sidFiles = {
            exeDir + "steam_settings\\force_steamid.txt",
            exeDir + "..\\steam_settings\\force_steamid.txt",
            exeDir + "..\\..\\steam_settings\\force_steamid.txt",
            exeDir + "..\\..\\..\\steam_settings\\force_steamid.txt"
        };
        uint64_t goldbergSteamId = 0;
        for (const auto& sf : sidFiles) {
            std::ifstream file(sf);
            if (file.is_open()) {
                std::string line;
                if (std::getline(file, line)) {
                    uint64_t sid = _strtoui64(line.c_str(), nullptr, 10);
                    if (sid != 0) {
                        goldbergSteamId = sid;
                        break;
                    }
                }
            }
        }

        char buf[256] = { 0 };
        // Priority 1: [User] Name
        GetPrivateProfileStringA("User", "Name", "", buf, sizeof(buf), iniPath.c_str());
        // Priority 2: [Unreal.Steam] PersonaName
        if (buf[0] == '\0') {
            GetPrivateProfileStringA("Unreal.Steam", "PersonaName", "", buf, sizeof(buf), iniPath.c_str());
        }
        // Priority 3: [User] PersonaName
        if (buf[0] == '\0') {
            GetPrivateProfileStringA("User", "PersonaName", "", buf, sizeof(buf), iniPath.c_str());
        }
        // Priority 4: [User] Username
        if (buf[0] == '\0') {
            GetPrivateProfileStringA("User", "Username", "", buf, sizeof(buf), iniPath.c_str());
        }
        // Priority 5: Goldberg steam_settings/force_account_name.txt
        if (buf[0] == '\0' && !goldbergName.empty()) {
            strncpy_s(buf, sizeof(buf), goldbergName.c_str(), _TRUNCATE);
        }
        // Priority 6: Environment variables
        if (buf[0] == '\0') {
            char envName[128] = { 0 };
            if (GetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", envName, sizeof(envName)) > 0 && envName[0]) {
                strncpy_s(buf, sizeof(buf), envName, _TRUNCATE);
            } else if (GetEnvironmentVariableA("REFIX_USER_NAME", envName, sizeof(envName)) > 0 && envName[0]) {
                strncpy_s(buf, sizeof(buf), envName, _TRUNCATE);
            } else if (GetEnvironmentVariableA("REFIX_USERNAME", envName, sizeof(envName)) > 0 && envName[0]) {
                strncpy_s(buf, sizeof(buf), envName, _TRUNCATE);
            } else if (GetEnvironmentVariableA("SteamPersonaName", envName, sizeof(envName)) > 0 && envName[0]) {
                strncpy_s(buf, sizeof(buf), envName, _TRUNCATE);
            }
        }
        if (buf[0] != '\0') {
            g_personaName = buf;
        } else {
            g_personaName = "Player";
        }

        char envSid[64] = { 0 };
        if (GetEnvironmentVariableA("REFIX_STEAM_ID", envSid, sizeof(envSid)) > 0 && envSid[0]) {
            g_localSteamID = _strtoui64(envSid, nullptr, 10);
        } else if (GetEnvironmentVariableA("REFIX_STEAMID", envSid, sizeof(envSid)) > 0 && envSid[0]) {
            g_localSteamID = _strtoui64(envSid, nullptr, 10);
        } else if (GetEnvironmentVariableA("SteamID", envSid, sizeof(envSid)) > 0 && envSid[0]) {
            g_localSteamID = _strtoui64(envSid, nullptr, 10);
        } else if (GetEnvironmentVariableA("SteamId", envSid, sizeof(envSid)) > 0 && envSid[0]) {
            g_localSteamID = _strtoui64(envSid, nullptr, 10);
        }

        if (g_localSteamID == 0) {
            char bufId[64] = { 0 };
            GetPrivateProfileStringA("Unreal.Steam", "SteamId", "", bufId, sizeof(bufId), iniPath.c_str());
            if (bufId[0]) {
                g_localSteamID = _strtoui64(bufId, nullptr, 10);
            }
            if (g_localSteamID == 0) {
                GetPrivateProfileStringA("User", "SteamId", "", bufId, sizeof(bufId), iniPath.c_str());
                if (bufId[0]) g_localSteamID = _strtoui64(bufId, nullptr, 10);
            }
            if (g_localSteamID == 0 && goldbergSteamId != 0) {
                g_localSteamID = goldbergSteamId;
            }
        }
        if (g_localSteamID == 0) {
            g_localSteamID = GenerateDeterministicSteamID();
        }

        char bufApp[64];
        GetPrivateProfileStringA("Unreal.Steam", "AppId", "", bufApp, sizeof(bufApp), iniPath.c_str());
        if (bufApp[0]) g_appID = (uint32_t)atoi(bufApp);
        else {
            GetPrivateProfileStringA("Steam", "RealAppId", "0", bufApp, sizeof(bufApp), iniPath.c_str());
            if (bufApp[0] && strcmp(bufApp, "0") != 0) g_appID = (uint32_t)atoi(bufApp);
            else {
                GetPrivateProfileStringA("Steam", "MaskAppId", "480", bufApp, sizeof(bufApp), iniPath.c_str());
                g_appID = (uint32_t)atoi(bufApp);
            }
        }
        if (g_appID == 0) g_appID = 480;

        GetPrivateProfileStringA("Unreal.Steam", "Language", "english", buf, sizeof(buf), iniPath.c_str());
        g_language = buf;

        g_listenPort = (uint16_t)GetPrivateProfileIntA("Unreal.Networking", "ListenPort", 47584, iniPath.c_str());
        if (g_listenPort == 0) g_listenPort = 47584;
        char envPort[32] = { 0 };
        if (GetEnvironmentVariableA("REFIX_LISTEN_PORT", envPort, sizeof(envPort)) > 0) {
            int p = atoi(envPort);
            if (p > 0 && p <= 65535) g_listenPort = (uint16_t)p;
        }

        GetPrivateProfileStringA("Unreal.Networking", "CustomBroadcasts", "", buf, sizeof(buf), iniPath.c_str());
        g_customBroadcasts = buf;

        char bufBypass[16];
        GetPrivateProfileStringA("Unreal.Steam", "BypassLicenseCheck", "true", bufBypass, sizeof(bufBypass), iniPath.c_str());
        g_bypassLicenseCheck = (_stricmp(bufBypass, "true") == 0 || strcmp(bufBypass, "1") == 0);

        char bufDLC[512] = { 0 };
        GetPrivateProfileStringA("Unreal.Steam", "UnlockDLCs", "", bufDLC, sizeof(bufDLC), iniPath.c_str());
        if (bufDLC[0]) {
            char* nextToken = nullptr;
            char* tok = strtok_s(bufDLC, ",; ", &nextToken);
            while (tok) {
                uint32_t dlcId = (uint32_t)atoi(tok);
                if (dlcId > 0) g_unlockedDLCs.insert(dlcId);
                tok = strtok_s(nullptr, ",; ", &nextToken);
            }
        }

        // Export identity environment variables for EOSSDK and all subsystems
        SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", g_personaName.c_str());
        SetEnvironmentVariableA("REFIX_USER_NAME", g_personaName.c_str());
        SetEnvironmentVariableA("REFIX_USERNAME", g_personaName.c_str());
        SetEnvironmentVariableA("SteamPersonaName", g_personaName.c_str());

        char sidStr[32];
        sprintf_s(sidStr, "%llu", g_localSteamID);
        SetEnvironmentVariableA("REFIX_STEAM_ID", sidStr);
        SetEnvironmentVariableA("REFIX_STEAMID", sidStr);
        SetEnvironmentVariableA("SteamID", sidStr);
        SetEnvironmentVariableA("SteamId", sidStr);

        // Apply Steam AppID environment variables
        char appIdStr[32];
        sprintf_s(appIdStr, "%u", g_appID);
        SetEnvironmentVariableA("SteamAppId", appIdStr);
        SetEnvironmentVariableA("SteamGameId", appIdStr);
        SetEnvironmentVariableA("STEAM_COMPAT_APP_ID", appIdStr);
    }

    // =========================================================================
    // CALLBACKS & CALLRESULTS ENGINE
    // =========================================================================
    struct QueuedCallbackItem {
        int iCallback;
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point triggerTime;
        bool isGameServer;
    };

    struct QueuedCallResultItem {
        uint64_t hAPICall;
        int iCallback;
        std::vector<uint8_t> data;
        bool completed;
        bool failed;
        std::chrono::steady_clock::time_point triggerTime;
    };

    static std::multimap<int, CCallbackBase*> g_clientCallbacks;
    static std::multimap<int, CCallbackBase*> g_serverCallbacks;
    static std::map<uint64_t, CCallbackBase*> g_callResultListeners;

    static std::vector<QueuedCallbackItem> g_callbackQueue;
    static std::vector<QueuedCallbackItem> g_manualCallbackQueue;
    static std::map<uint64_t, QueuedCallResultItem> g_callResultMap;

    void RegisterCallback(CCallbackBase* pCallback, int iCallback) {
        if (!pCallback) return;
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);

        CCallbackMgr::Register(pCallback, iCallback);

        if (CCallbackMgr::IsGameServer(pCallback)) {
            g_serverCallbacks.insert({ iCallback, pCallback });
        } else {
            g_clientCallbacks.insert({ iCallback, pCallback });
        }
        ReFixLog("[UnrealSteam] RegisterCallback: iCallback=%d, pCallback=%p (isServer=%d)",
                 iCallback, pCallback, CCallbackMgr::IsGameServer(pCallback));
    }

    void UnregisterCallback(CCallbackBase* pCallback) {
        if (!pCallback) return;
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);

        CCallbackMgr::Unregister(pCallback);

        for (auto it = g_clientCallbacks.begin(); it != g_clientCallbacks.end(); ) {
            if (it->second == pCallback) it = g_clientCallbacks.erase(it);
            else ++it;
        }
        for (auto it = g_serverCallbacks.begin(); it != g_serverCallbacks.end(); ) {
            if (it->second == pCallback) it = g_serverCallbacks.erase(it);
            else ++it;
        }
    }

    void RegisterCallResult(CCallbackBase* pCallback, uint64_t hAPICall) {
        if (!pCallback || hAPICall == 0) return;
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);

        g_callResultListeners[hAPICall] = pCallback;
        ReFixLog("[UnrealSteam] RegisterCallResult: hAPICall=%llu, pCallback=%p", hAPICall, pCallback);
    }

    void UnregisterCallResult(CCallbackBase* pCallback, uint64_t hAPICall) {
        if (!pCallback || hAPICall == 0) return;
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);

        auto it = g_callResultListeners.find(hAPICall);
        if (it != g_callResultListeners.end() && it->second == pCallback) {
            g_callResultListeners.erase(it);
        }
    }

    uint64_t PostCallResult(int iCallback, const void* pData, size_t dataSize, double delaySeconds) {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);

        uint64_t hAPICall = ++g_nextAPICall;
        QueuedCallResultItem item;
        item.hAPICall = hAPICall;
        item.iCallback = iCallback;
        if (pData && dataSize > 0) {
            item.data.assign((const uint8_t*)pData, (const uint8_t*)pData + dataSize);
        }
        item.completed = false;
        item.failed = false;
        item.triggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(delaySeconds * 1000.0));

        g_callResultMap[hAPICall] = item;
        ReFixLog("[UnrealSteam] PostCallResult: hAPICall=%llu, iCallback=%d, size=%zu, delay=%.3fs",
                 hAPICall, iCallback, dataSize, delaySeconds);
        return hAPICall;
    }

    void PostCallback(int iCallback, const void* pData, size_t dataSize, double delaySeconds) {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);

        QueuedCallbackItem item;
        item.iCallback = iCallback;
        if (pData && dataSize > 0) {
            item.data.assign((const uint8_t*)pData, (const uint8_t*)pData + dataSize);
        }
        item.triggerTime = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(delaySeconds * 1000.0));
        item.isGameServer = false;

        g_callbackQueue.push_back(item);
        g_manualCallbackQueue.push_back(item);
    }

    bool IsAPICallCompleted(uint64_t hAPICall, bool* pbFailed) {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        auto it = g_callResultMap.find(hAPICall);
        if (it == g_callResultMap.end()) return false;
        if (pbFailed) *pbFailed = it->second.failed;
        return it->second.completed;
    }

    bool GetAPICallResult(uint64_t hAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed) {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        auto it = g_callResultMap.find(hAPICall);
        if (it == g_callResultMap.end()) return false;
        if (!it->second.completed) return false;
        if (pbFailed) *pbFailed = it->second.failed;

        if (pCallback && cubCallback > 0) {
            size_t copyLen = (std::min)((size_t)cubCallback, it->second.data.size());
            memcpy(pCallback, it->second.data.data(), copyLen);
        }
        return !it->second.failed;
    }

    // =========================================================================
    // LAN NETWORKING & LOBBIES
    // =========================================================================
    struct LobbyInfo {
        uint64_t id;
        uint64_t owner;
        ELobbyType type;
        int maxMembers;
        bool joinable;
        std::vector<uint64_t> members;
        std::map<std::string, std::string> data;
        std::map<uint64_t, std::map<std::string, std::string>> memberData;
        uint32_t gameServerIP;
        uint16_t gameServerPort;
        uint64_t gameServerSteamID;
        std::chrono::steady_clock::time_point lastSeen;
    };

    static std::map<uint64_t, LobbyInfo> g_lobbies;

    static std::mutex s_steamLobbyMapMutex;
    static std::unordered_map<std::string, uint64_t> s_coreToSteamLobby;
    static std::unordered_map<uint64_t, std::string> s_steamToCoreLobby;

    struct PendingJoin {
        SteamAPICall_t callHandle = 0;
        uint64_t steamLobbyId = 0;
        std::string coreLobbyId;
        std::chrono::steady_clock::time_point requestTime;
    };
    static std::unordered_map<std::string, PendingJoin> g_pendingJoins;

    struct PendingSearch {
        SteamAPICall_t callHandle = 0;
        std::chrono::steady_clock::time_point startTime;
        refix::lan::MatchmakingCriteria criteria;
    };
    static std::vector<PendingSearch> g_pendingSearches;
    static refix::lan::MatchmakingCriteria g_pendingSearchCriteria;
    static std::vector<uint64_t> g_lastMatchmakingResults;

    static bool ShouldUseLegacyFallback() {
        static int s_legacyDisabled = -1;
        if (s_legacyDisabled == -1) {
            char buf[16] = {0};
            if (GetEnvironmentVariableA("REFIX_DISABLE_LEGACY_FALLBACKS", buf, sizeof(buf)) > 0) {
                s_legacyDisabled = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T' || buf[0] == 'y' || buf[0] == 'Y') ? 1 : 0;
            } else {
                s_legacyDisabled = 0;
            }
        }
        return s_legacyDisabled == 0;
    }

    static uint32_t ComputeLobbyAccountId(const std::string& coreLobbyId) {
        // Deterministic high-avalanche 64-bit FNV-1a hash followed by Murmur3/SplitMix64 finalizer
        uint64_t h = 0xCBF29CE484222325ULL;
        for (char c : coreLobbyId) {
            h ^= static_cast<uint8_t>(c);
            h *= 0x100000001B3ULL;
        }
        h ^= (h >> 33);
        h *= 0xFF51AFD7ED558CCDULL;
        h ^= (h >> 33);
        h *= 0xC4CEB9FE1A85EC53ULL;
        h ^= (h >> 33);
        uint32_t accountId = static_cast<uint32_t>(h ^ (h >> 32));
        if (accountId == 0) accountId = 1;
        return accountId;
    }

    static uint64_t ComputeLobbySteamID(const std::string& coreLobbyId) {
        if (coreLobbyId.empty()) return 0;
        uint32_t accountId = ComputeLobbyAccountId(coreLobbyId);
        // Canonical Steamworks Lobby CSteamID:
        // - Universe: k_EUniversePublic (1)
        // - AccountType: k_EAccountTypeChat (8)
        // - Instance: k_EChatInstanceFlagLobby (0x40000)
        // - AccountID: deterministic 32-bit ID derived from LanCore Lobby ID
        CSteamID lobbySteamId(accountId, k_EChatInstanceFlagLobby, k_EUniversePublic, k_EAccountTypeChat);
        return lobbySteamId.ConvertToUint64();
    }

    static uint64_t EnsureSteamLobbyID(const std::string& coreLobbyId, uint64_t fallbackId = 0) {
        if (coreLobbyId.empty()) return (fallbackId != 0) ? fallbackId : 0;
        std::lock_guard<std::mutex> lock(s_steamLobbyMapMutex);
        auto it = s_coreToSteamLobby.find(coreLobbyId);
        if (it != s_coreToSteamLobby.end()) return it->second;

        uint64_t steamLobbyId = (fallbackId != 0) ? fallbackId : ComputeLobbySteamID(coreLobbyId);

        // Explicit collision policy: detect if steamLobbyId already belongs to a different coreLobbyId
        auto revIt = s_steamToCoreLobby.find(steamLobbyId);
        if (revIt != s_steamToCoreLobby.end() && revIt->second != coreLobbyId) {
            ReFixLog("[UnrealSteam] LOBBY COLLISION REJECTED: coreId='%s' collides with existing '%s' on steamID=%llu",
                     coreLobbyId.c_str(), revIt->second.c_str(), steamLobbyId);
            return 0;
        }

        s_coreToSteamLobby[coreLobbyId] = steamLobbyId;
        s_steamToCoreLobby[steamLobbyId] = coreLobbyId;
        return steamLobbyId;
    }

    static std::string GetCoreLobbyId(uint64_t steamLobbyId) {
        std::lock_guard<std::mutex> lock(s_steamLobbyMapMutex);
        auto it = s_steamToCoreLobby.find(steamLobbyId);
        if (it != s_steamToCoreLobby.end()) return it->second;
        return "";
    }

    static void CompleteCallResult(uint64_t hAPICall, int iCallback, const void* pData, size_t dataSize, bool bFailed) {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        QueuedCallResultItem item;
        item.hAPICall = hAPICall;
        item.iCallback = iCallback;
        if (pData && dataSize > 0) {
            item.data.assign((const uint8_t*)pData, (const uint8_t*)pData + dataSize);
        }
        item.completed = false;
        item.failed = bFailed;
        item.triggerTime = std::chrono::steady_clock::now();
        g_callResultMap[hAPICall] = item;
    }

    struct DiscoveredPeer {
        uint64_t steamID;
        std::string personaName;
        uint32_t ip;
        uint16_t port;
        std::chrono::steady_clock::time_point lastSeen;
    };
    static std::map<uint64_t, DiscoveredPeer> g_peers;

    // Incoming P2P packet queue
    struct P2PPacket {
        uint64_t senderID;
        int channel;
        std::vector<uint8_t> data;
    };
    static std::map<int, std::queue<P2PPacket>> g_p2pIncoming;

    #pragma pack(push, 1)
    struct NetPacketHeader {
        uint32_t magic;      // 0x52464958 'RFIX'
        uint8_t  msgType;    // 1: Ping, 2: LobbyAnnounce, 3: LobbyQuery, 4: LobbyJoin, 5: P2P, 6: SocketsPayload, 7: SocketsHandshake, 8: SocketsHandshakeACK, 9: SocketsDataACK
        uint64_t senderID;
        uint32_t appID;
        uint32_t payloadLen;
    };
    
    struct SocketsHandshake {
        uint32_t protocolVersion; // 1
        uint32_t sessionId;       // random or lobby based
        uint32_t connectionNonce; // unique per connection attempt
        uint32_t capabilities;
        uint64_t remotePeerId;
    };

    struct SocketsHandshakeAck {
        uint32_t protocolVersion; // 1
        uint32_t sessionId;
        uint32_t connectionNonce;
    };

    struct SocketsAckPacket {
        uint32_t sessionId;
        uint64_t ackSequence;     // highest contiguous sequence received
    };
    
    struct SocketsPayloadHeader {
        int64_t messageNumber; // Application message number
        uint64_t sequence;     // Wire sequence number
        uint64_t ack;          // Wire ACK number
        uint16_t flags;        // k_nSteamNetworkingSend_Reliable etc.
        uint16_t channel;
    };
    #pragma pack(pop)

    static void InitSockets() {
        if (g_udpSocket != INVALID_SOCKET) return;

        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);

        g_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (g_udpSocket != INVALID_SOCKET) {
            BOOL bOpt = TRUE;
            setsockopt(g_udpSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&bOpt, sizeof(bOpt));
            setsockopt(g_udpSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&bOpt, sizeof(bOpt));

            u_long mode = 1;
            ioctlsocket(g_udpSocket, FIONBIO, &mode);

            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(g_listenPort);

            if (bind(g_udpSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                // If binding to requested g_listenPort failed, fall back to ephemeral port 0
                addr.sin_port = 0;
                if (bind(g_udpSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                    ReFixLog("[UnrealSteam] Warning: Could not bind UDP socket to port %u or ephemeral", g_listenPort);
                } else {
                    sockaddr_in boundAddr = {};
                    int boundLen = sizeof(boundAddr);
                    if (getsockname(g_udpSocket, (sockaddr*)&boundAddr, &boundLen) == 0) {
                        g_listenPort = ntohs(boundAddr.sin_port);
                        ReFixLog("[UnrealSteam] Bound UDP socket to ephemeral port %u (fallback from collision)", g_listenPort);
                    }
                }
            } else {
                sockaddr_in boundAddr = {};
                int boundLen = sizeof(boundAddr);
                if (getsockname(g_udpSocket, (sockaddr*)&boundAddr, &boundLen) == 0) {
                    g_listenPort = ntohs(boundAddr.sin_port);
                }
                ReFixLog("[UnrealSteam] Bound UDP socket to port %u for LAN discovery & P2P", g_listenPort);
            }
        }
    }

    static bool IsAllowedLanAddress(const sockaddr* addr) {
        if (!addr) return false;
        if (addr->sa_family == AF_INET) {
            const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(addr);
            uint32_t ip = ntohl(sin->sin_addr.s_addr);
            return refix::lan::LanFirewall::Get().IsAllowedIpv4(ip);
        } else if (addr->sa_family == AF_INET6) {
            // BLOCKER 8: AF_INET6 audit - The ReFix LAN transport currently uses AF_INET sockets exclusively.
            // IPv6 transport is UNVERIFIED / unsupported at Phase 3.6. Disallowed to prevent false claims.
            return false;
        }
        return false;
    }

    static void BroadcastNetPacket(uint8_t msgType, const void* payload, size_t payloadLen) {
        if (g_udpSocket == INVALID_SOCKET) return;

        std::vector<uint8_t> buf(sizeof(NetPacketHeader) + payloadLen);
        NetPacketHeader* hdr = (NetPacketHeader*)buf.data();
        hdr->magic = 0x52464958;
        hdr->msgType = msgType;
        hdr->senderID = g_localSteamID;
        hdr->appID = g_appID;
        hdr->payloadLen = (uint32_t)payloadLen;

        if (payload && payloadLen > 0) {
            memcpy(buf.data() + sizeof(NetPacketHeader), payload, payloadLen);
        }

        std::vector<std::string> destinations = { "255.255.255.255", "127.0.0.1" };
        if (!g_customBroadcasts.empty()) {
            std::stringstream ss(g_customBroadcasts);
            std::string ipStr;
            while (std::getline(ss, ipStr, ',')) {
                if (!ipStr.empty()) destinations.push_back(ipStr);
            }
        }

        std::vector<uint16_t> broadcastPorts = { 47584, 47585 };
        if (g_listenPort != 47584 && g_listenPort != 47585) broadcastPorts.push_back(g_listenPort);

        for (const auto& dstIP : destinations) {
            for (uint16_t port : broadcastPorts) {
                sockaddr_in dest = {};
                dest.sin_family = AF_INET;
                dest.sin_port = htons(port);
                inet_pton(AF_INET, dstIP.c_str(), &dest.sin_addr);

                if (!IsAllowedLanAddress((const sockaddr*)&dest)) {
                    ReFixLog("[InternetZero] BLOCKED broadcast egress attempt to disallowed IP %s:%u",
                             dstIP.c_str(), port);
                    g_blockedEgressCount.fetch_add(1);
                    continue;
                }

                sendto(g_udpSocket, (const char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&dest, sizeof(dest));
            }
        }
    }

    static void SendLanPacket(CSteamID remoteID, uint8_t msgType, const void* payload, size_t payloadLen, ReFix::PacketDirection dir = ReFix::PacketDirection::ANY) {
        sockaddr_in dest = {};
        bool hasEndpoint = false;
        {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            auto it = g_peers.find(remoteID.ConvertToUint64());
            if (it != g_peers.end() && it->second.ip != 0 && it->second.port != 0) {
                dest.sin_family = AF_INET;
                dest.sin_port = htons(it->second.port);
                dest.sin_addr.s_addr = htonl(it->second.ip);
                hasEndpoint = true;
            }
        }

        // BLOQUEANTE 1: Discovery y Handshake separados.
        // Never broadcast msgType 6, 7, 8, 9!
        if (!hasEndpoint) {
            ReFixLog("[SendLanPacket] Peer %llu endpoint not yet resolved; refusing to broadcast msgType=%d",
                     remoteID.ConvertToUint64(), msgType);
            return;
        }

        // BLOQUEANTE 11 & 13: Internet-zero enforcement at sendto site
        if (!IsAllowedLanAddress((const sockaddr*)&dest)) {
            char ipStr[INET_ADDRSTRLEN] = { 0 };
            inet_ntop(AF_INET, &dest.sin_addr, ipStr, sizeof(ipStr));
            ReFixLog("[InternetZero] BLOCKED egress attempt to disallowed endpoint %s:%u (msgType=%d)",
                     ipStr, ntohs(dest.sin_port), msgType);
            g_blockedEgressCount.fetch_add(1);
            return;
        }

        if (g_udpSocket == INVALID_SOCKET) return;

        NetPacketHeader hdr;
        hdr.magic = 0x52464958;
        hdr.msgType = msgType;
        hdr.senderID = g_localSteamID;
        hdr.appID = g_appID;
        hdr.payloadLen = (uint32_t)payloadLen;

        std::vector<uint8_t> netBuf(sizeof(hdr) + payloadLen);
        memcpy(netBuf.data(), &hdr, sizeof(hdr));
        if (payloadLen > 0) memcpy(netBuf.data() + sizeof(hdr), payload, payloadLen);

        ReFix::PacketClass pClass = ReFix::PacketClass::OTHER;
        switch (msgType) {
            case 6: pClass = ReFix::PacketClass::DATA; break;
            case 7: pClass = ReFix::PacketClass::HANDSHAKE; break;
            case 8: pClass = ReFix::PacketClass::HANDSHAKE_ACK; break;
            case 9: pClass = ReFix::PacketClass::DATA_ACK; break;
            default: pClass = ReFix::PacketClass::OTHER; break;
        }

        auto sendFn = [dest, msgType, remoteID](const uint8_t* sendData, size_t sendLen) {
            int ret = sendto(g_udpSocket, (const char*)sendData, (int)sendLen, 0, (const sockaddr*)&dest, sizeof(dest));
            if (ret != SOCKET_ERROR) {
                ReFixLog("[SendLanPacket] Sent unicast msgType=%d to=%llu len=%zu",
                         msgType, remoteID.ConvertToUint64(), sendLen);
            }
        };

        // BLOQUEANTE 4: Fault injection real y direccionable
        ReFix::FaultInjector::Get().ProcessSend(pClass, dir, netBuf.data(), netBuf.size(), sendFn);
    }

    // --- SteamNetworkingSockets Connection Management ---
    enum class ReFixConnSubstate {
        None,
        Connecting_WaitingDiscovery,
        Connecting_HandshakeSent,
        Connected,
        Closed
    };

    struct ReliablePacketOut {
        uint64_t sequence;
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point lastSendTime;
        int retries;
    };

    struct ReFixConnection {
        HSteamNetConnection handle;
        uint32_t sessionId;
        uint32_t connectionNonce;
        CSteamID remoteSteamID;
        ESteamNetworkingConnectionState state;
        ReFixConnSubstate substate;
        int64 userData;
        char name[128];
        int64 messageCountOut;
        std::queue<SteamNetworkingMessage_t*> incomingMessages;
        HSteamNetPollGroup pollGroup;
        bool isInitiator{ false };

        // Handshake retry timer & count
        std::chrono::steady_clock::time_point connectionStartTime;
        std::chrono::steady_clock::time_point lastHandshakeSendTime;
        int handshakeRetries;

        // Reliable state (decoupled from unreliable!)
        uint64_t nextReliableSequenceOut;
        uint64_t highestReliableAckReceived;
        uint64_t nextReliableSequenceExpected;
        std::vector<ReliablePacketOut> unackedOutbound;
        std::map<uint64_t, SteamNetworkingMessage_t*> outOfOrderInbound;

        // Real-Time measured metrics
        int64_t pingMs;
    };

    static std::mutex g_socketsMutex;
    static std::map<HSteamNetConnection, ReFixConnection> g_connections;
    static HSteamNetConnection g_nextConnectionHandle = 1000;
    static std::map<uint32_t, HSteamNetConnection> g_sessionToConnection;

    static uint32_t GenerateUniqueSessionId() {
        static std::atomic<uint32_t> s_sessionSeq{ 1 };
        uint32_t seq = s_sessionSeq.fetch_add(1);
        uint32_t entropy = (uint32_t)(std::chrono::high_resolution_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
        uint32_t sId = (entropy & 0xFFFF0000) | (seq & 0x0000FFFF);
        if (sId == 0) sId = 1;
        return sId;
    }

    static uint32_t GenerateConnectionNonce() {
        static std::atomic<uint32_t> s_nonceSeq{ 10000 };
        uint32_t seq = s_nonceSeq.fetch_add(1);
        uint32_t entropy = (uint32_t)(std::chrono::high_resolution_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
        return entropy ^ (seq * 2654435761u);
    }

    static void ReleaseReFixMessage(SteamNetworkingMessage_t* pMsg) {
        if (pMsg) {
            ReFix::MessageTracker::Get().TrackRelease(pMsg);
            free(pMsg);
        }
    }

    struct ConnectionSnapshot {
        HSteamNetConnection handle;
        CSteamID remoteSteamID;
        int64 userData;
        char name[128];
        ESteamNetworkingConnectionState oldState;
        ESteamNetworkingConnectionState newState;
        int endReason;
        char debugMsg[128];
    };

    static void EnqueueSocketCallbackFromSnapshot(const ConnectionSnapshot& snap) {
        SteamNetConnectionStatusChangedCallback_t cb;
        memset(&cb, 0, sizeof(cb));
        cb.m_hConn = snap.handle;
        cb.m_eOldState = snap.oldState;
        cb.m_info.m_eState = snap.newState;
        cb.m_info.m_identityRemote.SetSteamID64(snap.remoteSteamID.ConvertToUint64());
        cb.m_info.m_nUserData = snap.userData;
        cb.m_info.m_eEndReason = snap.endReason;
        strncpy_s(cb.m_info.m_szEndDebug, sizeof(cb.m_info.m_szEndDebug), snap.debugMsg, _TRUNCATE);
        snprintf(cb.m_info.m_szConnectionDescription, sizeof(cb.m_info.m_szConnectionDescription), "steamid:%llu", (unsigned long long)snap.remoteSteamID.ConvertToUint64());

        std::vector<uint8_t> cbData((uint8_t*)&cb, (uint8_t*)&cb + sizeof(cb));
        std::lock_guard<std::recursive_mutex> cbLock(g_emuMutex);
        g_callbackQueue.push_back({ SteamNetConnectionStatusChangedCallback_t::k_iCallback, cbData, std::chrono::steady_clock::now(), false });
    }

    static HSteamNetConnection CreateReFixConnection(CSteamID remoteID, ESteamNetworkingConnectionState initialState, uint32_t sessionId = 0, uint32_t connectionNonce = 0) {
        std::lock_guard<std::mutex> lock(g_socketsMutex);

        // 1. Allocate unique handle (generate -> verify unused)
        HSteamNetConnection handle = g_nextConnectionHandle++;
        while (handle == k_HSteamNetConnection_Invalid || g_connections.find(handle) != g_connections.end()) {
            handle = g_nextConnectionHandle++;
        }

        // 2. Allocate unique sessionId (generate -> verify unused)
        if (sessionId == 0) {
            do {
                sessionId = GenerateUniqueSessionId();
            } while (sessionId == 0 || g_sessionToConnection.find(sessionId) != g_sessionToConnection.end());
        }

        // 3. Allocate unique connectionNonce (generate -> verify unused)
        if (connectionNonce == 0) {
            bool inUse = true;
            while (inUse) {
                connectionNonce = GenerateConnectionNonce();
                if (connectionNonce == 0) continue;
                inUse = false;
                for (const auto& pair : g_connections) {
                    if (pair.second.connectionNonce == connectionNonce) {
                        inUse = true;
                        break;
                    }
                }
            }
        }

        ReFixConnection conn;
        conn.handle = handle;
        conn.sessionId = sessionId;
        conn.connectionNonce = connectionNonce;
        conn.remoteSteamID = remoteID;
        conn.state = initialState;
        conn.substate = (initialState == k_ESteamNetworkingConnectionState_Connecting) ? ReFixConnSubstate::Connecting_WaitingDiscovery : ReFixConnSubstate::Connected;
        conn.userData = 0;
        memset(conn.name, 0, sizeof(conn.name));
        conn.messageCountOut = 0;
        conn.pollGroup = 0;
        conn.isInitiator = true;
        conn.connectionStartTime = std::chrono::steady_clock::now();
        conn.lastHandshakeSendTime = conn.connectionStartTime;
        conn.handshakeRetries = 0;
        conn.nextReliableSequenceOut = 1;
        conn.highestReliableAckReceived = 0;
        conn.nextReliableSequenceExpected = 1;
        conn.pingMs = -1;

        g_connections[handle] = conn;
        g_sessionToConnection[sessionId] = handle;
        return handle;
    }

    static void PollNetwork() {
        if (g_udpSocket == INVALID_SOCKET) return;

        char recvBuf[65536];
        sockaddr_in fromAddr = {};
        int fromLen;

        while (true) {
            fromLen = sizeof(fromAddr);
            int ret = recvfrom(g_udpSocket, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&fromAddr, &fromLen);
            if (ret <= 0) break;

            if (ret >= (int)sizeof(NetPacketHeader)) {
                NetPacketHeader* hdr = (NetPacketHeader*)recvBuf;
                if (hdr->magic == 0x52464958) {
                    if (hdr->senderID == g_localSteamID) continue; // Ignore own echoes

                    uint8_t* payload = (uint8_t*)recvBuf + sizeof(NetPacketHeader);
                    size_t pLen = ret - sizeof(NetPacketHeader);

                    ReFixLog("[PollNetwork] Received msgType=%d from=%llu payloadLen=%zu", hdr->msgType, hdr->senderID, pLen);

                    uint32_t senderIp = ntohl(fromAddr.sin_addr.s_addr);
                    uint16_t senderPort = ntohs(fromAddr.sin_port);

                    // BLOCKER 12: Discovery Source Policy
                    // 1. DISCOVERY (msgType 1 [Ping/Beacon] or 2 [Lobby Announcement])
                    //    can establish or update peer endpoints.
                    if (hdr->msgType == 1 || hdr->msgType == 2) {
                        LobbyDataUpdate_t dataUpd = {};
                        bool hasUpdates = false;
                        {
                            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                        DiscoveredPeer& peer = g_peers[hdr->senderID];
                        peer.steamID = hdr->senderID;
                        peer.ip = senderIp;
                        peer.port = senderPort;
                        peer.lastSeen = std::chrono::steady_clock::now();

                        if (hdr->msgType == 1 && pLen > 0) { // Ping with persona name
                            peer.personaName.assign((char*)payload, pLen);
                        } else if (hdr->msgType == 2 && pLen > 0) { // Lobby Announcement
                            std::string meta((char*)payload, pLen);
                            std::stringstream ss(meta);
                            std::string firstLine;
                            if (std::getline(ss, firstLine)) {
                                std::stringstream fss(firstLine);
                                uint64_t lID = 0, lOwner = 0;
                                int lMax = 4;
                                std::string ownerName;
                                fss >> lID >> lOwner >> lMax;
                                if (fss >> ownerName && !ownerName.empty()) {
                                    peer.personaName = ownerName;
                                }
                                if (lID != 0) {
                                    LobbyInfo& lob = g_lobbies[lID];
                                    lob.id = lID;
                                    lob.owner = lOwner;
                                    lob.maxMembers = lMax;
                                    lob.lastSeen = std::chrono::steady_clock::now();
                                    if (std::find(lob.members.begin(), lob.members.end(), lOwner) == lob.members.end()) {
                                        lob.members.push_back(lOwner);
                                    }
                                    std::string line;
                                    bool hasUpdates = false;
                                    while (std::getline(ss, line)) {
                                        size_t eqPos = line.find('=');
                                        if (eqPos != std::string::npos && eqPos > 0) {
                                            std::string k = line.substr(0, eqPos);
                                            std::string v = line.substr(eqPos + 1);
                                            lob.data[k] = v;
                                            hasUpdates = true;
                                        }
                                    }
                                    if (hasUpdates) {
                                        dataUpd.m_ulSteamIDLobby = lID;
                                        dataUpd.m_ulSteamIDMember = lID;
                                        dataUpd.m_bSuccess = 1;
                                    }
                                }
                            }
                        }
                        }
                        if (hasUpdates) {
                            PostCallback(LobbyDataUpdate_t::k_iCallback, &dataUpd, sizeof(dataUpd));
                        }
                    } else if (hdr->msgType == 7 || hdr->msgType == 8) {
                        // 2. HANDSHAKE (7=Handshake, 8=Handshake ACK)
                        //    Only allowed if peer/endpoint is coherent.
                        //    Do NOT allow a packet to silently change the endpoint of an already discovered peer!
                        bool endpointCoherent = true;
                        {
                            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                            auto it = g_peers.find(hdr->senderID);
                            if (it != g_peers.end()) {
                                if (it->second.ip != senderIp || it->second.port != senderPort) {
                                    ReFixLog("[PollNetwork] INCOHERENT handshake endpoint from steamID=%llu! Known=%u:%u, Received=%u:%u. Dropping packet.",
                                             hdr->senderID, it->second.ip, it->second.port, senderIp, senderPort);
                                    endpointCoherent = false;
                                } else {
                                    it->second.lastSeen = std::chrono::steady_clock::now();
                                }
                            } else {
                                // First-time establishment via handshake
                                DiscoveredPeer& peer = g_peers[hdr->senderID];
                                peer.steamID = hdr->senderID;
                                peer.ip = senderIp;
                                peer.port = senderPort;
                                peer.lastSeen = std::chrono::steady_clock::now();
                            }
                        }
                        if (!endpointCoherent) {
                            continue; // Drop incoherent handshake packet!
                        }
                    } else if (hdr->msgType == 6 || hdr->msgType == 9 || hdr->msgType == 5) {
                        // 3. DATA / ACK (6=Sockets Payload, 9=Sockets ACK, 5=P2P)
                        //    MUST NOT turn into discovery by themselves.
                        //    If peer already discovered, verify matching endpoint; if mismatched, reject!
                        bool endpointValid = true;
                        {
                            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                            auto it = g_peers.find(hdr->senderID);
                            if (it != g_peers.end()) {
                                if (it->second.ip != senderIp || it->second.port != senderPort) {
                                    ReFixLog("[PollNetwork] DATA/ACK rejected: mismatched endpoint for steamID=%llu! Known=%u:%u, Received=%u:%u.",
                                             hdr->senderID, it->second.ip, it->second.port, senderIp, senderPort);
                                    endpointValid = false;
                                } else {
                                    it->second.lastSeen = std::chrono::steady_clock::now();
                                }
                            }
                        }
                        if (!endpointValid) {
                            continue; // Drop data packet from mismatched endpoint!
                        }
                        if (hdr->msgType == 5 && pLen >= 4) { // P2P packet
                            int channel = *(int*)payload;
                            P2PPacket pkt;
                            pkt.senderID = hdr->senderID;
                            pkt.channel = channel;
                            pkt.data.assign(payload + 4, payload + pLen);
                            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                            g_p2pIncoming[channel].push(pkt);
                        }
                    }

                    if (hdr->msgType == 1 && pLen > 0) {
                        if (pLen < 4 || memcmp(payload, "ACK_", 4) != 0) {
                            std::string reply = "ACK_" + g_personaName;
                            sockaddr_in replyDest = {};
                            replyDest.sin_family = AF_INET;
                            replyDest.sin_port = fromAddr.sin_port;
                            replyDest.sin_addr = fromAddr.sin_addr;

                            if (IsAllowedLanAddress((const sockaddr*)&replyDest)) {
                                std::vector<uint8_t> rBuf(sizeof(NetPacketHeader) + reply.size());
                                NetPacketHeader* rHdr = (NetPacketHeader*)rBuf.data();
                                rHdr->magic = 0x52464958;
                                rHdr->msgType = 1;
                                rHdr->senderID = g_localSteamID;
                                rHdr->appID = g_appID;
                                rHdr->payloadLen = (uint32_t)reply.size();
                                memcpy(rBuf.data() + sizeof(NetPacketHeader), reply.data(), reply.size());
                                sendto(g_udpSocket, (const char*)rBuf.data(), (int)rBuf.size(), 0, (sockaddr*)&replyDest, sizeof(replyDest));
                            }
                        }
                    } else if (hdr->msgType == 3) { // Lobby Query (FIND-06)
                        uint64_t actLobby = g_activeLobbyID.load();
                        if (actLobby != 0) {
                            std::string meta;
                            {
                                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                                auto it = g_lobbies.find(actLobby);
                                if (it != g_lobbies.end() && it->second.owner == g_localSteamID) {
                                    std::stringstream ss;
                                    ss << it->second.id << " " << it->second.owner << " " << it->second.maxMembers << " " << g_personaName;
                                    for (const auto& kv : it->second.data) {
                                        ss << "\n" << kv.first << "=" << kv.second;
                                    }
                                    meta = ss.str();
                                }
                            }
                            if (!meta.empty()) {
                                sockaddr_in replyDest = {};
                                replyDest.sin_family = AF_INET;
                                replyDest.sin_port = fromAddr.sin_port;
                                replyDest.sin_addr = fromAddr.sin_addr;
                                if (IsAllowedLanAddress((const sockaddr*)&replyDest)) {
                                    std::vector<uint8_t> rBuf(sizeof(NetPacketHeader) + meta.size());
                                    NetPacketHeader* rHdr = (NetPacketHeader*)rBuf.data();
                                    rHdr->magic = 0x52464958;
                                    rHdr->msgType = 2; // LobbyAnnounce response
                                    rHdr->senderID = g_localSteamID;
                                    rHdr->appID = g_appID;
                                    rHdr->payloadLen = (uint32_t)meta.size();
                                    memcpy(rBuf.data() + sizeof(NetPacketHeader), meta.data(), meta.size());
                                    sendto(g_udpSocket, (const char*)rBuf.data(), (int)rBuf.size(), 0, (sockaddr*)&replyDest, sizeof(replyDest));
                                }
                            }
                        }
                    } else if (hdr->msgType == 7 && pLen >= sizeof(SocketsHandshake)) { // Sockets Handshake
                        SocketsHandshake* hs = (SocketsHandshake*)payload;
                        if (hs->protocolVersion == 1) {
                            CSteamID remoteID(hdr->senderID);
                            uint32_t sessionId = hs->sessionId;
                            uint32_t nonce = hs->connectionNonce;
                            bool isNew = false;
                            HSteamNetConnection hNotify = 0;
                            ConnectionSnapshot snap = {};

                            {
                                std::lock_guard<std::mutex> lock(g_socketsMutex);
                                auto it = g_sessionToConnection.find(sessionId);
                                if (it == g_sessionToConnection.end()) {
                                    hNotify = g_nextConnectionHandle++;
                                    while (hNotify == k_HSteamNetConnection_Invalid || g_connections.find(hNotify) != g_connections.end()) {
                                        hNotify = g_nextConnectionHandle++;
                                    }
                                    ReFixConnection conn;
                                    conn.handle = hNotify;
                                    conn.sessionId = sessionId;
                                    conn.connectionNonce = nonce;
                                    conn.remoteSteamID = remoteID;
                                    conn.state = k_ESteamNetworkingConnectionState_Connecting;
                                    conn.substate = ReFixConnSubstate::Connected;
                                    conn.userData = 0;
                                    memset(conn.name, 0, sizeof(conn.name));
                                    conn.messageCountOut = 0;
                                    conn.pollGroup = 0;
                                    conn.isInitiator = false;
                                    conn.connectionStartTime = std::chrono::steady_clock::now();
                                    conn.lastHandshakeSendTime = conn.connectionStartTime;
                                    conn.handshakeRetries = 0;
                                    conn.nextReliableSequenceOut = 1;
                                    conn.highestReliableAckReceived = 0;
                                    conn.nextReliableSequenceExpected = 1;
                                    conn.pingMs = -1;

                                    g_connections[hNotify] = conn;
                                    g_sessionToConnection[sessionId] = hNotify;
                                    isNew = true;

                                    snap.handle = hNotify;
                                    snap.remoteSteamID = remoteID;
                                    snap.oldState = k_ESteamNetworkingConnectionState_None;
                                    snap.newState = k_ESteamNetworkingConnectionState_Connecting;
                                }
                            }

                            if (isNew) {
                                EnqueueSocketCallbackFromSnapshot(snap);
                            }

                            // Always send Handshake ACK back UNICAST (HOST_TO_CLIENT)
                            SocketsHandshakeAck ack = { 1, sessionId, nonce };
                            SendLanPacket(remoteID, 8, &ack, sizeof(ack), ReFix::PacketDirection::HOST_TO_CLIENT);
                        }
                    } else if (hdr->msgType == 8 && pLen >= sizeof(SocketsHandshakeAck)) { // Sockets Handshake ACK
                        SocketsHandshakeAck* ack = (SocketsHandshakeAck*)payload;
                        uint32_t sessionId = ack->sessionId;
                        uint32_t nonce = ack->connectionNonce;
                        CSteamID remoteID(hdr->senderID);
                        bool stateChanged = false;
                        ConnectionSnapshot snap = {};

                        {
                            std::lock_guard<std::mutex> lock(g_socketsMutex);
                            auto it = g_sessionToConnection.find(sessionId);
                            if (it != g_sessionToConnection.end()) {
                                ReFixConnection& conn = g_connections[it->second];
                                // BLOQUEANTE 2 & 8: Validate remote SteamID64, sessionId, and connectionNonce!
                                if (conn.remoteSteamID == remoteID && conn.connectionNonce == nonce) {
                                    if (conn.state == k_ESteamNetworkingConnectionState_Connecting) {
                                        snap.handle = conn.handle;
                                        snap.remoteSteamID = conn.remoteSteamID;
                                        snap.userData = conn.userData;
                                        strncpy_s(snap.name, sizeof(snap.name), conn.name, _TRUNCATE);
                                        snap.oldState = conn.state;
                                        snap.newState = k_ESteamNetworkingConnectionState_Connected;

                                        conn.state = k_ESteamNetworkingConnectionState_Connected;
                                        conn.substate = ReFixConnSubstate::Connected;
                                        stateChanged = true;

                                        // Measured Initial Handshake RTT: round-trip duration from client handshake transmission to handshake ACK reception
                                        auto now = std::chrono::steady_clock::now();
                                        conn.pingMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - conn.lastHandshakeSendTime).count();
                                    }
                                } else {
                                    ReFixLog("[PollNetwork] REJECTED handshake ACK: nonce or remoteID mismatch!");
                                }
                            }
                        }

                        if (stateChanged) {
                            EnqueueSocketCallbackFromSnapshot(snap);
                        }
                    } else if (hdr->msgType == 9 && pLen >= sizeof(SocketsAckPacket)) { // Sockets Data ACK
                        SocketsAckPacket* ackPkt = (SocketsAckPacket*)payload;
                        std::lock_guard<std::mutex> lock(g_socketsMutex);
                        auto it = g_sessionToConnection.find(ackPkt->sessionId);
                        if (it != g_sessionToConnection.end()) {
                            ReFixConnection& conn = g_connections[it->second];
                            if (conn.remoteSteamID == CSteamID(hdr->senderID)) {
                                if (ackPkt->ackSequence > conn.highestReliableAckReceived) {
                                    conn.highestReliableAckReceived = ackPkt->ackSequence;
                                    auto now = std::chrono::steady_clock::now();
                                    for (const auto& rOut : conn.unackedOutbound) {
                                        if (rOut.sequence <= ackPkt->ackSequence) {
                                            int64_t rtt = std::chrono::duration_cast<std::chrono::milliseconds>(now - rOut.lastSendTime).count();
                                            if (rtt >= 0) conn.pingMs = rtt;
                                        }
                                    }
                                    conn.unackedOutbound.erase(
                                        std::remove_if(conn.unackedOutbound.begin(), conn.unackedOutbound.end(),
                                            [&](const ReliablePacketOut& rOut) { return rOut.sequence <= ackPkt->ackSequence; }),
                                        conn.unackedOutbound.end());
                                }
                            }
                        }
                    } else if (hdr->msgType == 6 && pLen >= (sizeof(uint32_t) + sizeof(SocketsPayloadHeader))) { // Sockets Payload
                        uint32_t sessionId = *(uint32_t*)payload;
                        SocketsPayloadHeader head = *(SocketsPayloadHeader*)(payload + sizeof(uint32_t));
                        uint32_t payloadDataLen = pLen - sizeof(uint32_t) - sizeof(SocketsPayloadHeader);
                        const uint8_t* payloadData = payload + sizeof(uint32_t) + sizeof(SocketsPayloadHeader);

                        CSteamID remoteID(hdr->senderID);
                        bool sendAck = false;
                        uint64_t ackSeqToSend = 0;
                        bool connIsInitiator = false;

                        {
                            std::lock_guard<std::mutex> sockLock(g_socketsMutex);
                            auto it = g_sessionToConnection.find(sessionId);
                            if (it != g_sessionToConnection.end() && g_connections[it->second].state == k_ESteamNetworkingConnectionState_Connected) {
                                ReFixConnection& conn = g_connections[it->second];
                                connIsInitiator = conn.isInitiator;
                                if (conn.remoteSteamID == remoteID) {
                                    // 1. Process piggybacked ACK
                                    if (head.ack > conn.highestReliableAckReceived) {
                                        conn.highestReliableAckReceived = head.ack;
                                        conn.unackedOutbound.erase(
                                            std::remove_if(conn.unackedOutbound.begin(), conn.unackedOutbound.end(),
                                                [&](const ReliablePacketOut& rOut) { return rOut.sequence <= head.ack; }),
                                            conn.unackedOutbound.end());
                                    }

                                    bool isReliable = (head.flags & k_nSteamNetworkingSend_Reliable) != 0;

                                    if (isReliable) {
                                        sendAck = true;
                                        if (head.sequence < conn.nextReliableSequenceExpected) {
                                            // Duplicate reliable: re-ACK with current expected - 1
                                            ackSeqToSend = conn.nextReliableSequenceExpected > 0 ? (conn.nextReliableSequenceExpected - 1) : 0;
                                        } else if (head.sequence > conn.nextReliableSequenceExpected) {
                                            // Out of order: buffer it
                                            if (conn.outOfOrderInbound.find(head.sequence) == conn.outOfOrderInbound.end()) {
                                                SteamNetworkingMessage_t* msg = (SteamNetworkingMessage_t*)malloc(sizeof(SteamNetworkingMessage_t) + payloadDataLen);
                                                memset(msg, 0, sizeof(SteamNetworkingMessage_t));
                                                msg->m_pData = (void*)(msg + 1);
                                                memcpy(msg->m_pData, payloadData, payloadDataLen);
                                                msg->m_cbSize = payloadDataLen;
                                                msg->m_conn = it->second;
                                                msg->m_identityPeer.SetSteamID64(hdr->senderID);
                                                msg->m_pfnFreeData = nullptr;
                                                msg->m_pfnRelease = ReleaseReFixMessage;
                                                msg->m_nChannel = head.channel;
                                                msg->m_nFlags = head.flags;
                                                msg->m_nMessageNumber = head.messageNumber;

                                                ReFix::MessageTracker::Get().TrackAlloc(msg, sizeof(SteamNetworkingMessage_t) + payloadDataLen, "OutOfOrderInbound");
                                                conn.outOfOrderInbound[head.sequence] = msg;
                                            }
                                            ackSeqToSend = conn.nextReliableSequenceExpected > 0 ? (conn.nextReliableSequenceExpected - 1) : 0;
                                        } else {
                                            // In order reliable packet!
                                            SteamNetworkingMessage_t* msg = (SteamNetworkingMessage_t*)malloc(sizeof(SteamNetworkingMessage_t) + payloadDataLen);
                                            memset(msg, 0, sizeof(SteamNetworkingMessage_t));
                                            msg->m_pData = (void*)(msg + 1);
                                            memcpy(msg->m_pData, payloadData, payloadDataLen);
                                            msg->m_cbSize = payloadDataLen;
                                            msg->m_conn = it->second;
                                            msg->m_identityPeer.SetSteamID64(hdr->senderID);
                                            msg->m_pfnFreeData = nullptr;
                                            msg->m_pfnRelease = ReleaseReFixMessage;
                                            msg->m_nChannel = head.channel;
                                            msg->m_nFlags = head.flags;
                                            msg->m_nMessageNumber = head.messageNumber;

                                            ReFix::MessageTracker::Get().TrackAlloc(msg, sizeof(SteamNetworkingMessage_t) + payloadDataLen, "InOrderReliable");
                                            conn.incomingMessages.push(msg);

                                            conn.nextReliableSequenceExpected++;

                                            // Drain any buffered contiguous out of order packets
                                            while (conn.outOfOrderInbound.find(conn.nextReliableSequenceExpected) != conn.outOfOrderInbound.end()) {
                                                conn.incomingMessages.push(conn.outOfOrderInbound[conn.nextReliableSequenceExpected]);
                                                conn.outOfOrderInbound.erase(conn.nextReliableSequenceExpected);
                                                conn.nextReliableSequenceExpected++;
                                            }
                                            ackSeqToSend = conn.nextReliableSequenceExpected - 1;
                                        }
                                    } else {
                                        // Unreliable packet! Deliver immediately, do not touch reliable sequence!
                                        SteamNetworkingMessage_t* msg = (SteamNetworkingMessage_t*)malloc(sizeof(SteamNetworkingMessage_t) + payloadDataLen);
                                        memset(msg, 0, sizeof(SteamNetworkingMessage_t));
                                        msg->m_pData = (void*)(msg + 1);
                                        memcpy(msg->m_pData, payloadData, payloadDataLen);
                                        msg->m_cbSize = payloadDataLen;
                                        msg->m_conn = it->second;
                                        msg->m_identityPeer.SetSteamID64(hdr->senderID);
                                        msg->m_pfnFreeData = nullptr;
                                        msg->m_pfnRelease = ReleaseReFixMessage;
                                        msg->m_nChannel = head.channel;
                                        msg->m_nFlags = head.flags;
                                        msg->m_nMessageNumber = head.messageNumber;

                                        ReFix::MessageTracker::Get().TrackAlloc(msg, sizeof(SteamNetworkingMessage_t) + payloadDataLen, "UnreliableMessage");
                                        conn.incomingMessages.push(msg);
                                    }
                                }
                            }
                        }

                        if (sendAck) {
                            SocketsAckPacket ackPkt = { sessionId, ackSeqToSend };
                            ReFix::PacketDirection ackDir = connIsInitiator ? ReFix::PacketDirection::CLIENT_TO_HOST : ReFix::PacketDirection::HOST_TO_CLIENT;
                            SendLanPacket(remoteID, 9, &ackPkt, sizeof(ackPkt), ackDir);
                        }
                    }
                }
            }
        }

        // Retransmission Tick
        struct PendingSend {
            CSteamID remoteSteamID;
            uint8_t msgType;
            std::vector<uint8_t> payload;
            ReFix::PacketDirection dir;
        };
        std::vector<PendingSend> toSend;
        std::vector<ConnectionSnapshot> timeoutsToNotify;
        bool needDiscoveryBroadcast = false;
        std::string discoveryBroadcastPayload;

        // Snapshot known resolved peer IDs under g_emuMutex FIRST (Strict lock hierarchy: zero nesting!)
        std::vector<uint64_t> resolvedPeersList;
        {
            std::lock_guard<std::recursive_mutex> emuLock(g_emuMutex);
            for (const auto& kv : g_peers) {
                if (kv.second.ip != 0 && kv.second.port != 0) {
                    resolvedPeersList.push_back(kv.first);
                }
            }
        }

        {
            auto now = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            for (auto& pair : g_connections) {
                ReFixConnection& conn = pair.second;
                if (conn.state == k_ESteamNetworkingConnectionState_Connecting) {
                    if (conn.substate == ReFixConnSubstate::Connecting_WaitingDiscovery) {
                        uint64_t targetPeer = conn.remoteSteamID.ConvertToUint64();
                        bool peerResolved = false;
                        for (uint64_t pId : resolvedPeersList) {
                            if (pId == targetPeer) {
                                peerResolved = true;
                                break;
                            }
                        }

                        if (peerResolved) {
                            conn.substate = ReFixConnSubstate::Connecting_HandshakeSent;
                            conn.lastHandshakeSendTime = now;
                            conn.handshakeRetries = 0;

                            SocketsHandshake hs;
                            hs.protocolVersion = 1;
                            hs.sessionId = conn.sessionId;
                            hs.connectionNonce = conn.connectionNonce;
                            hs.capabilities = 0;
                            hs.remotePeerId = conn.remoteSteamID.ConvertToUint64();

                            std::vector<uint8_t> hsData((uint8_t*)&hs, (uint8_t*)&hs + sizeof(hs));
                            toSend.push_back({ conn.remoteSteamID, 7, std::move(hsData), ReFix::PacketDirection::CLIENT_TO_HOST });
                        } else {
                            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - conn.lastHandshakeSendTime).count() > 100) {
                                conn.lastHandshakeSendTime = now;
                                needDiscoveryBroadcast = true;
                                discoveryBroadcastPayload = g_personaName;
                            }
                        }
                    } else if (conn.substate == ReFixConnSubstate::Connecting_HandshakeSent) {
                        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - conn.connectionStartTime).count();
                        if (elapsed > 5000) {
                            conn.state = k_ESteamNetworkingConnectionState_ProblemDetectedLocally;
                            conn.substate = ReFixConnSubstate::Closed;
                            ConnectionSnapshot snap = {};
                            snap.handle = conn.handle;
                            snap.remoteSteamID = conn.remoteSteamID;
                            snap.userData = conn.userData;
                            snap.oldState = k_ESteamNetworkingConnectionState_Connecting;
                            snap.newState = k_ESteamNetworkingConnectionState_ProblemDetectedLocally;
                            snap.endReason = k_ESteamNetConnectionEnd_Misc_Timeout;
                            strcpy_s(snap.debugMsg, "Handshake timeout");
                            timeoutsToNotify.push_back(snap);
                        } else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - conn.lastHandshakeSendTime).count() > 150) {
                            conn.lastHandshakeSendTime = now;
                            conn.handshakeRetries++;

                            SocketsHandshake hs;
                            hs.protocolVersion = 1;
                            hs.sessionId = conn.sessionId;
                            hs.connectionNonce = conn.connectionNonce;
                            hs.capabilities = 0;
                            hs.remotePeerId = conn.remoteSteamID.ConvertToUint64();

                            std::vector<uint8_t> hsData((uint8_t*)&hs, (uint8_t*)&hs + sizeof(hs));
                            toSend.push_back({ conn.remoteSteamID, 7, std::move(hsData), ReFix::PacketDirection::CLIENT_TO_HOST });
                        }
                    }
                } else if (conn.state == k_ESteamNetworkingConnectionState_Connected) {
                    bool connTimedOut = false;
                    for (auto& rOut : conn.unackedOutbound) {
                        if (rOut.retries > 20) {
                            connTimedOut = true;
                            break;
                        }
                    }
                    if (connTimedOut) {
                        conn.state = k_ESteamNetworkingConnectionState_ProblemDetectedLocally;
                        conn.substate = ReFixConnSubstate::Closed;
                        ConnectionSnapshot snap = {};
                        snap.handle = conn.handle;
                        snap.remoteSteamID = conn.remoteSteamID;
                        snap.userData = conn.userData;
                        snap.oldState = k_ESteamNetworkingConnectionState_Connected;
                        snap.newState = k_ESteamNetworkingConnectionState_ProblemDetectedLocally;
                        snap.endReason = k_ESteamNetConnectionEnd_Misc_Timeout;
                        strcpy_s(snap.debugMsg, "Reliable retransmission timeout");
                        timeoutsToNotify.push_back(snap);
                    } else {
                        for (auto& rOut : conn.unackedOutbound) {
                            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - rOut.lastSendTime).count() > 100) {
                                SocketsPayloadHeader head = {};
                                head.flags = k_nSteamNetworkingSend_Reliable;
                                head.messageNumber = 0;
                                head.sequence = rOut.sequence;
                                head.ack = conn.nextReliableSequenceExpected > 0 ? (conn.nextReliableSequenceExpected - 1) : 0;

                                std::vector<uint8_t> payload(sizeof(uint32_t) + sizeof(SocketsPayloadHeader) + rOut.data.size());
                                uint8_t* ptr = payload.data();
                                *(uint32_t*)ptr = conn.sessionId; ptr += sizeof(uint32_t);
                                memcpy(ptr, &head, sizeof(SocketsPayloadHeader)); ptr += sizeof(SocketsPayloadHeader);
                                memcpy(ptr, rOut.data.data(), rOut.data.size());

                                ReFix::PacketDirection dir = conn.isInitiator ? ReFix::PacketDirection::CLIENT_TO_HOST : ReFix::PacketDirection::HOST_TO_CLIENT;
                                toSend.push_back({ conn.remoteSteamID, 6, std::move(payload), dir });

                                rOut.lastSendTime = now;
                                rOut.retries++;
                            }
                        }
                    }
                }
            }
        }

        // Discovery broadcast OUTSIDE of any lock (No I/O under lock!)
        if (needDiscoveryBroadcast) {
            BroadcastNetPacket(1, discoveryBroadcastPayload.c_str(), discoveryBroadcastPayload.size());
        }

        // Notify timeouts outside lock
        for (const auto& snap : timeoutsToNotify) {
            EnqueueSocketCallbackFromSnapshot(snap);
        }

        // Send all retransmitted packets outside of g_socketsMutex lock with exact direction
        for (const auto& item : toSend) {
            SendLanPacket(item.remoteSteamID, item.msgType, item.payload.data(), item.payload.size(), item.dir);
        }
    }

    void RunCallbacks() {
        RecordCall();
        if (!g_bInitialized) return;

        refix::lan::ILanCore::Get().Tick();
        PollNetwork();

        auto now = std::chrono::steady_clock::now();

        // 0a. Process pending async matchmaking searches (gathering window >= 150ms)
        std::vector<PendingSearch> readySearches;
        {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            for (auto it = g_pendingSearches.begin(); it != g_pendingSearches.end(); ) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->startTime).count() >= 150) {
                    readySearches.push_back(*it);
                    it = g_pendingSearches.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (const auto& ps : readySearches) {
            auto matches = refix::lan::ILanCore::Get().Matchmaking().SearchLobbies(ps.criteria);
            std::vector<uint64_t> matchingLobbyIDs;
            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                for (const auto& rec : matches) {
                    uint64_t sId = EnsureSteamLobbyID(rec.lobbyId);
                    if (sId == 0) continue;
                    LobbyInfo& lob = g_lobbies[sId];
                    lob.id = sId;
                    lob.maxMembers = rec.maxMembers;
                    lob.joinable = rec.joinable;
                    lob.lastSeen = now;
                    for (const auto& [k, v] : rec.attributes) {
                        lob.data[k] = v.asString;
                    }
                    matchingLobbyIDs.push_back(sId);
                }
                g_lastMatchmakingResults = matchingLobbyIDs;
            }

            LobbyMatchList_t resp = {};
            resp.m_nLobbiesMatching = (uint32_t)matchingLobbyIDs.size();
            CompleteCallResult(ps.callHandle, LobbyMatchList_t::k_iCallback, &resp, sizeof(resp), false);
        }

        // 0b. Process pending join timeouts (> 5000ms)
        {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            for (auto it = g_pendingJoins.begin(); it != g_pendingJoins.end(); ) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.requestTime).count() > 5000) {
                    uint64_t callHandle = it->second.callHandle;
                    uint64_t sId = it->second.steamLobbyId;
                    it = g_pendingJoins.erase(it);

                    LobbyEnter_t resp = {};
                    resp.m_ulSteamIDLobby = sId;
                    resp.m_rgfChatPermissions = 0xFFFFFFFF;
                    resp.m_bLocked = false;
                    resp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseError;

                    PostCallback(LobbyEnter_t::k_iCallback, &resp, sizeof(resp));
                    if (callHandle != 0) {
                        auto itCR = g_callResultMap.find(callHandle);
                        if (itCR != g_callResultMap.end()) {
                            itCR->second.failed = true;
                            itCR->second.completed = false;
                            itCR->second.triggerTime = now;
                            itCR->second.data.assign((const uint8_t*)&resp, (const uint8_t*)&resp + sizeof(resp));
                        }
                    }
                    ReFixLog("[UnrealSteam] JoinLobby timed out for lobby %llu", sId);
                } else {
                    ++it;
                }
            }
        }

        struct CallResultDispatch {
            CCallbackBase* pListener;
            std::vector<uint8_t> data;
            bool failed;
            uint64_t hAPICall;
        };
        std::vector<CallResultDispatch> crToDispatch;

        struct CallbackCompletedDispatch {
            CCallbackBase* pCallback;
            SteamAPICallCompleted_t cc;
        };
        std::vector<CallbackCompletedDispatch> ccToDispatch;

        struct CallbackDispatch {
            CCallbackBase* pCallback;
            std::vector<uint8_t> data;
        };
        std::vector<CallbackDispatch> cbToDispatch;

        {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);

            // 1. Process CallResults
            for (auto& pair : g_callResultMap) {
                auto& cr = pair.second;
                if (!cr.completed && now >= cr.triggerTime) {
                    cr.completed = true;

                    auto itListener = g_callResultListeners.find(cr.hAPICall);
                    if (itListener != g_callResultListeners.end() && itListener->second) {
                        crToDispatch.push_back({ itListener->second, cr.data, cr.failed, cr.hAPICall });
                    }

                    // Trigger SteamAPICallCompleted_t (callback 703)
                    SteamAPICallCompleted_t cc = {};
                    cc.m_hAsyncCall = cr.hAPICall;
                    cc.m_iCallback = cr.iCallback;
                    cc.m_cubParam = (uint32_t)cr.data.size();

                    auto range = g_clientCallbacks.equal_range(SteamAPICallCompleted_t::k_iCallback);
                    for (auto it = range.first; it != range.second; ++it) {
                        if (it->second) ccToDispatch.push_back({ it->second, cc });
                    }

                    // Enqueue into g_manualCallbackQueue for manual dispatch callers (FIND-03)
                    QueuedCallbackItem manualItem;
                    manualItem.iCallback = SteamAPICallCompleted_t::k_iCallback;
                    manualItem.data.assign((const uint8_t*)&cc, (const uint8_t*)&cc + sizeof(cc));
                    manualItem.triggerTime = now;
                    manualItem.isGameServer = false;
                    g_manualCallbackQueue.push_back(manualItem);
                }
            }

            // Prune old completed CallResults older than 30s to prevent unbounded memory growth (FIND-04)
            for (auto it = g_callResultMap.begin(); it != g_callResultMap.end(); ) {
                if (it->second.completed && now >= (it->second.triggerTime + std::chrono::seconds(30))) {
                    it = g_callResultMap.erase(it);
                } else {
                    ++it;
                }
            }

            // 2. Process generic Callbacks
            for (auto it = g_callbackQueue.begin(); it != g_callbackQueue.end(); ) {
                if (now >= it->triggerTime) {
                    auto range = g_clientCallbacks.equal_range(it->iCallback);
                    for (auto cbIt = range.first; cbIt != range.second; ++cbIt) {
                        if (cbIt->second) cbToDispatch.push_back({ cbIt->second, it->data });
                    }
                    it = g_callbackQueue.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Execute all dispatches with g_emuMutex RELEASED to eliminate lock inversion
        for (auto& item : crToDispatch) {
            item.pListener->Run(item.data.data(), item.failed, item.hAPICall);
        }
        for (auto& item : ccToDispatch) {
            item.pCallback->Run(&item.cc);
        }
        for (auto& item : cbToDispatch) {
            item.pCallback->Run(item.data.data());
        }
    }

    void GameServer_RunCallbacks() {
        if (!g_bInitialized) return;

        refix::lan::ILanCore::Get().Tick();
        PollNetwork();
        auto now = std::chrono::steady_clock::now();

        struct ServerCallbackDispatch {
            CCallbackBase* pCallback;
            std::vector<uint8_t> data;
        };
        std::vector<ServerCallbackDispatch> serverCbToDispatch;

        {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            for (auto it = g_callbackQueue.begin(); it != g_callbackQueue.end(); ) {
                if (it->isGameServer && now >= it->triggerTime) {
                    auto range = g_serverCallbacks.equal_range(it->iCallback);
                    for (auto cbIt = range.first; cbIt != range.second; ++cbIt) {
                        if (cbIt->second) serverCbToDispatch.push_back({ cbIt->second, it->data });
                    }
                    it = g_callbackQueue.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (auto& item : serverCbToDispatch) {
            item.pCallback->Run(item.data.data());
        }
    }

    void ManualDispatch_RunFrame(int32_t hSteamPipe) {
        RunCallbacks();
    }

    bool ManualDispatch_GetNextCallback(int32_t hSteamPipe, void* pCallbackMsg) {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        if (!pCallbackMsg || g_manualCallbackQueue.empty()) return false;

        CallbackMsg_t* msg = (CallbackMsg_t*)pCallbackMsg;
        auto& item = g_manualCallbackQueue.front();
        msg->m_hSteamUser = g_hSteamUser;
        msg->m_iCallback = item.iCallback;
        msg->m_pubParam = item.data.data();
        msg->m_cubParam = (int)item.data.size();
        return true;
    }

    void ManualDispatch_FreeLastCallback(int32_t hSteamPipe) {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        if (!g_manualCallbackQueue.empty()) {
            g_manualCallbackQueue.erase(g_manualCallbackQueue.begin());
        }
    }

    bool ManualDispatch_GetAPICallResult(int32_t hSteamPipe, uint64_t hSteamAPICall, void *pCallback, int cubCallback, int iCallbackExpected, bool *pbFailed) {
        return GetAPICallResult(hSteamAPICall, pCallback, cubCallback, iCallbackExpected, pbFailed);
    }

    // =========================================================================
    // STEAM INTERFACE IMPLEMENTATIONS (ABI & VTABLE COMPATIBLE)
    // =========================================================================

    // --- ISteamUser ---
    class CSteamUserEmu : public ISteamUser {
    public:
        virtual HSteamUser GetHSteamUser() override { return g_hSteamUser; }
        virtual bool BLoggedOn() override { return true; }
        virtual CSteamID GetSteamID() override { return CSteamID(g_localSteamID); }

        virtual int InitiateGameConnection(void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure) override {
            if (pAuthBlob && cbMaxAuthBlob >= 152) {
                uint32_t* p = (uint32_t*)pAuthBlob;
                p[0] = 1; // version
                *(uint64_t*)(p + 1) = g_localSteamID;
                p[3] = g_appID;
                p[4] = unIPServer;
                memset((uint8_t*)pAuthBlob + 20, 0xAA, 132);
                return 152;
            }
            return 0;
        }

        virtual void TerminateGameConnection(uint32 unIPServer, uint16 usPortServer) override {}
        virtual void TrackAppUsageEvent(CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo = "") override {}

        virtual bool GetUserDataFolder(char *pchBuffer, int cubBuffer) override {
            if (pchBuffer && cubBuffer > 0) {
                strcpy_s(pchBuffer, cubBuffer, "saves");
                return true;
            }
            return false;
        }

        virtual void StartVoiceRecording() override {}
        virtual void StopVoiceRecording() override {}
        virtual EVoiceResult GetAvailableVoice(uint32 *pcbCompressed, uint32 *pcbUncompressed_Deprecated = 0, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated = 0) override {
            if (pcbCompressed) *pcbCompressed = 0;
            return k_EVoiceResultNotRecording;
        }
        virtual EVoiceResult GetVoice(bool bWantCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, bool bWantUncompressed_Deprecated = false, void *pUncompressedDestBuffer_Deprecated = 0, uint32 cbUncompressedDestBufferSize_Deprecated = 0, uint32 *nUncompressBytesWritten_Deprecated = 0, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated = 0) override {
            if (nBytesWritten) *nBytesWritten = 0;
            return k_EVoiceResultNotRecording;
        }
        virtual EVoiceResult DecompressVoice(const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, uint32 nDesiredSampleRate) override {
            if (nBytesWritten) *nBytesWritten = 0;
            return k_EVoiceResultOK;
        }
        virtual uint32 GetVoiceOptimalSampleRate() override { return 48000; }

        virtual HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket, const SteamNetworkingIdentity *pSteamNetworkingIdentity) override {
            uint32_t ticketLen = 152;
            if (pTicket && cbMaxTicket >= (int)ticketLen) {
                uint64_t steam_id = g_localSteamID;
                uint64_t token = 0xA5A5A5A5A5A5A5A5ULL ^ steam_id;
                uint32_t date = (uint32_t)::time(NULL);
                uint32_t gc_len = 20;

                uint8_t* p = (uint8_t*)pTicket;
                memset(p, 0, ticketLen);
                memcpy(p + 0, &gc_len, 4);
                memcpy(p + 4, &token, 8);
                memcpy(p + 12, &steam_id, 8);
                memcpy(p + 20, &date, 4);
                uint32_t app = g_appID;
                memcpy(p + 24, &app, 4);
                uint32_t ip = 0x0100007F; // 127.0.0.1
                memcpy(p + 28, &ip, 4);
                memcpy(p + 32, &ip, 4);
                for (size_t i = 36; i < ticketLen; i++) {
                    p[i] = (uint8_t)((token >> ((i % 8) * 8)) ^ (steam_id >> ((i % 8) * 8)) ^ (uint8_t)i);
                }
                if (pcbTicket) *pcbTicket = ticketLen;

                HAuthTicket hTicket = ++g_nextAuthTicket;
                SyncTicketToEnvironment(p, ticketLen, hTicket);

                GetAuthSessionTicketResponse_t resp = {};
                resp.m_hAuthTicket = hTicket;
                resp.m_eResult = k_EResultOK;
                PostCallback(GetAuthSessionTicketResponse_t::k_iCallback, &resp, sizeof(resp), 0.0);

                ReFixLog("[UnrealSteam] GetAuthSessionTicket: Generated ticket %u for SteamID %llu (len=%u)",
                         hTicket, g_localSteamID, ticketLen);
                return hTicket;
            }
            if (pcbTicket) *pcbTicket = 0;
            return 0;
        }

        virtual EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) override {
            ValidateAuthTicketResponse_t resp = {};
            resp.m_SteamID = steamID;
            resp.m_eAuthSessionResponse = k_EAuthSessionResponseOK;
            resp.m_OwnerSteamID = steamID;
            PostCallback(ValidateAuthTicketResponse_t::k_iCallback, &resp, sizeof(resp), 0.0);

            ReFixLog("[UnrealSteam] BeginAuthSession: Accepted session for SteamID %llu", steamID.ConvertToUint64());
            return k_EBeginAuthSessionResultOK;
        }

        virtual void EndAuthSession(CSteamID steamID) override {}
        virtual void CancelAuthTicket(HAuthTicket hAuthTicket) override {}

        virtual EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) override {
            return k_EUserHasLicenseResultHasLicense;
        }
        virtual SteamAPICall_t GetDurationControl() override { return 0; }
        virtual bool BSetDurationControlOnlineState(EDurationControlOnlineState eNewState) override { return true; }

        virtual bool BIsBehindNAT() override { return false; }
        virtual void AdvertiseGame(CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer) override {}

        virtual SteamAPICall_t RequestEncryptedAppTicket(void *pDataToInclude, int cbDataToInclude) override {
            EncryptedAppTicketResponse_t resp = {};
            resp.m_eResult = k_EResultOK;
            return PostCallResult(EncryptedAppTicketResponse_t::k_iCallback, &resp, sizeof(resp));
        }

        virtual bool GetEncryptedAppTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override {
            if (pTicket && cbMaxTicket >= 128) {
                memset(pTicket, 0xEE, 128);
                if (pcbTicket) *pcbTicket = 128;
                return true;
            }
            if (pcbTicket) *pcbTicket = 0;
            return false;
        }

        virtual int GetGameBadgeLevel(int nSeries, bool bFoil) override { return 1; }
        virtual int GetPlayerSteamLevel() override { return 10; }

        virtual SteamAPICall_t RequestStoreAuthURL(const char *pchRedirectURL) override {
            return 0;
        }

        virtual bool BIsPhoneVerified() override { return true; }
        virtual bool BIsTwoFactorEnabled() override { return true; }
        virtual bool BIsPhoneIdentifying() override { return false; }
        virtual bool BIsPhoneRequiringVerification() override { return false; }

        virtual SteamAPICall_t GetMarketEligibility() override { return 0; }

        virtual HAuthTicket GetAuthTicketForWebApi(const char *pchIdentity) override {
            HAuthTicket hTicket = ++g_nextAuthTicket;
            ReFixLog("[UnrealSteam] GetAuthTicketForWebApi: Generating ticket %u for identity '%s'",
                     hTicket, pchIdentity ? pchIdentity : "null");

            uint64_t steam_id = g_localSteamID;
            uint64_t token = 0xA5A5A5A5A5A5A5A5ULL ^ steam_id;
            uint32_t date = (uint32_t)::time(NULL);
            uint32_t gc_len = 20;

            std::vector<uint8_t> ticketData(152, 0);
            memcpy(ticketData.data() + 0, &gc_len, 4);
            memcpy(ticketData.data() + 4, &token, 8);
            memcpy(ticketData.data() + 12, &steam_id, 8);
            memcpy(ticketData.data() + 20, &date, 4);
            uint32_t app = g_appID;
            memcpy(ticketData.data() + 24, &app, 4);
            uint32_t ip = 0x0100007F; // 127.0.0.1
            memcpy(ticketData.data() + 28, &ip, 4);
            memcpy(ticketData.data() + 32, &ip, 4);
            if (pchIdentity) {
                strncpy_s((char*)(ticketData.data() + 36), ticketData.size() - 36, pchIdentity, _TRUNCATE);
            }
            for (size_t i = 80; i < ticketData.size(); i++) {
                ticketData[i] = (uint8_t)((token >> ((i % 8) * 8)) ^ (steam_id >> ((i % 8) * 8)) ^ (uint8_t)i);
            }

            SyncTicketToEnvironment(ticketData.data(), ticketData.size(), hTicket);

            GetTicketForWebApiResponse_t resp = {};
            resp.m_hAuthTicket = hTicket;
            resp.m_eResult = k_EResultOK;
            resp.m_cubTicket = (int)ticketData.size();
            memcpy(resp.m_rgubTicket, ticketData.data(), resp.m_cubTicket);

            // Post to callback queue for next frame / manual dispatch
            PostCallback(GetTicketForWebApiResponse_t::k_iCallback, &resp, sizeof(resp), 0.0);

            return hTicket;
        }
    };
    static CSteamUserEmu g_steamUserInstance;

    // --- ISteamUser021 ABI Wrapper (Fixes Slot 14 Shift for Unreal Engine 4.25-4.27) ---
    class CSteamUser021Emu : public ISteamUser021 {
    public:
        virtual HSteamUser GetHSteamUser() override { return g_steamUserInstance.GetHSteamUser(); }
        virtual bool BLoggedOn() override { return g_steamUserInstance.BLoggedOn(); }
        virtual CSteamID GetSteamID() override { return g_steamUserInstance.GetSteamID(); }
        virtual int InitiateGameConnection(void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure) override {
            return g_steamUserInstance.InitiateGameConnection(pAuthBlob, cbMaxAuthBlob, steamIDGameServer, unIPServer, usPortServer, bSecure);
        }
        virtual void TerminateGameConnection(uint32 unIPServer, uint16 usPortServer) override {
            g_steamUserInstance.TerminateGameConnection(unIPServer, usPortServer);
        }
        virtual void TrackAppUsageEvent(CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo = "") override {
            g_steamUserInstance.TrackAppUsageEvent(gameID, eAppUsageEvent, pchExtraInfo);
        }
        virtual bool GetUserDataFolder(char *pchBuffer, int cubBuffer) override {
            return g_steamUserInstance.GetUserDataFolder(pchBuffer, cubBuffer);
        }
        virtual void StartVoiceRecording() override { g_steamUserInstance.StartVoiceRecording(); }
        virtual void StopVoiceRecording() override { g_steamUserInstance.StopVoiceRecording(); }
        virtual EVoiceResult GetAvailableVoice(uint32 *pcbCompressed, uint32 *pcbUncompressed_Deprecated = 0, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated = 0) override {
            return g_steamUserInstance.GetAvailableVoice(pcbCompressed, pcbUncompressed_Deprecated, nUncompressedVoiceDesiredSampleRate_Deprecated);
        }
        virtual EVoiceResult GetVoice(bool bWantCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, bool bWantUncompressed_Deprecated = false, void *pUncompressedDestBuffer_Deprecated = 0, uint32 cbUncompressedDestBufferSize_Deprecated = 0, uint32 *nUncompressBytesWritten_Deprecated = 0, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated = 0) override {
            return g_steamUserInstance.GetVoice(bWantCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, bWantUncompressed_Deprecated, pUncompressedDestBuffer_Deprecated, cbUncompressedDestBufferSize_Deprecated, nUncompressBytesWritten_Deprecated, nUncompressedVoiceDesiredSampleRate_Deprecated);
        }
        virtual EVoiceResult DecompressVoice(const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, uint32 nDesiredSampleRate) override {
            return g_steamUserInstance.DecompressVoice(pCompressed, cbCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, nDesiredSampleRate);
        }
        virtual uint32 GetVoiceOptimalSampleRate() override { return g_steamUserInstance.GetVoiceOptimalSampleRate(); }

        // Slot 13 in ISteamUser021 (3 parameters)
        virtual HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override {
            return g_steamUserInstance.GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket, nullptr);
        }

        // Slot 14 in ISteamUser021: BeginAuthSession!
        virtual EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) override {
            return g_steamUserInstance.BeginAuthSession(pAuthTicket, cbAuthTicket, steamID);
        }

        // Slot 15 in ISteamUser021: EndAuthSession!
        virtual void EndAuthSession(CSteamID steamID) override {
            g_steamUserInstance.EndAuthSession(steamID);
        }

        // Slot 16 in ISteamUser021: CancelAuthTicket!
        virtual void CancelAuthTicket(HAuthTicket hAuthTicket) override {
            g_steamUserInstance.CancelAuthTicket(hAuthTicket);
        }

        virtual EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) override {
            return g_steamUserInstance.UserHasLicenseForApp(steamID, appID);
        }
        virtual bool BIsBehindNAT() override { return g_steamUserInstance.BIsBehindNAT(); }
        virtual void AdvertiseGame(CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer) override {
            g_steamUserInstance.AdvertiseGame(steamIDGameServer, unIPServer, usPortServer);
        }
        virtual SteamAPICall_t RequestEncryptedAppTicket(void *pDataToInclude, int cbDataToInclude) override {
            return g_steamUserInstance.RequestEncryptedAppTicket(pDataToInclude, cbDataToInclude);
        }
        virtual bool GetEncryptedAppTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override {
            return g_steamUserInstance.GetEncryptedAppTicket(pTicket, cbMaxTicket, pcbTicket);
        }
        virtual int GetGameBadgeLevel(int nSeries, bool bFoil) override { return g_steamUserInstance.GetGameBadgeLevel(nSeries, bFoil); }
        virtual int GetPlayerSteamLevel() override { return g_steamUserInstance.GetPlayerSteamLevel(); }
        virtual SteamAPICall_t RequestStoreAuthURL(const char *pchRedirectURL) override { return g_steamUserInstance.RequestStoreAuthURL(pchRedirectURL); }
        virtual bool BIsPhoneVerified() override { return g_steamUserInstance.BIsPhoneVerified(); }
        virtual bool BIsTwoFactorEnabled() override { return g_steamUserInstance.BIsTwoFactorEnabled(); }
        virtual bool BIsPhoneIdentifying() override { return g_steamUserInstance.BIsPhoneIdentifying(); }
        virtual bool BIsPhoneRequiringVerification() override { return g_steamUserInstance.BIsPhoneRequiringVerification(); }
        virtual SteamAPICall_t GetMarketEligibility() override { return g_steamUserInstance.GetMarketEligibility(); }
        virtual SteamAPICall_t GetDurationControl() override { return g_steamUserInstance.GetDurationControl(); }
        virtual bool BSetDurationControlOnlineState(EDurationControlOnlineState eNewState) override { return g_steamUserInstance.BSetDurationControlOnlineState(eNewState); }
    };
    static CSteamUser021Emu g_steamUser021Instance;

    // --- ISteamFriends ---
    class CSteamFriendsEmu : public ISteamFriends017 {
    private:
        std::map<std::string, std::string> m_richPresence;
    public:
        virtual const char *GetPersonaName() override {
            return g_personaName.c_str();
        }

        virtual SteamAPICall_t SetPersonaName(const char *pchPersonaName) override {
            if (pchPersonaName && pchPersonaName[0] != '\0') {
                g_personaName = pchPersonaName;
                SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", g_personaName.c_str());
                SetEnvironmentVariableA("REFIX_USER_NAME", g_personaName.c_str());
                SetEnvironmentVariableA("REFIX_USERNAME", g_personaName.c_str());
                SetEnvironmentVariableA("SteamPersonaName", g_personaName.c_str());
            }
            SetPersonaNameResponse_t resp = {};
            resp.m_bSuccess = true;
            resp.m_bLocalSuccess = true;
            resp.m_result = k_EResultOK;
            return PostCallResult(SetPersonaNameResponse_t::k_iCallback, &resp, sizeof(resp));
        }

        virtual EPersonaState GetPersonaState() override {
            return k_EPersonaStateOnline;
        }

        virtual int GetFriendCount(int iFriendFlags) override {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            return (int)g_peers.size();
        }

        virtual CSteamID GetFriendByIndex(int iFriend, int iFriendFlags) override {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            if (iFriend >= 0 && iFriend < (int)g_peers.size()) {
                auto it = g_peers.begin();
                std::advance(it, iFriend);
                return CSteamID(it->first);
            }
            return CSteamID();
        }

        virtual EFriendRelationship GetFriendRelationship(CSteamID steamIDFriend) override {
            return k_EFriendRelationshipFriend;
        }

        virtual EPersonaState GetFriendPersonaState(CSteamID steamIDFriend) override {
            return k_EPersonaStateOnline;
        }

        virtual const char *GetFriendPersonaName(CSteamID steamIDFriend) override {
            if (steamIDFriend.ConvertToUint64() == g_localSteamID) {
                return g_personaName.c_str();
            }
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            auto it = g_peers.find(steamIDFriend.ConvertToUint64());
            if (it != g_peers.end() && !it->second.personaName.empty()) {
                thread_local char nameBuf[128];
                strncpy_s(nameBuf, sizeof(nameBuf), it->second.personaName.c_str(), _TRUNCATE);
                return nameBuf;
            }
            return "ReFix Peer";
        }

        virtual bool GetFriendGamePlayed(CSteamID steamIDFriend, FriendGameInfo_t *pFriendGameInfo) override {
            if (!pFriendGameInfo) return false;
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            auto it = g_peers.find(steamIDFriend.ConvertToUint64());
            if (it != g_peers.end() && it->second.port != 0) {
                pFriendGameInfo->m_gameID = CGameID(g_appID);
                pFriendGameInfo->m_unGameIP = it->second.ip;
                pFriendGameInfo->m_usGamePort = it->second.port;
                pFriendGameInfo->m_usQueryPort = it->second.port;
                pFriendGameInfo->m_steamIDLobby = CSteamID(g_activeLobbyID.load());
                return true;
            }
            return false;
        }

        virtual const char *GetFriendPersonaNameHistory(CSteamID steamIDFriend, int iPersonaName) override { return ""; }
        virtual int GetFriendSteamLevel(CSteamID steamIDFriend) override { return 10; }
        virtual const char *GetPlayerNickname(CSteamID steamIDPlayer) override { return nullptr; }

        virtual int GetFriendsGroupCount() override { return 0; }
        virtual FriendsGroupID_t GetFriendsGroupIDByIndex(int iFG) override { return 0; }
        virtual const char *GetFriendsGroupName(FriendsGroupID_t friendsGroupID) override { return ""; }
        virtual int GetFriendsGroupMembersCount(FriendsGroupID_t friendsGroupID) override { return 0; }
        virtual void GetFriendsGroupMembersList(FriendsGroupID_t friendsGroupID, CSteamID *pOutSteamIDMembers, int nMembersCount) override {}

        virtual bool HasFriend(CSteamID steamIDFriend, int iFriendFlags) override { return true; }
        virtual int GetClanCount() override { return 0; }
        virtual CSteamID GetClanByIndex(int iClan) override { return CSteamID(); }
        virtual const char *GetClanName(CSteamID steamIDClan) override { return ""; }
        virtual const char *GetClanTag(CSteamID steamIDClan) override { return ""; }
        virtual bool GetClanActivityCounts(CSteamID steamIDClan, int *pnOnline, int *pnInGame, int *pnChatting) override { return false; }
        virtual SteamAPICall_t DownloadClanActivityCounts(CSteamID *psteamIDClans, int cClansToRequest) override { return 0; }

        virtual int GetFriendCountFromSource(CSteamID steamIDSource) override { return 0; }
        virtual CSteamID GetFriendFromSourceByIndex(CSteamID steamIDSource, int iFriend) override { return CSteamID(); }
        virtual bool IsUserInSource(CSteamID steamIDUser, CSteamID steamIDSource) override { return false; }

        virtual void SetInGameVoiceSpeaking(CSteamID steamIDUser, bool bSpeaking) override {}
        virtual void ActivateGameOverlay(const char *pchDialog) override {}
        virtual void ActivateGameOverlayToUser(const char *pchDialog, CSteamID steamID) override {}
        virtual void ActivateGameOverlayToWebPage(const char *pchURL, EActivateGameOverlayToWebPageMode eMode = k_EActivateGameOverlayToWebPageMode_Default) override {}
        virtual void ActivateGameOverlayToStore(AppId_t nAppID, EOverlayToStoreFlag eFlag) override {}
        virtual void SetPlayedWith(CSteamID steamIDUserPlayedWith) override {}
        virtual void ActivateGameOverlayInviteDialog(CSteamID steamIDLobby) override {}

        virtual int GetSmallFriendAvatar(CSteamID steamIDFriend) override { return 0; }
        virtual int GetMediumFriendAvatar(CSteamID steamIDFriend) override { return 0; }
        virtual int GetLargeFriendAvatar(CSteamID steamIDFriend) override { return 0; }

        virtual bool RequestUserInformation(CSteamID steamIDUser, bool bRequireNameOnly) override { return false; }
        virtual SteamAPICall_t RequestClanOfficerList(CSteamID steamIDClan) override { return 0; }
        virtual CSteamID GetClanOwner(CSteamID steamIDClan) override { return CSteamID(); }
        virtual int GetClanOfficerCount(CSteamID steamIDClan) override { return 0; }
        virtual CSteamID GetClanOfficerByIndex(CSteamID steamIDClan, int iOfficer) override { return CSteamID(); }

        virtual uint32 GetUserRestrictions() override { return 0; }

        virtual bool SetRichPresence(const char *pchKey, const char *pchValue) override {
            if (!pchKey) return false;
            if (pchValue && pchValue[0] != '\0') {
                m_richPresence[pchKey] = pchValue;
            } else {
                m_richPresence.erase(pchKey);
            }
            ReFixLog("[UnrealSteam] SetRichPresence: '%s' = '%s'", pchKey, pchValue ? pchValue : "");
            return true;
        }

        virtual void ClearRichPresence() override {
            m_richPresence.clear();
        }

        virtual const char *GetFriendRichPresence(CSteamID steamIDFriend, const char *pchKey) override {
            if (steamIDFriend.ConvertToUint64() == g_localSteamID) {
                auto it = m_richPresence.find(pchKey ? pchKey : "");
                if (it != m_richPresence.end()) return it->second.c_str();
            }
            return "";
        }

        virtual int GetFriendRichPresenceKeyCount(CSteamID steamIDFriend) override {
            if (steamIDFriend.ConvertToUint64() == g_localSteamID) {
                return (int)m_richPresence.size();
            }
            return 0;
        }

        virtual const char *GetFriendRichPresenceKeyByIndex(CSteamID steamIDFriend, int iKey) override {
            if (steamIDFriend.ConvertToUint64() == g_localSteamID && iKey >= 0 && iKey < (int)m_richPresence.size()) {
                auto it = m_richPresence.begin();
                std::advance(it, iKey);
                return it->first.c_str();
            }
            return "";
        }

        virtual void RequestFriendRichPresence(CSteamID steamIDFriend) override {}

        virtual bool InviteUserToGame(CSteamID steamIDFriend, const char *pchConnectString) override {
            ReFixLog("[UnrealSteam] InviteUserToGame: steamID=%llu, connect='%s'", steamIDFriend.ConvertToUint64(), pchConnectString ? pchConnectString : "");
            return true;
        }

        virtual int GetCoplayFriendCount() override { return 0; }
        virtual CSteamID GetCoplayFriend(int iCoplayFriend) override { return CSteamID(); }
        virtual AppId_t GetFriendCoplayGame(CSteamID steamIDFriend) override { return 0; }
        virtual int GetFriendCoplayTime(CSteamID steamIDFriend) override { return 0; }

        virtual SteamAPICall_t JoinClanChatRoom(CSteamID steamIDClan) override { return 0; }
        virtual bool LeaveClanChatRoom(CSteamID steamIDClan) override { return false; }
        virtual int GetClanChatMemberCount(CSteamID steamIDClan) override { return 0; }
        virtual CSteamID GetChatMemberByIndex(CSteamID steamIDClan, int iUser) override { return CSteamID(); }
        virtual bool SendClanChatMessage(CSteamID steamIDClanChat, const char *pchText) override { return false; }
        virtual int GetClanChatMessage(CSteamID steamIDClanChat, int iMessage, void *prgchText, int cchTextMax, EChatEntryType *peChatEntryType, CSteamID *psteamidChatter) override { return 0; }
        virtual bool IsClanChatAdmin(CSteamID steamIDClanChat, CSteamID steamIDUser) override { return false; }

        virtual bool IsClanChatWindowOpenInSteam(CSteamID steamIDClanChat) override { return false; }
        virtual bool OpenClanChatWindowInSteam(CSteamID steamIDClanChat) override { return false; }
        virtual bool CloseClanChatWindowInSteam(CSteamID steamIDClanChat) override { return false; }

        virtual bool SetListenForFriendsMessages(bool bIntercept) override { return true; }
        virtual bool ReplyToFriendMessage(CSteamID steamIDFriend, const char *pchMsgToSend) override { return false; }
        virtual int GetFriendMessage(CSteamID steamIDFriend, int iMessageID, void *pvData, int cubData, EChatEntryType *peChatEntryType) override { return 0; }

        virtual SteamAPICall_t GetFollowerCount(CSteamID steamID) override { return 0; }
        virtual SteamAPICall_t IsFollowing(CSteamID steamID) override { return 0; }
        virtual SteamAPICall_t EnumerateFollowingList(uint32 unStartIndex) override { return 0; }

        virtual bool IsClanPublic(CSteamID steamIDClan) override { return false; }
        virtual bool IsClanOfficialGameGroup(CSteamID steamIDClan) override { return false; }
        virtual int GetNumChatsWithUnreadPriorityMessages() override { return 0; }
        virtual void ActivateGameOverlayRemotePlayTogetherInviteDialog(CSteamID steamIDLobby) override {}
        virtual bool RegisterProtocolInOverlayBrowser(const char *pchProtocol) override { return false; }
        virtual void ActivateGameOverlayInviteDialogConnectString(const char *pchConnectString) override {}
        virtual SteamAPICall_t RequestEquippedProfileItems(CSteamID steamID) override { return 0; }
        virtual bool BHasEquippedProfileItem(CSteamID steamID, ECommunityProfileItemType itemType) override { return false; }
        virtual const char *GetProfileItemPropertyString(CSteamID steamID, ECommunityProfileItemType itemType, ECommunityProfileItemProperty prop) override { return ""; }
        virtual uint32 GetProfileItemPropertyUint(CSteamID steamID, ECommunityProfileItemType itemType, ECommunityProfileItemProperty prop) override { return 0; }
    };
    static CSteamFriendsEmu g_steamFriendsInstance;

    // --- ISteamUtils ---
    class CSteamUtilsEmu : public ISteamUtils {
    public:
        virtual uint32 GetSecondsSinceAppActive() override { return 60; }
        virtual uint32 GetSecondsSinceComputerActive() override { return 3600; }
        virtual EUniverse GetConnectedUniverse() override { return k_EUniversePublic; }
        virtual uint32 GetServerRealTime() override { return (uint32)::time(NULL); }
        virtual const char *GetIPCountry() override { return "US"; }
        virtual bool GetImageSize(int iImage, uint32 *pnWidth, uint32 *pnHeight) override { return false; }
        virtual bool GetImageRGBA(int iImage, uint8 *pubDest, int nDestBufferSize) override { return false; }
        virtual bool GetCSERIPPort(uint32 *unIP, uint16 *usPort) override { return false; }
        virtual uint8 GetCurrentBatteryPower() override { return 255; }
        virtual uint32 GetAppID() override { return g_appID; }
        virtual void SetOverlayNotificationPosition(ENotificationPosition eNotificationPosition) override {}

        virtual bool IsAPICallCompleted(SteamAPICall_t hSteamAPICall, bool *pbFailed) override {
            return UnrealSteamEmu::IsAPICallCompleted(hSteamAPICall, pbFailed);
        }
        virtual ESteamAPICallFailure GetAPICallFailureReason(SteamAPICall_t hSteamAPICall) override {
            return k_ESteamAPICallFailureNone;
        }
        virtual bool GetAPICallResult(SteamAPICall_t hSteamAPICall, void *pCallback, int cubCallback, int iCallbackExpected, bool *pbFailed) override {
            return UnrealSteamEmu::GetAPICallResult(hSteamAPICall, pCallback, cubCallback, iCallbackExpected, pbFailed);
        }

        virtual void RunFrame() override {}
        virtual uint32 GetIPCCallCount() override { return 0; }
        virtual void SetWarningMessageHook(SteamAPIWarningMessageHook_t pFunction) override {}
        virtual bool IsOverlayEnabled() override { return false; }
        virtual bool BOverlayNeedsPresent() override { return false; }
        virtual SteamAPICall_t CheckFileSignature(const char *szFileName) override { return 0; }
        virtual bool ShowGamepadTextInput(EGamepadTextInputMode eInputMode, EGamepadTextInputLineMode eLineInputMode, const char *pchDescription, uint32 unCharMax, const char *pchExistingText) override { return false; }
        virtual uint32 GetEnteredGamepadTextLength() override { return 0; }
        virtual bool GetEnteredGamepadTextInput(char *pchText, uint32 cchText) override { return false; }
        virtual const char *GetSteamUILanguage() override { return g_language.c_str(); }
        virtual bool IsSteamRunningInVR() override { return false; }
        virtual void SetOverlayNotificationInset(int nHorizontalInset, int nVerticalInset) override {}
        virtual bool IsSteamInBigPictureMode() override { return false; }
        virtual void StartVRDashboard() override {}
        virtual bool IsVRHeadsetStreamingEnabled() override { return false; }
        virtual void SetVRHeadsetStreamingEnabled(bool bEnabled) override {}
        virtual bool IsSteamChinaLauncher() override { return false; }
        virtual bool InitFilterText(uint32 unFilterOptions = 0) override { return true; }
        virtual int FilterText(ETextFilteringContext eContext, CSteamID sourceSteamID, const char *pchInputMessage, char *pchOutFilteredText, uint32 nByteSizeOutFilteredText) override {
            if (pchInputMessage && pchOutFilteredText && nByteSizeOutFilteredText > 0) {
                strcpy_s(pchOutFilteredText, nByteSizeOutFilteredText, pchInputMessage);
                return (int)strlen(pchOutFilteredText);
            }
            return 0;
        }
        virtual ESteamIPv6ConnectivityState GetIPv6ConnectivityState(ESteamIPv6ConnectivityProtocol eProtocol) override {
            return k_ESteamIPv6ConnectivityState_Good;
        }
        virtual bool IsSteamRunningOnSteamDeck() override { return false; }
        virtual bool ShowFloatingGamepadTextInput(EFloatingGamepadTextInputMode eKeyboardMode, int nTextFieldXPosition, int nTextFieldYPosition, int nTextFieldWidth, int nTextFieldHeight) override { return false; }
        virtual void SetGameLauncherMode(bool bLauncherMode) override {}
        virtual bool DismissFloatingGamepadTextInput() override { return false; }
        virtual bool DismissGamepadTextInput() override { return true; }
    };
    static CSteamUtilsEmu g_steamUtilsInstance;

    static refix::lan::ComparisonOp ConvertSteamComparison(ELobbyComparison cmp) {
        switch (cmp) {
            case k_ELobbyComparisonEqualToOrLessThan: return refix::lan::ComparisonOp::LessThanOrEqual;
            case k_ELobbyComparisonLessThan: return refix::lan::ComparisonOp::LessThan;
            case k_ELobbyComparisonEqual: return refix::lan::ComparisonOp::Equal;
            case k_ELobbyComparisonGreaterThan: return refix::lan::ComparisonOp::GreaterThan;
            case k_ELobbyComparisonEqualToOrGreaterThan: return refix::lan::ComparisonOp::GreaterThanOrEqual;
            case k_ELobbyComparisonNotEqual: return refix::lan::ComparisonOp::NotEqual;
            default: return refix::lan::ComparisonOp::Equal;
        }
    }

    // --- ISteamMatchmaking ---
    class CSteamMatchmakingEmu : public ISteamMatchmaking {
    public:
        virtual int GetFavoriteGameCount() override { return 0; }
        virtual bool GetFavoriteGame(int iGame, AppId_t *pnAppID, uint32 *pnIP, uint16 *pnConnPort, uint16 *pnQueryPort, uint32 *punFlags, uint32 *pRTime32LastPlayedOnServer) override { return false; }
        virtual int AddFavoriteGame(AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags, uint32 rTime32LastPlayedOnServer) override { return 0; }
        virtual bool RemoveFavoriteGame(AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags) override { return false; }

        virtual SteamAPICall_t RequestLobbyList() override {
            refix::lan::ILanCore::Get().Matchmaking().RefreshLobbyList();
            refix::lan::ILanCore::Get().Discovery().BroadcastQuery();
            if (ShouldUseLegacyFallback()) {
                BroadcastNetPacket(3, nullptr, 0); // Query lobbies on LAN (legacy fallback)
            }

            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            uint64_t hCall = ++g_nextAPICall;
            QueuedCallResultItem item;
            item.hAPICall = hCall;
            item.iCallback = LobbyMatchList_t::k_iCallback;
            item.completed = false;
            item.failed = false;
            item.triggerTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            g_callResultMap[hCall] = item;

            PendingSearch ps;
            ps.callHandle = hCall;
            ps.startTime = std::chrono::steady_clock::now();
            ps.criteria = g_pendingSearchCriteria;
            g_pendingSearches.push_back(ps);
            g_pendingSearchCriteria = refix::lan::MatchmakingCriteria(); // Reset criteria

            ReFixLog("[UnrealSteam] RequestLobbyList: Scheduled async search hCall=%llu (filters=%zu)",
                     hCall, ps.criteria.filters.size());
            return hCall;
        }

        virtual void AddRequestLobbyListStringFilter(const char *pchKeyToMatch, const char *pchValueToMatch, ELobbyComparison eComparisonType) override {
            if (!pchKeyToMatch) return;
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            refix::lan::SearchFilter f;
            f.key = pchKeyToMatch;
            f.value = refix::lan::AttributeValue(pchValueToMatch ? pchValueToMatch : "");
            f.op = ConvertSteamComparison(eComparisonType);
            g_pendingSearchCriteria.filters.push_back(f);
        }

        virtual void AddRequestLobbyListNumericalFilter(const char *pchKeyToMatch, int nValueToMatch, ELobbyComparison eComparisonType) override {
            if (!pchKeyToMatch) return;
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            refix::lan::SearchFilter f;
            f.key = pchKeyToMatch;
            f.value = refix::lan::AttributeValue((int64_t)nValueToMatch);
            f.op = ConvertSteamComparison(eComparisonType);
            g_pendingSearchCriteria.filters.push_back(f);
        }

        virtual void AddRequestLobbyListNearValueFilter(const char *pchKeyToMatch, int nValueToBeCloseTo) override {
            ReFixLog("[UnrealSteam] AddRequestLobbyListNearValueFilter: Not supported in offline LAN");
        }

        virtual void AddRequestLobbyListFilterSlotsAvailable(int nSlotsAvailable) override {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            g_pendingSearchCriteria.minAvailableSlots = (nSlotsAvailable > 0) ? (uint32_t)nSlotsAvailable : 1;
        }

        virtual void AddRequestLobbyListDistanceFilter(ELobbyDistanceFilter eLobbyDistanceFilter) override {
            ReFixLog("[UnrealSteam] AddRequestLobbyListDistanceFilter: Not applicable in offline LAN");
        }

        virtual void AddRequestLobbyListResultCountFilter(int cMaxResults) override {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            g_pendingSearchCriteria.maxResults = (cMaxResults > 0) ? (uint32_t)cMaxResults : 50;
        }

        virtual void AddRequestLobbyListCompatibleMembersFilter(CSteamID steamIDLobby) override {
            ReFixLog("[UnrealSteam] AddRequestLobbyListCompatibleMembersFilter: Not supported in offline LAN");
        }

        virtual SteamAPICall_t CreateLobby(ELobbyType eLobbyType, int cMaxMembers) override {
            std::string coreLobbyId = refix::lan::ILanCore::Get().Lobby().CreateLobby(
                cMaxMembers, refix::lan::LobbyPermissionLevel::PublicAdvertised);
            refix::lan::ILanCore::Get().Lobby().SetLobbyData(coreLobbyId, "__steam_owner", refix::lan::AttributeValue(std::to_string(g_localSteamID)));

            uint64_t lID = EnsureSteamLobbyID(coreLobbyId);
            g_activeLobbyID.store(lID);

            LobbyInfo lob = {};
            lob.id = lID;
            lob.owner = g_localSteamID;
            lob.type = eLobbyType;
            lob.maxMembers = cMaxMembers;
            lob.joinable = true;
            lob.members.push_back(g_localSteamID);
            lob.lastSeen = std::chrono::steady_clock::now();
            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                g_lobbies[lID] = lob;
            }

            // Announce on LAN (legacy compatibility)
            if (ShouldUseLegacyFallback()) {
                std::stringstream ss;
                ss << lID << " " << g_localSteamID << " " << cMaxMembers << " " << g_personaName;
                std::string meta = ss.str();
                BroadcastNetPacket(2, meta.c_str(), meta.size());
            }

            // Post CallResult and Callback
            LobbyCreated_t crResp = {};
            crResp.m_eResult = k_EResultOK;
            crResp.m_ulSteamIDLobby = lID;
            SteamAPICall_t hCall = PostCallResult(LobbyCreated_t::k_iCallback, &crResp, sizeof(crResp));

            LobbyEnter_t cbResp = {};
            cbResp.m_ulSteamIDLobby = lID;
            cbResp.m_rgfChatPermissions = 0xFFFFFFFF;
            cbResp.m_bLocked = false;
            cbResp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseSuccess;
            PostCallback(LobbyEnter_t::k_iCallback, &cbResp, sizeof(cbResp));

            ReFixLog("[UnrealSteam] CreateLobby: Created Lobby %llu (coreId='%s', max=%d, owner=%llu)",
                     lID, coreLobbyId.c_str(), cMaxMembers, g_localSteamID);
            return hCall;
        }

        virtual SteamAPICall_t JoinLobby(CSteamID steamIDLobby) override {
            uint64_t lID = steamIDLobby.ConvertToUint64();
            std::string coreLobbyId = GetCoreLobbyId(lID);

            bool isLocalHost = false;
            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                auto it = g_lobbies.find(lID);
                if (it != g_lobbies.end() && it->second.owner == g_localSteamID) {
                    isLocalHost = true;
                }
            }

            if (isLocalHost) {
                g_activeLobbyID.store(lID);
                LobbyEnter_t resp = {};
                resp.m_ulSteamIDLobby = lID;
                resp.m_rgfChatPermissions = 0xFFFFFFFF;
                resp.m_bLocked = false;
                resp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseSuccess;
                PostCallback(LobbyEnter_t::k_iCallback, &resp, sizeof(resp));
                return PostCallResult(LobbyEnter_t::k_iCallback, &resp, sizeof(resp));
            }

            uint64_t hCall = 0;
            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                hCall = ++g_nextAPICall;
                QueuedCallResultItem item;
                item.hAPICall = hCall;
                item.iCallback = LobbyEnter_t::k_iCallback;
                item.completed = false;
                item.failed = false;
                item.triggerTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                g_callResultMap[hCall] = item;

                PendingJoin pj;
                pj.callHandle = hCall;
                pj.steamLobbyId = lID;
                pj.coreLobbyId = coreLobbyId;
                pj.requestTime = std::chrono::steady_clock::now();
                if (!coreLobbyId.empty()) {
                    g_pendingJoins[coreLobbyId] = pj;
                }
            }

            bool sentReq = false;
            if (!coreLobbyId.empty()) {
                sentReq = refix::lan::ILanCore::Get().Lobby().RequestJoin(coreLobbyId);
            }

            if (!sentReq) {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                if (!coreLobbyId.empty()) g_pendingJoins.erase(coreLobbyId);
                LobbyEnter_t resp = {};
                resp.m_ulSteamIDLobby = lID;
                resp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseDoesntExist;
                PostCallback(LobbyEnter_t::k_iCallback, &resp, sizeof(resp));
                CompleteCallResult(hCall, LobbyEnter_t::k_iCallback, &resp, sizeof(resp), true);
                ReFixLog("[UnrealSteam] JoinLobby: Failed to initiate join for lobby %llu (not found in Core)", lID);
                return hCall;
            }

            ReFixLog("[UnrealSteam] JoinLobby: Requested join for lobby %llu (coreId='%s', hCall=%llu)",
                     lID, coreLobbyId.c_str(), hCall);
            return hCall;
        }

        virtual void LeaveLobby(CSteamID steamIDLobby) override {
            uint64_t lID = steamIDLobby.ConvertToUint64();
            std::string coreLobbyId = GetCoreLobbyId(lID);
            if (!coreLobbyId.empty()) {
                refix::lan::ILanCore::Get().Lobby().LeaveLobby(coreLobbyId);
            }
            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                auto it = g_lobbies.find(lID);
                if (it != g_lobbies.end()) {
                    auto mIt = std::find(it->second.members.begin(), it->second.members.end(), g_localSteamID);
                    if (mIt != it->second.members.end()) it->second.members.erase(mIt);
                }
            }
            if (g_activeLobbyID.load() == lID) g_activeLobbyID.store(0);
            ReFixLog("[UnrealSteam] LeaveLobby: Left Lobby %llu", lID);
        }

        virtual bool InviteUserToLobby(CSteamID steamIDLobby, CSteamID steamIDInvitee) override { return true; }

        virtual int GetNumLobbyMembers(CSteamID steamIDLobby) override {
            uint64_t lID = steamIDLobby.ConvertToUint64();
            std::string coreLobbyId = GetCoreLobbyId(lID);
            if (!coreLobbyId.empty()) {
                auto rec = refix::lan::ILanCore::Get().Lobby().GetLobby(coreLobbyId);
                if (rec) {
                    return (int)rec->members.size();
                }
            }
            auto it = g_lobbies.find(lID);
            if (it != g_lobbies.end()) return (int)it->second.members.size();
            return 1;
        }

        virtual CSteamID GetLobbyMemberByIndex(CSteamID steamIDLobby, int iMember) override {
            uint64_t lID = steamIDLobby.ConvertToUint64();
            std::string coreLobbyId = GetCoreLobbyId(lID);
            if (!coreLobbyId.empty()) {
                auto rec = refix::lan::ILanCore::Get().Lobby().GetLobby(coreLobbyId);
                if (rec && iMember >= 0 && iMember < (int)rec->members.size()) {
                    const auto& m = rec->members[iMember];
                    if (m.peerId == refix::lan::ILanCore::Get().Identity().GetLocalPeerId()) {
                        return CSteamID(g_localSteamID);
                    }
                    auto pInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(m.peerId);
                    if (pInfo && pInfo->externalId.numericId != 0) {
                        return CSteamID(pInfo->externalId.numericId);
                    }
                    if (m.peerId.low != 0) {
                        return CSteamID(m.peerId.low);
                    }
                }
            }
            auto it = g_lobbies.find(lID);
            if (it != g_lobbies.end() && iMember >= 0 && iMember < (int)it->second.members.size()) {
                return CSteamID(it->second.members[iMember]);
            }
            return CSteamID(g_localSteamID);
        }

        virtual const char *GetLobbyData(CSteamID steamIDLobby, const char *pchKey) override {
            if (!pchKey) return "";
            uint64_t lID = steamIDLobby.ConvertToUint64();
            std::string coreLobbyId = GetCoreLobbyId(lID);
            if (!coreLobbyId.empty()) {
                auto rec = refix::lan::ILanCore::Get().Lobby().GetLobby(coreLobbyId);
                if (rec) {
                    auto it = rec->attributes.find(pchKey);
                    if (it != rec->attributes.end()) {
                        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                        g_lobbies[lID].data[pchKey] = it->second.asString;
                        return g_lobbies[lID].data[pchKey].c_str();
                    }
                }
            }
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            auto it = g_lobbies.find(lID);
            if (it != g_lobbies.end()) {
                auto dIt = it->second.data.find(pchKey);
                if (dIt != it->second.data.end()) return dIt->second.c_str();
            }
            return "";
        }

        virtual bool SetLobbyData(CSteamID steamIDLobby, const char *pchKey, const char *pchValue) override {
            if (!pchKey) return false;
            uint64_t lID = steamIDLobby.ConvertToUint64();
            std::string coreLobbyId = GetCoreLobbyId(lID);
            if (!coreLobbyId.empty()) {
                if (pchValue) {
                    refix::lan::ILanCore::Get().Lobby().SetLobbyData(coreLobbyId, pchKey, refix::lan::AttributeValue(std::string(pchValue)));
                } else {
                    refix::lan::ILanCore::Get().Lobby().DeleteLobbyData(coreLobbyId, pchKey);
                }
            }

            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                LobbyInfo& lob = g_lobbies[lID];
                lob.id = lID;
                if (pchValue) lob.data[pchKey] = pchValue;
                else lob.data.erase(pchKey);
            }

            LobbyDataUpdate_t resp = {};
            resp.m_ulSteamIDLobby = lID;
            resp.m_ulSteamIDMember = lID;
            resp.m_bSuccess = 1;
            PostCallback(LobbyDataUpdate_t::k_iCallback, &resp, sizeof(resp));

            // Sync metadata to LAN if host (FIND-07)
            if (ShouldUseLegacyFallback()) {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                auto it = g_lobbies.find(lID);
                if (it != g_lobbies.end() && it->second.owner == g_localSteamID) {
                    std::stringstream ss;
                    ss << lID << " " << it->second.owner << " " << it->second.maxMembers << " " << g_personaName;
                    for (const auto& kv : it->second.data) {
                        ss << "\n" << kv.first << "=" << kv.second;
                    }
                    std::string meta = ss.str();
                    BroadcastNetPacket(2, meta.c_str(), meta.size());
                }
            }

            ReFixLog("[UnrealSteam] SetLobbyData: Lobby %llu, '%s' = '%s'", lID, pchKey, pchValue ? pchValue : "");
            return true;
        }

        virtual int GetLobbyDataCount(CSteamID steamIDLobby) override {
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end()) return (int)it->second.data.size();
            return 0;
        }

        virtual bool GetLobbyDataByIndex(CSteamID steamIDLobby, int iLobbyData, char *pchKey, int cchKeyBufferSize, char *pchValue, int cchValueBufferSize) override {
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end() && iLobbyData >= 0 && iLobbyData < (int)it->second.data.size()) {
                auto dIt = it->second.data.begin();
                std::advance(dIt, iLobbyData);
                if (pchKey) strcpy_s(pchKey, cchKeyBufferSize, dIt->first.c_str());
                if (pchValue) strcpy_s(pchValue, cchValueBufferSize, dIt->second.c_str());
                return true;
            }
            return false;
        }

        virtual bool DeleteLobbyData(CSteamID steamIDLobby, const char *pchKey) override {
            return SetLobbyData(steamIDLobby, pchKey, nullptr);
        }

        virtual const char *GetLobbyMemberData(CSteamID steamIDLobby, CSteamID steamIDUser, const char *pchKey) override {
            if (!pchKey) return "";
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end()) {
                auto uIt = it->second.memberData.find(steamIDUser.ConvertToUint64());
                if (uIt != it->second.memberData.end()) {
                    auto dIt = uIt->second.find(pchKey);
                    if (dIt != uIt->second.end()) return dIt->second.c_str();
                }
            }
            return "";
        }

        virtual void SetLobbyMemberData(CSteamID steamIDLobby, const char *pchKey, const char *pchValue) override {
            if (!pchKey) return;
            uint64_t lID = steamIDLobby.ConvertToUint64();
            LobbyInfo& lob = g_lobbies[lID];
            if (pchValue) lob.memberData[g_localSteamID][pchKey] = pchValue;
            else lob.memberData[g_localSteamID].erase(pchKey);
        }

        virtual bool SendLobbyChatMsg(CSteamID steamIDLobby, const void *pvMsgBody, int cubMsgBody) override {
            return true;
        }

        virtual int GetLobbyChatEntry(CSteamID steamIDLobby, int iChatID, CSteamID *pSteamIDUser, void *pvData, int cubData, EChatEntryType *peChatEntryType) override {
            if (pSteamIDUser) *pSteamIDUser = CSteamID(g_localSteamID);
            if (peChatEntryType) *peChatEntryType = k_EChatEntryTypeChatMsg;
            return 0;
        }

        virtual bool RequestLobbyData(CSteamID steamIDLobby) override { return true; }

        virtual void SetLobbyGameServer(CSteamID steamIDLobby, uint32 unGameServerIP, uint16 unGameServerPort, CSteamID steamIDGameServer) override {
            uint64_t lID = steamIDLobby.ConvertToUint64();
            LobbyInfo& lob = g_lobbies[lID];
            lob.gameServerIP = unGameServerIP;
            lob.gameServerPort = unGameServerPort;
            lob.gameServerSteamID = steamIDGameServer.ConvertToUint64();
        }

        virtual bool GetLobbyGameServer(CSteamID steamIDLobby, uint32 *punGameServerIP, uint16 *punGameServerPort, CSteamID *psteamIDGameServer) override {
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end() && it->second.gameServerPort != 0) {
                if (punGameServerIP) *punGameServerIP = it->second.gameServerIP;
                if (punGameServerPort) *punGameServerPort = it->second.gameServerPort;
                if (psteamIDGameServer) *psteamIDGameServer = CSteamID(it->second.gameServerSteamID);
                return true;
            }
            return false;
        }

        virtual bool SetLobbyMemberLimit(CSteamID steamIDLobby, int cMaxMembers) override {
            g_lobbies[steamIDLobby.ConvertToUint64()].maxMembers = cMaxMembers;
            return true;
        }

        virtual int GetLobbyMemberLimit(CSteamID steamIDLobby) override {
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end()) return it->second.maxMembers;
            return 4;
        }

        virtual bool SetLobbyType(CSteamID steamIDLobby, ELobbyType eLobbyType) override {
            g_lobbies[steamIDLobby.ConvertToUint64()].type = eLobbyType;
            return true;
        }

        virtual bool SetLobbyJoinable(CSteamID steamIDLobby, bool bLobbyJoinable) override {
            g_lobbies[steamIDLobby.ConvertToUint64()].joinable = bLobbyJoinable;
            return true;
        }

        virtual CSteamID GetLobbyOwner(CSteamID steamIDLobby) override {
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end()) return CSteamID(it->second.owner);
            return CSteamID(g_localSteamID);
        }

        virtual bool SetLobbyOwner(CSteamID steamIDLobby, CSteamID steamIDNewOwner) override {
            g_lobbies[steamIDLobby.ConvertToUint64()].owner = steamIDNewOwner.ConvertToUint64();
            return true;
        }

        virtual CSteamID GetLobbyByIndex(int iLobby) override {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            if (!g_lastMatchmakingResults.empty()) {
                if (iLobby >= 0 && iLobby < (int)g_lastMatchmakingResults.size()) {
                    return CSteamID(g_lastMatchmakingResults[iLobby]);
                }
                return CSteamID((uint64)0);
            }
            int idx = 0;
            for (const auto& pair : g_lobbies) {
                if (idx == iLobby) return CSteamID(pair.first);
                idx++;
            }
            return CSteamID((uint64)0);
        }
        virtual bool SetLinkedLobby(CSteamID steamIDLobby, CSteamID steamIDLobbyDependent) override { return true; }
    };
    static CSteamMatchmakingEmu g_steamMatchmakingInstance;

    // --- ISteamMatchmakingServers ---
    class CSteamMatchmakingServersEmu : public ISteamMatchmakingServers {
    public:
        virtual HServerListRequest RequestInternetServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) override {
            if (pRequestServersResponse) pRequestServersResponse->RefreshComplete((HServerListRequest)1, EMatchMakingServerResponse::eServerResponded);
            return (HServerListRequest)1;
        }
        virtual HServerListRequest RequestLANServerList(AppId_t iApp, ISteamMatchmakingServerListResponse *pRequestServersResponse) override {
            if (pRequestServersResponse) pRequestServersResponse->RefreshComplete((HServerListRequest)2, EMatchMakingServerResponse::eServerResponded);
            return (HServerListRequest)2;
        }
        virtual HServerListRequest RequestFriendsServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) override {
            if (pRequestServersResponse) pRequestServersResponse->RefreshComplete((HServerListRequest)3, EMatchMakingServerResponse::eServerResponded);
            return (HServerListRequest)3;
        }
        virtual HServerListRequest RequestFavoritesServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) override {
            if (pRequestServersResponse) pRequestServersResponse->RefreshComplete((HServerListRequest)4, EMatchMakingServerResponse::eServerResponded);
            return (HServerListRequest)4;
        }
        virtual HServerListRequest RequestHistoryServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) override {
            if (pRequestServersResponse) pRequestServersResponse->RefreshComplete((HServerListRequest)5, EMatchMakingServerResponse::eServerResponded);
            return (HServerListRequest)5;
        }
        virtual HServerListRequest RequestSpectatorServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) override {
            if (pRequestServersResponse) pRequestServersResponse->RefreshComplete((HServerListRequest)6, EMatchMakingServerResponse::eServerResponded);
            return (HServerListRequest)6;
        }
        virtual void ReleaseRequest(HServerListRequest hServerListRequest) override {}
        virtual gameserveritem_t *GetServerDetails(HServerListRequest hRequest, int iServer) override { return nullptr; }
        virtual void CancelQuery(HServerListRequest hRequest) override {}
        virtual void RefreshQuery(HServerListRequest hRequest) override {}
        virtual bool IsRefreshing(HServerListRequest hRequest) override { return false; }
        virtual int GetServerCount(HServerListRequest hRequest) override { return 0; }
        virtual void RefreshServer(HServerListRequest hRequest, int iServer) override {}
        virtual HServerQuery PingServer(uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse *pRequestServersResponse) override { return 1; }
        virtual HServerQuery PlayerDetails(uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse *pRequestServersResponse) override { return 1; }
        virtual HServerQuery ServerRules(uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse *pRequestServersResponse) override { return 1; }
        virtual void CancelServerQuery(HServerQuery hServerQuery) override {}
    };
    static CSteamMatchmakingServersEmu g_steamMatchmakingServersInstance;

    // --- ISteamUserStats ---
    class CSteamUserStatsEmu : public ISteamUserStats {
    private:
        std::map<std::string, int32_t> m_intStats;
        std::map<std::string, float> m_floatStats;
        std::set<std::string> m_achievements;
    public:
        virtual bool RequestCurrentStats() {
            UserStatsReceived_t resp = {};
            resp.m_nGameID = g_appID;
            resp.m_eResult = k_EResultOK;
            resp.m_steamIDUser = CSteamID(g_localSteamID);
            PostCallback(UserStatsReceived_t::k_iCallback, &resp, sizeof(resp));
            return true;
        }

        virtual bool GetStat(const char *pchName, int32 *pData) override {
            if (!pchName || !pData) return false;
            auto it = m_intStats.find(pchName);
            if (it != m_intStats.end()) { *pData = it->second; return true; }
            *pData = 0;
            return true;
        }

        virtual bool GetStat(const char *pchName, float *pData) override {
            if (!pchName || !pData) return false;
            auto it = m_floatStats.find(pchName);
            if (it != m_floatStats.end()) { *pData = it->second; return true; }
            *pData = 0.0f;
            return true;
        }

        virtual bool SetStat(const char *pchName, int32 nData) override {
            if (!pchName) return false;
            m_intStats[pchName] = nData;
            return true;
        }

        virtual bool SetStat(const char *pchName, float fData) override {
            if (!pchName) return false;
            m_floatStats[pchName] = fData;
            return true;
        }

        virtual bool UpdateAvgRateStat(const char *pchName, float flCountThisSession, double dSessionLength) override { return true; }

        virtual bool GetAchievement(const char *pchName, bool *pbAchieved) override {
            if (!pchName || !pbAchieved) return false;
            *pbAchieved = (m_achievements.find(pchName) != m_achievements.end());
            return true;
        }

        virtual bool SetAchievement(const char *pchName) override {
            if (!pchName) return false;
            m_achievements.insert(pchName);

            UserAchievementStored_t resp = {};
            resp.m_nGameID = g_appID;
            resp.m_bGroupAchievement = false;
            strcpy_s(resp.m_rgchAchievementName, sizeof(resp.m_rgchAchievementName), pchName);
            resp.m_nCurProgress = 1;
            resp.m_nMaxProgress = 1;
            PostCallback(UserAchievementStored_t::k_iCallback, &resp, sizeof(resp));

            ReFixLog("[UnrealSteam] SetAchievement: Unlocked '%s'", pchName);
            return true;
        }

        virtual bool ClearAchievement(const char *pchName) override {
            if (pchName) m_achievements.erase(pchName);
            return true;
        }

        virtual bool GetAchievementAndUnlockTime(const char *pchName, bool *pbAchieved, uint32 *punUnlockTime) override {
            if (GetAchievement(pchName, pbAchieved)) {
                if (punUnlockTime) *punUnlockTime = (uint32)::time(NULL);
                return true;
            }
            return false;
        }

        virtual bool StoreStats() override {
            UserStatsStored_t resp = {};
            resp.m_nGameID = g_appID;
            resp.m_eResult = k_EResultOK;
            PostCallback(UserStatsStored_t::k_iCallback, &resp, sizeof(resp));
            return true;
        }

        virtual int GetAchievementIcon(const char *pchName) override { return 0; }
        virtual const char *GetAchievementDisplayAttribute(const char *pchName, const char *pchKey) override { return ""; }
        virtual bool IndicateAchievementProgress(const char *pchName, uint32 nCurProgress, uint32 nMaxProgress) override { return true; }
        virtual uint32 GetNumAchievements() override { return (uint32)m_achievements.size(); }
        virtual const char *GetAchievementName(uint32 iAchievement) override { return ""; }

        virtual SteamAPICall_t RequestUserStats(CSteamID steamIDUser) override {
            UserStatsReceived_t resp = {};
            resp.m_nGameID = g_appID;
            resp.m_eResult = k_EResultOK;
            resp.m_steamIDUser = steamIDUser;
            return PostCallResult(UserStatsReceived_t::k_iCallback, &resp, sizeof(resp));
        }

        virtual bool GetUserStat(CSteamID steamIDUser, const char *pchName, int32 *pData) override { return GetStat(pchName, pData); }
        virtual bool GetUserStat(CSteamID steamIDUser, const char *pchName, float *pData) override { return GetStat(pchName, pData); }
        virtual bool GetUserAchievement(CSteamID steamIDUser, const char *pchName, bool *pbAchieved) override { return GetAchievement(pchName, pbAchieved); }
        virtual bool GetUserAchievementAndUnlockTime(CSteamID steamIDUser, const char *pchName, bool *pbAchieved, uint32 *punUnlockTime) override {
            return GetAchievementAndUnlockTime(pchName, pbAchieved, punUnlockTime);
        }
        virtual bool ResetAllStats(bool bAchievementsToo) override {
            m_intStats.clear();
            m_floatStats.clear();
            if (bAchievementsToo) m_achievements.clear();
            return true;
        }

        virtual SteamAPICall_t FindOrCreateLeaderboard(const char *pchLeaderboardName, ELeaderboardSortMethod eLeaderboardSortMethod, ELeaderboardDisplayType eLeaderboardDisplayType) override { return 0; }
        virtual SteamAPICall_t FindLeaderboard(const char *pchLeaderboardName) override { return 0; }
        virtual const char *GetLeaderboardName(SteamLeaderboard_t hSteamLeaderboard) override { return ""; }
        virtual int GetLeaderboardEntryCount(SteamLeaderboard_t hSteamLeaderboard) override { return 0; }
        virtual ELeaderboardSortMethod GetLeaderboardSortMethod(SteamLeaderboard_t hSteamLeaderboard) override { return k_ELeaderboardSortMethodNone; }
        virtual ELeaderboardDisplayType GetLeaderboardDisplayType(SteamLeaderboard_t hSteamLeaderboard) override { return k_ELeaderboardDisplayTypeNone; }
        virtual SteamAPICall_t DownloadLeaderboardEntries(SteamLeaderboard_t hSteamLeaderboard, ELeaderboardDataRequest eLeaderboardDataRequest, int nRangeStart, int nRangeEnd) override { return 0; }
        virtual SteamAPICall_t DownloadLeaderboardEntriesForUsers(SteamLeaderboard_t hSteamLeaderboard, CSteamID *prgUsers, int cUsers) override { return 0; }
        virtual bool GetDownloadedLeaderboardEntry(SteamLeaderboardEntries_t hSteamLeaderboardEntries, int index, LeaderboardEntry_t *pLeaderboardEntry, int32 *pDetails, int cDetailsMax) override { return false; }
        virtual SteamAPICall_t UploadLeaderboardScore(SteamLeaderboard_t hSteamLeaderboard, ELeaderboardUploadScoreMethod eLeaderboardUploadScoreMethod, int32 nScore, const int32 *pScoreDetails, int cScoreDetailsCount) override { return 0; }
        virtual SteamAPICall_t AttachLeaderboardUGC(SteamLeaderboard_t hSteamLeaderboard, UGCHandle_t hUGC) override { return 0; }
        virtual SteamAPICall_t GetNumberOfCurrentPlayers() override {
            NumberOfCurrentPlayers_t resp = {};
            resp.m_bSuccess = 1;
            resp.m_cPlayers = 1;
            return PostCallResult(NumberOfCurrentPlayers_t::k_iCallback, &resp, sizeof(resp));
        }
        virtual SteamAPICall_t RequestGlobalAchievementPercentages() override { return 0; }
        virtual int GetMostAchievedAchievementInfo(char *pchName, uint32 unNameBufLen, float *pflPercent, bool *pbAchieved) override { return -1; }
        virtual int GetNextMostAchievedAchievementInfo(int iIterator, char *pchName, uint32 unNameBufLen, float *pflPercent, bool *pbAchieved) override { return -1; }
        virtual bool GetAchievementAchievedPercent(const char *pchName, float *pflPercent) override { if (pflPercent) *pflPercent = 100.0f; return true; }
        virtual SteamAPICall_t RequestGlobalStats(int nHistoryDays) override { return 0; }
        virtual bool GetGlobalStat(const char *pchStatName, int64 *pData) override { return false; }
        virtual bool GetGlobalStat(const char *pchStatName, double *pData) override { return false; }
        virtual int32 GetGlobalStatHistory(const char *pchStatName, int64 *pData, uint32 cubData) override { return 0; }
        virtual int32 GetGlobalStatHistory(const char *pchStatName, double *pData, uint32 cubData) override { return 0; }
        virtual bool GetAchievementProgressLimits(const char *pchName, int32 *pnMinProgress, int32 *pnMaxProgress) override { if (pnMinProgress) *pnMinProgress = 0; if (pnMaxProgress) *pnMaxProgress = 100; return true; }
        virtual bool GetAchievementProgressLimits(const char *pchName, float *pfMinProgress, float *pfMaxProgress) override { if (pfMinProgress) *pfMinProgress = 0.0f; if (pfMaxProgress) *pfMaxProgress = 100.0f; return true; }
    };
    static CSteamUserStatsEmu g_steamUserStatsInstance;

    // --- ISteamApps ---
    class CSteamAppsEmu : public ISteamApps {
    public:
        virtual bool BIsSubscribed() override { return true; }
        virtual bool BIsLowViolence() override { return false; }
        virtual bool BIsCybercafe() override { return false; }
        virtual bool BIsVACBanned() override { return false; }
        virtual const char *GetCurrentGameLanguage() override { return g_language.c_str(); }
        virtual const char *GetAvailableGameLanguages() override {
            return "english,spanish,french,german,italian,japanese,koreana,schinese,tchinese,russian,latam";
        }
        virtual bool BIsSubscribedApp(AppId_t appID) override { return true; }
        virtual bool BIsDlcInstalled(AppId_t appID) override {
            if (g_bypassLicenseCheck) return true;
            return (g_unlockedDLCs.find(appID) != g_unlockedDLCs.end());
        }
        virtual uint32 GetEarliestPurchaseUnixTime(AppId_t nAppID) override { return 1; }
        virtual bool BIsSubscribedFromFreeWeekend() override { return false; }
        virtual int GetDLCCount() override { return (int)g_unlockedDLCs.size(); }
        virtual bool BGetDLCDataByIndex(int iDLC, AppId_t *pAppID, bool *pbAvailable, char *pchName, int cchNameBufferSize) override {
            if (iDLC >= 0 && iDLC < (int)g_unlockedDLCs.size()) {
                auto it = g_unlockedDLCs.begin();
                std::advance(it, iDLC);
                if (pAppID) *pAppID = *it;
                if (pbAvailable) *pbAvailable = true;
                if (pchName && cchNameBufferSize > 0) sprintf_s(pchName, cchNameBufferSize, "DLC %u", *it);
                return true;
            }
            return false;
        }
        virtual void InstallDLC(AppId_t nAppID) override {}
        virtual void UninstallDLC(AppId_t nAppID) override {}
        virtual void RequestAppProofOfPurchaseKey(AppId_t nAppID) override {}
        virtual bool GetCurrentBetaName(char *pchName, int cchNameBufferSize) override { return false; }
        virtual bool MarkContentCorrupt(bool bMissingFilesOnly) override { return false; }
        virtual uint32 GetInstalledDepots(AppId_t appID, DepotId_t *pvecDepots, uint32 cMaxDepots) override {
            if (pvecDepots && cMaxDepots > 0) { pvecDepots[0] = appID + 1; return 1; }
            return 0;
        }
        virtual uint32 GetAppInstallDir(AppId_t appID, char *pchFolder, uint32 cchFolderBufferSize) override {
            char exePath[MAX_PATH] = { 0 };
            GetModuleFileNameA(NULL, exePath, MAX_PATH);
            std::string path(exePath);
            size_t pos = path.find_last_of("\\/");
            if (pos != std::string::npos) path = path.substr(0, pos);
            if (pchFolder && cchFolderBufferSize > 0) {
                strcpy_s(pchFolder, cchFolderBufferSize, path.c_str());
                return (uint32)strlen(pchFolder);
            }
            return 0;
        }
        virtual bool BIsAppInstalled(AppId_t appID) override { return true; }
        virtual CSteamID GetAppOwner() override { return CSteamID(g_localSteamID); }
        virtual const char *GetLaunchQueryParam(const char *pchKey) override { return ""; }
        virtual bool GetDlcDownloadProgress(AppId_t nAppID, uint64 *punBytesDownloaded, uint64 *punBytesTotal) override {
            if (punBytesDownloaded) *punBytesDownloaded = 1000;
            if (punBytesTotal) *punBytesTotal = 1000;
            return true;
        }
        virtual int GetAppBuildId() override { return 1000; }
        virtual void RequestAllProofOfPurchaseKeys() override {}
        virtual SteamAPICall_t GetFileDetails(const char *pszFileName) override {
            FileDetailsResult_t resp = {};
            resp.m_eResult = k_EResultOK;
            resp.m_ulFileSize = 1024;
            return PostCallResult(FileDetailsResult_t::k_iCallback, &resp, sizeof(resp));
        }
        virtual int GetLaunchCommandLine(char *pszCommandLine, int cubCommandLine) override {
            const char* cmd = GetCommandLineA();
            if (pszCommandLine && cubCommandLine > 0 && cmd) {
                strcpy_s(pszCommandLine, cubCommandLine, cmd);
                return (int)strlen(pszCommandLine);
            }
            return 0;
        }
        virtual bool BIsSubscribedFromFamilySharing() override { return false; }
        virtual bool BIsTimedTrial(uint32 *punSecondsAllowed, uint32 *punSecondsPlayed) override { return false; }
        virtual bool SetDlcContext(AppId_t nAppID) override { return true; }
        virtual int GetNumBetas(int *pnAvailable, int *pnPrivate) override { if (pnAvailable) *pnAvailable = 0; if (pnPrivate) *pnPrivate = 0; return 0; }
        virtual bool GetBetaInfo(int iBetaIndex, uint32 *punFlags, uint32 *punBuildID, char *pchBetaName, int cchBetaName, char *pchDescription, int cchDescription, uint32 *punInstalledDate) override { return false; }
        virtual bool SetActiveBeta(const char *pchBetaName) override { return true; }
    };
    static CSteamAppsEmu g_steamAppsInstance;

    // --- ISteamNetworking ---
    class CSteamNetworkingEmu : public ISteamNetworking {
    public:
        virtual bool SendP2PPacket(CSteamID steamIDRemote, const void *pubData, uint32 cubData, EP2PSend eP2PSendType, int nChannel = 0) override {
            if (cubData > 0 && !pubData) return false;

            // Route through ReFix Universal LAN Core if remote peer is registered
            auto peerId = refix::lan::ILanCore::Get().Peers().FindBySteamId(steamIDRemote.ConvertToUint64());
            if (peerId.has_value()) {
                auto pInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(*peerId);
                if (pInfo.has_value() && pInfo->endpoint.IsValid()) {
                    printf("[P2P_ROUTE_LANCORE] Remote=%llu EP=%s Chan=%d Bytes=%u\n",
                           (unsigned long long)steamIDRemote.ConvertToUint64(),
                           pInfo->endpoint.ToString().c_str(), (int)nChannel, (unsigned int)cubData);
                    fflush(stdout);
                    bool reliable = (eP2PSendType == k_EP2PSendReliable || eP2PSendType == k_EP2PSendReliableWithBuffering);
                    if (reliable) {
                        return refix::lan::ILanCore::Get().Transport().SendReliable(*peerId, pInfo->endpoint, (uint8_t)nChannel, pubData, cubData);
                    } else {
                        return refix::lan::ILanCore::Get().Transport().SendUnreliable(pInfo->endpoint, (uint8_t)nChannel, pubData, cubData);
                    }
                }
            } else {
                printf("[P2P_ROUTE_FALLBACK] Remote=%llu Chan=%d Bytes=%u\n",
                       (unsigned long long)steamIDRemote.ConvertToUint64(), (int)nChannel, (unsigned int)cubData);
                fflush(stdout);
            }

            if (!ShouldUseLegacyFallback()) {
                return false;
            }

            // Fallback path: legacy SendLanPacket / BroadcastNetPacket
            std::vector<uint8_t> payload(4 + cubData);
            *(int*)payload.data() = nChannel;
            if (cubData > 0 && pubData) memcpy(payload.data() + 4, pubData, cubData);

            bool hasEndpoint = false;
            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                auto it = g_peers.find(steamIDRemote.ConvertToUint64());
                if (it != g_peers.end() && it->second.ip != 0 && it->second.port != 0) {
                    hasEndpoint = true;
                }
            }

            if (hasEndpoint) {
                SendLanPacket(steamIDRemote, 5, payload.data(), payload.size());
            } else {
                BroadcastNetPacket(5, payload.data(), payload.size());
            }
            return true;
        }

        virtual bool IsP2PPacketAvailable(uint32 *pcubMsgSize, int nChannel = 0) override {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            auto it = g_p2pIncoming.find(nChannel);
            if (it != g_p2pIncoming.end() && !it->second.empty()) {
                if (pcubMsgSize) *pcubMsgSize = (uint32)it->second.front().data.size();
                return true;
            }
            if (pcubMsgSize) *pcubMsgSize = 0;
            return false;
        }

        virtual bool ReadP2PPacket(void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, CSteamID *psteamIDRemote, int nChannel = 0) override {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            auto it = g_p2pIncoming.find(nChannel);
            if (it != g_p2pIncoming.end() && !it->second.empty()) {
                auto pkt = it->second.front();
                it->second.pop();

                size_t copyLen = (std::min)((size_t)cubDest, pkt.data.size());
                if (pubDest && copyLen > 0) memcpy(pubDest, pkt.data.data(), copyLen);
                if (pcubMsgSize) *pcubMsgSize = (uint32)pkt.data.size();
                if (psteamIDRemote) *psteamIDRemote = CSteamID(pkt.senderID);
                return true;
            }
            return false;
        }

        virtual bool AcceptP2PSessionWithUser(CSteamID steamIDRemote) override { return true; }
        virtual bool CloseP2PSessionWithUser(CSteamID steamIDRemote) override { return true; }
        virtual bool CloseP2PChannelWithUser(CSteamID steamIDRemote, int nChannel) override { return true; }

        virtual bool GetP2PSessionState(CSteamID steamIDRemote, P2PSessionState_t *pConnectionState) override {
            if (!pConnectionState) return false;
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            auto it = g_peers.find(steamIDRemote.ConvertToUint64());
            if (it != g_peers.end()) {
                pConnectionState->m_bConnectionActive = 1;
                pConnectionState->m_bConnecting = 0;
                pConnectionState->m_eP2PSessionError = 0;
                pConnectionState->m_bUsingRelay = 0;
                pConnectionState->m_nBytesQueuedForSend = 0;
                pConnectionState->m_nPacketsQueuedForSend = 0;
                pConnectionState->m_nRemoteIP = it->second.ip;
                pConnectionState->m_nRemotePort = it->second.port;
                return true;
            }
            return false;
        }

        virtual bool AllowP2PPacketRelay(bool bAllow) override { return true; }

        virtual SNetListenSocket_t CreateListenSocket(int nVirtualP2PPort, SteamIPAddress_t nIP, uint16 nPort, bool bAllowUseOfPacketRelay) override { return 1; }
        virtual SNetSocket_t CreateP2PConnectionSocket(CSteamID steamIDTarget, int nVirtualPort, int nTimeoutSec, bool bAllowUseOfPacketRelay) override { return 1; }
        virtual SNetSocket_t CreateConnectionSocket(SteamIPAddress_t nIP, uint16 nPort, int nTimeoutSec) override { return 1; }
        virtual bool DestroySocket(SNetSocket_t hSocket, bool bNotifyRemoteEnd) override { return true; }
        virtual bool DestroyListenSocket(SNetListenSocket_t hSocket, bool bNotifyRemoteEnd) override { return true; }
        virtual bool SendDataOnSocket(SNetSocket_t hSocket, void *pubData, uint32 cubData, bool bReliable) override { return true; }
        virtual bool IsDataAvailableOnSocket(SNetSocket_t hSocket, uint32 *pcubMsgSize) override { return false; }
        virtual bool RetrieveDataFromSocket(SNetSocket_t hSocket, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize) override { return false; }
        virtual bool IsDataAvailable(SNetListenSocket_t hListenSocket, uint32 *pcubMsgSize, SNetSocket_t *phSocket) override { return false; }
        virtual bool RetrieveData(SNetListenSocket_t hListenSocket, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, SNetSocket_t *phSocket) override { return false; }
        virtual bool GetSocketInfo(SNetSocket_t hSocket, CSteamID *pSteamIDRemote, int *peSocketStatus, SteamIPAddress_t *punIPRemote, uint16 *pusPortRemote) override { return false; }
        virtual bool GetListenSocketInfo(SNetListenSocket_t hListenSocket, SteamIPAddress_t *pnIP, uint16 *pnPort) override { return false; }
        virtual ESNetSocketConnectionType GetSocketConnectionType(SNetSocket_t hSocket) override { return k_ESNetSocketConnectionTypeNotConnected; }
        virtual int GetMaxPacketSize(SNetSocket_t hSocket) override { return 1400; }
    };
    static CSteamNetworkingEmu g_steamNetworkingInstance;



    // --- ISteamNetworkingSockets (SteamSockets NetDriver) ---
    class CSteamNetworkingSocketsEmu : public ISteamNetworkingSockets {
    public:
        // 1. CreateListenSocketIP
        virtual HSteamListenSocket CreateListenSocketIP(const SteamNetworkingIPAddr &localAddress, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }

        // 2. ConnectByIPAddress
        virtual HSteamNetConnection ConnectByIPAddress(const SteamNetworkingIPAddr &address, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }

        // 3. CreateListenSocketP2P
        virtual HSteamListenSocket CreateListenSocketP2P(int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }

        // 4. ConnectP2P
        virtual HSteamNetConnection ConnectP2P(const SteamNetworkingIdentity &identityRemote, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            CSteamID remoteID(identityRemote.GetSteamID64());
            uint32_t sessionId = GenerateUniqueSessionId();
            uint32_t nonce = GenerateConnectionNonce();

            HSteamNetConnection handle = CreateReFixConnection(remoteID, k_ESteamNetworkingConnectionState_Connecting, sessionId, nonce);

            ConnectionSnapshot snap = {};
            snap.handle = handle;
            snap.remoteSteamID = remoteID;
            snap.oldState = k_ESteamNetworkingConnectionState_None;
            snap.newState = k_ESteamNetworkingConnectionState_Connecting;
            EnqueueSocketCallbackFromSnapshot(snap);

            // Check if endpoint is already resolved
            bool peerResolved = false;
            {
                std::lock_guard<std::recursive_mutex> emuLock(g_emuMutex);
                auto it = g_peers.find(remoteID.ConvertToUint64());
                if (it != g_peers.end() && it->second.ip != 0 && it->second.port != 0) {
                    peerResolved = true;
                }
            }

            if (peerResolved) {
                {
                    std::lock_guard<std::mutex> lock(g_socketsMutex);
                    g_connections[handle].substate = ReFixConnSubstate::Connecting_HandshakeSent;
                }
                SocketsHandshake hs;
                hs.protocolVersion = 1;
                hs.sessionId = sessionId;
                hs.connectionNonce = nonce;
                hs.capabilities = 0;
                hs.remotePeerId = remoteID.ConvertToUint64();
                SendLanPacket(remoteID, 7, (const uint8_t*)&hs, sizeof(hs), ReFix::PacketDirection::CLIENT_TO_HOST);
            } else {
                // Endpoint unresolved: broadcast discovery ping so peer announces itself (BLOQUEANTE 1)
                std::string pingPayload = g_personaName;
                BroadcastNetPacket(1, pingPayload.c_str(), pingPayload.size());
            }

            return handle;
        }

        // 5. AcceptConnection
        virtual EResult AcceptConnection(HSteamNetConnection hConn) override { 
            bool doEnqueue = false;
            ConnectionSnapshot snap = {};
            {
                std::lock_guard<std::mutex> lock(g_socketsMutex);
                auto it = g_connections.find(hConn);
                if (it != g_connections.end()) {
                    if (it->second.state == k_ESteamNetworkingConnectionState_Connecting) {
                        snap.handle = it->second.handle;
                        snap.remoteSteamID = it->second.remoteSteamID;
                        snap.userData = it->second.userData;
                        strncpy_s(snap.name, sizeof(snap.name), it->second.name, _TRUNCATE);
                        snap.oldState = it->second.state;
                        snap.newState = k_ESteamNetworkingConnectionState_Connected;
                        it->second.state = k_ESteamNetworkingConnectionState_Connected;
                        it->second.substate = ReFixConnSubstate::Connected;
                        doEnqueue = true;
                    }
                } else {
                    return k_EResultInvalidParam;
                }
            }
            if (doEnqueue) {
                EnqueueSocketCallbackFromSnapshot(snap);
            }
            return k_EResultOK;
        }

        // 6. CloseConnection
        virtual bool CloseConnection(HSteamNetConnection hPeer, int nReason, const char *pszDebug, bool bEnableLinger) override { 
            bool doEnqueue = false;
            ConnectionSnapshot snap = {};
            {
                std::lock_guard<std::mutex> lock(g_socketsMutex);
                auto it = g_connections.find(hPeer);
                if (it != g_connections.end()) {
                    snap.handle = it->second.handle;
                    snap.remoteSteamID = it->second.remoteSteamID;
                    snap.userData = it->second.userData;
                    strncpy_s(snap.name, sizeof(snap.name), it->second.name, _TRUNCATE);
                    snap.oldState = it->second.state;
                    snap.newState = k_ESteamNetworkingConnectionState_ClosedByPeer;
                    snap.endReason = nReason;
                    if (pszDebug) strncpy_s(snap.debugMsg, sizeof(snap.debugMsg), pszDebug, _TRUNCATE);

                    it->second.state = k_ESteamNetworkingConnectionState_ClosedByPeer;
                    it->second.substate = ReFixConnSubstate::Closed;
                    doEnqueue = true;
                    
                    // Cleanup incoming messages using Release()
                    while (!it->second.incomingMessages.empty()) {
                        auto* msg = it->second.incomingMessages.front();
                        if (msg) msg->Release();
                        it->second.incomingMessages.pop();
                    }
                    // Cleanup out-of-order inbound messages using Release() (BLOQUEANTE 5)
                    for (auto& oooPair : it->second.outOfOrderInbound) {
                        if (oooPair.second) oooPair.second->Release();
                    }
                    it->second.outOfOrderInbound.clear();

                    g_sessionToConnection.erase(it->second.sessionId);
                    g_connections.erase(it);
                } else {
                    return false;
                }
            }
            if (doEnqueue) {
                EnqueueSocketCallbackFromSnapshot(snap);
            }
            return true;
        }

        // 7. CloseListenSocket
        virtual bool CloseListenSocket(HSteamListenSocket hSocket) override { return true; }

        // 8. SetConnectionUserData
        virtual bool SetConnectionUserData(HSteamNetConnection hPeer, int64 nUserData) override { 
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            if (g_connections.find(hPeer) != g_connections.end()) {
                g_connections[hPeer].userData = nUserData;
                return true;
            }
            return false;
        }

        // 9. GetConnectionUserData
        virtual int64 GetConnectionUserData(HSteamNetConnection hPeer) override { 
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            if (g_connections.find(hPeer) != g_connections.end()) {
                return g_connections[hPeer].userData;
            }
            return 0;
        }

        // 10. SetConnectionName
        virtual void SetConnectionName(HSteamNetConnection hPeer, const char *pszName) override {
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            if (g_connections.find(hPeer) != g_connections.end() && pszName) {
                strncpy_s(g_connections[hPeer].name, sizeof(g_connections[hPeer].name), pszName, _TRUNCATE);
            }
        }

        // 11. GetConnectionName
        virtual bool GetConnectionName(HSteamNetConnection hPeer, char *pszName, int nMaxLen) override {
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            if (g_connections.find(hPeer) != g_connections.end() && pszName && nMaxLen > 0) {
                strncpy_s(pszName, nMaxLen, g_connections[hPeer].name, _TRUNCATE);
                return true;
            }
            return false;
        }

        // 12. SendMessageToConnection
        virtual EResult SendMessageToConnection(HSteamNetConnection hConn, const void *pData, uint32 cbData, int nSendFlags, int64 *pOutMessageNumber) override {
            if (!pData || cbData == 0) return k_EResultInvalidParam;
            
            CSteamID remoteID;
            uint32_t sessionId = 0;
            bool isInitiator = false;
            SocketsPayloadHeader head = {};
            head.flags = (uint16_t)nSendFlags;
            
            {
                std::lock_guard<std::mutex> lock(g_socketsMutex);
                auto it = g_connections.find(hConn);
                if (it == g_connections.end()) return k_EResultInvalidParam;
                if (it->second.state == k_ESteamNetworkingConnectionState_Connecting) return k_EResultIgnored;
                if (it->second.state != k_ESteamNetworkingConnectionState_Connected) return k_EResultNoConnection;
                
                remoteID = it->second.remoteSteamID;
                sessionId = it->second.sessionId;
                head.messageNumber = ++(it->second.messageCountOut);
                isInitiator = it->second.isInitiator;

                // BLOQUEANTE 3: Decouple reliable sequence from unreliable!
                if (nSendFlags & k_nSteamNetworkingSend_Reliable) {
                    head.sequence = it->second.nextReliableSequenceOut++;
                    ReliablePacketOut rOut;
                    rOut.sequence = head.sequence;
                    rOut.data.assign((const uint8_t*)pData, (const uint8_t*)pData + cbData);
                    rOut.lastSendTime = std::chrono::steady_clock::now();
                    rOut.retries = 0;
                    it->second.unackedOutbound.push_back(rOut);
                } else {
                    head.sequence = 0; // Unreliable packets do not consume reliable sequence
                }
                head.ack = it->second.nextReliableSequenceExpected > 0 ? (it->second.nextReliableSequenceExpected - 1) : 0;
            }
            
            // Build Sockets payload (msgType 6): SessionID (4 bytes) + Header + Data
            std::vector<uint8_t> payload(sizeof(uint32_t) + sizeof(SocketsPayloadHeader) + cbData);
            uint8_t* ptr = payload.data();
            *(uint32_t*)ptr = sessionId; ptr += sizeof(uint32_t);
            memcpy(ptr, &head, sizeof(SocketsPayloadHeader)); ptr += sizeof(SocketsPayloadHeader);
            memcpy(ptr, pData, cbData);
            
            // Send outside of g_socketsMutex lock with exact packet direction
            ReFix::PacketDirection dir = isInitiator ? ReFix::PacketDirection::CLIENT_TO_HOST : ReFix::PacketDirection::HOST_TO_CLIENT;
            SendLanPacket(remoteID, 6, payload.data(), payload.size(), dir);
            
            if (pOutMessageNumber) *pOutMessageNumber = head.messageNumber;
            return k_EResultOK;
        }

        // 13. SendMessages
        virtual void SendMessages(int nMessages, SteamNetworkingMessage_t *const *pMessages, int64 *pOutMessageNumberOrResult) override {
            for (int i = 0; i < nMessages; i++) {
                int64 num = 0;
                EResult res = SendMessageToConnection(pMessages[i]->m_conn, pMessages[i]->m_pData, pMessages[i]->m_cbSize, pMessages[i]->m_nFlags, &num);
                if (pOutMessageNumberOrResult) pOutMessageNumberOrResult[i] = (res == k_EResultOK) ? num : -res;
                pMessages[i]->Release();
            }
        }

        // 14. FlushMessagesOnConnection
        virtual EResult FlushMessagesOnConnection(HSteamNetConnection hConn) override { return k_EResultOK; }
        
        // 15. ReceiveMessagesOnConnection
        virtual int ReceiveMessagesOnConnection(HSteamNetConnection hConn, SteamNetworkingMessage_t **ppOutMessages, int nMaxMessages) override { 
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            auto it = g_connections.find(hConn);
            if (it == g_connections.end() || it->second.state != k_ESteamNetworkingConnectionState_Connected) {
                return -1;
            }
            
            int count = 0;
            while (count < nMaxMessages && !it->second.incomingMessages.empty()) {
                ppOutMessages[count] = it->second.incomingMessages.front();
                it->second.incomingMessages.pop();
                count++;
            }
            return count;
        }
        
        // 16. GetConnectionInfo
        virtual bool GetConnectionInfo(HSteamNetConnection hConn, SteamNetConnectionInfo_t *pInfo) override {
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            auto it = g_connections.find(hConn);
            if (it != g_connections.end() && pInfo) {
                memset(pInfo, 0, sizeof(SteamNetConnectionInfo_t));
                pInfo->m_eState = it->second.state;
                pInfo->m_identityRemote.SetSteamID64(it->second.remoteSteamID.ConvertToUint64());
                pInfo->m_nUserData = it->second.userData;
                return true;
            }
            return false;
        }

        // 17. GetConnectionRealTimeStatus
        virtual EResult GetConnectionRealTimeStatus(HSteamNetConnection hConn, SteamNetConnectionRealTimeStatus_t *pStatus, int nLanes, SteamNetConnectionRealTimeLaneStatus_t *pLanes) override {
            if (!pStatus) return k_EResultInvalidParam;
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            auto it = g_connections.find(hConn);
            if (it == g_connections.end()) return k_EResultInvalidParam;

            memset(pStatus, 0, sizeof(SteamNetConnectionRealTimeStatus_t));
            pStatus->m_eState = it->second.state;
            pStatus->m_nPing = (int)it->second.pingMs;

            int unackedBytes = 0;
            for (const auto& rOut : it->second.unackedOutbound) {
                unackedBytes += (int)rOut.data.size();
            }
            pStatus->m_cbPendingReliable = unackedBytes;
            pStatus->m_cbPendingUnreliable = 0;
            pStatus->m_cbSentUnackedReliable = unackedBytes;

            if (pLanes && nLanes > 0) {
                memset(pLanes, 0, sizeof(SteamNetConnectionRealTimeLaneStatus_t) * nLanes);
                pLanes[0].m_cbPendingReliable = unackedBytes;
                pLanes[0].m_cbSentUnackedReliable = unackedBytes;
            }
            return k_EResultOK;
        }

        // 18. GetDetailedConnectionStatus
        virtual int GetDetailedConnectionStatus(HSteamNetConnection hConn, char *pszBuf, int cbBuf) override { return 0; }

        // 19. GetListenSocketAddress
        virtual bool GetListenSocketAddress(HSteamListenSocket hSocket, SteamNetworkingIPAddr *address) override {
            if (address) {
                address->Clear();
                address->SetIPv4(0x7F000001, g_listenPort);
                return true;
            }
            return false;
        }

        // 20. CreateSocketPair
        virtual bool CreateSocketPair(HSteamNetConnection *pOutConnection1, HSteamNetConnection *pOutConnection2, bool bUseNetworkLoopback, const SteamNetworkingIdentity *pIdentity1, const SteamNetworkingIdentity *pIdentity2) override {
            if (pOutConnection1) *pOutConnection1 = 1;
            if (pOutConnection2) *pOutConnection2 = 2;
            return true;
        }

        // 21. ConfigureConnectionLanes
        virtual EResult ConfigureConnectionLanes(HSteamNetConnection hConn, int nNumLanes, const int *pLanePriorities, const uint16 *pLaneWeights) override {
            return k_EResultOK;
        }

        // 22. GetIdentity
        virtual bool GetIdentity(SteamNetworkingIdentity *pIdentity) override {
            if (pIdentity) { pIdentity->SetSteamID64(g_localSteamID); return true; }
            return false;
        }

        // 23. InitAuthentication
        virtual ESteamNetworkingAvailability InitAuthentication() override { return k_ESteamNetworkingAvailability_Current; }

        // 24. GetAuthenticationStatus
        virtual ESteamNetworkingAvailability GetAuthenticationStatus(SteamNetAuthenticationStatus_t *pDetails) override {
            if (pDetails) {
                memset(pDetails, 0, sizeof(SteamNetAuthenticationStatus_t));
                pDetails->m_eAvail = k_ESteamNetworkingAvailability_Current;
            }
            return k_ESteamNetworkingAvailability_Current;
        }

        // 25. CreatePollGroup
        virtual HSteamNetPollGroup CreatePollGroup() override { return 1; }

        // 26. DestroyPollGroup
        virtual bool DestroyPollGroup(HSteamNetPollGroup hPollGroup) override { return true; }

        // 27. SetConnectionPollGroup
        virtual bool SetConnectionPollGroup(HSteamNetConnection hConn, HSteamNetPollGroup hPollGroup) override { 
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            if (g_connections.find(hConn) != g_connections.end()) {
                g_connections[hConn].pollGroup = hPollGroup;
                return true;
            }
            return false;
        }

        // 28. ReceiveMessagesOnPollGroup
        virtual int ReceiveMessagesOnPollGroup(HSteamNetPollGroup hPollGroup, SteamNetworkingMessage_t **ppOutMessages, int nMaxMessages) override { 
            std::lock_guard<std::mutex> lock(g_socketsMutex);
            int count = 0;
            for (auto& pair : g_connections) {
                if (pair.second.pollGroup == hPollGroup && pair.second.state == k_ESteamNetworkingConnectionState_Connected) {
                    while (count < nMaxMessages && !pair.second.incomingMessages.empty()) {
                        ppOutMessages[count] = pair.second.incomingMessages.front();
                        pair.second.incomingMessages.pop();
                        count++;
                    }
                }
                if (count >= nMaxMessages) break;
            }
            return count;
        }

        // 29. ReceivedRelayAuthTicket
        virtual bool ReceivedRelayAuthTicket(const void *pvTicket, int cbTicket, SteamDatagramRelayAuthTicket *pOutParsedTicket) override { return false; }

        // 30. FindRelayAuthTicketForServer
        virtual int FindRelayAuthTicketForServer(const SteamNetworkingIdentity &identityGameServer, int nRemoteVirtualPort, SteamDatagramRelayAuthTicket *pOutParsedTicket) override { return 0; }

        // 31. ConnectToHostedDedicatedServer
        virtual HSteamNetConnection ConnectToHostedDedicatedServer(const SteamNetworkingIdentity &identityTarget, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return k_HSteamNetConnection_Invalid;
        }

        // 32. GetHostedDedicatedServerPort
        virtual uint16 GetHostedDedicatedServerPort() override { return 0; }

        // 33. GetHostedDedicatedServerPOPID
        virtual SteamNetworkingPOPID GetHostedDedicatedServerPOPID() override { return 0; }

        // 34. GetHostedDedicatedServerAddress
        virtual EResult GetHostedDedicatedServerAddress(SteamDatagramHostedAddress *pRouting) override { return k_EResultFail; }

        // 35. CreateHostedDedicatedServerListenSocket
        virtual HSteamListenSocket CreateHostedDedicatedServerListenSocket(int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override { return k_HSteamListenSocket_Invalid; }

        // 36. GetGameCoordinatorServerLogin
        virtual EResult GetGameCoordinatorServerLogin(SteamDatagramGameCoordinatorServerLogin *pLoginInfo, int *pcbSignedBlob, void *pBlob) override { return k_EResultFail; }

        // 37. ConnectP2PCustomSignaling
        virtual HSteamNetConnection ConnectP2PCustomSignaling(ISteamNetworkingConnectionSignaling *pSignaling, const SteamNetworkingIdentity *pPeerIdentity, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return k_HSteamNetConnection_Invalid;
        }

        // 38. ReceivedP2PCustomSignal
        virtual bool ReceivedP2PCustomSignal(const void *pMsg, int cbMsg, ISteamNetworkingSignalingRecvContext *pContext) override { return false; }

        // 39. GetCertificateRequest
        virtual bool GetCertificateRequest(int *pcbBlob, void *pBlob, SteamNetworkingErrMsg &errMsg) override {
            if (pcbBlob) *pcbBlob = 0;
            return false;
        }

        // 40. SetCertificate
        virtual bool SetCertificate(const void *pCertificate, int cbCertificate, SteamNetworkingErrMsg &errMsg) override { return false; }

        // 41. ResetIdentity
        virtual void ResetIdentity(const SteamNetworkingIdentity *pIdentity) override {}

        // 42. RunCallbacks
        virtual void RunCallbacks() override { UnrealSteamEmu::RunCallbacks(); }

        // 43. BeginAsyncRequestFakeIP
        virtual bool BeginAsyncRequestFakeIP(int nNumPorts) override { return false; }

        // 44. GetFakeIP
        virtual void GetFakeIP(int idxFirstPort, SteamNetworkingFakeIPResult_t *pInfo) override {
            if (pInfo) {
                memset(pInfo, 0, sizeof(*pInfo));
                pInfo->m_eResult = k_EResultFail;
            }
        }

        // 45. CreateListenSocketP2PFakeIP
        virtual HSteamListenSocket CreateListenSocketP2PFakeIP(int idxFakePort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override { return k_HSteamListenSocket_Invalid; }

        // 46. GetRemoteFakeIPForConnection
        virtual EResult GetRemoteFakeIPForConnection(HSteamNetConnection hConn, SteamNetworkingIPAddr *pOutAddr) override {
            return k_EResultFail;
        }

        // 47. CreateFakeUDPPort
        virtual ISteamNetworkingFakeUDPPort *CreateFakeUDPPort(int idxFakeServerPort) override { return nullptr; }
    };
    static CSteamNetworkingSocketsEmu g_steamNetworkingSocketsInstance;
    // --- ISteamNetworkingUtils ---
    class CSteamNetworkingUtilsEmu : public ISteamNetworkingUtils {
    public:
        virtual SteamNetworkingMessage_t *AllocateMessage(int cbAllocateBuffer) override {
            SteamNetworkingMessage_t *msg = (SteamNetworkingMessage_t*)malloc(sizeof(SteamNetworkingMessage_t) + (cbAllocateBuffer > 0 ? cbAllocateBuffer : 0));
            if (!msg) return nullptr;
            memset(msg, 0, sizeof(SteamNetworkingMessage_t));
            if (cbAllocateBuffer > 0) {
                msg->m_pData = (void*)(msg + 1);
                msg->m_cbSize = cbAllocateBuffer;
            }
            msg->m_pfnFreeData = [](SteamNetworkingMessage_t *pMsg) { /* embedded */ };
            msg->m_pfnRelease = ReleaseReFixMessage;
            ReFix::MessageTracker::Get().TrackAlloc(msg, sizeof(SteamNetworkingMessage_t) + (cbAllocateBuffer > 0 ? cbAllocateBuffer : 0), "UtilsAllocateMessage");
            return msg;
        }
        virtual ESteamNetworkingAvailability GetRelayNetworkStatus(SteamRelayNetworkStatus_t *pDetails) override {
            if (pDetails) {
                memset(pDetails, 0, sizeof(SteamRelayNetworkStatus_t));
                pDetails->m_eAvail = k_ESteamNetworkingAvailability_Current; // 100
                pDetails->m_bPingMeasurementInProgress = 0;
                pDetails->m_eAvailNetworkConfig = k_ESteamNetworkingAvailability_Current; // 100
                pDetails->m_eAvailAnyRelay = k_ESteamNetworkingAvailability_Current; // 100
                strncpy_s(pDetails->m_debugMsg, sizeof(pDetails->m_debugMsg), "OK (ReFix LAN)", _TRUNCATE);
            }
            return k_ESteamNetworkingAvailability_Current;
        }
        virtual float GetLocalPingLocation(SteamNetworkPingLocation_t &result) override {
            memset(&result, 0, sizeof(result));
            return 0.0f;
        }
        virtual int EstimatePingTimeBetweenTwoLocations(const SteamNetworkPingLocation_t &location1, const SteamNetworkPingLocation_t &location2) override {
            return -1;
        }
        virtual int EstimatePingTimeFromLocalHost(const SteamNetworkPingLocation_t &remoteLocation) override {
            return -1;
        }
        virtual void ConvertPingLocationToString(const SteamNetworkPingLocation_t &location, char *pszBuf, int cchBufSize) override {
            if (pszBuf && cchBufSize > 0) pszBuf[0] = '\0';
        }
        virtual bool ParsePingLocationString(const char *pszString, SteamNetworkPingLocation_t &result) override {
            memset(&result, 0, sizeof(result));
            return true;
        }
        virtual bool CheckPingDataUpToDate(float flMaxAgeSeconds) override { return true; }
        virtual int GetPingToDataCenter(SteamNetworkingPOPID popID, SteamNetworkingPOPID *pViaRelayPoP) override {
            if (pViaRelayPoP) *pViaRelayPoP = 0;
            return -1;
        }
        virtual int GetDirectPingToPOP(SteamNetworkingPOPID popID) override { return -1; }
        virtual int GetPOPCount() override { return 0; }
        virtual int GetPOPList(SteamNetworkingPOPID *list, int nListSz) override { return 0; }
        virtual SteamNetworkingMicroseconds GetLocalTimestamp() override {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        virtual void SetDebugOutputFunction(ESteamNetworkingSocketsDebugOutputType eDetailLevel, FSteamNetworkingSocketsDebugOutput pfnFunc) override {}
        virtual ESteamNetworkingFakeIPType GetIPv4FakeIPType(uint32 nIPv4) override { return k_ESteamNetworkingFakeIPType_NotFake; }
        virtual EResult GetRealIdentityForFakeIP(const SteamNetworkingIPAddr &fakeIP, SteamNetworkingIdentity *pOutRealIdentity) override {
            return k_EResultNoMatch;
        }
        virtual bool SetConfigValue(ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj, ESteamNetworkingConfigDataType eDataType, const void *pArg) override {
            return true;
        }
        virtual ESteamNetworkingGetConfigValueResult GetConfigValue(ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj, ESteamNetworkingConfigDataType *pOutDataType, void *pResult, size_t *cbResult) override {
            return k_ESteamNetworkingGetConfigValue_OK;
        }
        virtual const char *GetConfigValueInfo(ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigDataType *pOutDataType, ESteamNetworkingConfigScope *pOutScope) override {
            return "refix_val";
        }
        virtual ESteamNetworkingConfigValue IterateGenericEditableConfigValues(ESteamNetworkingConfigValue eCurrent, bool bEnumerateDevVars) override {
            return k_ESteamNetworkingConfig_Invalid;
        }
        virtual void SteamNetworkingIPAddr_ToString(const SteamNetworkingIPAddr &addr, char *buf, size_t cbBuf, bool bWithPort) override {
            if (!buf || cbBuf == 0) return;
            char ipStr[INET6_ADDRSTRLEN] = { 0 };
            if (addr.IsIPv4()) {
                uint32_t ip = addr.GetIPv4();
                sockaddr_in sa = {};
                sa.sin_family = AF_INET;
                sa.sin_addr.s_addr = htonl(ip);
                inet_ntop(AF_INET, &sa.sin_addr, ipStr, sizeof(ipStr));
            } else {
                sockaddr_in6 sa6 = {};
                sa6.sin6_family = AF_INET6;
                memcpy(&sa6.sin6_addr, addr.m_ipv6, 16);
                inet_ntop(AF_INET6, &sa6.sin6_addr, ipStr, sizeof(ipStr));
            }
            if (bWithPort) {
                snprintf(buf, cbBuf, "%s:%u", ipStr, addr.m_port);
            } else {
                snprintf(buf, cbBuf, "%s", ipStr);
            }
        }
        virtual bool SteamNetworkingIPAddr_ParseString(SteamNetworkingIPAddr *pAddr, const char *pszStr) override {
            if (!pAddr || !pszStr) return false;
            pAddr->Clear();
            std::string s(pszStr);
            std::string ipPart = s;
            uint16_t port = 0;

            size_t colon = s.rfind(':');
            if (colon != std::string::npos) {
                size_t rbracket = s.rfind(']');
                if (rbracket != std::string::npos && colon > rbracket) {
                    ipPart = s.substr(1, rbracket - 1);
                    port = (uint16_t)std::strtoul(s.c_str() + colon + 1, nullptr, 10);
                } else if (s.find(':') == colon) {
                    ipPart = s.substr(0, colon);
                    port = (uint16_t)std::strtoul(s.c_str() + colon + 1, nullptr, 10);
                }
            }

            IN_ADDR in4;
            if (inet_pton(AF_INET, ipPart.c_str(), &in4) == 1) {
                pAddr->SetIPv4(ntohl(in4.s_addr), port);
                return true;
            }

            IN6_ADDR in6;
            if (inet_pton(AF_INET6, ipPart.c_str(), &in6) == 1) {
                pAddr->SetIPv6((const uint8_t*)&in6, port);
                return true;
            }

            return false;
        }
        virtual ESteamNetworkingFakeIPType SteamNetworkingIPAddr_GetFakeIPType(const SteamNetworkingIPAddr &addr) override {
            return k_ESteamNetworkingFakeIPType_NotFake;
        }
        virtual void SteamNetworkingIdentity_ToString(const SteamNetworkingIdentity &identity, char *buf, size_t cbBuf) override {
            if (buf && cbBuf > 0) snprintf(buf, cbBuf, "steamid:%llu", (unsigned long long)identity.GetSteamID64());
        }
        virtual bool SteamNetworkingIdentity_ParseString(SteamNetworkingIdentity *pIdentity, const char *pszStr) override {
            if (!pIdentity || !pszStr) return false;
            pIdentity->Clear();
            if (strncmp(pszStr, "steamid:", 8) == 0) {
                uint64_t sid = _strtoui64(pszStr + 8, nullptr, 10);
                pIdentity->SetSteamID64(sid);
                return true;
            }
            uint64_t sid = _strtoui64(pszStr, nullptr, 10);
            if (sid != 0) {
                pIdentity->SetSteamID64(sid);
                return true;
            }
            return false;
        }
    };
    static CSteamNetworkingUtilsEmu g_steamNetworkingUtilsInstance;

    // --- ISteamNetworkingMessages ---
    class CSteamNetworkingMessagesEmu : public ISteamNetworkingMessages {
    public:
        virtual EResult SendMessageToUser(const SteamNetworkingIdentity &identityRemote, const void *pubData, uint32 cubData, int nSendFlags, int nRemoteChannel) override {
            if (!pubData || cubData == 0) return k_EResultOK;

            // Packet format for type 5 (P2P): [int32 channel][message payload]
            std::vector<uint8_t> payload(sizeof(int32_t) + cubData);
            *(int32_t*)payload.data() = nRemoteChannel;
            memcpy(payload.data() + sizeof(int32_t), pubData, cubData);

            uint64_t targetSteamID = identityRemote.GetSteamID64();

            sockaddr_in dest = {};
            bool hasPeer = false;
            if (targetSteamID != 0) {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                auto it = g_peers.find(targetSteamID);
                if (it != g_peers.end() && it->second.ip != 0 && it->second.port != 0) {
                    dest.sin_family = AF_INET;
                    dest.sin_port = htons(it->second.port);
                    dest.sin_addr.s_addr = htonl(it->second.ip);
                    hasPeer = true;
                }
            }

            if (hasPeer && g_udpSocket != INVALID_SOCKET) {
                if (!IsAllowedLanAddress((const sockaddr*)&dest)) {
                    g_blockedEgressCount.fetch_add(1);
                    return k_EResultAccessDenied;
                }

                std::vector<uint8_t> netBuf(sizeof(NetPacketHeader) + payload.size());
                NetPacketHeader* hdr = (NetPacketHeader*)netBuf.data();
                hdr->magic = 0x52464958;
                hdr->msgType = 5; // P2P
                hdr->senderID = g_localSteamID;
                hdr->appID = g_appID;
                hdr->payloadLen = (uint32_t)payload.size();
                memcpy(netBuf.data() + sizeof(NetPacketHeader), payload.data(), payload.size());

                sendto(g_udpSocket, (const char*)netBuf.data(), (int)netBuf.size(), 0, (sockaddr*)&dest, sizeof(dest));
            } else {
                BroadcastNetPacket(5, payload.data(), payload.size());
            }

            return k_EResultOK;
        }

        virtual int ReceiveMessagesOnChannel(int nLocalChannel, SteamNetworkingMessage_t **ppOutMessages, int nMaxMessages) override {
            if (!ppOutMessages || nMaxMessages <= 0) return 0;
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);

            auto it = g_p2pIncoming.find(nLocalChannel);
            if (it == g_p2pIncoming.end() || it->second.empty()) return 0;

            int count = 0;
            while (!it->second.empty() && count < nMaxMessages) {
                P2PPacket pkt = std::move(it->second.front());
                it->second.pop();

                SteamNetworkingMessage_t* msg = g_steamNetworkingUtilsInstance.AllocateMessage((int)pkt.data.size());
                if (msg) {
                    if (!pkt.data.empty() && msg->m_pData) {
                        memcpy(msg->m_pData, pkt.data.data(), pkt.data.size());
                    }
                    msg->m_identityPeer.SetSteamID64(pkt.senderID);
                    msg->m_nChannel = nLocalChannel;
                    msg->m_nFlags = k_nSteamNetworkingSend_Reliable;
                    msg->m_usecTimeReceived = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    ppOutMessages[count++] = msg;
                }
            }
            return count;
        }

        virtual bool AcceptSessionWithUser(const SteamNetworkingIdentity &identityRemote) override {
            return true;
        }

        virtual bool CloseSessionWithUser(const SteamNetworkingIdentity &identityRemote) override {
            return true;
        }

        virtual bool CloseChannelWithUser(const SteamNetworkingIdentity &identityRemote, int nLocalChannel) override {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            g_p2pIncoming.erase(nLocalChannel);
            return true;
        }

        virtual ESteamNetworkingConnectionState GetSessionConnectionInfo(const SteamNetworkingIdentity &identityRemote, SteamNetConnectionInfo_t *pConnectionInfo, SteamNetConnectionRealTimeStatus_t *pQuickStatus) override {
            if (pConnectionInfo) {
                memset(pConnectionInfo, 0, sizeof(SteamNetConnectionInfo_t));
                pConnectionInfo->m_identityRemote = identityRemote;
                pConnectionInfo->m_eState = k_ESteamNetworkingConnectionState_Connected;
            }
            if (pQuickStatus) {
                memset(pQuickStatus, 0, sizeof(SteamNetConnectionRealTimeStatus_t));
                pQuickStatus->m_eState = k_ESteamNetworkingConnectionState_Connected;
                pQuickStatus->m_nPing = 5;
            }
            return k_ESteamNetworkingConnectionState_Connected;
        }
    };
    static CSteamNetworkingMessagesEmu g_steamNetworkingMessagesInstance;

    // --- ISteamRemoteStorage ---
    class CSteamRemoteStorageEmu : public ISteamRemoteStorage {
    public:
        virtual bool FileWrite(const char *pchFile, const void *pvData, int32 cubData) override {
            if (!pchFile || !pvData) return false;
            CreateDirectoryA("saves", NULL);
            std::string path = std::string("saves/") + pchFile;
            std::ofstream f(path, std::ios::binary);
            if (f.is_open()) {
                f.write((const char*)pvData, cubData);
                return true;
            }
            return false;
        }
        virtual int32 FileRead(const char *pchFile, void *pvData, int32 cubDataToRead) override {
            if (!pchFile || !pvData) return 0;
            std::string path = std::string("saves/") + pchFile;
            std::ifstream f(path, std::ios::binary);
            if (f.is_open()) {
                f.read((char*)pvData, cubDataToRead);
                return (int32)f.gcount();
            }
            return 0;
        }
        virtual SteamAPICall_t FileWriteAsync(const char *pchFile, const void *pvData, uint32 cubData) override {
            FileWrite(pchFile, pvData, (int32)cubData);
            RemoteStorageFileWriteAsyncComplete_t resp = {};
            resp.m_eResult = k_EResultOK;
            return PostCallResult(RemoteStorageFileWriteAsyncComplete_t::k_iCallback, &resp, sizeof(resp));
        }
        virtual SteamAPICall_t FileReadAsync(const char *pchFile, uint32 nOffset, uint32 cubToRead) override {
            RemoteStorageFileReadAsyncComplete_t resp = {};
            resp.m_eResult = k_EResultOK;
            resp.m_nOffset = nOffset;
            resp.m_cubRead = cubToRead;
            return PostCallResult(RemoteStorageFileReadAsyncComplete_t::k_iCallback, &resp, sizeof(resp));
        }
        virtual bool FileReadAsyncComplete(SteamAPICall_t hReadCall, void *pvBuffer, uint32 cubToRead) override { return true; }
        virtual bool FileForget(const char *pchFile) override { return true; }
        virtual bool FileDelete(const char *pchFile) override {
            if (pchFile) {
                std::string path = std::string("saves/") + pchFile;
                DeleteFileA(path.c_str());
                return true;
            }
            return false;
        }
        virtual SteamAPICall_t FileShare(const char *pchFile) override { return 0; }
        virtual bool SetSyncPlatforms(const char *pchFile, ERemoteStoragePlatform eRemoteStoragePlatform) override { return true; }

        virtual UGCFileWriteStreamHandle_t FileWriteStreamOpen(const char *pchFile) override { return 1; }
        virtual bool FileWriteStreamWriteChunk(UGCFileWriteStreamHandle_t writeHandle, const void *pvData, int32 cubData) override { return true; }
        virtual bool FileWriteStreamClose(UGCFileWriteStreamHandle_t writeHandle) override { return true; }
        virtual bool FileWriteStreamCancel(UGCFileWriteStreamHandle_t writeHandle) override { return true; }

        virtual bool FileExists(const char *pchFile) override {
            if (!pchFile) return false;
            std::string path = std::string("saves/") + pchFile;
            return (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES);
        }
        virtual bool FilePersisted(const char *pchFile) override { return FileExists(pchFile); }
        virtual int32 GetFileSize(const char *pchFile) override {
            if (!pchFile) return 0;
            std::string path = std::string("saves/") + pchFile;
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) {
                return (int32)fad.nFileSizeLow;
            }
            return 0;
        }
        virtual int64 GetFileTimestamp(const char *pchFile) override { return ::time(NULL); }
        virtual ERemoteStoragePlatform GetSyncPlatforms(const char *pchFile) override { return k_ERemoteStoragePlatformAll; }
        virtual int32 GetFileCount() override { return 0; }
        virtual const char *GetFileNameAndSize(int iFile, int32 *pnFileSizeInBytes) override {
            if (pnFileSizeInBytes) *pnFileSizeInBytes = 0;
            return "";
        }
        virtual bool GetQuota(uint64 *pnTotalBytes, uint64 *puAvailableBytes) override {
            if (pnTotalBytes) *pnTotalBytes = 107374182400ULL; // 100 GB
            if (puAvailableBytes) *puAvailableBytes = 107374182400ULL;
            return true;
        }
        virtual bool IsCloudEnabledForAccount() override { return true; }
        virtual bool IsCloudEnabledForApp() override { return true; }
        virtual void SetCloudEnabledForApp(bool bEnabled) override {}

        virtual SteamAPICall_t UGCDownload(UGCHandle_t hContent, uint32 unPriority) override { return 0; }
        virtual bool GetUGCDownloadProgress(UGCHandle_t hContent, int32 *pnBytesDownloaded, int32 *pnBytesExpected) override { return false; }
        virtual bool GetUGCDetails(UGCHandle_t hContent, AppId_t *pnAppID, char **ppchName, int32 *pnFileSizeInBytes, CSteamID *pSteamIDOwner) override { return false; }
        virtual int32 UGCRead(UGCHandle_t hContent, void *pvData, int32 cubDataToRead, uint32 cOffset, EUGCReadAction eAction) override { return 0; }
        virtual int32 GetCachedUGCCount() override { return 0; }
        virtual UGCHandle_t GetCachedUGCHandle(int32 iCachedContent) override { return 0; }

        virtual SteamAPICall_t PublishWorkshopFile(const char *pchFile, const char *pchPreviewFile, AppId_t nConsumerAppId, const char *pchTitle, const char *pchDescription, ERemoteStoragePublishedFileVisibility eVisibility, SteamParamStringArray_t *pTags, EWorkshopFileType eWorkshopFileType) override { return 0; }
        virtual PublishedFileUpdateHandle_t CreatePublishedFileUpdateRequest(PublishedFileId_t unPublishedFileId) override { return 1; }
        virtual bool UpdatePublishedFileFile(PublishedFileUpdateHandle_t updateHandle, const char *pchFile) override { return true; }
        virtual bool UpdatePublishedFilePreviewFile(PublishedFileUpdateHandle_t updateHandle, const char *pchPreviewFile) override { return true; }
        virtual bool UpdatePublishedFileTitle(PublishedFileUpdateHandle_t updateHandle, const char *pchTitle) override { return true; }
        virtual bool UpdatePublishedFileDescription(PublishedFileUpdateHandle_t updateHandle, const char *pchDescription) override { return true; }
        virtual bool UpdatePublishedFileVisibility(PublishedFileUpdateHandle_t updateHandle, ERemoteStoragePublishedFileVisibility eVisibility) override { return true; }
        virtual bool UpdatePublishedFileTags(PublishedFileUpdateHandle_t updateHandle, SteamParamStringArray_t *pTags) override { return true; }
        virtual SteamAPICall_t CommitPublishedFileUpdate(PublishedFileUpdateHandle_t updateHandle) override { return 0; }
        virtual SteamAPICall_t GetPublishedFileDetails(PublishedFileId_t unPublishedFileId, uint32 unMaxSecondsOld) override { return 0; }
        virtual SteamAPICall_t DeletePublishedFile(PublishedFileId_t unPublishedFileId) override { return 0; }
        virtual SteamAPICall_t EnumerateUserPublishedFiles(uint32 unStartIndex) override { return 0; }
        virtual SteamAPICall_t SubscribePublishedFile(PublishedFileId_t unPublishedFileId) override { return 0; }
        virtual SteamAPICall_t EnumerateUserSubscribedFiles(uint32 unStartIndex) override { return 0; }
        virtual SteamAPICall_t UnsubscribePublishedFile(PublishedFileId_t unPublishedFileId) override { return 0; }
        virtual bool UpdatePublishedFileSetChangeDescription(PublishedFileUpdateHandle_t updateHandle, const char *pchChangeDescription) override { return true; }
        virtual SteamAPICall_t GetPublishedItemVoteDetails(PublishedFileId_t unPublishedFileId) override { return 0; }
        virtual SteamAPICall_t UpdateUserPublishedItemVote(PublishedFileId_t unPublishedFileId, bool bVoteUp) override { return 0; }
        virtual SteamAPICall_t GetUserPublishedItemVoteDetails(PublishedFileId_t unPublishedFileId) override { return 0; }
        virtual SteamAPICall_t EnumerateUserSharedWorkshopFiles(CSteamID steamId, uint32 unStartIndex, SteamParamStringArray_t *pRequiredTags, SteamParamStringArray_t *pExcludedTags) override { return 0; }
        virtual SteamAPICall_t PublishVideo(EWorkshopVideoProvider eVideoProvider, const char *pchVideoAccount, const char *pchVideoIdentifier, const char *pchPreviewFile, AppId_t nConsumerAppId, const char *pchTitle, const char *pchDescription, ERemoteStoragePublishedFileVisibility eVisibility, SteamParamStringArray_t *pTags) override { return 0; }
        virtual SteamAPICall_t SetUserPublishedFileAction(PublishedFileId_t unPublishedFileId, EWorkshopFileAction eAction) override { return 0; }
        virtual SteamAPICall_t EnumeratePublishedFilesByUserAction(EWorkshopFileAction eAction, uint32 unStartIndex) override { return 0; }
        virtual SteamAPICall_t EnumeratePublishedWorkshopFiles(EWorkshopEnumerationType eEnumerationType, uint32 unStartIndex, uint32 unCount, uint32 unDays, SteamParamStringArray_t *pTags, SteamParamStringArray_t *pUserTags) override { return 0; }

        virtual SteamAPICall_t UGCDownloadToLocation(UGCHandle_t hContent, const char *pchLocation, uint32 unPriority) override { return 0; }

        virtual int32 GetLocalFileChangeCount() override { return 0; }
        virtual const char *GetLocalFileChange(int iFile, ERemoteStorageLocalFileChange *pEChangeType, ERemoteStorageFilePathType *pEFilePathType) override { return ""; }
        virtual bool BeginFileWriteBatch() override { return true; }
        virtual bool EndFileWriteBatch() override { return true; }
    };
    static CSteamRemoteStorageEmu g_steamRemoteStorageInstance;

    // --- ISteamUGC ---
    class CSteamUGCEmu : public ISteamUGC {
    public:
        virtual UGCQueryHandle_t CreateQueryUserUGCRequest(AccountID_t unAccountID, EUserUGCList eListType, EUGCMatchingUGCType eMatchingUGCType, EUserUGCListSortOrder eSortOrder, AppId_t nCreatorAppID, AppId_t nConsumerAppID, uint32 unPage) override { return 1; }
        virtual UGCQueryHandle_t CreateQueryAllUGCRequest(EUGCQuery eQueryType, EUGCMatchingUGCType eMatchingeMatchingUGCTypeFileType, AppId_t nCreatorAppID, AppId_t nConsumerAppID, uint32 unPage) override { return 1; }
        virtual UGCQueryHandle_t CreateQueryAllUGCRequest(EUGCQuery eQueryType, EUGCMatchingUGCType eMatchingeMatchingUGCTypeFileType, AppId_t nCreatorAppID, AppId_t nConsumerAppID, const char *pchCursor) override { return 1; }
        virtual UGCQueryHandle_t CreateQueryUGCDetailsRequest(PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs) override { return 1; }
        virtual SteamAPICall_t SendQueryUGCRequest(UGCQueryHandle_t handle) override {
            SteamUGCQueryCompleted_t resp = {};
            resp.m_handle = handle;
            resp.m_eResult = k_EResultOK;
            resp.m_unNumResultsReturned = 0;
            resp.m_unTotalMatchingResults = 0;
            return PostCallResult(SteamUGCQueryCompleted_t::k_iCallback, &resp, sizeof(resp));
        }
        virtual bool GetQueryUGCResult(UGCQueryHandle_t handle, uint32 index, SteamUGCDetails_t *pDetails) override { return false; }
        virtual uint32 GetQueryUGCNumTags(UGCQueryHandle_t handle, uint32 index) override { return 0; }
        virtual bool GetQueryUGCTag(UGCQueryHandle_t handle, uint32 index, uint32 indexTag, char* pchValue, uint32 cchValueSize) override { return false; }
        virtual bool GetQueryUGCTagDisplayName(UGCQueryHandle_t handle, uint32 index, uint32 indexTag, char* pchValue, uint32 cchValueSize) override { return false; }
        virtual bool GetQueryUGCPreviewURL(UGCQueryHandle_t handle, uint32 index, char *pchURL, uint32 cchURLSize) override { return false; }
        virtual bool GetQueryUGCMetadata(UGCQueryHandle_t handle, uint32 index, char *pchMetadata, uint32 cchMetadatasize) override { return false; }
        virtual bool GetQueryUGCChildren(UGCQueryHandle_t handle, uint32 index, PublishedFileId_t* pvecPublishedFileID, uint32 cMaxEntries) override { return false; }
        virtual bool GetQueryUGCStatistic(UGCQueryHandle_t handle, uint32 index, EItemStatistic eStatType, uint64 *pStatValue) override { return false; }
        virtual uint32 GetQueryUGCNumAdditionalPreviews(UGCQueryHandle_t handle, uint32 index) override { return 0; }
        virtual bool GetQueryUGCAdditionalPreview(UGCQueryHandle_t handle, uint32 index, uint32 previewIndex, char *pchURLOrVideoID, uint32 cchURLSize, char *pchOriginalFileName, uint32 cchOriginalFileNameSize, EItemPreviewType *pPreviewType) override { return false; }
        virtual uint32 GetQueryUGCNumKeyValueTags(UGCQueryHandle_t handle, uint32 index) override { return 0; }
        virtual bool GetQueryUGCKeyValueTag(UGCQueryHandle_t handle, uint32 index, uint32 keyValueTagIndex, char *pchKey, uint32 cchKeySize, char *pchValue, uint32 cchValueSize) override { return false; }
        virtual bool GetQueryUGCKeyValueTag(UGCQueryHandle_t handle, uint32 index, const char *pchKey, char *pchValue, uint32 cchValueSize) override { return false; }
        virtual uint32 GetNumSupportedGameVersions(UGCQueryHandle_t handle, uint32 index) override { return 0; }
        virtual bool GetSupportedGameVersionData(UGCQueryHandle_t handle, uint32 index, uint32 versionIndex, char *pchGameBranchMin, char *pchGameBranchMax, uint32 cchGameBranchSize) override { return false; }
        virtual uint32 GetQueryUGCContentDescriptors(UGCQueryHandle_t handle, uint32 index, EUGCContentDescriptorID *pvecDescriptors, uint32 cMaxEntries) override { return 0; }
        virtual bool ReleaseQueryUGCRequest(UGCQueryHandle_t handle) override { return true; }
        virtual bool AddRequiredTag(UGCQueryHandle_t handle, const char *pTagName) override { return true; }
        virtual bool AddRequiredTagGroup(UGCQueryHandle_t handle, const SteamParamStringArray_t *pTagGroups) override { return true; }
        virtual bool AddExcludedTag(UGCQueryHandle_t handle, const char *pTagName) override { return true; }
        virtual bool SetReturnOnlyIDs(UGCQueryHandle_t handle, bool bReturnOnlyIDs) override { return true; }
        virtual bool SetReturnKeyValueTags(UGCQueryHandle_t handle, bool bReturnKeyValueTags) override { return true; }
        virtual bool SetReturnLongDescription(UGCQueryHandle_t handle, bool bReturnLongDescription) override { return true; }
        virtual bool SetReturnMetadata(UGCQueryHandle_t handle, bool bReturnMetadata) override { return true; }
        virtual bool SetReturnChildren(UGCQueryHandle_t handle, bool bReturnChildren) override { return true; }
        virtual bool SetReturnAdditionalPreviews(UGCQueryHandle_t handle, bool bReturnAdditionalPreviews) override { return true; }
        virtual bool SetReturnTotalOnly(UGCQueryHandle_t handle, bool bReturnTotalOnly) override { return true; }
        virtual bool SetReturnPlaytimeStats(UGCQueryHandle_t handle, uint32 unDays) override { return true; }
        virtual bool SetLanguage(UGCQueryHandle_t handle, const char *pchLanguage) override { return true; }
        virtual bool SetAllowCachedResponse(UGCQueryHandle_t handle, uint32 unMaxAgeSeconds) override { return true; }
        virtual bool SetAdminQuery(UGCUpdateHandle_t handle, bool bAdminQuery) override { return true; }
        virtual bool SetCloudFileNameFilter(UGCQueryHandle_t handle, const char *pMatchCloudFileName) override { return true; }
        virtual bool SetMatchAnyTag(UGCQueryHandle_t handle, bool bMatchAnyTag) override { return true; }
        virtual bool SetSearchText(UGCQueryHandle_t handle, const char *pSearchText) override { return true; }
        virtual bool SetRankedByTrendDays(UGCQueryHandle_t handle, uint32 unDays) override { return true; }
        virtual bool SetTimeCreatedDateRange(UGCQueryHandle_t handle, RTime32 rtStart, RTime32 rtEnd) override { return true; }
        virtual bool SetTimeUpdatedDateRange(UGCQueryHandle_t handle, RTime32 rtStart, RTime32 rtEnd) override { return true; }
        virtual bool AddRequiredKeyValueTag(UGCQueryHandle_t handle, const char *pKey, const char *pValue) override { return true; }
        virtual SteamAPICall_t RequestUGCDetails(PublishedFileId_t nPublishedFileID, uint32 unMaxAgeSeconds) override { return 0; }
        virtual SteamAPICall_t CreateItem(AppId_t nConsumerAppId, EWorkshopFileType eFileType) override { return 0; }
        virtual UGCUpdateHandle_t StartItemUpdate(AppId_t nConsumerAppId, PublishedFileId_t nPublishedFileId) override { return 1; }
        virtual bool SetItemTitle(UGCUpdateHandle_t handle, const char *pchTitle) override { return true; }
        virtual bool SetItemDescription(UGCUpdateHandle_t handle, const char *pchDescription) override { return true; }
        virtual bool SetItemUpdateLanguage(UGCUpdateHandle_t handle, const char *pchLanguage) override { return true; }
        virtual bool SetItemMetadata(UGCUpdateHandle_t handle, const char *pchMetaData) override { return true; }
        virtual bool SetItemVisibility(UGCUpdateHandle_t handle, ERemoteStoragePublishedFileVisibility eVisibility) override { return true; }
        virtual bool SetItemTags(UGCUpdateHandle_t updateHandle, const SteamParamStringArray_t *pTags, bool bAllowAdminTags = false) override { return true; }
        virtual bool SetItemContent(UGCUpdateHandle_t handle, const char *pszContentFolder) override { return true; }
        virtual bool SetItemPreview(UGCUpdateHandle_t handle, const char *pszPreviewFile) override { return true; }
        virtual bool SetAllowLegacyUpload(UGCUpdateHandle_t handle, bool bAllowLegacyUpload) override { return true; }
        virtual bool RemoveAllItemKeyValueTags(UGCUpdateHandle_t handle) override { return true; }
        virtual bool RemoveItemKeyValueTags(UGCUpdateHandle_t handle, const char *pchKey) override { return true; }
        virtual bool AddItemKeyValueTag(UGCUpdateHandle_t handle, const char *pchKey, const char *pchValue) override { return true; }
        virtual bool AddItemPreviewFile(UGCUpdateHandle_t handle, const char *pszPreviewFile, EItemPreviewType type) override { return true; }
        virtual bool AddItemPreviewVideo(UGCUpdateHandle_t handle, const char *pszVideoID) override { return true; }
        virtual bool UpdateItemPreviewFile(UGCUpdateHandle_t handle, uint32 index, const char *pszPreviewFile) override { return true; }
        virtual bool UpdateItemPreviewVideo(UGCUpdateHandle_t handle, uint32 index, const char *pszVideoID) override { return true; }
        virtual bool RemoveItemPreview(UGCUpdateHandle_t handle, uint32 index) override { return true; }
        virtual bool AddContentDescriptor(UGCUpdateHandle_t handle, EUGCContentDescriptorID descid) override { return true; }
        virtual bool RemoveContentDescriptor(UGCUpdateHandle_t handle, EUGCContentDescriptorID descid) override { return true; }
        virtual bool SetRequiredGameVersions(UGCUpdateHandle_t handle, const char *pszGameBranchMin, const char *pszGameBranchMax) override { return true; }
        virtual SteamAPICall_t SubmitItemUpdate(UGCUpdateHandle_t handle, const char *pchChangeNote) override { return 0; }
        virtual EItemUpdateStatus GetItemUpdateProgress(UGCUpdateHandle_t handle, uint64 *punBytesProcessed, uint64 *punBytesTotal) override {
            return k_EItemUpdateStatusInvalid;
        }
        virtual SteamAPICall_t SetUserItemVote(PublishedFileId_t nPublishedFileId, bool bVoteUp) override { return 0; }
        virtual SteamAPICall_t GetUserItemVote(PublishedFileId_t nPublishedFileId) override { return 0; }
        virtual SteamAPICall_t AddItemToFavorites(AppId_t nAppId, PublishedFileId_t nPublishedFileId) override { return 0; }
        virtual SteamAPICall_t RemoveItemFromFavorites(AppId_t nAppId, PublishedFileId_t nPublishedFileId) override { return 0; }
        virtual SteamAPICall_t SubscribeItem(PublishedFileId_t nPublishedFileId) override { return 0; }
        virtual SteamAPICall_t UnsubscribeItem(PublishedFileId_t nPublishedFileId) override { return 0; }
        virtual uint32 GetNumSubscribedItems(bool bIncludeLocallyDisabled = false) override { return 0; }
        virtual uint32 GetSubscribedItems(PublishedFileId_t *pvecPublishedFileID, uint32 cMaxEntries, bool bIncludeLocallyDisabled = false) override { return 0; }
        virtual uint32 GetItemState(PublishedFileId_t nPublishedFileId) override { return k_EItemStateNone; }
        virtual bool GetItemInstallInfo(PublishedFileId_t nPublishedFileId, uint64 *punSizeOnDisk, char *pchFolder, uint32 cchFolderBufferSize, uint32 *punTimeStamp) override { return false; }
        virtual bool GetItemDownloadInfo(PublishedFileId_t nPublishedFileId, uint64 *punBytesDownloaded, uint64 *punBytesTotal) override { return false; }
        virtual bool DownloadItem(PublishedFileId_t nPublishedFileId, bool bHighPriority) override { return false; }
        virtual bool BInitWorkshopForGameServer(DepotId_t unWorkshopDepotID, const char *pszFolder) override { return true; }
        virtual void SuspendDownloads(bool bSuspend) override {}
        virtual SteamAPICall_t StartPlaytimeTracking(PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs) override { return 0; }
        virtual SteamAPICall_t StopPlaytimeTracking(PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs) override { return 0; }
        virtual SteamAPICall_t StopPlaytimeTrackingForAllItems() override { return 0; }
        virtual SteamAPICall_t AddDependency(PublishedFileId_t nParentPublishedFileID, PublishedFileId_t nChildPublishedFileId) override { return 0; }
        virtual SteamAPICall_t RemoveDependency(PublishedFileId_t nParentPublishedFileID, PublishedFileId_t nChildPublishedFileId) override { return 0; }
        virtual SteamAPICall_t AddAppDependency(PublishedFileId_t nPublishedFileID, AppId_t nAppID) override { return 0; }
        virtual SteamAPICall_t RemoveAppDependency(PublishedFileId_t nPublishedFileID, AppId_t nAppID) override { return 0; }
        virtual SteamAPICall_t GetAppDependencies(PublishedFileId_t nPublishedFileID) override { return 0; }
        virtual SteamAPICall_t DeleteItem(PublishedFileId_t nPublishedFileID) override { return 0; }
        virtual bool ShowWorkshopEULA() override { return false; }
        virtual SteamAPICall_t GetWorkshopEULAStatus() override { return 0; }
        virtual uint32 GetUserContentDescriptorPreferences(EUGCContentDescriptorID *pvecDescriptors, uint32 cMaxEntries) override { return 0; }
        virtual bool SetItemsDisabledLocally(PublishedFileId_t *pvecPublishedFileIDs, uint32 unNumPublishedFileIDs, bool bDisabledLocally) override { return true; }
        virtual bool SetSubscriptionsLoadOrder(PublishedFileId_t *pvecPublishedFileIDs, uint32 unNumPublishedFileIDs) override { return true; }
        virtual bool MarkDownloadedItemAsUnused(PublishedFileId_t nPublishedFileID) override { return true; }
        virtual uint32 GetNumDownloadedItems() override { return 0; }
        virtual uint32 GetDownloadedItems(PublishedFileId_t *pvecPublishedFileIDs, uint32 cMaxEntries) override { return 0; }
    };
    static CSteamUGCEmu g_steamUGCInstance;

    // --- ISteamGameServer ---
    class CSteamGameServerEmu : public ISteamGameServer {
    public:
        virtual bool InitGameServer(uint32 unIP, uint16 usGamePort, uint16 usQueryPort, uint32 unFlags, AppId_t nGameAppId, const char *pchVersion) override {
            ReFixLog("[UnrealSteam] InitGameServer: port=%u, queryPort=%u, appID=%u", usGamePort, usQueryPort, nGameAppId);
            return true;
        }
        virtual void SetProduct(const char *pszProduct) override {}
        virtual void SetGameDescription(const char *pszGameDescription) override {}
        virtual void SetModDir(const char *pszModDir) override {}
        virtual void SetDedicatedServer(bool bDedicated) override {}
        virtual void LogOn(const char *pszToken) override {
            SteamServersConnected_t resp = {};
            QueuedCallbackItem item;
            item.iCallback = SteamServersConnected_t::k_iCallback;
            item.isGameServer = true;
            item.triggerTime = std::chrono::steady_clock::now();
            g_callbackQueue.push_back(item);
        }
        virtual void LogOnAnonymous() override { LogOn(nullptr); }
        virtual void LogOff() override {}
        virtual bool BLoggedOn() override { return true; }
        virtual bool BSecure() override { return true; }
        virtual CSteamID GetSteamID() override {
            return CSteamID(0x0110000100000000ULL | (uint64_t)((g_localSteamID & 0xFFFFFFFF) ^ 0x0000000012345678ULL));
        }
        virtual bool WasRestartRequested() override { return false; }
        virtual void SetMaxPlayerCount(int cPlayersMax) override {}
        virtual void SetBotPlayerCount(int cBotplayers) override {}
        virtual void SetServerName(const char *pszServerName) override {}
        virtual void SetMapName(const char *pszMapName) override {}
        virtual void SetPasswordProtected(bool bPasswordProtected) override {}
        virtual void SetSpectatorPort(uint16 unSpectatorPort) override {}
        virtual void SetSpectatorServerName(const char *pszSpectatorServerName) override {}
        virtual void ClearAllKeyValues() override {}
        virtual void SetKeyValue(const char *pKey, const char *pValue) override {}
        virtual void SetGameTags(const char *m_szGameTags) override {}
        virtual void SetGameData(const char *m_szGameData) override {}
        virtual void SetRegion(const char *pszRegion) override {}

        virtual bool SendUserConnectAndAuthenticate(uint32 unIPClient, const void *pvAuthBlob, uint32 cubAuthBlobSize, CSteamID *pSteamIDUser) override {
            CSteamID clientID(g_localSteamID ^ 0x01);
            if (pSteamIDUser) *pSteamIDUser = clientID;

            GSClientApprove_t resp = {};
            resp.m_SteamID = clientID;
            resp.m_OwnerSteamID = clientID;

            QueuedCallbackItem item;
            item.iCallback = GSClientApprove_t::k_iCallback;
            item.data.assign((uint8_t*)&resp, (uint8_t*)&resp + sizeof(resp));
            item.isGameServer = true;
            item.triggerTime = std::chrono::steady_clock::now();
            g_callbackQueue.push_back(item);

            ReFixLog("[UnrealSteam] SendUserConnectAndAuthenticate: Approved client %llu", clientID.ConvertToUint64());
            return true;
        }

        virtual CSteamID CreateUnauthenticatedUserConnection() override {
            return CSteamID(g_localSteamID ^ 0x02);
        }
        virtual void SendUserDisconnect(CSteamID steamIDUser) override {}
        virtual bool BUpdateUserData(CSteamID steamIDUser, const char *pchPlayerName, uint32 uScore) override { return true; }

        virtual HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket, const SteamNetworkingIdentity *pSnid) override {
            return g_steamUserInstance.GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket, pSnid);
        }
        virtual EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) override {
            return g_steamUserInstance.BeginAuthSession(pAuthTicket, cbAuthTicket, steamID);
        }
        virtual void EndAuthSession(CSteamID steamID) override {}
        virtual void CancelAuthTicket(HAuthTicket hAuthTicket) override {}
        virtual EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) override {
            return k_EUserHasLicenseResultHasLicense;
        }
        virtual bool RequestUserGroupStatus(CSteamID steamIDUser, CSteamID steamIDGroup) override { return true; }
        virtual void GetGameplayStats() override {}
        virtual SteamAPICall_t GetServerReputation() override { return 0; }
        virtual SteamIPAddress_t GetPublicIP() override {
            SteamIPAddress_t ip = {};
            ip.m_eType = k_ESteamIPTypeIPv4;
            ip.m_unIPv4 = 0x7F000001;
            return ip;
        }
        virtual bool HandleIncomingPacket(const void *pData, int cbData, uint32 srcIP, uint16 srcPort) override { return true; }
        virtual int GetNextOutgoingPacket(void *pOut, int cbMaxOut, uint32 *pNetAdr, uint16 *pPort) override { return 0; }
        virtual void SetAdvertiseServerActive(bool bActive) override {}
        virtual SteamAPICall_t AssociateWithClan(CSteamID steamIDClan) override { return 0; }
        virtual SteamAPICall_t ComputeNewPlayerCompatibility(CSteamID steamIDNewPlayer) override { return 0; }
        virtual void SetMasterServerHeartbeatInterval_DEPRECATED(int iHeartbeatInterval) override {}
        virtual void ForceMasterServerHeartbeat_DEPRECATED() override {}
    };
    static CSteamGameServerEmu g_steamGameServerInstance;

    // --- ISteamGameServer012 ABI Wrapper (Slot 24 = SendUserConnectAndAuthenticate) ---
    class CSteamGameServer012Emu : public ISteamGameServer012 {
    public:
        virtual bool InitGameServer(uint32 unIP, uint16 usGamePort, uint16 usQueryPort, uint32 unFlags, AppId_t nGameAppId, const char *pchVersion) override {
            return g_steamGameServerInstance.InitGameServer(unIP, usGamePort, usQueryPort, unFlags, nGameAppId, pchVersion);
        }
        virtual void SetProduct(const char *pszProduct) override { g_steamGameServerInstance.SetProduct(pszProduct); }
        virtual void SetGameDescription(const char *pszGameDescription) override { g_steamGameServerInstance.SetGameDescription(pszGameDescription); }
        virtual void SetModDir(const char *pszModDir) override { g_steamGameServerInstance.SetModDir(pszModDir); }
        virtual void SetDedicatedServer(bool bDedicated) override { g_steamGameServerInstance.SetDedicatedServer(bDedicated); }
        virtual void LogOn(const char *pszToken) override { g_steamGameServerInstance.LogOn(pszToken); }
        virtual void LogOnAnonymous() override { g_steamGameServerInstance.LogOnAnonymous(); }
        virtual void LogOff() override { g_steamGameServerInstance.LogOff(); }
        virtual bool BLoggedOn() override { return g_steamGameServerInstance.BLoggedOn(); }
        virtual bool BSecure() override { return g_steamGameServerInstance.BSecure(); }
        virtual CSteamID GetSteamID() override { return g_steamGameServerInstance.GetSteamID(); }
        virtual bool WasRestartRequested() override { return g_steamGameServerInstance.WasRestartRequested(); }
        virtual void SetMaxPlayerCount(int cPlayersMax) override { g_steamGameServerInstance.SetMaxPlayerCount(cPlayersMax); }
        virtual void SetBotPlayerCount(int cBotplayers) override { g_steamGameServerInstance.SetBotPlayerCount(cBotplayers); }
        virtual void SetServerName(const char *pszServerName) override { g_steamGameServerInstance.SetServerName(pszServerName); }
        virtual void SetMapName(const char *pszMapName) override { g_steamGameServerInstance.SetMapName(pszMapName); }
        virtual void SetPasswordProtected(bool bPasswordProtected) override { g_steamGameServerInstance.SetPasswordProtected(bPasswordProtected); }
        virtual void SetSpectatorPort(uint16 unSpectatorPort) override { g_steamGameServerInstance.SetSpectatorPort(unSpectatorPort); }
        virtual void SetSpectatorServerName(const char *pszSpectatorServerName) override { g_steamGameServerInstance.SetSpectatorServerName(pszSpectatorServerName); }
        virtual void ClearAllKeyValues() override { g_steamGameServerInstance.ClearAllKeyValues(); }
        virtual void SetKeyValue(const char *pKey, const char *pValue) override { g_steamGameServerInstance.SetKeyValue(pKey, pValue); }
        virtual void SetGameTags(const char *m_szGameTags) override { g_steamGameServerInstance.SetGameTags(m_szGameTags); }
        virtual void SetGameData(const char *m_szGameData) override { g_steamGameServerInstance.SetGameData(m_szGameData); }
        virtual void SetRegion(const char *pszRegion) override { g_steamGameServerInstance.SetRegion(pszRegion); }

        // Slot 24 in ISteamGameServer012!
        virtual bool SendUserConnectAndAuthenticate(uint32 unIPClient, const void *pvAuthBlob, uint32 cubAuthBlobSize, CSteamID *pSteamIDUser) override {
            return g_steamGameServerInstance.SendUserConnectAndAuthenticate(unIPClient, pvAuthBlob, cubAuthBlobSize, pSteamIDUser);
        }
        virtual CSteamID CreateUnauthenticatedUserConnection() override {
            return g_steamGameServerInstance.CreateUnauthenticatedUserConnection();
        }
        virtual void SendUserDisconnect(CSteamID steamIDUser) override {
            g_steamGameServerInstance.SendUserDisconnect(steamIDUser);
        }
        virtual bool BUpdateUserData(CSteamID steamIDUser, const char *pchPlayerName, uint32 uScore) override {
            return g_steamGameServerInstance.BUpdateUserData(steamIDUser, pchPlayerName, uScore);
        }

        virtual HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override {
            return g_steamGameServerInstance.GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket, nullptr);
        }
        virtual EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) override {
            return g_steamGameServerInstance.BeginAuthSession(pAuthTicket, cbAuthTicket, steamID);
        }
        virtual void EndAuthSession(CSteamID steamID) override {
            g_steamGameServerInstance.EndAuthSession(steamID);
        }
        virtual void CancelAuthTicket(HAuthTicket hAuthTicket) override {
            g_steamGameServerInstance.CancelAuthTicket(hAuthTicket);
        }
        virtual EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) override {
            return g_steamGameServerInstance.UserHasLicenseForApp(steamID, appID);
        }
        virtual bool RequestUserGroupStatus(CSteamID steamIDUser, CSteamID steamIDGroup) override {
            return g_steamGameServerInstance.RequestUserGroupStatus(steamIDUser, steamIDGroup);
        }
        virtual void GetGameplayStats() override { g_steamGameServerInstance.GetGameplayStats(); }
        virtual SteamAPICall_t GetServerReputation() override { return g_steamGameServerInstance.GetServerReputation(); }
        virtual uint32 GetPublicIP_old() override { return 0x7F000001; }
        virtual bool HandleIncomingPacket(const void *pData, int cbData, uint32 srcIP, uint16 srcPort) override {
            return g_steamGameServerInstance.HandleIncomingPacket(pData, cbData, srcIP, srcPort);
        }
        virtual int GetNextOutgoingPacket(void *pOut, int cbMaxOut, uint32 *pNetAdr, uint16 *pPort) override {
            return g_steamGameServerInstance.GetNextOutgoingPacket(pOut, cbMaxOut, pNetAdr, pPort);
        }
        virtual void EnableHeartbeats(bool bActive) override { g_steamGameServerInstance.SetAdvertiseServerActive(bActive); }
        virtual void SetHeartbeatInterval(int iHeartbeatInterval) override {}
        virtual void ForceHeartbeat() override {}
        virtual SteamAPICall_t AssociateWithClan(CSteamID steamIDClan) override { return g_steamGameServerInstance.AssociateWithClan(steamIDClan); }
        virtual SteamAPICall_t ComputeNewPlayerCompatibility(CSteamID steamIDNewPlayer) override { return g_steamGameServerInstance.ComputeNewPlayerCompatibility(steamIDNewPlayer); }
    };
    static CSteamGameServer012Emu g_steamGameServer012Instance;

    // --- ISteamGameServerStats ---
    class CSteamGameServerStatsEmu : public ISteamGameServerStats {
    public:
        virtual SteamAPICall_t RequestUserStats(CSteamID steamIDUser) override {
            GSStatsReceived_t resp = {};
            resp.m_eResult = k_EResultOK;
            resp.m_steamIDUser = steamIDUser;
            return PostCallResult(GSStatsReceived_t::k_iCallback, &resp, sizeof(resp));
        }
        virtual bool GetUserStat(CSteamID steamIDUser, const char *pchName, int32 *pData) override {
            if (pData) *pData = 0; return true;
        }
        virtual bool GetUserStat(CSteamID steamIDUser, const char *pchName, float *pData) override {
            if (pData) *pData = 0.0f; return true;
        }
        virtual bool GetUserAchievement(CSteamID steamIDUser, const char *pchName, bool *pbAchieved) override {
            if (pbAchieved) *pbAchieved = false; return true;
        }
        virtual bool SetUserStat(CSteamID steamIDUser, const char *pchName, int32 nData) override { return true; }
        virtual bool SetUserStat(CSteamID steamIDUser, const char *pchName, float fData) override { return true; }
        virtual bool UpdateUserAvgRateStat(CSteamID steamIDUser, const char *pchName, float flCountThisSession, double dSessionLength) override { return true; }
        virtual bool SetUserAchievement(CSteamID steamIDUser, const char *pchName) override { return true; }
        virtual bool ClearUserAchievement(CSteamID steamIDUser, const char *pchName) override { return true; }
        virtual SteamAPICall_t StoreUserStats(CSteamID steamIDUser) override {
            GSStatsStored_t resp = {};
            resp.m_eResult = k_EResultOK;
            resp.m_steamIDUser = steamIDUser;
            return PostCallResult(GSStatsStored_t::k_iCallback, &resp, sizeof(resp));
        }
    };
    static CSteamGameServerStatsEmu g_steamGameServerStatsInstance;

    // --- ISteamHTTP ---
    class CSteamHTTPEmu : public ISteamHTTP {
    public:
        virtual HTTPRequestHandle CreateHTTPRequest(EHTTPMethod eHTTPRequestMethod, const char *pchAbsoluteURL) override { return 1; }
        virtual bool SetHTTPRequestContextValue(HTTPRequestHandle hRequest, uint64 ulContextValue) override { return true; }
        virtual bool SetHTTPRequestNetworkActivityTimeout(HTTPRequestHandle hRequest, uint32 unTimeoutSeconds) override { return true; }
        virtual bool SetHTTPRequestHeaderValue(HTTPRequestHandle hRequest, const char *pchHeaderName, const char *pchHeaderValue) override { return true; }
        virtual bool SetHTTPRequestGetOrPostParameter(HTTPRequestHandle hRequest, const char *pchParamName, const char *pchParamValue) override { return true; }
        virtual bool SendHTTPRequest(HTTPRequestHandle hRequest, SteamAPICall_t *pCallHandle) override {
            HTTPRequestCompleted_t resp = {};
            resp.m_hRequest = hRequest;
            resp.m_eStatusCode = k_EHTTPStatusCode200OK;
            resp.m_bRequestSuccessful = true;
            resp.m_unBodySize = 0;
            SteamAPICall_t call = PostCallResult(HTTPRequestCompleted_t::k_iCallback, &resp, sizeof(resp));
            if (pCallHandle) *pCallHandle = call;
            return true;
        }
        virtual bool SendHTTPRequestAndStreamResponse(HTTPRequestHandle hRequest, SteamAPICall_t *pCallHandle) override {
            return SendHTTPRequest(hRequest, pCallHandle);
        }
        virtual bool DeferHTTPRequest(HTTPRequestHandle hRequest) override { return true; }
        virtual bool PrioritizeHTTPRequest(HTTPRequestHandle hRequest) override { return true; }
        virtual bool GetHTTPResponseHeaderSize(HTTPRequestHandle hRequest, const char *pchHeaderName, uint32 *unResponseHeaderSize) override { return false; }
        virtual bool GetHTTPResponseHeaderValue(HTTPRequestHandle hRequest, const char *pchHeaderName, uint8 *pHeaderValueBuffer, uint32 unBufferSize) override { return false; }
        virtual bool GetHTTPResponseBodySize(HTTPRequestHandle hRequest, uint32 *unBodySize) override { if (unBodySize) *unBodySize = 0; return true; }
        virtual bool GetHTTPResponseBodyData(HTTPRequestHandle hRequest, uint8 *pBodyDataBuffer, uint32 unBufferSize) override { return true; }
        virtual bool GetHTTPStreamingResponseBodyData(HTTPRequestHandle hRequest, uint32 cOffset, uint8 *pBodyDataBuffer, uint32 unBufferSize) override { return true; }
        virtual bool ReleaseHTTPRequest(HTTPRequestHandle hRequest) override { return true; }
        virtual bool GetHTTPDownloadProgressPct(HTTPRequestHandle hRequest, float *pflPercentOut) override { if (pflPercentOut) *pflPercentOut = 1.0f; return true; }
        virtual bool SetHTTPRequestRawPostBody(HTTPRequestHandle hRequest, const char *pchContentType, uint8 *pubBody, uint32 unBodyLen) override { return true; }
        virtual HTTPCookieContainerHandle CreateCookieContainer(bool bAllowResponsesToModify) override { return 1; }
        virtual bool ReleaseCookieContainer(HTTPCookieContainerHandle hCookieContainer) override { return true; }
        virtual bool SetCookie(HTTPCookieContainerHandle hCookieContainer, const char *pchHost, const char *pchUrl, const char *pchCookie) override { return true; }
        virtual bool SetHTTPRequestCookieContainer(HTTPRequestHandle hRequest, HTTPCookieContainerHandle hCookieContainer) override { return true; }
        virtual bool SetHTTPRequestUserAgentInfo(HTTPRequestHandle hRequest, const char *pchUserAgentInfo) override { return true; }
        virtual bool SetHTTPRequestRequiresVerifiedCertificate(HTTPRequestHandle hRequest, bool bRequireVerifiedCertificate) override { return true; }
        virtual bool SetHTTPRequestAbsoluteTimeoutMS(HTTPRequestHandle hRequest, uint32 unTimeoutMilliseconds) override { return true; }
        virtual bool GetHTTPRequestWasTimedOut(HTTPRequestHandle hRequest, bool *pbWasTimedOut) override { if (pbWasTimedOut) *pbWasTimedOut = false; return true; }
    };
    static CSteamHTTPEmu g_steamHTTPInstance;

    // --- ISteamInput / ISteamController ---
    class CSteamInputEmu : public ISteamInput {
    public:
        virtual bool Init(bool bExplicitlyCallRunFrame) override { return true; }
        virtual bool Shutdown() override { return true; }
        virtual bool SetInputActionManifestFilePath(const char *pchInputActionManifestAbsolutePath) override { return true; }
        virtual void RunFrame(bool bReservedValue = true) override {}
        virtual bool BWaitForData(bool bWaitForever, uint32 unTimeout) override { return true; }
        virtual bool BNewDataAvailable() override { return false; }
        virtual int GetConnectedControllers(InputHandle_t *handlesOut) override { return 0; }
        virtual void EnableDeviceCallbacks() override {}
        virtual void EnableActionEventCallbacks(SteamInputActionEventCallbackPointer pCallback) override {}
        virtual InputActionSetHandle_t GetActionSetHandle(const char *pszActionSetName) override { return 1; }
        virtual void ActivateActionSet(InputHandle_t inputHandle, InputActionSetHandle_t actionSetHandle) override {}
        virtual InputActionSetHandle_t GetCurrentActionSet(InputHandle_t inputHandle) override { return 1; }
        virtual void ActivateActionSetLayer(InputHandle_t inputHandle, InputActionSetHandle_t actionSetLayerHandle) override {}
        virtual void DeactivateActionSetLayer(InputHandle_t inputHandle, InputActionSetHandle_t actionSetLayerHandle) override {}
        virtual void DeactivateAllActionSetLayers(InputHandle_t inputHandle) override {}
        virtual int GetActiveActionSetLayers(InputHandle_t inputHandle, InputActionSetHandle_t *handlesOut) override { return 0; }
        virtual InputDigitalActionHandle_t GetDigitalActionHandle(const char *pszActionName) override { return 1; }
        virtual InputDigitalActionData_t GetDigitalActionData(InputHandle_t inputHandle, InputDigitalActionHandle_t digitalActionHandle) override {
            InputDigitalActionData_t d = {}; return d;
        }
        virtual int GetDigitalActionOrigins(InputHandle_t inputHandle, InputActionSetHandle_t actionSetHandle, InputDigitalActionHandle_t digitalActionHandle, EInputActionOrigin *originsOut) override { return 0; }
        virtual InputAnalogActionHandle_t GetAnalogActionHandle(const char *pszActionName) override { return 1; }
        virtual InputAnalogActionData_t GetAnalogActionData(InputHandle_t inputHandle, InputAnalogActionHandle_t analogActionHandle) override {
            InputAnalogActionData_t d = {}; return d;
        }
        virtual int GetAnalogActionOrigins(InputHandle_t inputHandle, InputActionSetHandle_t actionSetHandle, InputAnalogActionHandle_t analogActionHandle, EInputActionOrigin *originsOut) override { return 0; }
        virtual const char *GetGlyphPNGForActionOrigin(EInputActionOrigin eOrigin, ESteamInputGlyphSize eSize, uint32 unFlags) override { return ""; }
        virtual const char *GetGlyphSVGForActionOrigin(EInputActionOrigin eOrigin, uint32 unFlags) override { return ""; }
        virtual const char *GetStringForActionOrigin(EInputActionOrigin eOrigin) override { return ""; }
        virtual const char *GetStringForAnalogActionName(InputAnalogActionHandle_t eActionHandle) override { return ""; }
        virtual void StopAnalogActionMomentum(InputHandle_t inputHandle, InputAnalogActionHandle_t eAction) override {}
        virtual InputMotionData_t GetMotionData(InputHandle_t inputHandle) override { InputMotionData_t m = {}; return m; }
        virtual void TriggerVibration(InputHandle_t inputHandle, unsigned short usLeftSpeed, unsigned short usRightSpeed) override {}
        virtual void TriggerVibrationExtended(InputHandle_t inputHandle, unsigned short usLeftSpeed, unsigned short usRightSpeed, unsigned short usLeftTriggerSpeed, unsigned short usRightTriggerSpeed) override {}
        virtual void TriggerSimpleHapticEvent(InputHandle_t inputHandle, EControllerHapticLocation eHapticLocation, uint8 nIntensity, char nGainDB, uint8 nOtherIntensity, char nOtherGainDB) override {}
        virtual void SetLEDColor(InputHandle_t inputHandle, uint8 nColorR, uint8 nColorG, uint8 nColorB, unsigned int nFlags) override {}
        virtual void Legacy_TriggerHapticPulse(InputHandle_t inputHandle, ESteamControllerPad eTargetPad, unsigned short usDurationMicroSec) override {}
        virtual void Legacy_TriggerRepeatedHapticPulse(InputHandle_t inputHandle, ESteamControllerPad eTargetPad, unsigned short usDurationMicroSec, unsigned short usOffMicroSec, unsigned short unRepeat, unsigned int nFlags) override {}
        virtual bool ShowBindingPanel(InputHandle_t inputHandle) override { return true; }
        virtual ESteamInputType GetInputTypeForHandle(InputHandle_t inputHandle) override { return k_ESteamInputType_XBox360Controller; }
        virtual InputHandle_t GetControllerForGamepadIndex(int nIndex) override { return (InputHandle_t)1; }
        virtual int GetGamepadIndexForController(InputHandle_t ulinputHandle) override { return 0; }
        virtual const char *GetStringForDigitalActionName(InputDigitalActionHandle_t eActionHandle) override { return ""; }
        virtual const char *GetStringForXboxOrigin(EXboxOrigin eXboxOrigin) override { return ""; }
        virtual const char *GetGlyphForXboxOrigin(EXboxOrigin eXboxOrigin) override { return ""; }
        virtual EInputActionOrigin GetActionOriginFromXboxOrigin(InputHandle_t inputHandle, EXboxOrigin eXboxOrigin) override { return k_EInputActionOrigin_None; }
        virtual EInputActionOrigin TranslateActionOrigin(ESteamInputType eDestinationInputType, EInputActionOrigin eSourceOrigin) override { return eSourceOrigin; }
        virtual bool GetDeviceBindingRevision(InputHandle_t inputHandle, int *pMajor, int *pMinor) override {
            if (pMajor) *pMajor = 1; if (pMinor) *pMinor = 0; return true;
        }
        virtual uint32 GetRemotePlaySessionID(InputHandle_t inputHandle) override { return 0; }
        virtual uint16 GetSessionInputConfigurationSettings() override { return 0; }
        virtual const char *GetGlyphForActionOrigin_Legacy(EInputActionOrigin eOrigin) override { return ""; }
        virtual void SetDualSenseTriggerEffect(InputHandle_t inputHandle, const ScePadTriggerEffectParam *pParam) override {}
    };
    static CSteamInputEmu g_steamInputInstance;

    // --- ISteamInventory ---
    class CSteamInventoryEmu : public ISteamInventory {
    public:
        virtual EResult GetResultStatus(SteamInventoryResult_t resultHandle) override { return k_EResultOK; }
        virtual bool GetResultItems(SteamInventoryResult_t resultHandle, SteamItemDetails_t *pOutItemsArray, uint32 *punOutItemsArraySize) override {
            if (punOutItemsArraySize) *punOutItemsArraySize = 0; return true;
        }
        virtual bool GetResultItemProperty(SteamInventoryResult_t resultHandle, uint32 unItemIndex, const char *pchPropertyName, char *pchValueBuffer, uint32 *punValueBufferSizeOut) override { return false; }
        virtual uint32 GetResultTimestamp(SteamInventoryResult_t resultHandle) override { return (uint32)::time(NULL); }
        virtual bool CheckResultSteamID(SteamInventoryResult_t resultHandle, CSteamID steamIDExpected) override { return true; }
        virtual void DestroyResult(SteamInventoryResult_t resultHandle) override {}
        virtual bool GetAllItems(SteamInventoryResult_t *pResultHandle) override {
            if (pResultHandle) *pResultHandle = 1;
            SteamInventoryFullUpdate_t resp = {};
            resp.m_handle = 1;
            PostCallback(SteamInventoryFullUpdate_t::k_iCallback, &resp, sizeof(resp));
            return true;
        }
        virtual bool GetItemsByID(SteamInventoryResult_t *pResultHandle, const SteamItemInstanceID_t *pInstanceIDs, uint32 unCountInstanceIDs) override {
            if (pResultHandle) *pResultHandle = 1; return true;
        }
        virtual bool SerializeResult(SteamInventoryResult_t resultHandle, void *pOutBuffer, uint32 *punOutBufferSize) override { return false; }
        virtual bool DeserializeResult(SteamInventoryResult_t *pOutResultHandle, const void *pBuffer, uint32 unBufferSize, bool bRESERVED_MUST_BE_FALSE = false) override { return false; }
        virtual bool GenerateItems(SteamInventoryResult_t *pResultHandle, const SteamItemDef_t *pArrayItemDefs, const uint32 *punArrayQuantity, uint32 unArrayLength) override { return false; }
        virtual bool GrantPromoItems(SteamInventoryResult_t *pResultHandle) override { return false; }
        virtual bool AddPromoItem(SteamInventoryResult_t *pResultHandle, SteamItemDef_t itemDef) override { return false; }
        virtual bool AddPromoItems(SteamInventoryResult_t *pResultHandle, const SteamItemDef_t *pArrayItemDefs, uint32 unArrayLength) override { return false; }
        virtual bool ConsumeItem(SteamInventoryResult_t *pResultHandle, SteamItemInstanceID_t itemConsume, uint32 unQuantity) override { return false; }
        virtual bool ExchangeItems(SteamInventoryResult_t *pResultHandle, const SteamItemDef_t *pArrayGenerate, const uint32 *punArrayGenerateQuantity, uint32 unArrayGenerateLength, const SteamItemInstanceID_t *pArrayDestroy, const uint32 *punArrayDestroyQuantity, uint32 unArrayDestroyLength) override { return false; }
        virtual bool TransferItemQuantity(SteamInventoryResult_t *pResultHandle, SteamItemInstanceID_t itemIdSource, uint32 unQuantity, SteamItemInstanceID_t itemIdDest) override { return false; }
        virtual void SendItemDropHeartbeat() override {}
        virtual bool TriggerItemDrop(SteamInventoryResult_t *pResultHandle, SteamItemDef_t dropListDefinition) override { return false; }
        virtual bool TradeItems(SteamInventoryResult_t *pResultHandle, CSteamID steamIDPartner, const SteamItemInstanceID_t *pArrayGive, const uint32 *pArrayGiveQuantity, uint32 nArrayGiveLength, const SteamItemInstanceID_t *pArrayGet, const uint32 *pArrayGetQuantity, uint32 nArrayGetLength) override { return false; }
        virtual bool LoadItemDefinitions() override { return true; }
        virtual bool GetItemDefinitionIDs(SteamItemDef_t *pItemDefIDs, uint32 *punItemDefIDsArraySize) override {
            if (punItemDefIDsArraySize) *punItemDefIDsArraySize = 0; return true;
        }
        virtual bool GetItemDefinitionProperty(SteamItemDef_t iDefinition, const char *pchPropertyName, char *pchValueBuffer, uint32 *punValueBufferSizeOut) override { return false; }
        virtual SteamAPICall_t RequestEligiblePromoItemDefinitionsIDs(CSteamID steamID) override { return 0; }
        virtual bool GetEligiblePromoItemDefinitionIDs(CSteamID steamID, SteamItemDef_t *pItemDefIDs, uint32 *punItemDefIDsArraySize) override { return false; }
        virtual SteamAPICall_t StartPurchase(const SteamItemDef_t *pArrayItemDefs, const uint32 *punArrayQuantity, uint32 unArrayLength) override { return 0; }
        virtual SteamAPICall_t RequestPrices() override { return 0; }
        virtual uint32 GetNumItemsWithPrices() override { return 0; }
        virtual bool GetItemsWithPrices(SteamItemDef_t *pArrayItemDefs, uint64 *pCurrentPrices, uint64 *pBasePrices, uint32 unArrayLength) override { return false; }
        virtual bool GetItemPrice(SteamItemDef_t iDefinition, uint64 *pCurrentPrice, uint64 *pBasePrice) override { return false; }
        virtual SteamInventoryUpdateHandle_t StartUpdateProperties() override { return 1; }
        virtual bool SetProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName, const char *pchPropertyValue) override { return true; }
        virtual bool SetProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName, bool bValue) override { return true; }
        virtual bool SetProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName, int64 nValue) override { return true; }
        virtual bool SetProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName, float flValue) override { return true; }
        virtual bool RemoveProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName) override { return true; }
        virtual bool SubmitUpdateProperties(SteamInventoryUpdateHandle_t handle, SteamInventoryResult_t *pResultHandle) override {
            if (pResultHandle) *pResultHandle = 1; return true;
        }
        virtual bool InspectItem(SteamInventoryResult_t *pResultHandle, const char *pchItemToken) override { return false; }
    };
    static CSteamInventoryEmu g_steamInventoryInstance;

    // --- ISteamScreenshots ---
    class CSteamScreenshotsEmu : public ISteamScreenshots {
    public:
        virtual ScreenshotHandle WriteScreenshot(void *pubRGB, uint32 cubRGB, int nWidth, int nHeight) override { return 1; }
        virtual ScreenshotHandle AddScreenshotToLibrary(const char *pchFilename, const char *pchThumbnailFilename, int nWidth, int nHeight) override { return 1; }
        virtual void TriggerScreenshot() override {}
        virtual void HookScreenshots(bool bHook) override {}
        virtual bool SetLocation(ScreenshotHandle hScreenshot, const char *pchLocation) override { return true; }
        virtual bool TagUser(ScreenshotHandle hScreenshot, CSteamID steamID) override { return true; }
        virtual bool TagPublishedFile(ScreenshotHandle hScreenshot, PublishedFileId_t unPublishedFileId) override { return true; }
        virtual bool IsScreenshotsHooked() override { return false; }
        virtual ScreenshotHandle AddVRScreenshotToLibrary(EVRScreenshotType eType, const char *pchFilename, const char *pchVRFilename) override { return 1; }
    };
    static CSteamScreenshotsEmu g_steamScreenshotsInstance;

    // --- ISteamTimeline ---
    class CSteamTimelineEmu : public ISteamTimeline {
    public:
        virtual void SetTimelineTooltip(const char *pchDescription, float flTimeDelta) override {}
        virtual void ClearTimelineTooltip(float flTimeDelta) override {}
        virtual void SetTimelineGameMode(ETimelineGameMode eMode) override {}

        virtual TimelineEventHandle_t AddInstantaneousTimelineEvent(const char *pchTitle, const char *pchDescription, const char *pchIcon, uint32 unIconPriority, float flStartOffsetSeconds = 0.f, ETimelineEventClipPriority ePossibleClip = k_ETimelineEventClipPriority_None) override { return 1; }
        virtual TimelineEventHandle_t AddRangeTimelineEvent(const char *pchTitle, const char *pchDescription, const char *pchIcon, uint32 unIconPriority, float flStartOffsetSeconds = 0.f, float flDuration = 0.f, ETimelineEventClipPriority ePossibleClip = k_ETimelineEventClipPriority_None) override { return 1; }
        virtual TimelineEventHandle_t StartRangeTimelineEvent(const char *pchTitle, const char *pchDescription, const char *pchIcon, uint32 unPriority, float flStartOffsetSeconds, ETimelineEventClipPriority ePossibleClip) override { return 1; }
        virtual void UpdateRangeTimelineEvent(TimelineEventHandle_t ulEvent, const char *pchTitle, const char *pchDescription, const char *pchIcon, uint32 unPriority, ETimelineEventClipPriority ePossibleClip) override {}
        virtual void EndRangeTimelineEvent(TimelineEventHandle_t ulEvent, float flEndOffsetSeconds) override {}
        virtual void RemoveTimelineEvent(TimelineEventHandle_t ulEvent) override {}
        virtual SteamAPICall_t DoesEventRecordingExist(TimelineEventHandle_t ulEvent) override { return 0; }

        virtual void StartGamePhase() override {}
        virtual void EndGamePhase() override {}
        virtual void SetGamePhaseID(const char *pchPhaseID) override {}
        virtual SteamAPICall_t DoesGamePhaseRecordingExist(const char *pchPhaseID) override { return 0; }
        virtual void AddGamePhaseTag(const char *pchTagName, const char *pchTagIcon, const char *pchTagGroup, uint32 unPriority) override {}
        virtual void SetGamePhaseAttribute(const char *pchAttributeGroup, const char *pchAttributeValue, uint32 unPriority) override {}
        virtual void OpenOverlayToGamePhase(const char *pchPhaseID) override {}
        virtual void OpenOverlayToTimelineEvent(const TimelineEventHandle_t ulEvent) override {}
    };
    static CSteamTimelineEmu g_steamTimelineInstance;

    // --- ISteamClient ---
    class CSteamClientEmu : public ISteamClient {
    public:
        virtual HSteamPipe CreateSteamPipe() override { return g_hSteamPipe; }
        virtual bool BReleaseSteamPipe(HSteamPipe hSteamPipe) override { return true; }
        virtual HSteamUser ConnectToGlobalUser(HSteamPipe hSteamPipe) override { return g_hSteamUser; }
        virtual HSteamUser CreateLocalUser(HSteamPipe *phSteamPipe, EAccountType eAccountType) override {
            if (phSteamPipe) *phSteamPipe = g_hSteamPipe;
            return g_hSteamUser;
        }
        virtual void ReleaseUser(HSteamPipe hSteamPipe, HSteamUser hUser) override {}

        virtual ISteamUser *GetISteamUser(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            if (pchVersion && (strstr(pchVersion, "SteamUser021") || strstr(pchVersion, "SteamUser020") ||
                strstr(pchVersion, "SteamUser019") || strstr(pchVersion, "SteamUser018") ||
                strstr(pchVersion, "SteamUser017") || strstr(pchVersion, "SteamUser016") ||
                strstr(pchVersion, "SteamUser015") || strstr(pchVersion, "SteamUser014") ||
                strstr(pchVersion, "SteamUser013") || strstr(pchVersion, "SteamUser012") ||
                strstr(pchVersion, "SteamUser011") || strstr(pchVersion, "SteamUser010") ||
                strstr(pchVersion, "SteamUser009") || strstr(pchVersion, "SteamUser008"))) {
                return (ISteamUser*)&g_steamUser021Instance;
            }
            return &g_steamUserInstance;
        }
        virtual ISteamGameServer *GetISteamGameServer(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            if (pchVersion && (strstr(pchVersion, "SteamGameServer012") || strstr(pchVersion, "SteamGameServer011") ||
                strstr(pchVersion, "SteamGameServer010") || strstr(pchVersion, "SteamGameServer009") ||
                strstr(pchVersion, "SteamGameServer008"))) {
                return (ISteamGameServer*)&g_steamGameServer012Instance;
            }
            return &g_steamGameServerInstance;
        }
        virtual void SetLocalIPBinding(const SteamIPAddress_t &unIP, uint16 usPort) override {}
        virtual ISteamFriends *GetISteamFriends(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return reinterpret_cast<ISteamFriends*>(&g_steamFriendsInstance);
        }
        virtual ISteamUtils *GetISteamUtils(HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamUtilsInstance;
        }
        virtual ISteamMatchmaking *GetISteamMatchmaking(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamMatchmakingInstance;
        }
        virtual ISteamMatchmakingServers *GetISteamMatchmakingServers(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamMatchmakingServersInstance;
        }
        virtual void *GetISteamGenericInterface(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return GetGenericInterface(pchVersion);
        }
        virtual ISteamUserStats *GetISteamUserStats(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamUserStatsInstance;
        }
        virtual ISteamGameServerStats *GetISteamGameServerStats(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamGameServerStatsInstance;
        }
        virtual ISteamApps *GetISteamApps(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamAppsInstance;
        }
        virtual ISteamNetworking *GetISteamNetworking(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamNetworkingInstance;
        }
        virtual ISteamRemoteStorage *GetISteamRemoteStorage(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamRemoteStorageInstance;
        }
        virtual ISteamScreenshots *GetISteamScreenshots(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamScreenshotsInstance;
        }
        virtual void RunFrame() override {}
        virtual uint32 GetIPCCallCount() override { return 0; }
        virtual void SetWarningMessageHook(SteamAPIWarningMessageHook_t pFunction) override {}
        virtual bool BShutdownIfAllPipesClosed() override { return true; }
        virtual ISteamHTTP *GetISteamHTTP(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamHTTPInstance;
        }
        virtual ISteamController *GetISteamController(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return (ISteamController*)&g_steamInputInstance;
        }
        virtual ISteamUGC *GetISteamUGC(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamUGCInstance;
        }
        virtual ISteamMusic *GetISteamMusic(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override { return nullptr; }
        virtual ISteamHTMLSurface *GetISteamHTMLSurface(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override { return nullptr; }
        virtual void DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess(void (*)()) override {}
        virtual void DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess(void (*)()) override {}
        virtual void Set_SteamAPI_CCheckCallbackRegisteredInProcess(SteamAPI_CheckCallbackRegistered_t func) override {}
        virtual ISteamInventory *GetISteamInventory(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamInventoryInstance;
        }
        virtual ISteamVideo *GetISteamVideo(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override { return nullptr; }
        virtual ISteamParentalSettings *GetISteamParentalSettings(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override { return nullptr; }
        virtual ISteamInput *GetISteamInput(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
            return &g_steamInputInstance;
        }
        virtual ISteamParties *GetISteamParties(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override { return nullptr; }
        virtual ISteamRemotePlay *GetISteamRemotePlay(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override { return nullptr; }
        virtual void DestroyAllInterfaces() override {}
    };
    static CSteamClientEmu g_steamClientInstance;

    // =========================================================================
    // INTERFACE RESOLUTION & PUBLIC ACCESSORS
    // =========================================================================
    void* GetGenericInterface(const char* pchVersion) {
        RecordCall();
        if (!pchVersion) return nullptr;

        ReFixLog("[UnrealSteam] Resolving Interface Version: '%s'", pchVersion);

        if (strstr(pchVersion, "SteamClient") || strstr(pchVersion, "STEAMCLIENT"))
            return &g_steamClientInstance;
        if (strstr(pchVersion, "SteamUser021") || strstr(pchVersion, "SteamUser020") ||
            strstr(pchVersion, "SteamUser019") || strstr(pchVersion, "SteamUser018") ||
            strstr(pchVersion, "SteamUser017") || strstr(pchVersion, "SteamUser016") ||
            strstr(pchVersion, "SteamUser015") || strstr(pchVersion, "SteamUser014") ||
            strstr(pchVersion, "SteamUser013") || strstr(pchVersion, "SteamUser012") ||
            strstr(pchVersion, "SteamUser011") || strstr(pchVersion, "SteamUser010") ||
            strstr(pchVersion, "SteamUser009") || strstr(pchVersion, "SteamUser008"))
            return &g_steamUser021Instance;
        if (strstr(pchVersion, "SteamUser0") || strstr(pchVersion, "STEAMUSER"))
            return &g_steamUserInstance;
        if (strstr(pchVersion, "SteamFriends") || strstr(pchVersion, "STEAMFRIENDS"))
            return &g_steamFriendsInstance;
        if (strstr(pchVersion, "SteamUtils") || strstr(pchVersion, "STEAMUTILS"))
            return &g_steamUtilsInstance;
        if (strstr(pchVersion, "SteamMatchMakingServers") || strstr(pchVersion, "STEAMMATCHMAKINGSERVERS") || strstr(pchVersion, "SteamMatchmakingServers"))
            return &g_steamMatchmakingServersInstance;
        if (strstr(pchVersion, "SteamMatchMaking") || strstr(pchVersion, "STEAMMATCHMAKING") || strstr(pchVersion, "SteamMatchmaking"))
            return &g_steamMatchmakingInstance;
        if (strstr(pchVersion, "STEAMUSERSTATS") || strstr(pchVersion, "SteamUserStats"))
            return &g_steamUserStatsInstance;
        if (strstr(pchVersion, "STEAMAPPS") || strstr(pchVersion, "SteamApps"))
            return &g_steamAppsInstance;
        if (strstr(pchVersion, "SteamNetworkingSockets") || strstr(pchVersion, "STEAMNETWORKINGSOCKETS"))
            return &g_steamNetworkingSocketsInstance;
        if (strstr(pchVersion, "SteamNetworkingUtils") || strstr(pchVersion, "STEAMNETWORKINGUTILS"))
            return &g_steamNetworkingUtilsInstance;
        if (strstr(pchVersion, "SteamNetworkingMessages") || strstr(pchVersion, "STEAMNETWORKINGMESSAGES"))
            return &g_steamNetworkingMessagesInstance;
        if (strstr(pchVersion, "SteamNetworking") || strstr(pchVersion, "STEAMNETWORKING"))
            return &g_steamNetworkingInstance;
        if (strstr(pchVersion, "STEAMREMOTESTORAGE") || strstr(pchVersion, "SteamRemoteStorage"))
            return &g_steamRemoteStorageInstance;
        if (strstr(pchVersion, "STEAMUGC") || strstr(pchVersion, "SteamUGC"))
            return &g_steamUGCInstance;
        if (strstr(pchVersion, "SteamGameServerStats") || strstr(pchVersion, "STEAMGAMESERVERSTATS"))
            return &g_steamGameServerStatsInstance;
        if (strstr(pchVersion, "SteamGameServer012") || strstr(pchVersion, "SteamGameServer011") ||
            strstr(pchVersion, "SteamGameServer010") || strstr(pchVersion, "SteamGameServer009") ||
            strstr(pchVersion, "SteamGameServer008"))
            return &g_steamGameServer012Instance;
        if (strstr(pchVersion, "SteamGameServer") || strstr(pchVersion, "STEAMGAMESERVER"))
            return &g_steamGameServerInstance;
        if (strstr(pchVersion, "STEAMHTTP") || strstr(pchVersion, "SteamHTTP"))
            return &g_steamHTTPInstance;
        if (strstr(pchVersion, "SteamInput") || strstr(pchVersion, "STEAMINPUT") || strstr(pchVersion, "SteamController"))
            return &g_steamInputInstance;
        if (strstr(pchVersion, "STEAMINVENTORY") || strstr(pchVersion, "SteamInventory"))
            return &g_steamInventoryInstance;
        if (strstr(pchVersion, "STEAMSCREENSHOTS") || strstr(pchVersion, "SteamScreenshots"))
            return &g_steamScreenshotsInstance;
        if (strstr(pchVersion, "STEAMTIMELINE") || strstr(pchVersion, "SteamTimeline"))
            return &g_steamTimelineInstance;

        ReFixLog("[UnrealSteam] Warning: Unrecognized interface '%s'", pchVersion);
        return nullptr;
    }

    void* FindOrCreateUserInterface(int32_t hUser, const char* pszVersion) {
        return GetGenericInterface(pszVersion);
    }

    void* FindOrCreateGameServerInterface(int32_t hUser, const char* pszVersion) {
        return GetGenericInterface(pszVersion);
    }

    void* CreateInterface(const char* ver) {
        return GetGenericInterface(ver);
    }

    struct ContextInitData {
        void (*pFn)(void* pCtx);
        uintptr_t counter;
        void* ctx;
    };
    static std::atomic<uintptr_t> g_contextCounter{ 1 };

    void* ContextInit(void* pContextInitData) {
        if (!pContextInitData) return nullptr;
        static std::mutex ctxLock;
        std::lock_guard<std::mutex> lock(ctxLock);

        auto data = reinterpret_cast<ContextInitData*>(pContextInitData);
        void* localCtx = &data->ctx;
        if (data->counter != g_contextCounter.load()) {
            if (data->pFn) data->pFn(localCtx);
            data->counter = g_contextCounter.load();
        }
        return localCtx;
    }

    int32_t GetHSteamPipe() { return g_hSteamPipe; }
    int32_t GetHSteamUser() { return g_hSteamUser; }
    int32_t GameServer_GetHSteamPipe() { return g_hGameServerPipe; }
    int32_t GameServer_GetHSteamUser() { return g_hGameServerUser; }

    uint64_t GetLocalSteamID() { return g_localSteamID; }
    const char* GetPersonaName() { return g_personaName.c_str(); }
    uint32_t GetAppID() { return g_appID; }

    void* GetSteamClient() { return &g_steamClientInstance; }
    void* GetSteamUser() { return &g_steamUser021Instance; }
    void* GetSteamFriends() { return &g_steamFriendsInstance; }
    void* GetSteamUtils() { return &g_steamUtilsInstance; }
    void* GetSteamMatchmaking() { return &g_steamMatchmakingInstance; }
    void* GetSteamMatchmakingServers() { return &g_steamMatchmakingServersInstance; }
    void* GetSteamUserStats() { return &g_steamUserStatsInstance; }
    void* GetSteamApps() { return &g_steamAppsInstance; }
    void* GetSteamNetworking() { return &g_steamNetworkingInstance; }
    void* GetSteamNetworkingSockets() { RecordCall(); return &g_steamNetworkingSocketsInstance; }
    void* GetSteamNetworkingUtils() { RecordCall(); return &g_steamNetworkingUtilsInstance; }
    void* GetSteamNetworkingMessages() { RecordCall(); return &g_steamNetworkingMessagesInstance; }
    void* GetSteamRemoteStorage() { return &g_steamRemoteStorageInstance; }
    void* GetSteamUGC() { return &g_steamUGCInstance; }
    void* GetSteamGameServer() { return &g_steamGameServer012Instance; }
    void* GetSteamGameServerStats() { return &g_steamGameServerStatsInstance; }
    void* GetSteamGameServerNetworking() { return &g_steamNetworkingInstance; }
    void* GetSteamHTTP() { return &g_steamHTTPInstance; }
    void* GetSteamInput() { return &g_steamInputInstance; }
    void* GetSteamInventory() { return &g_steamInventoryInstance; }
    void* GetSteamScreenshots() { return &g_steamScreenshotsInstance; }
    void* GetSteamTimeline() { return &g_steamTimelineInstance; }

    void NotifyEOSLobby(uint64_t lobbyID) {
        if (lobbyID != 0) {
            g_activeLobbyID.store(lobbyID);
            LobbyInfo lob = {};
            lob.id = lobbyID;
            lob.owner = g_localSteamID;
            lob.maxMembers = 4;
            lob.joinable = true;
            lob.members.push_back(g_localSteamID);
            lob.lastSeen = std::chrono::steady_clock::now();
            g_lobbies[lobbyID] = lob;

            ReFixLog("[UnrealSteam] Synchronized active Lobby ID from EOS: %llu", lobbyID);
        }
    }

    bool Initialize() {
        {
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            if (g_bInitialized) return true;
            g_bInitialized = true;
        }

        LoadConfig();
        InitSockets();

        // 1. Initialize ReFix Universal LAN Core
        refix::lan::ILanCore& core = refix::lan::ILanCore::Get();
        if (!core.Initialize("Steam_" + std::to_string(g_appID), g_listenPort)) {
            ReFixLog("[UnrealSteam] Fatal: LanCore::Initialize failed (could not start transport or discovery)");
            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            g_bInitialized = false;
            return false;
        }
        core.Identity().SetDisplayName(g_personaName);

        // Bind local SteamID
        refix::lan::PeerId localPeer = core.Identity().GetLocalPeerId();
        refix::lan::ExternalId ext;
        ext.platform = refix::lan::ExternalPlatform::Steam;
        ext.numericId = g_localSteamID;
        ext.stringId = std::to_string(g_localSteamID);

        refix::lan::PeerInfo selfInfo;
        selfInfo.peerId = localPeer;
        selfInfo.displayName = g_personaName;
        selfInfo.endpoint = core.Transport().GetLocalDataEndpoint();
        selfInfo.externalId = ext;
        selfInfo.state = refix::lan::PeerTransportState::Connected;
        core.Peers().RegisterOrUpdatePeer(selfInfo);
        core.Peers().BindExternalId(localPeer, ext);

        // Subscribe to LanCore events
        core.Callbacks().Subscribe(refix::lan::LanEvent::Type::PeerDiscovered, [](const refix::lan::LanEvent& ev) {
            auto pInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(ev.peerId);
            if (pInfo && pInfo->externalId.platform == refix::lan::ExternalPlatform::Steam && pInfo->externalId.numericId != 0) {
                uint64_t sid = pInfo->externalId.numericId;
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                auto it = g_peers.find(sid);
                if (it != g_peers.end()) {
                    it->second.personaName = pInfo->displayName;
                }
                ReFixLog("[UnrealSteam] LanCore PeerDiscovered: steamID=%llu (%s) at %s",
                         sid, pInfo->displayName.c_str(), pInfo->endpoint.ToString().c_str());
            }
        });

        core.Callbacks().Subscribe(refix::lan::LanEvent::Type::LobbyCreated, [](const refix::lan::LanEvent& ev) {
            uint64_t steamLobbyId = EnsureSteamLobbyID(ev.lobbyId);
            ReFixLog("[UnrealSteam] LanCore LobbyCreated: coreId='%s' -> steamID=%llu", ev.lobbyId.c_str(), steamLobbyId);
        });

        core.Callbacks().Subscribe(refix::lan::LanEvent::Type::LobbyUpdated, [](const refix::lan::LanEvent& ev) {
            auto rec = refix::lan::ILanCore::Get().Lobby().GetLobby(ev.lobbyId);
            if (!rec) return;

            uint64_t steamLobbyId = EnsureSteamLobbyID(ev.lobbyId);
            if (steamLobbyId == 0) return;
            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                LobbyInfo& lob = g_lobbies[steamLobbyId];
                lob.id = steamLobbyId;
                lob.maxMembers = rec->maxMembers;
                lob.joinable = rec->joinable;
                lob.lastSeen = std::chrono::steady_clock::now();

                uint64_t ownerSteamId = 0;
                auto itOwnerAttr = rec->attributes.find("__steam_owner");
                if (itOwnerAttr != rec->attributes.end() && !itOwnerAttr->second.asString.empty()) {
                    ownerSteamId = _strtoui64(itOwnerAttr->second.asString.c_str(), nullptr, 10);
                }

                auto ownerInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(rec->ownerPeerId);
                if (ownerInfo && ownerInfo->externalId.numericId != 0) {
                    lob.owner = ownerInfo->externalId.numericId;
                } else if (ownerSteamId != 0) {
                    lob.owner = ownerSteamId;
                    refix::lan::PeerInfo hInfo;
                    hInfo.peerId = rec->ownerPeerId;
                    hInfo.machineId = rec->ownerPeerId.high;
                    hInfo.endpoint = rec->hostEndpoint;
                    hInfo.externalId.platform = refix::lan::ExternalPlatform::Steam;
                    hInfo.externalId.numericId = ownerSteamId;
                    hInfo.externalId.stringId = std::to_string(ownerSteamId);
                    hInfo.state = refix::lan::PeerTransportState::Discovered;
                    refix::lan::ILanCore::Get().Peers().RegisterOrUpdatePeer(hInfo);
                }

                lob.members.clear();
                for (const auto& m : rec->members) {
                    auto mInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(m.peerId);
                    if (mInfo && mInfo->externalId.numericId != 0) {
                        lob.members.push_back(mInfo->externalId.numericId);
                    }
                }

                for (const auto& [k, v] : rec->attributes) {
                    lob.data[k] = v.asString;
                }
            }

            LobbyDataUpdate_t dataUpd = {};
            dataUpd.m_ulSteamIDLobby = steamLobbyId;
            dataUpd.m_ulSteamIDMember = steamLobbyId;
            dataUpd.m_bSuccess = 1;
            PostCallback(LobbyDataUpdate_t::k_iCallback, &dataUpd, sizeof(dataUpd));
        });

        core.Callbacks().Subscribe(refix::lan::LanEvent::Type::MemberJoined, [](const refix::lan::LanEvent& ev) {
            uint64_t steamLobbyId = EnsureSteamLobbyID(ev.lobbyId);
            uint64_t memberSteamId = 0;
            auto mInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(ev.peerId);
            if (mInfo && mInfo->externalId.numericId != 0) {
                memberSteamId = mInfo->externalId.numericId;
            } else if (ev.peerId.low != 0) {
                memberSteamId = ev.peerId.low;
            }

            if (memberSteamId != 0) {
                {
                    std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                    auto& members = g_lobbies[steamLobbyId].members;
                    if (std::find(members.begin(), members.end(), memberSteamId) == members.end()) {
                        members.push_back(memberSteamId);
                    }
                }

                LobbyChatUpdate_t cu = {};
                cu.m_ulSteamIDLobby = steamLobbyId;
                cu.m_ulSteamIDUserChanged = memberSteamId;
                cu.m_ulSteamIDMakingChange = memberSteamId;
                cu.m_rgfChatMemberStateChange = 0x0001; // k_EChatMemberStateChangeEntered
                PostCallback(LobbyChatUpdate_t::k_iCallback, &cu, sizeof(cu));
            }
        });

        core.Callbacks().Subscribe(refix::lan::LanEvent::Type::MemberLeft, [](const refix::lan::LanEvent& ev) {
            uint64_t steamLobbyId = EnsureSteamLobbyID(ev.lobbyId);
            uint64_t memberSteamId = 0;
            auto mInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(ev.peerId);
            if (mInfo && mInfo->externalId.numericId != 0) {
                memberSteamId = mInfo->externalId.numericId;
            } else if (ev.peerId.low != 0) {
                memberSteamId = ev.peerId.low;
            }

            if (memberSteamId != 0) {
                {
                    std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                    auto it = g_lobbies.find(steamLobbyId);
                    if (it != g_lobbies.end()) {
                        auto& members = it->second.members;
                        members.erase(std::remove(members.begin(), members.end(), memberSteamId), members.end());
                    }
                }

                LobbyChatUpdate_t cu = {};
                cu.m_ulSteamIDLobby = steamLobbyId;
                cu.m_ulSteamIDUserChanged = memberSteamId;
                cu.m_ulSteamIDMakingChange = memberSteamId;
                cu.m_rgfChatMemberStateChange = (ev.leaveReason == refix::lan::MemberLeaveReason::Kicked) ? 0x0008 : 0x0002;
                PostCallback(LobbyChatUpdate_t::k_iCallback, &cu, sizeof(cu));
            }
        });

        core.Callbacks().Subscribe(refix::lan::LanEvent::Type::LobbyJoinResult, [](const refix::lan::LanEvent& ev) {
            uint64_t steamLobbyId = EnsureSteamLobbyID(ev.lobbyId);
            SteamAPICall_t callHandle = 0;
            {
                std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                auto it = g_pendingJoins.find(ev.lobbyId);
                if (it != g_pendingJoins.end()) {
                    callHandle = it->second.callHandle;
                    g_pendingJoins.erase(it);
                }
            }

            LobbyEnter_t resp = {};
            resp.m_ulSteamIDLobby = steamLobbyId;
            resp.m_rgfChatPermissions = 0xFFFFFFFF;
            resp.m_bLocked = false;

            if (ev.success) {
                resp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseSuccess;
                g_activeLobbyID.store(steamLobbyId);
                {
                    std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
                    LobbyInfo& lob = g_lobbies[steamLobbyId];
                    lob.id = steamLobbyId;
                    if (std::find(lob.members.begin(), lob.members.end(), g_localSteamID) == lob.members.end()) {
                        lob.members.push_back(g_localSteamID);
                    }
                }
                PostCallback(LobbyEnter_t::k_iCallback, &resp, sizeof(resp));
                if (callHandle != 0) {
                    CompleteCallResult(callHandle, LobbyEnter_t::k_iCallback, &resp, sizeof(resp), false);
                }

                LobbyDataUpdate_t dataUpd = {};
                dataUpd.m_ulSteamIDLobby = steamLobbyId;
                dataUpd.m_ulSteamIDMember = steamLobbyId;
                dataUpd.m_bSuccess = 1;
                PostCallback(LobbyDataUpdate_t::k_iCallback, &dataUpd, sizeof(dataUpd));
                ReFixLog("[UnrealSteam] LanCore LobbyJoinResult: Successfully joined lobby %llu (coreId='%s')",
                         steamLobbyId, ev.lobbyId.c_str());
            } else {
                if (ev.joinResponseCode == 1) {
                    resp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseFull;
                } else if (ev.joinResponseCode == 2) {
                    resp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseNotAllowed;
                } else {
                    resp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseDoesntExist;
                }
                PostCallback(LobbyEnter_t::k_iCallback, &resp, sizeof(resp));
                if (callHandle != 0) {
                    CompleteCallResult(callHandle, LobbyEnter_t::k_iCallback, &resp, sizeof(resp), true);
                }
                ReFixLog("[UnrealSteam] LanCore LobbyJoinResult: Join REJECTED for lobby %llu (coreId='%s', code=%u)",
                         steamLobbyId, ev.lobbyId.c_str(), ev.joinResponseCode);
            }
        });

        core.Callbacks().Subscribe(refix::lan::LanEvent::Type::DataPacketReceived, [](const refix::lan::LanEvent& ev) {
            uint64_t senderSteamId = 0;
            auto pInfo = refix::lan::ILanCore::Get().Peers().FindByPeerId(ev.peerId);
            if (pInfo && pInfo->externalId.numericId != 0) {
                senderSteamId = pInfo->externalId.numericId;
            } else {
                senderSteamId = ev.peerId.low != 0 ? ev.peerId.low : ev.peerId.high;
            }

            P2PPacket pkt;
            pkt.senderID = senderSteamId;
            pkt.channel = ev.channel;
            pkt.data = ev.payload;

            std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
            g_p2pIncoming[ev.channel].push(pkt);
            ReFixLog("[UnrealSteam] LanCore DataPacketReceived: channel=%u sender=%llu bytes=%zu",
                     ev.channel, senderSteamId, ev.payload.size());
        });

        g_contextCounter.fetch_add(1);

        // Queue SteamServersConnected_t on startup (outside state lock)
        SteamServersConnected_t conn = {};
        PostCallback(SteamServersConnected_t::k_iCallback, &conn, sizeof(conn), 0.01);

        // Broadcast NetPacket Ping with our persona name so LAN peers discover us immediately
        BroadcastNetPacket(1, g_personaName.c_str(), g_personaName.size());

        ReFixLog("=================================================================");
        ReFixLog("  Re:Goldberg for Unreal Engine Initialized Successfully");
        ReFixLog("  Persona Name: '%s' | SteamID64: %llu | AppID: %u", g_personaName.c_str(), g_localSteamID, g_appID);
        ReFixLog("  UDP Listen Port: %u | Language: '%s'", g_listenPort, g_language.c_str());
        ReFixLog("=================================================================");

        return true;
    }

    void Shutdown() {
        refix::lan::ILanCore::Get().Shutdown();

        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        if (!g_bInitialized) return;

        if (g_udpSocket != INVALID_SOCKET) {
            closesocket(g_udpSocket);
            g_udpSocket = INVALID_SOCKET;
        }

        g_clientCallbacks.clear();
        g_serverCallbacks.clear();
        g_callResultListeners.clear();
        g_callbackQueue.clear();
        g_callResultMap.clear();
        g_lobbies.clear();
        g_peers.clear();

        {
            std::lock_guard<std::mutex> sLock(g_socketsMutex);
            for (auto& pair : g_connections) {
                while (!pair.second.incomingMessages.empty()) {
                    auto* msg = pair.second.incomingMessages.front();
                    if (msg) msg->Release();
                    pair.second.incomingMessages.pop();
                }
                for (auto& ooo : pair.second.outOfOrderInbound) {
                    if (ooo.second) ooo.second->Release();
                }
                pair.second.outOfOrderInbound.clear();
            }
            g_connections.clear();
            g_sessionToConnection.clear();
        }

        g_bInitialized = false;
        ReFixLog("[UnrealSteam] Shutdown complete.");
    }

    bool InitFlat(char* pOutErrMsg) {
        bool ok = Initialize();
        if (!ok && pOutErrMsg) {
            strncpy_s(pOutErrMsg, 1024, "ReFix UnrealSteamEmu::Initialize failed", _TRUNCATE);
        }
        return ok;
    }

    int InitInternal(const char* pszInternalCheckInterfaceVersions, char* pOutErrMsg) {
        ReFixLog("[UnrealSteam] InitInternal called with versions: '%s'", pszInternalCheckInterfaceVersions ? pszInternalCheckInterfaceVersions : "null");
        bool ok = Initialize();
        if (!ok) {
            if (pOutErrMsg) strncpy_s(pOutErrMsg, 1024, "ReFix UnrealSteamEmu::Initialize failed", _TRUNCATE);
            return 1; // k_ESteamAPIInitResult_Failed
        }
        return 0; // k_ESteamAPIInitResult_OK
    }

    bool GameServer_Init(uint32_t unIP, uint16_t usGamePort, uint16_t usQueryPort, int eServerMode, const char* pchVersionString) {
        ReFixLog("[UnrealSteam] GameServer_Init: IP=%u, GamePort=%u, QueryPort=%u, Mode=%d, Ver=%s",
                 unIP, usGamePort, usQueryPort, eServerMode, pchVersionString ? pchVersionString : "null");
        return Initialize();
    }

    bool GameServer_InitSafe() {
        return Initialize();
    }

    uint32_t GetAuthSessionTicket(void* pTicket, int cbMaxTicket, uint32_t* pcbTicket, const void* pSteamNetworkingIdentity) {
        return g_steamUserInstance.GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket, (const SteamNetworkingIdentity*)pSteamNetworkingIdentity);
    }

    uint32_t GetAuthTicketForWebApi(const char* pchIdentity) {
        return g_steamUserInstance.GetAuthTicketForWebApi(pchIdentity);
    }

    bool IsInitialized() {
        return g_bInitialized;
    }

    void SimulatePeerEndpoint(uint64_t steamID, const char* ipStr, uint16_t port) {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        DiscoveredPeer& peer = g_peers[steamID];
        peer.steamID = steamID;
        inet_pton(AF_INET, ipStr, &peer.ip);
        peer.ip = ntohl(peer.ip);
        peer.port = port;
        peer.lastSeen = std::chrono::steady_clock::now();
    }

    void SendTestLanPacket(uint64_t remoteID, uint8_t msgType, const void* payload, size_t payloadLen, int dir) {
        SendLanPacket(CSteamID(remoteID), msgType, payload, payloadLen, (ReFix::PacketDirection)dir);
    }
}

