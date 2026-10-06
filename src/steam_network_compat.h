// =============================================================================
// ReFix - Steam Networking Compatibility Module (NetCompat)
// steam_network_compat.h - Generic compatibility for SteamNetworkingSockets
// =============================================================================
#pragma once

#include <windows.h>
#include <stdint.h>

namespace SteamNetCompat {

// Initialize compatibility system (reads configuration, sets initial state)
void Initialize();

// Evaluates activation conditions after Steam initialization
void OnSteamInitialized(bool isGoldbergMode, uint32_t maskAppId, uint32_t realAppId);

// Notification when any SteamNetworkingSockets function or interface is accessed
void NotifySocketsPathDetected();

// Diagnostic processor for Callback 1221 (SteamNetConnectionStatusChangedCallback_t)
void ProcessConnectionStatusChanged(void* pubParam, int cubParam);

// Query whether compatibility is currently active
bool IsActive();

// Query whether verbose logging is enabled
bool IsVerbose();

// Interception handlers for flat SteamNetworkingSockets APIs
uint32_t Intercept_CreateListenSocketP2P(void* self, int nLocalVirtualPort, int nNumOptions, const void* pOptions);
uint32_t Intercept_ConnectP2P(void* self, const void* pIdentityRemote, int nRemoteVirtualPort, int nNumOptions, const void* pOptions);
int Intercept_AcceptConnection(void* self, uint32_t hConn);
bool Intercept_CloseConnection(void* self, uint32_t hConn, int nReason, const char* pszDebug, bool bEnableLinger);

// Fallback interface accessors
void* Intercept_SteamNetworkingSockets_v008();
void* Intercept_SteamNetworkingUtils_v003();

// Reset on SteamAPI_Shutdown
void Shutdown();

} // namespace SteamNetCompat
