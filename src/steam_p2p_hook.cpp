// =============================================================================
// ReFix - Steam P2P Winsock Redirect Layer (Hardened Implementation)
// =============================================================================
// Hooks ws2_32.dll sendto / recvfrom / connect / select / bind / closesocket
// using MinHook (inline hooks).
//
// Packets destined for known peer IPs are forwarded through ISteamNetworking
// P2P API instead of raw UDP — achieving transparent NAT traversal via
// Valve's Steam Datagram Relay (SDR) network.
//
// Hardened against Red Team vulnerabilities:
//   - V-01: Multi-socket context table (std::unordered_map<SOCKET, SocketContext>)
//           separating Game (ports 7777 / 27015 / 5055) and Voice (port 5058).
//   - V-02: Pre-Lobby Hold Buffer with 3000 ms TTL for Photon Fusion & nanosockets.
//   - V-04: Port Virtualization on WSAEADDRINUSE (10048) in Hook_bind.
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
#include <string>

#include "steam_p2p_hook.h"
#include "minhook/MinHook.h"
#include "network/network_mode.h"

// Helper to determine if an IP is local/LAN
#include "lan_core/refix_lan_firewall.h"

// Helper to determine if an IP is local/LAN
static bool IsAllowedLanEndpoint(const sockaddr* addr) {
    if (!addr) return false;
    return refix::lan::LanFirewall::Get().IsAllowedSockaddr(addr);
}

// =============================================================================
// ISteamNetworking vtable layout (ISteamNetworking008)
// =============================================================================
enum EP2PSend {
    k_EP2PSendUnreliable              = 0,
    k_EP2PSendUnreliableNoDelay       = 1,
    k_EP2PSendReliable                = 2,
    k_EP2PSendReliableWithBuffering   = 3,
};

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
typedef int (WSAAPI* fn_bind_t)(SOCKET s, const struct sockaddr* name, int namelen);
typedef int (WSAAPI* fn_closesocket_t)(SOCKET s);

static fn_sendto_t      g_orig_sendto      = nullptr;
static fn_recvfrom_t    g_orig_recvfrom    = nullptr;
static fn_connect_t     g_orig_connect     = nullptr;
static fn_select_t      g_orig_select      = nullptr;
static fn_bind_t        g_orig_bind        = nullptr;
static fn_closesocket_t g_orig_closesocket = nullptr;

extern "C" void ReFix_OnGamePortVirtualized(uint16_t newPort);

// =============================================================================
// Service and Port Classification (V-01 Hardening)
// =============================================================================
enum class SocketServiceType : uint8_t {
    Unknown = 0,
    Game    = 1,
    Voice   = 2
};

static inline bool IsVoicePort(uint16_t port) {
    return (port == 5058);
}

