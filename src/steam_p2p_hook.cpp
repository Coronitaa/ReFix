// =============================================================================
// ReFix - Steam P2P Winsock Redirect Layer (Implementation)
// =============================================================================
// Hooks ws2_32.dll sendto/recvfrom/connect/select using MinHook (inline hooks).
// Packets destined for known peer IPs are forwarded through ISteamNetworking
// P2P API instead of raw UDP — achieving transparent NAT traversal via
// Valve's Steam Datagram Relay (SDR) network.
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <deque>

#include "steam_p2p_hook.h"
#include "minhook/MinHook.h"

// =============================================================================
// ISteamNetworking vtable layout (ISteamNetworking008)
// https://partner.steamgames.com/doc/api/ISteamNetworking
// We resolve the interface pointer from SteamAPI and call methods by vtable
// index to avoid needing the actual Steam SDK headers at compile time.
// =============================================================================
// vtable indices for ISteamNetworking (confirmed for steamworks v1.55+):
//  0 = SendP2PPacket
//  1 = IsP2PPacketAvailable
//  2 = ReadP2PPacket
//  3 = AcceptP2PSessionWithUser
//  4 = CloseP2PSessionWithUser
//  5 = CloseP2PChannelWithUser
//  6 = GetP2PSessionState
//  7 = AllowP2PPacketRelay

enum EP2PSend {
    k_EP2PSendUnreliable              = 0,
    k_EP2PSendUnreliableNoDelay       = 1,
    k_EP2PSendReliable                = 2,
    k_EP2PSendReliableWithBuffering   = 3,
};

// Forward-declare the vtable caller helpers below.
static bool ISteamNetworking_SendP2PPacket(void* self, uint64_t steamIDRemote,
    const void* pubData, uint32_t cubData, EP2PSend eP2PSendType, int nChannel);
static bool ISteamNetworking_IsP2PPacketAvailable(void* self, uint32_t* pcubMsgSize, int nChannel);
static bool ISteamNetworking_ReadP2PPacket(void* self, void* pubDest, uint32_t cubDest,
    uint32_t* pcubMsgSize, uint64_t* psteamIDRemote, int nChannel);
static bool ISteamNetworking_AcceptP2PSessionWithUser(void* self, uint64_t steamIDRemote);

// =============================================================================
// Forward declarations for original Winsock functions
// =============================================================================
typedef int (WSAAPI* fn_sendto_t)(SOCKET s, const char* buf, int len, int flags,
    const struct sockaddr* to, int tolen);
typedef int (WSAAPI* fn_recvfrom_t)(SOCKET s, char* buf, int len, int flags,
    struct sockaddr* from, int* fromlen);
typedef int (WSAAPI* fn_connect_t)(SOCKET s, const struct sockaddr* name, int namelen);
typedef int (WSAAPI* fn_select_t)(int nfds, fd_set* readfds, fd_set* writefds,
    fd_set* exceptfds, const struct timeval* timeout);
typedef SOCKET(WSAAPI* fn_socket_t)(int af, int type, int protocol);

static fn_sendto_t   g_orig_sendto   = nullptr;
static fn_recvfrom_t g_orig_recvfrom = nullptr;
static fn_connect_t  g_orig_connect  = nullptr;
static fn_select_t   g_orig_select   = nullptr;
static fn_socket_t   g_orig_socket   = nullptr;

// =============================================================================
// Global state
// =============================================================================
static void*  g_hSteamOriginal  = nullptr;   // steam_api64_valve.dll handle
static void*  g_pSteamNetworking = nullptr;  // ISteamNetworking interface ptr
static bool   g_hooksInstalled  = false;
static std::atomic<bool> g_pumpRunning{ false };
static HANDLE g_pumpThread      = nullptr;

// P2P channel used for all game traffic
static const int k_nChannel = 0;

// Maximum size of a single P2P packet we support (UE typically < 4096)
static const uint32_t k_maxPacketSize = 8192;

// Lock protecting the IP<->SteamID maps
static std::mutex g_peerMutex;

// IP (host byte order) -> SteamID
static std::unordered_map<uint32_t, uint64_t> g_ipToSteamID;
// SteamID -> IP (host byte order)  [for injecting source in recvfrom]
static std::unordered_map<uint64_t, uint32_t> g_steamIDToIP;

