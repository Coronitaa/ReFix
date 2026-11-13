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
#include <iphlpapi.h>
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <string>

#include "steam_p2p_hook.h"
#include "unreal_steam_emu.h"
#include "minhook/MinHook.h"
#include "network/network_mode.h"

// Helper to determine if an IP is local/LAN
#include "lan_core/refix_lan_firewall.h"

// ReFix LAN reaches every peer directly over the local network, so raw UDP must never be
// redirected into the emulated relay. Redirecting by IP also breaks several instances on one
// PC: they all share one IP, and an EOS join request sent to the host's socket ended up in the
// relay instead (MECCHA CHAMELEON could find rooms but never enter them).
#ifdef REFIX_LAN_EDITION
static constexpr bool kRedirectRawUdpToRelay = false;
#else
static constexpr bool kRedirectRawUdpToRelay = true;
#endif

// Helper to determine if an IP is local/LAN
static bool IsAllowedLanEndpoint(const sockaddr* addr) {
    if (!addr) return false;
    return refix::lan::LanFirewall::Get().IsAllowedSockaddr(addr);
}

static bool IsPhotonEgressAllowed(uint16_t port) {
    if ((port >= 27000 && port <= 27100) || (port >= 5055 && port <= 5058) || (port >= 9090 && port <= 9093) || port == 443) {
        static int s_photonAllowed = -1;
        if (s_photonAllowed == -1) {
            char buf[64] = {0};
            char iniPath[MAX_PATH] = {0};
            GetModuleFileNameA(NULL, iniPath, MAX_PATH);
            std::string p(iniPath);
            size_t pos = p.find_last_of("\\/");
            std::string ini = (pos != std::string::npos ? p.substr(0, pos + 1) : ".\\") + "ReFix.ini";
            GetPrivateProfileStringA("Photon", "Backend", "", buf, sizeof(buf), ini.c_str());
            if (_stricmp(buf, "OfficialPhoton") == 0) {
                s_photonAllowed = 1;
            } else {
                s_photonAllowed = 0;
            }
        }
        return s_photonAllowed == 1;
    }
    return false;
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
typedef int (WSAAPI* fn_send_t)(SOCKET s, const char* buf, int len, int flags);
typedef int (WSAAPI* fn_recv_t)(SOCKET s, char* buf, int len, int flags);

typedef int (WSAAPI* fn_WSASendTo_t)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr* lpTo, int iTolen,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int (WSAAPI* fn_WSARecvFrom_t)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr* lpFrom, LPINT lpFromlen,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int (WSAAPI* fn_WSAConnect_t)(SOCKET s, const struct sockaddr* name, int namelen,
    LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS);
typedef int (WSAAPI* fn_WSASend_t)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int (WSAAPI* fn_WSARecv_t)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

static fn_sendto_t      g_orig_sendto      = nullptr;
static fn_recvfrom_t    g_orig_recvfrom    = nullptr;
static fn_connect_t     g_orig_connect     = nullptr;
static fn_select_t      g_orig_select      = nullptr;
static fn_bind_t        g_orig_bind        = nullptr;
static fn_closesocket_t g_orig_closesocket = nullptr;
static fn_send_t        g_orig_send        = nullptr;
static fn_recv_t        g_orig_recv        = nullptr;

static fn_WSASendTo_t   g_orig_WSASendTo   = nullptr;
static fn_WSARecvFrom_t g_orig_WSARecvFrom = nullptr;
static fn_WSAConnect_t  g_orig_WSAConnect  = nullptr;
static fn_WSASend_t     g_orig_WSASend     = nullptr;
static fn_WSARecv_t     g_orig_WSARecv     = nullptr;

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
// Channels in Steam P2P (ISteamNetworking) dedicated for Winsock redirection
static const int k_nChannelGame  = 250;
static const int k_nChannelVoice = 251;

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
static std::recursive_mutex g_socketMutex;
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

static std::recursive_mutex g_holdBufferMutex;
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

static std::recursive_mutex g_peerMutex;
static std::unordered_map<uint32_t, uint64_t> g_ipToSteamID;
static std::unordered_map<uint64_t, uint32_t> g_steamIDToIP;
static std::unordered_map<uint64_t, uint16_t> g_steamIDToPort;

struct PeerEndpointInfo {
    uint64_t steamID = 0;
    uint32_t lanDataIP = 0;      // host byte order
    uint16_t lanDataPort = 0;
    uint16_t gameServerPort = 0;
    uint32_t overlayIP = 0;
    uint16_t overlayPort = 0;
    uint32_t wanIP = 0;
    uint16_t wanPort = 0;
    bool isLocalMachine = false;
};

static std::unordered_map<uint64_t, PeerEndpointInfo> g_peersBySteamID;
static std::unordered_map<uint64_t, uint64_t> g_peerAliases; // aliasSteamID -> realSteamID
static std::unordered_map<uint16_t, uint16_t> g_portRemap;    // originalPort -> remappedPort

namespace SteamP2PHook {
    uint32_t GetLocalLanIp() {
        static uint32_t s_localIp = 0;
        if (s_localIp != 0) return s_localIp;

        ULONG size = 16 * 1024;
        std::vector<uint8_t> buf(size);
        auto* aa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, aa, &size) == ERROR_BUFFER_OVERFLOW) {
            buf.resize(size);
            aa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        }
        if (GetAdaptersAddresses(AF_INET, flags, nullptr, aa, &size) == NO_ERROR) {
            uint32_t fallbackIp = 0;
            for (auto* a = aa; a; a = a->Next) {
                if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                std::wstring fn = a->FriendlyName ? a->FriendlyName : L"";
                std::wstring desc = a->Description ? a->Description : L"";
                for (auto& c : fn) c = towlower(c);
                for (auto& c : desc) c = towlower(c);
                const wchar_t* vkw[] = { L"vethernet", L"wsl", L"hyper-v", L"default switch", L"docker",
                                         L"vmware", L"virtualbox", L"zerotier", L"hamachi", L"tap", L"tun",
                                         L"tailscale", L"wireguard", L"host-only", nullptr };
                bool isVirt = false;
                for (int i = 0; vkw[i]; ++i) {
                    if (fn.find(vkw[i]) != std::wstring::npos || desc.find(vkw[i]) != std::wstring::npos) {
                        isVirt = true;
                        break;
                    }
                }
                if (isVirt) continue;

                bool hasGateway = (a->FirstGatewayAddress != nullptr);
                for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
                    if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
                    uint32_t ip = ntohl(reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr)->sin_addr.s_addr);
                    if (ip == 0 || (ip >> 24) == 127 || (ip >> 16) == 0xA9FE) continue;
                    if (hasGateway) {
                        s_localIp = ip;
                        return s_localIp;
                    }
                    if (fallbackIp == 0) fallbackIp = ip;
                }
            }
            if (fallbackIp != 0) {
                s_localIp = fallbackIp;
                return s_localIp;
            }
        }
        s_localIp = 0x7F000001u;
        return s_localIp;
    }

    uint64_t ResolvePeerAlias(uint64_t steamID) {
        std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
        auto it = g_peerAliases.find(steamID);
        if (it != g_peerAliases.end()) return it->second;
        return steamID;
    }

    void RegisterPeerAlias(uint64_t aliasSteamID, uint64_t realSteamID) {
        if (!aliasSteamID || !realSteamID || aliasSteamID == realSteamID) return;
        std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
        auto it = g_peerAliases.find(aliasSteamID);
        if (it != g_peerAliases.end() && it->second == realSteamID) return;
        g_peerAliases[aliasSteamID] = realSteamID;
        Log("RegisterPeerAlias: Alias %llu -> Real %llu", aliasSteamID, realSteamID);
    }

    bool IsLocalMachine(uint64_t steamID) {
        std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
        uint64_t resolved = steamID;
        auto itA = g_peerAliases.find(steamID);
        if (itA != g_peerAliases.end()) resolved = itA->second;
        auto it = g_peersBySteamID.find(resolved);
        if (it != g_peersBySteamID.end()) return it->second.isLocalMachine;
        return false;
    }

    uint16_t GetRemappedPort(uint16_t originalPort) {
        std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
        auto it = g_portRemap.find(originalPort);
        if (it != g_portRemap.end()) return it->second;
        return 0;
    }
}

