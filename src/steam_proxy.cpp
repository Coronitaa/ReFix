// =============================================================================
// ReFix - steam_api64.dll Proxy (Active Matchmaking & Direct P2P UPnP Helper)
// =============================================================================
// Forwards 1055 exports to steam_api64_valve.dll via ASM jump table (g_steamProcs).
// Intercepts SteamAPI_Init, ISteamMatchmaking (CreateLobby, RequestLobbyList, SetLobbyData),
// and callbacks (GameLobbyJoinRequested_t, GameRichPresenceJoinRequested_t).
//
// Native UPnP Router Port Forwarding & Windows Firewall rule added on startup.
// Registers Direct P2P Connect string in Steam Rich Presence (+connect <PUBLIC_IP>:7777).
// =============================================================================

#pragma comment(lib, "user32.lib")
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include "upnp_firewall.h"
#include "steam_p2p_hook.h"
#include "minhook/MinHook.h"

#define STEAM_FORWARD_COUNT 1057

extern "C" {
    __declspec(dllexport) FARPROC g_steamProcs[STEAM_FORWARD_COUNT] = { 0 };
}

static const char* g_forwardNames[STEAM_FORWARD_COUNT] = {
    "GetHSteamPipe",
    "GetHSteamUser",
    "SteamAPI_GetHSteamPipe",
    "SteamAPI_GetHSteamUser",
    "SteamAPI_GetSteamInstallPath",
    "SteamAPI_ISteamAppList_GetAppBuildId",
    "SteamAPI_ISteamAppList_GetAppInstallDir",
    "SteamAPI_ISteamAppList_GetAppName",
    "SteamAPI_ISteamAppList_GetInstalledApps",
    "SteamAPI_ISteamAppList_GetNumInstalledApps",
    "SteamAPI_ISteamApps_BGetDLCDataByIndex",
    "SteamAPI_ISteamApps_BIsAppInstalled",
    "SteamAPI_ISteamApps_BIsCybercafe",
    "SteamAPI_ISteamApps_BIsDlcInstalled",
    "SteamAPI_ISteamApps_BIsLowViolence",
    "SteamAPI_ISteamApps_BIsSubscribed",
    "SteamAPI_ISteamApps_BIsSubscribedApp",
    "SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing",
    "SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend",
    "SteamAPI_ISteamApps_BIsTimedTrial",
    "SteamAPI_ISteamApps_BIsVACBanned",
    "SteamAPI_ISteamApps_GetAppBuildId",
    "SteamAPI_ISteamApps_GetAppInstallDir",
    "SteamAPI_ISteamApps_GetAppOwner",
    "SteamAPI_ISteamApps_GetAvailableGameLanguages",
    "SteamAPI_ISteamApps_GetCurrentBetaName",
    "SteamAPI_ISteamApps_GetCurrentGameLanguage",
    "SteamAPI_ISteamApps_GetDLCCount",
    "SteamAPI_ISteamApps_GetDlcDownloadProgress",
    "SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime",
    "SteamAPI_ISteamApps_GetFileDetails",
    "SteamAPI_ISteamApps_GetInstalledDepots",
    "SteamAPI_ISteamApps_GetLaunchCommandLine",
    "SteamAPI_ISteamApps_GetLaunchQueryParam",
    "SteamAPI_ISteamApps_InstallDLC",
    "SteamAPI_ISteamApps_MarkContentCorrupt",
    "SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys",
    "SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey",
    "SteamAPI_ISteamApps_UninstallDLC",
    "SteamAPI_ISteamClient_BReleaseSteamPipe",
    "SteamAPI_ISteamClient_BShutdownIfAllPipesClosed",
    "SteamAPI_ISteamClient_ConnectToGlobalUser",
    "SteamAPI_ISteamClient_CreateLocalUser",
    "SteamAPI_ISteamClient_CreateSteamPipe",
    "SteamAPI_ISteamClient_GetIPCCallCount",
    "SteamAPI_ISteamClient_GetISteamAppList",
    "SteamAPI_ISteamClient_GetISteamApps",
    "SteamAPI_ISteamClient_GetISteamController",
    "SteamAPI_ISteamClient_GetISteamFriends",
    "SteamAPI_ISteamClient_GetISteamGameSearch",
    "SteamAPI_ISteamClient_GetISteamGameServer",
    "SteamAPI_ISteamClient_GetISteamGameServerStats",
    "SteamAPI_ISteamClient_GetISteamGenericInterface",
    "SteamAPI_ISteamClient_GetISteamHTMLSurface",
    "SteamAPI_ISteamClient_GetISteamHTTP",
    "SteamAPI_ISteamClient_GetISteamInput",
    "SteamAPI_ISteamClient_GetISteamInventory",
    "SteamAPI_ISteamClient_GetISteamMatchmaking",
    "SteamAPI_ISteamClient_GetISteamMatchmakingServers",
    "SteamAPI_ISteamClient_GetISteamMusic",
    "SteamAPI_ISteamClient_GetISteamMusicRemote",
    "SteamAPI_ISteamClient_GetISteamNetworking",
    "SteamAPI_ISteamClient_GetISteamParentalSettings",
    "SteamAPI_ISteamClient_GetISteamParties",
    "SteamAPI_ISteamClient_GetISteamRemotePlay",
    "SteamAPI_ISteamClient_GetISteamRemoteStorage",
    "SteamAPI_ISteamClient_GetISteamScreenshots",
    "SteamAPI_ISteamClient_GetISteamUGC",
    "SteamAPI_ISteamClient_GetISteamUser",
    "SteamAPI_ISteamClient_GetISteamUserStats",
    "SteamAPI_ISteamClient_GetISteamUtils",
    "SteamAPI_ISteamClient_GetISteamVideo",
    "SteamAPI_ISteamClient_ReleaseUser",
    "SteamAPI_ISteamClient_SetLocalIPBinding",
    "SteamAPI_ISteamClient_SetWarningMessageHook",
    "SteamAPI_ISteamController_ActivateActionSet",
    "SteamAPI_ISteamController_ActivateActionSetLayer",
    "SteamAPI_ISteamController_DeactivateActionSetLayer",
    "SteamAPI_ISteamController_DeactivateAllActionSetLayers",
    "SteamAPI_ISteamController_GetActionOriginFromXboxOrigin",
    "SteamAPI_ISteamController_GetActionSetHandle",
    "SteamAPI_ISteamController_GetActiveActionSetLayers",
    "SteamAPI_ISteamController_GetAnalogActionData",
    "SteamAPI_ISteamController_GetAnalogActionHandle",
    "SteamAPI_ISteamController_GetAnalogActionOrigins",
    "SteamAPI_ISteamController_GetConnectedControllers",
    "SteamAPI_ISteamController_GetControllerBindingRevision",
    "SteamAPI_ISteamController_GetControllerForGamepadIndex",
    "SteamAPI_ISteamController_GetCurrentActionSet",
    "SteamAPI_ISteamController_GetDigitalActionData",
    "SteamAPI_ISteamController_GetDigitalActionHandle",
    "SteamAPI_ISteamController_GetDigitalActionOrigins",
    "SteamAPI_ISteamController_GetGamepadIndexForController",
    "SteamAPI_ISteamController_GetGlyphForActionOrigin",
    "SteamAPI_ISteamController_GetGlyphForXboxOrigin",
    "SteamAPI_ISteamController_GetInputTypeForHandle",
    "SteamAPI_ISteamController_GetMotionData",
    "SteamAPI_ISteamController_GetStringForActionOrigin",
    "SteamAPI_ISteamController_GetStringForXboxOrigin",
    "SteamAPI_ISteamController_Init",
    "SteamAPI_ISteamController_RunFrame",
    "SteamAPI_ISteamController_SetLEDColor",
    "SteamAPI_ISteamController_ShowBindingPanel",
    "SteamAPI_ISteamController_Shutdown",
    "SteamAPI_ISteamController_StopAnalogActionMomentum",
    "SteamAPI_ISteamController_TranslateActionOrigin",
    "SteamAPI_ISteamController_TriggerHapticPulse",
    "SteamAPI_ISteamController_TriggerRepeatedHapticPulse",
    "SteamAPI_ISteamController_TriggerVibration",
    "SteamAPI_ISteamFriends_ActivateGameOverlay",
    "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog",
    "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString",
    "SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog",
    "SteamAPI_ISteamFriends_ActivateGameOverlayToStore",
    "SteamAPI_ISteamFriends_ActivateGameOverlayToUser",
    "SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage",
    "SteamAPI_ISteamFriends_ClearRichPresence",
    "SteamAPI_ISteamFriends_CloseClanChatWindowInSteam",
    "SteamAPI_ISteamFriends_DownloadClanActivityCounts",
    "SteamAPI_ISteamFriends_EnumerateFollowingList",
    "SteamAPI_ISteamFriends_GetChatMemberByIndex",
    "SteamAPI_ISteamFriends_GetClanActivityCounts",
    "SteamAPI_ISteamFriends_GetClanByIndex",
    "SteamAPI_ISteamFriends_GetClanChatMemberCount",
    "SteamAPI_ISteamFriends_GetClanChatMessage",
    "SteamAPI_ISteamFriends_GetClanCount",
    "SteamAPI_ISteamFriends_GetClanName",
    "SteamAPI_ISteamFriends_GetClanOfficerByIndex",
    "SteamAPI_ISteamFriends_GetClanOfficerCount",
    "SteamAPI_ISteamFriends_GetClanOwner",
    "SteamAPI_ISteamFriends_GetClanTag",
    "SteamAPI_ISteamFriends_GetCoplayFriend",
    "SteamAPI_ISteamFriends_GetCoplayFriendCount",
    "SteamAPI_ISteamFriends_GetFollowerCount",
    "SteamAPI_ISteamFriends_GetFriendByIndex",
    "SteamAPI_ISteamFriends_GetFriendCoplayGame",
    "SteamAPI_ISteamFriends_GetFriendCoplayTime",
    "SteamAPI_ISteamFriends_GetFriendCount",
    "SteamAPI_ISteamFriends_GetFriendCountFromSource",
    "SteamAPI_ISteamFriends_GetFriendFromSourceByIndex",
    "SteamAPI_ISteamFriends_GetFriendGamePlayed",
    "SteamAPI_ISteamFriends_GetFriendMessage",
    "SteamAPI_ISteamFriends_GetFriendPersonaName",
    "SteamAPI_ISteamFriends_GetFriendPersonaNameHistory",
    "SteamAPI_ISteamFriends_GetFriendPersonaState",
    "SteamAPI_ISteamFriends_GetFriendRelationship",
    "SteamAPI_ISteamFriends_GetFriendRichPresence",
    "SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex",
    "SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount",
    "SteamAPI_ISteamFriends_GetFriendSteamLevel",
    "SteamAPI_ISteamFriends_GetFriendsGroupCount",
    "SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex",
    "SteamAPI_ISteamFriends_GetFriendsGroupMembersCount",
    "SteamAPI_ISteamFriends_GetFriendsGroupMembersList",
    "SteamAPI_ISteamFriends_GetFriendsGroupName",
    "SteamAPI_ISteamFriends_GetLargeFriendAvatar",
    "SteamAPI_ISteamFriends_GetMediumFriendAvatar",
    "SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages",
    "SteamAPI_ISteamFriends_GetPersonaName",
    "SteamAPI_ISteamFriends_GetPersonaState",
    "SteamAPI_ISteamFriends_GetPlayerNickname",
    "SteamAPI_ISteamFriends_GetSmallFriendAvatar",
    "SteamAPI_ISteamFriends_GetUserRestrictions",
    "SteamAPI_ISteamFriends_HasFriend",
    "SteamAPI_ISteamFriends_InviteUserToGame",
    "SteamAPI_ISteamFriends_IsClanChatAdmin",
    "SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam",
    "SteamAPI_ISteamFriends_IsClanOfficialGameGroup",
    "SteamAPI_ISteamFriends_IsClanPublic",
    "SteamAPI_ISteamFriends_IsFollowing",
    "SteamAPI_ISteamFriends_IsUserInSource",
    "SteamAPI_ISteamFriends_JoinClanChatRoom",
    "SteamAPI_ISteamFriends_LeaveClanChatRoom",
    "SteamAPI_ISteamFriends_OpenClanChatWindowInSteam",
    "SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser",
    "SteamAPI_ISteamFriends_ReplyToFriendMessage",
    "SteamAPI_ISteamFriends_RequestClanOfficerList",
    "SteamAPI_ISteamFriends_RequestFriendRichPresence",
    "SteamAPI_ISteamFriends_RequestUserInformation",
    "SteamAPI_ISteamFriends_SendClanChatMessage",
    "SteamAPI_ISteamFriends_SetInGameVoiceSpeaking",
    "SteamAPI_ISteamFriends_SetListenForFriendsMessages",
    "SteamAPI_ISteamFriends_SetPersonaName",
    "SteamAPI_ISteamFriends_SetPlayedWith",
    "SteamAPI_ISteamFriends_SetRichPresence",
    "SteamAPI_ISteamGameSearch_AcceptGame",
    "SteamAPI_ISteamGameSearch_AddGameSearchParams",
    "SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame",
    "SteamAPI_ISteamGameSearch_DeclineGame",
    "SteamAPI_ISteamGameSearch_EndGame",
    "SteamAPI_ISteamGameSearch_EndGameSearch",
    "SteamAPI_ISteamGameSearch_HostConfirmGameStart",
    "SteamAPI_ISteamGameSearch_RequestPlayersForGame",
    "SteamAPI_ISteamGameSearch_RetrieveConnectionDetails",
    "SteamAPI_ISteamGameSearch_SearchForGameSolo",
    "SteamAPI_ISteamGameSearch_SearchForGameWithLobby",
    "SteamAPI_ISteamGameSearch_SetConnectionDetails",
    "SteamAPI_ISteamGameSearch_SetGameHostParams",
    "SteamAPI_ISteamGameSearch_SubmitPlayerResult",
    "SteamAPI_ISteamGameServerStats_ClearUserAchievement",
    "SteamAPI_ISteamGameServerStats_GetUserAchievement",
    "SteamAPI_ISteamGameServerStats_GetUserStatFloat",
    "SteamAPI_ISteamGameServerStats_GetUserStatInt32",
    "SteamAPI_ISteamGameServerStats_RequestUserStats",
    "SteamAPI_ISteamGameServerStats_SetUserAchievement",
    "SteamAPI_ISteamGameServerStats_SetUserStatFloat",
    "SteamAPI_ISteamGameServerStats_SetUserStatInt32",
    "SteamAPI_ISteamGameServerStats_StoreUserStats",
    "SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat",
    "SteamAPI_ISteamGameServer_AssociateWithClan",
    "SteamAPI_ISteamGameServer_BLoggedOn",
    "SteamAPI_ISteamGameServer_BSecure",
    "SteamAPI_ISteamGameServer_BUpdateUserData",
    "SteamAPI_ISteamGameServer_BeginAuthSession",
    "SteamAPI_ISteamGameServer_CancelAuthTicket",
    "SteamAPI_ISteamGameServer_ClearAllKeyValues",
    "SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility",
    "SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection",
    "SteamAPI_ISteamGameServer_EndAuthSession",
    "SteamAPI_ISteamGameServer_GetAuthSessionTicket",
    "SteamAPI_ISteamGameServer_GetGameplayStats",
    "SteamAPI_ISteamGameServer_GetNextOutgoingPacket",
    "SteamAPI_ISteamGameServer_GetPublicIP",
    "SteamAPI_ISteamGameServer_GetServerReputation",
    "SteamAPI_ISteamGameServer_GetSteamID",
    "SteamAPI_ISteamGameServer_HandleIncomingPacket",
    "SteamAPI_ISteamGameServer_LogOff",
    "SteamAPI_ISteamGameServer_LogOn",
    "SteamAPI_ISteamGameServer_LogOnAnonymous",
    "SteamAPI_ISteamGameServer_RequestUserGroupStatus",
    "SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED",
    "SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED",
    "SteamAPI_ISteamGameServer_SetAdvertiseServerActive",
    "SteamAPI_ISteamGameServer_SetBotPlayerCount",
    "SteamAPI_ISteamGameServer_SetDedicatedServer",
    "SteamAPI_ISteamGameServer_SetGameData",
    "SteamAPI_ISteamGameServer_SetGameDescription",
    "SteamAPI_ISteamGameServer_SetGameTags",
    "SteamAPI_ISteamGameServer_SetKeyValue",
    "SteamAPI_ISteamGameServer_SetMapName",
    "SteamAPI_ISteamGameServer_SetMaxPlayerCount",
    "SteamAPI_ISteamGameServer_SetModDir",
    "SteamAPI_ISteamGameServer_SetPasswordProtected",
    "SteamAPI_ISteamGameServer_SetProduct",
    "SteamAPI_ISteamGameServer_SetRegion",
    "SteamAPI_ISteamGameServer_SetServerName",
    "SteamAPI_ISteamGameServer_SetSpectatorPort",
    "SteamAPI_ISteamGameServer_SetSpectatorServerName",
    "SteamAPI_ISteamGameServer_UserHasLicenseForApp",
    "SteamAPI_ISteamGameServer_WasRestartRequested",
    "SteamAPI_ISteamHTMLSurface_AddHeader",
    "SteamAPI_ISteamHTMLSurface_AllowStartRequest",
    "SteamAPI_ISteamHTMLSurface_CopyToClipboard",
    "SteamAPI_ISteamHTMLSurface_CreateBrowser",
    "SteamAPI_ISteamHTMLSurface_ExecuteJavascript",
    "SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse",
    "SteamAPI_ISteamHTMLSurface_Find",
    "SteamAPI_ISteamHTMLSurface_GetLinkAtPosition",
    "SteamAPI_ISteamHTMLSurface_GoBack",
    "SteamAPI_ISteamHTMLSurface_GoForward",
    "SteamAPI_ISteamHTMLSurface_Init",
    "SteamAPI_ISteamHTMLSurface_JSDialogResponse",
    "SteamAPI_ISteamHTMLSurface_KeyChar",
    "SteamAPI_ISteamHTMLSurface_KeyDown",
    "SteamAPI_ISteamHTMLSurface_KeyUp",
    "SteamAPI_ISteamHTMLSurface_LoadURL",
    "SteamAPI_ISteamHTMLSurface_MouseDoubleClick",
    "SteamAPI_ISteamHTMLSurface_MouseDown",
    "SteamAPI_ISteamHTMLSurface_MouseMove",
    "SteamAPI_ISteamHTMLSurface_MouseUp",
    "SteamAPI_ISteamHTMLSurface_MouseWheel",
    "SteamAPI_ISteamHTMLSurface_OpenDeveloperTools",
    "SteamAPI_ISteamHTMLSurface_PasteFromClipboard",
    "SteamAPI_ISteamHTMLSurface_Reload",
    "SteamAPI_ISteamHTMLSurface_RemoveBrowser",
    "SteamAPI_ISteamHTMLSurface_SetBackgroundMode",
    "SteamAPI_ISteamHTMLSurface_SetCookie",
    "SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor",
    "SteamAPI_ISteamHTMLSurface_SetHorizontalScroll",
    "SteamAPI_ISteamHTMLSurface_SetKeyFocus",
    "SteamAPI_ISteamHTMLSurface_SetPageScaleFactor",
    "SteamAPI_ISteamHTMLSurface_SetSize",
    "SteamAPI_ISteamHTMLSurface_SetVerticalScroll",
    "SteamAPI_ISteamHTMLSurface_Shutdown",
    "SteamAPI_ISteamHTMLSurface_StopFind",
    "SteamAPI_ISteamHTMLSurface_StopLoad",
    "SteamAPI_ISteamHTMLSurface_ViewSource",
    "SteamAPI_ISteamHTTP_CreateCookieContainer",
    "SteamAPI_ISteamHTTP_CreateHTTPRequest",
    "SteamAPI_ISteamHTTP_DeferHTTPRequest",
    "SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct",
    "SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut",
    "SteamAPI_ISteamHTTP_GetHTTPResponseBodyData",
    "SteamAPI_ISteamHTTP_GetHTTPResponseBodySize",
    "SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize",
    "SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue",
    "SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData",
    "SteamAPI_ISteamHTTP_PrioritizeHTTPRequest",
    "SteamAPI_ISteamHTTP_ReleaseCookieContainer",
    "SteamAPI_ISteamHTTP_ReleaseHTTPRequest",
    "SteamAPI_ISteamHTTP_SendHTTPRequest",
    "SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse",
    "SteamAPI_ISteamHTTP_SetCookie",
    "SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS",
    "SteamAPI_ISteamHTTP_SetHTTPRequestContextValue",
    "SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer",
    "SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter",
    "SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue",
    "SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout",
    "SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody",
    "SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate",
    "SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo",
    "SteamAPI_ISteamInput_ActivateActionSet",
    "SteamAPI_ISteamInput_ActivateActionSetLayer",
    "SteamAPI_ISteamInput_BNewDataAvailable",
    "SteamAPI_ISteamInput_BWaitForData",
    "SteamAPI_ISteamInput_DeactivateActionSetLayer",
    "SteamAPI_ISteamInput_DeactivateAllActionSetLayers",
    "SteamAPI_ISteamInput_EnableActionEventCallbacks",
    "SteamAPI_ISteamInput_EnableDeviceCallbacks",
    "SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin",
    "SteamAPI_ISteamInput_GetActionSetHandle",
    "SteamAPI_ISteamInput_GetActiveActionSetLayers",
    "SteamAPI_ISteamInput_GetAnalogActionData",
    "SteamAPI_ISteamInput_GetAnalogActionHandle",
    "SteamAPI_ISteamInput_GetAnalogActionOrigins",
    "SteamAPI_ISteamInput_GetConnectedControllers",
    "SteamAPI_ISteamInput_GetControllerForGamepadIndex",
    "SteamAPI_ISteamInput_GetCurrentActionSet",
    "SteamAPI_ISteamInput_GetDeviceBindingRevision",
    "SteamAPI_ISteamInput_GetDigitalActionData",
    "SteamAPI_ISteamInput_GetDigitalActionHandle",
    "SteamAPI_ISteamInput_GetDigitalActionOrigins",
    "SteamAPI_ISteamInput_GetGamepadIndexForController",
    "SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy",
    "SteamAPI_ISteamInput_GetGlyphForXboxOrigin",
    "SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin",
    "SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin",
    "SteamAPI_ISteamInput_GetInputTypeForHandle",
    "SteamAPI_ISteamInput_GetMotionData",
    "SteamAPI_ISteamInput_GetRemotePlaySessionID",
    "SteamAPI_ISteamInput_GetSessionInputConfigurationSettings",
    "SteamAPI_ISteamInput_GetStringForActionOrigin",
    "SteamAPI_ISteamInput_GetStringForAnalogActionName",
    "SteamAPI_ISteamInput_GetStringForDigitalActionName",
    "SteamAPI_ISteamInput_GetStringForXboxOrigin",
    "SteamAPI_ISteamInput_Init",
    "SteamAPI_ISteamInput_Legacy_TriggerHapticPulse",
    "SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse",
    "SteamAPI_ISteamInput_RunFrame",
    "SteamAPI_ISteamInput_SetInputActionManifestFilePath",
    "SteamAPI_ISteamInput_SetLEDColor",
    "SteamAPI_ISteamInput_ShowBindingPanel",
    "SteamAPI_ISteamInput_Shutdown",
    "SteamAPI_ISteamInput_StopAnalogActionMomentum",
    "SteamAPI_ISteamInput_TranslateActionOrigin",
    "SteamAPI_ISteamInput_TriggerSimpleHapticEvent",
    "SteamAPI_ISteamInput_TriggerVibration",
    "SteamAPI_ISteamInput_TriggerVibrationExtended",
    "SteamAPI_ISteamInventory_AddPromoItem",
    "SteamAPI_ISteamInventory_AddPromoItems",
    "SteamAPI_ISteamInventory_CheckResultSteamID",
    "SteamAPI_ISteamInventory_ConsumeItem",
    "SteamAPI_ISteamInventory_DeserializeResult",
    "SteamAPI_ISteamInventory_DestroyResult",
    "SteamAPI_ISteamInventory_ExchangeItems",
    "SteamAPI_ISteamInventory_GenerateItems",
    "SteamAPI_ISteamInventory_GetAllItems",
    "SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs",
    "SteamAPI_ISteamInventory_GetItemDefinitionIDs",
    "SteamAPI_ISteamInventory_GetItemDefinitionProperty",
    "SteamAPI_ISteamInventory_GetItemPrice",
    "SteamAPI_ISteamInventory_GetItemsByID",
    "SteamAPI_ISteamInventory_GetItemsWithPrices",
    "SteamAPI_ISteamInventory_GetNumItemsWithPrices",
    "SteamAPI_ISteamInventory_GetResultItemProperty",
    "SteamAPI_ISteamInventory_GetResultItems",
    "SteamAPI_ISteamInventory_GetResultStatus",
    "SteamAPI_ISteamInventory_GetResultTimestamp",
    "SteamAPI_ISteamInventory_GrantPromoItems",
    "SteamAPI_ISteamInventory_InspectItem",
    "SteamAPI_ISteamInventory_LoadItemDefinitions",
    "SteamAPI_ISteamInventory_RemoveProperty",
    "SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs",
    "SteamAPI_ISteamInventory_RequestPrices",
    "SteamAPI_ISteamInventory_SendItemDropHeartbeat",
    "SteamAPI_ISteamInventory_SerializeResult",
    "SteamAPI_ISteamInventory_SetPropertyBool",
    "SteamAPI_ISteamInventory_SetPropertyFloat",
    "SteamAPI_ISteamInventory_SetPropertyInt64",
    "SteamAPI_ISteamInventory_SetPropertyString",
    "SteamAPI_ISteamInventory_StartPurchase",
    "SteamAPI_ISteamInventory_StartUpdateProperties",
    "SteamAPI_ISteamInventory_SubmitUpdateProperties",
    "SteamAPI_ISteamInventory_TradeItems",
    "SteamAPI_ISteamInventory_TransferItemQuantity",
    "SteamAPI_ISteamInventory_TriggerItemDrop",
    "SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond",
    "SteamAPI_ISteamMatchmakingPingResponse_ServerResponded",
    "SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList",
    "SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond",
    "SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete",
    "SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond",
    "SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete",
    "SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded",
    "SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete",
    "SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond",
    "SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded",
    "SteamAPI_ISteamMatchmakingServers_CancelQuery",
    "SteamAPI_ISteamMatchmakingServers_CancelServerQuery",
    "SteamAPI_ISteamMatchmakingServers_GetServerCount",
    "SteamAPI_ISteamMatchmakingServers_GetServerDetails",
    "SteamAPI_ISteamMatchmakingServers_IsRefreshing",
    "SteamAPI_ISteamMatchmakingServers_PingServer",
    "SteamAPI_ISteamMatchmakingServers_PlayerDetails",
    "SteamAPI_ISteamMatchmakingServers_RefreshQuery",
    "SteamAPI_ISteamMatchmakingServers_RefreshServer",
    "SteamAPI_ISteamMatchmakingServers_ReleaseRequest",
    "SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList",
    "SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList",
    "SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList",
    "SteamAPI_ISteamMatchmakingServers_RequestInternetServerList",
    "SteamAPI_ISteamMatchmakingServers_RequestLANServerList",
    "SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList",
    "SteamAPI_ISteamMatchmakingServers_ServerRules",
    "SteamAPI_ISteamMatchmaking_AddFavoriteGame",
    "SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter",
    "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter",
    "SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable",
    "SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter",
    "SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter",
    "SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter",
    "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter",
    "SteamAPI_ISteamMatchmaking_CreateLobby",
    "SteamAPI_ISteamMatchmaking_DeleteLobbyData",
    "SteamAPI_ISteamMatchmaking_GetFavoriteGame",
    "SteamAPI_ISteamMatchmaking_GetFavoriteGameCount",
    "SteamAPI_ISteamMatchmaking_GetLobbyByIndex",
    "SteamAPI_ISteamMatchmaking_GetLobbyChatEntry",
    "SteamAPI_ISteamMatchmaking_GetLobbyData",
    "SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex",
    "SteamAPI_ISteamMatchmaking_GetLobbyDataCount",
    "SteamAPI_ISteamMatchmaking_GetLobbyGameServer",
    "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex",
    "SteamAPI_ISteamMatchmaking_GetLobbyMemberData",
    "SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit",
    "SteamAPI_ISteamMatchmaking_GetLobbyOwner",
    "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers",
    "SteamAPI_ISteamMatchmaking_InviteUserToLobby",
    "SteamAPI_ISteamMatchmaking_JoinLobby",
    "SteamAPI_ISteamMatchmaking_LeaveLobby",
    "SteamAPI_ISteamMatchmaking_RemoveFavoriteGame",
    "SteamAPI_ISteamMatchmaking_RequestLobbyData",
    "SteamAPI_ISteamMatchmaking_RequestLobbyList",
    "SteamAPI_ISteamMatchmaking_SendLobbyChatMsg",
    "SteamAPI_ISteamMatchmaking_SetLinkedLobby",
    "SteamAPI_ISteamMatchmaking_SetLobbyData",
    "SteamAPI_ISteamMatchmaking_SetLobbyGameServer",
    "SteamAPI_ISteamMatchmaking_SetLobbyJoinable",
    "SteamAPI_ISteamMatchmaking_SetLobbyMemberData",
    "SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit",
    "SteamAPI_ISteamMatchmaking_SetLobbyOwner",
    "SteamAPI_ISteamMatchmaking_SetLobbyType",
    "SteamAPI_ISteamMusicRemote_BActivationSuccess",
    "SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote",
    "SteamAPI_ISteamMusicRemote_CurrentEntryDidChange",
    "SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable",
    "SteamAPI_ISteamMusicRemote_CurrentEntryWillChange",
    "SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote",
    "SteamAPI_ISteamMusicRemote_EnableLooped",
    "SteamAPI_ISteamMusicRemote_EnablePlayNext",
    "SteamAPI_ISteamMusicRemote_EnablePlayPrevious",
    "SteamAPI_ISteamMusicRemote_EnablePlaylists",
    "SteamAPI_ISteamMusicRemote_EnableQueue",
    "SteamAPI_ISteamMusicRemote_EnableShuffled",
    "SteamAPI_ISteamMusicRemote_PlaylistDidChange",
    "SteamAPI_ISteamMusicRemote_PlaylistWillChange",
    "SteamAPI_ISteamMusicRemote_QueueDidChange",
    "SteamAPI_ISteamMusicRemote_QueueWillChange",
    "SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote",
    "SteamAPI_ISteamMusicRemote_ResetPlaylistEntries",
    "SteamAPI_ISteamMusicRemote_ResetQueueEntries",
    "SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry",
    "SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry",
    "SteamAPI_ISteamMusicRemote_SetDisplayName",
    "SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64",
    "SteamAPI_ISteamMusicRemote_SetPlaylistEntry",
    "SteamAPI_ISteamMusicRemote_SetQueueEntry",
    "SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt",
    "SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds",
    "SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText",
    "SteamAPI_ISteamMusicRemote_UpdateLooped",
    "SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus",
    "SteamAPI_ISteamMusicRemote_UpdateShuffled",
    "SteamAPI_ISteamMusicRemote_UpdateVolume",
    "SteamAPI_ISteamMusic_BIsEnabled",
    "SteamAPI_ISteamMusic_BIsPlaying",
    "SteamAPI_ISteamMusic_GetPlaybackStatus",
    "SteamAPI_ISteamMusic_GetVolume",
    "SteamAPI_ISteamMusic_Pause",
    "SteamAPI_ISteamMusic_Play",
    "SteamAPI_ISteamMusic_PlayNext",
    "SteamAPI_ISteamMusic_PlayPrevious",
    "SteamAPI_ISteamMusic_SetVolume",
    "SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort",
    "SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages",
    "SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup",
    "SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP",
    "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser",
    "SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser",
    "SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser",
    "SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo",
    "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel",
    "SteamAPI_ISteamNetworkingMessages_SendMessageToUser",
    "SteamAPI_ISteamNetworkingSockets_AcceptConnection",
    "SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP",
    "SteamAPI_ISteamNetworkingSockets_CloseConnection",
    "SteamAPI_ISteamNetworkingSockets_CloseListenSocket",
    "SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes",
    "SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress",
    "SteamAPI_ISteamNetworkingSockets_ConnectP2P",
    "SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling",
    "SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer",
    "SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort",
    "SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket",
    "SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP",
    "SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P",
    "SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP",
    "SteamAPI_ISteamNetworkingSockets_CreatePollGroup",
    "SteamAPI_ISteamNetworkingSockets_CreateSocketPair",
    "SteamAPI_ISteamNetworkingSockets_DestroyPollGroup",
    "SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer",
    "SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection",
    "SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus",
    "SteamAPI_ISteamNetworkingSockets_GetCertificateRequest",
    "SteamAPI_ISteamNetworkingSockets_GetConnectionInfo",
    "SteamAPI_ISteamNetworkingSockets_GetConnectionName",
    "SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus",
    "SteamAPI_ISteamNetworkingSockets_GetConnectionUserData",
    "SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus",
    "SteamAPI_ISteamNetworkingSockets_GetFakeIP",
    "SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin",
    "SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress",
    "SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID",
    "SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort",
    "SteamAPI_ISteamNetworkingSockets_GetIdentity",
    "SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress",
    "SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection",
    "SteamAPI_ISteamNetworkingSockets_InitAuthentication",
    "SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection",
    "SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup",
    "SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal",
    "SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket",
    "SteamAPI_ISteamNetworkingSockets_ResetIdentity",
    "SteamAPI_ISteamNetworkingSockets_RunCallbacks",
    "SteamAPI_ISteamNetworkingSockets_SendMessageToConnection",
    "SteamAPI_ISteamNetworkingSockets_SendMessages",
    "SteamAPI_ISteamNetworkingSockets_SetCertificate",
    "SteamAPI_ISteamNetworkingSockets_SetConnectionName",
    "SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup",
    "SteamAPI_ISteamNetworkingSockets_SetConnectionUserData",
    "SteamAPI_ISteamNetworkingUtils_AllocateMessage",
    "SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate",
    "SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString",
    "SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations",
    "SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost",
    "SteamAPI_ISteamNetworkingUtils_GetConfigValue",
    "SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo",
    "SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP",
    "SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType",
    "SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation",
    "SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp",
    "SteamAPI_ISteamNetworkingUtils_GetPOPCount",
    "SteamAPI_ISteamNetworkingUtils_GetPOPList",
    "SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter",
    "SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP",
    "SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus",
    "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess",
    "SteamAPI_ISteamNetworkingUtils_IsFakeIPv4",
    "SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues",
    "SteamAPI_ISteamNetworkingUtils_ParsePingLocationString",
    "SteamAPI_ISteamNetworkingUtils_SetConfigValue",
    "SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct",
    "SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat",
    "SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32",
    "SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString",
    "SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr",
    "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString",
    "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType",
    "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString",
    "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString",
    "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString",
    "SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString",
    "SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser",
    "SteamAPI_ISteamNetworking_AllowP2PPacketRelay",
    "SteamAPI_ISteamNetworking_CloseP2PChannelWithUser",
    "SteamAPI_ISteamNetworking_CloseP2PSessionWithUser",
    "SteamAPI_ISteamNetworking_CreateConnectionSocket",
    "SteamAPI_ISteamNetworking_CreateListenSocket",
    "SteamAPI_ISteamNetworking_CreateP2PConnectionSocket",
    "SteamAPI_ISteamNetworking_DestroyListenSocket",
    "SteamAPI_ISteamNetworking_DestroySocket",
    "SteamAPI_ISteamNetworking_GetListenSocketInfo",
    "SteamAPI_ISteamNetworking_GetMaxPacketSize",
    "SteamAPI_ISteamNetworking_GetP2PSessionState",
    "SteamAPI_ISteamNetworking_GetSocketConnectionType",
    "SteamAPI_ISteamNetworking_GetSocketInfo",
    "SteamAPI_ISteamNetworking_IsDataAvailable",
    "SteamAPI_ISteamNetworking_IsDataAvailableOnSocket",
    "SteamAPI_ISteamNetworking_IsP2PPacketAvailable",
    "SteamAPI_ISteamNetworking_ReadP2PPacket",
    "SteamAPI_ISteamNetworking_RetrieveData",
    "SteamAPI_ISteamNetworking_RetrieveDataFromSocket",
    "SteamAPI_ISteamNetworking_SendDataOnSocket",
    "SteamAPI_ISteamNetworking_SendP2PPacket",
    "SteamAPI_ISteamParentalSettings_BIsAppBlocked",
    "SteamAPI_ISteamParentalSettings_BIsAppInBlockList",
    "SteamAPI_ISteamParentalSettings_BIsFeatureBlocked",
    "SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList",
    "SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled",
    "SteamAPI_ISteamParentalSettings_BIsParentalLockLocked",
    "SteamAPI_ISteamParties_CancelReservation",
    "SteamAPI_ISteamParties_ChangeNumOpenSlots",
    "SteamAPI_ISteamParties_CreateBeacon",
    "SteamAPI_ISteamParties_DestroyBeacon",
    "SteamAPI_ISteamParties_GetAvailableBeaconLocations",
    "SteamAPI_ISteamParties_GetBeaconByIndex",
    "SteamAPI_ISteamParties_GetBeaconDetails",
    "SteamAPI_ISteamParties_GetBeaconLocationData",
    "SteamAPI_ISteamParties_GetNumActiveBeacons",
    "SteamAPI_ISteamParties_GetNumAvailableBeaconLocations",
    "SteamAPI_ISteamParties_JoinParty",
    "SteamAPI_ISteamParties_OnReservationCompleted",
    "SteamAPI_ISteamRemotePlay_BGetSessionClientResolution",
    "SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite",
    "SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor",
    "SteamAPI_ISteamRemotePlay_GetSessionClientName",
    "SteamAPI_ISteamRemotePlay_GetSessionCount",
    "SteamAPI_ISteamRemotePlay_GetSessionID",
    "SteamAPI_ISteamRemotePlay_GetSessionSteamID",
    "SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch",
    "SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate",
    "SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest",
    "SteamAPI_ISteamRemoteStorage_DeletePublishedFile",
    "SteamAPI_ISteamRemoteStorage_EndFileWriteBatch",
    "SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction",
    "SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles",
    "SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles",
    "SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles",
    "SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles",
    "SteamAPI_ISteamRemoteStorage_FileDelete",
    "SteamAPI_ISteamRemoteStorage_FileExists",
    "SteamAPI_ISteamRemoteStorage_FileForget",
    "SteamAPI_ISteamRemoteStorage_FilePersisted",
    "SteamAPI_ISteamRemoteStorage_FileRead",
    "SteamAPI_ISteamRemoteStorage_FileReadAsync",
    "SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete",
    "SteamAPI_ISteamRemoteStorage_FileShare",
    "SteamAPI_ISteamRemoteStorage_FileWrite",
    "SteamAPI_ISteamRemoteStorage_FileWriteAsync",
    "SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel",
    "SteamAPI_ISteamRemoteStorage_FileWriteStreamClose",
    "SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen",
    "SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk",
    "SteamAPI_ISteamRemoteStorage_GetCachedUGCCount",
    "SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle",
    "SteamAPI_ISteamRemoteStorage_GetFileCount",
    "SteamAPI_ISteamRemoteStorage_GetFileNameAndSize",
    "SteamAPI_ISteamRemoteStorage_GetFileSize",
    "SteamAPI_ISteamRemoteStorage_GetFileTimestamp",
    "SteamAPI_ISteamRemoteStorage_GetLocalFileChange",
    "SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount",
    "SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails",
    "SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails",
    "SteamAPI_ISteamRemoteStorage_GetQuota",
    "SteamAPI_ISteamRemoteStorage_GetSyncPlatforms",
    "SteamAPI_ISteamRemoteStorage_GetUGCDetails",
    "SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress",
    "SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails",
    "SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount",
    "SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp",
    "SteamAPI_ISteamRemoteStorage_PublishVideo",
    "SteamAPI_ISteamRemoteStorage_PublishWorkshopFile",
    "SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp",
    "SteamAPI_ISteamRemoteStorage_SetSyncPlatforms",
    "SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction",
    "SteamAPI_ISteamRemoteStorage_SubscribePublishedFile",
    "SteamAPI_ISteamRemoteStorage_UGCDownload",
    "SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation",
    "SteamAPI_ISteamRemoteStorage_UGCRead",
    "SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile",
    "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription",
    "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile",
    "SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile",
    "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription",
    "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags",
    "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle",
    "SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility",
    "SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote",
    "SteamAPI_ISteamScreenshots_AddScreenshotToLibrary",
    "SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary",
    "SteamAPI_ISteamScreenshots_HookScreenshots",
    "SteamAPI_ISteamScreenshots_IsScreenshotsHooked",
    "SteamAPI_ISteamScreenshots_SetLocation",
    "SteamAPI_ISteamScreenshots_TagPublishedFile",
    "SteamAPI_ISteamScreenshots_TagUser",
    "SteamAPI_ISteamScreenshots_TriggerScreenshot",
    "SteamAPI_ISteamScreenshots_WriteScreenshot",
    "SteamAPI_ISteamUGC_AddAppDependency",
    "SteamAPI_ISteamUGC_AddDependency",
    "SteamAPI_ISteamUGC_AddExcludedTag",
    "SteamAPI_ISteamUGC_AddItemKeyValueTag",
    "SteamAPI_ISteamUGC_AddItemPreviewFile",
    "SteamAPI_ISteamUGC_AddItemPreviewVideo",
    "SteamAPI_ISteamUGC_AddItemToFavorites",
    "SteamAPI_ISteamUGC_AddRequiredKeyValueTag",
    "SteamAPI_ISteamUGC_AddRequiredTag",
    "SteamAPI_ISteamUGC_AddRequiredTagGroup",
    "SteamAPI_ISteamUGC_BInitWorkshopForGameServer",
    "SteamAPI_ISteamUGC_CreateItem",
    "SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor",
    "SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage",
    "SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest",
    "SteamAPI_ISteamUGC_CreateQueryUserUGCRequest",
    "SteamAPI_ISteamUGC_DeleteItem",
    "SteamAPI_ISteamUGC_DownloadItem",
    "SteamAPI_ISteamUGC_GetAppDependencies",
    "SteamAPI_ISteamUGC_GetItemDownloadInfo",
    "SteamAPI_ISteamUGC_GetItemInstallInfo",
    "SteamAPI_ISteamUGC_GetItemState",
    "SteamAPI_ISteamUGC_GetItemUpdateProgress",
    "SteamAPI_ISteamUGC_GetNumSubscribedItems",
    "SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag",
    "SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview",
    "SteamAPI_ISteamUGC_GetQueryUGCChildren",
    "SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag",
    "SteamAPI_ISteamUGC_GetQueryUGCMetadata",
    "SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews",
    "SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags",
    "SteamAPI_ISteamUGC_GetQueryUGCNumTags",
    "SteamAPI_ISteamUGC_GetQueryUGCPreviewURL",
    "SteamAPI_ISteamUGC_GetQueryUGCResult",
    "SteamAPI_ISteamUGC_GetQueryUGCStatistic",
    "SteamAPI_ISteamUGC_GetQueryUGCTag",
    "SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName",
    "SteamAPI_ISteamUGC_GetSubscribedItems",
    "SteamAPI_ISteamUGC_GetUserItemVote",
    "SteamAPI_ISteamUGC_GetWorkshopEULAStatus",
    "SteamAPI_ISteamUGC_ReleaseQueryUGCRequest",
    "SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags",
    "SteamAPI_ISteamUGC_RemoveAppDependency",
    "SteamAPI_ISteamUGC_RemoveDependency",
    "SteamAPI_ISteamUGC_RemoveItemFromFavorites",
    "SteamAPI_ISteamUGC_RemoveItemKeyValueTags",
    "SteamAPI_ISteamUGC_RemoveItemPreview",
    "SteamAPI_ISteamUGC_RequestUGCDetails",
    "SteamAPI_ISteamUGC_SendQueryUGCRequest",
    "SteamAPI_ISteamUGC_SetAllowCachedResponse",
    "SteamAPI_ISteamUGC_SetAllowLegacyUpload",
    "SteamAPI_ISteamUGC_SetCloudFileNameFilter",
    "SteamAPI_ISteamUGC_SetItemContent",
    "SteamAPI_ISteamUGC_SetItemDescription",
    "SteamAPI_ISteamUGC_SetItemMetadata",
    "SteamAPI_ISteamUGC_SetItemPreview",
    "SteamAPI_ISteamUGC_SetItemTags",
    "SteamAPI_ISteamUGC_SetItemTitle",
    "SteamAPI_ISteamUGC_SetItemUpdateLanguage",
    "SteamAPI_ISteamUGC_SetItemVisibility",
    "SteamAPI_ISteamUGC_SetLanguage",
    "SteamAPI_ISteamUGC_SetMatchAnyTag",
    "SteamAPI_ISteamUGC_SetRankedByTrendDays",
    "SteamAPI_ISteamUGC_SetReturnAdditionalPreviews",
    "SteamAPI_ISteamUGC_SetReturnChildren",
    "SteamAPI_ISteamUGC_SetReturnKeyValueTags",
    "SteamAPI_ISteamUGC_SetReturnLongDescription",
    "SteamAPI_ISteamUGC_SetReturnMetadata",
    "SteamAPI_ISteamUGC_SetReturnOnlyIDs",
    "SteamAPI_ISteamUGC_SetReturnPlaytimeStats",
    "SteamAPI_ISteamUGC_SetReturnTotalOnly",
    "SteamAPI_ISteamUGC_SetSearchText",
    "SteamAPI_ISteamUGC_SetTimeCreatedDateRange",
    "SteamAPI_ISteamUGC_SetTimeUpdatedDateRange",
    "SteamAPI_ISteamUGC_SetUserItemVote",
    "SteamAPI_ISteamUGC_ShowWorkshopEULA",
    "SteamAPI_ISteamUGC_StartItemUpdate",
    "SteamAPI_ISteamUGC_StartPlaytimeTracking",
    "SteamAPI_ISteamUGC_StopPlaytimeTracking",
    "SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems",
    "SteamAPI_ISteamUGC_SubmitItemUpdate",
    "SteamAPI_ISteamUGC_SubscribeItem",
    "SteamAPI_ISteamUGC_SuspendDownloads",
    "SteamAPI_ISteamUGC_UnsubscribeItem",
    "SteamAPI_ISteamUGC_UpdateItemPreviewFile",
    "SteamAPI_ISteamUGC_UpdateItemPreviewVideo",
    "SteamAPI_ISteamUserStats_AttachLeaderboardUGC",
    "SteamAPI_ISteamUserStats_ClearAchievement",
    "SteamAPI_ISteamUserStats_DownloadLeaderboardEntries",
    "SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers",
    "SteamAPI_ISteamUserStats_FindLeaderboard",
    "SteamAPI_ISteamUserStats_FindOrCreateLeaderboard",
    "SteamAPI_ISteamUserStats_GetAchievement",
    "SteamAPI_ISteamUserStats_GetAchievementAchievedPercent",
    "SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime",
    "SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute",
    "SteamAPI_ISteamUserStats_GetAchievementIcon",
    "SteamAPI_ISteamUserStats_GetAchievementName",
    "SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat",
    "SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32",
    "SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry",
    "SteamAPI_ISteamUserStats_GetGlobalStatDouble",
    "SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble",
    "SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64",
    "SteamAPI_ISteamUserStats_GetGlobalStatInt64",
    "SteamAPI_ISteamUserStats_GetLeaderboardDisplayType",
    "SteamAPI_ISteamUserStats_GetLeaderboardEntryCount",
    "SteamAPI_ISteamUserStats_GetLeaderboardName",
    "SteamAPI_ISteamUserStats_GetLeaderboardSortMethod",
    "SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo",
    "SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo",
    "SteamAPI_ISteamUserStats_GetNumAchievements",
    "SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers",
    "SteamAPI_ISteamUserStats_GetStatFloat",
    "SteamAPI_ISteamUserStats_GetStatInt32",
    "SteamAPI_ISteamUserStats_GetUserAchievement",
    "SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime",
    "SteamAPI_ISteamUserStats_GetUserStatFloat",
    "SteamAPI_ISteamUserStats_GetUserStatInt32",
    "SteamAPI_ISteamUserStats_IndicateAchievementProgress",
    "SteamAPI_ISteamUserStats_RequestCurrentStats",
    "SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages",
    "SteamAPI_ISteamUserStats_RequestGlobalStats",
    "SteamAPI_ISteamUserStats_RequestUserStats",
    "SteamAPI_ISteamUserStats_ResetAllStats",
    "SteamAPI_ISteamUserStats_SetAchievement",
    "SteamAPI_ISteamUserStats_SetStatFloat",
    "SteamAPI_ISteamUserStats_SetStatInt32",
    "SteamAPI_ISteamUserStats_StoreStats",
    "SteamAPI_ISteamUserStats_UpdateAvgRateStat",
    "SteamAPI_ISteamUserStats_UploadLeaderboardScore",
    "SteamAPI_ISteamUser_AdvertiseGame",
    "SteamAPI_ISteamUser_BIsBehindNAT",
    "SteamAPI_ISteamUser_BIsPhoneIdentifying",
    "SteamAPI_ISteamUser_BIsPhoneRequiringVerification",
    "SteamAPI_ISteamUser_BIsPhoneVerified",
    "SteamAPI_ISteamUser_BIsTwoFactorEnabled",
    "SteamAPI_ISteamUser_BLoggedOn",
    "SteamAPI_ISteamUser_BSetDurationControlOnlineState",
    "SteamAPI_ISteamUser_BeginAuthSession",
    "SteamAPI_ISteamUser_CancelAuthTicket",
    "SteamAPI_ISteamUser_DecompressVoice",
    "SteamAPI_ISteamUser_EndAuthSession",
    "SteamAPI_ISteamUser_GetAuthSessionTicket",
    "SteamAPI_ISteamUser_GetAvailableVoice",
    "SteamAPI_ISteamUser_GetDurationControl",
    "SteamAPI_ISteamUser_GetEncryptedAppTicket",
    "SteamAPI_ISteamUser_GetGameBadgeLevel",
    "SteamAPI_ISteamUser_GetHSteamUser",
    "SteamAPI_ISteamUser_GetMarketEligibility",
    "SteamAPI_ISteamUser_GetPlayerSteamLevel",
    "SteamAPI_ISteamUser_GetSteamID",
    "SteamAPI_ISteamUser_GetUserDataFolder",
    "SteamAPI_ISteamUser_GetVoice",
    "SteamAPI_ISteamUser_GetVoiceOptimalSampleRate",
    "SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED",
    "SteamAPI_ISteamUser_RequestEncryptedAppTicket",
    "SteamAPI_ISteamUser_RequestStoreAuthURL",
    "SteamAPI_ISteamUser_StartVoiceRecording",
    "SteamAPI_ISteamUser_StopVoiceRecording",
    "SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED",
    "SteamAPI_ISteamUser_TrackAppUsageEvent",
    "SteamAPI_ISteamUser_UserHasLicenseForApp",
    "SteamAPI_ISteamUtils_BOverlayNeedsPresent",
    "SteamAPI_ISteamUtils_CheckFileSignature",
    "SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput",
    "SteamAPI_ISteamUtils_FilterText",
    "SteamAPI_ISteamUtils_GetAPICallFailureReason",
    "SteamAPI_ISteamUtils_GetAPICallResult",
    "SteamAPI_ISteamUtils_GetAppID",
    "SteamAPI_ISteamUtils_GetConnectedUniverse",
    "SteamAPI_ISteamUtils_GetCurrentBatteryPower",
    "SteamAPI_ISteamUtils_GetEnteredGamepadTextInput",
    "SteamAPI_ISteamUtils_GetEnteredGamepadTextLength",
    "SteamAPI_ISteamUtils_GetIPCCallCount",
    "SteamAPI_ISteamUtils_GetIPCountry",
    "SteamAPI_ISteamUtils_GetIPv6ConnectivityState",
    "SteamAPI_ISteamUtils_GetImageRGBA",
    "SteamAPI_ISteamUtils_GetImageSize",
    "SteamAPI_ISteamUtils_GetSecondsSinceAppActive",
    "SteamAPI_ISteamUtils_GetSecondsSinceComputerActive",
    "SteamAPI_ISteamUtils_GetServerRealTime",
    "SteamAPI_ISteamUtils_GetSteamUILanguage",
    "SteamAPI_ISteamUtils_InitFilterText",
    "SteamAPI_ISteamUtils_IsAPICallCompleted",
    "SteamAPI_ISteamUtils_IsOverlayEnabled",
    "SteamAPI_ISteamUtils_IsSteamChinaLauncher",
    "SteamAPI_ISteamUtils_IsSteamInBigPictureMode",
    "SteamAPI_ISteamUtils_IsSteamRunningInVR",
    "SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck",
    "SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled",
    "SteamAPI_ISteamUtils_SetGameLauncherMode",
    "SteamAPI_ISteamUtils_SetOverlayNotificationInset",
    "SteamAPI_ISteamUtils_SetOverlayNotificationPosition",
    "SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled",
    "SteamAPI_ISteamUtils_SetWarningMessageHook",
    "SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput",
    "SteamAPI_ISteamUtils_ShowGamepadTextInput",
    "SteamAPI_ISteamUtils_StartVRDashboard",
    "SteamAPI_ISteamVideo_GetOPFSettings",
    "SteamAPI_ISteamVideo_GetOPFStringForApp",
    "SteamAPI_ISteamVideo_GetVideoURL",
    "SteamAPI_ISteamVideo_IsBroadcasting",
    "SteamAPI_IsSteamRunning",
    "SteamAPI_ManualDispatch_FreeLastCallback",
    "SteamAPI_ManualDispatch_GetAPICallResult",
    "SteamAPI_ManualDispatch_GetNextCallback",
    "SteamAPI_ManualDispatch_Init",
    "SteamAPI_ManualDispatch_RunFrame",
    "SteamAPI_MatchMakingKeyValuePair_t_Construct",
    "SteamAPI_RegisterCallResult",
    "SteamAPI_RegisterCallback",
    "SteamAPI_ReleaseCurrentThreadMemory",
    "SteamAPI_RunCallbacks",
    "SteamAPI_SetBreakpadAppID",
    "SteamAPI_SetMiniDumpComment",
    "SteamAPI_SetTryCatchCallbacks",
    "SteamAPI_Shutdown",
    "SteamAPI_SteamAppList_v001",
    "SteamAPI_SteamApps_v008",
    "SteamAPI_SteamController_v008",
    "SteamAPI_SteamDatagramHostedAddress_Clear",
    "SteamAPI_SteamDatagramHostedAddress_GetPopID",
    "SteamAPI_SteamDatagramHostedAddress_SetDevAddress",
    "SteamAPI_SteamFriends_v017",
    "SteamAPI_SteamGameSearch_v001",
    "SteamAPI_SteamGameServerHTTP_v003",
    "SteamAPI_SteamGameServerInventory_v003",
    "SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002",
    "SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012",
    "SteamAPI_SteamGameServerNetworking_v006",
    "SteamAPI_SteamGameServerStats_v001",
    "SteamAPI_SteamGameServerUGC_v016",
    "SteamAPI_SteamGameServerUtils_v010",
    "SteamAPI_SteamGameServer_v014",
    "SteamAPI_SteamHTMLSurface_v005",
    "SteamAPI_SteamHTTP_v003",
    "SteamAPI_SteamIPAddress_t_IsSet",
    "SteamAPI_SteamInput_v006",
    "SteamAPI_SteamInventory_v003",
    "SteamAPI_SteamMatchmakingServers_v002",
    "SteamAPI_SteamMatchmaking_v009",
    "SteamAPI_SteamMusicRemote_v001",
    "SteamAPI_SteamMusic_v001",
    "SteamAPI_SteamNetworkingConfigValue_t_SetFloat",
    "SteamAPI_SteamNetworkingConfigValue_t_SetInt32",
    "SteamAPI_SteamNetworkingConfigValue_t_SetInt64",
    "SteamAPI_SteamNetworkingConfigValue_t_SetPtr",
    "SteamAPI_SteamNetworkingConfigValue_t_SetString",
    "SteamAPI_SteamNetworkingIPAddr_Clear",
    "SteamAPI_SteamNetworkingIPAddr_GetFakeIPType",
    "SteamAPI_SteamNetworkingIPAddr_GetIPv4",
    "SteamAPI_SteamNetworkingIPAddr_IsEqualTo",
    "SteamAPI_SteamNetworkingIPAddr_IsFakeIP",
    "SteamAPI_SteamNetworkingIPAddr_IsIPv4",
    "SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros",
    "SteamAPI_SteamNetworkingIPAddr_IsLocalHost",
    "SteamAPI_SteamNetworkingIPAddr_ParseString",
    "SteamAPI_SteamNetworkingIPAddr_SetIPv4",
    "SteamAPI_SteamNetworkingIPAddr_SetIPv6",
    "SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost",
    "SteamAPI_SteamNetworkingIPAddr_ToString",
    "SteamAPI_SteamNetworkingIdentity_Clear",
    "SteamAPI_SteamNetworkingIdentity_GetFakeIPType",
    "SteamAPI_SteamNetworkingIdentity_GetGenericBytes",
    "SteamAPI_SteamNetworkingIdentity_GetGenericString",
    "SteamAPI_SteamNetworkingIdentity_GetIPAddr",
    "SteamAPI_SteamNetworkingIdentity_GetIPv4",
    "SteamAPI_SteamNetworkingIdentity_GetPSNID",
    "SteamAPI_SteamNetworkingIdentity_GetStadiaID",
    "SteamAPI_SteamNetworkingIdentity_GetSteamID",
    "SteamAPI_SteamNetworkingIdentity_GetSteamID64",
    "SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID",
    "SteamAPI_SteamNetworkingIdentity_IsEqualTo",
    "SteamAPI_SteamNetworkingIdentity_IsFakeIP",
    "SteamAPI_SteamNetworkingIdentity_IsInvalid",
    "SteamAPI_SteamNetworkingIdentity_IsLocalHost",
    "SteamAPI_SteamNetworkingIdentity_ParseString",
    "SteamAPI_SteamNetworkingIdentity_SetGenericBytes",
    "SteamAPI_SteamNetworkingIdentity_SetGenericString",
    "SteamAPI_SteamNetworkingIdentity_SetIPAddr",
    "SteamAPI_SteamNetworkingIdentity_SetIPv4Addr",
    "SteamAPI_SteamNetworkingIdentity_SetLocalHost",
    "SteamAPI_SteamNetworkingIdentity_SetPSNID",
    "SteamAPI_SteamNetworkingIdentity_SetStadiaID",
    "SteamAPI_SteamNetworkingIdentity_SetSteamID",
    "SteamAPI_SteamNetworkingIdentity_SetSteamID64",
    "SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID",
    "SteamAPI_SteamNetworkingIdentity_ToString",
    "SteamAPI_SteamNetworkingMessage_t_Release",
    "SteamAPI_SteamNetworkingMessages_SteamAPI_v002",
    "SteamAPI_SteamNetworkingSockets_SteamAPI_v012",
    "SteamAPI_SteamNetworkingUtils_SteamAPI_v004",
    "SteamAPI_SteamNetworking_v006",
    "SteamAPI_SteamParentalSettings_v001",
    "SteamAPI_SteamParties_v002",
    "SteamAPI_SteamRemotePlay_v001",
    "SteamAPI_SteamRemoteStorage_v016",
    "SteamAPI_SteamScreenshots_v003",
    "SteamAPI_SteamUGC_v016",
    "SteamAPI_SteamUserStats_v012",
    "SteamAPI_SteamUser_v021",
    "SteamAPI_SteamUtils_v010",
    "SteamAPI_SteamVideo_v002",
    "SteamAPI_UnregisterCallResult",
    "SteamAPI_UnregisterCallback",
    "SteamAPI_UseBreakpadCrashHandler",
    "SteamAPI_WriteMiniDump",
    "SteamAPI_gameserveritem_t_Construct",
    "SteamAPI_gameserveritem_t_GetName",
    "SteamAPI_gameserveritem_t_SetName",
    "SteamAPI_servernetadr_t_Assign",
    "SteamAPI_servernetadr_t_Construct",
    "SteamAPI_servernetadr_t_GetConnectionAddressString",
    "SteamAPI_servernetadr_t_GetConnectionPort",
    "SteamAPI_servernetadr_t_GetIP",
    "SteamAPI_servernetadr_t_GetQueryAddressString",
    "SteamAPI_servernetadr_t_GetQueryPort",
    "SteamAPI_servernetadr_t_Init",
    "SteamAPI_servernetadr_t_IsLessThan",
    "SteamAPI_servernetadr_t_SetConnectionPort",
    "SteamAPI_servernetadr_t_SetIP",
    "SteamAPI_servernetadr_t_SetQueryPort",
    "SteamClient",
    "SteamGameServer_BSecure",
    "SteamGameServer_GetHSteamPipe",
    "SteamGameServer_GetHSteamUser",
    "SteamGameServer_GetIPCCallCount",
    "SteamGameServer_GetSteamID",
    "SteamGameServer_InitSafe",
    "SteamGameServer_RunCallbacks",
    "SteamGameServer_Shutdown",
    "SteamInternal_ContextInit",
    "SteamInternal_CreateInterface",
    "SteamInternal_FindOrCreateGameServerInterface",
    "SteamInternal_FindOrCreateUserInterface",
    "SteamInternal_GameServer_Init",
    "g_pSteamClientGameServer",
    "SteamInternal_SteamAPI_Init",
    "SteamInternal_GameServer_Init_V2"
};