// ---------------------------------------------------------------------------
// Received packet ring buffer (pump thread -> hooked recvfrom)
// ---------------------------------------------------------------------------
struct RecvPacket {
    std::vector<uint8_t> data;
    uint64_t             fromSteamID;
};

static std::mutex           g_recvMutex;
static std::deque<RecvPacket> g_recvQueue;
static const size_t         k_maxRecvQueue = 4096;

// =============================================================================
// Logging
// =============================================================================
// Forward-declared — ReFixLog is defined in steam_proxy.cpp (same DLL).
extern void ReFixLog(const char* fmt, ...);

void SteamP2PHook::Log(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ReFixLog("[P2PHook] %s", buf);
}

// =============================================================================
// ISteamNetworking vtable helpers  (no SDK headers needed)
// =============================================================================
static bool ISteamNetworking_SendP2PPacket(void* self, uint64_t steamIDRemote,
    const void* pubData, uint32_t cubData, EP2PSend eP2PSendType, int nChannel)
{
    if (!self) return false;
    // vtable index 0
    using fn_t = bool(__thiscall*)(void*, uint64_t, const void*, uint32_t, EP2PSend, int);
    void** vt = *(void***)self;
    auto fn = (fn_t)vt[0];
    return fn(self, steamIDRemote, pubData, cubData, eP2PSendType, nChannel);
}

static bool ISteamNetworking_IsP2PPacketAvailable(void* self, uint32_t* pcubMsgSize, int nChannel)
{
    if (!self) return false;
    using fn_t = bool(__thiscall*)(void*, uint32_t*, int);
    void** vt = *(void***)self;
    auto fn = (fn_t)vt[1];
    return fn(self, pcubMsgSize, nChannel);
}

static bool ISteamNetworking_ReadP2PPacket(void* self, void* pubDest, uint32_t cubDest,
    uint32_t* pcubMsgSize, uint64_t* psteamIDRemote, int nChannel)
{
    if (!self) return false;
    using fn_t = bool(__thiscall*)(void*, void*, uint32_t, uint32_t*, uint64_t*, int);
    void** vt = *(void***)self;
    auto fn = (fn_t)vt[2];
    return fn(self, pubDest, cubDest, pcubMsgSize, psteamIDRemote, nChannel);
}

static bool ISteamNetworking_AcceptP2PSessionWithUser(void* self, uint64_t steamIDRemote)
{
    if (!self) return false;
    using fn_t = bool(__thiscall*)(void*, uint64_t);
    void** vt = *(void***)self;
    auto fn = (fn_t)vt[3];
    return fn(self, steamIDRemote);
}

// =============================================================================
// Resolve ISteamNetworking pointer from the real Steam dll
// We call SteamAPI_ISteamNetworking_v006 (or newer) exported by the real dll.
// =============================================================================
typedef int (*fn_GetHSteamUser_t)();
typedef int (*fn_GetHSteamPipe_t)();
typedef void* (*fn_SteamNetworking_Pipe_t)(int hSteamUser, int hSteamPipe);
typedef void* (*fn_SteamNetworking_NoArg_t)();