static bool ExtractIpv4FromSockaddr(const sockaddr* sa, int sa_len, uint32_t& outIpv4Host, uint16_t& outPort, bool& outWasV6Mapped) {
    outWasV6Mapped = false;
    outIpv4Host = 0;
    outPort = 0;
    if (!sa) return false;
    if (sa->sa_family == AF_INET && sa_len >= (int)sizeof(sockaddr_in)) {
        const auto* sin = reinterpret_cast<const sockaddr_in*>(sa);
        outIpv4Host = ntohl(sin->sin_addr.s_addr);
        outPort = ntohs(sin->sin_port);
        return true;
    }
    if (sa->sa_family == AF_INET6 && sa_len >= (int)sizeof(sockaddr_in6)) {
        const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(sa);
        outPort = ntohs(sin6->sin6_port);
        if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
            outWasV6Mapped = true;
            uint32_t ipNet = 0;
            memcpy(&ipNet, &sin6->sin6_addr.s6_addr[12], 4);
            outIpv4Host = ntohl(ipNet);
            return true;
        }
    }
    return false;
}

static void SetSockaddrIpv4(sockaddr* sa, int sa_len, uint32_t newIpv4Host, uint16_t newPort, bool wasV6Mapped) {
    if (!sa) return;
    if (wasV6Mapped && sa_len >= (int)sizeof(sockaddr_in6)) {
        auto* sin6 = reinterpret_cast<sockaddr_in6*>(sa);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(newPort);
        memset(&sin6->sin6_addr, 0, 10);
        sin6->sin6_addr.s6_addr[10] = 0xFF;
        sin6->sin6_addr.s6_addr[11] = 0xFF;
        uint32_t ipNet = htonl(newIpv4Host);
        memcpy(&sin6->sin6_addr.s6_addr[12], &ipNet, 4);
    } else if (sa_len >= (int)sizeof(sockaddr_in)) {
        auto* sin = reinterpret_cast<sockaddr_in*>(sa);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(newPort);
        sin->sin_addr.s_addr = htonl(newIpv4Host);
    }
}

static std::recursive_mutex g_internalSocketsMutex;
static std::unordered_set<SOCKET> g_internalSockets;

namespace SteamP2PHook {
    void RegisterInternalSocket(uintptr_t s) {
        if (s == (uintptr_t)INVALID_SOCKET || s == 0) return;
        std::lock_guard<std::recursive_mutex> lock(g_internalSocketsMutex);
        g_internalSockets.insert((SOCKET)s);
    }

    void UnregisterInternalSocket(uintptr_t s) {
        if (s == (uintptr_t)INVALID_SOCKET || s == 0) return;
        std::lock_guard<std::recursive_mutex> lock(g_internalSocketsMutex);
        g_internalSockets.erase((SOCKET)s);
    }