static HMODULE g_hOriginalDll = nullptr;
static HMODULE g_hSelfModule = nullptr;
static bool g_configLoaded = false;
static bool g_enableLogAllowed = false;
static char g_gameFilterSP[128] = "";  // Set dynamically by LoadConfig() from [Network] GameFilter or refix_game_<RealAppId>
static char g_maskAppId[16] = "480";
static uint32_t g_maskAppIdNum = 480;
static char g_realAppId[64] = "";
static uint32_t g_realAppIdNum = 0;    // Numeric RealAppId — used for workshop UGC and server browser
static char g_language[32] = "english";
static std::string g_hostPublicIP = "";
static std::string g_hostLocalIP = "";

// Function pointer typedefs
typedef bool(*fn_SteamAPI_Init_t)();
typedef bool(*fn_SteamAPI_InitSafe_t)();
typedef int(*fn_SteamAPI_InitFlat_t)(char* pOutErrMsg);
typedef bool(*fn_SteamAPI_RestartAppIfNecessary_t)(unsigned int);
typedef bool(*fn_SteamInternal_GameServer_Init_t)(uint32_t, uint16_t, uint16_t, int, const char*);
typedef bool(*fn_SteamGameServer_InitSafe_t)();
// SteamInternal_SteamAPI_Init: new SDK primary init (returns ESteamAPIInitResult enum, 0=OK)
typedef int(*fn_SteamInternal_SteamAPI_Init_t)(const char* pszInternalCheckInterfaceVersions, char* pOutErrMsg);