static void* ResolveSteamNetworking() {
    HMODULE hSteam = (HMODULE)g_hSteamOriginal;
    if (!hSteam) hSteam = GetModuleHandleA("steam_api64_valve.dll");
    if (!hSteam) hSteam = GetModuleHandleA("steam_api64_o.dll");
    if (!hSteam) hSteam = GetModuleHandleA("steam_api64.dll.valve");
    if (!hSteam) return nullptr;

    // --- Candidate 1: Direct 0-arg / 2-arg accessors (post SteamAPI_Init) ---
    const char* candidates[] = {
        "SteamAPI_SteamNetworking_v006",
        "SteamAPI_SteamNetworking_v005",
        "SteamAPI_ISteamNetworking_v006",
        "SteamAPI_ISteamNetworking_v005",
        "SteamAPI_SteamNetworking",
        "SteamNetworking",
        nullptr
    };

    auto pfnUser = (fn_GetHSteamUser_t)GetProcAddress(hSteam, "SteamAPI_GetHSteamUser");
    if (!pfnUser) pfnUser = (fn_GetHSteamUser_t)GetProcAddress(hSteam, "GetHSteamUser");
    auto pfnPipe = (fn_GetHSteamPipe_t)GetProcAddress(hSteam, "SteamAPI_GetHSteamPipe");
    if (!pfnPipe) pfnPipe = (fn_GetHSteamPipe_t)GetProcAddress(hSteam, "GetHSteamPipe");
    int user = pfnUser ? pfnUser() : 0;
    int pipe = pfnPipe ? pfnPipe() : 0;

    for (int i = 0; candidates[i]; ++i) {
        FARPROC pProc = GetProcAddress(hSteam, candidates[i]);
        if (!pProc) continue;

        // Try 0-arg first (works post-SteamAPI_Init)
        auto fnNoArg = (fn_SteamNetworking_NoArg_t)pProc;
        void* ptr = fnNoArg();
        if (ptr) {
            SteamP2PHook::Log("ISteamNetworking resolved via '%s()' -> %p", candidates[i], ptr);
            return ptr;
        }

        // Try pipe-based call
        if (user && pipe) {
            auto fnPipe = (fn_SteamNetworking_Pipe_t)pProc;
            ptr = fnPipe(user, pipe);
            if (ptr) {
                SteamP2PHook::Log("ISteamNetworking resolved via '%s(%d, %d)' -> %p", candidates[i], user, pipe, ptr);
                return ptr;
            }
        }
    }

    // --- Candidate 2: Via ISteamClient::GetISteamNetworking (most reliable post-Init path) ---
    if (user && pipe) {
        // Try SteamAPI_ISteamClient_GetISteamNetworking(hSteamUser, hSteamPipe, pszVersion)
        typedef void* (*fn_ClientGetNetworking_t)(int, int, const char*);
        auto pfnClientNet = (fn_ClientGetNetworking_t)GetProcAddress(hSteam, "SteamAPI_ISteamClient_GetISteamNetworking");
        if (pfnClientNet) {
            const char* versions[] = { "SteamNetworking006", "SteamNetworking005", nullptr };
            for (int v = 0; versions[v]; ++v) {
                // Need ISteamClient ptr — get via SteamClient() or CreateInterface
                typedef void* (*fn_SteamClient_t)();
                auto pfnSteamClient = (fn_SteamClient_t)GetProcAddress(hSteam, "SteamClient");
                if (!pfnSteamClient) pfnSteamClient = (fn_SteamClient_t)GetProcAddress(hSteam, "SteamAPI_SteamClient");
                void* pClient = pfnSteamClient ? pfnSteamClient() : nullptr;
                if (pClient) {
                    // ISteamClient::GetISteamNetworking(hSteamUser, hSteamPipe, pszVersion)
                    typedef void* (__thiscall* fn_GetISteamNetworking_t)(void*, int, int, const char*);
                    void** vt = *(void***)pClient;
                    // vtable index 8 is GetISteamNetworking in ISteamClient017+
                    auto fn = (fn_GetISteamNetworking_t)vt[8];
                    void* ptr = fn(pClient, user, pipe, versions[v]);
                    if (ptr) {
                        SteamP2PHook::Log("ISteamNetworking resolved via ISteamClient::GetISteamNetworking('%s') -> %p", versions[v], ptr);
                        return ptr;
                    }
                }
            }
        }
    }

    // --- Candidate 3: CreateInterface fallback ---
    typedef void* (*fn_CreateInterface_t)(const char* pName, int* pReturnCode);
    auto pfnCreate = (fn_CreateInterface_t)GetProcAddress(hSteam, "CreateInterface");
    if (pfnCreate) {
        const char* ifaceNames[] = {
            "SteamNetworking006",
            "SteamNetworking005",
            "SteamNetworking004",
            "SteamNetworking003",
            nullptr
        };
        for (int i = 0; ifaceNames[i]; ++i) {
            void* ptr = pfnCreate(ifaceNames[i], nullptr);
            if (ptr) {
                SteamP2PHook::Log("ISteamNetworking resolved via CreateInterface('%s') -> %p", ifaceNames[i], ptr);
                return ptr;
            }
        }
    }

    // Throttle warning to every 2s to avoid log spam
    static DWORD s_lastWarn = 0;
    DWORD now = GetTickCount();
    if (now - s_lastWarn > 2000) {
        s_lastWarn = now;
        SteamP2PHook::Log("WARNING: Could not resolve ISteamNetworking interface (user=%d, pipe=%d)", user, pipe);
    }
    return nullptr;
}

