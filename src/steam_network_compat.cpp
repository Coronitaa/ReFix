// =============================================================================
// ReFix - Steam Networking Compatibility Module (NetCompat)
// steam_network_compat.cpp - Generic compatibility for SteamNetworkingSockets
// =============================================================================
#include "steam_network_compat.h"
#include <cstdio>
#include <atomic>
#include <string>
#include <cstdint>
#include <cassert>

extern void ReFixLog(const char* fmt, ...);
extern FARPROC g_steamProcs[];
extern HMODULE g_hOriginalDll;

namespace SteamNetCompat {

static std::atomic<bool> s_initialized{ false };
static std::atomic<bool> s_active{ false };
static std::atomic<bool> s_isGoldbergMode{ false };
static std::atomic<uint32_t> s_maskAppId{ 0 };
static std::atomic<uint32_t> s_realAppId{ 0 };
static std::atomic<bool> s_pathDetected{ false };

static std::atomic<bool> s_configEnableCompat{ true };
static std::atomic<bool> s_configVerboseLog{ true };

// Original flat function pointers
typedef uint32_t (*fn_CreateListenSocketP2P_t)(void* self, int nLocalVirtualPort, int nNumOptions, const void* pOptions);
typedef uint32_t (*fn_ConnectP2P_t)(void* self, const void* pIdentityRemote, int nRemoteVirtualPort, int nNumOptions, const void* pOptions);
typedef int (*fn_AcceptConnection_t)(void* self, uint32_t hConn);
typedef bool (*fn_CloseConnection_t)(void* self, uint32_t hConn, int nReason, const char* pszDebug, bool bEnableLinger);

typedef void* (*fn_SteamNetworkingSockets_v008_t)();
typedef void* (*fn_SteamNetworkingUtils_v003_t)();

static fn_CreateListenSocketP2P_t s_orig_CreateListenSocketP2P = nullptr;
static fn_ConnectP2P_t s_orig_ConnectP2P = nullptr;
static fn_AcceptConnection_t s_orig_AcceptConnection = nullptr;
static fn_CloseConnection_t s_orig_CloseConnection = nullptr;
static fn_SteamNetworkingSockets_v008_t s_orig_SteamNetworkingSockets_v008 = nullptr;
static fn_SteamNetworkingUtils_v003_t s_orig_SteamNetworkingUtils_v003 = nullptr;

// Exact Valve Steamworks SDK ABI Layout matching isteamnetworkingsockets.h and steamnetworkingtypes.h
#pragma pack(push, 8)
struct SteamNetworkingIdentity_t {
    int m_eType;
    int m_cbSize;
    union {
        uint64_t m_steamID64;
        char m_szGenericString[32];
        uint8_t m_reserved[128];
    };
};
static_assert(sizeof(SteamNetworkingIdentity_t) == 136, "SteamNetworkingIdentity_t ABI size mismatch");

struct SteamNetworkingIPAddr_t {
    uint8_t m_ipv6[16];
    uint16_t m_port;
};
static_assert(sizeof(SteamNetworkingIPAddr_t) == 18, "SteamNetworkingIPAddr_t ABI size mismatch");

struct SteamNetConnectionInfo_t {
    SteamNetworkingIdentity_t m_identityRemote;         // 136 bytes (offset 0)
    int64_t                   m_nUserData;              // 8 bytes   (offset 136)
    uint32_t                  m_hListenSocket;          // 4 bytes   (offset 144)
    SteamNetworkingIPAddr_t   m_addrRemote;             // 18 bytes  (offset 148)
    uint16_t                  m__pad1;                  // 2 bytes   (offset 166)
    uint32_t                  m_idPOPRemote;            // 4 bytes   (offset 168)
    uint32_t                  m_idPOPRelay;             // 4 bytes   (offset 172)
    int32_t                   m_eState;                 // 4 bytes   (offset 176)
    int32_t                   m_eEndReason;             // 4 bytes   (offset 180)
    char                      m_szEndDebug[128];        // 128 bytes (offset 184)
    char                      m_szConnectionDescription[128]; // 128 bytes (offset 312)
    int32_t                   m_nFlags;                 // 4 bytes   (offset 440)
    uint32_t                  reserved[63];             // 252 bytes (offset 444)
};
static_assert(sizeof(SteamNetConnectionInfo_t) == 696, "SteamNetConnectionInfo_t ABI size mismatch");

struct SteamNetConnectionStatusChangedCallback_t {
    enum { k_iCallback = 1221 };
    uint32_t m_hConn;                                   // 4 bytes   (offset 0, padded to 8)
    SteamNetConnectionInfo_t m_info;                    // 696 bytes (offset 8)
    int32_t m_eOldState;                                // 4 bytes   (offset 704)
};
static_assert(sizeof(SteamNetConnectionStatusChangedCallback_t) == 712, "SteamNetConnectionStatusChangedCallback_t ABI size mismatch");
#pragma pack(pop)

static const char* GetConnectionStateString(int state) {
    switch (state) {
        case 0: return "None";
        case 1: return "Connecting";
        case 2: return "FindingRoute";
        case 3: return "Connected";
        case 4: return "ClosedByPeer";
        case 5: return "ProblemDetectedLocally";
        case -1: return "FinWait";
        case -2: return "Linger";
        case -3: return "Dead";
        default: return "Unknown";
    }
}

static const char* GetEndReasonString(int reason) {
    switch (reason) {
        case 0: return "None";
        case 1000: return "App_Min";
        case 1999: return "App_Max";
        case 2000: return "AppException_Min";
        case 3001: return "Local_OfflineMode";
        case 3002: return "Local_ManyRelayConnectivity";
        case 3003: return "Local_HostedServerPrimaryRelay";
        case 3004: return "Local_NetworkConfig";
        case 3005: return "Local_Rights";
        case 3006: return "Local_P2P_ICE_NoPublicAddresses";
        case 4001: return "Remote_Timeout";
        case 4002: return "Remote_BadCrypt";
        case 4003: return "Remote_BadCert";
        case 4006: return "Remote_BadProtocolVersion";
        case 5001: return "Misc_Generic";
        case 5002: return "Misc_InternalError";
        case 5003: return "Misc_Timeout";
        case 5004: return "Misc_SteamConnectivity";
        case 5005: return "Misc_NoRelaySessionsToClient";
        case 5006: return "Misc_P2P_Rendezvous";
        case 5007: return "Misc_P2P_NAT_Firewall";
        case 5008: return "Misc_PeerSentNoConnection";
        default: return "Other";
    }
}

static std::string GetIniPath() {
    char path[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string s(path);
    size_t pos = s.find_last_of("\\/");
    std::string dir = (pos != std::string::npos ? s.substr(0, pos + 1) : ".\\");
    std::string ini = dir + "ReFix.ini";
    if (GetFileAttributesA(ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // Fallback: check plugins dir
        char modPath[MAX_PATH] = { 0 };
        HMODULE hSelf = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&GetIniPath, &hSelf);
        if (hSelf) {
            GetModuleFileNameA(hSelf, modPath, MAX_PATH);
            std::string ms(modPath);
            size_t mpos = ms.find_last_of("\\/");
            if (mpos != std::string::npos) {
                std::string fallback = ms.substr(0, mpos + 1) + "ReFix.ini";
                if (GetFileAttributesA(fallback.c_str()) != INVALID_FILE_ATTRIBUTES) return fallback;
            }
        }
    }
    return ini;
}

void Initialize() {
    if (s_initialized.exchange(true)) return;

    std::string ini = GetIniPath();
    char buf[64] = { 0 };

    GetPrivateProfileStringA("NetCompat", "EnableCompat", "true", buf, sizeof(buf), ini.c_str());
    bool enableCompat = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0 || _stricmp(buf, "yes") == 0);
    s_configEnableCompat.store(enableCompat);

    GetPrivateProfileStringA("NetCompat", "VerboseLog", "true", buf, sizeof(buf), ini.c_str());
    bool verboseLog = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0 || _stricmp(buf, "yes") == 0);
    s_configVerboseLog.store(verboseLog);