    bool IsInternalSocket(uintptr_t s) {
        if (s == (uintptr_t)INVALID_SOCKET || s == 0) return false;
        std::lock_guard<std::recursive_mutex> lock(g_internalSocketsMutex);
        return g_internalSockets.find((SOCKET)s) != g_internalSockets.end();
    }
}

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
    if (!hSteam) {
        void* pEmuNet = UnrealSteamEmu::GetSteamNetworking();
        if (pEmuNet) {
            SteamP2PHook::Log("ISteamNetworking resolved via UnrealSteamEmu::GetSteamNetworking() -> %p", pEmuNet);
            return pEmuNet;
        }
        return nullptr;
    }

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

    void* pEmuNet = UnrealSteamEmu::GetSteamNetworking();
    if (pEmuNet) {
        SteamP2PHook::Log("ISteamNetworking resolved via UnrealSteamEmu fallback -> %p", pEmuNet);
        return pEmuNet;
    }
    return nullptr;
}

// =============================================================================
// Packet Dispatch & Queue Routing (V-01 Hardening)
// =============================================================================
static void RouteIncomingPacket(RecvPacket pkt) {
    std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
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
        std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
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
    std::lock_guard<std::recursive_mutex> lg(g_holdBufferMutex);
    if (g_holdBuffer.empty()) return;

    DWORD now = GetTickCount();
    auto it = g_holdBuffer.begin();
    while (it != g_holdBuffer.end()) {
        uint64_t targetSteamID = 0;
        {
            std::lock_guard<std::recursive_mutex> lgPeer(g_peerMutex);
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
                if (!ReFix::NetworkModeManager::IsInternetEgressAllowed() &&
                    !IsAllowedLanEndpoint((const sockaddr*)&it->destAddr) &&
                    !IsPhotonEgressAllowed(it->destPort)) {
                    refix::lan::LanFirewall::Get().RecordBlockedEgress();
                } else {
                    g_orig_sendto(it->s, (const char*)it->data.data(), (int)it->data.size(), it->flags,
                        (const sockaddr*)&it->destAddr, sizeof(it->destAddr));
                }
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
                        std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
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

                        // Route packet into isolated per-socket / per-service queue
                        RouteIncomingPacket(std::move(pkt));
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

        Sleep(1); // 1ms polling
    }

    SteamP2PHook::Log("P2P pump thread stopped");
    return 0;
}

// =============================================================================
// Hooked Winsock Functions
// =============================================================================

static bool TranslatePeerDestination(const sockaddr* originalTo, int tolen, sockaddr_storage& outTranslated, int& outTolen, uint64_t& outSteamId) {
    outSteamId = 0;
    outTolen = tolen;
    if (!originalTo || tolen <= 0) return false;
    memcpy(&outTranslated, originalTo, (std::min)(tolen, (int)sizeof(outTranslated)));

    uint32_t destIP = 0;
    uint16_t destPort = 0;
    bool wasV6 = false;
    if (!ExtractIpv4FromSockaddr(originalTo, tolen, destIP, destPort, wasV6)) {
        return false;
    }

    std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
    auto it = g_ipToSteamID.find(destIP);
    if (it != g_ipToSteamID.end()) {
        uint64_t resolvedId = it->second;
        auto aliasIt = g_peerAliases.find(resolvedId);
        if (aliasIt != g_peerAliases.end()) resolvedId = aliasIt->second;

        if (resolvedId != UnrealSteamEmu::GetLocalSteamID()) {
            outSteamId = resolvedId;
            auto pIt = g_peersBySteamID.find(resolvedId);
            if (pIt != g_peersBySteamID.end()) {
                const auto& peer = pIt->second;
                uint16_t targetPort = peer.gameServerPort ? peer.gameServerPort : destPort;
                auto itRemap = g_portRemap.find(targetPort);
                if (itRemap != g_portRemap.end()) targetPort = itRemap->second;

                if (peer.isLocalMachine) {
                    SetSockaddrIpv4((sockaddr*)&outTranslated, tolen, 0x7F000001u, targetPort, wasV6);
                    return true;
                } else if (destIP == peer.wanIP) {
                    if (peer.lanDataIP != 0) {
                        SetSockaddrIpv4((sockaddr*)&outTranslated, tolen, peer.lanDataIP, targetPort, wasV6);
                        return true;
                    } else {
                        // Fallback to loopback if local or unknown
                        SetSockaddrIpv4((sockaddr*)&outTranslated, tolen, 0x7F000001u, targetPort, wasV6);
                        return true;
                    }
                }
            }
        }
    }

    // Direct loopback remapping for same-machine multi-instance
    if (destIP == 0x7F000001u) {
        auto itRemap = g_portRemap.find(destPort);
        if (itRemap != g_portRemap.end() && itRemap->second != destPort) {
            SetSockaddrIpv4((sockaddr*)&outTranslated, tolen, 0x7F000001u, itRemap->second, wasV6);
            return true;
        }
    }
    return false;
}

// ------ sendto ---------------------------------------------------------------
static int WSAAPI Hook_sendto(SOCKET s, const char* buf, int len, int flags,
    const struct sockaddr* to, int tolen)
{
    try {
        // 1. Bypass any registered internal sockets (LanTransport, UnrealSteamEmu)
        if (SteamP2PHook::IsInternalSocket((uintptr_t)s)) {
            return g_orig_sendto(s, buf, len, flags, to, tolen);
        }

        // 2. Defense-in-depth: If payload is already an internal ReFix wire frame or P2P envelope, never intercept
        if (len >= 4 && buf != nullptr) {
            uint32_t magic = *reinterpret_cast<const uint32_t*>(buf);
            if (magic == 0x52464958 || magic == 0x58464552) { // 'RFIX' (0x52464958) or 'REFX' (0x58464552)
                return g_orig_sendto(s, buf, len, flags, to, tolen);
            }
        }

        sockaddr_storage connectedPeer{};
        int peerLen = sizeof(connectedPeer);
        if (!to && s != INVALID_SOCKET) {
            if (getpeername(s, (sockaddr*)&connectedPeer, &peerLen) == 0) {
                to = (const sockaddr*)&connectedPeer;
                tolen = peerLen;
            }
        }

        sockaddr_storage translatedTo;
        int translatedLen = tolen;
        uint64_t targetSteamId = 0;
        bool wasTranslated = TranslatePeerDestination(to, tolen, translatedTo, translatedLen, targetSteamId);
        const sockaddr* effectiveTo = wasTranslated ? (const sockaddr*)&translatedTo : to;

        if (effectiveTo && len > 0) {
            uint32_t destIP = 0;
            uint16_t destPort = 0;
            bool wasV6 = false;
            ExtractIpv4FromSockaddr(effectiveTo, translatedLen, destIP, destPort, wasV6);

            // Bypass internal emulator discovery and broadcast ports (all slots 47584..47599, plus 47600)
            if (destPort >= 47584 && destPort <= 47600) {
                return g_orig_sendto(s, buf, len, flags, effectiveTo, translatedLen);
            }

            SocketServiceType service = ClassifyPort(destPort);

            // Update SocketContext
            {
                std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
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

            if (kRedirectRawUdpToRelay && targetSteamId != 0 && targetSteamId != UnrealSteamEmu::GetLocalSteamID()) {
                if (SendP2PWithEnvelope(targetSteamId, s, (const uint8_t*)buf, (uint32_t)len, destPort, service)) {
                    return len;
                }
                return SOCKET_ERROR;
            }

            // Enforce Internet-Zero BEFORE buffering into Pre-Lobby Hold Buffer
            if (!ReFix::NetworkModeManager::IsInternetEgressAllowed() && !IsAllowedLanEndpoint(effectiveTo) && !IsPhotonEgressAllowed(destPort)) {
                refix::lan::LanFirewall::Get().RecordBlockedEgress();
                uint64_t totalBlocked = refix::lan::LanFirewall::Get().GetBlockedEgressCount();
                SteamP2PHook::Log("[Internet-Zero] BLOCKED sendto to external endpoint %u.%u.%u.%u:%u (synthetic_success, total_blocked=%llu)",
                    (destIP >> 24) & 0xFF, (destIP >> 16) & 0xFF, (destIP >> 8) & 0xFF, destIP & 0xFF, destPort, totalBlocked);
                return len; // Synthetic success: drop packet under Internet-Zero
            }

            if (kRedirectRawUdpToRelay && (service != SocketServiceType::Unknown || IsGamePort(destPort))) {
                std::lock_guard<std::recursive_mutex> lg(g_holdBufferMutex);
                if (g_holdBuffer.size() < k_maxHoldBufferSize) {
                    BufferedHoldPacket hpkt;
                    hpkt.s = s;
                    hpkt.data.assign((const uint8_t*)buf, (const uint8_t*)buf + len);
                    hpkt.flags = flags;
                    if (effectiveTo->sa_family == AF_INET) {
                        hpkt.destAddr = *reinterpret_cast<const sockaddr_in*>(effectiveTo);
                    }
                    hpkt.destIP = destIP;
                    hpkt.destPort = destPort;
                    hpkt.service = (service != SocketServiceType::Unknown) ? service : SocketServiceType::Game;
                    hpkt.timestampMs = GetTickCount();
                    g_holdBuffer.push_back(std::move(hpkt));
                    return len; // Synthetic success: held in buffer
                }
            }
        }

        return g_orig_sendto(s, buf, len, flags, effectiveTo, translatedLen);
    } catch (const std::exception& e) {
        SteamP2PHook::Log("[P2PHook] Exception in Hook_sendto: %s", e.what());
    } catch (...) {
        SteamP2PHook::Log("[P2PHook] Unknown exception in Hook_sendto");
    }
    return g_orig_sendto ? g_orig_sendto(s, buf, len, flags, to, tolen) : SOCKET_ERROR;
}

// ------ WSASendTo ------------------------------------------------------------
static int WSAAPI Hook_WSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr* lpTo, int iTolen,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    try {
        if (SteamP2PHook::IsInternalSocket((uintptr_t)s)) {
            return g_orig_WSASendTo ? g_orig_WSASendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine) : SOCKET_ERROR;
        }

        DWORD totalBytes = 0;
        for (DWORD i = 0; i < dwBufferCount; ++i) {
            if (lpBuffers && lpBuffers[i].buf) totalBytes += lpBuffers[i].len;
        }

        sockaddr_storage translatedTo;
        int translatedLen = iTolen;
        uint64_t targetSteamId = 0;
        bool wasTranslated = TranslatePeerDestination(lpTo, iTolen, translatedTo, translatedLen, targetSteamId);
        const sockaddr* effectiveTo = wasTranslated ? (const sockaddr*)&translatedTo : lpTo;

        if (kRedirectRawUdpToRelay && targetSteamId != 0 && totalBytes > 0) {
            std::vector<uint8_t> linearBuf(totalBytes);
            size_t offset = 0;
            for (DWORD i = 0; i < dwBufferCount; ++i) {
                if (lpBuffers && lpBuffers[i].buf && lpBuffers[i].len > 0) {
                    memcpy(linearBuf.data() + offset, lpBuffers[i].buf, lpBuffers[i].len);
                    offset += lpBuffers[i].len;
                }
            }
            uint32_t destIP = 0;
            uint16_t destPort = 0;
            bool wasV6 = false;
            ExtractIpv4FromSockaddr(effectiveTo, translatedLen, destIP, destPort, wasV6);
            SocketServiceType service = ClassifyPort(destPort);
            if (SendP2PWithEnvelope(targetSteamId, s, linearBuf.data(), (uint32_t)totalBytes, destPort, service)) {
                if (lpNumberOfBytesSent) *lpNumberOfBytesSent = totalBytes;
                return 0;
            }
        }

        if (g_orig_WSASendTo) {
            return g_orig_WSASendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, effectiveTo, translatedLen, lpOverlapped, lpCompletionRoutine);
        }
    } catch (const std::exception& e) {
        SteamP2PHook::Log("[P2PHook] Exception in Hook_WSASendTo: %s", e.what());
    } catch (...) {
        SteamP2PHook::Log("[P2PHook] Unknown exception in Hook_WSASendTo");
    }
    return g_orig_WSASendTo ? g_orig_WSASendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine) : SOCKET_ERROR;
}