typedef const char* (*fn_GetPersonaName_t)(void* self);
typedef void* (*fn_SteamFriends_v017_t)();

typedef uint64_t(*fn_GetSteamID_t)(void* self);
typedef void* (*fn_SteamUser_v021_t)();

typedef void* (*fn_RequestServerList4_t)(void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse);
typedef void* (*fn_RequestLANServerList_t)(void* self, uint32_t iApp, void* pResponse);
typedef int (*fn_AddFavoriteGame_t)(void* self, uint32_t nAppID, uint32_t nIP, uint16_t nConnPort, uint16_t nQueryPort, uint32_t unFlags, uint32_t rTime32);

typedef void(*fn_SteamAPI_RegisterCallback_t)(void* pCallback, int iCallback);
typedef uint64_t(*fn_SteamAPI_ISteamMatchmaking_CreateLobby_t)(void* self, int eLobbyType, int cMaxMembers);
typedef bool(*fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t)(void* self, uint64_t steamIDLobby, const char* pchKey, const char* pchValue);
typedef uint64_t(*fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t)(void* self);
typedef void(*fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t)(void* self, const char* pchKeyToMatch, const char* pchValueToMatch, int eComparisonType);
typedef void(*fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_t)(void* self, int eLobbyDistanceFilter);
typedef bool(*fn_SteamAPI_ISteamFriends_SetRichPresence_t)(void* self, const char* pchKey, const char* pchValue);

// Lobby member query typedefs (used to register P2P peers)
typedef int     (*fn_GetNumLobbyMembers_t)(void* self, uint64_t steamIDLobby);
typedef uint64_t(*fn_GetLobbyMemberByIndex_t)(void* self, uint64_t steamIDLobby, int iMember);
typedef const char* (*fn_GetFriendPersonaName_t)(void* self, uint64_t steamIDFriend);
typedef bool    (*fn_GetFriendGamePlayed_t)(void* self, uint64_t steamIDFriend, void* pFriendGameInfo);
typedef uint64_t(*fn_GetLobbyOwner_t)(void* self, uint64_t steamIDLobby);
typedef const char* (*fn_GetLobbyData_t)(void* self, uint64_t steamIDLobby, const char* pchKey);

static fn_SteamAPI_Init_t g_pfn_Init = nullptr;
static fn_SteamAPI_InitSafe_t g_pfn_InitSafe = nullptr;
static fn_SteamAPI_InitFlat_t g_pfn_InitFlat = nullptr;
static fn_SteamAPI_Init_t g_pfn_InitAnon = nullptr;
static fn_SteamAPI_RestartAppIfNecessary_t g_pfn_Restart = nullptr;
static fn_SteamInternal_GameServer_Init_t g_pfn_GSInit = nullptr;
static fn_SteamGameServer_InitSafe_t g_pfn_GSInitSafe = nullptr;
static fn_SteamInternal_SteamAPI_Init_t g_pfn_SteamAPIInit_Internal = nullptr;

static fn_GetPersonaName_t g_pfn_GetPersonaName = nullptr;
static fn_SteamFriends_v017_t g_pfn_SteamFriends = nullptr;
static fn_GetSteamID_t g_pfn_GetSteamID = nullptr;
static fn_SteamUser_v021_t g_pfn_SteamUser = nullptr;

static fn_RequestServerList4_t g_pfn_RequestFavoritesServerList = nullptr;
static fn_RequestServerList4_t g_pfn_RequestFriendsServerList = nullptr;
static fn_RequestServerList4_t g_pfn_RequestHistoryServerList = nullptr;
static fn_RequestServerList4_t g_pfn_RequestInternetServerList = nullptr;
static fn_RequestLANServerList_t g_pfn_RequestLANServerList = nullptr;
static fn_RequestServerList4_t g_pfn_RequestSpectatorServerList = nullptr;
static fn_AddFavoriteGame_t g_pfn_AddFavoriteGame = nullptr;

static fn_SteamAPI_ISteamMatchmaking_CreateLobby_t g_pfn_CreateLobby = nullptr;

// Forward declarations of exported interceptor functions
extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestInternetServerList(void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse);
extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList(void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse);
extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList(void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse);
extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList(void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse);
extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList(void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse);
extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestLANServerList(void* self, uint32_t iApp, void* pResponse);
extern "C" __declspec(dllexport) uint32_t SteamAPI_ISteamUtils_GetAppID(void* self);

typedef void (*fn_SteamAPI_Shutdown_t)();
static fn_SteamAPI_Shutdown_t g_pfn_Shutdown = nullptr;
extern "C" __declspec(dllexport) void SteamAPI_Shutdown();

extern "C" __declspec(dllexport) void SteamAPI_RunCallbacks();
extern "C" __declspec(dllexport) void SteamAPI_ManualDispatch_RunFrame(uint32_t hSteamPipe);
extern "C" __declspec(dllexport) bool SteamAPI_ManualDispatch_GetNextCallback(uint32_t hSteamPipe, void* pCallbackMsg);
extern "C" __declspec(dllexport) uint32_t SteamAPI_ISteamUser_GetAuthSessionTicket(void* self, void* pTicket, int cbMaxTicket, uint32_t* pcbTicket);
extern "C" __declspec(dllexport) uint32_t SteamAPI_ISteamUser_GetAuthTicketForWebApi(void* self, const char* pchIdentity);
typedef uint64_t(*fn_SteamAPI_ISteamMatchmaking_JoinLobby_t)(void* self, uint64_t steamIDLobby);
static fn_SteamAPI_ISteamMatchmaking_JoinLobby_t g_pfn_JoinLobby = nullptr;
static fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t g_pfn_SetLobbyData = nullptr;
static fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t g_pfn_RequestLobbyList = nullptr;
static fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t g_pfn_AddRequestLobbyListStringFilter = nullptr;
static fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_t g_pfn_AddRequestLobbyListDistanceFilter = nullptr;
typedef void(*fn_SteamAPI_RegisterCallResult_t)(void* pCallback, uint64_t hAPICall);
static fn_SteamAPI_RegisterCallback_t g_pfn_RegisterCallback = nullptr;
static fn_SteamAPI_RegisterCallResult_t g_pfn_RegisterCallResult = nullptr;
static fn_SteamAPI_ISteamFriends_SetRichPresence_t g_pfn_SetRichPresence = nullptr;

static fn_GetNumLobbyMembers_t    g_pfn_GetNumLobbyMembers    = nullptr;
static fn_GetLobbyMemberByIndex_t g_pfn_GetLobbyMemberByIndex = nullptr;
static fn_GetLobbyOwner_t         g_pfn_GetLobbyOwner         = nullptr;
static fn_GetLobbyData_t          g_pfn_GetLobbyData          = nullptr;

// Currently tracked Steam lobby ID (for peer scanning)
static uint64_t g_activeLobbyID = 0;
static uint64_t g_capturedSteamID = 0;

// Forward declaration (defined later in this file)
void ReFixLog(const char* fmt, ...);
extern "C" void ReFix_NotifyLobbyID(uint64_t lobbyID);

#pragma pack(push, 8)
struct LobbyCreated_t {
    enum { k_iCallback = 513 };
    int32_t m_eResult;       // 0x00: 1 = k_EResultOK
    uint64_t m_ulSteamIDLobby; // 0x08: 64-bit Steam ID of created lobby
};

// LobbyEnter_t: fired when the local user joins a lobby (iCallback=504)
// GodotSteam's steam-multiplayer-peer listens for this to know join succeeded,
// then calls ISteamNetworkingSockets::ConnectP2P(hostSteamID, 0).
struct LobbyEnter_t {
    enum { k_iCallback = 504 };
    uint64_t m_ulSteamIDLobby;  // 0x00: lobby entered
    uint32_t m_rgfChatPermissions; // 0x08
    bool     m_bLocked;         // 0x0C
    uint32_t m_EChatRoomEnterResponse; // 0x10: 1=success
};
#pragma pack(pop)

static uint64_t ExtractLobbyIDFromParam(void* pvParam, const char* srcName) {
    if (!pvParam) return 0;

    uint8_t* b = (uint8_t*)pvParam;
    uint32_t* u32 = (uint32_t*)pvParam;
    uint64_t* u64 = (uint64_t*)pvParam;

    ReFixLog("%s pvParam RAW DUMP: u32[0]=%u (0x%X), u32[1]=%u (0x%X), u32[2]=%u (0x%X), u32[3]=%u (0x%X)",
             srcName, u32[0], u32[0], u32[1], u32[1], u32[2], u32[2], u32[3], u32[3]);
    ReFixLog("  Bytes[0..15]: %02X %02X %02X %02X | %02X %02X %02X %02X | %02X %02X %02X %02X | %02X %02X %02X %02X",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);

    uint64_t foundLobbyID = 0;

    // Standard 8-byte aligned struct: m_eResult (0..3), pad (4..7), m_ulSteamIDLobby (8..15)
    LobbyCreated_t* pStandard = (LobbyCreated_t*)pvParam;
    if (pStandard->m_ulSteamIDLobby != 0) {
        foundLobbyID = pStandard->m_ulSteamIDLobby;
        ReFixLog("  -> Extracted LobbyID (pack 8): %llu", foundLobbyID);
    }

    // 4-byte packed struct: m_eResult (0..3), m_ulSteamIDLobby (4..11)
    if (foundLobbyID == 0) {
        uint64_t packed4ID = *(uint64_t*)(b + 4);
        if (packed4ID != 0) {
            foundLobbyID = packed4ID;
            ReFixLog("  -> Extracted LobbyID (pack 4): %llu", foundLobbyID);
        }
    }

    // Direct scan across first 24 bytes for valid 64-bit Steam ID (> 100000000000000ULL)
    if (foundLobbyID == 0) {
        for (int i = 0; i < 3; i++) {
            if (u64[i] > 100000000000000ULL) {
                foundLobbyID = u64[i];
                ReFixLog("  -> Extracted LobbyID via u64[%d] scan: %llu", i, foundLobbyID);
                break;
            }
        }
    }

    return foundLobbyID;
}