    if (s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] initialization");
    }
}

void OnSteamInitialized(bool isGoldbergMode, uint32_t maskAppId, uint32_t realAppId) {
    Initialize();

    s_isGoldbergMode.store(isGoldbergMode);
    s_maskAppId.store(maskAppId);
    s_realAppId.store(realAppId);

    // Rule 1: Non-Valve / Goldberg mode -> compatibility not required
    if (isGoldbergMode) {
        if (s_configVerboseLog.load(std::memory_order_relaxed)) {
            ReFixLog("[NetCompat] compatibility not required (Goldberg LAN mode)");
            ReFixLog("[NetCompat] no action");
        }
        s_active.store(false);
        return;
    }

    // Rule 2: MaskAppId == 0 or RealAppId == 0 -> no fake/masked AppID in use
    if (maskAppId == 0 || realAppId == 0) {
        if (s_configVerboseLog.load(std::memory_order_relaxed)) {
            ReFixLog("[NetCompat] compatibility not required (no fake/masked AppID detected)");
            ReFixLog("[NetCompat] no action");
        }
        s_active.store(false);
        return;
    }

    // Rule 3: MaskAppId == RealAppId -> AppID is already matching genuine app
    if (maskAppId == realAppId) {
        if (s_configVerboseLog.load(std::memory_order_relaxed)) {
            ReFixLog("[NetCompat] compatibility not required (MaskAppId %u matches RealAppId %u)", maskAppId, realAppId);
            ReFixLog("[NetCompat] no action");
        }
        s_active.store(false);
        return;
    }

    // Rule 4: User disabled in config
    if (!s_configEnableCompat.load(std::memory_order_relaxed)) {
        if (s_configVerboseLog.load(std::memory_order_relaxed)) {
            ReFixLog("[NetCompat] compatibility disabled via ReFix.ini [NetCompat] EnableCompat=false");
            ReFixLog("[NetCompat] no action");
        }
        s_active.store(false);
        return;
    }

    // Technical conditions satisfied:
    // Online/Valve Mode + MaskAppId != RealAppId (fake/masked AppID active)
    s_active.store(true);
    if (s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] relevant networking path detected (Online/Valve with MaskAppId=%u, RealAppId=%u)",
                 maskAppId, realAppId);
        ReFixLog("[NetCompat] compatibility required");
        ReFixLog("[NetCompat] compatibility enabled");
    }
}