// =============================================================================
// Background pump thread — drains incoming P2P packets into g_recvQueue
// =============================================================================
static bool P2PPumpStep() {
    static uint8_t pktBuf[k_maxPacketSize];

    // Lazily resolve networking if not yet done
    if (!g_pSteamNetworking) {
        g_pSteamNetworking = ResolveSteamNetworking();
    }

    if (g_pSteamNetworking && g_pumpRunning.load(std::memory_order_relaxed)) {
        uint32_t pktSize = 0;
        while (g_pumpRunning.load(std::memory_order_relaxed) &&
               ISteamNetworking_IsP2PPacketAvailable(g_pSteamNetworking, &pktSize, k_nChannel) &&
               pktSize > 0)
        {
            if (pktSize > k_maxPacketSize) pktSize = k_maxPacketSize;

            uint64_t fromID = 0;
            uint32_t bytesRead = 0;
            bool ok = ISteamNetworking_ReadP2PPacket(g_pSteamNetworking,
                pktBuf, pktSize, &bytesRead, &fromID, k_nChannel);

            if (ok && bytesRead > 0 && fromID != 0) {
                // Accept P2P session automatically (required first time)
                ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, fromID);

                // Ensure this peer is in our map (remote-initiated join)
                {
                    std::lock_guard<std::mutex> lg(g_peerMutex);
                    if (g_steamIDToIP.find(fromID) == g_steamIDToIP.end()) {
                        uint32_t syntheticIP = 0x7F000001u | (uint32_t)(fromID & 0x00FFFFFFu);
                        g_steamIDToIP[fromID]   = syntheticIP;
                        g_ipToSteamID[syntheticIP] = fromID;
                    }
                }

                {
                    std::lock_guard<std::mutex> lg(g_recvMutex);
                    if (g_recvQueue.size() < k_maxRecvQueue) {
                        RecvPacket pkt;
                        pkt.data.assign(pktBuf, pktBuf + bytesRead);
                        pkt.fromSteamID = fromID;
                        g_recvQueue.push_back(std::move(pkt));
                    }
                }
            }
        }
    }
    return true;
}

static DWORD WINAPI P2PPumpThread(LPVOID) {
    SteamP2PHook::Log("P2P pump thread started");

    while (g_pumpRunning.load(std::memory_order_relaxed)) {
        __try {
            P2PPumpStep();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            break;
        }

        Sleep(1); // 1ms polling — very low CPU overhead
    }

    SteamP2PHook::Log("P2P pump thread stopped");
    return 0;
}

// =============================================================================
// Hooked Winsock functions
// =============================================================================

// ------ sendto ---------------------------------------------------------------
static int WSAAPI Hook_sendto(SOCKET s, const char* buf, int len, int flags,
    const struct sockaddr* to, int tolen)
{
    // Only intercept IPv4 UDP to known peer IPs
    if (to && to->sa_family == AF_INET && len > 0) {
        const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(to);
        uint32_t destIP = ntohl(sin->sin_addr.s_addr);
        uint16_t destPort = ntohs(sin->sin_port);

        uint64_t steamID = 0;
        {
            std::lock_guard<std::mutex> lg(g_peerMutex);
            auto it = g_ipToSteamID.find(destIP);
            if (it != g_ipToSteamID.end()) {
                steamID = it->second;
            } else if ((destPort == 7777 || destPort == 7778 || destPort == 27015 || destIP == 0x7F000001u || destIP == 0) && g_ipToSteamID.size() == 1) {
                // If game connects to server port or loopback and we have 1 known peer (the host), redirect!
                steamID = g_ipToSteamID.begin()->second;
            }
        }

        if (steamID != 0 && g_pSteamNetworking) {
            // Redirect through Steam P2P
            bool ok = ISteamNetworking_SendP2PPacket(g_pSteamNetworking, steamID,
                buf, (uint32_t)len, k_EP2PSendUnreliable, k_nChannel);
            return ok ? len : SOCKET_ERROR;
        }
    }

    // Fall through to real Winsock for all other traffic
    return g_orig_sendto(s, buf, len, flags, to, tolen);
}

