// =============================================================================
// ReFix - ISteamNetworkingSockets Flat C API Implementation
// =============================================================================
// Exposes the 47 flat SteamAPI_ISteamNetworkingSockets_* and key
// SteamAPI_ISteamNetworkingUtils_* exports used by Godot (GodotSteam)
// and Unity (FishySteamworks / Facepunch.Steamworks) when running in LAN/Offline mode.
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define STEAM_WIN32 1
#define STEAM_API_NODLL 1
#include "include/steam/steam_api.h"
#include "include/steam/isteamnetworkingsockets.h"
#include "include/steam/isteamnetworkingutils.h"
#include "include/steam/steamnetworkingtypes.h"
#include "unreal_steam_emu.h"

static inline ISteamNetworkingSockets* ResolveSockets(void* self) {
    if (self) return reinterpret_cast<ISteamNetworkingSockets*>(self);
    return reinterpret_cast<ISteamNetworkingSockets*>(UnrealSteamEmu::GetSteamNetworkingSockets());
}

static inline ISteamNetworkingUtils* ResolveUtils(void* self) {
    if (self) return reinterpret_cast<ISteamNetworkingUtils*>(self);
    return reinterpret_cast<ISteamNetworkingUtils*>(UnrealSteamEmu::GetSteamNetworkingUtils());
}