void NotifySocketsPathDetected() {
    if (!IsActive()) return;
    if (!s_pathDetected.exchange(true)) {
        if (s_configVerboseLog.load(std::memory_order_relaxed)) {
            ReFixLog("[NetCompat] SteamNetworkingSockets subsystem accessed by application");
        }
    }
}

void ProcessConnectionStatusChanged(void* pubParam, int cubParam) {
    if (!IsActive()) return;
    if (!pubParam || cubParam < (int)sizeof(SteamNetConnectionStatusChangedCallback_t)) return;

    auto* cb = static_cast<const SteamNetConnectionStatusChangedCallback_t*>(pubParam);
    uint32_t conn = cb->m_hConn;
    int state = cb->m_info.m_eState;
    int oldState = cb->m_eOldState;
    int endReason = cb->m_info.m_eEndReason;
    const char* pszDebug = cb->m_info.m_szEndDebug;

    if (s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] Connection status changed: conn=%u, oldState=%s(%d), newState=%s(%d), reason=%s(%d), debug='%s'",
                 conn,
                 GetConnectionStateString(oldState), oldState,
                 GetConnectionStateString(state), state,
                 GetEndReasonString(endReason), endReason,
                 pszDebug ? pszDebug : "");
    }

    // Diagnostic assistance for peer rejection
    if (state == 4 /* ClosedByPeer */ || state == 5 /* ProblemDetectedLocally */) {
        if (endReason == 4003 /* k_ESteamNetConnectionEnd_Remote_BadCert */) {
            ReFixLog("[NetCompat] [DIAGNOSTIC] Peer connection rejected with BadCert (4003): "
                     "In Valve Online mode, Valve CA issues certs for MaskAppId (%u). "
                     "Ensure connecting clients run under the exact same MaskAppId (%u).",
                     s_maskAppId.load(std::memory_order_relaxed), s_maskAppId.load(std::memory_order_relaxed));
        } else if (endReason == 4002 /* k_ESteamNetConnectionEnd_Remote_BadCrypt */) {
            ReFixLog("[NetCompat] [DIAGNOSTIC] Peer connection rejected with BadCrypt (4002): "
                     "Cryptographic handshake failed or self-connection attempted.");
        }
    }
}