// ------ recvfrom -------------------------------------------------------------
static int WSAAPI Hook_recvfrom(SOCKET s, char* buf, int len, int flags,
    struct sockaddr* from, int* fromlen)
{
    // Check our P2P receive queue first
    {
        std::lock_guard<std::mutex> lg(g_recvMutex);
        if (!g_recvQueue.empty()) {
            RecvPacket& pkt = g_recvQueue.front();
            int copyLen = (int)pkt.data.size();
            if (copyLen > len) copyLen = len;
            memcpy(buf, pkt.data.data(), copyLen);

            if (from && fromlen && *fromlen >= (int)sizeof(struct sockaddr_in)) {
                struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(from);
                sin->sin_family = AF_INET;
                sin->sin_port   = htons(7777);

                uint32_t srcIP = 0;
                {
                    // look up IP for this SteamID (brief unlock not needed — already under recvMutex)
                    auto it = g_steamIDToIP.find(pkt.fromSteamID);
                    if (it != g_steamIDToIP.end()) srcIP = it->second;
                }
                sin->sin_addr.s_addr = htonl(srcIP);
                *fromlen = sizeof(struct sockaddr_in);
            }

            g_recvQueue.pop_front();
            return copyLen;
        }
    }

    // Fall through — check real socket
    return g_orig_recvfrom(s, buf, len, flags, from, fromlen);
}

// ------ connect (TCP or UDP connected-mode) -----------------------------------
static int WSAAPI Hook_connect(SOCKET s, const struct sockaddr* name, int namelen)
{
    // If connecting to a known peer IP, accept it silently on the P2P layer
    if (name && name->sa_family == AF_INET) {
        const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(name);
        uint32_t destIP = ntohl(sin->sin_addr.s_addr);

        uint64_t steamID = 0;
        {
            std::lock_guard<std::mutex> lg(g_peerMutex);
            auto it = g_ipToSteamID.find(destIP);
            if (it != g_ipToSteamID.end()) steamID = it->second;
        }

        if (steamID != 0 && g_pSteamNetworking) {
            ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, steamID);
            SteamP2PHook::Log("Hook_connect: P2P session accepted for SteamID=%llu", steamID);
            // Still call the real connect so the socket is bound/connected
        }
    }
    return g_orig_connect(s, name, namelen);
}

// ------ select (make P2P data visible to the game's readability check) --------
static int WSAAPI Hook_select(int nfds, fd_set* readfds, fd_set* writefds,
    fd_set* exceptfds, const struct timeval* timeout)
{
    fd_set inRead;
    bool hasInRead = false;
    if (readfds && readfds->fd_count > 0) {
        inRead = *readfds;
        hasInRead = true;
    }

    int result = g_orig_select(nfds, readfds, writefds, exceptfds, timeout);

    // If P2P data is queued, we restore ready sockets so Unreal/Unity network tick reads them
    {
        std::lock_guard<std::mutex> lg(g_recvMutex);
        if (!g_recvQueue.empty() && result <= 0) {
            if (hasInRead && readfds) {
                *readfds = inRead;
                return (int)readfds->fd_count;
            }
            return 1;
        }
    }
    return result;
}

// =============================================================================
// MinHook installation
// =============================================================================
namespace SteamP2PHook {

void Install(void* hSteamOriginal) {
    if (hSteamOriginal) g_hSteamOriginal = hSteamOriginal;
    if (g_hooksInstalled) return;

    Log("Installing Winsock P2P hooks via MinHook...");

    if (MH_Initialize() != MH_OK) {
        Log("ERROR: MH_Initialize failed");
        return;
    }

    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) hWs2 = LoadLibraryA("ws2_32.dll");
    if (!hWs2) {
        Log("ERROR: Could not load ws2_32.dll");
        return;
    }

    // Hook sendto
    {
        void* target = GetProcAddress(hWs2, "sendto");
        if (MH_CreateHook(target, (void*)Hook_sendto, (void**)&g_orig_sendto) == MH_OK)
            MH_EnableHook(target);
        Log("sendto hook: %s", g_orig_sendto ? "OK" : "FAIL");
    }

    // Hook recvfrom
    {
        void* target = GetProcAddress(hWs2, "recvfrom");
        if (MH_CreateHook(target, (void*)Hook_recvfrom, (void**)&g_orig_recvfrom) == MH_OK)
            MH_EnableHook(target);
        Log("recvfrom hook: %s", g_orig_recvfrom ? "OK" : "FAIL");
    }