// ------ send -----------------------------------------------------------------
static int WSAAPI Hook_send(SOCKET s, const char* buf, int len, int flags) {
    sockaddr_storage peerAddr{};
    int peerLen = sizeof(peerAddr);
    if (getpeername(s, (sockaddr*)&peerAddr, &peerLen) == 0) {
        return Hook_sendto(s, buf, len, flags, (const sockaddr*)&peerAddr, peerLen);
    }
    if (g_orig_send) return g_orig_send(s, buf, len, flags);
    return SOCKET_ERROR;
}

// ------ WSASend --------------------------------------------------------------
static int WSAAPI Hook_WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    sockaddr_storage peerAddr{};
    int peerLen = sizeof(peerAddr);
    if (getpeername(s, (sockaddr*)&peerAddr, &peerLen) == 0) {
        return Hook_WSASendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, (const sockaddr*)&peerAddr, peerLen, lpOverlapped, lpCompletionRoutine);
    }
    if (g_orig_WSASend) {
        return g_orig_WSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
    }
    return SOCKET_ERROR;
}

// ------ recvfrom -------------------------------------------------------------
static int WSAAPI Hook_recvfrom(SOCKET s, char* buf, int len, int flags,
    struct sockaddr* from, int* fromlen)
{
    try {
        if (SteamP2PHook::IsInternalSocket((uintptr_t)s)) {
            return g_orig_recvfrom(s, buf, len, flags, from, fromlen);
        }

        RecvPacket pkt;
        bool hasPkt = false;

        {
            std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
            auto it = g_socketContexts.find(s);
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

            if (from && fromlen) {
                uint16_t srcPort = pkt.fromPort ? pkt.fromPort : (pkt.service == SocketServiceType::Voice ? 5058 : 7777);
                uint32_t srcIP = 0x7F000001u;
                {
                    std::lock_guard<std::recursive_mutex> lgPeer(g_peerMutex);
                    auto it = g_steamIDToIP.find(pkt.fromSteamID);
                    if (it != g_steamIDToIP.end()) srcIP = it->second;
                }
                if (*fromlen >= (int)sizeof(sockaddr_in6) && (from->sa_family == AF_INET6 || from->sa_family != AF_INET)) {
                    SetSockaddrIpv4(from, *fromlen, srcIP, srcPort, true);
                    *fromlen = sizeof(sockaddr_in6);
                } else if (*fromlen >= (int)sizeof(sockaddr_in)) {
                    SetSockaddrIpv4(from, *fromlen, srcIP, srcPort, false);
                    *fromlen = sizeof(sockaddr_in);
                }
            }

            return copyLen;
        }

        return g_orig_recvfrom(s, buf, len, flags, from, fromlen);
    } catch (const std::exception& e) {
        SteamP2PHook::Log("[P2PHook] Exception in Hook_recvfrom: %s", e.what());
    } catch (...) {
        SteamP2PHook::Log("[P2PHook] Unknown exception in Hook_recvfrom");
    }
    return g_orig_recvfrom ? g_orig_recvfrom(s, buf, len, flags, from, fromlen) : SOCKET_ERROR;
}