bool IsActive() {
    return s_active.load(std::memory_order_relaxed);
}

bool IsVerbose() {
    return s_configVerboseLog.load(std::memory_order_relaxed);
}

uint32_t Intercept_CreateListenSocketP2P(void* self, int nLocalVirtualPort, int nNumOptions, const void* pOptions) {
    if (!s_orig_CreateListenSocketP2P) {
        if (g_hOriginalDll) {
            s_orig_CreateListenSocketP2P = (fn_CreateListenSocketP2P_t)GetProcAddress(g_hOriginalDll, "SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P");
        }
    }
    if (!IsActive()) {
        return s_orig_CreateListenSocketP2P ? s_orig_CreateListenSocketP2P(self, nLocalVirtualPort, nNumOptions, pOptions) : 0;
    }
    NotifySocketsPathDetected();
    uint32_t handle = 0;
    if (s_orig_CreateListenSocketP2P) {
        handle = s_orig_CreateListenSocketP2P(self, nLocalVirtualPort, nNumOptions, pOptions);
    }
    if (s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] CreateListenSocketP2P(vport=%d, nOptions=%d) -> handle=%u",
                 nLocalVirtualPort, nNumOptions, handle);
    }
    return handle;
}

uint32_t Intercept_ConnectP2P(void* self, const void* pIdentityRemote, int nRemoteVirtualPort, int nNumOptions, const void* pOptions) {
    if (!s_orig_ConnectP2P) {
        if (g_hOriginalDll) {
            s_orig_ConnectP2P = (fn_ConnectP2P_t)GetProcAddress(g_hOriginalDll, "SteamAPI_ISteamNetworkingSockets_ConnectP2P");
        }
    }
    if (!IsActive()) {
        return s_orig_ConnectP2P ? s_orig_ConnectP2P(self, pIdentityRemote, nRemoteVirtualPort, nNumOptions, pOptions) : 0;
    }
    NotifySocketsPathDetected();
    uint64_t targetSteamId = 0;
    if (pIdentityRemote) {
        auto* id = static_cast<const SteamNetworkingIdentity_t*>(pIdentityRemote);
        targetSteamId = id->m_steamID64;
    }
    uint32_t handle = 0;
    if (s_orig_ConnectP2P) {
        handle = s_orig_ConnectP2P(self, pIdentityRemote, nRemoteVirtualPort, nNumOptions, pOptions);
    }
    if (s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] ConnectP2P(target=%llu, vport=%d, nOptions=%d) -> conn=%u",
                 targetSteamId, nRemoteVirtualPort, nNumOptions, handle);
    }
    return handle;
}

int Intercept_AcceptConnection(void* self, uint32_t hConn) {
    if (!s_orig_AcceptConnection) {
        if (g_hOriginalDll) {
            s_orig_AcceptConnection = (fn_AcceptConnection_t)GetProcAddress(g_hOriginalDll, "SteamAPI_ISteamNetworkingSockets_AcceptConnection");
        }
    }
    if (!IsActive()) {
        return s_orig_AcceptConnection ? s_orig_AcceptConnection(self, hConn) : 2 /* k_EResultFail */;
    }
    NotifySocketsPathDetected();
    int res = 2; // k_EResultFail
    if (s_orig_AcceptConnection) {
        res = s_orig_AcceptConnection(self, hConn);
    }
    if (s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] AcceptConnection(conn=%u) -> result=%d", hConn, res);
    }
    return res;
}