class CRefixLobbyCreatedCallResult {
public:
    void** m_pVtable;
    uint8_t m_nCallbackFlags;
    int32_t m_iCallback;
    uint64_t m_hAPICall;

    CRefixLobbyCreatedCallResult() {
        m_pVtable = *(void***)this;
        m_nCallbackFlags = 0;
        m_iCallback = LobbyCreated_t::k_iCallback;
        m_hAPICall = 0;
    }

    virtual void Run(void* pvParam) {
        if (!pvParam) return;
        uint64_t lobbyID = ExtractLobbyIDFromParam(pvParam, "CCallResult[LobbyCreated_t]");
        if (lobbyID != 0) {
            ReFix_NotifyLobbyID(lobbyID);
        }
    }

    virtual void Run(void* pvParam, bool bIOFailure, uint64_t hSteamAPICall) {
        Run(pvParam);
    }

    virtual int GetCallbackSizeBytes() {
        return sizeof(LobbyCreated_t);
    }
};

static CRefixLobbyCreatedCallResult g_lobbyCreatedCallResult;

// =============================================================================
// Scan lobby members and register P2P peer mappings
// Called whenever a lobby member joins or lobby data changes
// =============================================================================
// =============================================================================
// Godot (GodotSteam) — Synthetic LobbyEnter_t callback injection
// =============================================================================
// When EngineType=Godot, the game uses steam-multiplayer-peer.dll which
// listens for LobbyEnter_t (iCallback=504) to know the lobby join succeeded,
// and only then calls ISteamNetworkingSockets::ConnectP2P(hostSteamID, 0).
//
// When our lobby is synthetic (not a real Steam lobby), JoinLobby() returns
// without firing LobbyEnter_t — causing a timeout. We inject the callback
// manually so Godot can proceed with the P2P connection.
// =============================================================================
static bool g_godotIsEngine = false;  // Set to true once config is loaded and EngineType=Godot
static bool g_unrealIsEngine = false; // Set to true once config is loaded and EngineType=Unreal
static bool g_isGoldbergMode = false; // Set to true if Online Mode=goldberg/offline/lan

struct CRefixCallback {
    void** m_pVtable;
    uint8_t m_nCallbackFlags;
    int m_iCallback;
};

// Registered LobbyEnter_t callback pointers captured from the game
struct GodotCallbackEntry {
    void* pCallback;
    int iCallback;
};
static std::vector<GodotCallbackEntry> g_registeredCallbacks;
static std::mutex g_callbackMutex;

// Called from Intercepted_SteamAPI_RegisterCallback to track registered callbacks
static void TrackCallback(void* pCallback, int iCallback) {
    std::lock_guard<std::mutex> lg(g_callbackMutex);
    for (auto& e : g_registeredCallbacks) {
        if (e.pCallback == pCallback) { e.iCallback = iCallback; return; }
    }
    g_registeredCallbacks.push_back({ pCallback, iCallback });
}

static void UntrackCallback(void* pCallback) {
    std::lock_guard<std::mutex> lg(g_callbackMutex);
    for (auto it = g_registeredCallbacks.begin(); it != g_registeredCallbacks.end(); ) {
        if (it->pCallback == pCallback) {
            it = g_registeredCallbacks.erase(it);
        } else {
            ++it;
        }
    }
}

// Synthesize a LobbyEnter_t callback dispatch into all registered listeners
static void SynthesizeLobbyEnterCallback(uint64_t lobbyID) {
    if (!lobbyID) return;

    LobbyEnter_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.m_ulSteamIDLobby         = lobbyID;
    evt.m_rgfChatPermissions     = 0;
    evt.m_bLocked                = false;
    evt.m_EChatRoomEnterResponse = 1; // k_EChatRoomEnterResponseSuccess

    ReFixLog("[Godot] SynthesizeLobbyEnterCallback: lobbyID=%llu, scanning registered callbacks", lobbyID);

    std::vector<GodotCallbackEntry> snapshot;
    {
        std::lock_guard<std::mutex> lg(g_callbackMutex);
        snapshot = g_registeredCallbacks;
    }

    int dispatched = 0;
    for (auto& entry : snapshot) {
        if (entry.iCallback != LobbyEnter_t::k_iCallback) continue;
        if (!entry.pCallback) continue;

        // The Steam callback object layout:
        // [0] vtable ptr  -> Run(void* pvParam) is vt[0]
        // Call Run(pvParam) via vtable slot 0
        void** vt = *(void***)entry.pCallback;
        if (!vt || !vt[0]) continue;

        using fn_Run_t = void(__thiscall*)(void*, void*);
        auto fn = (fn_Run_t)vt[0];
        fn(entry.pCallback, &evt);
        dispatched++;
        ReFixLog("[Godot]   -> Dispatched LobbyEnter_t to callback %p", entry.pCallback);
    }

    ReFixLog("[Godot] SynthesizeLobbyEnterCallback: dispatched to %d listeners", dispatched);
}

static void UpdateP2PPeers(uint64_t lobbyID) {
    if (!lobbyID) return;

    void* matchmaking = nullptr;
    typedef void* (*fn_SteamMatchmaking_t)();
    auto pfnMM = (fn_SteamMatchmaking_t)GetProcAddress((HMODULE)g_hOriginalDll,
        "SteamAPI_SteamMatchmaking_v009");
    if (!pfnMM) pfnMM = (fn_SteamMatchmaking_t)GetProcAddress((HMODULE)g_hOriginalDll,
        "SteamMatchmaking");
    if (pfnMM) matchmaking = pfnMM();
    if (!matchmaking) return;

    // Retrieve host IP from lobby data
    const char* serverIP = nullptr;
    if (g_pfn_GetLobbyData) {
        serverIP = g_pfn_GetLobbyData(matchmaking, lobbyID, "SERVER_IP");
        if (!serverIP || serverIP[0] == '\0')
            serverIP = g_pfn_GetLobbyData(matchmaking, lobbyID, "connect");
    }

    // Also register the lobby owner (Host)
    typedef uint64_t (*fn_GetLobbyOwner_t)(void*, uint64_t);
    auto pfnGetOwner = (fn_GetLobbyOwner_t)GetProcAddress((HMODULE)g_hOriginalDll,
        "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    if (pfnGetOwner) {
        uint64_t ownerID = pfnGetOwner(matchmaking, lobbyID);
        if (ownerID != 0) {
            uint32_t ownerIP = 0;
            if (serverIP && serverIP[0] != '\0') {
                struct in_addr addr;
                if (inet_pton(AF_INET, serverIP, &addr) == 1) {
                    ownerIP = ntohl(addr.s_addr);
                }
            }
            if (ownerIP == 0) {
                ownerIP = 0x0A000001u | (uint32_t)(ownerID & 0x00FFFFFFu);
            }
            if (!g_godotIsEngine) {
                SteamP2PHook::RegisterPeer(ownerID, ownerIP);
            }
        }
    }

    if (!g_pfn_GetNumLobbyMembers || !g_pfn_GetLobbyMemberByIndex) return;
    int count = g_pfn_GetNumLobbyMembers(matchmaking, lobbyID);
    ReFixLog("UpdateP2PPeers: lobby=%llu members=%d (serverIP='%s')", lobbyID, count, serverIP ? serverIP : "");

    for (int i = 0; i < count; i++) {
        uint64_t memberID = g_pfn_GetLobbyMemberByIndex(matchmaking, lobbyID, i);
        if (!memberID) continue;

        // Resolve IP: use SERVER_IP from lobby data for the host; for other members
        // we derive a synthetic IP from their SteamID so sendto can find them.
        uint32_t ip4 = 0;
        if (serverIP && serverIP[0] != '\0') {
            // Parse the host IP
            struct in_addr addr;
            if (inet_pton(AF_INET, serverIP, &addr) == 1) {
                ip4 = ntohl(addr.s_addr);
            }
        }
        if (ip4 == 0) {
            // Synthetic: use low 24 bits of SteamID offset from 10.x.x.x range
            ip4 = 0x0A000001u | (uint32_t)(memberID & 0x00FFFFFFu);
        }

        // Skip Winsock peer registration for Godot — it uses ISteamNetworkingSockets, not raw UDP
        if (!g_godotIsEngine) {
            SteamP2PHook::RegisterPeer(memberID, ip4);
        }
    }
}

static std::string GetExeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string dir(path);
    size_t pos = dir.find_last_of("\\/");
    return (pos != std::string::npos) ? dir.substr(0, pos + 1) : ".\\";
}

static std::string GetProxyDllDir() {
    char path[MAX_PATH] = { 0 };
    if (g_hSelfModule) {
        GetModuleFileNameA(g_hSelfModule, path, MAX_PATH);
    } else {
        GetModuleFileNameA(NULL, path, MAX_PATH);
    }
    std::string dir(path);
    size_t pos = dir.find_last_of("\\/");
    return (pos != std::string::npos) ? dir.substr(0, pos + 1) : ".\\";
}

struct ReFixConfig {
    std::string gameName = "Shift At Midnight";
    std::string engineType = "Unity";

    std::string maskAppId = "480";
    uint32_t maskAppIdNum = 480;
    std::string realAppId = "";
    uint32_t realAppIdNum = 0;
    std::string language = "english";
    bool bypassLicenseCheck = true;

    bool enableLobbyFilter = false;
    char lobbyFilterKey[64] = "game_filter";
    char lobbyFilterValue[128] = "";
    std::string lobbyDistanceFilterStr = "worldwide";
    uint32_t lobbyDistanceFilterEnum = 3;
    uint32_t maxLobbyResults = 50;

    bool overrideServerListAppId = false;
    uint32_t serverListAppIdNum = 0;

    bool enableOverlay = true;
    std::string overlayAppId = "480";

    std::string workshopAppId = "";
    uint32_t workshopAppIdNum = 0;
    bool autoCreateSteamAppIdFile = true;

    bool enableLog = true;
    bool enableConsole = false;
    bool enableServerBrowser = true;
};

static ReFixConfig g_config;

static void LoadConfig() {
    if (g_configLoaded) return;
    g_configLoaded = true;

    std::string ini = GetExeDir() + "ReFix.ini";
    DWORD attrib = GetFileAttributesA(ini.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        ini = GetProxyDllDir() + "ReFix.ini";
    }

    auto ReadBool = [&](const char* section, const char* key, bool defaultVal) -> bool {
        char buf[64];
        GetPrivateProfileStringA(section, key, defaultVal ? "true" : "false", buf, sizeof(buf), ini.c_str());
        return (_stricmp(buf, "true") == 0 || strcmp(buf, "1") == 0 || _stricmp(buf, "yes") == 0);
    };

    auto ReadString = [&](const char* section, const char* key, const char* defaultVal, char* outBuf, size_t outSize) {
        GetPrivateProfileStringA(section, key, defaultVal, outBuf, (DWORD)outSize, ini.c_str());
    };

    // [Online]
    char bufOnlineMode[64];
    ReadString("Online", "Mode", "valve", bufOnlineMode, sizeof(bufOnlineMode));
    g_isGoldbergMode = (_stricmp(bufOnlineMode, "goldberg") == 0 || _stricmp(bufOnlineMode, "offline") == 0 || _stricmp(bufOnlineMode, "lan") == 0);

    // [Game]
    char bufGameName[128], bufEngine[64];
    ReadString("Game", "GameName", "Shift At Midnight", bufGameName, sizeof(bufGameName));
    ReadString("Game", "EngineType", "Unity", bufEngine, sizeof(bufEngine));
    g_config.gameName = bufGameName;
    g_config.engineType = bufEngine;
    g_godotIsEngine = (_stricmp(g_config.engineType.c_str(), "Godot") == 0);
    g_unrealIsEngine = (_stricmp(g_config.engineType.c_str(), "Unreal") == 0);

    // [Steam]
    char bufMask[64], bufReal[64], bufLang[64];
    ReadString("Steam", "MaskAppId", "480", bufMask, sizeof(bufMask));
    ReadString("Steam", "RealAppId", "", bufReal, sizeof(bufReal));
    ReadString("Steam", "Language", "english", bufLang, sizeof(bufLang));

    g_config.maskAppId = bufMask;
    g_config.maskAppIdNum = (uint32_t)atoi(bufMask);
    if (g_config.maskAppIdNum == 0) g_config.maskAppIdNum = 480;

    g_config.realAppId = bufReal;
    g_config.realAppIdNum = (uint32_t)atoi(bufReal);

    g_config.language = bufLang;
    g_config.bypassLicenseCheck = ReadBool("Steam", "BypassLicenseCheck", true);

    // Sync legacy globals for full compatibility
    strcpy_s(g_maskAppId, sizeof(g_maskAppId), g_config.maskAppId.c_str());
    g_maskAppIdNum = g_config.maskAppIdNum;
    strcpy_s(g_realAppId, sizeof(g_realAppId), g_config.realAppId.c_str());
    g_realAppIdNum = g_config.realAppIdNum;
    strcpy_s(g_language, sizeof(g_language), g_config.language.c_str());

    // [Matchmaking]
    g_config.enableLobbyFilter = ReadBool("Matchmaking", "EnableLobbyFilter", false);
    ReadString("Matchmaking", "LobbyFilterKey", "game_filter", g_config.lobbyFilterKey, sizeof(g_config.lobbyFilterKey));
    ReadString("Matchmaking", "LobbyFilterValue", "", g_config.lobbyFilterValue, sizeof(g_config.lobbyFilterValue));

    // Also check legacy [Network] GameFilter
    char bufLegacyFilter[128] = "";
    ReadString("Network", "GameFilter", "", bufLegacyFilter, sizeof(bufLegacyFilter));
    if (bufLegacyFilter[0] != '\0') {
        strcpy_s(g_config.lobbyFilterValue, sizeof(g_config.lobbyFilterValue), bufLegacyFilter);
    }

    if (g_config.lobbyFilterValue[0] == '\0') {
        if (g_config.realAppIdNum != 0) {
            sprintf_s(g_config.lobbyFilterValue, sizeof(g_config.lobbyFilterValue), "%u", g_config.realAppIdNum);
        } else {
            strcpy_s(g_config.lobbyFilterValue, sizeof(g_config.lobbyFilterValue), "480");
        }
    }
    strcpy_s(g_gameFilterSP, sizeof(g_gameFilterSP), g_config.lobbyFilterValue);
    SetEnvironmentVariableA("REFIX_GAME_FILTER", g_gameFilterSP);

    char bufDist[64];
    ReadString("Matchmaking", "LobbyDistanceFilter", "worldwide", bufDist, sizeof(bufDist));
    g_config.lobbyDistanceFilterStr = bufDist;
    if (_stricmp(bufDist, "close") == 0) g_config.lobbyDistanceFilterEnum = 0;
    else if (_stricmp(bufDist, "default") == 0) g_config.lobbyDistanceFilterEnum = 1;
    else if (_stricmp(bufDist, "far") == 0) g_config.lobbyDistanceFilterEnum = 2;
    else g_config.lobbyDistanceFilterEnum = 3; // worldwide

    g_config.maxLobbyResults = (uint32_t)GetPrivateProfileIntA("Matchmaking", "MaxLobbyResults", 50, ini.c_str());

    // [ServerBrowser]
    g_config.overrideServerListAppId = ReadBool("ServerBrowser", "OverrideServerListAppId", false);
    uint32_t sbAppId = (uint32_t)GetPrivateProfileIntA("ServerBrowser", "ServerListAppId", 0, ini.c_str());
    g_config.serverListAppIdNum = (sbAppId != 0) ? sbAppId : (g_config.realAppIdNum != 0 ? g_config.realAppIdNum : g_config.maskAppIdNum);

    // [Overlay]
    if (g_isGoldbergMode) {
        g_config.enableOverlay = false;
    } else {
        g_config.enableOverlay = ReadBool("Overlay", "EnableOverlay", true);
    }
    char bufOverlayAppId[64];
    ReadString("Overlay", "OverlayAppId", "480", bufOverlayAppId, sizeof(bufOverlayAppId));
    g_config.overlayAppId = bufOverlayAppId;

    // [Workshop]
    char bufWorkshopAppId[64];
    ReadString("Workshop", "WorkshopAppId", "", bufWorkshopAppId, sizeof(bufWorkshopAppId));
    g_config.workshopAppId = bufWorkshopAppId;
    g_config.workshopAppIdNum = (uint32_t)atoi(bufWorkshopAppId);
    if (g_config.workshopAppIdNum == 0) g_config.workshopAppIdNum = (g_config.realAppIdNum != 0 ? g_config.realAppIdNum : g_config.maskAppIdNum);
    g_config.autoCreateSteamAppIdFile = ReadBool("Workshop", "AutoCreateSteamAppIdFile", true);

    // [Debug]
    g_config.enableLog = ReadBool("Debug", "EnableLog", true);
    g_enableLogAllowed = g_config.enableLog;
    g_config.enableConsole = ReadBool("Debug", "EnableConsole", false);
    g_config.enableServerBrowser = ReadBool("Debug", "EnableServerBrowser", true);
}

void ReFixLog(const char* fmt, ...) {
    LoadConfig();
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsprintf_s(buf, sizeof(buf), fmt, args);
    va_end(args);

    HWND hCons = GetConsoleWindow();
    if (hCons) {
        printf("[SteamProxy] %s\n", buf);
        fflush(stdout);
    }

    if (!g_enableLogAllowed) return;

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string logPath(exePath);
    size_t pos = logPath.find_last_of("\\/");
    if (pos != std::string::npos) logPath = logPath.substr(0, pos + 1) + "ReFix.log";

    FILE* f = nullptr;
    fopen_s(&f, logPath.c_str(), "a");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [steam_api64] %s\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);

    fclose(f);
}

#pragma comment(lib, "advapi32.lib")

static bool InjectSteamOverlay() {
#ifdef _WIN64
    const char* overlayDllName = "GameOverlayRenderer64.dll";
#else
    const char* overlayDllName = "GameOverlayRenderer.dll";
#endif

    if (GetModuleHandleA(overlayDllName) != NULL) {
        ReFixLog("InjectSteamOverlay: '%s' is already loaded in process.", overlayDllName);
        return true;
    }

    char steamPath[MAX_PATH] = { 0 };
    DWORD pathSize = sizeof(steamPath);
    HKEY hKey = NULL;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "SteamPath", NULL, NULL, (LPBYTE)steamPath, &pathSize);
        RegCloseKey(hKey);
    }

    if (steamPath[0] == '\0') {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            pathSize = sizeof(steamPath);
            RegQueryValueExA(hKey, "InstallPath", NULL, NULL, (LPBYTE)steamPath, &pathSize);
            RegCloseKey(hKey);
        }
    }

    if (steamPath[0] == '\0') {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            pathSize = sizeof(steamPath);
            RegQueryValueExA(hKey, "InstallPath", NULL, NULL, (LPBYTE)steamPath, &pathSize);
            RegCloseKey(hKey);
        }
    }

    if (steamPath[0] == '\0') {
        ReFixLog("InjectSteamOverlay: Could not locate SteamPath in Registry.");
        return false;
    }

    std::string sPath(steamPath);
    for (size_t i = 0; i < sPath.size(); i++) {
        if (sPath[i] == '/') sPath[i] = '\\';
    }

    const char* targetAppId = g_config.overlayAppId.c_str();

    char pidStr[32];
    sprintf_s(pidStr, sizeof(pidStr), "%lu", GetCurrentProcessId());

    SetEnvironmentVariableA("SteamPath", sPath.c_str());
    SetEnvironmentVariableA("SteamAppId", targetAppId);
    SetEnvironmentVariableA("SteamGameId", targetAppId);
    SetEnvironmentVariableA("SteamOverlayGameId", targetAppId);
    SetEnvironmentVariableA("_SteamInjectionPIDs", pidStr);
    SetEnvironmentVariableA("ENABLE_STEAM_OVERLAY", "1");

    char eventName[128];
    sprintf_s(eventName, sizeof(eventName), "SteamOverlayRunning_%s", targetAppId);
    HANDLE hEvt = CreateEventA(NULL, TRUE, TRUE, eventName);
    (void)hEvt;

    std::string fullOverlayPath = sPath + "\\" + overlayDllName;
    ReFixLog("InjectSteamOverlay: Attempting to load '%s' (AppID=%s, PID=%s)...", fullOverlayPath.c_str(), targetAppId, pidStr);

    HMODULE hOverlay = LoadLibraryA(fullOverlayPath.c_str());
    if (hOverlay) {
        ReFixLog("InjectSteamOverlay: SUCCESS! Steam Overlay loaded successfully.");
        return true;
    } else {
        ReFixLog("InjectSteamOverlay: LoadLibraryA failed for '%s' (Error %lu)", fullOverlayPath.c_str(), GetLastError());
        return false;
    }
}

static void EnsureSteamAppIdFile(const char* appIdStr) {
    if (!g_config.autoCreateSteamAppIdFile) return;
    if (!appIdStr || appIdStr[0] == '\0') appIdStr = "480";

    auto writeAppId = [](const std::string& dir, const char* appid) {
        if (dir.empty()) return;
        std::string appIdPath = dir + "steam_appid.txt";
        FILE* fWrite = nullptr;
        fopen_s(&fWrite, appIdPath.c_str(), "w");
        if (fWrite) {
            fprintf(fWrite, "%s\n", appid);
            fclose(fWrite);
            ReFixLog("EnsureSteamAppIdFile: Updated steam_appid.txt (%s) at %s", appid, appIdPath.c_str());
        }
    };

    writeAppId(GetExeDir(), appIdStr);
    if (GetProxyDllDir() != GetExeDir()) {
        writeAppId(GetProxyDllDir(), appIdStr);
    }
}

static void ApplySteamEnv() {
    LoadConfig();
    if (g_isGoldbergMode) {
        std::string targetApp = (!g_config.realAppId.empty() && g_config.realAppId != "0") ? g_config.realAppId : g_config.maskAppId;
        if (targetApp.empty() || targetApp == "0") targetApp = "480";
        SetEnvironmentVariableA("SteamAppId", targetApp.c_str());
        SetEnvironmentVariableA("SteamGameId", targetApp.c_str());
        SetEnvironmentVariableA("SteamOverlayGameId", targetApp.c_str());
        SetEnvironmentVariableA("STEAM_COMPAT_APP_ID", targetApp.c_str());
        EnsureSteamAppIdFile(targetApp.c_str());
        ReFixLog("ApplySteamEnv (Goldberg LAN): AppId=%s", targetApp.c_str());
    } else {
        SetEnvironmentVariableA("SteamAppId", g_config.maskAppId.c_str());
        SetEnvironmentVariableA("SteamGameId", g_config.maskAppId.c_str());
        SetEnvironmentVariableA("SteamOverlayGameId", g_config.maskAppId.c_str());
        SetEnvironmentVariableA("STEAM_COMPAT_APP_ID", g_config.maskAppId.c_str());
        EnsureSteamAppIdFile(g_config.maskAppId.c_str());
        ReFixLog("ApplySteamEnv (Valve Online): MaskAppId=%s", g_config.maskAppId.c_str());
    }

    if (!g_config.language.empty()) {
        SetEnvironmentVariableA("SteamLanguage", g_config.language.c_str());
    }

    if (g_config.enableOverlay && !g_isGoldbergMode) {
        InjectSteamOverlay();
    }
}

// =============================================================================
// VTABLE HOOKING SYSTEM (MinHook on ISteamMatchmaking, ISteamMatchmakingServers, etc.)
// =============================================================================

