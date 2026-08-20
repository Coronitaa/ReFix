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
#include "include/steam/isteamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"

#include "unreal_steam_emu.h"
#include "unreal_detect.h"

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

    static void LoadConfig() {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string exeDir(exePath);
        size_t lastSlash = exeDir.find_last_of("\\/");
        if (lastSlash != std::string::npos) exeDir = exeDir.substr(0, lastSlash + 1);

        std::string iniPath = exeDir + "ReFix.ini";
        DWORD dwAttr = GetFileAttributesA(iniPath.c_str());
        if (dwAttr == INVALID_FILE_ATTRIBUTES) {
            iniPath = exeDir + "..\\..\\ReFix.ini";
        }

        char buf[256];
        GetPrivateProfileStringA("Unreal.Steam", "PersonaName", "", buf, sizeof(buf), iniPath.c_str());
        if (buf[0]) g_personaName = buf;
        else {
            GetPrivateProfileStringA("User", "PersonaName", "Player", buf, sizeof(buf), iniPath.c_str());
            g_personaName = buf;
        }

        char bufId[64];
        GetPrivateProfileStringA("Unreal.Steam", "SteamId", "", bufId, sizeof(bufId), iniPath.c_str());
        if (bufId[0]) {
            g_localSteamID = _strtoui64(bufId, nullptr, 10);
        } else {
            GetPrivateProfileStringA("User", "SteamId", "0", bufId, sizeof(bufId), iniPath.c_str());
            g_localSteamID = _strtoui64(bufId, nullptr, 10);
            if (g_localSteamID == 0) {
                g_localSteamID = GenerateDeterministicSteamID();
            }
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

        // Apply Steam AppID environment variables
        char appIdStr[32];
        sprintf_s(appIdStr, "%u", g_appID);
        SetEnvironmentVariableA("SteamAppId", appIdStr);
        SetEnvironmentVariableA("SteamGameId", appIdStr);
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
        uint8_t  msgType;    // 1: Ping, 2: LobbyAnnounce, 3: LobbyQuery, 4: LobbyJoin, 5: P2P
        uint64_t senderID;
        uint32_t appID;
        uint32_t payloadLen;
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
                ReFixLog("[UnrealSteam] Warning: Could not bind UDP socket to port %u", g_listenPort);
            } else {
                ReFixLog("[UnrealSteam] Bound UDP socket to port %u for LAN discovery & P2P", g_listenPort);
            }
        }
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

        std::vector<std::string> destinations = { "255.255.255.255" };
        if (!g_customBroadcasts.empty()) {
            std::stringstream ss(g_customBroadcasts);
            std::string ipStr;
            while (std::getline(ss, ipStr, ',')) {
                if (!ipStr.empty()) destinations.push_back(ipStr);
            }
        }

        for (const auto& dstIP : destinations) {
            sockaddr_in dest = {};
            dest.sin_family = AF_INET;
            dest.sin_port = htons(g_listenPort);
            inet_pton(AF_INET, dstIP.c_str(), &dest.sin_addr);

            sendto(g_udpSocket, (const char*)buf.data(), (int)buf.size(), 0, (sockaddr*)&dest, sizeof(dest));
        }
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

                    // Track peer
                    DiscoveredPeer& peer = g_peers[hdr->senderID];
                    peer.steamID = hdr->senderID;
                    peer.ip = ntohl(fromAddr.sin_addr.s_addr);
                    peer.port = ntohs(fromAddr.sin_port);
                    peer.lastSeen = std::chrono::steady_clock::now();

                    if (hdr->msgType == 2 && pLen > 0) { // Lobby Announcement
                        std::string meta((char*)payload, pLen);
                        std::stringstream ss(meta);
                        uint64_t lID = 0, lOwner = 0;
                        int lMax = 4;
                        ss >> lID >> lOwner >> lMax;
                        if (lID != 0) {
                            LobbyInfo& lob = g_lobbies[lID];
                            lob.id = lID;
                            lob.owner = lOwner;
                            lob.maxMembers = lMax;
                            lob.lastSeen = std::chrono::steady_clock::now();
                            if (std::find(lob.members.begin(), lob.members.end(), lOwner) == lob.members.end()) {
                                lob.members.push_back(lOwner);
                            }
                        }
                    } else if (hdr->msgType == 5 && pLen >= 4) { // P2P packet
                        int channel = *(int*)payload;
                        P2PPacket pkt;
                        pkt.senderID = hdr->senderID;
                        pkt.channel = channel;
                        pkt.data.assign(payload + 4, payload + pLen);
                        g_p2pIncoming[channel].push(pkt);
                    }
                }
            }
        }
    }

    void RunCallbacks() {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        if (!g_bInitialized) return;

        PollNetwork();

        auto now = std::chrono::steady_clock::now();

        // 1. Process CallResults
        for (auto& pair : g_callResultMap) {
            auto& cr = pair.second;
            if (!cr.completed && now >= cr.triggerTime) {
                cr.completed = true;

                auto itListener = g_callResultListeners.find(cr.hAPICall);
                if (itListener != g_callResultListeners.end() && itListener->second) {
                    itListener->second->Run(cr.data.data(), cr.failed, cr.hAPICall);
                }

                // Trigger SteamAPICallCompleted_t (callback 703)
                SteamAPICallCompleted_t cc = {};
                cc.m_hAsyncCall = cr.hAPICall;
                cc.m_iCallback = cr.iCallback;
                cc.m_cubParam = (uint32_t)cr.data.size();

                auto range = g_clientCallbacks.equal_range(SteamAPICallCompleted_t::k_iCallback);
                for (auto it = range.first; it != range.second; ++it) {
                    if (it->second) it->second->Run(&cc);
                }
            }
        }

        // 2. Process generic Callbacks
        for (auto it = g_callbackQueue.begin(); it != g_callbackQueue.end(); ) {
            if (now >= it->triggerTime) {
                auto range = g_clientCallbacks.equal_range(it->iCallback);
                for (auto cbIt = range.first; cbIt != range.second; ++cbIt) {
                    if (cbIt->second) cbIt->second->Run(it->data.data());
                }
                it = g_callbackQueue.erase(it);
            } else {
                ++it;
            }
        }
    }

    void GameServer_RunCallbacks() {
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        if (!g_bInitialized) return;

        PollNetwork();
        auto now = std::chrono::steady_clock::now();

        for (auto it = g_callbackQueue.begin(); it != g_callbackQueue.end(); ) {
            if (it->isGameServer && now >= it->triggerTime) {
                auto range = g_serverCallbacks.equal_range(it->iCallback);
                for (auto cbIt = range.first; cbIt != range.second; ++cbIt) {
                    if (cbIt->second) cbIt->second->Run(it->data.data());
                }
                it = g_callbackQueue.erase(it);
            } else {
                ++it;
            }
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
                uint32_t* p = (uint32_t*)pTicket;
                p[0] = 1; // ticket format version
                *(uint64_t*)(p + 1) = g_localSteamID;
                p[3] = g_appID;
                p[4] = 0x7F000001; // 127.0.0.1
                p[5] = (uint32_t)::time(NULL);
                memset((uint8_t*)pTicket + 24, 0xCC, ticketLen - 24);
                if (pcbTicket) *pcbTicket = ticketLen;

                HAuthTicket hTicket = ++g_nextAuthTicket;

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

            // Generate deterministic 256-byte ticket payload for EOS
            std::vector<uint8_t> ticketData(256, 0);
            uint64_t sid = g_localSteamID;
            uint32_t app = g_appID;
            uint32_t tNow = (uint32_t)::time(NULL);
            memcpy(ticketData.data() + 0, &sid, sizeof(sid));
            memcpy(ticketData.data() + 8, &app, sizeof(app));
            memcpy(ticketData.data() + 12, &tNow, sizeof(tNow));
            if (pchIdentity) {
                strncpy_s((char*)(ticketData.data() + 16), 64, pchIdentity, _TRUNCATE);
            }
            for (size_t i = 80; i < ticketData.size(); i++) {
                ticketData[i] = (uint8_t)((hTicket * 31 + i * 13) & 0xFF);
            }

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
            return (int)g_peers.size();
        }

        virtual CSteamID GetFriendByIndex(int iFriend, int iFriendFlags) override {
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
            auto it = g_peers.find(steamIDFriend.ConvertToUint64());
            if (it != g_peers.end() && !it->second.personaName.empty()) {
                return it->second.personaName.c_str();
            }
            return "ReFix Peer";
        }

        virtual bool GetFriendGamePlayed(CSteamID steamIDFriend, FriendGameInfo_t *pFriendGameInfo) override {
            if (pFriendGameInfo) {
                pFriendGameInfo->m_gameID = CGameID(g_appID);
                pFriendGameInfo->m_unGameIP = 0x7F000001;
                pFriendGameInfo->m_usGamePort = 7777;
                pFriendGameInfo->m_usQueryPort = 27015;
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

    // --- ISteamMatchmaking ---
    class CSteamMatchmakingEmu : public ISteamMatchmaking {
    public:
        virtual int GetFavoriteGameCount() override { return 0; }
        virtual bool GetFavoriteGame(int iGame, AppId_t *pnAppID, uint32 *pnIP, uint16 *pnConnPort, uint16 *pnQueryPort, uint32 *punFlags, uint32 *pRTime32LastPlayedOnServer) override { return false; }
        virtual int AddFavoriteGame(AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags, uint32 rTime32LastPlayedOnServer) override { return 0; }
        virtual bool RemoveFavoriteGame(AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags) override { return false; }

        virtual SteamAPICall_t RequestLobbyList() override {
            BroadcastNetPacket(3, nullptr, 0); // Query lobbies on LAN

            LobbyMatchList_t resp = {};
            resp.m_nLobbiesMatching = (uint32_t)g_lobbies.size();
            ReFixLog("[UnrealSteam] RequestLobbyList: Returning %u lobbies", resp.m_nLobbiesMatching);
            return PostCallResult(LobbyMatchList_t::k_iCallback, &resp, sizeof(resp), 0.05);
        }

        virtual void AddRequestLobbyListStringFilter(const char *pchKeyToMatch, const char *pchValueToMatch, ELobbyComparison eComparisonType) override {}
        virtual void AddRequestLobbyListNumericalFilter(const char *pchKeyToMatch, int nValueToMatch, ELobbyComparison eComparisonType) override {}
        virtual void AddRequestLobbyListNearValueFilter(const char *pchKeyToMatch, int nValueToBeCloseTo) override {}
        virtual void AddRequestLobbyListFilterSlotsAvailable(int nSlotsAvailable) override {}
        virtual void AddRequestLobbyListDistanceFilter(ELobbyDistanceFilter eLobbyDistanceFilter) override {}
        virtual void AddRequestLobbyListResultCountFilter(int cMaxResults) override {}
        virtual void AddRequestLobbyListCompatibleMembersFilter(CSteamID steamIDLobby) override {}

        virtual SteamAPICall_t CreateLobby(ELobbyType eLobbyType, int cMaxMembers) override {
            uint64_t lID = 0x1860000000000000ULL | (uint64_t)(g_localSteamID & 0xFFFFFFFF) | ((uint64_t)(::time(NULL) & 0xFFFF) << 32);
            g_activeLobbyID.store(lID);

            LobbyInfo lob = {};
            lob.id = lID;
            lob.owner = g_localSteamID;
            lob.type = eLobbyType;
            lob.maxMembers = cMaxMembers;
            lob.joinable = true;
            lob.members.push_back(g_localSteamID);
            lob.lastSeen = std::chrono::steady_clock::now();
            g_lobbies[lID] = lob;

            // Announce on LAN
            std::stringstream ss;
            ss << lID << " " << g_localSteamID << " " << cMaxMembers;
            std::string meta = ss.str();
            BroadcastNetPacket(2, meta.c_str(), meta.size());

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

            ReFixLog("[UnrealSteam] CreateLobby: Created Lobby %llu (max=%d, owner=%llu)", lID, cMaxMembers, g_localSteamID);
            return hCall;
        }

        virtual SteamAPICall_t JoinLobby(CSteamID steamIDLobby) override {
            uint64_t lID = steamIDLobby.ConvertToUint64();
            g_activeLobbyID.store(lID);

            LobbyInfo& lob = g_lobbies[lID];
            lob.id = lID;
            if (std::find(lob.members.begin(), lob.members.end(), g_localSteamID) == lob.members.end()) {
                lob.members.push_back(g_localSteamID);
            }

            LobbyEnter_t resp = {};
            resp.m_ulSteamIDLobby = lID;
            resp.m_rgfChatPermissions = 0xFFFFFFFF;
            resp.m_bLocked = false;
            resp.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseSuccess;

            PostCallback(LobbyEnter_t::k_iCallback, &resp, sizeof(resp));

            LobbyDataUpdate_t dataUpd = {};
            dataUpd.m_ulSteamIDLobby = lID;
            dataUpd.m_ulSteamIDMember = lID;
            dataUpd.m_bSuccess = 1;
            PostCallback(LobbyDataUpdate_t::k_iCallback, &dataUpd, sizeof(dataUpd));

            ReFixLog("[UnrealSteam] JoinLobby: Joined Lobby %llu", lID);
            return PostCallResult(LobbyEnter_t::k_iCallback, &resp, sizeof(resp));
        }

        virtual void LeaveLobby(CSteamID steamIDLobby) override {
            uint64_t lID = steamIDLobby.ConvertToUint64();
            auto it = g_lobbies.find(lID);
            if (it != g_lobbies.end()) {
                auto mIt = std::find(it->second.members.begin(), it->second.members.end(), g_localSteamID);
                if (mIt != it->second.members.end()) it->second.members.erase(mIt);
            }
            if (g_activeLobbyID.load() == lID) g_activeLobbyID.store(0);
            ReFixLog("[UnrealSteam] LeaveLobby: Left Lobby %llu", lID);
        }

        virtual bool InviteUserToLobby(CSteamID steamIDLobby, CSteamID steamIDInvitee) override { return true; }

        virtual int GetNumLobbyMembers(CSteamID steamIDLobby) override {
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end()) return (int)it->second.members.size();
            return 1;
        }

        virtual CSteamID GetLobbyMemberByIndex(CSteamID steamIDLobby, int iMember) override {
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end() && iMember >= 0 && iMember < (int)it->second.members.size()) {
                return CSteamID(it->second.members[iMember]);
            }
            return CSteamID(g_localSteamID);
        }

        virtual const char *GetLobbyData(CSteamID steamIDLobby, const char *pchKey) override {
            if (!pchKey) return "";
            auto it = g_lobbies.find(steamIDLobby.ConvertToUint64());
            if (it != g_lobbies.end()) {
                auto dIt = it->second.data.find(pchKey);
                if (dIt != it->second.data.end()) return dIt->second.c_str();
            }
            return "";
        }

        virtual bool SetLobbyData(CSteamID steamIDLobby, const char *pchKey, const char *pchValue) override {
            if (!pchKey) return false;
            uint64_t lID = steamIDLobby.ConvertToUint64();
            LobbyInfo& lob = g_lobbies[lID];
            lob.id = lID;
            if (pchValue) lob.data[pchKey] = pchValue;
            else lob.data.erase(pchKey);

            LobbyDataUpdate_t resp = {};
            resp.m_ulSteamIDLobby = lID;
            resp.m_ulSteamIDMember = lID;
            resp.m_bSuccess = 1;
            PostCallback(LobbyDataUpdate_t::k_iCallback, &resp, sizeof(resp));

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
            if (!pubData || cubData == 0) return false;
            std::vector<uint8_t> payload(4 + cubData);
            *(int*)payload.data() = nChannel;
            memcpy(payload.data() + 4, pubData, cubData);
            BroadcastNetPacket(5, payload.data(), payload.size());
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
                if (pubDest) memcpy(pubDest, pkt.data.data(), copyLen);
                if (pcubMsgSize) *pcubMsgSize = (uint32)copyLen;
                if (psteamIDRemote) *psteamIDRemote = CSteamID(pkt.senderID);
                return true;
            }
            return false;
        }

        virtual bool AcceptP2PSessionWithUser(CSteamID steamIDRemote) override { return true; }
        virtual bool CloseP2PSessionWithUser(CSteamID steamIDRemote) override { return true; }
        virtual bool CloseP2PChannelWithUser(CSteamID steamIDRemote, int nChannel) override { return true; }

        virtual bool GetP2PSessionState(CSteamID steamIDRemote, P2PSessionState_t *pConnectionState) override {
            if (pConnectionState) {
                pConnectionState->m_bConnectionActive = 1;
                pConnectionState->m_bConnecting = 0;
                pConnectionState->m_eP2PSessionError = 0;
                pConnectionState->m_bUsingRelay = 0;
                pConnectionState->m_nBytesQueuedForSend = 0;
                pConnectionState->m_nPacketsQueuedForSend = 0;
                pConnectionState->m_nRemoteIP = 0x7F000001;
                pConnectionState->m_nRemotePort = 7777;
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
        virtual HSteamListenSocket CreateListenSocketIP(const SteamNetworkingIPAddr &localAddress, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }
        virtual HSteamNetConnection ConnectByIPAddress(const SteamNetworkingIPAddr &address, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }
        virtual HSteamListenSocket CreateListenSocketP2P(int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }
        virtual HSteamNetConnection ConnectP2P(const SteamNetworkingIdentity &identityRemote, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }
        virtual EResult AcceptConnection(HSteamNetConnection hConn) override { return k_EResultOK; }
        virtual bool CloseConnection(HSteamNetConnection hPeer, int nReason, const char *pszDebug, bool bEnableLinger) override { return true; }
        virtual bool CloseListenSocket(HSteamListenSocket hSocket) override { return true; }
        virtual bool SetConnectionUserData(HSteamNetConnection hPeer, int64 nUserData) override { return true; }
        virtual int64 GetConnectionUserData(HSteamNetConnection hPeer) override { return 0; }
        virtual void SetConnectionName(HSteamNetConnection hPeer, const char *pszName) override {}
        virtual bool GetConnectionName(HSteamNetConnection hPeer, char *pszName, int nMaxLen) override {
            if (pszName && nMaxLen > 0) { strcpy_s(pszName, nMaxLen, "ReFixConn"); return true; }
            return false;
        }
        virtual EResult SendMessageToConnection(HSteamNetConnection hConn, const void *pData, uint32 cbData, int nSendFlags, int64 *pOutMessageNumber) override {
            if (pOutMessageNumber) *pOutMessageNumber = 1;
            return k_EResultOK;
        }
        virtual void SendMessages(int nMessages, SteamNetworkingMessage_t *const *pMessages, int64 *pOutMessageNumberOrResult) override {
            if (pOutMessageNumberOrResult) {
                for (int i = 0; i < nMessages; i++) pOutMessageNumberOrResult[i] = 1;
            }
        }
        virtual EResult FlushMessagesOnConnection(HSteamNetConnection hConn) override { return k_EResultOK; }
        virtual int ReceiveMessagesOnConnection(HSteamNetConnection hConn, SteamNetworkingMessage_t **ppOutMessages, int nMaxMessages) override { return 0; }
        virtual HSteamNetPollGroup CreatePollGroup() override { return 1; }
        virtual bool DestroyPollGroup(HSteamNetPollGroup hPollGroup) override { return true; }
        virtual bool SetConnectionPollGroup(HSteamNetConnection hConn, HSteamNetPollGroup hPollGroup) override { return true; }
        virtual int ReceiveMessagesOnPollGroup(HSteamNetPollGroup hPollGroup, SteamNetworkingMessage_t **ppOutMessages, int nMaxMessages) override { return 0; }
        virtual bool GetConnectionInfo(HSteamNetConnection hConn, SteamNetConnectionInfo_t *pInfo) override {
            if (pInfo) {
                memset(pInfo, 0, sizeof(SteamNetConnectionInfo_t));
                pInfo->m_eState = k_ESteamNetworkingConnectionState_Connected;
                pInfo->m_identityRemote.SetSteamID64(g_localSteamID);
                return true;
            }
            return false;
        }
        virtual EResult GetConnectionRealTimeStatus(HSteamNetConnection hConn, SteamNetConnectionRealTimeStatus_t *pStatus, int nLanes, SteamNetConnectionRealTimeLaneStatus_t *pLanes) override {
            if (pStatus) memset(pStatus, 0, sizeof(SteamNetConnectionRealTimeStatus_t));
            if (pLanes && nLanes > 0) memset(pLanes, 0, sizeof(SteamNetConnectionRealTimeLaneStatus_t) * nLanes);
            return k_EResultOK;
        }
        virtual int GetDetailedConnectionStatus(HSteamNetConnection hConn, char *pszBuf, int cbBuf) override { return 0; }
        virtual bool GetListenSocketAddress(HSteamListenSocket hSocket, SteamNetworkingIPAddr *address) override {
            if (address) {
                address->Clear();
                address->SetIPv4(0x7F000001, 7777);
                return true;
            }
            return false;
        }
        virtual bool CreateSocketPair(HSteamNetConnection *pOutConnection1, HSteamNetConnection *pOutConnection2, bool bUseNetworkLoopback, const SteamNetworkingIdentity *pIdentity1, const SteamNetworkingIdentity *pIdentity2) override {
            if (pOutConnection1) *pOutConnection1 = 1;
            if (pOutConnection2) *pOutConnection2 = 2;
            return true;
        }
        virtual EResult ConfigureConnectionLanes(HSteamNetConnection hConn, int nNumLanes, const int *pLanePriorities, const uint16 *pLaneWeights) override {
            return k_EResultOK;
        }
        virtual bool ReceivedRelayAuthTicket(const void *pvTicket, int cbTicket, SteamDatagramRelayAuthTicket *pOutParsedTicket) override { return true; }
        virtual int FindRelayAuthTicketForServer(const SteamNetworkingIdentity &identityGameServer, int nRemoteVirtualPort, SteamDatagramRelayAuthTicket *pOutParsedTicket) override { return 0; }
        virtual HSteamNetConnection ConnectToHostedDedicatedServer(const SteamNetworkingIdentity &identityTarget, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }
        virtual uint16 GetHostedDedicatedServerPort() override { return 7777; }
        virtual SteamNetworkingPOPID GetHostedDedicatedServerPOPID() override { return 0; }
        virtual EResult GetHostedDedicatedServerAddress(SteamDatagramHostedAddress *pRouting) override { return k_EResultOK; }
        virtual HSteamListenSocket CreateHostedDedicatedServerListenSocket(int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override { return 1; }
        virtual EResult GetGameCoordinatorServerLogin(SteamDatagramGameCoordinatorServerLogin *pLoginInfo, int *pcbSignedBlob, void *pBlob) override { return k_EResultOK; }
        virtual HSteamNetConnection ConnectP2PCustomSignaling(ISteamNetworkingConnectionSignaling *pSignaling, const SteamNetworkingIdentity *pPeerIdentity, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override {
            return 1;
        }
        virtual bool ReceivedP2PCustomSignal(const void *pMsg, int cbMsg, ISteamNetworkingSignalingRecvContext *pContext) override { return true; }
        virtual bool GetCertificateRequest(int *pcbBlob, void *pBlob, SteamNetworkingErrMsg &errMsg) override {
            if (pcbBlob) *pcbBlob = 0;
            return true;
        }
        virtual bool SetCertificate(const void *pCertificate, int cbCertificate, SteamNetworkingErrMsg &errMsg) override { return true; }
        virtual void ResetIdentity(const SteamNetworkingIdentity *pIdentity) override {}
        virtual bool GetIdentity(SteamNetworkingIdentity *pIdentity) override {
            if (pIdentity) { pIdentity->SetSteamID64(g_localSteamID); return true; }
            return false;
        }
        virtual ESteamNetworkingAvailability InitAuthentication() override { return k_ESteamNetworkingAvailability_Current; }
        virtual ESteamNetworkingAvailability GetAuthenticationStatus(SteamNetAuthenticationStatus_t *pDetails) override {
            if (pDetails) {
                memset(pDetails, 0, sizeof(SteamNetAuthenticationStatus_t));
                pDetails->m_eAvail = k_ESteamNetworkingAvailability_Current;
            }
            return k_ESteamNetworkingAvailability_Current;
        }
        virtual void RunCallbacks() override { UnrealSteamEmu::RunCallbacks(); }
        virtual bool BeginAsyncRequestFakeIP(int nNumPorts) override { return true; }
        virtual void GetFakeIP(int idxFirstPort, SteamNetworkingFakeIPResult_t *pInfo) override {
            if (pInfo) {
                pInfo->m_eResult = k_EResultOK;
                pInfo->m_identity.SetSteamID64(g_localSteamID);
                pInfo->m_unIP = 0x7F000001;
                pInfo->m_unPorts[0] = 7777;
            }
        }
        virtual HSteamListenSocket CreateListenSocketP2PFakeIP(int idxFakePort, int nOptions, const SteamNetworkingConfigValue_t *pOptions) override { return 1; }
        virtual EResult GetRemoteFakeIPForConnection(HSteamNetConnection hConn, SteamNetworkingIPAddr *pOutAddr) override {
            if (pOutAddr) {
                pOutAddr->Clear();
                pOutAddr->SetIPv4(0x7F000001, 7777);
                return k_EResultOK;
            }
            return k_EResultInvalidParam;
        }
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
                msg->m_pfnFreeData = [](SteamNetworkingMessage_t *pMsg) { free(pMsg); };
            }
            return msg;
        }
        virtual ESteamNetworkingAvailability GetRelayNetworkStatus(SteamRelayNetworkStatus_t *pDetails) override {
            if (pDetails) {
                memset(pDetails, 0, sizeof(SteamRelayNetworkStatus_t));
                pDetails->m_eAvail = k_ESteamNetworkingAvailability_Current;
            }
            return k_ESteamNetworkingAvailability_Current;
        }
        virtual float GetLocalPingLocation(SteamNetworkPingLocation_t &result) override {
            memset(&result, 0, sizeof(result));
            return 0.0f;
        }
        virtual int EstimatePingTimeBetweenTwoLocations(const SteamNetworkPingLocation_t &location1, const SteamNetworkPingLocation_t &location2) override {
            return 5;
        }
        virtual int EstimatePingTimeFromLocalHost(const SteamNetworkPingLocation_t &remoteLocation) override {
            return 5;
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
            return 5;
        }
        virtual int GetDirectPingToPOP(SteamNetworkingPOPID popID) override { return 5; }
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
            if (buf && cbBuf > 0) snprintf(buf, cbBuf, "127.0.0.1:%u", addr.m_port);
        }
        virtual bool SteamNetworkingIPAddr_ParseString(SteamNetworkingIPAddr *pAddr, const char *pszStr) override {
            if (pAddr) {
                pAddr->Clear();
                pAddr->SetIPv4(0x7F000001, 7777);
            }
            return true;
        }
        virtual ESteamNetworkingFakeIPType SteamNetworkingIPAddr_GetFakeIPType(const SteamNetworkingIPAddr &addr) override {
            return k_ESteamNetworkingFakeIPType_NotFake;
        }
        virtual void SteamNetworkingIdentity_ToString(const SteamNetworkingIdentity &identity, char *buf, size_t cbBuf) override {
            if (buf && cbBuf > 0) snprintf(buf, cbBuf, "steamid:%llu", (unsigned long long)g_localSteamID);
        }
        virtual bool SteamNetworkingIdentity_ParseString(SteamNetworkingIdentity *pIdentity, const char *pszStr) override {
            if (pIdentity) {
                pIdentity->SetSteamID64(g_localSteamID);
            }
            return true;
        }
    };
    static CSteamNetworkingUtilsEmu g_steamNetworkingUtilsInstance;

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
            return &g_steamUserInstance;
        }
        virtual ISteamGameServer *GetISteamGameServer(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
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
        if (!pchVersion) return nullptr;

        ReFixLog("[UnrealSteam] Resolving Interface Version: '%s'", pchVersion);

        if (strstr(pchVersion, "SteamClient") || strstr(pchVersion, "STEAMCLIENT"))
            return &g_steamClientInstance;
        if (strstr(pchVersion, "SteamUser0") || strstr(pchVersion, "STEAMUSER"))
            return &g_steamUserInstance;
        if (strstr(pchVersion, "SteamFriends") || strstr(pchVersion, "STEAMFRIENDS"))
            return &g_steamFriendsInstance;
        if (strstr(pchVersion, "SteamUtils") || strstr(pchVersion, "STEAMUTILS"))
            return &g_steamUtilsInstance;
        if (strstr(pchVersion, "SteamMatchMakingServers") || strstr(pchVersion, "STEAMMATCHMAKINGSERVERS"))
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
        if (strstr(pchVersion, "SteamNetworking") || strstr(pchVersion, "STEAMNETWORKING"))
            return &g_steamNetworkingInstance;
        if (strstr(pchVersion, "STEAMREMOTESTORAGE") || strstr(pchVersion, "SteamRemoteStorage"))
            return &g_steamRemoteStorageInstance;
        if (strstr(pchVersion, "STEAMUGC") || strstr(pchVersion, "SteamUGC"))
            return &g_steamUGCInstance;
        if (strstr(pchVersion, "SteamGameServerStats") || strstr(pchVersion, "STEAMGAMESERVERSTATS"))
            return &g_steamGameServerStatsInstance;
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

        ReFixLog("[UnrealSteam] Warning: Unrecognized interface '%s', falling back to Client", pchVersion);
        return &g_steamClientInstance;
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
        static std::recursive_mutex ctxLock;
        std::lock_guard<std::recursive_mutex> lock(ctxLock);

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
    void* GetSteamUser() { return &g_steamUserInstance; }
    void* GetSteamFriends() { return &g_steamFriendsInstance; }
    void* GetSteamUtils() { return &g_steamUtilsInstance; }
    void* GetSteamMatchmaking() { return &g_steamMatchmakingInstance; }
    void* GetSteamMatchmakingServers() { return &g_steamMatchmakingServersInstance; }
    void* GetSteamUserStats() { return &g_steamUserStatsInstance; }
    void* GetSteamApps() { return &g_steamAppsInstance; }
    void* GetSteamNetworking() { return &g_steamNetworkingInstance; }
    void* GetSteamNetworkingSockets() { return &g_steamNetworkingSocketsInstance; }
    void* GetSteamNetworkingUtils() { return &g_steamNetworkingUtilsInstance; }
    void* GetSteamRemoteStorage() { return &g_steamRemoteStorageInstance; }
    void* GetSteamUGC() { return &g_steamUGCInstance; }
    void* GetSteamGameServer() { return &g_steamGameServerInstance; }
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
        std::lock_guard<std::recursive_mutex> lock(g_emuMutex);
        if (g_bInitialized) return true;

        LoadConfig();
        InitSockets();

        g_bInitialized = true;
        g_contextCounter.fetch_add(1);

        // Queue SteamServersConnected_t on startup
        SteamServersConnected_t conn = {};
        PostCallback(SteamServersConnected_t::k_iCallback, &conn, sizeof(conn), 0.01);

        ReFixLog("=================================================================");
        ReFixLog("  Re:Goldberg for Unreal Engine Initialized Successfully");
        ReFixLog("  Persona Name: '%s' | SteamID64: %llu | AppID: %u", g_personaName.c_str(), g_localSteamID, g_appID);
        ReFixLog("  UDP Listen Port: %u | Language: '%s'", g_listenPort, g_language.c_str());
        ReFixLog("=================================================================");

        return true;
    }

    void Shutdown() {
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
}