// ------ WSARecvFrom ----------------------------------------------------------
static int WSAAPI Hook_WSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr* lpFrom, LPINT lpFromlen,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    try {
        if (SteamP2PHook::IsInternalSocket((uintptr_t)s)) {
            return g_orig_WSARecvFrom ? g_orig_WSARecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine) : SOCKET_ERROR;
        }

        RecvPacket pkt;
        bool hasPkt = false;
        {
            std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
            auto it = g_socketContexts.find(s);
            if (it != g_socketContexts.end() && !it->second.recvQueue.empty()) {
                pkt = std::move(it->second.recvQueue.front());
                it->second.recvQueue.pop_front();
                it->second.lastActivity = GetTickCount();
                hasPkt = true;
            }
        }

        if (hasPkt) {
            DWORD copied = 0;
            DWORD remaining = (DWORD)pkt.data.size();
            const uint8_t* src = pkt.data.data();
            for (DWORD i = 0; i < dwBufferCount && remaining > 0; ++i) {
                DWORD chunk = (std::min)(lpBuffers[i].len, remaining);
                if (chunk > 0 && lpBuffers[i].buf) {
                    memcpy(lpBuffers[i].buf, src, chunk);
                    src += chunk;
                    remaining -= chunk;
                    copied += chunk;
                }
            }
            if (lpNumberOfBytesRecvd) *lpNumberOfBytesRecvd = copied;
            if (lpFlags) *lpFlags = 0;

            if (lpFrom && lpFromlen) {
                uint16_t srcPort = pkt.fromPort ? pkt.fromPort : (pkt.service == SocketServiceType::Voice ? 5058 : 7777);
                uint32_t srcIP = 0x7F000001u;
                {
                    std::lock_guard<std::recursive_mutex> lgPeer(g_peerMutex);
                    auto it = g_steamIDToIP.find(pkt.fromSteamID);
                    if (it != g_steamIDToIP.end()) srcIP = it->second;
                }
                if (*lpFromlen >= (int)sizeof(sockaddr_in6) && (lpFrom->sa_family == AF_INET6 || lpFrom->sa_family != AF_INET)) {
                    SetSockaddrIpv4(lpFrom, *lpFromlen, srcIP, srcPort, true);
                    *lpFromlen = sizeof(sockaddr_in6);
                } else if (*lpFromlen >= (int)sizeof(sockaddr_in)) {
                    SetSockaddrIpv4(lpFrom, *lpFromlen, srcIP, srcPort, false);
                    *lpFromlen = sizeof(sockaddr_in);
                }
            }
            return 0; // Success
        }

        if (g_orig_WSARecvFrom) {
            return g_orig_WSARecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);
        }
    } catch (const std::exception& e) {
        SteamP2PHook::Log("[P2PHook] Exception in Hook_WSARecvFrom: %s", e.what());
    } catch (...) {
        SteamP2PHook::Log("[P2PHook] Unknown exception in Hook_WSARecvFrom");
    }
    return g_orig_WSARecvFrom ? g_orig_WSARecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine) : SOCKET_ERROR;
}