    // Hook connect
    {
        void* target = GetProcAddress(hWs2, "connect");
        if (MH_CreateHook(target, (void*)Hook_connect, (void**)&g_orig_connect) == MH_OK)
            MH_EnableHook(target);
        Log("connect hook: %s", g_orig_connect ? "OK" : "FAIL");
    }

    // Hook select
    {
        void* target = GetProcAddress(hWs2, "select");
        if (MH_CreateHook(target, (void*)Hook_select, (void**)&g_orig_select) == MH_OK)
            MH_EnableHook(target);
        Log("select hook: %s", g_orig_select ? "OK" : "FAIL");
    }

    g_hooksInstalled = true;

    // Resolve ISteamNetworking immediately if possible, else pump thread will retry
    g_pSteamNetworking = ResolveSteamNetworking();

    // Start background pump thread
    g_pumpRunning.store(true, std::memory_order_relaxed);
    g_pumpThread = CreateThread(NULL, 0, P2PPumpThread, NULL, 0, NULL);

    Log("Winsock P2P hooks installed. ISteamNetworking=%p", g_pSteamNetworking);
}

void Uninstall() {
    if (!g_hooksInstalled) return;

    // Signal pump thread to stop
    g_pumpRunning.store(false, std::memory_order_relaxed);

    if (g_pumpThread) {
        WaitForSingleObject(g_pumpThread, 100);
        CloseHandle(g_pumpThread);
        g_pumpThread = nullptr;
    }

    g_pSteamNetworking = nullptr;
    g_hooksInstalled = false;
}

void RegisterPeer(uint64_t steamID, uint32_t ipv4_host) {
    if (!steamID || !ipv4_host) return;

    std::lock_guard<std::mutex> lg(g_peerMutex);

    // Remove old IP mapping for this SteamID if it changed
    auto itOld = g_steamIDToIP.find(steamID);
    if (itOld != g_steamIDToIP.end()) {
        g_ipToSteamID.erase(itOld->second);
    }

    g_steamIDToIP[steamID]  = ipv4_host;
    g_ipToSteamID[ipv4_host] = steamID;

    // Immediately accept any incoming P2P session from this peer
    if (g_pSteamNetworking) {
        ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, steamID);
    }

    char ipStr[32];
    uint32_t n = htonl(ipv4_host);
    inet_ntop(AF_INET, &n, ipStr, sizeof(ipStr));
    Log("RegisterPeer: SteamID=%llu <-> IP=%s", steamID, ipStr);
}

void UnregisterPeer(uint64_t steamID) {
    std::lock_guard<std::mutex> lg(g_peerMutex);
    auto it = g_steamIDToIP.find(steamID);
    if (it != g_steamIDToIP.end()) {
        g_ipToSteamID.erase(it->second);
        g_steamIDToIP.erase(it);
        Log("UnregisterPeer: SteamID=%llu removed", steamID);
    }
}

} // namespace SteamP2PHook

// =============================================================================
// Force-resolve ISteamNetworking immediately post SteamAPI_Init.
// Called from steam_proxy.cpp right after the real SteamAPI_Init() returns true.
// At that point, HUser and HPipe handles are valid and the interface resolves.
// =============================================================================
extern "C" void SteamP2PHook_ForceResolve() {
    // Clear any stale null so ResolveSteamNetworking() runs the full lookup
    g_pSteamNetworking = nullptr;
    g_pSteamNetworking = ResolveSteamNetworking();
    if (g_pSteamNetworking) {
        SteamP2PHook::Log("[ForceResolve] ISteamNetworking obtained post-Init -> %p", g_pSteamNetworking);
        // Allow relay packets through Steam SDR (Valve relay network)
        using fn_AllowRelay_t = bool(__thiscall*)(void*, bool);
        void** vt = *(void***)g_pSteamNetworking;
        auto fnRelay = (fn_AllowRelay_t)vt[7]; // vtable index 7 = AllowP2PPacketRelay
        fnRelay(g_pSteamNetworking, true);
        SteamP2PHook::Log("[ForceResolve] AllowP2PPacketRelay(true) called - SDR relay enabled");
    } else {
        SteamP2PHook::Log("[ForceResolve] WARNING: ISteamNetworking still not resolved post-Init");
    }
}