typedef uint64_t(*fn_VTable_RequestLobbyList_t)(void* self);
typedef void(*fn_VTable_AddStringFilter_t)(void* self, const char* pchKeyToMatch, const char* pchValueToMatch, int eComparisonType);
typedef void(*fn_VTable_AddDistanceFilter_t)(void* self, int eLobbyDistanceFilter);
typedef uint64_t(*fn_VTable_CreateLobby_t)(void* self, int eLobbyType, int cMaxMembers);
typedef uint64_t(*fn_VTable_JoinLobby_t)(void* self, uint64_t steamIDLobby);
typedef bool(*fn_VTable_SetLobbyData_t)(void* self, uint64_t steamIDLobby, const char* pchKey, const char* pchValue);

typedef void*(*fn_VTable_RequestInternetServerList_t)(void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse);
typedef void*(*fn_VTable_RequestLANServerList_t)(void* self, uint32_t iApp, void* pResponse);

typedef uint32_t(*fn_VTable_GetAppID_t)(void* self);
typedef bool(*fn_VTable_BIsSubscribedApp_t)(void* self, uint32_t appID);
typedef bool(*fn_VTable_BIsDlcInstalled_t)(void* self, uint32_t appID);

typedef uint32_t(*fn_VTable_GetNumSubscribedItems_t)(void* self);
typedef uint32_t(*fn_VTable_GetSubscribedItems_t)(void* self, uint64_t* pvecPublishedFileID, uint32_t cMaxEntries);
typedef bool(*fn_VTable_GetItemInstallInfo_t)(void* self, uint64_t nPublishedFileID, uint64_t* punSizeOnDisk, char* pchFolder, uint32_t cchFolderSize, uint32_t* punTimeStamp);
typedef void(*fn_VTable_ActivateGameOverlayInviteDialog_t)(void* self, uint64_t steamIDLobby);
typedef bool(*fn_VTable_SetRichPresence_t)(void* self, const char* pchKey, const char* pchValue);

static fn_VTable_RequestLobbyList_t g_orig_VTable_RequestLobbyList = nullptr;
static fn_VTable_AddStringFilter_t g_orig_VTable_AddStringFilter = nullptr;
static fn_VTable_AddDistanceFilter_t g_orig_VTable_AddDistanceFilter = nullptr;
static fn_VTable_CreateLobby_t g_orig_VTable_CreateLobby = nullptr;
static fn_VTable_JoinLobby_t g_orig_VTable_JoinLobby = nullptr;
static fn_VTable_SetLobbyData_t g_orig_VTable_SetLobbyData = nullptr;

static fn_VTable_RequestInternetServerList_t g_orig_VTable_RequestInternetServerList = nullptr;
static fn_VTable_RequestLANServerList_t g_orig_VTable_RequestLANServerList = nullptr;

static fn_VTable_GetAppID_t g_orig_VTable_GetAppID = nullptr;
static fn_VTable_BIsSubscribedApp_t g_orig_VTable_BIsSubscribedApp = nullptr;
static fn_VTable_BIsDlcInstalled_t g_orig_VTable_BIsDlcInstalled = nullptr;

static fn_VTable_GetNumSubscribedItems_t g_orig_VTable_GetNumSubscribedItems = nullptr;
static fn_VTable_GetSubscribedItems_t g_orig_VTable_GetSubscribedItems = nullptr;
static fn_VTable_GetItemInstallInfo_t g_orig_VTable_GetItemInstallInfo = nullptr;
static fn_VTable_ActivateGameOverlayInviteDialog_t g_orig_VTable_ActivateGameOverlayInviteDialog = nullptr;
static fn_VTable_SetRichPresence_t g_orig_VTable_SetRichPresence = nullptr;

struct ReFix_FriendGameInfo_t {
    uint64_t m_gameID;
    uint32_t m_unGameIP;
    uint16_t m_usGamePort;
    uint16_t m_usQueryPort;
    uint64_t m_steamIDLobby;
};

typedef bool (*fn_VTable_GetFriendGamePlayed_t)(void* self, uint64_t steamIDFriend, void* pFriendGameInfo);
static fn_VTable_GetFriendGamePlayed_t g_orig_VTable_GetFriendGamePlayed = nullptr;

typedef bool (*fn_Flat_GetFriendGamePlayed_t)(void* self, uint64_t steamIDFriend, void* pFriendGameInfo);
static fn_Flat_GetFriendGamePlayed_t g_pfn_Flat_GetFriendGamePlayed = nullptr;

// === ISteamNetworkingUtils relay hook (fix: 'Checking Steam Relay...' hang) ===
// k_ESteamNetworkingAvailability_Current = 100 (in official Steam SDK & Steamworks.NET)
// ISteamNetworkingUtils vtable (v003+): [0]=AllocateMessage, [1]=GetRelayNetworkStatus
typedef int(__thiscall* fn_VTable_GetRelayNetworkStatus_t)(void* self, void* pDetails);
static fn_VTable_GetRelayNetworkStatus_t g_orig_VTable_GetRelayNetworkStatus = nullptr;

// =============================================================================
// ISteamNetworkingUtils::GetRelayNetworkStatus Hook
// =============================================================================
// Fixes the "Checking Steam Relay..." infinite hang for games using
// ISteamNetworkingSockets (spacewar / AppID 480 and similar).
//
// Root cause: the real Steam SDR network takes 1-5s to initialize after
// SteamAPI_Init(). Games polling GetRelayNetworkStatus() immediately, or
// waiting for SteamRelayNetworkStatus_t with m_eAvail==Current(100), hang.
//
// Fix: always return k_ESteamNetworkingAvailability_Current(100). The real
// SDR init continues in background; the game proceeds immediately.
// GENERAL: applies to ALL games using ISteamNetworkingUtils.
// =============================================================================
struct ReFix_SteamRelayNetworkStatus_t {
    int  m_eAvail;
    int  m_bPingMeasurementInProgress;
    int  m_eAvailNetworkConfig;
    int  m_eAvailAnyRelay;
    char m_debugMsg[256];
};
static const int k_eRelayAvail_Current = 100;
static DWORD g_steamInitTick = 0;

static int __fastcall Hooked_ISteamNetworkingUtils_GetRelayNetworkStatus(
    void* self, void* /*edx*/, void* pDetails)
{
    if (g_orig_VTable_GetRelayNetworkStatus)
        g_orig_VTable_GetRelayNetworkStatus(self, pDetails);
    
    DWORD elapsed = (g_steamInitTick != 0) ? (GetTickCount() - g_steamInitTick) : 1000;
    int avail = (elapsed < 500) ? 3 : k_eRelayAvail_Current;

    if (pDetails) {
        auto* d = static_cast<ReFix_SteamRelayNetworkStatus_t*>(pDetails);
        d->m_eAvail = avail;
        d->m_bPingMeasurementInProgress = (avail == 3) ? 1 : 0;
        d->m_eAvailNetworkConfig = avail;
        d->m_eAvailAnyRelay = avail;
        if (d->m_debugMsg[0] == '\0')
            strncpy_s(d->m_debugMsg, sizeof(d->m_debugMsg), (avail == 100) ? "OK (ReFix)" : "Connecting (ReFix)", _TRUNCATE);
    }
    return avail;
}
static bool HookVTableMethod(void* pInterface, int vtableIndex, void* pHookFn, void** ppOriginalFn) {
    if (!pInterface) return false;
    void** vtable = *(void***)pInterface;
    if (!vtable || !vtable[vtableIndex]) return false;
    void* pTarget = vtable[vtableIndex];
    if (MH_CreateHook(pTarget, pHookFn, ppOriginalFn) == MH_OK) {
        MH_EnableHook(pTarget);
        ReFixLog("VTable Hook Installed: Interface %p index %d (Target %p -> Hook %p)",
                 pInterface, vtableIndex, pTarget, pHookFn);
        return true;
    }
    return false;
}

static bool IsWorkshopItemCompatible(void* pUGC, uint64_t nPublishedFileID) {
    if (!pUGC || !g_orig_VTable_GetItemInstallInfo) return true;
    uint64_t sizeOnDisk = 0;
    char folder[MAX_PATH] = { 0 };
    uint32_t timestamp = 0;
    if (!g_orig_VTable_GetItemInstallInfo(pUGC, nPublishedFileID, &sizeOnDisk, folder, sizeof(folder), &timestamp)) {
        return true;
    }
    if (folder[0] == '\0') return true;

    std::string folderStr(folder);
    if (folderStr.back() != '\\' && folderStr.back() != '/') folderStr += "\\";

    char appidBuf[64] = { 0 };
    std::string refixAppIdFile = folderStr + "refix_appid.txt";
    FILE* f = nullptr;
    fopen_s(&f, refixAppIdFile.c_str(), "r");
    if (!f) {
        std::string steamAppIdFile = folderStr + "steam_appid.txt";
        fopen_s(&f, steamAppIdFile.c_str(), "r");
    }
    if (f) {
        if (fgets(appidBuf, sizeof(appidBuf), f)) {
            uint32_t fileAppId = (uint32_t)atoi(appidBuf);
            fclose(f);
            if (fileAppId != 0 && g_config.realAppIdNum != 0 && fileAppId != g_config.realAppIdNum) {
                ReFixLog("IsWorkshopItemCompatible: Item %llu belongs to AppID %u, skipping (realAppId=%u)",
                         nPublishedFileID, fileAppId, g_config.realAppIdNum);
                return false;
            }
        } else {
            fclose(f);
        }
    }
    return true;
}

static uint32_t Hooked_ISteamUGC_GetSubscribedItems(void* self, uint64_t* pvecPublishedFileID, uint32_t cMaxEntries) {
    if (!g_orig_VTable_GetSubscribedItems) return 0;
    uint32_t count = g_orig_VTable_GetSubscribedItems(self, pvecPublishedFileID, cMaxEntries);
    if (count == 0 || !pvecPublishedFileID) return count;

    uint32_t validCount = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (IsWorkshopItemCompatible(self, pvecPublishedFileID[i])) {
            pvecPublishedFileID[validCount++] = pvecPublishedFileID[i];
        }
    }
    ReFixLog("ISteamUGC::GetSubscribedItems Hook: total=%u, compatible=%u", count, validCount);
    return validCount;
}

static uint32_t Hooked_ISteamUGC_GetNumSubscribedItems(void* self) {
    if (!g_orig_VTable_GetNumSubscribedItems) return 0;
    if (!g_orig_VTable_GetSubscribedItems) return g_orig_VTable_GetNumSubscribedItems(self);

    uint32_t total = g_orig_VTable_GetNumSubscribedItems(self);
    if (total == 0) return 0;

    std::vector<uint64_t> items(total);
    uint32_t fetched = g_orig_VTable_GetSubscribedItems(self, items.data(), total);
    uint32_t validCount = 0;
    for (uint32_t i = 0; i < fetched; i++) {
        if (IsWorkshopItemCompatible(self, items[i])) {
            validCount++;
        }
    }
    ReFixLog("ISteamUGC::GetNumSubscribedItems Hook: total=%u, compatible=%u", total, validCount);
    return validCount;
}

static void Hooked_ISteamFriends_ActivateGameOverlayInviteDialog(void* self, uint64_t steamIDLobby) {
    ReFixLog("ISteamFriends::ActivateGameOverlayInviteDialog Hook called for lobby=%llu", steamIDLobby);
    if (g_orig_VTable_ActivateGameOverlayInviteDialog) {
        g_orig_VTable_ActivateGameOverlayInviteDialog(self, steamIDLobby);
    }
}

static bool Hooked_ISteamFriends_SetRichPresence(void* self, const char* pchKey, const char* pchValue) {
    ReFixLog("ISteamFriends::SetRichPresence Hook: Key='%s', Value='%s'",
             pchKey ? pchKey : "", pchValue ? pchValue : "");
    if (g_orig_VTable_SetRichPresence) {
        return g_orig_VTable_SetRichPresence(self, pchKey, pchValue);
    }
    return true;
}

static void NormalizeFriendGameInfo(void* pFriendGameInfo, uint64_t steamIDFriend) {
    if (!pFriendGameInfo) return;
    auto* info = (ReFix_FriendGameInfo_t*)pFriendGameInfo;
    uint32_t fid = (uint32_t)(info->m_gameID & 0x00FFFFFF);
    if (fid != 0) {
        if (fid == 480 || fid == g_config.maskAppIdNum || fid == g_config.realAppIdNum) {
            uint32_t myAppId = (g_config.realAppIdNum != 0) ? g_config.realAppIdNum : g_config.maskAppIdNum;
            info->m_gameID = (info->m_gameID & 0xFFFFFFFFFF000000ULL) | (uint64_t)myAppId;
            ReFixLog("GetFriendGamePlayed: Normalized friend %llu AppID (%u -> %u, Lobby=%llu)", steamIDFriend, fid, myAppId, info->m_steamIDLobby);
        }
    }
}

static bool Hooked_ISteamFriends_GetFriendGamePlayed(void* self, uint64_t steamIDFriend, void* pFriendGameInfo) {
    bool res = false;
    if (g_orig_VTable_GetFriendGamePlayed) {
        res = g_orig_VTable_GetFriendGamePlayed(self, steamIDFriend, pFriendGameInfo);
    }
    if (res) NormalizeFriendGameInfo(pFriendGameInfo, steamIDFriend);
    return res;
}

static bool Intercepted_SteamAPI_ISteamFriends_GetFriendGamePlayed(void* self, uint64_t steamIDFriend, void* pFriendGameInfo) {
    bool res = false;
    if (g_pfn_Flat_GetFriendGamePlayed) {
        res = g_pfn_Flat_GetFriendGamePlayed(self, steamIDFriend, pFriendGameInfo);
    }
    if (res) NormalizeFriendGameInfo(pFriendGameInfo, steamIDFriend);
    return res;
}

static uint64_t Hooked_ISteamMatchmaking_RequestLobbyList(void* self) {
    ReFixLog("ISteamMatchmaking::RequestLobbyList Hook called (self=%p)", self);
    if (self) {
        if (g_orig_VTable_AddDistanceFilter) {
            g_orig_VTable_AddDistanceFilter(self, g_config.lobbyDistanceFilterEnum);
            ReFixLog("  -> Applied DistanceFilter=%d (%s)", g_config.lobbyDistanceFilterEnum, g_config.lobbyDistanceFilterStr.c_str());
        } else if (g_pfn_AddRequestLobbyListDistanceFilter) {
            g_pfn_AddRequestLobbyListDistanceFilter(self, g_config.lobbyDistanceFilterEnum);
        }

        if (g_config.enableLobbyFilter && g_config.lobbyFilterKey[0] != '\0' && g_config.lobbyFilterValue[0] != '\0') {
            if (g_orig_VTable_AddStringFilter) {
                g_orig_VTable_AddStringFilter(self, g_config.lobbyFilterKey, g_config.lobbyFilterValue, 0);
                ReFixLog("  -> Applied StringFilter '%s'='%s'", g_config.lobbyFilterKey, g_config.lobbyFilterValue);
            } else if (g_pfn_AddRequestLobbyListStringFilter) {
                g_pfn_AddRequestLobbyListStringFilter(self, g_config.lobbyFilterKey, g_config.lobbyFilterValue, 0);
            }
        }
    }

    uint64_t hCall = 0;
    if (g_orig_VTable_RequestLobbyList) {
        hCall = g_orig_VTable_RequestLobbyList(self);
    } else if (g_pfn_RequestLobbyList) {
        hCall = g_pfn_RequestLobbyList(self);
    }
    ReFixLog("  -> RequestLobbyList APICall handle: %llu", hCall);
    return hCall;
}

static uint64_t Hooked_ISteamMatchmaking_CreateLobby(void* self, int eLobbyType, int cMaxMembers) {
    int safeMaxMembers = cMaxMembers;
    if (safeMaxMembers <= 0 || safeMaxMembers > 250) {
        safeMaxMembers = 20;
        ReFixLog("ISteamMatchmaking::CreateLobby Hook called with invalid MaxMembers (%d), adjusted to %d (Type=%d)",
                 cMaxMembers, safeMaxMembers, eLobbyType);
    } else {
        ReFixLog("ISteamMatchmaking::CreateLobby Hook called (Type=%d, MaxMembers=%d)", eLobbyType, safeMaxMembers);
    }

    uint64_t hResult = 0;
    if (g_orig_VTable_CreateLobby) {
        hResult = g_orig_VTable_CreateLobby(self, eLobbyType, safeMaxMembers);
    } else if (g_pfn_CreateLobby) {
        hResult = g_pfn_CreateLobby(self, eLobbyType, safeMaxMembers);
    }
    ReFixLog("  -> CreateLobby APICall handle: %llu", hResult);

    std::thread([]() {
        // Wait before fallback — Godot/Unreal lobby creation can take up to 2-3s
        int waitMs = (g_godotIsEngine || g_unrealIsEngine) ? 3000 : 1000;
        Sleep(waitMs);
        if (g_activeLobbyID == 0) {
            uint64_t userSteamID = g_capturedSteamID != 0 ? g_capturedSteamID : 76561198362393833ULL;
            uint64_t syntheticLobbyID = 0x0110000100000000ULL | (userSteamID & 0xFFFFFFFFULL);
            ReFixLog("  -> CreateLobby Fallback: Active Lobby ID still 0 after %dms. Assigning Synthetic Lobby ID: %llu", waitMs, syntheticLobbyID);
            ReFix_NotifyLobbyID(syntheticLobbyID);
        }
    }).detach();

    return hResult;
}

static uint64_t Hooked_ISteamMatchmaking_JoinLobby(void* self, uint64_t steamIDLobby) {
    LoadConfig();
    uint32_t realApp = (g_config.realAppIdNum != 0) ? g_config.realAppIdNum : g_config.maskAppIdNum;
    uint32_t maskApp = g_config.maskAppIdNum;

    ReFixLog("ISteamMatchmaking::JoinLobby Hook called:");
    ReFixLog("  LobbyID: %llu", steamIDLobby);
    ReFixLog("  RealAppId: %u | MaskAppId: %u", realApp, maskApp);
    ReFixLog("  Interface (self): %p", self);
    ReFixLog("  g_orig_VTable_JoinLobby: %p", g_orig_VTable_JoinLobby);
    ReFixLog("  g_pfn_JoinLobby: %p", g_pfn_JoinLobby);

    ReFix_NotifyLobbyID(steamIDLobby);

    const char* targetName = "None";
    void* targetAddr = nullptr;
    uint64_t hCall = 0;

    if (g_orig_VTable_JoinLobby) {
        targetName = "VTable";
        targetAddr = (void*)g_orig_VTable_JoinLobby;
        hCall = g_orig_VTable_JoinLobby(self, steamIDLobby);
    } else if (g_pfn_JoinLobby) {
        targetName = "Flat Export";
        targetAddr = (void*)g_pfn_JoinLobby;
        hCall = g_pfn_JoinLobby(self, steamIDLobby);
    } else {
        targetName = "None (No valid JoinLobby target resolved)";
        targetAddr = nullptr;
        hCall = 0;
    }

    ReFixLog("  Target Selected: %s (%p)", targetName, targetAddr);
    ReFixLog("  -> JoinLobby APICall handle: %llu", hCall);

    if (hCall == 0) {
        if (!targetAddr) {
            ReFixLog("  [ERROR] JoinLobby failed: No backend function pointer available (VTable=null, Export=null)");
        } else {
            ReFixLog("  [WARNING] JoinLobby backend target %s (%p) returned 0 (k_uAPICallInvalid) for LobbyID=%llu",
                     targetName, targetAddr, steamIDLobby);
        }
    }

    // Godot-specific: steam-multiplayer-peer.dll waits for LobbyEnter_t (iCallback=504)
    // before calling ISteamNetworkingSockets::ConnectP2P(). If the lobby ID is synthetic
    // (not a real Steam lobby) the real Steam backend won't fire LobbyEnter_t, causing
    // the "Failed to join. The connection timed out." error.
    // We synthesize LobbyEnter_t so Godot can proceed with the P2P connection.
    if (g_godotIsEngine) {
        // Detect synthetic lobby IDs: format is 0x0110000100000000 | lowBits
        bool isSyntheticOrUnknown = ((steamIDLobby >> 52) == 0x011) || (hCall == 0);
        if (isSyntheticOrUnknown) {
            ReFixLog("[Godot] JoinLobby: lobby appears synthetic or join handle=0, scheduling LobbyEnter_t synthesis");
            std::thread([steamIDLobby]() {
                Sleep(300); // brief delay to let real callbacks fire first if any
                SynthesizeLobbyEnterCallback(steamIDLobby);
            }).detach();
        } else {
            // Real lobby — also synthesize as fallback in case real callback is lost
            std::thread([steamIDLobby]() {
                Sleep(1500); // wait for real LobbyEnter_t; if not fired, inject ours
                // Only inject if still tracking this lobby (active lobby = this lobby)
                if (g_activeLobbyID == steamIDLobby) {
                    ReFixLog("[Godot] JoinLobby: fallback LobbyEnter_t synthesis after 1500ms for real lobby %llu", steamIDLobby);
                    SynthesizeLobbyEnterCallback(steamIDLobby);
                }
            }).detach();
        }
    }

    return hCall;
}

static bool Hooked_ISteamMatchmaking_SetLobbyData(void* self, uint64_t steamIDLobby, const char* pchKey, const char* pchValue) {
    ReFixLog("ISteamMatchmaking::SetLobbyData Hook: Lobby=%llu, Key='%s', Value='%s'",
             steamIDLobby, pchKey ? pchKey : "", pchValue ? pchValue : "");

    // Immediately track real lobby ID whenever the game sets lobby metadata
    if (steamIDLobby != 0 && g_activeLobbyID != steamIDLobby) {
        ReFix_NotifyLobbyID(steamIDLobby);
    }

    if (g_config.enableLobbyFilter && g_config.lobbyFilterKey[0] != '\0' && g_config.lobbyFilterValue[0] != '\0' && self) {
        if (g_orig_VTable_SetLobbyData) {
            g_orig_VTable_SetLobbyData(self, steamIDLobby, g_config.lobbyFilterKey, g_config.lobbyFilterValue);
        } else if (g_pfn_SetLobbyData) {
            g_pfn_SetLobbyData(self, steamIDLobby, g_config.lobbyFilterKey, g_config.lobbyFilterValue);
        }
        ReFixLog("  -> Injected metadata '%s'='%s' into Lobby %llu", g_config.lobbyFilterKey, g_config.lobbyFilterValue, steamIDLobby);
    }
    if (g_orig_VTable_SetLobbyData) {
        return g_orig_VTable_SetLobbyData(self, steamIDLobby, pchKey, pchValue);
    } else if (g_pfn_SetLobbyData) {
        return g_pfn_SetLobbyData(self, steamIDLobby, pchKey, pchValue);
    }
    return false;
}

static void* Intercept_RequestServerList4(
    fn_RequestServerList4_t pfnOriginal,
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse)
{
    LoadConfig();
    uint32_t targetApp = g_config.overrideServerListAppId ? g_config.serverListAppIdNum : g_config.maskAppIdNum;
    ReFixLog("RequestServerList: replacing AppID %u -> %u (serverListAppId=%u, mask=%u)",
             iApp, targetApp, g_config.serverListAppIdNum, g_config.maskAppIdNum);
    if (!pfnOriginal) return nullptr;
    return pfnOriginal(self, targetApp, ppchFilters, nFilters, pResponse);
}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestInternetServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {
    return Intercept_RequestServerList4(g_pfn_RequestInternetServerList, self, iApp, ppchFilters, nFilters, pResponse);
}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {
    return Intercept_RequestServerList4(g_pfn_RequestFavoritesServerList, self, iApp, ppchFilters, nFilters, pResponse);
}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {
    return Intercept_RequestServerList4(g_pfn_RequestFriendsServerList, self, iApp, ppchFilters, nFilters, pResponse);
}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {
    return Intercept_RequestServerList4(g_pfn_RequestHistoryServerList, self, iApp, ppchFilters, nFilters, pResponse);
}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse) {
    return Intercept_RequestServerList4(g_pfn_RequestSpectatorServerList, self, iApp, ppchFilters, nFilters, pResponse);
}

extern "C" __declspec(dllexport) void* SteamAPI_ISteamMatchmakingServers_RequestLANServerList(
    void* self, uint32_t iApp, void* pResponse) {
    LoadConfig();
    if (!g_pfn_RequestLANServerList) return nullptr;
    uint32_t targetApp = g_config.overrideServerListAppId ? g_config.serverListAppIdNum : g_config.maskAppIdNum;
    return g_pfn_RequestLANServerList(self, targetApp, pResponse);
}