// ------ recv -----------------------------------------------------------------
static int WSAAPI Hook_recv(SOCKET s, char* buf, int len, int flags) {
    return Hook_recvfrom(s, buf, len, flags, nullptr, nullptr);
}

// ------ WSARecv --------------------------------------------------------------
static int WSAAPI Hook_WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags,
    LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    return Hook_WSARecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, nullptr, nullptr, lpOverlapped, lpCompletionRoutine);
}

// ------ connect --------------------------------------------------------------
static int WSAAPI Hook_connect(SOCKET s, const struct sockaddr* name, int namelen)
{
    try {
        sockaddr_storage translated;
        int translatedLen = namelen;
        uint64_t targetSteamId = 0;
        bool translatedOk = TranslatePeerDestination(name, namelen, translated, translatedLen, targetSteamId);
        const sockaddr* effectiveName = translatedOk ? (const sockaddr*)&translated : name;

        if (targetSteamId != 0 && g_pSteamNetworking) {
            ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, targetSteamId);
        }

        return g_orig_connect(s, effectiveName, translatedLen);
    } catch (const std::exception& e) {
        SteamP2PHook::Log("[P2PHook] Exception in Hook_connect: %s", e.what());
    } catch (...) {
        SteamP2PHook::Log("[P2PHook] Unknown exception in Hook_connect");
    }
    return g_orig_connect ? g_orig_connect(s, name, namelen) : SOCKET_ERROR;
}