bool Intercept_CloseConnection(void* self, uint32_t hConn, int nReason, const char* pszDebug, bool bEnableLinger) {
    if (!s_orig_CloseConnection) {
        if (g_hOriginalDll) {
            s_orig_CloseConnection = (fn_CloseConnection_t)GetProcAddress(g_hOriginalDll, "SteamAPI_ISteamNetworkingSockets_CloseConnection");
        }
    }
    if (!IsActive()) {
        return s_orig_CloseConnection ? s_orig_CloseConnection(self, hConn, nReason, pszDebug, bEnableLinger) : false;
    }
    NotifySocketsPathDetected();
    bool res = false;
    if (s_orig_CloseConnection) {
        res = s_orig_CloseConnection(self, hConn, nReason, pszDebug, bEnableLinger);
    }
    if (s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] CloseConnection(conn=%u, reason=%d ('%s'), linger=%d) -> res=%d",
                 hConn, nReason, pszDebug ? pszDebug : "", bEnableLinger ? 1 : 0, res ? 1 : 0);
    }
    return res;
}

void* Intercept_SteamNetworkingSockets_v008() {
    if (!s_orig_SteamNetworkingSockets_v008) {
        if (g_hOriginalDll) {
            s_orig_SteamNetworkingSockets_v008 = (fn_SteamNetworkingSockets_v008_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingSockets_v008");
        }
    }
    void* ptr = nullptr;
    if (s_orig_SteamNetworkingSockets_v008) {
        ptr = s_orig_SteamNetworkingSockets_v008();
    }
    // Fail-safe fallback if original DLL used newer or flat interface versioning
    if (!ptr && g_hOriginalDll) {
        auto pfnV12 = (fn_SteamNetworkingSockets_v008_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingSockets_SteamAPI_v012");
        if (pfnV12) ptr = pfnV12();
        if (!ptr) {
            auto pfnOld = (fn_SteamNetworkingSockets_v008_t)GetProcAddress(g_hOriginalDll, "SteamNetworkingSockets");
            if (pfnOld) ptr = pfnOld();
        }
    }
    if (IsActive()) {
        NotifySocketsPathDetected();
        if (s_configVerboseLog.load(std::memory_order_relaxed)) {
            ReFixLog("[NetCompat] SteamAPI_SteamNetworkingSockets_v008 resolved -> %p", ptr);
        }
    }
    return ptr;
}

void* Intercept_SteamNetworkingUtils_v003() {
    if (!s_orig_SteamNetworkingUtils_v003) {
        if (g_hOriginalDll) {
            s_orig_SteamNetworkingUtils_v003 = (fn_SteamNetworkingUtils_v003_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingUtils_v003");
        }
    }
    void* ptr = nullptr;
    if (s_orig_SteamNetworkingUtils_v003) {
        ptr = s_orig_SteamNetworkingUtils_v003();
    }
    // Fail-safe fallback
    if (!ptr && g_hOriginalDll) {
        auto pfnV4 = (fn_SteamNetworkingUtils_v003_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004");
        if (pfnV4) ptr = pfnV4();
        if (!ptr) {
            auto pfnOld = (fn_SteamNetworkingUtils_v003_t)GetProcAddress(g_hOriginalDll, "SteamNetworkingUtils");
            if (pfnOld) ptr = pfnOld();
        }
    }
    if (IsActive()) {
        NotifySocketsPathDetected();
        if (s_configVerboseLog.load(std::memory_order_relaxed)) {
            ReFixLog("[NetCompat] SteamAPI_SteamNetworkingUtils_v003 resolved -> %p", ptr);
        }
    }
    return ptr;
}

void Shutdown() {
    s_active.store(false);
    s_pathDetected.store(false);
    if (s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] shutdown");
    }
}

} // namespace SteamNetCompat
