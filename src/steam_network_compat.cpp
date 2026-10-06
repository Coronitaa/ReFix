// =============================================================================
// ReFix - Steam Networking Compatibility Module (NetCompat)
// steam_network_compat.cpp - Generic compatibility for SteamNetworkingSockets
// =============================================================================
#include "steam_network_compat.h"
#include <isteamnetworkingsockets.h>
#include <cstdio>
#include <atomic>
#include <string>
#include <cstdint>
#include <cassert>

extern void ReFixLog(const char* fmt, ...);
extern FARPROC g_steamProcs[];
extern HMODULE g_hOriginalDll;

namespace SteamNetCompat {

static_assert(sizeof(SteamNetConnectionStatusChangedCallback_t) == 712,
              "SteamNetConnectionStatusChangedCallback_t ABI size mismatch with SDK");

static std::atomic<bool> s_initialized{ false };
static std::atomic<bool> s_compatEligible{ false };
static std::atomic<bool> s_active{ false };
static std::atomic<bool> s_isGoldbergMode{ false };
static std::atomic<uint32_t> s_maskAppId{ 0 };
static std::atomic<uint32_t> s_realAppId{ 0 };
static std::atomic<bool> s_pathDetected{ false };

static std::atomic<bool> s_configEnableCompat{ true };
static std::atomic<bool> s_configVerboseLog{ false }; // Disabled by default for maximum performance

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

static const char* GetConnectionStateString(int state) {
    switch (state) {
        case k_ESteamNetworkingConnectionState_None: return "None";
        case k_ESteamNetworkingConnectionState_Connecting: return "Connecting";
        case k_ESteamNetworkingConnectionState_FindingRoute: return "FindingRoute";
        case k_ESteamNetworkingConnectionState_Connected: return "Connected";
        case k_ESteamNetworkingConnectionState_ClosedByPeer: return "ClosedByPeer";
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: return "ProblemDetectedLocally";
        case k_ESteamNetworkingConnectionState_FinWait: return "FinWait";
        case k_ESteamNetworkingConnectionState_Linger: return "Linger";
        case k_ESteamNetworkingConnectionState_Dead: return "Dead";
        default: return "Unknown";
    }
}

static const char* GetEndReasonString(int reason) {
    switch (reason) {
        case 0: return "None";
        case 1: return "App_Generic";
        case 1000: return "AppMin";
        case 1999: return "AppMax";
        case 2001: return "Local_Offline";
        case 2002: return "Local_ManyRelayConnectivity";
        case 2003: return "Local_HostedServerPrimaryReserveTimeout";
        case 3001: return "Remote_Timeout";
        case 3002: return "Remote_BadCrypt";
        case 3003: return "Remote_BadCert";
        case 4001: return "Misc_Timeout";
        case 4002: return "Misc_BadCrypt";
        case 4003: return "Misc_BadCert";
        default: return "Other/Unmapped";
    }
}

static std::string GetIniPath() {
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string s(exePath);
    size_t pos = s.find_last_of("\\/");
    std::string dir = (pos != std::string::npos) ? s.substr(0, pos + 1) : "";
    std::string ini = dir + "ReFix.ini";

    if (GetFileAttributesA(ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HMODULE hSelf = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&Initialize, &hSelf);
        if (hSelf) {
            char modPath[MAX_PATH] = { 0 };
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

    GetPrivateProfileStringA("NetCompat", "VerboseLog", "false", buf, sizeof(buf), ini.c_str());
    bool verboseLog = (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0 || _stricmp(buf, "yes") == 0);
    s_configVerboseLog.store(verboseLog);

    ReFixLog("[NetCompat] initialization");
}

void OnSteamInitialized(bool isGoldbergMode, uint32_t maskAppId, uint32_t realAppId) {
    Initialize();

    s_isGoldbergMode.store(isGoldbergMode);
    s_maskAppId.store(maskAppId);
    s_realAppId.store(realAppId);
    s_compatEligible.store(false);
    s_active.store(false);
    s_pathDetected.store(false);

    // Rule 1: Non-Valve / Goldberg mode -> compatibility not required
    if (isGoldbergMode) {
        ReFixLog("[NetCompat] compatibility not required (Goldberg LAN mode)");
        ReFixLog("[NetCompat] no action");
        return;
    }

    // Rule 2: MaskAppId == 0 or RealAppId == 0 -> no fake/masked AppID in use
    if (maskAppId == 0 || realAppId == 0) {
        ReFixLog("[NetCompat] compatibility not required (no fake/masked AppID detected)");
        ReFixLog("[NetCompat] no action");
        return;
    }

    // Rule 3: MaskAppId == RealAppId -> AppID is already matching genuine app
    if (maskAppId == realAppId) {
        ReFixLog("[NetCompat] compatibility not required (MaskAppId %u matches RealAppId %u)", maskAppId, realAppId);
        ReFixLog("[NetCompat] no action");
        return;
    }

    // Rule 4: User disabled in config
    if (!s_configEnableCompat.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] compatibility disabled via ReFix.ini [NetCompat] EnableCompat=false");
        ReFixLog("[NetCompat] no action");
        return;
    }

    // Technical conditions satisfied for eligibility:
    // Online/Valve Mode + MaskAppId != RealAppId (fake/masked AppID active)
    s_compatEligible.store(true);
    ReFixLog("[NetCompat] relevant networking path detected (Online/Valve with MaskAppId=%u, RealAppId=%u)",
             maskAppId, realAppId);
    ReFixLog("[NetCompat] compatibility eligible (awaiting SteamNetworkingSockets access)");
}

void NotifySocketsPathDetected() {
    if (!s_compatEligible.load(std::memory_order_relaxed)) return;

    if (!s_pathDetected.exchange(true)) {
        s_active.store(true);
        ReFixLog("[NetCompat] SteamNetworkingSockets subsystem accessed by application");
        ReFixLog("[NetCompat] compatibility required");
        ReFixLog("[NetCompat] compatibility enabled");
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

    // Diagnostic assistance for peer rejection (always logged on error)
    if (state == k_ESteamNetworkingConnectionState_ClosedByPeer ||
        state == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
        if (endReason == 4003 /* k_ESteamNetConnectionEnd_Remote_BadCert / Misc_BadCert */) {
            ReFixLog("[NetCompat] [DIAGNOSTIC] Peer connection rejected with BadCert (4003): "
                     "In Valve Online mode, Valve CA issues certs for MaskAppId (%u). "
                     "Ensure connecting clients run under the exact same MaskAppId (%u).",
                     s_maskAppId.load(std::memory_order_relaxed), s_maskAppId.load(std::memory_order_relaxed));
        } else if (endReason == 4002 /* k_ESteamNetConnectionEnd_Remote_BadCrypt / Misc_BadCrypt */) {
            ReFixLog("[NetCompat] [DIAGNOSTIC] Peer connection rejected with BadCrypt (4002): "
                     "Cryptographic handshake failed or self-connection attempted.");
        }
    }
}

bool IsEligible() {
    return s_compatEligible.load(std::memory_order_relaxed);
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
    NotifySocketsPathDetected();
    if (!IsActive()) {
        return s_orig_CreateListenSocketP2P ? s_orig_CreateListenSocketP2P(self, nLocalVirtualPort, nNumOptions, pOptions) : 0;
    }
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
    NotifySocketsPathDetected();
    if (!IsActive()) {
        return s_orig_ConnectP2P ? s_orig_ConnectP2P(self, pIdentityRemote, nRemoteVirtualPort, nNumOptions, pOptions) : 0;
    }
    uint64_t targetSteamId = 0;
    if (pIdentityRemote) {
        auto* id = static_cast<const SteamNetworkingIdentity*>(pIdentityRemote);
        targetSteamId = id->GetSteamID64();
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
    NotifySocketsPathDetected();
    if (!IsActive()) {
        return s_orig_AcceptConnection ? s_orig_AcceptConnection(self, hConn) : 2 /* k_EResultFail */;
    }
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
    NotifySocketsPathDetected();
    if (!IsActive()) {
        return s_orig_CloseConnection ? s_orig_CloseConnection(self, hConn, nReason, pszDebug, bEnableLinger) : false;
    }
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
    NotifySocketsPathDetected();
    if (IsActive() && s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] SteamAPI_SteamNetworkingSockets_v008 resolved -> %p", ptr);
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
    NotifySocketsPathDetected();
    if (IsActive() && s_configVerboseLog.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] SteamAPI_SteamNetworkingUtils_v003 resolved -> %p", ptr);
    }
    return ptr;
}

void Shutdown() {
    if (s_active.load(std::memory_order_relaxed) || s_compatEligible.load(std::memory_order_relaxed)) {
        ReFixLog("[NetCompat] shutdown");
    }
    s_initialized.store(false);
    s_compatEligible.store(false);
    s_active.store(false);
    s_pathDetected.store(false);
    s_maskAppId.store(0);
    s_realAppId.store(0);
    s_isGoldbergMode.store(false);

    s_orig_CreateListenSocketP2P = nullptr;
    s_orig_ConnectP2P = nullptr;
    s_orig_AcceptConnection = nullptr;
    s_orig_CloseConnection = nullptr;
    s_orig_SteamNetworkingSockets_v008 = nullptr;
    s_orig_SteamNetworkingUtils_v003 = nullptr;
}

} // namespace SteamNetCompat