extern "C" {

__declspec(dllexport) HSteamListenSocket SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP(void* self, const SteamNetworkingIPAddr* localAddress, int nOptions, const SteamNetworkingConfigValue_t* pOptions) {
    auto* s = ResolveSockets(self);
    return s ? s->CreateListenSocketIP(localAddress ? *localAddress : SteamNetworkingIPAddr{}, nOptions, pOptions) : k_HSteamListenSocket_Invalid;
}

__declspec(dllexport) HSteamNetConnection SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress(void* self, const SteamNetworkingIPAddr* address, int nOptions, const SteamNetworkingConfigValue_t* pOptions) {
    auto* s = ResolveSockets(self);
    return s ? s->ConnectByIPAddress(address ? *address : SteamNetworkingIPAddr{}, nOptions, pOptions) : k_HSteamNetConnection_Invalid;
}

__declspec(dllexport) HSteamListenSocket SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P(void* self, int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t* pOptions) {
    auto* s = ResolveSockets(self);
    return s ? s->CreateListenSocketP2P(nLocalVirtualPort, nOptions, pOptions) : k_HSteamListenSocket_Invalid;
}

__declspec(dllexport) HSteamNetConnection SteamAPI_ISteamNetworkingSockets_ConnectP2P(void* self, const SteamNetworkingIdentity* identityRemote, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t* pOptions) {
    auto* s = ResolveSockets(self);
    return s ? s->ConnectP2P(identityRemote ? *identityRemote : SteamNetworkingIdentity{}, nRemoteVirtualPort, nOptions, pOptions) : k_HSteamNetConnection_Invalid;
}

__declspec(dllexport) EResult SteamAPI_ISteamNetworkingSockets_AcceptConnection(void* self, HSteamNetConnection hConn) {
    auto* s = ResolveSockets(self);
    return s ? s->AcceptConnection(hConn) : k_EResultFail;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_CloseConnection(void* self, HSteamNetConnection hPeer, int nReason, const char* pszDebug, bool bEnableLinger) {
    auto* s = ResolveSockets(self);
    return s ? s->CloseConnection(hPeer, nReason, pszDebug, bEnableLinger) : false;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_CloseListenSocket(void* self, HSteamListenSocket hSocket) {
    auto* s = ResolveSockets(self);
    return s ? s->CloseListenSocket(hSocket) : false;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_SetConnectionUserData(void* self, HSteamNetConnection hPeer, int64_t nUserData) {
    auto* s = ResolveSockets(self);
    return s ? s->SetConnectionUserData(hPeer, nUserData) : false;
}

__declspec(dllexport) int64_t SteamAPI_ISteamNetworkingSockets_GetConnectionUserData(void* self, HSteamNetConnection hPeer) {
    auto* s = ResolveSockets(self);
    return s ? s->GetConnectionUserData(hPeer) : -1;
}

__declspec(dllexport) void SteamAPI_ISteamNetworkingSockets_SetConnectionName(void* self, HSteamNetConnection hPeer, const char* pszName) {
    auto* s = ResolveSockets(self);
    if (s) s->SetConnectionName(hPeer, pszName);
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_GetConnectionName(void* self, HSteamNetConnection hPeer, char* pszName, int nMaxLen) {
    auto* s = ResolveSockets(self);
    return s ? s->GetConnectionName(hPeer, pszName, nMaxLen) : false;
}

__declspec(dllexport) EResult SteamAPI_ISteamNetworkingSockets_SendMessageToConnection(void* self, HSteamNetConnection hConn, const void* pData, uint32_t cbData, int nSendFlags, int64_t* pOutMessageNumber) {
    auto* s = ResolveSockets(self);
    return s ? s->SendMessageToConnection(hConn, pData, cbData, nSendFlags, pOutMessageNumber) : k_EResultFail;
}

__declspec(dllexport) void SteamAPI_ISteamNetworkingSockets_SendMessages(void* self, int nMessages, SteamNetworkingMessage_t* const* pMessages, int64_t* pOutMessageNumberOrResult) {
    auto* s = ResolveSockets(self);
    if (s) s->SendMessages(nMessages, pMessages, pOutMessageNumberOrResult);
}

__declspec(dllexport) EResult SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection(void* self, HSteamNetConnection hConn) {
    auto* s = ResolveSockets(self);
    return s ? s->FlushMessagesOnConnection(hConn) : k_EResultFail;
}

__declspec(dllexport) int SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection(void* self, HSteamNetConnection hConn, SteamNetworkingMessage_t** ppOutMessages, int nMaxMessages) {
    auto* s = ResolveSockets(self);
    return s ? s->ReceiveMessagesOnConnection(hConn, ppOutMessages, nMaxMessages) : -1;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_GetConnectionInfo(void* self, HSteamNetConnection hConn, SteamNetConnectionInfo_t* pInfo) {
    auto* s = ResolveSockets(self);
    return s ? s->GetConnectionInfo(hConn, pInfo) : false;
}

__declspec(dllexport) EResult SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus(void* self, HSteamNetConnection hConn, SteamNetConnectionRealTimeStatus_t* pStatus, int nLanes, SteamNetConnectionRealTimeLaneStatus_t* pLanes) {
    auto* s = ResolveSockets(self);
    return s ? s->GetConnectionRealTimeStatus(hConn, pStatus, nLanes, pLanes) : k_EResultFail;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_GetQuickConnectionStatus(void* self, HSteamNetConnection hConn, SteamNetworkingQuickConnectionStatus* pStats) {
    return false;
}

__declspec(dllexport) int SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus(void* self, HSteamNetConnection hConn, char* pszBuf, int cbBuf) {
    auto* s = ResolveSockets(self);
    return s ? s->GetDetailedConnectionStatus(hConn, pszBuf, cbBuf) : -1;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress(void* self, HSteamListenSocket hSocket, SteamNetworkingIPAddr* address) {
    auto* s = ResolveSockets(self);
    return s ? s->GetListenSocketAddress(hSocket, address) : false;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_CreateSocketPair(void* self, HSteamNetConnection* pOutConnection1, HSteamNetConnection* pOutConnection2, bool bUseNetworkLoopback, const SteamNetworkingIdentity* pIdentity1, const SteamNetworkingIdentity* pIdentity2) {
    auto* s = ResolveSockets(self);
    return s ? s->CreateSocketPair(pOutConnection1, pOutConnection2, bUseNetworkLoopback, pIdentity1, pIdentity2) : false;
}

__declspec(dllexport) EResult SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes(void* self, HSteamNetConnection hConn, int nNumLanes, const int* pLanePriorities, const uint16_t* pLaneWeights) {
    auto* s = ResolveSockets(self);
    return s ? s->ConfigureConnectionLanes(hConn, nNumLanes, pLanePriorities, pLaneWeights) : k_EResultFail;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_GetIdentity(void* self, SteamNetworkingIdentity* pIdentity) {
    auto* s = ResolveSockets(self);
    return s ? s->GetIdentity(pIdentity) : false;
}

__declspec(dllexport) ESteamNetworkingAvailability SteamAPI_ISteamNetworkingSockets_InitAuthentication(void* self) {
    auto* s = ResolveSockets(self);
    return s ? s->InitAuthentication() : k_ESteamNetworkingAvailability_CannotTry;
}

__declspec(dllexport) ESteamNetworkingAvailability SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus(void* self, SteamNetAuthenticationStatus_t* pDetails) {
    auto* s = ResolveSockets(self);
    return s ? s->GetAuthenticationStatus(pDetails) : k_ESteamNetworkingAvailability_CannotTry;
}

__declspec(dllexport) HSteamNetPollGroup SteamAPI_ISteamNetworkingSockets_CreatePollGroup(void* self) {
    auto* s = ResolveSockets(self);
    return s ? s->CreatePollGroup() : k_HSteamNetPollGroup_Invalid;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_DestroyPollGroup(void* self, HSteamNetPollGroup hPollGroup) {
    auto* s = ResolveSockets(self);
    return s ? s->DestroyPollGroup(hPollGroup) : false;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup(void* self, HSteamNetConnection hConn, HSteamNetPollGroup hPollGroup) {
    auto* s = ResolveSockets(self);
    return s ? s->SetConnectionPollGroup(hConn, hPollGroup) : false;
}

__declspec(dllexport) int SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup(void* self, HSteamNetPollGroup hPollGroup, SteamNetworkingMessage_t** ppOutMessages, int nMaxMessages) {
    auto* s = ResolveSockets(self);
    return s ? s->ReceiveMessagesOnPollGroup(hPollGroup, ppOutMessages, nMaxMessages) : -1;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket(void* self, const void* pvTicket, int cbTicket, SteamDatagramRelayAuthTicket* pOutParsedTicket) {
    auto* s = ResolveSockets(self);
    return s ? s->ReceivedRelayAuthTicket(pvTicket, cbTicket, pOutParsedTicket) : false;
}

__declspec(dllexport) int SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer(void* self, const SteamNetworkingIdentity* identityGameServer, int nRemoteVirtualPort, SteamDatagramRelayAuthTicket* pOutParsedTicket) {
    auto* s = ResolveSockets(self);
    return s ? s->FindRelayAuthTicketForServer(identityGameServer ? *identityGameServer : SteamNetworkingIdentity{}, nRemoteVirtualPort, pOutParsedTicket) : 0;
}

__declspec(dllexport) HSteamNetConnection SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer(void* self, const SteamNetworkingIdentity* identityTarget, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t* pOptions) {
    auto* s = ResolveSockets(self);
    return s ? s->ConnectToHostedDedicatedServer(identityTarget ? *identityTarget : SteamNetworkingIdentity{}, nRemoteVirtualPort, nOptions, pOptions) : k_HSteamNetConnection_Invalid;
}

__declspec(dllexport) uint16_t SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort(void* self) {
    auto* s = ResolveSockets(self);
    return s ? s->GetHostedDedicatedServerPort() : 0;
}

__declspec(dllexport) SteamNetworkingPOPID SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID(void* self) {
    auto* s = ResolveSockets(self);
    return s ? s->GetHostedDedicatedServerPOPID() : 0;
}

__declspec(dllexport) EResult SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress(void* self, SteamDatagramHostedAddress* pRouting) {
    auto* s = ResolveSockets(self);
    return s ? s->GetHostedDedicatedServerAddress(pRouting) : k_EResultFail;
}

__declspec(dllexport) HSteamListenSocket SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket(void* self, int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t* pOptions) {
    auto* s = ResolveSockets(self);
    return s ? s->CreateHostedDedicatedServerListenSocket(nLocalVirtualPort, nOptions, pOptions) : k_HSteamListenSocket_Invalid;
}

__declspec(dllexport) EResult SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin(void* self, SteamDatagramGameCoordinatorServerLogin* pLoginInfo, int* pcbSignedBlob, void* pBlob) {
    auto* s = ResolveSockets(self);
    return s ? s->GetGameCoordinatorServerLogin(pLoginInfo, pcbSignedBlob, pBlob) : k_EResultFail;
}

__declspec(dllexport) HSteamNetConnection SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling(void* self, ISteamNetworkingConnectionSignaling* pSignaling, const SteamNetworkingIdentity* pPeerIdentity, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t* pOptions) {
    auto* s = ResolveSockets(self);
    return s ? s->ConnectP2PCustomSignaling(pSignaling, pPeerIdentity, nRemoteVirtualPort, nOptions, pOptions) : k_HSteamNetConnection_Invalid;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal(void* self, const void* pMsg, int cbMsg, ISteamNetworkingSignalingRecvContext* pContext) {
    auto* s = ResolveSockets(self);
    return s ? s->ReceivedP2PCustomSignal(pMsg, cbMsg, pContext) : false;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_GetCertificateRequest(void* self, int* pcbBlob, void* pBlob, char* errMsg) {
    auto* s = ResolveSockets(self);
    SteamNetworkingErrMsg dummy{};
    bool res = s ? s->GetCertificateRequest(pcbBlob, pBlob, dummy) : false;
    if (errMsg) memcpy(errMsg, dummy, sizeof(dummy));
    return res;
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_SetCertificate(void* self, const void* pCertificate, int cbCertificate, char* errMsg) {
    auto* s = ResolveSockets(self);
    SteamNetworkingErrMsg dummy{};
    bool res = s ? s->SetCertificate(pCertificate, cbCertificate, dummy) : false;
    if (errMsg) memcpy(errMsg, dummy, sizeof(dummy));
    return res;
}

__declspec(dllexport) void SteamAPI_ISteamNetworkingSockets_ResetIdentity(void* self, const SteamNetworkingIdentity* pIdentity) {
    auto* s = ResolveSockets(self);
    if (s) s->ResetIdentity(pIdentity);
}

__declspec(dllexport) void SteamAPI_ISteamNetworkingSockets_RunCallbacks(void* self) {
    auto* s = ResolveSockets(self);
    if (s) s->RunCallbacks();
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP(void* self, int nNumPorts) {
    auto* s = ResolveSockets(self);
    return s ? s->BeginAsyncRequestFakeIP(nNumPorts) : false;
}

__declspec(dllexport) void SteamAPI_ISteamNetworkingSockets_GetFakeIP(void* self, int idxFirstPort, SteamNetworkingFakeIPResult_t* pInfo) {
    auto* s = ResolveSockets(self);
    if (s) s->GetFakeIP(idxFirstPort, pInfo);
}

__declspec(dllexport) HSteamListenSocket SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP(void* self, int idxFakePort, int nOptions, const SteamNetworkingConfigValue_t* pOptions) {
    auto* s = ResolveSockets(self);
    return s ? s->CreateListenSocketP2PFakeIP(idxFakePort, nOptions, pOptions) : k_HSteamListenSocket_Invalid;
}

__declspec(dllexport) EResult SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection(void* self, HSteamNetConnection hConn, SteamNetworkingIPAddr* pOutAddr) {
    auto* s = ResolveSockets(self);
    return s ? s->GetRemoteFakeIPForConnection(hConn, pOutAddr) : k_EResultFail;
}

__declspec(dllexport) ISteamNetworkingFakeUDPPort* SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort(void* self, int idxFakeServerPort) {
    auto* s = ResolveSockets(self);
    return s ? s->CreateFakeUDPPort(idxFakeServerPort) : nullptr;
}

// --- SteamAPI_ISteamNetworkingUtils Flat Wrappers ---

__declspec(dllexport) SteamNetworkingMicroseconds SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp(void* self) {
    auto* u = ResolveUtils(self);
    return u ? u->GetLocalTimestamp() : 0;
}

__declspec(dllexport) void SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction(void* self, ESteamNetworkingSocketsDebugOutputType eDetailLevel, FSteamNetworkingSocketsDebugOutput pfnFunc) {
    auto* u = ResolveUtils(self);
    if (u) u->SetDebugOutputFunction(eDetailLevel, pfnFunc);
}

__declspec(dllexport) bool SteamAPI_ISteamNetworkingUtils_SetConfigValue(void* self, ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj, ESteamNetworkingConfigDataType eDataType, const void* pArg) {
    auto* u = ResolveUtils(self);
    return u ? u->SetConfigValue(eValue, eScopeType, scopeObj, eDataType, pArg) : false;
}

__declspec(dllexport) ESteamNetworkingGetConfigValueResult SteamAPI_ISteamNetworkingUtils_GetConfigValue(void* self, ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj, ESteamNetworkingConfigDataType* pOutDataType, void* pResult, size_t* cbResult) {
    auto* u = ResolveUtils(self);
    return u ? u->GetConfigValue(eValue, eScopeType, scopeObj, pOutDataType, pResult, cbResult) : k_ESteamNetworkingGetConfigValue_BadValue;
}

__declspec(dllexport) SteamNetworkingMessage_t* SteamAPI_ISteamNetworkingUtils_AllocateMessage(void* self, int cbAllocateBuffer) {
    auto* u = ResolveUtils(self);
    return u ? u->AllocateMessage(cbAllocateBuffer) : nullptr;
}

__declspec(dllexport) ESteamNetworkingAvailability SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus(void* self, SteamRelayNetworkStatus_t* pDetails) {
    auto* u = ResolveUtils(self);
    return u ? u->GetRelayNetworkStatus(pDetails) : k_ESteamNetworkingAvailability_Current;
}

__declspec(dllexport) void SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(void* self) {
    auto* u = ResolveUtils(self);
    if (u) u->InitRelayNetworkAccess();
}

// ---------------------------------------------------------------------------------------------------
// SteamNetworkingIdentity / SteamNetworkingIPAddr helpers. C# and Godot wrappers build identities
// through these flat calls; in ReFix LAN there is no Valve DLL behind them, so they were no-ops and
// Valheim called ConnectP2P with an empty identity ("Failed to connect"). Not exported here: the LAN
// override table in steam_proxy.cpp routes the SteamAPI_* names to them.
// ---------------------------------------------------------------------------------------------------
void ReFix_Identity_Clear(SteamNetworkingIdentity* self) { if (self) self->Clear(); }
bool ReFix_Identity_IsInvalid(SteamNetworkingIdentity* self) { return !self || self->IsInvalid(); }
void ReFix_Identity_SetSteamID64(SteamNetworkingIdentity* self, uint64 id) { if (self) self->SetSteamID64(id); }
uint64 ReFix_Identity_GetSteamID64(SteamNetworkingIdentity* self) { return self ? self->GetSteamID64() : 0; }
void ReFix_Identity_SetIPAddr(SteamNetworkingIdentity* self, const SteamNetworkingIPAddr* addr) { if (self && addr) self->SetIPAddr(*addr); }
const SteamNetworkingIPAddr* ReFix_Identity_GetIPAddr(SteamNetworkingIdentity* self) { return self ? self->GetIPAddr() : nullptr; }
void ReFix_Identity_SetIPv4Addr(SteamNetworkingIdentity* self, uint32 ip, uint16 port) { if (self) self->SetIPv4Addr(ip, port); }
uint32 ReFix_Identity_GetIPv4(SteamNetworkingIdentity* self) {
    const SteamNetworkingIPAddr* a = self ? self->GetIPAddr() : nullptr;
    return a ? a->GetIPv4() : 0;
}
void ReFix_Identity_SetLocalHost(SteamNetworkingIdentity* self) { if (self) self->SetLocalHost(); }
bool ReFix_Identity_IsLocalHost(SteamNetworkingIdentity* self) { return self && self->IsLocalHost(); }
bool ReFix_Identity_IsEqualTo(SteamNetworkingIdentity* self, const SteamNetworkingIdentity* other) {
    if (!self || !other || self->m_eType != other->m_eType || self->m_cbSize != other->m_cbSize) return false;
    return self->m_cbSize <= 0 || memcmp(self->m_szUnknownRawString, other->m_szUnknownRawString, (size_t)self->m_cbSize) == 0;
}
void ReFix_Identity_ToString(SteamNetworkingIdentity* self, char* buf, uint32 cbBuf) {
    if (!buf || cbBuf == 0) return;
    if (!self || self->IsInvalid()) { strncpy_s(buf, cbBuf, "invalid", _TRUNCATE); return; }
    if (self->m_eType == k_ESteamNetworkingIdentityType_SteamID) {
        _snprintf_s(buf, cbBuf, _TRUNCATE, "steamid:%llu", (unsigned long long)self->m_steamID64);
    } else if (const SteamNetworkingIPAddr* a = self->GetIPAddr()) {
        const uint32 ip = a->GetIPv4();
        _snprintf_s(buf, cbBuf, _TRUNCATE, "ip:%u.%u.%u.%u:%u", ip >> 24, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, a->m_port);
    } else {
        _snprintf_s(buf, cbBuf, _TRUNCATE, "type%d", (int)self->m_eType);
    }
}
bool ReFix_Identity_ParseString(SteamNetworkingIdentity* self, const char* str) {
    if (!self || !str) return false;
    unsigned long long id = 0;
    unsigned a = 0, b = 0, c = 0, d = 0, port = 0;
    if (sscanf_s(str, "steamid:%llu", &id) == 1) { self->SetSteamID64(id); return true; }
    if (sscanf_s(str, "ip:%u.%u.%u.%u:%u", &a, &b, &c, &d, &port) >= 4) {
        self->SetIPv4Addr((a << 24) | (b << 16) | (c << 8) | d, (uint16)port);
        return true;
    }
    self->Clear();
    return false;
}

void ReFix_IPAddr_Clear(SteamNetworkingIPAddr* self) { if (self) self->Clear(); }
bool ReFix_IPAddr_IsIPv6AllZeros(SteamNetworkingIPAddr* self) { return self && self->IsIPv6AllZeros(); }
void ReFix_IPAddr_SetIPv6(SteamNetworkingIPAddr* self, const uint8* ipv6, uint16 port) { if (self && ipv6) self->SetIPv6(ipv6, port); }
void ReFix_IPAddr_SetIPv4(SteamNetworkingIPAddr* self, uint32 ip, uint16 port) { if (self) self->SetIPv4(ip, port); }
bool ReFix_IPAddr_IsIPv4(SteamNetworkingIPAddr* self) { return self && self->IsIPv4(); }
uint32 ReFix_IPAddr_GetIPv4(SteamNetworkingIPAddr* self) { return self ? self->GetIPv4() : 0; }
void ReFix_IPAddr_SetIPv6LocalHost(SteamNetworkingIPAddr* self, uint16 port) { if (self) self->SetIPv6LocalHost(port); }
bool ReFix_IPAddr_IsLocalHost(SteamNetworkingIPAddr* self) { return self && self->IsLocalHost(); }
bool ReFix_IPAddr_IsEqualTo(SteamNetworkingIPAddr* self, const SteamNetworkingIPAddr* other) {
    return self && other && memcmp(self->m_ipv6, other->m_ipv6, 16) == 0 && self->m_port == other->m_port;
}
void ReFix_IPAddr_ToString(SteamNetworkingIPAddr* self, char* buf, uint32 cbBuf, bool bWithPort) {
    if (!buf || cbBuf == 0) return;
    const uint32 ip = self ? self->GetIPv4() : 0;
    if (bWithPort && self)
        _snprintf_s(buf, cbBuf, _TRUNCATE, "%u.%u.%u.%u:%u", ip >> 24, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, self->m_port);
    else
        _snprintf_s(buf, cbBuf, _TRUNCATE, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
}
bool ReFix_IPAddr_ParseString(SteamNetworkingIPAddr* self, const char* str) {
    if (!self || !str) return false;
    unsigned a = 0, b = 0, c = 0, d = 0, port = 0;
    const int n = sscanf_s(str, "%u.%u.%u.%u:%u", &a, &b, &c, &d, &port);
    if (n < 4) { self->Clear(); return false; }
    self->SetIPv4((a << 24) | (b << 16) | (c << 8) | d, (uint16)(n == 5 ? port : 0));
    return true;
}

} // extern "C"