static inline bool IsGamePort(uint16_t port) {
    static uint16_t s_iniPort = 0;
    if (s_iniPort == 0) {
        char buf[MAX_PATH] = { 0 };
        GetModuleFileNameA(NULL, buf, MAX_PATH);
        std::string p(buf);
        size_t pos = p.find_last_of("\\/");
        std::string ini = (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix.ini";
        s_iniPort = (uint16_t)GetPrivateProfileIntA("P2P", "P2PPort", 7777, ini.c_str());
    }
    if (port == s_iniPort) return true;
    if (port >= 7770 && port <= 7799) return true;
    if (port >= 27015 && port <= 27035) return true;
    if (port == 5055 || port == 5056) return true;
    return false;
}

static inline SocketServiceType ClassifyPort(uint16_t port) {
    if (IsVoicePort(port)) return SocketServiceType::Voice;
    if (IsGamePort(port)) return SocketServiceType::Game;
    return SocketServiceType::Unknown;
}

// =============================================================================
// Wire Protocol & Channels
// =============================================================================
// Channels in Steam P2P (ISteamNetworking)
static const int k_nChannelGame  = 0;
static const int k_nChannelVoice = 1;

#pragma pack(push, 1)
struct ReFixP2PHeader {
    uint32_t magic;       // 0x58464552 'REFX'
    uint16_t srcPort;     // Sender local port
    uint16_t dstPort;     // Target remote port
    uint8_t  service;     // 1 = Game, 2 = Voice
    uint8_t  flags;       // reserved
    uint16_t payloadLen;  // byte length of following payload
};
#pragma pack(pop)

static const uint32_t k_refixP2PMagic = 0x58464552; // 'REFX'
static const uint32_t k_maxPacketSize = 16384;

// =============================================================================
// Data Structures for Received Packets & Sockets (V-01)
// =============================================================================
struct RecvPacket {
    std::vector<uint8_t> data;
    uint64_t             fromSteamID = 0;
    uint16_t             fromPort    = 0;
    uint16_t             destPort    = 0;
    SocketServiceType    service     = SocketServiceType::Game;
};

struct SocketContext {
    SOCKET            socket         = INVALID_SOCKET;
    SocketServiceType serviceType    = SocketServiceType::Unknown;
    uint16_t          localPort      = 0;
    uint16_t          virtualPort    = 0;
    uint16_t          lastRemotePort = 0;
    DWORD             lastActivity   = 0;
    std::deque<RecvPacket> recvQueue;
    static const size_t kMaxQueueSize = 2048;
};

// Global socket table and pending queues
static std::mutex g_socketMutex;
static std::unordered_map<SOCKET, SocketContext> g_socketContexts;
static std::deque<RecvPacket> g_pendingGamePackets;
static std::deque<RecvPacket> g_pendingVoicePackets;

// =============================================================================
// Pre-Lobby Hold Buffer (V-02 Hardening)
// =============================================================================
struct BufferedHoldPacket {
    SOCKET               s           = INVALID_SOCKET;
    std::vector<uint8_t> data;
    int                  flags       = 0;
    sockaddr_in          destAddr    = {};
    uint32_t             destIP      = 0; // host byte order
    uint16_t             destPort    = 0; // host byte order
    SocketServiceType    service     = SocketServiceType::Game;
    DWORD                timestampMs = 0; // GetTickCount()
};

static std::mutex g_holdBufferMutex;
static std::vector<BufferedHoldPacket> g_holdBuffer;
static const DWORD k_holdBufferTtlMs = 3000;   // 3000 ms TTL
static const size_t k_maxHoldBufferSize = 512; // cap buffered datagrams

// =============================================================================
// Peer Mappings & General State
// =============================================================================
static void*  g_hSteamOriginal   = nullptr;
static void*  g_pSteamNetworking = nullptr;
static bool   g_hooksInstalled   = false;
static std::atomic<bool> g_pumpRunning{ false };
static HANDLE g_pumpThread       = nullptr;
static std::atomic<uint16_t> g_virtualizedGamePort{ 0 };

static std::mutex g_peerMutex;
static std::unordered_map<uint32_t, uint64_t> g_ipToSteamID;
static std::unordered_map<uint64_t, uint32_t> g_steamIDToIP;
static std::unordered_map<uint64_t, uint16_t> g_steamIDToPort;

// Forward declaration
static void* ResolveSteamNetworking();
static bool SendP2PWithEnvelope(uint64_t steamID, SOCKET s, const uint8_t* payload,
    uint32_t payloadLen, uint16_t dstPort, SocketServiceType service);

// =============================================================================
// Logging
// =============================================================================
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
// ISteamNetworking vtable callers
// =============================================================================
static bool ISteamNetworking_SendP2PPacket(void* self, uint64_t steamIDRemote,
    const void* pubData, uint32_t cubData, EP2PSend eP2PSendType, int nChannel)
{
    if (!self) return false;
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
// Interface Resolution from Steam API
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

        auto fnNoArg = (fn_SteamNetworking_NoArg_t)pProc;
        void* ptr = fnNoArg();
        if (ptr) {
            SteamP2PHook::Log("ISteamNetworking resolved via '%s()' -> %p", candidates[i], ptr);
            return ptr;
        }

        if (user && pipe) {
            auto fnPipe = (fn_SteamNetworking_Pipe_t)pProc;
            ptr = fnPipe(user, pipe);
            if (ptr) {
                SteamP2PHook::Log("ISteamNetworking resolved via '%s(%d, %d)' -> %p", candidates[i], user, pipe, ptr);
                return ptr;
            }
        }
    }

    if (user && pipe) {
        typedef void* (*fn_ClientGetNetworking_t)(int, int, const char*);
        auto pfnClientNet = (fn_ClientGetNetworking_t)GetProcAddress(hSteam, "SteamAPI_ISteamClient_GetISteamNetworking");
        if (pfnClientNet) {
            const char* versions[] = { "SteamNetworking006", "SteamNetworking005", nullptr };
            for (int v = 0; versions[v]; ++v) {
                typedef void* (*fn_SteamClient_t)();
                auto pfnSteamClient = (fn_SteamClient_t)GetProcAddress(hSteam, "SteamClient");
                if (!pfnSteamClient) pfnSteamClient = (fn_SteamClient_t)GetProcAddress(hSteam, "SteamAPI_SteamClient");
                void* pClient = pfnSteamClient ? pfnSteamClient() : nullptr;
                if (pClient) {
                    typedef void* (__thiscall* fn_GetISteamNetworking_t)(void*, int, int, const char*);
                    void** vt = *(void***)pClient;
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

    static DWORD s_lastWarn = 0;
    DWORD now = GetTickCount();
    if (now - s_lastWarn > 2000) {
        s_lastWarn = now;
        SteamP2PHook::Log("WARNING: Could not resolve ISteamNetworking interface (user=%d, pipe=%d)", user, pipe);
    }
    return nullptr;
}

// =============================================================================
// Packet Dispatch & Queue Routing (V-01 Hardening)
// =============================================================================
static void RouteIncomingPacket(RecvPacket pkt) {
    std::lock_guard<std::mutex> lg(g_socketMutex);
    SOCKET targetSocket = INVALID_SOCKET;

    if (pkt.service == SocketServiceType::Voice) {
        // Look for a voice socket
        // 1. Exact match on localPort or lastRemotePort
        for (auto& kv : g_socketContexts) {
            if (kv.second.serviceType == SocketServiceType::Voice) {
                if (kv.second.localPort == pkt.destPort || kv.second.lastRemotePort == pkt.fromPort) {
                    targetSocket = kv.first;
                    break;
                }
            }
        }
        // 2. Any voice socket
        if (targetSocket == INVALID_SOCKET) {
            for (auto& kv : g_socketContexts) {
                if (kv.second.serviceType == SocketServiceType::Voice) {
                    targetSocket = kv.first;
                    break;
                }
            }
        }
        if (targetSocket != INVALID_SOCKET) {
            auto& ctx = g_socketContexts[targetSocket];
            if (ctx.recvQueue.size() < SocketContext::kMaxQueueSize) {
                ctx.recvQueue.push_back(std::move(pkt));
            }
            return;
        }
        // No voice socket registered yet -> queue in pending voice
        if (g_pendingVoicePackets.size() < 1024) {
            g_pendingVoicePackets.push_back(std::move(pkt));
        }
    } else {
        // Game service
        // 1. Exact match on localPort == destPort or virtualPort == destPort
        for (auto& kv : g_socketContexts) {
            if (kv.second.serviceType == SocketServiceType::Game) {
                if ((kv.second.localPort && kv.second.localPort == pkt.destPort) ||
                    (kv.second.virtualPort && kv.second.virtualPort == pkt.destPort)) {
                    targetSocket = kv.first;
                    break;
                }
            }
        }
        // 2. Match on lastRemotePort == fromPort
        if (targetSocket == INVALID_SOCKET) {
            for (auto& kv : g_socketContexts) {
                if (kv.second.serviceType == SocketServiceType::Game && kv.second.lastRemotePort == pkt.fromPort) {
                    targetSocket = kv.first;
                    break;
                }
            }
        }
        // 3. Any game socket
        if (targetSocket == INVALID_SOCKET) {
            for (auto& kv : g_socketContexts) {
                if (kv.second.serviceType == SocketServiceType::Game) {
                    targetSocket = kv.first;
                    break;
                }
            }
        }
        // 4. Any socket whose type is unknown but has bound to a game port
        if (targetSocket == INVALID_SOCKET) {
            for (auto& kv : g_socketContexts) {
                if (kv.second.serviceType == SocketServiceType::Unknown && IsGamePort(kv.second.localPort)) {
                    kv.second.serviceType = SocketServiceType::Game;
                    targetSocket = kv.first;
                    break;
                }
            }
        }
        if (targetSocket != INVALID_SOCKET) {
            auto& ctx = g_socketContexts[targetSocket];
            if (ctx.recvQueue.size() < SocketContext::kMaxQueueSize) {
                ctx.recvQueue.push_back(std::move(pkt));
            }
            return;
        }
        // No game socket ready yet -> queue in pending game
        if (g_pendingGamePackets.size() < 1024) {
            g_pendingGamePackets.push_back(std::move(pkt));
        }
    }
}

// =============================================================================
// Packet Transmission with Framing
// =============================================================================
static bool SendP2PWithEnvelope(uint64_t steamID, SOCKET s, const uint8_t* payload,
    uint32_t payloadLen, uint16_t dstPort, SocketServiceType service)
{
    if (!g_pSteamNetworking) {
        g_pSteamNetworking = ResolveSteamNetworking();
    }
    if (!g_pSteamNetworking) return false;

    // Determine local / source port
    uint16_t srcPort = 0;
    {
        std::lock_guard<std::mutex> lg(g_socketMutex);
        auto it = g_socketContexts.find(s);
        if (it != g_socketContexts.end()) {
            srcPort = it->second.virtualPort ? it->second.virtualPort : it->second.localPort;
        }
    }
    if (srcPort == 0) {
        sockaddr_in localAddr{};
        int localLen = sizeof(localAddr);
        if (s != INVALID_SOCKET && getsockname(s, (sockaddr*)&localAddr, &localLen) == 0) {
            srcPort = ntohs(localAddr.sin_port);
        }
        if (srcPort == 0) {
            srcPort = (service == SocketServiceType::Voice) ? 5058 : 7777;
        }
    }

    // Build framed packet buffer
    std::vector<uint8_t> wireBuffer(sizeof(ReFixP2PHeader) + payloadLen);
    ReFixP2PHeader* hdr = reinterpret_cast<ReFixP2PHeader*>(wireBuffer.data());
    hdr->magic = k_refixP2PMagic;
    hdr->srcPort = srcPort;
    hdr->dstPort = dstPort;
    hdr->service = (service == SocketServiceType::Voice) ? 2 : 1;
    hdr->flags = 0;
    hdr->payloadLen = (uint16_t)payloadLen;
    if (payload && payloadLen > 0) {
        memcpy(wireBuffer.data() + sizeof(ReFixP2PHeader), payload, payloadLen);
    }

    int channel = (service == SocketServiceType::Voice) ? k_nChannelVoice : k_nChannelGame;
    EP2PSend sendType = (service == SocketServiceType::Voice) ? k_EP2PSendUnreliableNoDelay : k_EP2PSendUnreliable;

    return ISteamNetworking_SendP2PPacket(g_pSteamNetworking, steamID,
        wireBuffer.data(), (uint32_t)wireBuffer.size(), sendType, channel);
}

// =============================================================================
// Pre-Lobby Hold Buffer Flush (V-02 Hardening)
// =============================================================================
void SteamP2PHook::FlushHoldBuffer() {
    std::lock_guard<std::mutex> lg(g_holdBufferMutex);
    if (g_holdBuffer.empty()) return;

    DWORD now = GetTickCount();
    auto it = g_holdBuffer.begin();
    while (it != g_holdBuffer.end()) {
        uint64_t targetSteamID = 0;
        {
            std::lock_guard<std::mutex> lgPeer(g_peerMutex);
            auto itPeer = g_ipToSteamID.find(it->destIP);
            if (itPeer != g_ipToSteamID.end()) {
                targetSteamID = itPeer->second;
            } else if (g_ipToSteamID.size() == 1) {
                targetSteamID = g_ipToSteamID.begin()->second;
            }
        }

        if (targetSteamID != 0) {
            SendP2PWithEnvelope(targetSteamID, it->s, it->data.data(), (uint32_t)it->data.size(), it->destPort, it->service);
            SteamP2PHook::Log("[HoldBuffer] Dispatched held packet (%zu bytes) to SteamID=%llu", it->data.size(), targetSteamID);
            it = g_holdBuffer.erase(it);
        } else if (now - it->timestampMs >= k_holdBufferTtlMs) {
            // TTL expired (3000 ms) -> fallback to raw Winsock so offline/LAN connections continue
            SteamP2PHook::Log("[HoldBuffer] TTL expired (%u ms) for packet destined to port %u -> releasing to raw Winsock",
                k_holdBufferTtlMs, it->destPort);
            if (g_orig_sendto && it->s != INVALID_SOCKET) {
                g_orig_sendto(it->s, (const char*)it->data.data(), (int)it->data.size(), it->flags,
                    (const sockaddr*)&it->destAddr, sizeof(it->destAddr));
            }
            it = g_holdBuffer.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// Background Pump Thread — Drains P2P packets into isolated queues
// =============================================================================
static bool P2PPumpStep() {
    static uint8_t pktBuf[k_maxPacketSize];

    if (!g_pSteamNetworking) {
        g_pSteamNetworking = ResolveSteamNetworking();
    }

    // Flush any newly routable or expired packets from Pre-Lobby Hold Buffer
    SteamP2PHook::FlushHoldBuffer();

    if (g_pSteamNetworking && g_pumpRunning.load(std::memory_order_relaxed)) {
        // Poll Game channel (0) and Voice channel (1) independently
        for (int ch : { k_nChannelGame, k_nChannelVoice }) {
            uint32_t pktSize = 0;
            while (g_pumpRunning.load(std::memory_order_relaxed) &&
                   ISteamNetworking_IsP2PPacketAvailable(g_pSteamNetworking, &pktSize, ch) &&
                   pktSize > 0)
            {
                if (pktSize > k_maxPacketSize) pktSize = k_maxPacketSize;

                uint64_t fromID = 0;
                uint32_t bytesRead = 0;
                bool ok = ISteamNetworking_ReadP2PPacket(g_pSteamNetworking,
                    pktBuf, pktSize, &bytesRead, &fromID, ch);

                if (ok && bytesRead > 0 && fromID != 0) {
                    ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, fromID);

                    // Ensure this peer is registered in synthetic IP mapping
                    {
                        std::lock_guard<std::mutex> lg(g_peerMutex);
                        if (g_steamIDToIP.find(fromID) == g_steamIDToIP.end()) {
                            uint32_t syntheticIP = 0x7F000001u | (uint32_t)(fromID & 0x00FFFFFFu);
                            g_steamIDToIP[fromID]   = syntheticIP;
                            g_ipToSteamID[syntheticIP] = fromID;
                        }
                    }

                    // Extract payload and metadata
                    RecvPacket pkt;
                    pkt.fromSteamID = fromID;

                    if (bytesRead >= sizeof(ReFixP2PHeader) && *(const uint32_t*)pktBuf == k_refixP2PMagic) {
                        const ReFixP2PHeader* hdr = reinterpret_cast<const ReFixP2PHeader*>(pktBuf);
                        uint16_t payloadLen = hdr->payloadLen;
                        if (sizeof(ReFixP2PHeader) + payloadLen <= bytesRead) {
                            pkt.data.assign(pktBuf + sizeof(ReFixP2PHeader), pktBuf + sizeof(ReFixP2PHeader) + payloadLen);
                            pkt.fromPort = hdr->srcPort;
                            pkt.destPort = hdr->dstPort;
                            pkt.service = (hdr->service == 2) ? SocketServiceType::Voice : SocketServiceType::Game;
                        } else {
                            pkt.data.assign(pktBuf + sizeof(ReFixP2PHeader), pktBuf + bytesRead);
                            pkt.fromPort = hdr->srcPort;
                            pkt.destPort = hdr->dstPort;
                            pkt.service = (hdr->service == 2) ? SocketServiceType::Voice : SocketServiceType::Game;
                        }
                    } else {
                        // Unframed / legacy fallback
                        pkt.data.assign(pktBuf, pktBuf + bytesRead);
                        pkt.service = (ch == k_nChannelVoice) ? SocketServiceType::Voice : SocketServiceType::Game;
                        pkt.destPort = (pkt.service == SocketServiceType::Voice) ? 5058 : 7777;
                        pkt.fromPort = pkt.destPort;
                    }

                    // Route packet into isolated per-socket / per-service queue
                    RouteIncomingPacket(std::move(pkt));
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

        Sleep(1); // 1ms polling
    }

    SteamP2PHook::Log("P2P pump thread stopped");
    return 0;
}

// =============================================================================
// Hooked Winsock Functions
// =============================================================================

// ------ sendto ---------------------------------------------------------------
static int WSAAPI Hook_sendto(SOCKET s, const char* buf, int len, int flags,
    const struct sockaddr* to, int tolen)
{
    if (to && to->sa_family == AF_INET && len > 0) {
        const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(to);
        uint32_t destIP = ntohl(sin->sin_addr.s_addr);
        uint16_t destPort = ntohs(sin->sin_port);
        SocketServiceType service = ClassifyPort(destPort);

        // Update SocketContext
        {
            std::lock_guard<std::mutex> lg(g_socketMutex);
            auto& ctx = g_socketContexts[s];
            ctx.socket = s;
            if (ctx.serviceType == SocketServiceType::Unknown) {
                ctx.serviceType = service;
            }
            ctx.lastRemotePort = destPort;
            ctx.lastActivity = GetTickCount();
            if (ctx.localPort == 0) {
                sockaddr_in localAddr{};
                int localLen = sizeof(localAddr);
                if (getsockname(s, (sockaddr*)&localAddr, &localLen) == 0) {
                    ctx.localPort = ntohs(localAddr.sin_port);
                }
            }
        }

        // Check if destination resolves to a known peer
        uint64_t steamID = 0;
        {
            std::lock_guard<std::mutex> lg(g_peerMutex);
            auto it = g_ipToSteamID.find(destIP);
            if (it != g_ipToSteamID.end()) {
                steamID = it->second;
                g_steamIDToPort[steamID] = destPort;
            } else if ((IsGamePort(destPort) || IsVoicePort(destPort) ||
                        (destIP & 0xFF000000u) == 0x7F000000u || destIP == 0 ||
                        (destIP & 0xFFFF0000u) == 0xC0A80000u || (destIP & 0xFF000000u) == 0x0A000000u) &&
                       g_ipToSteamID.size() == 1) {
                // If connecting to game/voice port or loopback/private subnet and 1 peer known
                steamID = g_ipToSteamID.begin()->second;
                g_steamIDToPort[steamID] = destPort;
            }
        }

        if (steamID != 0) {
            if (SendP2PWithEnvelope(steamID, s, (const uint8_t*)buf, (uint32_t)len, destPort, service)) {
                return len;
            }
            return SOCKET_ERROR;
        }

        // If no peer is known yet, but it's a Game or Voice port, buffer in Pre-Lobby Hold Buffer (V-02)
        // for P2P correlation. Returns len immediately to eliminate double-send.
        // FlushHoldBuffer will dispatch via P2P on correlation, or release to Winsock on TTL expiration.
        if (service != SocketServiceType::Unknown || IsGamePort(destPort)) {
            std::lock_guard<std::mutex> lg(g_holdBufferMutex);
            if (g_holdBuffer.size() < k_maxHoldBufferSize) {
                BufferedHoldPacket hpkt;
                hpkt.s = s;
                hpkt.data.assign((const uint8_t*)buf, (const uint8_t*)buf + len);
                hpkt.flags = flags;
                hpkt.destAddr = *sin;
                hpkt.destIP = destIP;
                hpkt.destPort = destPort;
                hpkt.service = (service != SocketServiceType::Unknown) ? service : SocketServiceType::Game;
                hpkt.timestampMs = GetTickCount();
                g_holdBuffer.push_back(std::move(hpkt));
                return len; // Synthetic success: held in buffer
            }
        }
    }

    // Fall through to real Winsock for raw packets
    if (to) {
        if (!ReFix::NetworkModeManager::IsExternalNetworkingAllowed() && !IsAllowedLanEndpoint(to)) {
            refix::lan::LanFirewall::Get().RecordBlockedEgress();
            return len; // Synthetic success: drop packet under Internet-Zero
        }
    }
    return g_orig_sendto(s, buf, len, flags, to, tolen);
}

// ------ recvfrom -------------------------------------------------------------
static int WSAAPI Hook_recvfrom(SOCKET s, char* buf, int len, int flags,
    struct sockaddr* from, int* fromlen)
{
    RecvPacket pkt;
    bool hasPkt = false;

    {
        std::lock_guard<std::mutex> lg(g_socketMutex);
        auto it = g_socketContexts.find(s);
        if (it == g_socketContexts.end()) {
            sockaddr_in localAddr{};
            int localLen = sizeof(localAddr);
            uint16_t lport = 0;
            if (getsockname(s, (sockaddr*)&localAddr, &localLen) == 0) {
                lport = ntohs(localAddr.sin_port);
            }
            auto& ctx = g_socketContexts[s];
            ctx.socket = s;
            ctx.localPort = lport;
            ctx.serviceType = ClassifyPort(lport);
            ctx.lastActivity = GetTickCount();

            // Drain any pending packets for this service type
            if (ctx.serviceType == SocketServiceType::Game) {
                while (!g_pendingGamePackets.empty() && ctx.recvQueue.size() < SocketContext::kMaxQueueSize) {
                    ctx.recvQueue.push_back(std::move(g_pendingGamePackets.front()));
                    g_pendingGamePackets.pop_front();
                }
            } else if (ctx.serviceType == SocketServiceType::Voice) {
                while (!g_pendingVoicePackets.empty() && ctx.recvQueue.size() < SocketContext::kMaxQueueSize) {
                    ctx.recvQueue.push_back(std::move(g_pendingVoicePackets.front()));
                    g_pendingVoicePackets.pop_front();
                }
            } else if (lport == 0) {
                // Ephemeral client socket: if pending game packets exist and no other game socket, treat as game
                bool otherGameSock = false;
                for (const auto& other : g_socketContexts) {
                    if (other.first != s && other.second.serviceType == SocketServiceType::Game) {
                        otherGameSock = true; break;
                    }
                }
                if (!otherGameSock && !g_pendingGamePackets.empty()) {
                    ctx.serviceType = SocketServiceType::Game;
                    while (!g_pendingGamePackets.empty() && ctx.recvQueue.size() < SocketContext::kMaxQueueSize) {
                        ctx.recvQueue.push_back(std::move(g_pendingGamePackets.front()));
                        g_pendingGamePackets.pop_front();
                    }
                }
            }
            it = g_socketContexts.find(s);
        } else {
            auto& ctx = it->second;
            if (ctx.serviceType == SocketServiceType::Game && !g_pendingGamePackets.empty()) {
                while (!g_pendingGamePackets.empty() && ctx.recvQueue.size() < SocketContext::kMaxQueueSize) {
                    ctx.recvQueue.push_back(std::move(g_pendingGamePackets.front()));
                    g_pendingGamePackets.pop_front();
                }
            } else if (ctx.serviceType == SocketServiceType::Voice && !g_pendingVoicePackets.empty()) {
                while (!g_pendingVoicePackets.empty() && ctx.recvQueue.size() < SocketContext::kMaxQueueSize) {
                    ctx.recvQueue.push_back(std::move(g_pendingVoicePackets.front()));
                    g_pendingVoicePackets.pop_front();
                }
            }
        }

        if (it != g_socketContexts.end() && !it->second.recvQueue.empty()) {
            pkt = std::move(it->second.recvQueue.front());
            it->second.recvQueue.pop_front();
            it->second.lastActivity = GetTickCount();
            hasPkt = true;
        }
    }

    if (hasPkt) {
        int copyLen = (int)pkt.data.size();
        if (copyLen > len) copyLen = len;
        memcpy(buf, pkt.data.data(), copyLen);

        if (from && fromlen && *fromlen >= (int)sizeof(struct sockaddr_in)) {
            struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(from);
            sin->sin_family = AF_INET;

            uint16_t srcPort = pkt.fromPort ? pkt.fromPort : (pkt.service == SocketServiceType::Voice ? 5058 : 7777);
            uint32_t srcIP = 0x7F000001u;
            {
                std::lock_guard<std::mutex> lgPeer(g_peerMutex);
                auto it = g_steamIDToIP.find(pkt.fromSteamID);
                if (it != g_steamIDToIP.end()) {
                    srcIP = it->second;
                }
            }
            sin->sin_port = htons(srcPort);
            sin->sin_addr.s_addr = htonl(srcIP);
            *fromlen = sizeof(struct sockaddr_in);
        }

        return copyLen;
    }

    // Fall through to real Winsock for raw packets
    return g_orig_recvfrom(s, buf, len, flags, from, fromlen);
}

// ------ connect --------------------------------------------------------------
static int WSAAPI Hook_connect(SOCKET s, const struct sockaddr* name, int namelen)
{
    if (name && name->sa_family == AF_INET) {
        const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(name);
        uint32_t destIP = ntohl(sin->sin_addr.s_addr);
        uint16_t destPort = ntohs(sin->sin_port);

        {
            std::lock_guard<std::mutex> lg(g_socketMutex);
            auto& ctx = g_socketContexts[s];
            ctx.socket = s;
            ctx.serviceType = ClassifyPort(destPort);
            ctx.lastRemotePort = destPort;
            ctx.lastActivity = GetTickCount();
        }

        uint64_t steamID = 0;
        {
            std::lock_guard<std::mutex> lg(g_peerMutex);
            auto it = g_ipToSteamID.find(destIP);
            if (it != g_ipToSteamID.end()) steamID = it->second;
        }

        if (steamID != 0 && g_pSteamNetworking) {
            ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, steamID);
            SteamP2PHook::Log("Hook_connect: P2P session accepted for SteamID=%llu", steamID);
        }
    }

    if (name) {
        if (!ReFix::NetworkModeManager::IsExternalNetworkingAllowed() && !IsAllowedLanEndpoint(name)) {
            refix::lan::LanFirewall::Get().RecordBlockedEgress();
            WSASetLastError(WSAEHOSTUNREACH);
            return SOCKET_ERROR;
        }
    }
    return g_orig_connect(s, name, namelen);
}

// ------ select ---------------------------------------------------------------
static int WSAAPI Hook_select(int nfds, fd_set* readfds, fd_set* writefds,
    fd_set* exceptfds, const struct timeval* timeout)
{
    fd_set inRead;
    bool hasInRead = false;
    if (readfds && readfds->fd_count > 0) {
        inRead = *readfds;
        hasInRead = true;
    }

    fd_set readySet;
    FD_ZERO(&readySet);
    int p2pReadyCount = 0;

    if (hasInRead) {
        std::lock_guard<std::mutex> lg(g_socketMutex);
        for (u_int i = 0; i < inRead.fd_count; ++i) {
            SOCKET s = inRead.fd_array[i];
            auto it = g_socketContexts.find(s);
            if (it != g_socketContexts.end() && !it->second.recvQueue.empty()) {
                FD_SET(s, &readySet);
                p2pReadyCount++;
            } else if (!g_pendingGamePackets.empty() || !g_pendingVoicePackets.empty()) {
                if (it != g_socketContexts.end()) {
                    if ((it->second.serviceType == SocketServiceType::Game && !g_pendingGamePackets.empty()) ||
                        (it->second.serviceType == SocketServiceType::Voice && !g_pendingVoicePackets.empty())) {
                        FD_SET(s, &readySet);
                        p2pReadyCount++;
                    }
                }
            }
        }
    }

    if (p2pReadyCount > 0) {
        struct timeval zeroTv = { 0, 0 };
        int realRes = g_orig_select(nfds, readfds, writefds, exceptfds, &zeroTv);
        if (realRes > 0 && readfds) {
            for (u_int i = 0; i < readySet.fd_count; ++i) {
                FD_SET(readySet.fd_array[i], readfds);
            }
            return (int)readfds->fd_count;
        } else {
            if (readfds) *readfds = readySet;
            return p2pReadyCount;
        }
    }

    return g_orig_select(nfds, readfds, writefds, exceptfds, timeout);
}

// ------ bind (V-04 Port Virtualization) ---------------------------------------
static int WSAAPI Hook_bind(SOCKET s, const struct sockaddr* name, int namelen)
{
    if (name && name->sa_family == AF_INET) {
        const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(name);
        uint16_t requestedPort = ntohs(sin->sin_port);
        SocketServiceType service = ClassifyPort(requestedPort);

        int res = g_orig_bind(s, name, namelen);
        if (res != 0) {
            int err = WSAGetLastError();
            // Port Virtualization on WSAEADDRINUSE (10048) for game ports
            if (err == WSAEADDRINUSE && (service == SocketServiceType::Game || IsGamePort(requestedPort))) {
                struct sockaddr_in retryAddr = *sin;
                for (uint16_t offset = 1; offset <= 30; ++offset) {
                    uint16_t newPort = requestedPort + offset;
                    retryAddr.sin_port = htons(newPort);
                    if (g_orig_bind(s, reinterpret_cast<const struct sockaddr*>(&retryAddr), namelen) == 0) {
                        g_virtualizedGamePort.store(newPort);
                        SteamP2PHook::Log("[PortVirtualization] Port %u collision (WSAEADDRINUSE) -> virtualized to %u",
                            requestedPort, newPort);

                        ReFix_OnGamePortVirtualized(newPort);

                        std::lock_guard<std::mutex> lg(g_socketMutex);
                        auto& ctx = g_socketContexts[s];
                        ctx.socket = s;
                        ctx.localPort = newPort;
                        ctx.virtualPort = newPort;
                        ctx.serviceType = SocketServiceType::Game;
                        ctx.lastActivity = GetTickCount();
                        return 0; // Success!
                    }
                }
                SteamP2PHook::Log("[PortVirtualization] Failed to find alternative port for %u after 30 attempts", requestedPort);
            }
            return res;
        }

        // Successful normal bind
        uint16_t actualPort = requestedPort;
        if (actualPort == 0) {
            struct sockaddr_in boundAddr = {};
            int boundLen = sizeof(boundAddr);
            if (getsockname(s, reinterpret_cast<struct sockaddr*>(&boundAddr), &boundLen) == 0) {
                actualPort = ntohs(boundAddr.sin_port);
            }
        }
        {
            std::lock_guard<std::mutex> lg(g_socketMutex);
            auto& ctx = g_socketContexts[s];
            ctx.socket = s;
            ctx.localPort = actualPort;
            ctx.serviceType = (service != SocketServiceType::Unknown) ? service : ClassifyPort(actualPort);
            ctx.lastActivity = GetTickCount();
        }
        if (service == SocketServiceType::Game || service == SocketServiceType::Voice || actualPort != requestedPort) {
            SteamP2PHook::Log("Hook_bind: Socket %llu bound to port %u (Service: %s)",
                (uint64_t)s, actualPort, (service == SocketServiceType::Voice) ? "Voice" : "Game");
        }
        return 0;
    }
    return g_orig_bind(s, name, namelen);
}

// ------ closesocket ----------------------------------------------------------
static int WSAAPI Hook_closesocket(SOCKET s)
{
    {
        std::lock_guard<std::mutex> lg(g_socketMutex);
        g_socketContexts.erase(s);
    }
    if (g_orig_closesocket) {
        return g_orig_closesocket(s);
    }
    return 0;
}

// =============================================================================
// MinHook Installation & Lifecycle
// =============================================================================
namespace SteamP2PHook {

uint16_t GetBoundGamePort() {
    uint16_t v = g_virtualizedGamePort.load();
    if (v != 0) return v;
    std::lock_guard<std::mutex> lg(g_socketMutex);
    for (const auto& kv : g_socketContexts) {
        if (kv.second.serviceType == SocketServiceType::Game && kv.second.localPort != 0) {
            return kv.second.localPort;
        }
    }
    return 0;
}

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

    // Hook bind
    {
        void* target = GetProcAddress(hWs2, "bind");
        if (MH_CreateHook(target, (void*)Hook_bind, (void**)&g_orig_bind) == MH_OK)
            MH_EnableHook(target);
        Log("bind hook: %s", g_orig_bind ? "OK" : "FAIL");
    }

    // Hook closesocket
    {
        void* target = GetProcAddress(hWs2, "closesocket");
        if (MH_CreateHook(target, (void*)Hook_closesocket, (void**)&g_orig_closesocket) == MH_OK)
            MH_EnableHook(target);
        Log("closesocket hook: %s", g_orig_closesocket ? "OK" : "FAIL");
    }

    g_hooksInstalled = true;

    // Resolve ISteamNetworking immediately if possible
    g_pSteamNetworking = ResolveSteamNetworking();

    // Start background pump thread
    g_pumpRunning.store(true, std::memory_order_relaxed);
    g_pumpThread = CreateThread(NULL, 0, P2PPumpThread, NULL, 0, NULL);

    Log("Winsock P2P hooks installed. ISteamNetworking=%p", g_pSteamNetworking);
}

void Uninstall() {
    if (!g_hooksInstalled) return;

    g_pumpRunning.store(false, std::memory_order_relaxed);

    if (g_pumpThread) {
        WaitForSingleObject(g_pumpThread, 100);
        CloseHandle(g_pumpThread);
        g_pumpThread = nullptr;
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    g_pSteamNetworking = nullptr;
    g_hooksInstalled = false;

    {
        std::lock_guard<std::mutex> lg(g_socketMutex);
        g_socketContexts.clear();
        g_pendingGamePackets.clear();
        g_pendingVoicePackets.clear();
    }
    {
        std::lock_guard<std::mutex> lg(g_holdBufferMutex);
        g_holdBuffer.clear();
    }
    g_virtualizedGamePort.store(0);
}

void RegisterPeer(uint64_t steamID, uint32_t ipv4_host, uint16_t port) {
    if (!steamID || !ipv4_host) return;

    {
        std::lock_guard<std::mutex> lg(g_peerMutex);
        auto itOld = g_steamIDToIP.find(steamID);
        if (itOld != g_steamIDToIP.end()) {
            g_ipToSteamID.erase(itOld->second);
        }

        g_steamIDToIP[steamID]  = ipv4_host;
        g_ipToSteamID[ipv4_host] = steamID;
        if (port != 0) {
            g_steamIDToPort[steamID] = port;
        }
    }

    if (g_pSteamNetworking) {
        ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, steamID);
    }

    // Flush any pre-lobby packets held waiting for this peer!
    FlushHoldBuffer();

    char ipStr[32];
    uint32_t n = htonl(ipv4_host);
    inet_ntop(AF_INET, &n, ipStr, sizeof(ipStr));
    Log("RegisterPeer: SteamID=%llu <-> IP=%s (port=%u)", steamID, ipStr, port);
}

void UnregisterPeer(uint64_t steamID) {
    std::lock_guard<std::mutex> lg(g_peerMutex);
    auto it = g_steamIDToIP.find(steamID);
    if (it != g_steamIDToIP.end()) {
        g_ipToSteamID.erase(it->second);
        g_steamIDToIP.erase(it);
        auto itPort = g_steamIDToPort.find(steamID);
        if (itPort != g_steamIDToPort.end()) {
            g_steamIDToPort.erase(itPort);
        }
        Log("UnregisterPeer: SteamID=%llu removed", steamID);
    }
}

} // namespace SteamP2PHook

// =============================================================================
// Force-resolve ISteamNetworking post SteamAPI_Init
// =============================================================================
extern "C" void SteamP2PHook_ForceResolve() {
    g_pSteamNetworking = nullptr;
    g_pSteamNetworking = ResolveSteamNetworking();
    if (g_pSteamNetworking) {
        SteamP2PHook::Log("[ForceResolve] ISteamNetworking obtained post-Init -> %p", g_pSteamNetworking);
        using fn_AllowRelay_t = bool(__thiscall*)(void*, bool);
        void** vt = *(void***)g_pSteamNetworking;
        auto fnRelay = (fn_AllowRelay_t)vt[7]; // vtable index 7 = AllowP2PPacketRelay
        fnRelay(g_pSteamNetworking, true);
        SteamP2PHook::Log("[ForceResolve] AllowP2PPacketRelay(true) called - SDR relay enabled");
    } else {
        SteamP2PHook::Log("[ForceResolve] WARNING: ISteamNetworking still not resolved post-Init");
    }
}

extern "C" {
__declspec(dllexport) void ReFix_RegisterP2PPeer(uint64_t steamID, uint32_t ipv4_host) {
    SteamP2PHook::RegisterPeer(steamID, ipv4_host);
}

__declspec(dllexport) void ReFix_RegisterP2PPeerStr(uint64_t steamID, const char* ipStr) {
    if (!ipStr || !*ipStr) return;
    struct in_addr addr;
    if (inet_pton(AF_INET, ipStr, &addr) == 1) {
        SteamP2PHook::RegisterPeer(steamID, ntohl(addr.s_addr));
    }
}

__declspec(dllexport) uint16_t ReFix_GetBoundGamePort() {
    return SteamP2PHook::GetBoundGamePort();
}
}