// ------ WSAConnect -----------------------------------------------------------
static int WSAAPI Hook_WSAConnect(SOCKET s, const struct sockaddr* name, int namelen,
    LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS, LPQOS lpGQOS)
{
    try {
        sockaddr_storage translated;
        int translatedLen = namelen;
        uint64_t targetSteamId = 0;
        bool translatedOk = TranslatePeerDestination(name, namelen, translated, translatedLen, targetSteamId);
        const sockaddr* effectiveName = translatedOk ? (const sockaddr*)&translated : name;

        if (targetSteamId != 0 && g_pSteamNetworking) {
            ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, targetSteamId);
        }
        if (g_orig_WSAConnect) {
            return g_orig_WSAConnect(s, effectiveName, translatedLen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS);
        }
        return g_orig_connect ? g_orig_connect(s, effectiveName, translatedLen) : SOCKET_ERROR;
    } catch (const std::exception& e) {
        SteamP2PHook::Log("[P2PHook] Exception in Hook_WSAConnect: %s", e.what());
    } catch (...) {
        SteamP2PHook::Log("[P2PHook] Unknown exception in Hook_WSAConnect");
    }
    return g_orig_WSAConnect ? g_orig_WSAConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS) : SOCKET_ERROR;
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
        std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
        for (u_int i = 0; i < inRead.fd_count; ++i) {
            SOCKET s = inRead.fd_array[i];
            if (SteamP2PHook::IsInternalSocket((uintptr_t)s)) continue;
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
    if (SteamP2PHook::IsInternalSocket((uintptr_t)s)) {
        return g_orig_bind(s, name, namelen);
    }

    if (name && (name->sa_family == AF_INET || name->sa_family == AF_INET6)) {
        uint32_t boundIp = 0;
        uint16_t requestedPort = 0;
        bool wasV6 = false;
        ExtractIpv4FromSockaddr(name, namelen, boundIp, requestedPort, wasV6);
        SocketServiceType service = ClassifyPort(requestedPort);

        int res = g_orig_bind(s, name, namelen);
        if (res != 0) {
            int err = WSAGetLastError();
            // Port Virtualization on WSAEADDRINUSE (10048) for game ports
            if (err == WSAEADDRINUSE && (service == SocketServiceType::Game || IsGamePort(requestedPort))) {
                int slot = UnrealSteamEmu::GetInstanceSlot();
                uint16_t basePort = (slot >= 1) ? (uint16_t)(requestedPort + slot * 10) : (uint16_t)(requestedPort + 1);
                sockaddr_storage retryAddr;
                memcpy(&retryAddr, name, namelen);

                for (uint16_t offset = 0; offset <= 30; ++offset) {
                    uint16_t newPort = basePort + offset;
                    SetSockaddrIpv4((sockaddr*)&retryAddr, namelen, 0, newPort, wasV6);
                    if (g_orig_bind(s, reinterpret_cast<const struct sockaddr*>(&retryAddr), namelen) == 0) {
                        {
                            std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
                            g_portRemap[requestedPort] = newPort;
                        }
                        g_virtualizedGamePort.store(newPort);
                        SteamP2PHook::Log("[PortVirtualization] Port %u collision (WSAEADDRINUSE) in slot %d -> virtualized to %u",
                            requestedPort, slot, newPort);

                        ReFix_OnGamePortVirtualized(newPort);

                        std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
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
            sockaddr_storage boundAddr = {};
            int boundLen = sizeof(boundAddr);
            if (getsockname(s, reinterpret_cast<struct sockaddr*>(&boundAddr), &boundLen) == 0) {
                uint32_t dummyIp = 0;
                bool dummyV6 = false;
                ExtractIpv4FromSockaddr((const sockaddr*)&boundAddr, boundLen, dummyIp, actualPort, dummyV6);
            }
        }
        {
            std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
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
    try {
        SteamP2PHook::UnregisterInternalSocket((uintptr_t)s);
        {
            std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
            g_socketContexts.erase(s);
        }
    } catch (...) {}
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
    std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
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

    // ALREADY_INITIALIZED: the overlay's save redirection may have started MinHook during SteamAPI_Init.
    MH_STATUS mhInit = MH_Initialize();
    if (mhInit != MH_OK && mhInit != MH_ERROR_ALREADY_INITIALIZED) {
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

    // Hook send
    {
        void* target = GetProcAddress(hWs2, "send");
        if (MH_CreateHook(target, (void*)Hook_send, (void**)&g_orig_send) == MH_OK)
            MH_EnableHook(target);
        Log("send hook: %s", g_orig_send ? "OK" : "FAIL");
    }

    // Hook recv
    {
        void* target = GetProcAddress(hWs2, "recv");
        if (MH_CreateHook(target, (void*)Hook_recv, (void**)&g_orig_recv) == MH_OK)
            MH_EnableHook(target);
        Log("recv hook: %s", g_orig_recv ? "OK" : "FAIL");
    }

    // Hook WSASendTo
    {
        void* target = GetProcAddress(hWs2, "WSASendTo");
        if (MH_CreateHook(target, (void*)Hook_WSASendTo, (void**)&g_orig_WSASendTo) == MH_OK)
            MH_EnableHook(target);
        Log("WSASendTo hook: %s", g_orig_WSASendTo ? "OK" : "FAIL");
    }

    // Hook WSARecvFrom
    {
        void* target = GetProcAddress(hWs2, "WSARecvFrom");
        if (MH_CreateHook(target, (void*)Hook_WSARecvFrom, (void**)&g_orig_WSARecvFrom) == MH_OK)
            MH_EnableHook(target);
        Log("WSARecvFrom hook: %s", g_orig_WSARecvFrom ? "OK" : "FAIL");
    }

    // Hook WSAConnect
    {
        void* target = GetProcAddress(hWs2, "WSAConnect");
        if (MH_CreateHook(target, (void*)Hook_WSAConnect, (void**)&g_orig_WSAConnect) == MH_OK)
            MH_EnableHook(target);
        Log("WSAConnect hook: %s", g_orig_WSAConnect ? "OK" : "FAIL");
    }

    // Hook WSASend
    {
        void* target = GetProcAddress(hWs2, "WSASend");
        if (MH_CreateHook(target, (void*)Hook_WSASend, (void**)&g_orig_WSASend) == MH_OK)
            MH_EnableHook(target);
        Log("WSASend hook: %s", g_orig_WSASend ? "OK" : "FAIL");
    }

    // Hook WSARecv
    {
        void* target = GetProcAddress(hWs2, "WSARecv");
        if (MH_CreateHook(target, (void*)Hook_WSARecv, (void**)&g_orig_WSARecv) == MH_OK)
            MH_EnableHook(target);
        Log("WSARecv hook: %s", g_orig_WSARecv ? "OK" : "FAIL");
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
        std::lock_guard<std::recursive_mutex> lg(g_socketMutex);
        g_socketContexts.clear();
        g_pendingGamePackets.clear();
        g_pendingVoicePackets.clear();
    }
    {
        std::lock_guard<std::recursive_mutex> lg(g_holdBufferMutex);
        g_holdBuffer.clear();
    }
    g_virtualizedGamePort.store(0);
}

void RegisterLanDataEndpoint(uint64_t steamID, uint32_t ipv4_host, uint16_t port) {
    if (!steamID || !ipv4_host) return;
    if (steamID == UnrealSteamEmu::GetLocalSteamID()) return;

    bool alreadySame = false;
    {
        std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
        auto it = g_peersBySteamID.find(steamID);
        if (it != g_peersBySteamID.end() && it->second.lanDataIP == ipv4_host && it->second.lanDataPort == port) {
            alreadySame = true;
        } else {
            auto& peer = g_peersBySteamID[steamID];
            peer.steamID = steamID;
            peer.lanDataIP = ipv4_host;
            peer.lanDataPort = port;
            if (ipv4_host == GetLocalLanIp() || ipv4_host == 0x7F000001u) {
                peer.isLocalMachine = true;
            }

            g_steamIDToIP[steamID] = ipv4_host;
            g_ipToSteamID[ipv4_host] = steamID;
            if (port != 0) g_steamIDToPort[steamID] = port;
        }
    }

    if (alreadySame) return;

    if (g_pSteamNetworking) {
        ISteamNetworking_AcceptP2PSessionWithUser(g_pSteamNetworking, steamID);
    }

    FlushHoldBuffer();

    char ipStr[32];
    uint32_t n = htonl(ipv4_host);
    inet_ntop(AF_INET, &n, ipStr, sizeof(ipStr));
    Log("RegisterLanDataEndpoint: SteamID=%llu <-> IP=%s (port=%u, isLocal=%d)",
        steamID, ipStr, port, g_peersBySteamID[steamID].isLocalMachine);
}

void RegisterGameServerPort(uint64_t steamID, uint16_t port) {
    if (!steamID || !port) return;
    std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
    auto it = g_peersBySteamID.find(steamID);
    if (it != g_peersBySteamID.end() && it->second.gameServerPort == port) {
        return;
    }
    auto& peer = g_peersBySteamID[steamID];
    peer.steamID = steamID;
    peer.gameServerPort = port;
    Log("RegisterGameServerPort: SteamID=%llu <-> GameServerPort=%u", steamID, port);
}

void RegisterOverlayEndpoint(uint64_t steamID, uint32_t ipv4_host, uint16_t port) {
    if (!steamID) return;
    std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
    auto it = g_peersBySteamID.find(steamID);
    if (it != g_peersBySteamID.end() && it->second.overlayIP == ipv4_host && it->second.overlayPort == port) {
        return;
    }
    auto& peer = g_peersBySteamID[steamID];
    peer.steamID = steamID;
    peer.overlayIP = ipv4_host;
    peer.overlayPort = port;
    Log("RegisterOverlayEndpoint: SteamID=%llu <-> OverlayPort=%u (ip=%u.%u.%u.%u)",
        steamID, port, (ipv4_host>>24)&0xFF, (ipv4_host>>16)&0xFF, (ipv4_host>>8)&0xFF, ipv4_host&0xFF);
}

void RegisterWanEndpoint(uint64_t steamID, uint32_t ipv4_host, uint16_t port) {
    if (!steamID || !ipv4_host) return;
    std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
    auto it = g_peersBySteamID.find(steamID);
    if (it != g_peersBySteamID.end() && it->second.wanIP == ipv4_host && it->second.wanPort == port) {
        return;
    }
    auto& peer = g_peersBySteamID[steamID];
    peer.steamID = steamID;
    peer.wanIP = ipv4_host;
    peer.wanPort = port;
    g_ipToSteamID[ipv4_host] = steamID;
    Log("RegisterWanEndpoint: SteamID=%llu <-> WAN IP=%u.%u.%u.%u:%u",
        steamID, (ipv4_host>>24)&0xFF, (ipv4_host>>16)&0xFF, (ipv4_host>>8)&0xFF, ipv4_host&0xFF, port);
}

void RegisterWanEndpoint(uint64_t steamID, const char* ipStr, uint16_t port) {
    if (!steamID || !ipStr || !ipStr[0]) return;
    in_addr addr{};
    if (inet_pton(AF_INET, ipStr, &addr) == 1) {
        RegisterWanEndpoint(steamID, ntohl(addr.s_addr), port);
    }
}

void RegisterPeer(uint64_t steamID, uint32_t ipv4_host, uint16_t port) {
    if (!steamID || !ipv4_host) return;
    if (steamID == UnrealSteamEmu::GetLocalSteamID()) return;

    // Guard: Never let overlay ports overwrite LanCore data endpoints
    if (port == 49996 || port == 50750 || port == 50751 || (port >= 50740 && port <= 50760)) {
        RegisterOverlayEndpoint(steamID, ipv4_host, port);
        return;
    }

    RegisterLanDataEndpoint(steamID, ipv4_host, port);
}

void UnregisterPeer(uint64_t steamID) {
    std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
    auto it = g_steamIDToIP.find(steamID);
    if (it != g_steamIDToIP.end()) {
        g_ipToSteamID.erase(it->second);
        g_steamIDToIP.erase(it);
    }
    auto itPort = g_steamIDToPort.find(steamID);
    if (itPort != g_steamIDToPort.end()) {
        g_steamIDToPort.erase(itPort);
    }
    auto itPeer = g_peersBySteamID.find(steamID);
    if (itPeer != g_peersBySteamID.end()) {
        if (itPeer->second.wanIP) g_ipToSteamID.erase(itPeer->second.wanIP);
        g_peersBySteamID.erase(itPeer);
    }
    Log("UnregisterPeer: SteamID=%llu removed", steamID);
}

std::vector<uint64_t> GetRegisteredPeers() {
    std::lock_guard<std::recursive_mutex> lg(g_peerMutex);
    std::vector<uint64_t> result;
    result.reserve(g_steamIDToIP.size());
    for (const auto& kv : g_steamIDToIP) {
        if (kv.first) result.push_back(kv.first);
    }
    return result;
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

__declspec(dllexport) void ReFix_RegisterInternalSocket(uintptr_t s) {
    SteamP2PHook::RegisterInternalSocket(s);
}

__declspec(dllexport) void ReFix_UnregisterInternalSocket(uintptr_t s) {
    SteamP2PHook::UnregisterInternalSocket(s);
}
}