extern "C" __declspec(dllexport) uint32_t SteamAPI_ISteamUtils_GetAppID(void* self) {
    LoadConfig();
    uint32_t targetApp = (g_config.realAppIdNum != 0) ? g_config.realAppIdNum : g_config.maskAppIdNum;
    ReFixLog("SteamAPI_ISteamUtils_GetAppID returning AppId=%u", targetApp);
    return targetApp;
}

extern "C" __declspec(dllexport) bool SteamAPI_ISteamUser_BLoggedOn(void* self) {
    return true;
}

// =============================================================================
// REFIX IN-GAME DEBUG CONSOLE TOGGLE (VK_INSERT / VK_F1)
// =============================================================================
static bool g_ConsoleAllocated = false;
static bool g_ConsoleVisible = false;
static DWORD g_LastToggleTime = 0;

static void ToggleConsoleWindow() {
    DWORD now = GetTickCount();
    if (now - g_LastToggleTime < 300) return;
    g_LastToggleTime = now;

    if (!g_ConsoleAllocated) {
        AllocConsole();
        SetConsoleTitleA("ReFix Internal Debug Console");
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        freopen_s(&fDummy, "CONIN$", "r", stdin);
        g_ConsoleAllocated = true;
        g_ConsoleVisible = true;
        printf("=========================================\n");
        printf(" ReFix Debug Console (Valve / Goldberg LAN)\n");
        printf("=========================================\n");
        return;
    }

    HWND hCons = GetConsoleWindow();
    if (!hCons) return;
    g_ConsoleVisible = !g_ConsoleVisible;
    ShowWindow(hCons, g_ConsoleVisible ? SW_SHOW : SW_HIDE);
}

static std::atomic<bool> g_hotkeyRunning{ true };
static DWORD WINAPI ConsoleHotkeyThread(LPVOID) {
    while (g_hotkeyRunning) {
        Sleep(50);
        if (!g_config.enableConsole) continue;
        if ((GetAsyncKeyState(VK_INSERT) & 0x8000) || (GetAsyncKeyState(VK_F1) & 0x8000)) {
            ToggleConsoleWindow();
        }
    }
    return 0;
}

static void StartConsoleHotkeyMonitor() {
    CreateThread(NULL, 0, ConsoleHotkeyThread, NULL, 0, NULL);
}

static void* Hooked_ISteamMatchmakingServers_RequestInternetServerList(
    void* self, uint32_t iApp, void** ppchFilters, uint32_t nFilters, void* pResponse)
{
    uint32_t targetApp = g_config.overrideServerListAppId ? g_config.serverListAppIdNum : g_config.maskAppIdNum;
    ReFixLog("ISteamMatchmakingServers::RequestInternetServerList Hook: AppID %u -> %u", iApp, targetApp);
    if (g_orig_VTable_RequestInternetServerList) {
        return g_orig_VTable_RequestInternetServerList(self, targetApp, ppchFilters, nFilters, pResponse);
    } else if (g_pfn_RequestInternetServerList) {
        return g_pfn_RequestInternetServerList(self, targetApp, ppchFilters, nFilters, pResponse);
    }
    return nullptr;
}

static uint32_t Hooked_ISteamUtils_GetAppID(void* self) {
    LoadConfig();
    uint32_t targetApp = (g_config.realAppIdNum != 0) ? g_config.realAppIdNum : g_config.maskAppIdNum;
    ReFixLog("ISteamUtils::GetAppID Hook returning AppId=%u", targetApp);
    return targetApp;
}

static bool Hooked_ISteamApps_BIsSubscribedApp(void* self, uint32_t appID) {
    if (g_config.bypassLicenseCheck) {
        ReFixLog("ISteamApps::BIsSubscribedApp Hook: appID=%u -> returning true (bypassed)", appID);
        return true;
    }
    if (g_orig_VTable_BIsSubscribedApp) {
        return g_orig_VTable_BIsSubscribedApp(self, appID);
    }
    return true;
}

static bool Hooked_ISteamApps_BIsDlcInstalled(void* self, uint32_t appID) {
    if (g_config.bypassLicenseCheck) {
        ReFixLog("ISteamApps::BIsDlcInstalled Hook: appID=%u -> returning true (bypassed)", appID);
        return true;
    }
    if (g_orig_VTable_BIsDlcInstalled) {
        return g_orig_VTable_BIsDlcInstalled(self, appID);
    }
    return true;
}

static uint32_t g_lastAuthTicketHandle = 0;
static std::vector<uint8_t> g_lastAuthTicketData;

static uint64_t GetMachineUniqueHash() {
    static uint64_t s_hash = 0;
    if (s_hash != 0) return s_hash;

    char compName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    DWORD compLen = sizeof(compName);
    GetComputerNameA(compName, &compLen);

    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);

    // FNV-1a 64-bit hash
    uint64_t h = 14695981039346656037ULL;
    for (const char* p = compName; *p; ++p) {
        h ^= (uint8_t)*p;
        h *= 1099511628211ULL;
    }
    h ^= volSerial;
    h *= 1099511628211ULL;

    s_hash = h ? h : 0x123456789ABCDEF0ULL;
    return s_hash;
}

static uint64_t GetMachineUniqueSteamID() {
    if (g_capturedSteamID != 0) return g_capturedSteamID;
    char envBuf[64] = { 0 };
    if (GetEnvironmentVariableA("REFIX_STEAM_ID", envBuf, sizeof(envBuf)) > 0) {
        uint64_t sid = _strtoui64(envBuf, nullptr, 10);
        if (sid != 0) return sid;
    }
    uint64_t mHash = GetMachineUniqueHash();
    uint32_t accountId = (uint32_t)(mHash & 0x0FFFFFFF);
    if (accountId == 0) accountId = 100001;
    return 76561197960265728ULL + accountId; // Standard Steam ID64 calculation
}

static uint64_t GetMachineUniqueToken() {
    uint64_t mHash = GetMachineUniqueHash();
    uint32_t pid = GetCurrentProcessId();
    uint64_t token = (mHash ^ ((uint64_t)pid << 32) ^ 0xA5A5A5A5A5A5A5A5ULL);
    return token ? token : 0x0102030405060708ULL;
}

static std::vector<uint8_t> GenerateDummyAuthTicket() {
    uint64_t steam_id = GetMachineUniqueSteamID();
    uint64_t token = GetMachineUniqueToken();
    uint32_t date = (uint32_t)time(nullptr);
    uint32_t gc_len = 24;

    std::vector<uint8_t> ticket(72, 0);
    memcpy(ticket.data(), &gc_len, 4);
    memcpy(ticket.data() + 4, &token, 8);
    memcpy(ticket.data() + 12, &steam_id, 8);
    memcpy(ticket.data() + 20, &date, 4);

    uint32_t appId = g_maskAppIdNum ? g_maskAppIdNum : 480;
    memcpy(ticket.data() + 24, &appId, 4);
    uint32_t ip = 0x0100007F; // 127.0.0.1
    memcpy(ticket.data() + 28, &ip, 4);
    memcpy(ticket.data() + 32, &ip, 4);

    for (size_t i = 36; i < ticket.size(); i++) {
        ticket[i] = (uint8_t)((token >> ((i % 8) * 8)) ^ (steam_id >> ((i % 8) * 8)) ^ (uint8_t)i);
    }
    ReFixLog("GenerateDummyAuthTicket: steam_id=%llu, token=0x%016llX, size=%zu", steam_id, token, ticket.size());
    return ticket;
}

typedef bool (*fn_BLoggedOn_t)(void* self);
static fn_BLoggedOn_t g_orig_VTable_BLoggedOn = nullptr;

static bool Hooked_ISteamUser_BLoggedOn(void* self) {
    ReFixLog("ISteamUser::BLoggedOn Hook -> returning true");
    return true;
}

typedef uint32_t (*fn_VTable_GetAuthSessionTicket_t)(void* self, void* pTicket, int cbMaxTicket, uint32_t* pcbTicket);
static fn_VTable_GetAuthSessionTicket_t g_orig_VTable_GetAuthSessionTicket = nullptr;

static void DispatchAuthCallbacksImmediately(int targetCallback);

static uint32_t Hooked_VTable_GetAuthSessionTicket(void* self, void* pTicket, int cbMaxTicket, uint32_t* pcbTicket) {
    uint32_t handle = 1;
    if (g_orig_VTable_GetAuthSessionTicket) {
        handle = g_orig_VTable_GetAuthSessionTicket(self, pTicket, cbMaxTicket, pcbTicket);
    }
    ReFixLog("ISteamUser::GetAuthSessionTicket (VTable index 13) -> handle=%u, cbTicket=%u", handle, (pcbTicket ? *pcbTicket : 0));
    if (handle != 0) {
        g_lastAuthTicketHandle = handle;
    }
    if (pTicket && pcbTicket && *pcbTicket > 0) {
        g_lastAuthTicketData.assign((uint8_t*)pTicket, (uint8_t*)pTicket + *pcbTicket);
    }
    DispatchAuthCallbacksImmediately(163);
    DispatchAuthCallbacksImmediately(168);
    return handle;
}

typedef uint32_t (*fn_VTable_GetAuthTicketForWebApi_t)(void* self, const char* pchIdentity);
static fn_VTable_GetAuthTicketForWebApi_t g_orig_VTable_GetAuthTicketForWebApi = nullptr;

static uint32_t Hooked_VTable_GetAuthTicketForWebApi(void* self, const char* pchIdentity) {
    uint32_t handle = 1;
    if (g_orig_VTable_GetAuthTicketForWebApi) {
        handle = g_orig_VTable_GetAuthTicketForWebApi(self, pchIdentity);
    }
    ReFixLog("ISteamUser::GetAuthTicketForWebApi (VTable index 14) -> handle=%u, pchIdentity='%s'", handle, pchIdentity ? pchIdentity : "");
    if (handle != 0) {
        g_lastAuthTicketHandle = handle;
    }
    if (g_lastAuthTicketData.empty()) {
        g_lastAuthTicketData = GenerateDummyAuthTicket();
    }
    DispatchAuthCallbacksImmediately(168);
    DispatchAuthCallbacksImmediately(163);
    return handle;
}

static bool g_vtableHooksInstalled = false;

static void InstallVTableHooks() {
    if (g_vtableHooksInstalled) return;

    MH_Initialize();

    typedef void* (*fn_GetInterface_t)();

    // 1. ISteamMatchmaking
    fn_GetInterface_t pfnMM = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_SteamMatchmaking_v009");
    if (!pfnMM) pfnMM = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamMatchmaking");
    if (pfnMM) {
        void* pMM = pfnMM();
        if (pMM) {
            HookVTableMethod(pMM, 14, (void*)Hooked_ISteamMatchmaking_JoinLobby, (void**)&g_orig_VTable_JoinLobby);
            HookVTableMethod(pMM, 20, (void*)Hooked_ISteamMatchmaking_SetLobbyData, (void**)&g_orig_VTable_SetLobbyData);
        }
    }

    // 2. ISteamUtils
    fn_GetInterface_t pfnUtils = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_SteamUtils_v010");
    if (!pfnUtils) pfnUtils = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamUtils");
    if (pfnUtils) {
        void* pUtils = pfnUtils();
        if (pUtils) {
            HookVTableMethod(pUtils, 9, (void*)Hooked_ISteamUtils_GetAppID, (void**)&g_orig_VTable_GetAppID);
        }
    }

    // 3. ISteamUser
    fn_GetInterface_t pfnUser = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_SteamUser_v021");
    if (!pfnUser) pfnUser = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_SteamUser_v020");
    if (!pfnUser) pfnUser = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamUser");
    if (pfnUser) {
        void* pUser = pfnUser();
        if (pUser) {
            HookVTableMethod(pUser, 14, (void*)Hooked_VTable_GetAuthTicketForWebApi, (void**)&g_orig_VTable_GetAuthTicketForWebApi);
        }
    }

    // 4. ISteamApps
    fn_GetInterface_t pfnApps = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_SteamApps_v008");
    if (!pfnApps) pfnApps = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamApps");
    if (pfnApps) {
        void* pApps = pfnApps();
        if (pApps) {
            HookVTableMethod(pApps, 6, (void*)Hooked_ISteamApps_BIsSubscribedApp, (void**)&g_orig_VTable_BIsSubscribedApp);
            HookVTableMethod(pApps, 7, (void*)Hooked_ISteamApps_BIsDlcInstalled, (void**)&g_orig_VTable_BIsDlcInstalled);
        }
    }

    // 5. ISteamFriends
    fn_GetInterface_t pfnFriends = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_SteamFriends_v017");
    if (!pfnFriends) pfnFriends = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamFriends");
    if (pfnFriends) {
        void* pFriends = pfnFriends();
        if (pFriends) {
            HookVTableMethod(pFriends, 8, (void*)Hooked_ISteamFriends_GetFriendGamePlayed, (void**)&g_orig_VTable_GetFriendGamePlayed);
            HookVTableMethod(pFriends, 15, (void*)Hooked_ISteamFriends_ActivateGameOverlayInviteDialog, (void**)&g_orig_VTable_ActivateGameOverlayInviteDialog);
            HookVTableMethod(pFriends, 43, (void*)Hooked_ISteamFriends_SetRichPresence, (void**)&g_orig_VTable_SetRichPresence);
        }
    }

    // 6. ISteamUGC
    fn_GetInterface_t pfnUGC = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_SteamUGC_v016");
    if (!pfnUGC) pfnUGC = (fn_GetInterface_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamUGC");
    if (pfnUGC) {
        void* pUGC = pfnUGC();
        if (pUGC) {
            void** vtable = *(void***)pUGC;
            if (vtable && vtable[30]) {
                g_orig_VTable_GetItemInstallInfo = (fn_VTable_GetItemInstallInfo_t)vtable[30];
            }
            HookVTableMethod(pUGC, 27, (void*)Hooked_ISteamUGC_GetNumSubscribedItems, (void**)&g_orig_VTable_GetNumSubscribedItems);
            HookVTableMethod(pUGC, 28, (void*)Hooked_ISteamUGC_GetSubscribedItems, (void**)&g_orig_VTable_GetSubscribedItems);
        }
    }

    // 7. ISteamNetworkingUtils -- GetRelayNetworkStatus hook
    // Fixes "Checking Steam Relay..." hang for all ISteamNetworkingSockets games.
    // vtable[0]=AllocateMessage, vtable[1]=GetRelayNetworkStatus (hooked).
    {
        fn_GetInterface_t pfnNetUtils = nullptr;
        pfnNetUtils = (fn_GetInterface_t)GetProcAddress(
            (HMODULE)g_hOriginalDll, "SteamAPI_SteamNetworkingUtils_v003");
        if (!pfnNetUtils)
            pfnNetUtils = (fn_GetInterface_t)GetProcAddress(
                (HMODULE)g_hOriginalDll, "SteamNetworkingUtils_v003");
        if (!pfnNetUtils)
            pfnNetUtils = (fn_GetInterface_t)GetProcAddress(
                (HMODULE)g_hOriginalDll, "SteamNetworkingUtils");
        if (pfnNetUtils) {
            void* pNetUtils = pfnNetUtils();
            if (pNetUtils) {
                HookVTableMethod(pNetUtils, 1,
                    (void*)Hooked_ISteamNetworkingUtils_GetRelayNetworkStatus,
                    (void**)&g_orig_VTable_GetRelayNetworkStatus);
                ReFixLog("InstallVTableHooks: ISteamNetworkingUtils relay hook OK (vtable[1])");
            } else {
                ReFixLog("InstallVTableHooks: ISteamNetworkingUtils->pfn returned null");
            }
        } else {
            ReFixLog("InstallVTableHooks: SteamNetworkingUtils accessor not in valve DLL");
        }
    }
    g_vtableHooksInstalled = true;
    ReFixLog("InstallVTableHooks: All VTable hooks initialized successfully.");
}

typedef void (*fn_SteamAPI_RunCallbacks_t)();
static fn_SteamAPI_RunCallbacks_t g_pfn_RunCallbacks = nullptr;

typedef void (*fn_SteamAPI_ManualDispatch_RunFrame_t)(uint32_t hSteamPipe);
static fn_SteamAPI_ManualDispatch_RunFrame_t g_pfn_ManualDispatch_RunFrame = nullptr;

typedef bool (*fn_SteamAPI_ManualDispatch_GetNextCallback_t)(uint32_t hSteamPipe, void* pCallbackMsg);
static fn_SteamAPI_ManualDispatch_GetNextCallback_t g_pfn_ManualDispatch_GetNextCallback = nullptr;

typedef uint32_t (*fn_SteamAPI_ISteamUser_GetAuthSessionTicket_t)(void* self, void* pTicket, int cbMaxTicket, uint32_t* pcbTicket);
static fn_SteamAPI_ISteamUser_GetAuthSessionTicket_t g_pfn_GetAuthSessionTicket = nullptr;

typedef uint32_t (*fn_SteamAPI_ISteamUser_GetAuthTicketForWebApi_t)(void* self, const char* pchIdentity);
static fn_SteamAPI_ISteamUser_GetAuthTicketForWebApi_t g_pfn_GetAuthTicketForWebApi = nullptr;

static int s_runCallbacksLogged = 0;

struct PendingAuthCallback {
    void* pCallback;
    int iCallback;
    int framesRemaining;
    int dispatchCount;
};
static std::vector<PendingAuthCallback> g_pendingAuthCallbacks;
static std::mutex g_pendingAuthMutex;

static bool SafeCallRun(void* pCallback, void* pData, uint64_t hAPICall = 1) {
    if (!pCallback) return false;
    __try {
        void** vtable = *(void***)pCallback;
        if (!vtable) return false;

        typedef void (*fn_Run1_t)(void* self, void* pvParam, bool bIOFailure, uint64_t hAPICall);
        fn_Run1_t pRun1 = (fn_Run1_t)vtable[1];
        if (pRun1) {
            pRun1(pCallback, pData, false, hAPICall);
            return true;
        }

        typedef void (*fn_Run0_t)(void* self, void* pvParam);
        fn_Run0_t pRun0 = (fn_Run0_t)vtable[0];
        if (pRun0) {
            pRun0(pCallback, pData);
            return true;
        }
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        DWORD code = GetExceptionCode();
        ReFixLog("SafeCallRun: Handled exception 0x%08X on %p", code, pCallback);
        return false;
    }
}

// =============================================================================
// ISteamNetworkingUtils Flat Export Interception & Relay Callback Dispatcher
// =============================================================================
struct ReFix_CallbackMsg_t {
    int32_t  m_hSteamUser;
    int32_t  m_iCallback;
    uint8_t* m_pubParam;
    int32_t  m_cubParam;
};

static std::atomic<bool> g_syntheticRelayPending{ false };
static ReFix_SteamRelayNetworkStatus_t g_syntheticRelayData = {};

static void TriggerSyntheticRelayCallback() {
    g_syntheticRelayPending.store(true, std::memory_order_relaxed);
}

struct RelayCallbackEntry {
    void* pCallback;
    int iCallback;
};
static std::vector<RelayCallbackEntry> g_registeredRelayCallbacks;
static std::mutex g_relayCallbackMutex;

static void TrackRelayCallback(void* pCallback, int iCallback) {
    std::lock_guard<std::mutex> lg(g_relayCallbackMutex);
    for (auto& e : g_registeredRelayCallbacks) {
        if (e.pCallback == pCallback) { e.iCallback = iCallback; return; }
    }
    g_registeredRelayCallbacks.push_back({ pCallback, iCallback });
}

static void UntrackRelayCallback(void* pCallback) {
    std::lock_guard<std::mutex> lg(g_relayCallbackMutex);
    for (auto it = g_registeredRelayCallbacks.begin(); it != g_registeredRelayCallbacks.end(); ) {
        if (it->pCallback == pCallback) {
            it = g_registeredRelayCallbacks.erase(it);
        } else {
            ++it;
        }
    }
}

static void DispatchRelayCallbacks() {
    std::vector<RelayCallbackEntry> snapshot;
    {
        std::lock_guard<std::mutex> lg(g_relayCallbackMutex);
        snapshot = g_registeredRelayCallbacks;
    }
    if (snapshot.empty()) return;

    ReFix_SteamRelayNetworkStatus_t data = {};
    data.m_eAvail = k_eRelayAvail_Current; // 100
    data.m_bPingMeasurementInProgress = 0;
    data.m_eAvailNetworkConfig = k_eRelayAvail_Current;
    data.m_eAvailAnyRelay = k_eRelayAvail_Current;
    strncpy_s(data.m_debugMsg, sizeof(data.m_debugMsg), "OK (ReFix)", _TRUNCATE);

    for (auto& entry : snapshot) {
        if (entry.pCallback) {
            SafeCallRun(entry.pCallback, &data);
        }
    }
}

typedef int (*fn_Flat_GetRelayNetworkStatus_t)(void* self, void* pDetails);
static fn_Flat_GetRelayNetworkStatus_t g_pfn_Flat_GetRelayNetworkStatus = nullptr;

static int Intercepted_SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus(void* self, void* pDetails) {
    if (g_pfn_Flat_GetRelayNetworkStatus) {
        g_pfn_Flat_GetRelayNetworkStatus(self, pDetails);
    }
    DWORD elapsed = (g_steamInitTick != 0) ? (GetTickCount() - g_steamInitTick) : 1000;
    int avail = (elapsed < 500) ? 3 : k_eRelayAvail_Current;

    if (pDetails) {
        auto* d = static_cast<ReFix_SteamRelayNetworkStatus_t*>(pDetails);
        d->m_eAvail = avail;
        d->m_bPingMeasurementInProgress = (avail == 3) ? 1 : 0;
        d->m_eAvailNetworkConfig = avail;
        d->m_eAvailAnyRelay = avail;
        if (d->m_debugMsg[0] == '\0')
            strncpy_s(d->m_debugMsg, sizeof(d->m_debugMsg), (avail == 100) ? "OK (ReFix)" : "Connecting (ReFix)", _TRUNCATE);
    }
    return avail;
}

typedef void (*fn_Flat_InitRelayNetworkAccess_t)(void* self);
static fn_Flat_InitRelayNetworkAccess_t g_pfn_Flat_InitRelayNetworkAccess = nullptr;

static void Intercepted_SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(void* self) {
    if (g_pfn_Flat_InitRelayNetworkAccess) {
        g_pfn_Flat_InitRelayNetworkAccess(self);
    }
    g_syntheticRelayPending.store(true, std::memory_order_relaxed);
    DispatchRelayCallbacks();
}



static void DispatchAuthCallbacksImmediately(int targetCallback) {
    if (g_lastAuthTicketData.empty()) {
        g_lastAuthTicketData = GenerateDummyAuthTicket();
    }
    uint32_t handle = g_lastAuthTicketHandle ? g_lastAuthTicketHandle : 1;
    
    std::vector<void*> targets;
    {
        std::lock_guard<std::mutex> lg(g_pendingAuthMutex);
        for (const auto& item : g_pendingAuthCallbacks) {
            if (item.iCallback == targetCallback && item.pCallback) {
                targets.push_back(item.pCallback);
            }
        }
    }
    for (void* pCb : targets) {
        if (targetCallback == 168) {
            struct {
                uint32_t m_hAuthTicket;
                int32_t  m_eResult;
                int32_t  m_cubTicket;
                uint8_t  m_rgubTicket[1024];
            } data = {};
            data.m_hAuthTicket = handle;
            data.m_eResult = 1; // k_EResultOK
            data.m_cubTicket = (int32_t)g_lastAuthTicketData.size();
            if (data.m_cubTicket > 0 && data.m_cubTicket <= sizeof(data.m_rgubTicket)) {
                memcpy(data.m_rgubTicket, g_lastAuthTicketData.data(), data.m_cubTicket);
            }
            if (SafeCallRun(pCb, &data)) {
                ReFixLog("DispatchAuthCallbacksImmediately: Dispatched 168 (handle=%u) to %p", handle, pCb);
            }
        } else if (targetCallback == 163) {
            struct {
                uint32_t m_hAuthTicket;
                int32_t  m_eResult;
            } data = {};
            data.m_hAuthTicket = handle;
            data.m_eResult = 1; // k_EResultOK
            if (SafeCallRun(pCb, &data)) {
                ReFixLog("DispatchAuthCallbacksImmediately: Dispatched 163 (handle=%u) to %p", handle, pCb);
            }
        }
    }
}

static void DispatchPendingAuthCallbacks() {
    std::vector<PendingAuthCallback> toFire;
    {
        std::lock_guard<std::mutex> lg(g_pendingAuthMutex);
        for (auto it = g_pendingAuthCallbacks.begin(); it != g_pendingAuthCallbacks.end(); ) {
            if (--it->framesRemaining <= 0) {
                toFire.push_back(*it);
                if (it->iCallback == 101 && ++it->dispatchCount < 3) {
                    it->framesRemaining = 1;
                    ++it;
                } else {
                    it = g_pendingAuthCallbacks.erase(it);
                }
            } else {
                ++it;
            }
        }
    }

    for (const auto& item : toFire) {
        if (!item.pCallback) continue;

        if (item.iCallback == 101) {
            struct {
                enum { k_iCallback = 101 };
            } data = {};
            if (SafeCallRun(item.pCallback, &data)) {
                ReFixLog("DispatchPendingAuthCallbacks: Successfully dispatched SteamServersConnected_t (101) to %p (count=%d)", item.pCallback, item.dispatchCount);
            }
        } else if (item.iCallback == 154) {
            struct {
                int32_t m_eResult;
            } data = {};
            data.m_eResult = 1; // k_EResultOK
            if (SafeCallRun(item.pCallback, &data)) {
                ReFixLog("DispatchPendingAuthCallbacks: Successfully dispatched EncryptedAppTicketResponse_t (154) to %p", item.pCallback);
            }
        } else if (item.iCallback == 168) {
            uint32_t handle = g_lastAuthTicketHandle ? g_lastAuthTicketHandle : 1;
            if (g_lastAuthTicketData.empty()) {
                g_lastAuthTicketData = GenerateDummyAuthTicket();
            }
            struct {
                uint32_t m_hAuthTicket;
                int32_t  m_eResult;
                int32_t  m_cubTicket;
                uint8_t  m_rgubTicket[1024];
            } data = {};
            data.m_hAuthTicket = handle;
            data.m_eResult = 1; // k_EResultOK
            data.m_cubTicket = (int32_t)g_lastAuthTicketData.size();
            if (data.m_cubTicket > 0 && data.m_cubTicket <= sizeof(data.m_rgubTicket)) {
                memcpy(data.m_rgubTicket, g_lastAuthTicketData.data(), data.m_cubTicket);
            }

            if (SafeCallRun(item.pCallback, &data, handle)) {
                ReFixLog("DispatchPendingAuthCallbacks: Successfully dispatched GetTicketForWebApiResponse_t (168, handle=%u) to %p", handle, item.pCallback);
            }
        } else if (item.iCallback == 163) {
            uint32_t handle = g_lastAuthTicketHandle ? g_lastAuthTicketHandle : 1;
            struct {
                uint32_t m_hAuthTicket;
                int32_t  m_eResult;
            } data = {};
            data.m_hAuthTicket = handle;
            data.m_eResult = 1; // k_EResultOK

            if (SafeCallRun(item.pCallback, &data, handle)) {
                ReFixLog("DispatchPendingAuthCallbacks: Successfully dispatched GetAuthSessionTicketResponse_t (163, handle=%u) to %p", handle, item.pCallback);
            }
        }
    }
}

static std::thread g_authDispatchThread;
static std::atomic<bool> g_authDispatchRunning{ true };
static std::once_flag g_authDispatchOnce;

static void AuthDispatchWorker() {
    while (g_authDispatchRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        try {
            DispatchPendingAuthCallbacks();
        } catch (...) {
            ReFixLog("AuthDispatchWorker: caught unhandled exception");
        }
    }
}

static void StartAuthDispatchWorker() {
    std::call_once(g_authDispatchOnce, []() {
        g_authDispatchThread = std::thread(AuthDispatchWorker);
        g_authDispatchThread.detach();
    });
}

typedef void (*fn_SteamAPI_UnregisterCallback_t)(void* pCallback);
static fn_SteamAPI_UnregisterCallback_t g_pfn_UnregisterCallback = nullptr;

static void Intercepted_SteamAPI_UnregisterCallback(void* pCallback) {
    UntrackRelayCallback(pCallback);
    if (g_godotIsEngine) {
        UntrackCallback(pCallback);
    }
    {
        std::lock_guard<std::mutex> lg(g_pendingAuthMutex);
        for (auto it = g_pendingAuthCallbacks.begin(); it != g_pendingAuthCallbacks.end(); ) {
            if (it->pCallback == pCallback) {
                it = g_pendingAuthCallbacks.erase(it);
            } else {
                ++it;
            }
        }
    }
    if (g_pfn_UnregisterCallback) {
        g_pfn_UnregisterCallback(pCallback);
    }
}

// C++ Interception handlers
static void Intercepted_SteamAPI_RegisterCallback(void* pCallback, int iCallback) {
    ReFixLog("SteamAPI_RegisterCallback: pCallback=%p, iCallback=%d", pCallback, iCallback);
    
    if (iCallback == 1281) {
        ReFixLog("  -> Intercepted SteamRelayNetworkStatus_t (1281) callback registration for %p", pCallback);
        TrackRelayCallback(pCallback, iCallback);
        ReFix_SteamRelayNetworkStatus_t data = {};
        data.m_eAvail = k_eRelayAvail_Current;
        data.m_bPingMeasurementInProgress = 0;
        data.m_eAvailNetworkConfig = k_eRelayAvail_Current;
        data.m_eAvailAnyRelay = k_eRelayAvail_Current;
        strncpy_s(data.m_debugMsg, sizeof(data.m_debugMsg), "OK (ReFix)", _TRUNCATE);
        SafeCallRun(pCallback, &data);
    }

    if (iCallback == 333) ReFixLog("  -> Intercepted GameLobbyJoinRequested_t (333)");
    else if (iCallback == 337) ReFixLog("  -> Intercepted GameRichPresenceJoinRequested_t (337)");

    // Track callbacks for Godot LobbyEnter_t synthesis
    if (g_godotIsEngine && (iCallback == LobbyEnter_t::k_iCallback || iCallback == 333 || iCallback == 337)) {
        TrackCallback(pCallback, iCallback);
    }

    if (g_pfn_RegisterCallback) g_pfn_RegisterCallback(pCallback, iCallback);

    if (g_isGoldbergMode) {
        if (iCallback == 101 || iCallback == 154 || iCallback == 163 || iCallback == 168) {
            std::lock_guard<std::mutex> lg(g_pendingAuthMutex);
            g_pendingAuthCallbacks.push_back({ pCallback, iCallback, 0, 0 });
            ReFixLog("  -> Queued auth callback %d for %p", iCallback, pCallback);
            StartAuthDispatchWorker();
        }
    }
}

static bool Intercepted_SetRichPresence(void* self, const char* pchKey, const char* pchValue) {
    ReFixLog("SteamAPI_ISteamFriends_SetRichPresence: Key='%s', Value='%s'",
             pchKey ? pchKey : "", pchValue ? pchValue : "");

    if (g_pfn_SetRichPresence) {
        return g_pfn_SetRichPresence(self, pchKey, pchValue);
    }
    return true;
}

static uint64_t Intercepted_CreateLobby(void* self, int eLobbyType, int cMaxMembers) {
    return Hooked_ISteamMatchmaking_CreateLobby(self, eLobbyType, cMaxMembers);
}

static uint64_t Intercepted_JoinLobby(void* self, uint64_t steamIDLobby) {
    return Hooked_ISteamMatchmaking_JoinLobby(self, steamIDLobby);
}

// Called from eos_proxy.cpp (or anywhere) when we know the real Steam lobby ID
extern "C" void ReFix_NotifyLobbyID(uint64_t lobbyID) {
    if (lobbyID && lobbyID != g_activeLobbyID) {
        g_activeLobbyID = lobbyID;
        ReFixLog("ReFix_NotifyLobbyID: tracking lobby=%llu", lobbyID);

        // Inject game_filter tag into the Steam Lobby so RequestLobbyList filters locate it
        if (g_config.enableLobbyFilter && g_config.lobbyFilterKey[0] != '\0' && g_config.lobbyFilterValue[0] != '\0') {
            void* matchmaking = nullptr;
            typedef void* (*fn_SteamMatchmaking_t)();
            auto pfnMM = (fn_SteamMatchmaking_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_SteamMatchmaking_v009");
            if (!pfnMM) pfnMM = (fn_SteamMatchmaking_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamMatchmaking");
            if (pfnMM) matchmaking = pfnMM();
            if (matchmaking) {
                Hooked_ISteamMatchmaking_SetLobbyData(matchmaking, lobbyID, g_config.lobbyFilterKey, g_config.lobbyFilterValue);
                ReFixLog("  -> Injected '%s'='%s' into Steam Lobby %llu", g_config.lobbyFilterKey, g_config.lobbyFilterValue, lobbyID);
            }
        }

        UpdateP2PPeers(lobbyID);

        // Notify EOS proxy if loaded
        HMODULE hEOS = GetModuleHandleA("EOSSDK-Win64-Shipping.dll");
        if (hEOS) {
            auto pfnSet = (void(*)(uint64_t))GetProcAddress(hEOS, "ReFix_EOS_SetLobbyID");
            if (pfnSet) pfnSet(lobbyID);
        }

        // Register +connect_lobby in Steam Rich Presence for 1-click Steam Join/Invitations
        if (g_pfn_SetRichPresence && g_pfn_SteamFriends) {
            void* friends = g_pfn_SteamFriends();
            if (friends) {
                char connectString[128];
                sprintf_s(connectString, sizeof(connectString), "+connect_lobby %llu", lobbyID);
                g_pfn_SetRichPresence(friends, "connect", connectString);
                ReFixLog("  -> Updated Steam Rich Presence connect string: '%s'", connectString);
            }
        }
    }
}

// Called from eos_proxy.cpp when lobby member list changes (LobbyChatUpdate_t)
extern "C" void ReFix_NotifyLobbyMemberChange(uint64_t lobbyID) {
    if (lobbyID) {
        if (lobbyID != g_activeLobbyID) g_activeLobbyID = lobbyID;
        UpdateP2PPeers(lobbyID);
    }
}

static bool Intercepted_SetLobbyData(void* self, uint64_t steamIDLobby, const char* pchKey, const char* pchValue) {
    return Hooked_ISteamMatchmaking_SetLobbyData(self, steamIDLobby, pchKey, pchValue);
}

static uint64_t Intercepted_RequestLobbyList(void* self) {
    return Hooked_ISteamMatchmaking_RequestLobbyList(self);
}


extern "C" __declspec(dllexport) bool SteamAPI_Init();

static bool EnsureOriginal() {
    if (g_hOriginalDll) return true;

    std::string proxyDir = GetProxyDllDir();
    std::string exeDir = GetExeDir();

    const char* candNames[] = {
        "steam_api64_valve.dll",
        "steam_api64_goldberg.dll",
        "steam_api64_o.dll",
        "steam_api64_original.dll",
        "steam_api64.dll.valve"
    };

    std::vector<std::string> fullPaths;
    for (auto name : candNames) fullPaths.push_back(proxyDir + name);
    for (auto name : candNames) fullPaths.push_back(exeDir + name);
    for (auto name : candNames) {
        fullPaths.push_back(std::string(name));
        fullPaths.push_back(proxyDir + "bin\\" + name);
        fullPaths.push_back(exeDir + "bin\\" + name);
        fullPaths.push_back(exeDir + "..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv157\\Win64\\" + name);
        fullPaths.push_back(exeDir + "..\\..\\..\\Engine\\Binaries\\ThirdParty\\Steamworks\\Steamv153\\Win64\\" + name);
    }

    // Auto-discover Unity *_Data/Plugins and Plugins/ folders
    std::vector<std::string> searchDirs;
    searchDirs.push_back(proxyDir);
    if (proxyDir != exeDir) searchDirs.push_back(exeDir);

    for (const auto& sDir : searchDirs) {
        WIN32_FIND_DATAA ffd;
        HANDLE hFind = FindFirstFileA((sDir + "*_Data").c_str(), &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    std::string dataPlugins64 = sDir + ffd.cFileName + "\\Plugins\\x86_64\\";
                    std::string dataPlugins = sDir + ffd.cFileName + "\\Plugins\\";
                    for (auto name : candNames) {
                        fullPaths.push_back(dataPlugins64 + name);
                        fullPaths.push_back(dataPlugins + name);
                    }
                    fullPaths.push_back(dataPlugins64 + "steam_api64.dll");
                    fullPaths.push_back(dataPlugins + "steam_api64.dll");
                }
            } while (FindNextFileA(hFind, &ffd));
            FindClose(hFind);
        }

        std::string p64 = sDir + "Plugins\\x86_64\\";
        std::string p = sDir + "Plugins\\";
        for (auto name : candNames) {
            fullPaths.push_back(p64 + name);
            fullPaths.push_back(p + name);
        }
        fullPaths.push_back(p64 + "steam_api64.dll");
        fullPaths.push_back(p + "steam_api64.dll");
    }

    // Probe exports to identify a genuine Valve DLL. Newer SDK versions
    // (v08.63+) removed the classic SteamAPI_Init export and only ship
    // SteamAPI_InitFlat / SteamAPI_InitSafe, so we try a chain of known
    // init-family exports before giving up.
    const char* probeExports[] = {
        "SteamAPI_Init",
        "SteamAPI_InitFlat",
        "SteamAPI_InitSafe",
        "SteamAPI_Shutdown"   // last-resort: any real Valve DLL has this
    };

    for (const auto& path : fullPaths) {
        HMODULE hMod = LoadLibraryA(path.c_str());
        if (hMod && hMod != g_hSelfModule) {
            // Ensure this is not another instance of ReFix proxy
            if (GetProcAddress(hMod, "g_steamProcs") != nullptr) {
                FreeLibrary(hMod);
                continue;
            }

            FARPROC pProbe = nullptr;
            for (auto probeName : probeExports) {
                pProbe = GetProcAddress(hMod, probeName);
                if (pProbe) break;
            }
            if (pProbe && pProbe != (FARPROC)SteamAPI_Init) {
                g_hOriginalDll = hMod;
                ReFixLog("EnsureOriginal: Successfully loaded original Valve DLL from '%s'", path.c_str());
                break;
            } else {
                FreeLibrary(hMod);
            }
        }
    }

    // Fallback: If steam_api64_valve.dll was not found in target dir, try copying it from proxyDir/bin
    if (!g_hOriginalDll) {
        std::string fallbackSource = proxyDir + "steam_api64_valve.dll";
        std::string fallbackTarget = proxyDir + "steam_api64_valve.dll";
        if (proxyDir != exeDir) {
            std::string exeTarget = exeDir + "steam_api64_valve.dll";
            CopyFileA(fallbackSource.c_str(), exeTarget.c_str(), FALSE);
            HMODULE hMod = LoadLibraryA(exeTarget.c_str());
            if (hMod && hMod != g_hSelfModule) g_hOriginalDll = hMod;
        }
    }

    if (!g_hOriginalDll) {
        ReFixLog("EnsureOriginal: ERROR - Could not find steam_api64_valve.dll (proxyDir='%s', exeDir='%s')",
                 proxyDir.c_str(), exeDir.c_str());
        MessageBoxA(NULL,
            "ReFix Error: Could not find 'steam_api64_valve.dll'.\n\n"
            "Please ensure 'steam_api64_valve.dll' is present in the game's plugins folder.",
            "ReFix - Steam Proxy", MB_ICONERROR | MB_OK);
        return false;
    }

    g_pfn_Init                  = (fn_SteamAPI_Init_t)GetProcAddress(g_hOriginalDll, "SteamAPI_Init");
    g_pfn_InitFlat              = (fn_SteamAPI_InitFlat_t)GetProcAddress(g_hOriginalDll, "SteamAPI_InitFlat");
    g_pfn_InitSafe              = (fn_SteamAPI_InitSafe_t)GetProcAddress(g_hOriginalDll, "SteamAPI_InitSafe");
    g_pfn_InitAnon              = (fn_SteamAPI_Init_t)GetProcAddress(g_hOriginalDll, "SteamAPI_InitAnonymousUser");
    g_pfn_Restart               = (fn_SteamAPI_RestartAppIfNecessary_t)GetProcAddress(g_hOriginalDll, "SteamAPI_RestartAppIfNecessary");

    g_pfn_GSInit                = (fn_SteamInternal_GameServer_Init_t)GetProcAddress(g_hOriginalDll, "SteamInternal_GameServer_Init");
    if (!g_pfn_GSInit)
        g_pfn_GSInit            = (fn_SteamInternal_GameServer_Init_t)GetProcAddress(g_hOriginalDll, "SteamInternal_GameServer_Init_V2");
    g_pfn_GSInitSafe            = (fn_SteamGameServer_InitSafe_t)GetProcAddress(g_hOriginalDll, "SteamGameServer_InitSafe");
    g_pfn_SteamAPIInit_Internal = (fn_SteamInternal_SteamAPI_Init_t)GetProcAddress(g_hOriginalDll, "SteamInternal_SteamAPI_Init");

    g_pfn_GetPersonaName = (fn_GetPersonaName_t)GetProcAddress(g_hOriginalDll, "SteamAPI_ISteamFriends_GetPersonaName");
    if (!g_pfn_GetPersonaName)
        g_pfn_GetPersonaName = (fn_GetPersonaName_t)GetProcAddress(g_hOriginalDll, "SteamFriends");

    g_pfn_SteamFriends   = (fn_SteamFriends_v017_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamFriends_v017");
    if (!g_pfn_SteamFriends)
        g_pfn_SteamFriends = (fn_SteamFriends_v017_t)GetProcAddress(g_hOriginalDll, "SteamFriends");

    g_pfn_GetSteamID = (fn_GetSteamID_t)GetProcAddress(g_hOriginalDll, "SteamAPI_ISteamUser_GetSteamID");
    if (!g_pfn_GetSteamID)
        g_pfn_GetSteamID = (fn_GetSteamID_t)GetProcAddress(g_hOriginalDll, "SteamUser");

    g_pfn_SteamUser  = (fn_SteamUser_v021_t)GetProcAddress(g_hOriginalDll, "SteamAPI_SteamUser_v021");
    if (!g_pfn_SteamUser)
        g_pfn_SteamUser = (fn_SteamUser_v021_t)GetProcAddress(g_hOriginalDll, "SteamUser");

    g_pfn_RegisterCallback   = (fn_SteamAPI_RegisterCallback_t)GetProcAddress(g_hOriginalDll, "SteamAPI_RegisterCallback");
    g_pfn_RegisterCallResult = (fn_SteamAPI_RegisterCallResult_t)GetProcAddress(g_hOriginalDll, "SteamAPI_RegisterCallResult");

    // Populate forwarding table for ASM trampolines
    int resolvedCount = 0;
    for (int i = 0; i < STEAM_FORWARD_COUNT; i++) {
        g_steamProcs[i] = GetProcAddress(g_hOriginalDll, g_forwardNames[i]);
        if (g_steamProcs[i]) resolvedCount++;
    }
    ReFixLog("steam_api64 forwarding table populated: %d/%d exports resolved", resolvedCount, STEAM_FORWARD_COUNT);

    // Assign custom C++ function pointers using dynamic string-based export index lookup
    auto FindSteamExportIndex = [](const char* name) -> int {
        for (int i = 0; i < STEAM_FORWARD_COUNT; i++) {
            if (g_forwardNames[i] && strcmp(g_forwardNames[i], name) == 0) return i;
        }
        return -1;
    };

    int idxFavorites = FindSteamExportIndex("SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList");
    if (idxFavorites >= 0) {
        g_pfn_RequestFavoritesServerList = (fn_RequestServerList4_t)g_steamProcs[idxFavorites];
        g_steamProcs[idxFavorites] = (FARPROC)SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList;
    }

    int idxFriends = FindSteamExportIndex("SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList");
    if (idxFriends >= 0) {
        g_pfn_RequestFriendsServerList = (fn_RequestServerList4_t)g_steamProcs[idxFriends];
        g_steamProcs[idxFriends] = (FARPROC)SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList;
    }

    int idxHistory = FindSteamExportIndex("SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList");
    if (idxHistory >= 0) {
        g_pfn_RequestHistoryServerList = (fn_RequestServerList4_t)g_steamProcs[idxHistory];
        g_steamProcs[idxHistory] = (FARPROC)SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList;
    }

    int idxInternet = FindSteamExportIndex("SteamAPI_ISteamMatchmakingServers_RequestInternetServerList");
    if (idxInternet >= 0) {
        g_pfn_RequestInternetServerList = (fn_RequestServerList4_t)g_steamProcs[idxInternet];
        g_steamProcs[idxInternet] = (FARPROC)SteamAPI_ISteamMatchmakingServers_RequestInternetServerList;
    }

    int idxLAN = FindSteamExportIndex("SteamAPI_ISteamMatchmakingServers_RequestLANServerList");
    if (idxLAN >= 0) {
        g_pfn_RequestLANServerList = (fn_RequestLANServerList_t)g_steamProcs[idxLAN];
        g_steamProcs[idxLAN] = (FARPROC)SteamAPI_ISteamMatchmakingServers_RequestLANServerList;
    }

    int idxSpectator = FindSteamExportIndex("SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList");
    if (idxSpectator >= 0) {
        g_pfn_RequestSpectatorServerList = (fn_RequestServerList4_t)g_steamProcs[idxSpectator];
        g_steamProcs[idxSpectator] = (FARPROC)SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList;
    }

    int idxAddFavorite = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_AddFavoriteGame");
    if (idxAddFavorite >= 0) g_pfn_AddFavoriteGame = (fn_AddFavoriteGame_t)g_steamProcs[idxAddFavorite];

    int idxGetNumMembers = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
    if (idxGetNumMembers >= 0) g_pfn_GetNumLobbyMembers = (fn_GetNumLobbyMembers_t)g_steamProcs[idxGetNumMembers];

    int idxGetMemberByIdx = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
    if (idxGetMemberByIdx >= 0) g_pfn_GetLobbyMemberByIndex = (fn_GetLobbyMemberByIndex_t)g_steamProcs[idxGetMemberByIdx];

    int idxGetLobbyOwner = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    if (idxGetLobbyOwner >= 0) g_pfn_GetLobbyOwner = (fn_GetLobbyOwner_t)g_steamProcs[idxGetLobbyOwner];

    int idxGetLobbyData = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_GetLobbyData");
    if (idxGetLobbyData >= 0) g_pfn_GetLobbyData = (fn_GetLobbyData_t)g_steamProcs[idxGetLobbyData];

    int idxSetRichPresence = FindSteamExportIndex("SteamAPI_ISteamFriends_SetRichPresence");
    if (idxSetRichPresence >= 0) {
        g_pfn_SetRichPresence = (fn_SteamAPI_ISteamFriends_SetRichPresence_t)g_steamProcs[idxSetRichPresence];
        g_steamProcs[idxSetRichPresence] = (FARPROC)Intercepted_SetRichPresence;
    }

    int idxGetFriendGamePlayed = FindSteamExportIndex("SteamAPI_ISteamFriends_GetFriendGamePlayed");
    if (idxGetFriendGamePlayed >= 0) {
        g_pfn_Flat_GetFriendGamePlayed = (fn_Flat_GetFriendGamePlayed_t)g_steamProcs[idxGetFriendGamePlayed];
        g_steamProcs[idxGetFriendGamePlayed] = (FARPROC)Intercepted_SteamAPI_ISteamFriends_GetFriendGamePlayed;
        ReFixLog("EnsureOriginal: Intercepted flat export SteamAPI_ISteamFriends_GetFriendGamePlayed");
    }

    int idxAddStringFilter = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
    if (idxAddStringFilter >= 0) g_pfn_AddRequestLobbyListStringFilter = (fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_t)g_steamProcs[idxAddStringFilter];

    int idxDistanceFilter = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter");
    if (idxDistanceFilter >= 0) g_pfn_AddRequestLobbyListDistanceFilter = (fn_SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_t)g_steamProcs[idxDistanceFilter];

    int idxCreateLobby = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_CreateLobby");
    if (idxCreateLobby >= 0) {
        g_pfn_CreateLobby = (fn_SteamAPI_ISteamMatchmaking_CreateLobby_t)g_steamProcs[idxCreateLobby];
        g_steamProcs[idxCreateLobby] = (FARPROC)Intercepted_CreateLobby;
    }

    int idxRequestLobbies = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_RequestLobbyList");
    if (idxRequestLobbies >= 0) {
        g_pfn_RequestLobbyList = (fn_SteamAPI_ISteamMatchmaking_RequestLobbyList_t)g_steamProcs[idxRequestLobbies];
        g_steamProcs[idxRequestLobbies] = (FARPROC)Intercepted_RequestLobbyList;
    }

    int idxSetLobbyData = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_SetLobbyData");
    if (idxSetLobbyData >= 0) {
        g_pfn_SetLobbyData = (fn_SteamAPI_ISteamMatchmaking_SetLobbyData_t)g_steamProcs[idxSetLobbyData];
        g_steamProcs[idxSetLobbyData] = (FARPROC)Intercepted_SetLobbyData;
    }

    int idxJoinLobby = FindSteamExportIndex("SteamAPI_ISteamMatchmaking_JoinLobby");
    if (idxJoinLobby >= 0) {
        g_pfn_JoinLobby = (fn_SteamAPI_ISteamMatchmaking_JoinLobby_t)g_steamProcs[idxJoinLobby];
        g_steamProcs[idxJoinLobby] = (FARPROC)Intercepted_JoinLobby;
    }

    int idxRegisterNotif = FindSteamExportIndex("SteamAPI_RegisterCallback");
    if (idxRegisterNotif >= 0) {
        g_pfn_RegisterCallback = (fn_SteamAPI_RegisterCallback_t)g_steamProcs[idxRegisterNotif];
        g_steamProcs[idxRegisterNotif] = (FARPROC)Intercepted_SteamAPI_RegisterCallback;
    }

    int idxUnregisterNotif = FindSteamExportIndex("SteamAPI_UnregisterCallback");
    if (idxUnregisterNotif >= 0) {
        g_pfn_UnregisterCallback = (fn_SteamAPI_UnregisterCallback_t)g_steamProcs[idxUnregisterNotif];
        g_steamProcs[idxUnregisterNotif] = (FARPROC)Intercepted_SteamAPI_UnregisterCallback;
    }

    int idxGetAppID = FindSteamExportIndex("SteamAPI_ISteamUtils_GetAppID");
    if (idxGetAppID >= 0) {
        g_steamProcs[idxGetAppID] = (FARPROC)SteamAPI_ISteamUtils_GetAppID;
    }

    int idxBLoggedOn = FindSteamExportIndex("SteamAPI_ISteamUser_BLoggedOn");
    if (idxBLoggedOn >= 0) {
        g_steamProcs[idxBLoggedOn] = (FARPROC)SteamAPI_ISteamUser_BLoggedOn;
    }

    g_pfn_GetAuthSessionTicket = (fn_SteamAPI_ISteamUser_GetAuthSessionTicket_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_ISteamUser_GetAuthSessionTicket");
    int idxGetAuthSessionTicket = FindSteamExportIndex("SteamAPI_ISteamUser_GetAuthSessionTicket");
    if (idxGetAuthSessionTicket >= 0) {
        g_steamProcs[idxGetAuthSessionTicket] = (FARPROC)SteamAPI_ISteamUser_GetAuthSessionTicket;
    }

    g_pfn_GetAuthTicketForWebApi = (fn_SteamAPI_ISteamUser_GetAuthTicketForWebApi_t)GetProcAddress((HMODULE)g_hOriginalDll, "SteamAPI_ISteamUser_GetAuthTicketForWebApi");
    int idxGetAuthTicketForWebApi = FindSteamExportIndex("SteamAPI_ISteamUser_GetAuthTicketForWebApi");
    if (idxGetAuthTicketForWebApi >= 0) {
        g_steamProcs[idxGetAuthTicketForWebApi] = (FARPROC)SteamAPI_ISteamUser_GetAuthTicketForWebApi;
    }

    
    int idxGetRelayStatus = FindSteamExportIndex("SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus");
    if (idxGetRelayStatus >= 0) {
        g_pfn_Flat_GetRelayNetworkStatus = (fn_Flat_GetRelayNetworkStatus_t)g_steamProcs[idxGetRelayStatus];
        g_steamProcs[idxGetRelayStatus] = (FARPROC)Intercepted_SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus;
        ReFixLog("EnsureOriginal: Intercepted flat export SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus");
    }

    int idxInitRelay = FindSteamExportIndex("SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess");
    if (idxInitRelay >= 0) {
        g_pfn_Flat_InitRelayNetworkAccess = (fn_Flat_InitRelayNetworkAccess_t)g_steamProcs[idxInitRelay];
        g_steamProcs[idxInitRelay] = (FARPROC)Intercepted_SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess;
        ReFixLog("EnsureOriginal: Intercepted flat export SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess");
    }

    
    int idxShutdown = FindSteamExportIndex("SteamAPI_Shutdown");
    if (idxShutdown >= 0) {
        g_pfn_Shutdown = (fn_SteamAPI_Shutdown_t)g_steamProcs[idxShutdown];
        g_steamProcs[idxShutdown] = (FARPROC)SteamAPI_Shutdown;
        ReFixLog("EnsureOriginal: Intercepted SteamAPI_Shutdown");
    }

    int idxRunCallbacks = FindSteamExportIndex("SteamAPI_RunCallbacks");
    if (idxRunCallbacks >= 0) {
        g_pfn_RunCallbacks = (fn_SteamAPI_RunCallbacks_t)g_steamProcs[idxRunCallbacks];
        g_steamProcs[idxRunCallbacks] = (FARPROC)SteamAPI_RunCallbacks;
    }

    int idxManualRunFrame = FindSteamExportIndex("SteamAPI_ManualDispatch_RunFrame");
    if (idxManualRunFrame >= 0) {
        g_pfn_ManualDispatch_RunFrame = (fn_SteamAPI_ManualDispatch_RunFrame_t)g_steamProcs[idxManualRunFrame];
        g_steamProcs[idxManualRunFrame] = (FARPROC)SteamAPI_ManualDispatch_RunFrame;
    }

    int idxManualGetNext = FindSteamExportIndex("SteamAPI_ManualDispatch_GetNextCallback");
    if (idxManualGetNext >= 0) {
        g_pfn_ManualDispatch_GetNextCallback = (fn_SteamAPI_ManualDispatch_GetNextCallback_t)g_steamProcs[idxManualGetNext];
        g_steamProcs[idxManualGetNext] = (FARPROC)SteamAPI_ManualDispatch_GetNextCallback;
    }

    // SteamInternal_SteamAPI_Init is handled via C++ __declspec(dllexport) below
    // (no slot redirect needed — dllexport takes precedence over the .def passthrough)


    // Check engine type for Godot-specific behavior
    g_godotIsEngine = (_stricmp(g_config.engineType.c_str(), "Godot") == 0);
    if (g_godotIsEngine) {
        ReFixLog("EngineType=Godot detected: Winsock P2P hook disabled, LobbyEnter_t synthesis enabled");
    }

    ReFixLog("steam_api64.dll proxy initialized successfully");
    return true;
}

static void CapturePersonaName() {
    if (!g_pfn_GetPersonaName || !g_pfn_SteamFriends) return;
    void* friends = g_pfn_SteamFriends();
    if (!friends) return;
    const char* name = g_pfn_GetPersonaName(friends);
    if (name && name[0] != '\0') {
        SetEnvironmentVariableA("REFIX_STEAM_PERSONA_NAME", name);
        ReFixLog("CapturePersonaName: %s", name);
    }
    if (g_pfn_GetSteamID && g_pfn_SteamUser) {
        void* user = g_pfn_SteamUser();
        if (user) {
            uint64_t steamId = g_pfn_GetSteamID(user);
            if (steamId != 0) {
                g_capturedSteamID = steamId;
                char idStr[32];
                sprintf_s(idStr, sizeof(idStr), "%llu", steamId);
                SetEnvironmentVariableA("REFIX_STEAM_ID", idStr);
                ReFixLog("CaptureSteamID: %s", idStr);
            }
        }
    }
}

// Intercepted Exports implemented directly
extern "C" __declspec(dllexport) bool SteamAPI_Init() {
    ApplySteamEnv();
    ReFixLog("SteamAPI_Init called");
    if (!EnsureOriginal()) {
        ReFixLog("SteamAPI_Init: EnsureOriginal failed");
        return false;
    }
    bool result = false;
    if (g_pfn_Init) {
        result = g_pfn_Init();
        ReFixLog("SteamAPI_Init: called via SteamAPI_Init -> result=%d", result);
    } else if (g_pfn_InitFlat) {
        char errMsg[1024] = { 0 };
        int flatRes = g_pfn_InitFlat(errMsg);
        result = (flatRes == 0);
        ReFixLog("SteamAPI_Init: called via SteamAPI_InitFlat -> result=%d, msg='%s'", flatRes, errMsg);
    } else if (g_pfn_SteamAPIInit_Internal) {
        char errMsg[1024] = { 0 };
        int intRes = g_pfn_SteamAPIInit_Internal("", errMsg);
        result = (intRes == 0);
        ReFixLog("SteamAPI_Init: called via SteamInternal_SteamAPI_Init -> result=%d, msg='%s'", intRes, errMsg);
    } else if (g_pfn_InitSafe) {
        result = g_pfn_InitSafe();
        ReFixLog("SteamAPI_Init: called via SteamAPI_InitSafe -> result=%d", result);
    }
    ReFixLog("SteamAPI_Init: final result=%d", result);
    if (result) {
        g_steamInitTick = GetTickCount();
        CapturePersonaName();
        TriggerSyntheticRelayCallback();
        // Install Winsock -> Steam P2P redirect hooks (ONLY for Unreal Engine in Valve Online mode)
        if (g_unrealIsEngine && !g_isGoldbergMode) {
            SteamP2PHook::Install(g_hOriginalDll);
            ReFixLog("SteamAPI_Init: Steam P2P Winsock hooks installed");
            // Force immediate re-resolve of ISteamNetworking now that Steam is initialized
            extern void SteamP2PHook_ForceResolve();
            SteamP2PHook_ForceResolve();
        } else {
            ReFixLog("SteamAPI_Init: Winsock P2P hook skipped (godot=%d, unreal=%d, goldberg=%d)", g_godotIsEngine, g_unrealIsEngine, g_isGoldbergMode);
        }
        InstallVTableHooks();
    }
    return result;
}

extern "C" __declspec(dllexport) bool SteamAPI_InitSafe() {
    ApplySteamEnv();
    ReFixLog("SteamAPI_InitSafe called");
    if (!EnsureOriginal()) return false;
    bool result = false;
    if (g_pfn_InitSafe) {
        result = g_pfn_InitSafe();
    } else if (g_pfn_Init) {
        result = g_pfn_Init();
    } else if (g_pfn_InitFlat) {
        char errMsg[1024] = { 0 };
        result = (g_pfn_InitFlat(errMsg) == 0);
    } else if (g_pfn_SteamAPIInit_Internal) {
        char errMsg[1024] = { 0 };
        result = (g_pfn_SteamAPIInit_Internal("", errMsg) == 0);
    }
    ReFixLog("SteamAPI_InitSafe: result=%d", result);
    if (result) { CapturePersonaName(); InstallVTableHooks(); }
    return result;
}

extern "C" __declspec(dllexport) int SteamAPI_InitFlat(char* pOutErrMsg) {
    ApplySteamEnv();
    ReFixLog("SteamAPI_InitFlat called");
    if (!EnsureOriginal()) {
        if (pOutErrMsg) strncpy_s(pOutErrMsg, 1024, "ReFix: EnsureOriginal failed", _TRUNCATE);
        return 1;
    }
    char localErr[1024] = { 0 };
    char* targetErr = pOutErrMsg ? pOutErrMsg : localErr;
    int result = 1;

    if (g_pfn_InitFlat) {
        result = g_pfn_InitFlat(targetErr);
    } else if (g_pfn_SteamAPIInit_Internal) {
        result = g_pfn_SteamAPIInit_Internal("", targetErr);
    } else if (g_pfn_Init) {
        bool ok = g_pfn_Init();
        result = ok ? 0 : 1;
    } else if (g_pfn_InitSafe) {
        bool ok = g_pfn_InitSafe();
        result = ok ? 0 : 1;
    }
    ReFixLog("SteamAPI_InitFlat: result=%d, msg='%s'", result, targetErr);
    if (result == 0) {
        CapturePersonaName();
        if (!g_godotIsEngine && !g_isGoldbergMode) {
            SteamP2PHook::Install(g_hOriginalDll);
            extern void SteamP2PHook_ForceResolve();
            SteamP2PHook_ForceResolve();
        }
        InstallVTableHooks();
    }
    return result;
}

extern "C" __declspec(dllexport) bool SteamAPI_InitAnonymousUser() {
    ApplySteamEnv();
    if (!EnsureOriginal()) return false;
    bool result = false;
    if (g_pfn_InitAnon) result = g_pfn_InitAnon();
    else if (g_pfn_Init) result = g_pfn_Init();
    if (result) { CapturePersonaName(); InstallVTableHooks(); }
    return result;
}

extern "C" __declspec(dllexport) bool SteamAPI_RestartAppIfNecessary(unsigned int unOwnAppID) {
    (void)unOwnAppID;
    return false;
}

extern "C" __declspec(dllexport) bool SteamInternal_GameServer_Init(
    uint32_t unIP, uint16_t usGamePort, uint16_t usQueryPort,
    int eServerMode, const char* pchVersionString)
{
    ApplySteamEnv();
    ReFixLog("SteamInternal_GameServer_Init: IP=%u, GamePort=%u, QueryPort=%u, Mode=%d, Ver=%s",
             unIP, usGamePort, usQueryPort, eServerMode, pchVersionString ? pchVersionString : "null");
    if (!EnsureOriginal() || !g_pfn_GSInit) return false;
    return g_pfn_GSInit(unIP, usGamePort, usQueryPort, eServerMode, pchVersionString);
}

// SteamInternal_SteamAPI_Init: new Steam SDK primary init path (SDK v08.63+)
// Returns ESteamAPIInitResult: 0 = k_ESteamAPIInitResult_OK
extern "C" __declspec(dllexport) int SteamInternal_SteamAPI_Init(
    const char* pszInternalCheckInterfaceVersions, char* pOutErrMsg)
{
    ApplySteamEnv();
    ReFixLog("SteamInternal_SteamAPI_Init called (ver='%s')", pszInternalCheckInterfaceVersions ? pszInternalCheckInterfaceVersions : "");
    if (!EnsureOriginal()) {
        if (pOutErrMsg) strncpy_s(pOutErrMsg, 1024, "ReFix: EnsureOriginal failed", _TRUNCATE);
        return 1; // k_ESteamAPIInitResult_NoSteamClient
    }
    char localErr[1024] = { 0 };
    char* targetErr = pOutErrMsg ? pOutErrMsg : localErr;
    int result = 1;

    if (g_pfn_SteamAPIInit_Internal) {
        result = g_pfn_SteamAPIInit_Internal(pszInternalCheckInterfaceVersions, targetErr);
    } else if (g_pfn_InitFlat) {
        result = g_pfn_InitFlat(targetErr);
    } else if (g_pfn_Init) {
        bool ok = g_pfn_Init();
        result = ok ? 0 : 1;
    } else if (g_pfn_InitSafe) {
        bool ok = g_pfn_InitSafe();
        result = ok ? 0 : 1;
    }
    ReFixLog("SteamInternal_SteamAPI_Init: result=%d, msg='%s'", result, targetErr);
    if (result == 0) {
        g_steamInitTick = GetTickCount();
        CapturePersonaName();
        TriggerSyntheticRelayCallback();
        // Install Winsock -> Steam P2P redirect hooks (ONLY for Unreal Engine in Valve Online mode)
        if (g_unrealIsEngine && !g_isGoldbergMode) {
            SteamP2PHook::Install(g_hOriginalDll);
            extern void SteamP2PHook_ForceResolve();
            SteamP2PHook_ForceResolve();
        } else {
            ReFixLog("SteamInternal_SteamAPI_Init: Winsock P2P hook skipped (godot=%d, unreal=%d, goldberg=%d)", g_godotIsEngine, g_unrealIsEngine, g_isGoldbergMode);
        }
        InstallVTableHooks();
    }

    return result;
}

extern "C" __declspec(dllexport) bool SteamGameServer_InitSafe() {
    ApplySteamEnv();
    ReFixLog("SteamGameServer_InitSafe called");
    if (!EnsureOriginal() || !g_pfn_GSInitSafe) return false;
    return g_pfn_GSInitSafe();
}

extern "C" __declspec(dllexport) int SteamAPI_ISteamUser_UserHasLicenseForApp(
    void* self, uint64_t steamID, uint32_t appID) {
    ReFixLog("UserHasLicenseForApp: steamID=%llu, appID=%u -> HasLicense(0)", steamID, appID);
    return 0;
}

extern "C" __declspec(dllexport) int SteamAPI_ISteamGameServer_UserHasLicenseForApp(
    void* self, uint64_t steamID, uint32_t appID) {
    ReFixLog("GS_UserHasLicenseForApp: steamID=%llu, appID=%u -> HasLicense(0)", steamID, appID);
    return 0;
}

extern "C" __declspec(dllexport) int SteamAPI_ISteamMatchmaking_AddFavoriteGame(
    void* self, uint32_t nAppID, uint32_t nIP, uint16_t nConnPort,
    uint16_t nQueryPort, uint32_t unFlags, uint32_t rTime32) {
    LoadConfig();
    if (!g_pfn_AddFavoriteGame) return 0;
    return g_pfn_AddFavoriteGame(self, g_maskAppIdNum, nIP, nConnPort, nQueryPort, unFlags, rTime32);
}

extern "C" __declspec(dllexport) void SteamAPI_RunCallbacks() {
    if (s_runCallbacksLogged++ < 3) {
        ReFixLog("SteamAPI_RunCallbacks called (frame=%d)", s_runCallbacksLogged);
    }
    if (g_pfn_RunCallbacks) g_pfn_RunCallbacks();
    DispatchRelayCallbacks();
    DispatchPendingAuthCallbacks();
}

static int s_manualRunFrameLogged = 0;
extern "C" __declspec(dllexport) void SteamAPI_ManualDispatch_RunFrame(uint32_t hSteamPipe) {
    if (s_manualRunFrameLogged++ < 3) {
        ReFixLog("SteamAPI_ManualDispatch_RunFrame called (pipe=%u, frame=%d)", hSteamPipe, s_manualRunFrameLogged);
    }
    if (g_pfn_ManualDispatch_RunFrame) g_pfn_ManualDispatch_RunFrame(hSteamPipe);
    DispatchPendingAuthCallbacks();
}

extern "C" __declspec(dllexport) bool SteamAPI_ManualDispatch_GetNextCallback(uint32_t hSteamPipe, void* pCallbackMsg) {
    if (g_pfn_ManualDispatch_GetNextCallback) {
        bool res = g_pfn_ManualDispatch_GetNextCallback(hSteamPipe, pCallbackMsg);
        if (res) {
            if (pCallbackMsg) {
                auto* msg = (ReFix_CallbackMsg_t*)pCallbackMsg;
                if (msg->m_iCallback == 1281) { // k_iSteamRelayNetworkStatusChanged
                    if (msg->m_pubParam && msg->m_cubParam >= (int32_t)sizeof(ReFix_SteamRelayNetworkStatus_t)) {
                        auto* d = (ReFix_SteamRelayNetworkStatus_t*)msg->m_pubParam;
                        d->m_eAvail = k_eRelayAvail_Current; // 100
                        d->m_bPingMeasurementInProgress = 0;
                        d->m_eAvailNetworkConfig = k_eRelayAvail_Current; // 100
                        d->m_eAvailAnyRelay = k_eRelayAvail_Current; // 100
                        strncpy_s(d->m_debugMsg, sizeof(d->m_debugMsg), "OK (ReFix)", _TRUNCATE);
                        ReFixLog("ManualDispatch_GetNextCallback: Overrode Callback 1281 to Current (100)");
                    }
                }
            }
            return true;
        }
    }
    if (g_syntheticRelayPending.exchange(false) && pCallbackMsg) {
        g_syntheticRelayData.m_eAvail = k_eRelayAvail_Current; // 100
        g_syntheticRelayData.m_bPingMeasurementInProgress = 0;
        g_syntheticRelayData.m_eAvailNetworkConfig = k_eRelayAvail_Current; // 100
        g_syntheticRelayData.m_eAvailAnyRelay = k_eRelayAvail_Current; // 100
        strncpy_s(g_syntheticRelayData.m_debugMsg, sizeof(g_syntheticRelayData.m_debugMsg), "OK (ReFix)", _TRUNCATE);

        auto* msg = (ReFix_CallbackMsg_t*)pCallbackMsg;
        msg->m_hSteamUser = 0;
        msg->m_iCallback = 1281; // k_iSteamRelayNetworkStatusChanged
        msg->m_pubParam = (uint8_t*)&g_syntheticRelayData;
        msg->m_cubParam = (int32_t)sizeof(ReFix_SteamRelayNetworkStatus_t);
        ReFixLog("ManualDispatch_GetNextCallback: Delivered synthetic Callback 1281 (Current 100)");
        return true;
    }
    return false;
}

extern "C" __declspec(dllexport) uint32_t SteamAPI_ISteamUser_GetAuthSessionTicket(void* self, void* pTicket, int cbMaxTicket, uint32_t* pcbTicket) {
    uint32_t handle = 1;
    if (g_pfn_GetAuthSessionTicket) {
        handle = g_pfn_GetAuthSessionTicket(self, pTicket, cbMaxTicket, pcbTicket);
    }
    ReFixLog("SteamAPI_ISteamUser_GetAuthSessionTicket: handle=%u, cbTicket=%u", handle, (pcbTicket ? *pcbTicket : 0));
    g_lastAuthTicketHandle = handle;
    if (pTicket && pcbTicket && *pcbTicket > 0) {
        g_lastAuthTicketData.assign((uint8_t*)pTicket, (uint8_t*)pTicket + *pcbTicket);
    }
    return handle;
}

extern "C" __declspec(dllexport) uint32_t SteamAPI_ISteamUser_GetAuthTicketForWebApi(void* self, const char* pchIdentity) {
    uint32_t handle = 1;
    if (g_pfn_GetAuthTicketForWebApi) {
        handle = g_pfn_GetAuthTicketForWebApi(self, pchIdentity);
    }
    ReFixLog("SteamAPI_ISteamUser_GetAuthTicketForWebApi ('%s'): handle=%u", pchIdentity ? pchIdentity : "", handle);
    if (handle != 0) {
        g_lastAuthTicketHandle = handle;
    }
    return handle;
}


static void SafeBackendShutdown() {
    if (g_pfn_Shutdown) {
        __try {
            g_pfn_Shutdown();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

extern "C" __declspec(dllexport) void SteamAPI_Shutdown() {
    ReFixLog("SteamAPI_Shutdown called - cleaning up proxy state");
    g_authDispatchRunning.store(false, std::memory_order_relaxed);
    g_hotkeyRunning.store(false, std::memory_order_relaxed);
    SteamP2PHook::Uninstall();
    {
        std::lock_guard<std::mutex> lg(g_relayCallbackMutex);
        g_registeredRelayCallbacks.clear();
    }
    {
        std::lock_guard<std::mutex> lg(g_callbackMutex);
        g_registeredCallbacks.clear();
    }
    SafeBackendShutdown();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_hSelfModule = hModule;
            DisableThreadLibraryCalls(hModule);
            ApplySteamEnv();
            EnsureOriginal();
            break;
        case DLL_PROCESS_DETACH:
            g_authDispatchRunning.store(false, std::memory_order_relaxed);
            g_hotkeyRunning.store(false, std::memory_order_relaxed);
            SteamP2PHook::Uninstall();
            // Note: DO NOT call ReFixNet::UnmapUPnPPort (COM initialization) or FreeLibrary
            // during DLL_PROCESS_DETACH. Calling COM/FreeLibrary in DllMain violates loader lock
            // and causes crashes on process exit. OS cleans up process memory safely.
            break;
    }
    return TRUE;
}