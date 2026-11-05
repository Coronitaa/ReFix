; =============================================================================
; ReFix - steam_api.dll x86 Forwarding Trampolines (AUTO-GENERATED)
; =============================================================================
; Forwards 1141 exports via jump table (g_steamProcs).
; =============================================================================

.386
.model flat, c

.data
EXTERN g_steamProcs:DWORD

.code

GetHSteamPipe_proxy PROC
    jmp DWORD PTR [g_steamProcs + 0]
GetHSteamPipe_proxy ENDP
GetHSteamUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4]
GetHSteamUser_proxy ENDP
SteamAPI_GetHSteamPipe_proxy PROC
    jmp DWORD PTR [g_steamProcs + 8]
SteamAPI_GetHSteamPipe_proxy ENDP
SteamAPI_GetHSteamUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 12]
SteamAPI_GetHSteamUser_proxy ENDP
SteamAPI_GetSteamInstallPath_proxy PROC
    jmp DWORD PTR [g_steamProcs + 16]
SteamAPI_GetSteamInstallPath_proxy ENDP
SteamAPI_ISteamAppList_GetAppBuildId_proxy PROC
    jmp DWORD PTR [g_steamProcs + 20]
SteamAPI_ISteamAppList_GetAppBuildId_proxy ENDP
SteamAPI_ISteamAppList_GetAppInstallDir_proxy PROC
    jmp DWORD PTR [g_steamProcs + 24]
SteamAPI_ISteamAppList_GetAppInstallDir_proxy ENDP
SteamAPI_ISteamAppList_GetAppName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 28]
SteamAPI_ISteamAppList_GetAppName_proxy ENDP
SteamAPI_ISteamAppList_GetInstalledApps_proxy PROC
    jmp DWORD PTR [g_steamProcs + 32]
SteamAPI_ISteamAppList_GetInstalledApps_proxy ENDP
SteamAPI_ISteamAppList_GetNumInstalledApps_proxy PROC
    jmp DWORD PTR [g_steamProcs + 36]
SteamAPI_ISteamAppList_GetNumInstalledApps_proxy ENDP
SteamAPI_ISteamApps_BGetDLCDataByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 40]
SteamAPI_ISteamApps_BGetDLCDataByIndex_proxy ENDP
SteamAPI_ISteamApps_BIsAppInstalled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 44]
SteamAPI_ISteamApps_BIsAppInstalled_proxy ENDP
SteamAPI_ISteamApps_BIsCybercafe_proxy PROC
    jmp DWORD PTR [g_steamProcs + 48]
SteamAPI_ISteamApps_BIsCybercafe_proxy ENDP
SteamAPI_ISteamApps_BIsDlcInstalled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 52]
SteamAPI_ISteamApps_BIsDlcInstalled_proxy ENDP
SteamAPI_ISteamApps_BIsLowViolence_proxy PROC
    jmp DWORD PTR [g_steamProcs + 56]
SteamAPI_ISteamApps_BIsLowViolence_proxy ENDP
SteamAPI_ISteamApps_BIsSubscribed_proxy PROC
    jmp DWORD PTR [g_steamProcs + 60]
SteamAPI_ISteamApps_BIsSubscribed_proxy ENDP
SteamAPI_ISteamApps_BIsSubscribedApp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 64]
SteamAPI_ISteamApps_BIsSubscribedApp_proxy ENDP
SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing_proxy PROC
    jmp DWORD PTR [g_steamProcs + 68]
SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing_proxy ENDP
SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend_proxy PROC
    jmp DWORD PTR [g_steamProcs + 72]
SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend_proxy ENDP
SteamAPI_ISteamApps_BIsTimedTrial_proxy PROC
    jmp DWORD PTR [g_steamProcs + 76]
SteamAPI_ISteamApps_BIsTimedTrial_proxy ENDP
SteamAPI_ISteamApps_BIsVACBanned_proxy PROC
    jmp DWORD PTR [g_steamProcs + 80]
SteamAPI_ISteamApps_BIsVACBanned_proxy ENDP
SteamAPI_ISteamApps_GetAppBuildId_proxy PROC
    jmp DWORD PTR [g_steamProcs + 84]
SteamAPI_ISteamApps_GetAppBuildId_proxy ENDP
SteamAPI_ISteamApps_GetAppInstallDir_proxy PROC
    jmp DWORD PTR [g_steamProcs + 88]
SteamAPI_ISteamApps_GetAppInstallDir_proxy ENDP
SteamAPI_ISteamApps_GetAppOwner_proxy PROC
    jmp DWORD PTR [g_steamProcs + 92]
SteamAPI_ISteamApps_GetAppOwner_proxy ENDP
SteamAPI_ISteamApps_GetAvailableGameLanguages_proxy PROC
    jmp DWORD PTR [g_steamProcs + 96]
SteamAPI_ISteamApps_GetAvailableGameLanguages_proxy ENDP
SteamAPI_ISteamApps_GetCurrentBetaName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 100]
SteamAPI_ISteamApps_GetCurrentBetaName_proxy ENDP
SteamAPI_ISteamApps_GetCurrentGameLanguage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 104]
SteamAPI_ISteamApps_GetCurrentGameLanguage_proxy ENDP
SteamAPI_ISteamApps_GetDLCCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 108]
SteamAPI_ISteamApps_GetDLCCount_proxy ENDP
SteamAPI_ISteamApps_GetDlcDownloadProgress_proxy PROC
    jmp DWORD PTR [g_steamProcs + 112]
SteamAPI_ISteamApps_GetDlcDownloadProgress_proxy ENDP
SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime_proxy PROC
    jmp DWORD PTR [g_steamProcs + 116]
SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime_proxy ENDP
SteamAPI_ISteamApps_GetFileDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 120]
SteamAPI_ISteamApps_GetFileDetails_proxy ENDP
SteamAPI_ISteamApps_GetInstalledDepots_proxy PROC
    jmp DWORD PTR [g_steamProcs + 124]
SteamAPI_ISteamApps_GetInstalledDepots_proxy ENDP
SteamAPI_ISteamApps_GetLaunchCommandLine_proxy PROC
    jmp DWORD PTR [g_steamProcs + 128]
SteamAPI_ISteamApps_GetLaunchCommandLine_proxy ENDP
SteamAPI_ISteamApps_GetLaunchQueryParam_proxy PROC
    jmp DWORD PTR [g_steamProcs + 132]
SteamAPI_ISteamApps_GetLaunchQueryParam_proxy ENDP
SteamAPI_ISteamApps_InstallDLC_proxy PROC
    jmp DWORD PTR [g_steamProcs + 136]
SteamAPI_ISteamApps_InstallDLC_proxy ENDP
SteamAPI_ISteamApps_MarkContentCorrupt_proxy PROC
    jmp DWORD PTR [g_steamProcs + 140]
SteamAPI_ISteamApps_MarkContentCorrupt_proxy ENDP
SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys_proxy PROC
    jmp DWORD PTR [g_steamProcs + 144]
SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys_proxy ENDP
SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey_proxy PROC
    jmp DWORD PTR [g_steamProcs + 148]
SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey_proxy ENDP
SteamAPI_ISteamApps_UninstallDLC_proxy PROC
    jmp DWORD PTR [g_steamProcs + 152]
SteamAPI_ISteamApps_UninstallDLC_proxy ENDP
SteamAPI_ISteamClient_BReleaseSteamPipe_proxy PROC
    jmp DWORD PTR [g_steamProcs + 156]
SteamAPI_ISteamClient_BReleaseSteamPipe_proxy ENDP
SteamAPI_ISteamClient_BShutdownIfAllPipesClosed_proxy PROC
    jmp DWORD PTR [g_steamProcs + 160]
SteamAPI_ISteamClient_BShutdownIfAllPipesClosed_proxy ENDP
SteamAPI_ISteamClient_ConnectToGlobalUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 164]
SteamAPI_ISteamClient_ConnectToGlobalUser_proxy ENDP
SteamAPI_ISteamClient_CreateLocalUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 168]
SteamAPI_ISteamClient_CreateLocalUser_proxy ENDP
SteamAPI_ISteamClient_CreateSteamPipe_proxy PROC
    jmp DWORD PTR [g_steamProcs + 172]
SteamAPI_ISteamClient_CreateSteamPipe_proxy ENDP
SteamAPI_ISteamClient_GetIPCCallCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 176]
SteamAPI_ISteamClient_GetIPCCallCount_proxy ENDP
SteamAPI_ISteamClient_GetISteamAppList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 180]
SteamAPI_ISteamClient_GetISteamAppList_proxy ENDP
SteamAPI_ISteamClient_GetISteamApps_proxy PROC
    jmp DWORD PTR [g_steamProcs + 184]
SteamAPI_ISteamClient_GetISteamApps_proxy ENDP
SteamAPI_ISteamClient_GetISteamController_proxy PROC
    jmp DWORD PTR [g_steamProcs + 188]
SteamAPI_ISteamClient_GetISteamController_proxy ENDP
SteamAPI_ISteamClient_GetISteamFriends_proxy PROC
    jmp DWORD PTR [g_steamProcs + 192]
SteamAPI_ISteamClient_GetISteamFriends_proxy ENDP
SteamAPI_ISteamClient_GetISteamGameSearch_proxy PROC
    jmp DWORD PTR [g_steamProcs + 196]
SteamAPI_ISteamClient_GetISteamGameSearch_proxy ENDP
SteamAPI_ISteamClient_GetISteamGameServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 200]
SteamAPI_ISteamClient_GetISteamGameServer_proxy ENDP
SteamAPI_ISteamClient_GetISteamGameServerStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 204]
SteamAPI_ISteamClient_GetISteamGameServerStats_proxy ENDP
SteamAPI_ISteamClient_GetISteamGenericInterface_proxy PROC
    jmp DWORD PTR [g_steamProcs + 208]
SteamAPI_ISteamClient_GetISteamGenericInterface_proxy ENDP
SteamAPI_ISteamClient_GetISteamHTMLSurface_proxy PROC
    jmp DWORD PTR [g_steamProcs + 212]
SteamAPI_ISteamClient_GetISteamHTMLSurface_proxy ENDP
SteamAPI_ISteamClient_GetISteamHTTP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 216]
SteamAPI_ISteamClient_GetISteamHTTP_proxy ENDP
SteamAPI_ISteamClient_GetISteamInput_proxy PROC
    jmp DWORD PTR [g_steamProcs + 220]
SteamAPI_ISteamClient_GetISteamInput_proxy ENDP
SteamAPI_ISteamClient_GetISteamInventory_proxy PROC
    jmp DWORD PTR [g_steamProcs + 224]
SteamAPI_ISteamClient_GetISteamInventory_proxy ENDP
SteamAPI_ISteamClient_GetISteamMatchmaking_proxy PROC
    jmp DWORD PTR [g_steamProcs + 228]
SteamAPI_ISteamClient_GetISteamMatchmaking_proxy ENDP
SteamAPI_ISteamClient_GetISteamMatchmakingServers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 232]
SteamAPI_ISteamClient_GetISteamMatchmakingServers_proxy ENDP
SteamAPI_ISteamClient_GetISteamMusic_proxy PROC
    jmp DWORD PTR [g_steamProcs + 236]
SteamAPI_ISteamClient_GetISteamMusic_proxy ENDP
SteamAPI_ISteamClient_GetISteamMusicRemote_proxy PROC
    jmp DWORD PTR [g_steamProcs + 240]
SteamAPI_ISteamClient_GetISteamMusicRemote_proxy ENDP
SteamAPI_ISteamClient_GetISteamNetworking_proxy PROC
    jmp DWORD PTR [g_steamProcs + 244]
SteamAPI_ISteamClient_GetISteamNetworking_proxy ENDP
SteamAPI_ISteamClient_GetISteamParentalSettings_proxy PROC
    jmp DWORD PTR [g_steamProcs + 248]
SteamAPI_ISteamClient_GetISteamParentalSettings_proxy ENDP
SteamAPI_ISteamClient_GetISteamParties_proxy PROC
    jmp DWORD PTR [g_steamProcs + 252]
SteamAPI_ISteamClient_GetISteamParties_proxy ENDP
SteamAPI_ISteamClient_GetISteamRemotePlay_proxy PROC
    jmp DWORD PTR [g_steamProcs + 256]
SteamAPI_ISteamClient_GetISteamRemotePlay_proxy ENDP
SteamAPI_ISteamClient_GetISteamRemoteStorage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 260]
SteamAPI_ISteamClient_GetISteamRemoteStorage_proxy ENDP
SteamAPI_ISteamClient_GetISteamScreenshots_proxy PROC
    jmp DWORD PTR [g_steamProcs + 264]
SteamAPI_ISteamClient_GetISteamScreenshots_proxy ENDP
SteamAPI_ISteamClient_GetISteamUGC_proxy PROC
    jmp DWORD PTR [g_steamProcs + 268]
SteamAPI_ISteamClient_GetISteamUGC_proxy ENDP
SteamAPI_ISteamClient_GetISteamUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 272]
SteamAPI_ISteamClient_GetISteamUser_proxy ENDP
SteamAPI_ISteamClient_GetISteamUserStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 276]
SteamAPI_ISteamClient_GetISteamUserStats_proxy ENDP
SteamAPI_ISteamClient_GetISteamUtils_proxy PROC
    jmp DWORD PTR [g_steamProcs + 280]
SteamAPI_ISteamClient_GetISteamUtils_proxy ENDP
SteamAPI_ISteamClient_GetISteamVideo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 284]
SteamAPI_ISteamClient_GetISteamVideo_proxy ENDP
SteamAPI_ISteamClient_ReleaseUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 288]
SteamAPI_ISteamClient_ReleaseUser_proxy ENDP
SteamAPI_ISteamClient_SetLocalIPBinding_proxy PROC
    jmp DWORD PTR [g_steamProcs + 292]
SteamAPI_ISteamClient_SetLocalIPBinding_proxy ENDP
SteamAPI_ISteamClient_SetWarningMessageHook_proxy PROC
    jmp DWORD PTR [g_steamProcs + 296]
SteamAPI_ISteamClient_SetWarningMessageHook_proxy ENDP
SteamAPI_ISteamController_ActivateActionSet_proxy PROC
    jmp DWORD PTR [g_steamProcs + 300]
SteamAPI_ISteamController_ActivateActionSet_proxy ENDP
SteamAPI_ISteamController_ActivateActionSetLayer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 304]
SteamAPI_ISteamController_ActivateActionSetLayer_proxy ENDP
SteamAPI_ISteamController_DeactivateActionSetLayer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 308]
SteamAPI_ISteamController_DeactivateActionSetLayer_proxy ENDP
SteamAPI_ISteamController_DeactivateAllActionSetLayers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 312]
SteamAPI_ISteamController_DeactivateAllActionSetLayers_proxy ENDP
SteamAPI_ISteamController_GetActionOriginFromXboxOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 316]
SteamAPI_ISteamController_GetActionOriginFromXboxOrigin_proxy ENDP
SteamAPI_ISteamController_GetActionSetHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 320]
SteamAPI_ISteamController_GetActionSetHandle_proxy ENDP
SteamAPI_ISteamController_GetActiveActionSetLayers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 324]
SteamAPI_ISteamController_GetActiveActionSetLayers_proxy ENDP
SteamAPI_ISteamController_GetAnalogActionData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 328]
SteamAPI_ISteamController_GetAnalogActionData_proxy ENDP
SteamAPI_ISteamController_GetAnalogActionHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 332]
SteamAPI_ISteamController_GetAnalogActionHandle_proxy ENDP
SteamAPI_ISteamController_GetAnalogActionOrigins_proxy PROC
    jmp DWORD PTR [g_steamProcs + 336]
SteamAPI_ISteamController_GetAnalogActionOrigins_proxy ENDP
SteamAPI_ISteamController_GetConnectedControllers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 340]
SteamAPI_ISteamController_GetConnectedControllers_proxy ENDP
SteamAPI_ISteamController_GetControllerBindingRevision_proxy PROC
    jmp DWORD PTR [g_steamProcs + 344]
SteamAPI_ISteamController_GetControllerBindingRevision_proxy ENDP
SteamAPI_ISteamController_GetControllerForGamepadIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 348]
SteamAPI_ISteamController_GetControllerForGamepadIndex_proxy ENDP
SteamAPI_ISteamController_GetCurrentActionSet_proxy PROC
    jmp DWORD PTR [g_steamProcs + 352]
SteamAPI_ISteamController_GetCurrentActionSet_proxy ENDP
SteamAPI_ISteamController_GetDigitalActionData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 356]
SteamAPI_ISteamController_GetDigitalActionData_proxy ENDP
SteamAPI_ISteamController_GetDigitalActionHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 360]
SteamAPI_ISteamController_GetDigitalActionHandle_proxy ENDP
SteamAPI_ISteamController_GetDigitalActionOrigins_proxy PROC
    jmp DWORD PTR [g_steamProcs + 364]
SteamAPI_ISteamController_GetDigitalActionOrigins_proxy ENDP
SteamAPI_ISteamController_GetGamepadIndexForController_proxy PROC
    jmp DWORD PTR [g_steamProcs + 368]
SteamAPI_ISteamController_GetGamepadIndexForController_proxy ENDP
SteamAPI_ISteamController_GetGlyphForActionOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 372]
SteamAPI_ISteamController_GetGlyphForActionOrigin_proxy ENDP
SteamAPI_ISteamController_GetGlyphForXboxOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 376]
SteamAPI_ISteamController_GetGlyphForXboxOrigin_proxy ENDP
SteamAPI_ISteamController_GetInputTypeForHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 380]
SteamAPI_ISteamController_GetInputTypeForHandle_proxy ENDP
SteamAPI_ISteamController_GetMotionData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 384]
SteamAPI_ISteamController_GetMotionData_proxy ENDP
SteamAPI_ISteamController_GetStringForActionOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 388]
SteamAPI_ISteamController_GetStringForActionOrigin_proxy ENDP
SteamAPI_ISteamController_GetStringForXboxOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 392]
SteamAPI_ISteamController_GetStringForXboxOrigin_proxy ENDP
SteamAPI_ISteamController_Init_proxy PROC
    jmp DWORD PTR [g_steamProcs + 396]
SteamAPI_ISteamController_Init_proxy ENDP
SteamAPI_ISteamController_RunFrame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 400]
SteamAPI_ISteamController_RunFrame_proxy ENDP
SteamAPI_ISteamController_SetLEDColor_proxy PROC
    jmp DWORD PTR [g_steamProcs + 404]
SteamAPI_ISteamController_SetLEDColor_proxy ENDP
SteamAPI_ISteamController_ShowBindingPanel_proxy PROC
    jmp DWORD PTR [g_steamProcs + 408]
SteamAPI_ISteamController_ShowBindingPanel_proxy ENDP
SteamAPI_ISteamController_Shutdown_proxy PROC
    jmp DWORD PTR [g_steamProcs + 412]
SteamAPI_ISteamController_Shutdown_proxy ENDP
SteamAPI_ISteamController_StopAnalogActionMomentum_proxy PROC
    jmp DWORD PTR [g_steamProcs + 416]
SteamAPI_ISteamController_StopAnalogActionMomentum_proxy ENDP
SteamAPI_ISteamController_TranslateActionOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 420]
SteamAPI_ISteamController_TranslateActionOrigin_proxy ENDP
SteamAPI_ISteamController_TriggerHapticPulse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 424]
SteamAPI_ISteamController_TriggerHapticPulse_proxy ENDP
SteamAPI_ISteamController_TriggerRepeatedHapticPulse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 428]
SteamAPI_ISteamController_TriggerRepeatedHapticPulse_proxy ENDP
SteamAPI_ISteamController_TriggerVibration_proxy PROC
    jmp DWORD PTR [g_steamProcs + 432]
SteamAPI_ISteamController_TriggerVibration_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlay_proxy PROC
    jmp DWORD PTR [g_steamProcs + 436]
SteamAPI_ISteamFriends_ActivateGameOverlay_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog_proxy PROC
    jmp DWORD PTR [g_steamProcs + 440]
SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 444]
SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog_proxy PROC
    jmp DWORD PTR [g_steamProcs + 448]
SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayToStore_proxy PROC
    jmp DWORD PTR [g_steamProcs + 452]
SteamAPI_ISteamFriends_ActivateGameOverlayToStore_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayToUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 456]
SteamAPI_ISteamFriends_ActivateGameOverlayToUser_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 460]
SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage_proxy ENDP
SteamAPI_ISteamFriends_ClearRichPresence_proxy PROC
    jmp DWORD PTR [g_steamProcs + 464]
SteamAPI_ISteamFriends_ClearRichPresence_proxy ENDP
SteamAPI_ISteamFriends_CloseClanChatWindowInSteam_proxy PROC
    jmp DWORD PTR [g_steamProcs + 468]
SteamAPI_ISteamFriends_CloseClanChatWindowInSteam_proxy ENDP
SteamAPI_ISteamFriends_DownloadClanActivityCounts_proxy PROC
    jmp DWORD PTR [g_steamProcs + 472]
SteamAPI_ISteamFriends_DownloadClanActivityCounts_proxy ENDP
SteamAPI_ISteamFriends_EnumerateFollowingList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 476]
SteamAPI_ISteamFriends_EnumerateFollowingList_proxy ENDP
SteamAPI_ISteamFriends_GetChatMemberByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 480]
SteamAPI_ISteamFriends_GetChatMemberByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetClanActivityCounts_proxy PROC
    jmp DWORD PTR [g_steamProcs + 484]
SteamAPI_ISteamFriends_GetClanActivityCounts_proxy ENDP
SteamAPI_ISteamFriends_GetClanByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 488]
SteamAPI_ISteamFriends_GetClanByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetClanChatMemberCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 492]
SteamAPI_ISteamFriends_GetClanChatMemberCount_proxy ENDP
SteamAPI_ISteamFriends_GetClanChatMessage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 496]
SteamAPI_ISteamFriends_GetClanChatMessage_proxy ENDP
SteamAPI_ISteamFriends_GetClanCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 500]
SteamAPI_ISteamFriends_GetClanCount_proxy ENDP
SteamAPI_ISteamFriends_GetClanName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 504]
SteamAPI_ISteamFriends_GetClanName_proxy ENDP
SteamAPI_ISteamFriends_GetClanOfficerByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 508]
SteamAPI_ISteamFriends_GetClanOfficerByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetClanOfficerCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 512]
SteamAPI_ISteamFriends_GetClanOfficerCount_proxy ENDP
SteamAPI_ISteamFriends_GetClanOwner_proxy PROC
    jmp DWORD PTR [g_steamProcs + 516]
SteamAPI_ISteamFriends_GetClanOwner_proxy ENDP
SteamAPI_ISteamFriends_GetClanTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 520]
SteamAPI_ISteamFriends_GetClanTag_proxy ENDP
SteamAPI_ISteamFriends_GetCoplayFriend_proxy PROC
    jmp DWORD PTR [g_steamProcs + 524]
SteamAPI_ISteamFriends_GetCoplayFriend_proxy ENDP
SteamAPI_ISteamFriends_GetCoplayFriendCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 528]
SteamAPI_ISteamFriends_GetCoplayFriendCount_proxy ENDP
SteamAPI_ISteamFriends_GetFollowerCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 532]
SteamAPI_ISteamFriends_GetFollowerCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 536]
SteamAPI_ISteamFriends_GetFriendByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetFriendCoplayGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 540]
SteamAPI_ISteamFriends_GetFriendCoplayGame_proxy ENDP
SteamAPI_ISteamFriends_GetFriendCoplayTime_proxy PROC
    jmp DWORD PTR [g_steamProcs + 544]
SteamAPI_ISteamFriends_GetFriendCoplayTime_proxy ENDP
SteamAPI_ISteamFriends_GetFriendCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 548]
SteamAPI_ISteamFriends_GetFriendCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendCountFromSource_proxy PROC
    jmp DWORD PTR [g_steamProcs + 552]
SteamAPI_ISteamFriends_GetFriendCountFromSource_proxy ENDP
SteamAPI_ISteamFriends_GetFriendFromSourceByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 556]
SteamAPI_ISteamFriends_GetFriendFromSourceByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetFriendGamePlayed_proxy PROC
    jmp DWORD PTR [g_steamProcs + 560]
SteamAPI_ISteamFriends_GetFriendGamePlayed_proxy ENDP
SteamAPI_ISteamFriends_GetFriendMessage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 564]
SteamAPI_ISteamFriends_GetFriendMessage_proxy ENDP
SteamAPI_ISteamFriends_GetFriendPersonaName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 568]
SteamAPI_ISteamFriends_GetFriendPersonaName_proxy ENDP
SteamAPI_ISteamFriends_GetFriendPersonaNameHistory_proxy PROC
    jmp DWORD PTR [g_steamProcs + 572]
SteamAPI_ISteamFriends_GetFriendPersonaNameHistory_proxy ENDP
SteamAPI_ISteamFriends_GetFriendPersonaState_proxy PROC
    jmp DWORD PTR [g_steamProcs + 576]
SteamAPI_ISteamFriends_GetFriendPersonaState_proxy ENDP
SteamAPI_ISteamFriends_GetFriendRelationship_proxy PROC
    jmp DWORD PTR [g_steamProcs + 580]
SteamAPI_ISteamFriends_GetFriendRelationship_proxy ENDP
SteamAPI_ISteamFriends_GetFriendRichPresence_proxy PROC
    jmp DWORD PTR [g_steamProcs + 584]
SteamAPI_ISteamFriends_GetFriendRichPresence_proxy ENDP
SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 588]
SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 592]
SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendSteamLevel_proxy PROC
    jmp DWORD PTR [g_steamProcs + 596]
SteamAPI_ISteamFriends_GetFriendSteamLevel_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 600]
SteamAPI_ISteamFriends_GetFriendsGroupCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 604]
SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupMembersCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 608]
SteamAPI_ISteamFriends_GetFriendsGroupMembersCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupMembersList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 612]
SteamAPI_ISteamFriends_GetFriendsGroupMembersList_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 616]
SteamAPI_ISteamFriends_GetFriendsGroupName_proxy ENDP
SteamAPI_ISteamFriends_GetLargeFriendAvatar_proxy PROC
    jmp DWORD PTR [g_steamProcs + 620]
SteamAPI_ISteamFriends_GetLargeFriendAvatar_proxy ENDP
SteamAPI_ISteamFriends_GetMediumFriendAvatar_proxy PROC
    jmp DWORD PTR [g_steamProcs + 624]
SteamAPI_ISteamFriends_GetMediumFriendAvatar_proxy ENDP
SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages_proxy PROC
    jmp DWORD PTR [g_steamProcs + 628]
SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages_proxy ENDP
SteamAPI_ISteamFriends_GetPersonaName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 632]
SteamAPI_ISteamFriends_GetPersonaName_proxy ENDP
SteamAPI_ISteamFriends_GetPersonaState_proxy PROC
    jmp DWORD PTR [g_steamProcs + 636]
SteamAPI_ISteamFriends_GetPersonaState_proxy ENDP
SteamAPI_ISteamFriends_GetPlayerNickname_proxy PROC
    jmp DWORD PTR [g_steamProcs + 640]
SteamAPI_ISteamFriends_GetPlayerNickname_proxy ENDP
SteamAPI_ISteamFriends_GetSmallFriendAvatar_proxy PROC
    jmp DWORD PTR [g_steamProcs + 644]
SteamAPI_ISteamFriends_GetSmallFriendAvatar_proxy ENDP
SteamAPI_ISteamFriends_GetUserRestrictions_proxy PROC
    jmp DWORD PTR [g_steamProcs + 648]
SteamAPI_ISteamFriends_GetUserRestrictions_proxy ENDP
SteamAPI_ISteamFriends_HasFriend_proxy PROC
    jmp DWORD PTR [g_steamProcs + 652]
SteamAPI_ISteamFriends_HasFriend_proxy ENDP
SteamAPI_ISteamFriends_InviteUserToGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 656]
SteamAPI_ISteamFriends_InviteUserToGame_proxy ENDP
SteamAPI_ISteamFriends_IsClanChatAdmin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 660]
SteamAPI_ISteamFriends_IsClanChatAdmin_proxy ENDP
SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam_proxy PROC
    jmp DWORD PTR [g_steamProcs + 664]
SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam_proxy ENDP
SteamAPI_ISteamFriends_IsClanOfficialGameGroup_proxy PROC
    jmp DWORD PTR [g_steamProcs + 668]
SteamAPI_ISteamFriends_IsClanOfficialGameGroup_proxy ENDP
SteamAPI_ISteamFriends_IsClanPublic_proxy PROC
    jmp DWORD PTR [g_steamProcs + 672]
SteamAPI_ISteamFriends_IsClanPublic_proxy ENDP
SteamAPI_ISteamFriends_IsFollowing_proxy PROC
    jmp DWORD PTR [g_steamProcs + 676]
SteamAPI_ISteamFriends_IsFollowing_proxy ENDP
SteamAPI_ISteamFriends_IsUserInSource_proxy PROC
    jmp DWORD PTR [g_steamProcs + 680]
SteamAPI_ISteamFriends_IsUserInSource_proxy ENDP
SteamAPI_ISteamFriends_JoinClanChatRoom_proxy PROC
    jmp DWORD PTR [g_steamProcs + 684]
SteamAPI_ISteamFriends_JoinClanChatRoom_proxy ENDP
SteamAPI_ISteamFriends_LeaveClanChatRoom_proxy PROC
    jmp DWORD PTR [g_steamProcs + 688]
SteamAPI_ISteamFriends_LeaveClanChatRoom_proxy ENDP
SteamAPI_ISteamFriends_OpenClanChatWindowInSteam_proxy PROC
    jmp DWORD PTR [g_steamProcs + 692]
SteamAPI_ISteamFriends_OpenClanChatWindowInSteam_proxy ENDP
SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 696]
SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser_proxy ENDP
SteamAPI_ISteamFriends_ReplyToFriendMessage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 700]
SteamAPI_ISteamFriends_ReplyToFriendMessage_proxy ENDP
SteamAPI_ISteamFriends_RequestClanOfficerList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 704]
SteamAPI_ISteamFriends_RequestClanOfficerList_proxy ENDP
SteamAPI_ISteamFriends_RequestFriendRichPresence_proxy PROC
    jmp DWORD PTR [g_steamProcs + 708]
SteamAPI_ISteamFriends_RequestFriendRichPresence_proxy ENDP
SteamAPI_ISteamFriends_RequestUserInformation_proxy PROC
    jmp DWORD PTR [g_steamProcs + 712]
SteamAPI_ISteamFriends_RequestUserInformation_proxy ENDP
SteamAPI_ISteamFriends_SendClanChatMessage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 716]
SteamAPI_ISteamFriends_SendClanChatMessage_proxy ENDP
SteamAPI_ISteamFriends_SetInGameVoiceSpeaking_proxy PROC
    jmp DWORD PTR [g_steamProcs + 720]
SteamAPI_ISteamFriends_SetInGameVoiceSpeaking_proxy ENDP
SteamAPI_ISteamFriends_SetListenForFriendsMessages_proxy PROC
    jmp DWORD PTR [g_steamProcs + 724]
SteamAPI_ISteamFriends_SetListenForFriendsMessages_proxy ENDP
SteamAPI_ISteamFriends_SetPersonaName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 728]
SteamAPI_ISteamFriends_SetPersonaName_proxy ENDP
SteamAPI_ISteamFriends_SetPlayedWith_proxy PROC
    jmp DWORD PTR [g_steamProcs + 732]
SteamAPI_ISteamFriends_SetPlayedWith_proxy ENDP
SteamAPI_ISteamFriends_SetRichPresence_proxy PROC
    jmp DWORD PTR [g_steamProcs + 736]
SteamAPI_ISteamFriends_SetRichPresence_proxy ENDP
SteamAPI_ISteamGameSearch_AcceptGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 740]
SteamAPI_ISteamGameSearch_AcceptGame_proxy ENDP
SteamAPI_ISteamGameSearch_AddGameSearchParams_proxy PROC
    jmp DWORD PTR [g_steamProcs + 744]
SteamAPI_ISteamGameSearch_AddGameSearchParams_proxy ENDP
SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 748]
SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame_proxy ENDP
SteamAPI_ISteamGameSearch_DeclineGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 752]
SteamAPI_ISteamGameSearch_DeclineGame_proxy ENDP
SteamAPI_ISteamGameSearch_EndGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 756]
SteamAPI_ISteamGameSearch_EndGame_proxy ENDP
SteamAPI_ISteamGameSearch_EndGameSearch_proxy PROC
    jmp DWORD PTR [g_steamProcs + 760]
SteamAPI_ISteamGameSearch_EndGameSearch_proxy ENDP
SteamAPI_ISteamGameSearch_HostConfirmGameStart_proxy PROC
    jmp DWORD PTR [g_steamProcs + 764]
SteamAPI_ISteamGameSearch_HostConfirmGameStart_proxy ENDP
SteamAPI_ISteamGameSearch_RequestPlayersForGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 768]
SteamAPI_ISteamGameSearch_RequestPlayersForGame_proxy ENDP
SteamAPI_ISteamGameSearch_RetrieveConnectionDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 772]
SteamAPI_ISteamGameSearch_RetrieveConnectionDetails_proxy ENDP
SteamAPI_ISteamGameSearch_SearchForGameSolo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 776]
SteamAPI_ISteamGameSearch_SearchForGameSolo_proxy ENDP
SteamAPI_ISteamGameSearch_SearchForGameWithLobby_proxy PROC
    jmp DWORD PTR [g_steamProcs + 780]
SteamAPI_ISteamGameSearch_SearchForGameWithLobby_proxy ENDP
SteamAPI_ISteamGameSearch_SetConnectionDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 784]
SteamAPI_ISteamGameSearch_SetConnectionDetails_proxy ENDP
SteamAPI_ISteamGameSearch_SetGameHostParams_proxy PROC
    jmp DWORD PTR [g_steamProcs + 788]
SteamAPI_ISteamGameSearch_SetGameHostParams_proxy ENDP
SteamAPI_ISteamGameSearch_SubmitPlayerResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 792]
SteamAPI_ISteamGameSearch_SubmitPlayerResult_proxy ENDP
SteamAPI_ISteamGameServerStats_ClearUserAchievement_proxy PROC
    jmp DWORD PTR [g_steamProcs + 796]
SteamAPI_ISteamGameServerStats_ClearUserAchievement_proxy ENDP
SteamAPI_ISteamGameServerStats_GetUserAchievement_proxy PROC
    jmp DWORD PTR [g_steamProcs + 800]
SteamAPI_ISteamGameServerStats_GetUserAchievement_proxy ENDP
SteamAPI_ISteamGameServerStats_GetUserStatFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 804]
SteamAPI_ISteamGameServerStats_GetUserStatFloat_proxy ENDP
SteamAPI_ISteamGameServerStats_GetUserStatInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 808]
SteamAPI_ISteamGameServerStats_GetUserStatInt32_proxy ENDP
SteamAPI_ISteamGameServerStats_RequestUserStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 812]
SteamAPI_ISteamGameServerStats_RequestUserStats_proxy ENDP
SteamAPI_ISteamGameServerStats_SetUserAchievement_proxy PROC
    jmp DWORD PTR [g_steamProcs + 816]
SteamAPI_ISteamGameServerStats_SetUserAchievement_proxy ENDP
SteamAPI_ISteamGameServerStats_SetUserStatFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 820]
SteamAPI_ISteamGameServerStats_SetUserStatFloat_proxy ENDP
SteamAPI_ISteamGameServerStats_SetUserStatInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 824]
SteamAPI_ISteamGameServerStats_SetUserStatInt32_proxy ENDP
SteamAPI_ISteamGameServerStats_StoreUserStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 828]
SteamAPI_ISteamGameServerStats_StoreUserStats_proxy ENDP
SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 832]
SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat_proxy ENDP
SteamAPI_ISteamGameServer_AssociateWithClan_proxy PROC
    jmp DWORD PTR [g_steamProcs + 836]
SteamAPI_ISteamGameServer_AssociateWithClan_proxy ENDP
SteamAPI_ISteamGameServer_BLoggedOn_proxy PROC
    jmp DWORD PTR [g_steamProcs + 840]
SteamAPI_ISteamGameServer_BLoggedOn_proxy ENDP
SteamAPI_ISteamGameServer_BSecure_proxy PROC
    jmp DWORD PTR [g_steamProcs + 844]
SteamAPI_ISteamGameServer_BSecure_proxy ENDP
SteamAPI_ISteamGameServer_BUpdateUserData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 848]
SteamAPI_ISteamGameServer_BUpdateUserData_proxy ENDP
SteamAPI_ISteamGameServer_BeginAuthSession_proxy PROC
    jmp DWORD PTR [g_steamProcs + 852]
SteamAPI_ISteamGameServer_BeginAuthSession_proxy ENDP
SteamAPI_ISteamGameServer_CancelAuthTicket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 856]
SteamAPI_ISteamGameServer_CancelAuthTicket_proxy ENDP
SteamAPI_ISteamGameServer_ClearAllKeyValues_proxy PROC
    jmp DWORD PTR [g_steamProcs + 860]
SteamAPI_ISteamGameServer_ClearAllKeyValues_proxy ENDP
SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility_proxy PROC
    jmp DWORD PTR [g_steamProcs + 864]
SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility_proxy ENDP
SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 868]
SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection_proxy ENDP
SteamAPI_ISteamGameServer_EndAuthSession_proxy PROC
    jmp DWORD PTR [g_steamProcs + 872]
SteamAPI_ISteamGameServer_EndAuthSession_proxy ENDP
SteamAPI_ISteamGameServer_GetAuthSessionTicket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 876]
SteamAPI_ISteamGameServer_GetAuthSessionTicket_proxy ENDP
SteamAPI_ISteamGameServer_GetGameplayStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 880]
SteamAPI_ISteamGameServer_GetGameplayStats_proxy ENDP
SteamAPI_ISteamGameServer_GetNextOutgoingPacket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 884]
SteamAPI_ISteamGameServer_GetNextOutgoingPacket_proxy ENDP
SteamAPI_ISteamGameServer_GetPublicIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 888]
SteamAPI_ISteamGameServer_GetPublicIP_proxy ENDP
SteamAPI_ISteamGameServer_GetServerReputation_proxy PROC
    jmp DWORD PTR [g_steamProcs + 892]
SteamAPI_ISteamGameServer_GetServerReputation_proxy ENDP
SteamAPI_ISteamGameServer_GetSteamID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 896]
SteamAPI_ISteamGameServer_GetSteamID_proxy ENDP
SteamAPI_ISteamGameServer_HandleIncomingPacket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 900]
SteamAPI_ISteamGameServer_HandleIncomingPacket_proxy ENDP
SteamAPI_ISteamGameServer_LogOff_proxy PROC
    jmp DWORD PTR [g_steamProcs + 904]
SteamAPI_ISteamGameServer_LogOff_proxy ENDP
SteamAPI_ISteamGameServer_LogOn_proxy PROC
    jmp DWORD PTR [g_steamProcs + 908]
SteamAPI_ISteamGameServer_LogOn_proxy ENDP
SteamAPI_ISteamGameServer_LogOnAnonymous_proxy PROC
    jmp DWORD PTR [g_steamProcs + 912]
SteamAPI_ISteamGameServer_LogOnAnonymous_proxy ENDP
SteamAPI_ISteamGameServer_RequestUserGroupStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 916]
SteamAPI_ISteamGameServer_RequestUserGroupStatus_proxy ENDP
SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED_proxy PROC
    jmp DWORD PTR [g_steamProcs + 920]
SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED_proxy ENDP
SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED_proxy PROC
    jmp DWORD PTR [g_steamProcs + 924]
SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED_proxy ENDP
SteamAPI_ISteamGameServer_SetAdvertiseServerActive_proxy PROC
    jmp DWORD PTR [g_steamProcs + 928]
SteamAPI_ISteamGameServer_SetAdvertiseServerActive_proxy ENDP
SteamAPI_ISteamGameServer_SetBotPlayerCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 932]
SteamAPI_ISteamGameServer_SetBotPlayerCount_proxy ENDP
SteamAPI_ISteamGameServer_SetDedicatedServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 936]
SteamAPI_ISteamGameServer_SetDedicatedServer_proxy ENDP
SteamAPI_ISteamGameServer_SetGameData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 940]
SteamAPI_ISteamGameServer_SetGameData_proxy ENDP
SteamAPI_ISteamGameServer_SetGameDescription_proxy PROC
    jmp DWORD PTR [g_steamProcs + 944]
SteamAPI_ISteamGameServer_SetGameDescription_proxy ENDP
SteamAPI_ISteamGameServer_SetGameTags_proxy PROC
    jmp DWORD PTR [g_steamProcs + 948]
SteamAPI_ISteamGameServer_SetGameTags_proxy ENDP
SteamAPI_ISteamGameServer_SetKeyValue_proxy PROC
    jmp DWORD PTR [g_steamProcs + 952]
SteamAPI_ISteamGameServer_SetKeyValue_proxy ENDP
SteamAPI_ISteamGameServer_SetMapName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 956]
SteamAPI_ISteamGameServer_SetMapName_proxy ENDP
SteamAPI_ISteamGameServer_SetMaxPlayerCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 960]
SteamAPI_ISteamGameServer_SetMaxPlayerCount_proxy ENDP
SteamAPI_ISteamGameServer_SetModDir_proxy PROC
    jmp DWORD PTR [g_steamProcs + 964]
SteamAPI_ISteamGameServer_SetModDir_proxy ENDP
SteamAPI_ISteamGameServer_SetPasswordProtected_proxy PROC
    jmp DWORD PTR [g_steamProcs + 968]
SteamAPI_ISteamGameServer_SetPasswordProtected_proxy ENDP
SteamAPI_ISteamGameServer_SetProduct_proxy PROC
    jmp DWORD PTR [g_steamProcs + 972]
SteamAPI_ISteamGameServer_SetProduct_proxy ENDP
SteamAPI_ISteamGameServer_SetRegion_proxy PROC
    jmp DWORD PTR [g_steamProcs + 976]
SteamAPI_ISteamGameServer_SetRegion_proxy ENDP
SteamAPI_ISteamGameServer_SetServerName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 980]
SteamAPI_ISteamGameServer_SetServerName_proxy ENDP
SteamAPI_ISteamGameServer_SetSpectatorPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 984]
SteamAPI_ISteamGameServer_SetSpectatorPort_proxy ENDP
SteamAPI_ISteamGameServer_SetSpectatorServerName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 988]
SteamAPI_ISteamGameServer_SetSpectatorServerName_proxy ENDP
SteamAPI_ISteamGameServer_UserHasLicenseForApp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 992]
SteamAPI_ISteamGameServer_UserHasLicenseForApp_proxy ENDP
SteamAPI_ISteamGameServer_WasRestartRequested_proxy PROC
    jmp DWORD PTR [g_steamProcs + 996]
SteamAPI_ISteamGameServer_WasRestartRequested_proxy ENDP
SteamAPI_ISteamHTMLSurface_AddHeader_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1000]
SteamAPI_ISteamHTMLSurface_AddHeader_proxy ENDP
SteamAPI_ISteamHTMLSurface_AllowStartRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1004]
SteamAPI_ISteamHTMLSurface_AllowStartRequest_proxy ENDP
SteamAPI_ISteamHTMLSurface_CopyToClipboard_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1008]
SteamAPI_ISteamHTMLSurface_CopyToClipboard_proxy ENDP
SteamAPI_ISteamHTMLSurface_CreateBrowser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1012]
SteamAPI_ISteamHTMLSurface_CreateBrowser_proxy ENDP
SteamAPI_ISteamHTMLSurface_ExecuteJavascript_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1016]
SteamAPI_ISteamHTMLSurface_ExecuteJavascript_proxy ENDP
SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1020]
SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse_proxy ENDP
SteamAPI_ISteamHTMLSurface_Find_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1024]
SteamAPI_ISteamHTMLSurface_Find_proxy ENDP
SteamAPI_ISteamHTMLSurface_GetLinkAtPosition_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1028]
SteamAPI_ISteamHTMLSurface_GetLinkAtPosition_proxy ENDP
SteamAPI_ISteamHTMLSurface_GoBack_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1032]
SteamAPI_ISteamHTMLSurface_GoBack_proxy ENDP
SteamAPI_ISteamHTMLSurface_GoForward_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1036]
SteamAPI_ISteamHTMLSurface_GoForward_proxy ENDP
SteamAPI_ISteamHTMLSurface_Init_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1040]
SteamAPI_ISteamHTMLSurface_Init_proxy ENDP
SteamAPI_ISteamHTMLSurface_JSDialogResponse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1044]
SteamAPI_ISteamHTMLSurface_JSDialogResponse_proxy ENDP
SteamAPI_ISteamHTMLSurface_KeyChar_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1048]
SteamAPI_ISteamHTMLSurface_KeyChar_proxy ENDP
SteamAPI_ISteamHTMLSurface_KeyDown_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1052]
SteamAPI_ISteamHTMLSurface_KeyDown_proxy ENDP
SteamAPI_ISteamHTMLSurface_KeyUp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1056]
SteamAPI_ISteamHTMLSurface_KeyUp_proxy ENDP
SteamAPI_ISteamHTMLSurface_LoadURL_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1060]
SteamAPI_ISteamHTMLSurface_LoadURL_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseDoubleClick_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1064]
SteamAPI_ISteamHTMLSurface_MouseDoubleClick_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseDown_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1068]
SteamAPI_ISteamHTMLSurface_MouseDown_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseMove_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1072]
SteamAPI_ISteamHTMLSurface_MouseMove_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseUp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1076]
SteamAPI_ISteamHTMLSurface_MouseUp_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseWheel_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1080]
SteamAPI_ISteamHTMLSurface_MouseWheel_proxy ENDP
SteamAPI_ISteamHTMLSurface_OpenDeveloperTools_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1084]
SteamAPI_ISteamHTMLSurface_OpenDeveloperTools_proxy ENDP
SteamAPI_ISteamHTMLSurface_PasteFromClipboard_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1088]
SteamAPI_ISteamHTMLSurface_PasteFromClipboard_proxy ENDP
SteamAPI_ISteamHTMLSurface_Reload_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1092]
SteamAPI_ISteamHTMLSurface_Reload_proxy ENDP
SteamAPI_ISteamHTMLSurface_RemoveBrowser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1096]
SteamAPI_ISteamHTMLSurface_RemoveBrowser_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetBackgroundMode_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1100]
SteamAPI_ISteamHTMLSurface_SetBackgroundMode_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetCookie_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1104]
SteamAPI_ISteamHTMLSurface_SetCookie_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1108]
SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetHorizontalScroll_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1112]
SteamAPI_ISteamHTMLSurface_SetHorizontalScroll_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetKeyFocus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1116]
SteamAPI_ISteamHTMLSurface_SetKeyFocus_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetPageScaleFactor_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1120]
SteamAPI_ISteamHTMLSurface_SetPageScaleFactor_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetSize_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1124]
SteamAPI_ISteamHTMLSurface_SetSize_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetVerticalScroll_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1128]
SteamAPI_ISteamHTMLSurface_SetVerticalScroll_proxy ENDP
SteamAPI_ISteamHTMLSurface_Shutdown_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1132]
SteamAPI_ISteamHTMLSurface_Shutdown_proxy ENDP
SteamAPI_ISteamHTMLSurface_StopFind_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1136]
SteamAPI_ISteamHTMLSurface_StopFind_proxy ENDP
SteamAPI_ISteamHTMLSurface_StopLoad_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1140]
SteamAPI_ISteamHTMLSurface_StopLoad_proxy ENDP
SteamAPI_ISteamHTMLSurface_ViewSource_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1144]
SteamAPI_ISteamHTMLSurface_ViewSource_proxy ENDP
SteamAPI_ISteamHTTP_CreateCookieContainer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1148]
SteamAPI_ISteamHTTP_CreateCookieContainer_proxy ENDP
SteamAPI_ISteamHTTP_CreateHTTPRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1152]
SteamAPI_ISteamHTTP_CreateHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_DeferHTTPRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1156]
SteamAPI_ISteamHTTP_DeferHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1160]
SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1164]
SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPResponseBodyData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1168]
SteamAPI_ISteamHTTP_GetHTTPResponseBodyData_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPResponseBodySize_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1172]
SteamAPI_ISteamHTTP_GetHTTPResponseBodySize_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1176]
SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1180]
SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1184]
SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData_proxy ENDP
SteamAPI_ISteamHTTP_PrioritizeHTTPRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1188]
SteamAPI_ISteamHTTP_PrioritizeHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_ReleaseCookieContainer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1192]
SteamAPI_ISteamHTTP_ReleaseCookieContainer_proxy ENDP
SteamAPI_ISteamHTTP_ReleaseHTTPRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1196]
SteamAPI_ISteamHTTP_ReleaseHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_SendHTTPRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1200]
SteamAPI_ISteamHTTP_SendHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1204]
SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse_proxy ENDP
SteamAPI_ISteamHTTP_SetCookie_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1208]
SteamAPI_ISteamHTTP_SetCookie_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1212]
SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestContextValue_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1216]
SteamAPI_ISteamHTTP_SetHTTPRequestContextValue_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1220]
SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1224]
SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1228]
SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1232]
SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1236]
SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1240]
SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1244]
SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo_proxy ENDP
SteamAPI_ISteamInput_ActivateActionSet_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1248]
SteamAPI_ISteamInput_ActivateActionSet_proxy ENDP
SteamAPI_ISteamInput_ActivateActionSetLayer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1252]
SteamAPI_ISteamInput_ActivateActionSetLayer_proxy ENDP
SteamAPI_ISteamInput_BNewDataAvailable_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1256]
SteamAPI_ISteamInput_BNewDataAvailable_proxy ENDP
SteamAPI_ISteamInput_BWaitForData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1260]
SteamAPI_ISteamInput_BWaitForData_proxy ENDP
SteamAPI_ISteamInput_DeactivateActionSetLayer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1264]
SteamAPI_ISteamInput_DeactivateActionSetLayer_proxy ENDP
SteamAPI_ISteamInput_DeactivateAllActionSetLayers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1268]
SteamAPI_ISteamInput_DeactivateAllActionSetLayers_proxy ENDP
SteamAPI_ISteamInput_EnableActionEventCallbacks_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1272]
SteamAPI_ISteamInput_EnableActionEventCallbacks_proxy ENDP
SteamAPI_ISteamInput_EnableDeviceCallbacks_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1276]
SteamAPI_ISteamInput_EnableDeviceCallbacks_proxy ENDP
SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1280]
SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin_proxy ENDP
SteamAPI_ISteamInput_GetActionSetHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1284]
SteamAPI_ISteamInput_GetActionSetHandle_proxy ENDP
SteamAPI_ISteamInput_GetActiveActionSetLayers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1288]
SteamAPI_ISteamInput_GetActiveActionSetLayers_proxy ENDP
SteamAPI_ISteamInput_GetAnalogActionData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1292]
SteamAPI_ISteamInput_GetAnalogActionData_proxy ENDP
SteamAPI_ISteamInput_GetAnalogActionHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1296]
SteamAPI_ISteamInput_GetAnalogActionHandle_proxy ENDP
SteamAPI_ISteamInput_GetAnalogActionOrigins_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1300]
SteamAPI_ISteamInput_GetAnalogActionOrigins_proxy ENDP
SteamAPI_ISteamInput_GetConnectedControllers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1304]
SteamAPI_ISteamInput_GetConnectedControllers_proxy ENDP
SteamAPI_ISteamInput_GetControllerForGamepadIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1308]
SteamAPI_ISteamInput_GetControllerForGamepadIndex_proxy ENDP
SteamAPI_ISteamInput_GetCurrentActionSet_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1312]
SteamAPI_ISteamInput_GetCurrentActionSet_proxy ENDP
SteamAPI_ISteamInput_GetDeviceBindingRevision_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1316]
SteamAPI_ISteamInput_GetDeviceBindingRevision_proxy ENDP
SteamAPI_ISteamInput_GetDigitalActionData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1320]
SteamAPI_ISteamInput_GetDigitalActionData_proxy ENDP
SteamAPI_ISteamInput_GetDigitalActionHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1324]
SteamAPI_ISteamInput_GetDigitalActionHandle_proxy ENDP
SteamAPI_ISteamInput_GetDigitalActionOrigins_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1328]
SteamAPI_ISteamInput_GetDigitalActionOrigins_proxy ENDP
SteamAPI_ISteamInput_GetGamepadIndexForController_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1332]
SteamAPI_ISteamInput_GetGamepadIndexForController_proxy ENDP
SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1336]
SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy_proxy ENDP
SteamAPI_ISteamInput_GetGlyphForXboxOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1340]
SteamAPI_ISteamInput_GetGlyphForXboxOrigin_proxy ENDP
SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1344]
SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin_proxy ENDP
SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1348]
SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin_proxy ENDP
SteamAPI_ISteamInput_GetInputTypeForHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1352]
SteamAPI_ISteamInput_GetInputTypeForHandle_proxy ENDP
SteamAPI_ISteamInput_GetMotionData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1356]
SteamAPI_ISteamInput_GetMotionData_proxy ENDP
SteamAPI_ISteamInput_GetRemotePlaySessionID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1360]
SteamAPI_ISteamInput_GetRemotePlaySessionID_proxy ENDP
SteamAPI_ISteamInput_GetSessionInputConfigurationSettings_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1364]
SteamAPI_ISteamInput_GetSessionInputConfigurationSettings_proxy ENDP
SteamAPI_ISteamInput_GetStringForActionOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1368]
SteamAPI_ISteamInput_GetStringForActionOrigin_proxy ENDP
SteamAPI_ISteamInput_GetStringForAnalogActionName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1372]
SteamAPI_ISteamInput_GetStringForAnalogActionName_proxy ENDP
SteamAPI_ISteamInput_GetStringForDigitalActionName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1376]
SteamAPI_ISteamInput_GetStringForDigitalActionName_proxy ENDP
SteamAPI_ISteamInput_GetStringForXboxOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1380]
SteamAPI_ISteamInput_GetStringForXboxOrigin_proxy ENDP
SteamAPI_ISteamInput_Init_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1384]
SteamAPI_ISteamInput_Init_proxy ENDP
SteamAPI_ISteamInput_Legacy_TriggerHapticPulse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1388]
SteamAPI_ISteamInput_Legacy_TriggerHapticPulse_proxy ENDP
SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1392]
SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse_proxy ENDP
SteamAPI_ISteamInput_RunFrame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1396]
SteamAPI_ISteamInput_RunFrame_proxy ENDP
SteamAPI_ISteamInput_SetInputActionManifestFilePath_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1400]
SteamAPI_ISteamInput_SetInputActionManifestFilePath_proxy ENDP
SteamAPI_ISteamInput_SetLEDColor_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1404]
SteamAPI_ISteamInput_SetLEDColor_proxy ENDP
SteamAPI_ISteamInput_ShowBindingPanel_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1408]
SteamAPI_ISteamInput_ShowBindingPanel_proxy ENDP
SteamAPI_ISteamInput_Shutdown_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1412]
SteamAPI_ISteamInput_Shutdown_proxy ENDP
SteamAPI_ISteamInput_StopAnalogActionMomentum_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1416]
SteamAPI_ISteamInput_StopAnalogActionMomentum_proxy ENDP
SteamAPI_ISteamInput_TranslateActionOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1420]
SteamAPI_ISteamInput_TranslateActionOrigin_proxy ENDP
SteamAPI_ISteamInput_TriggerSimpleHapticEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1424]
SteamAPI_ISteamInput_TriggerSimpleHapticEvent_proxy ENDP
SteamAPI_ISteamInput_TriggerVibration_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1428]
SteamAPI_ISteamInput_TriggerVibration_proxy ENDP
SteamAPI_ISteamInput_TriggerVibrationExtended_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1432]
SteamAPI_ISteamInput_TriggerVibrationExtended_proxy ENDP
SteamAPI_ISteamInventory_AddPromoItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1436]
SteamAPI_ISteamInventory_AddPromoItem_proxy ENDP
SteamAPI_ISteamInventory_AddPromoItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1440]
SteamAPI_ISteamInventory_AddPromoItems_proxy ENDP
SteamAPI_ISteamInventory_CheckResultSteamID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1444]
SteamAPI_ISteamInventory_CheckResultSteamID_proxy ENDP
SteamAPI_ISteamInventory_ConsumeItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1448]
SteamAPI_ISteamInventory_ConsumeItem_proxy ENDP
SteamAPI_ISteamInventory_DeserializeResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1452]
SteamAPI_ISteamInventory_DeserializeResult_proxy ENDP
SteamAPI_ISteamInventory_DestroyResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1456]
SteamAPI_ISteamInventory_DestroyResult_proxy ENDP
SteamAPI_ISteamInventory_ExchangeItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1460]
SteamAPI_ISteamInventory_ExchangeItems_proxy ENDP
SteamAPI_ISteamInventory_GenerateItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1464]
SteamAPI_ISteamInventory_GenerateItems_proxy ENDP
SteamAPI_ISteamInventory_GetAllItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1468]
SteamAPI_ISteamInventory_GetAllItems_proxy ENDP
SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1472]
SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs_proxy ENDP
SteamAPI_ISteamInventory_GetItemDefinitionIDs_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1476]
SteamAPI_ISteamInventory_GetItemDefinitionIDs_proxy ENDP
SteamAPI_ISteamInventory_GetItemDefinitionProperty_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1480]
SteamAPI_ISteamInventory_GetItemDefinitionProperty_proxy ENDP
SteamAPI_ISteamInventory_GetItemPrice_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1484]
SteamAPI_ISteamInventory_GetItemPrice_proxy ENDP
SteamAPI_ISteamInventory_GetItemsByID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1488]
SteamAPI_ISteamInventory_GetItemsByID_proxy ENDP
SteamAPI_ISteamInventory_GetItemsWithPrices_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1492]
SteamAPI_ISteamInventory_GetItemsWithPrices_proxy ENDP
SteamAPI_ISteamInventory_GetNumItemsWithPrices_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1496]
SteamAPI_ISteamInventory_GetNumItemsWithPrices_proxy ENDP
SteamAPI_ISteamInventory_GetResultItemProperty_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1500]
SteamAPI_ISteamInventory_GetResultItemProperty_proxy ENDP
SteamAPI_ISteamInventory_GetResultItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1504]
SteamAPI_ISteamInventory_GetResultItems_proxy ENDP
SteamAPI_ISteamInventory_GetResultStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1508]
SteamAPI_ISteamInventory_GetResultStatus_proxy ENDP
SteamAPI_ISteamInventory_GetResultTimestamp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1512]
SteamAPI_ISteamInventory_GetResultTimestamp_proxy ENDP
SteamAPI_ISteamInventory_GrantPromoItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1516]
SteamAPI_ISteamInventory_GrantPromoItems_proxy ENDP
SteamAPI_ISteamInventory_InspectItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1520]
SteamAPI_ISteamInventory_InspectItem_proxy ENDP
SteamAPI_ISteamInventory_LoadItemDefinitions_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1524]
SteamAPI_ISteamInventory_LoadItemDefinitions_proxy ENDP
SteamAPI_ISteamInventory_RemoveProperty_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1528]
SteamAPI_ISteamInventory_RemoveProperty_proxy ENDP
SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1532]
SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs_proxy ENDP
SteamAPI_ISteamInventory_RequestPrices_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1536]
SteamAPI_ISteamInventory_RequestPrices_proxy ENDP
SteamAPI_ISteamInventory_SendItemDropHeartbeat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1540]
SteamAPI_ISteamInventory_SendItemDropHeartbeat_proxy ENDP
SteamAPI_ISteamInventory_SerializeResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1544]
SteamAPI_ISteamInventory_SerializeResult_proxy ENDP
SteamAPI_ISteamInventory_SetPropertyBool_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1548]
SteamAPI_ISteamInventory_SetPropertyBool_proxy ENDP
SteamAPI_ISteamInventory_SetPropertyFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1552]
SteamAPI_ISteamInventory_SetPropertyFloat_proxy ENDP
SteamAPI_ISteamInventory_SetPropertyInt64_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1556]
SteamAPI_ISteamInventory_SetPropertyInt64_proxy ENDP
SteamAPI_ISteamInventory_SetPropertyString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1560]
SteamAPI_ISteamInventory_SetPropertyString_proxy ENDP
SteamAPI_ISteamInventory_StartPurchase_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1564]
SteamAPI_ISteamInventory_StartPurchase_proxy ENDP
SteamAPI_ISteamInventory_StartUpdateProperties_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1568]
SteamAPI_ISteamInventory_StartUpdateProperties_proxy ENDP
SteamAPI_ISteamInventory_SubmitUpdateProperties_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1572]
SteamAPI_ISteamInventory_SubmitUpdateProperties_proxy ENDP
SteamAPI_ISteamInventory_TradeItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1576]
SteamAPI_ISteamInventory_TradeItems_proxy ENDP
SteamAPI_ISteamInventory_TransferItemQuantity_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1580]
SteamAPI_ISteamInventory_TransferItemQuantity_proxy ENDP
SteamAPI_ISteamInventory_TriggerItemDrop_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1584]
SteamAPI_ISteamInventory_TriggerItemDrop_proxy ENDP
SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1588]
SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond_proxy ENDP
SteamAPI_ISteamMatchmakingPingResponse_ServerResponded_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1592]
SteamAPI_ISteamMatchmakingPingResponse_ServerResponded_proxy ENDP
SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1596]
SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList_proxy ENDP
SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1600]
SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond_proxy ENDP
SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1604]
SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete_proxy ENDP
SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1608]
SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond_proxy ENDP
SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1612]
SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete_proxy ENDP
SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1616]
SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded_proxy ENDP
SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1620]
SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete_proxy ENDP
SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1624]
SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond_proxy ENDP
SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1628]
SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded_proxy ENDP
SteamAPI_ISteamMatchmakingServers_CancelQuery_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1632]
SteamAPI_ISteamMatchmakingServers_CancelQuery_proxy ENDP
SteamAPI_ISteamMatchmakingServers_CancelServerQuery_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1636]
SteamAPI_ISteamMatchmakingServers_CancelServerQuery_proxy ENDP
SteamAPI_ISteamMatchmakingServers_GetServerCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1640]
SteamAPI_ISteamMatchmakingServers_GetServerCount_proxy ENDP
SteamAPI_ISteamMatchmakingServers_GetServerDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1644]
SteamAPI_ISteamMatchmakingServers_GetServerDetails_proxy ENDP
SteamAPI_ISteamMatchmakingServers_IsRefreshing_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1648]
SteamAPI_ISteamMatchmakingServers_IsRefreshing_proxy ENDP
SteamAPI_ISteamMatchmakingServers_PingServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1652]
SteamAPI_ISteamMatchmakingServers_PingServer_proxy ENDP
SteamAPI_ISteamMatchmakingServers_PlayerDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1656]
SteamAPI_ISteamMatchmakingServers_PlayerDetails_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RefreshQuery_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1660]
SteamAPI_ISteamMatchmakingServers_RefreshQuery_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RefreshServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1664]
SteamAPI_ISteamMatchmakingServers_RefreshServer_proxy ENDP
SteamAPI_ISteamMatchmakingServers_ReleaseRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1668]
SteamAPI_ISteamMatchmakingServers_ReleaseRequest_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1672]
SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1676]
SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1680]
SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestInternetServerList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1684]
SteamAPI_ISteamMatchmakingServers_RequestInternetServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestLANServerList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1688]
SteamAPI_ISteamMatchmakingServers_RequestLANServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1692]
SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_ServerRules_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1696]
SteamAPI_ISteamMatchmakingServers_ServerRules_proxy ENDP
SteamAPI_ISteamMatchmaking_AddFavoriteGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1700]
SteamAPI_ISteamMatchmaking_AddFavoriteGame_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1704]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1708]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1712]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1716]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1720]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1724]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1728]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_CreateLobby_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1732]
SteamAPI_ISteamMatchmaking_CreateLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_DeleteLobbyData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1736]
SteamAPI_ISteamMatchmaking_DeleteLobbyData_proxy ENDP
SteamAPI_ISteamMatchmaking_GetFavoriteGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1740]
SteamAPI_ISteamMatchmaking_GetFavoriteGame_proxy ENDP
SteamAPI_ISteamMatchmaking_GetFavoriteGameCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1744]
SteamAPI_ISteamMatchmaking_GetFavoriteGameCount_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1748]
SteamAPI_ISteamMatchmaking_GetLobbyByIndex_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyChatEntry_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1752]
SteamAPI_ISteamMatchmaking_GetLobbyChatEntry_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1756]
SteamAPI_ISteamMatchmaking_GetLobbyData_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1760]
SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyDataCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1764]
SteamAPI_ISteamMatchmaking_GetLobbyDataCount_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyGameServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1768]
SteamAPI_ISteamMatchmaking_GetLobbyGameServer_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1772]
SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyMemberData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1776]
SteamAPI_ISteamMatchmaking_GetLobbyMemberData_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1780]
SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyOwner_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1784]
SteamAPI_ISteamMatchmaking_GetLobbyOwner_proxy ENDP
SteamAPI_ISteamMatchmaking_GetNumLobbyMembers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1788]
SteamAPI_ISteamMatchmaking_GetNumLobbyMembers_proxy ENDP
SteamAPI_ISteamMatchmaking_InviteUserToLobby_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1792]
SteamAPI_ISteamMatchmaking_InviteUserToLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_JoinLobby_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1796]
SteamAPI_ISteamMatchmaking_JoinLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_LeaveLobby_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1800]
SteamAPI_ISteamMatchmaking_LeaveLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_RemoveFavoriteGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1804]
SteamAPI_ISteamMatchmaking_RemoveFavoriteGame_proxy ENDP
SteamAPI_ISteamMatchmaking_RequestLobbyData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1808]
SteamAPI_ISteamMatchmaking_RequestLobbyData_proxy ENDP
SteamAPI_ISteamMatchmaking_RequestLobbyList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1812]
SteamAPI_ISteamMatchmaking_RequestLobbyList_proxy ENDP
SteamAPI_ISteamMatchmaking_SendLobbyChatMsg_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1816]
SteamAPI_ISteamMatchmaking_SendLobbyChatMsg_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLinkedLobby_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1820]
SteamAPI_ISteamMatchmaking_SetLinkedLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1824]
SteamAPI_ISteamMatchmaking_SetLobbyData_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyGameServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1828]
SteamAPI_ISteamMatchmaking_SetLobbyGameServer_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyJoinable_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1832]
SteamAPI_ISteamMatchmaking_SetLobbyJoinable_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyMemberData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1836]
SteamAPI_ISteamMatchmaking_SetLobbyMemberData_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1840]
SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyOwner_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1844]
SteamAPI_ISteamMatchmaking_SetLobbyOwner_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyType_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1848]
SteamAPI_ISteamMatchmaking_SetLobbyType_proxy ENDP
SteamAPI_ISteamMusicRemote_BActivationSuccess_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1852]
SteamAPI_ISteamMusicRemote_BActivationSuccess_proxy ENDP
SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1856]
SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote_proxy ENDP
SteamAPI_ISteamMusicRemote_CurrentEntryDidChange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1860]
SteamAPI_ISteamMusicRemote_CurrentEntryDidChange_proxy ENDP
SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1864]
SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable_proxy ENDP
SteamAPI_ISteamMusicRemote_CurrentEntryWillChange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1868]
SteamAPI_ISteamMusicRemote_CurrentEntryWillChange_proxy ENDP
SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1872]
SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote_proxy ENDP
SteamAPI_ISteamMusicRemote_EnableLooped_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1876]
SteamAPI_ISteamMusicRemote_EnableLooped_proxy ENDP
SteamAPI_ISteamMusicRemote_EnablePlayNext_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1880]
SteamAPI_ISteamMusicRemote_EnablePlayNext_proxy ENDP
SteamAPI_ISteamMusicRemote_EnablePlayPrevious_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1884]
SteamAPI_ISteamMusicRemote_EnablePlayPrevious_proxy ENDP
SteamAPI_ISteamMusicRemote_EnablePlaylists_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1888]
SteamAPI_ISteamMusicRemote_EnablePlaylists_proxy ENDP
SteamAPI_ISteamMusicRemote_EnableQueue_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1892]
SteamAPI_ISteamMusicRemote_EnableQueue_proxy ENDP
SteamAPI_ISteamMusicRemote_EnableShuffled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1896]
SteamAPI_ISteamMusicRemote_EnableShuffled_proxy ENDP
SteamAPI_ISteamMusicRemote_PlaylistDidChange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1900]
SteamAPI_ISteamMusicRemote_PlaylistDidChange_proxy ENDP
SteamAPI_ISteamMusicRemote_PlaylistWillChange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1904]
SteamAPI_ISteamMusicRemote_PlaylistWillChange_proxy ENDP
SteamAPI_ISteamMusicRemote_QueueDidChange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1908]
SteamAPI_ISteamMusicRemote_QueueDidChange_proxy ENDP
SteamAPI_ISteamMusicRemote_QueueWillChange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1912]
SteamAPI_ISteamMusicRemote_QueueWillChange_proxy ENDP
SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1916]
SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote_proxy ENDP
SteamAPI_ISteamMusicRemote_ResetPlaylistEntries_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1920]
SteamAPI_ISteamMusicRemote_ResetPlaylistEntries_proxy ENDP
SteamAPI_ISteamMusicRemote_ResetQueueEntries_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1924]
SteamAPI_ISteamMusicRemote_ResetQueueEntries_proxy ENDP
SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1928]
SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry_proxy ENDP
SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1932]
SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry_proxy ENDP
SteamAPI_ISteamMusicRemote_SetDisplayName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1936]
SteamAPI_ISteamMusicRemote_SetDisplayName_proxy ENDP
SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1940]
SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64_proxy ENDP
SteamAPI_ISteamMusicRemote_SetPlaylistEntry_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1944]
SteamAPI_ISteamMusicRemote_SetPlaylistEntry_proxy ENDP
SteamAPI_ISteamMusicRemote_SetQueueEntry_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1948]
SteamAPI_ISteamMusicRemote_SetQueueEntry_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1952]
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1956]
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1960]
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateLooped_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1964]
SteamAPI_ISteamMusicRemote_UpdateLooped_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1968]
SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateShuffled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1972]
SteamAPI_ISteamMusicRemote_UpdateShuffled_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateVolume_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1976]
SteamAPI_ISteamMusicRemote_UpdateVolume_proxy ENDP
SteamAPI_ISteamMusic_BIsEnabled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1980]
SteamAPI_ISteamMusic_BIsEnabled_proxy ENDP
SteamAPI_ISteamMusic_BIsPlaying_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1984]
SteamAPI_ISteamMusic_BIsPlaying_proxy ENDP
SteamAPI_ISteamMusic_GetPlaybackStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1988]
SteamAPI_ISteamMusic_GetPlaybackStatus_proxy ENDP
SteamAPI_ISteamMusic_GetVolume_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1992]
SteamAPI_ISteamMusic_GetVolume_proxy ENDP
SteamAPI_ISteamMusic_Pause_proxy PROC
    jmp DWORD PTR [g_steamProcs + 1996]
SteamAPI_ISteamMusic_Pause_proxy ENDP
SteamAPI_ISteamMusic_Play_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2000]
SteamAPI_ISteamMusic_Play_proxy ENDP
SteamAPI_ISteamMusic_PlayNext_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2004]
SteamAPI_ISteamMusic_PlayNext_proxy ENDP
SteamAPI_ISteamMusic_PlayPrevious_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2008]
SteamAPI_ISteamMusic_PlayPrevious_proxy ENDP
SteamAPI_ISteamMusic_SetVolume_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2012]
SteamAPI_ISteamMusic_SetVolume_proxy ENDP
SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2016]
SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort_proxy ENDP
SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2020]
SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages_proxy ENDP
SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2024]
SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup_proxy ENDP
SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2028]
SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2032]
SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser_proxy ENDP
SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2036]
SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser_proxy ENDP
SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2040]
SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser_proxy ENDP
SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2044]
SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo_proxy ENDP
SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2048]
SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel_proxy ENDP
SteamAPI_ISteamNetworkingMessages_SendMessageToUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2052]
SteamAPI_ISteamNetworkingMessages_SendMessageToUser_proxy ENDP
SteamAPI_ISteamNetworkingSockets_AcceptConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2056]
SteamAPI_ISteamNetworkingSockets_AcceptConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2060]
SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CloseConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2064]
SteamAPI_ISteamNetworkingSockets_CloseConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CloseListenSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2068]
SteamAPI_ISteamNetworkingSockets_CloseListenSocket_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2072]
SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2076]
SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConnectP2P_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2080]
SteamAPI_ISteamNetworkingSockets_ConnectP2P_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2084]
SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2088]
SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2092]
SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2096]
SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2100]
SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2104]
SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2108]
SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreatePollGroup_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2112]
SteamAPI_ISteamNetworkingSockets_CreatePollGroup_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateSocketPair_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2116]
SteamAPI_ISteamNetworkingSockets_CreateSocketPair_proxy ENDP
SteamAPI_ISteamNetworkingSockets_DestroyPollGroup_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2120]
SteamAPI_ISteamNetworkingSockets_DestroyPollGroup_proxy ENDP
SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2124]
SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer_proxy ENDP
SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2128]
SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2132]
SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetCertificateRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2136]
SteamAPI_ISteamNetworkingSockets_GetCertificateRequest_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetConnectionInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2140]
SteamAPI_ISteamNetworkingSockets_GetConnectionInfo_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetConnectionName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2144]
SteamAPI_ISteamNetworkingSockets_GetConnectionName_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2148]
SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetConnectionUserData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2152]
SteamAPI_ISteamNetworkingSockets_GetConnectionUserData_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2156]
SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetFakeIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2160]
SteamAPI_ISteamNetworkingSockets_GetFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2164]
SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2168]
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2172]
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2176]
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetIdentity_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2180]
SteamAPI_ISteamNetworkingSockets_GetIdentity_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2184]
SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2188]
SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_InitAuthentication_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2192]
SteamAPI_ISteamNetworkingSockets_InitAuthentication_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2196]
SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2200]
SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2204]
SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2208]
SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ResetIdentity_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2212]
SteamAPI_ISteamNetworkingSockets_ResetIdentity_proxy ENDP
SteamAPI_ISteamNetworkingSockets_RunCallbacks_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2216]
SteamAPI_ISteamNetworkingSockets_RunCallbacks_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SendMessageToConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2220]
SteamAPI_ISteamNetworkingSockets_SendMessageToConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SendMessages_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2224]
SteamAPI_ISteamNetworkingSockets_SendMessages_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SetCertificate_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2228]
SteamAPI_ISteamNetworkingSockets_SetCertificate_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SetConnectionName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2232]
SteamAPI_ISteamNetworkingSockets_SetConnectionName_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2236]
SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SetConnectionUserData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2240]
SteamAPI_ISteamNetworkingSockets_SetConnectionUserData_proxy ENDP
SteamAPI_ISteamNetworkingUtils_AllocateMessage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2244]
SteamAPI_ISteamNetworkingUtils_AllocateMessage_proxy ENDP
SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2248]
SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate_proxy ENDP
SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2252]
SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2256]
SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations_proxy ENDP
SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2260]
SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetConfigValue_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2264]
SteamAPI_ISteamNetworkingUtils_GetConfigValue_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2268]
SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2272]
SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2276]
SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2280]
SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2284]
SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetPOPCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2288]
SteamAPI_ISteamNetworkingUtils_GetPOPCount_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetPOPList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2292]
SteamAPI_ISteamNetworkingUtils_GetPOPList_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2296]
SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2300]
SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2304]
SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus_proxy ENDP
SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2308]
SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess_proxy ENDP
SteamAPI_ISteamNetworkingUtils_IsFakeIPv4_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2312]
SteamAPI_ISteamNetworkingUtils_IsFakeIPv4_proxy ENDP
SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2316]
SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues_proxy ENDP
SteamAPI_ISteamNetworkingUtils_ParsePingLocationString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2320]
SteamAPI_ISteamNetworkingUtils_ParsePingLocationString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConfigValue_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2324]
SteamAPI_ISteamNetworkingUtils_SetConfigValue_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2328]
SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2332]
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2336]
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2340]
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2344]
SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2348]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2352]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2356]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2360]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2364]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2368]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2372]
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2376]
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2380]
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2384]
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2388]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2392]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2396]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2400]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2404]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString_proxy ENDP
SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2408]
SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser_proxy ENDP
SteamAPI_ISteamNetworking_AllowP2PPacketRelay_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2412]
SteamAPI_ISteamNetworking_AllowP2PPacketRelay_proxy ENDP
SteamAPI_ISteamNetworking_CloseP2PChannelWithUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2416]
SteamAPI_ISteamNetworking_CloseP2PChannelWithUser_proxy ENDP
SteamAPI_ISteamNetworking_CloseP2PSessionWithUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2420]
SteamAPI_ISteamNetworking_CloseP2PSessionWithUser_proxy ENDP
SteamAPI_ISteamNetworking_CreateConnectionSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2424]
SteamAPI_ISteamNetworking_CreateConnectionSocket_proxy ENDP
SteamAPI_ISteamNetworking_CreateListenSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2428]
SteamAPI_ISteamNetworking_CreateListenSocket_proxy ENDP
SteamAPI_ISteamNetworking_CreateP2PConnectionSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2432]
SteamAPI_ISteamNetworking_CreateP2PConnectionSocket_proxy ENDP
SteamAPI_ISteamNetworking_DestroyListenSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2436]
SteamAPI_ISteamNetworking_DestroyListenSocket_proxy ENDP
SteamAPI_ISteamNetworking_DestroySocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2440]
SteamAPI_ISteamNetworking_DestroySocket_proxy ENDP
SteamAPI_ISteamNetworking_GetListenSocketInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2444]
SteamAPI_ISteamNetworking_GetListenSocketInfo_proxy ENDP
SteamAPI_ISteamNetworking_GetMaxPacketSize_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2448]
SteamAPI_ISteamNetworking_GetMaxPacketSize_proxy ENDP
SteamAPI_ISteamNetworking_GetP2PSessionState_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2452]
SteamAPI_ISteamNetworking_GetP2PSessionState_proxy ENDP
SteamAPI_ISteamNetworking_GetSocketConnectionType_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2456]
SteamAPI_ISteamNetworking_GetSocketConnectionType_proxy ENDP
SteamAPI_ISteamNetworking_GetSocketInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2460]
SteamAPI_ISteamNetworking_GetSocketInfo_proxy ENDP
SteamAPI_ISteamNetworking_IsDataAvailable_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2464]
SteamAPI_ISteamNetworking_IsDataAvailable_proxy ENDP
SteamAPI_ISteamNetworking_IsDataAvailableOnSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2468]
SteamAPI_ISteamNetworking_IsDataAvailableOnSocket_proxy ENDP
SteamAPI_ISteamNetworking_IsP2PPacketAvailable_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2472]
SteamAPI_ISteamNetworking_IsP2PPacketAvailable_proxy ENDP
SteamAPI_ISteamNetworking_ReadP2PPacket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2476]
SteamAPI_ISteamNetworking_ReadP2PPacket_proxy ENDP
SteamAPI_ISteamNetworking_RetrieveData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2480]
SteamAPI_ISteamNetworking_RetrieveData_proxy ENDP
SteamAPI_ISteamNetworking_RetrieveDataFromSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2484]
SteamAPI_ISteamNetworking_RetrieveDataFromSocket_proxy ENDP
SteamAPI_ISteamNetworking_SendDataOnSocket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2488]
SteamAPI_ISteamNetworking_SendDataOnSocket_proxy ENDP
SteamAPI_ISteamNetworking_SendP2PPacket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2492]
SteamAPI_ISteamNetworking_SendP2PPacket_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsAppBlocked_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2496]
SteamAPI_ISteamParentalSettings_BIsAppBlocked_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsAppInBlockList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2500]
SteamAPI_ISteamParentalSettings_BIsAppInBlockList_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsFeatureBlocked_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2504]
SteamAPI_ISteamParentalSettings_BIsFeatureBlocked_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2508]
SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2512]
SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsParentalLockLocked_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2516]
SteamAPI_ISteamParentalSettings_BIsParentalLockLocked_proxy ENDP
SteamAPI_ISteamParties_CancelReservation_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2520]
SteamAPI_ISteamParties_CancelReservation_proxy ENDP
SteamAPI_ISteamParties_ChangeNumOpenSlots_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2524]
SteamAPI_ISteamParties_ChangeNumOpenSlots_proxy ENDP
SteamAPI_ISteamParties_CreateBeacon_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2528]
SteamAPI_ISteamParties_CreateBeacon_proxy ENDP
SteamAPI_ISteamParties_DestroyBeacon_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2532]
SteamAPI_ISteamParties_DestroyBeacon_proxy ENDP
SteamAPI_ISteamParties_GetAvailableBeaconLocations_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2536]
SteamAPI_ISteamParties_GetAvailableBeaconLocations_proxy ENDP
SteamAPI_ISteamParties_GetBeaconByIndex_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2540]
SteamAPI_ISteamParties_GetBeaconByIndex_proxy ENDP
SteamAPI_ISteamParties_GetBeaconDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2544]
SteamAPI_ISteamParties_GetBeaconDetails_proxy ENDP
SteamAPI_ISteamParties_GetBeaconLocationData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2548]
SteamAPI_ISteamParties_GetBeaconLocationData_proxy ENDP
SteamAPI_ISteamParties_GetNumActiveBeacons_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2552]
SteamAPI_ISteamParties_GetNumActiveBeacons_proxy ENDP
SteamAPI_ISteamParties_GetNumAvailableBeaconLocations_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2556]
SteamAPI_ISteamParties_GetNumAvailableBeaconLocations_proxy ENDP
SteamAPI_ISteamParties_JoinParty_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2560]
SteamAPI_ISteamParties_JoinParty_proxy ENDP
SteamAPI_ISteamParties_OnReservationCompleted_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2564]
SteamAPI_ISteamParties_OnReservationCompleted_proxy ENDP
SteamAPI_ISteamRemotePlay_BGetSessionClientResolution_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2568]
SteamAPI_ISteamRemotePlay_BGetSessionClientResolution_proxy ENDP
SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2572]
SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2576]
SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionClientName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2580]
SteamAPI_ISteamRemotePlay_GetSessionClientName_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2584]
SteamAPI_ISteamRemotePlay_GetSessionCount_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2588]
SteamAPI_ISteamRemotePlay_GetSessionID_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionSteamID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2592]
SteamAPI_ISteamRemotePlay_GetSessionSteamID_proxy ENDP
SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2596]
SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch_proxy ENDP
SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2600]
SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate_proxy ENDP
SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2604]
SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest_proxy ENDP
SteamAPI_ISteamRemoteStorage_DeletePublishedFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2608]
SteamAPI_ISteamRemoteStorage_DeletePublishedFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_EndFileWriteBatch_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2612]
SteamAPI_ISteamRemoteStorage_EndFileWriteBatch_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2616]
SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2620]
SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2624]
SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2628]
SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2632]
SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileDelete_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2636]
SteamAPI_ISteamRemoteStorage_FileDelete_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileExists_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2640]
SteamAPI_ISteamRemoteStorage_FileExists_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileForget_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2644]
SteamAPI_ISteamRemoteStorage_FileForget_proxy ENDP
SteamAPI_ISteamRemoteStorage_FilePersisted_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2648]
SteamAPI_ISteamRemoteStorage_FilePersisted_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileRead_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2652]
SteamAPI_ISteamRemoteStorage_FileRead_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileReadAsync_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2656]
SteamAPI_ISteamRemoteStorage_FileReadAsync_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2660]
SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileShare_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2664]
SteamAPI_ISteamRemoteStorage_FileShare_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWrite_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2668]
SteamAPI_ISteamRemoteStorage_FileWrite_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteAsync_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2672]
SteamAPI_ISteamRemoteStorage_FileWriteAsync_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2676]
SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteStreamClose_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2680]
SteamAPI_ISteamRemoteStorage_FileWriteStreamClose_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2684]
SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2688]
SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetCachedUGCCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2692]
SteamAPI_ISteamRemoteStorage_GetCachedUGCCount_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2696]
SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetFileCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2700]
SteamAPI_ISteamRemoteStorage_GetFileCount_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetFileNameAndSize_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2704]
SteamAPI_ISteamRemoteStorage_GetFileNameAndSize_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetFileSize_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2708]
SteamAPI_ISteamRemoteStorage_GetFileSize_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetFileTimestamp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2712]
SteamAPI_ISteamRemoteStorage_GetFileTimestamp_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetLocalFileChange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2716]
SteamAPI_ISteamRemoteStorage_GetLocalFileChange_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2720]
SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2724]
SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2728]
SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetQuota_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2732]
SteamAPI_ISteamRemoteStorage_GetQuota_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetSyncPlatforms_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2736]
SteamAPI_ISteamRemoteStorage_GetSyncPlatforms_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetUGCDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2740]
SteamAPI_ISteamRemoteStorage_GetUGCDetails_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2744]
SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2748]
SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails_proxy ENDP
SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2752]
SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount_proxy ENDP
SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2756]
SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp_proxy ENDP
SteamAPI_ISteamRemoteStorage_PublishVideo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2760]
SteamAPI_ISteamRemoteStorage_PublishVideo_proxy ENDP
SteamAPI_ISteamRemoteStorage_PublishWorkshopFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2764]
SteamAPI_ISteamRemoteStorage_PublishWorkshopFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2768]
SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp_proxy ENDP
SteamAPI_ISteamRemoteStorage_SetSyncPlatforms_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2772]
SteamAPI_ISteamRemoteStorage_SetSyncPlatforms_proxy ENDP
SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2776]
SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction_proxy ENDP
SteamAPI_ISteamRemoteStorage_SubscribePublishedFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2780]
SteamAPI_ISteamRemoteStorage_SubscribePublishedFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_UGCDownload_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2784]
SteamAPI_ISteamRemoteStorage_UGCDownload_proxy ENDP
SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2788]
SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation_proxy ENDP
SteamAPI_ISteamRemoteStorage_UGCRead_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2792]
SteamAPI_ISteamRemoteStorage_UGCRead_proxy ENDP
SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2796]
SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2800]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2804]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2808]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2812]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2816]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2820]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2824]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2828]
SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote_proxy ENDP
SteamAPI_ISteamScreenshots_AddScreenshotToLibrary_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2832]
SteamAPI_ISteamScreenshots_AddScreenshotToLibrary_proxy ENDP
SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2836]
SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary_proxy ENDP
SteamAPI_ISteamScreenshots_HookScreenshots_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2840]
SteamAPI_ISteamScreenshots_HookScreenshots_proxy ENDP
SteamAPI_ISteamScreenshots_IsScreenshotsHooked_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2844]
SteamAPI_ISteamScreenshots_IsScreenshotsHooked_proxy ENDP
SteamAPI_ISteamScreenshots_SetLocation_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2848]
SteamAPI_ISteamScreenshots_SetLocation_proxy ENDP
SteamAPI_ISteamScreenshots_TagPublishedFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2852]
SteamAPI_ISteamScreenshots_TagPublishedFile_proxy ENDP
SteamAPI_ISteamScreenshots_TagUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2856]
SteamAPI_ISteamScreenshots_TagUser_proxy ENDP
SteamAPI_ISteamScreenshots_TriggerScreenshot_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2860]
SteamAPI_ISteamScreenshots_TriggerScreenshot_proxy ENDP
SteamAPI_ISteamScreenshots_WriteScreenshot_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2864]
SteamAPI_ISteamScreenshots_WriteScreenshot_proxy ENDP
SteamAPI_ISteamUGC_AddAppDependency_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2868]
SteamAPI_ISteamUGC_AddAppDependency_proxy ENDP
SteamAPI_ISteamUGC_AddDependency_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2872]
SteamAPI_ISteamUGC_AddDependency_proxy ENDP
SteamAPI_ISteamUGC_AddExcludedTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2876]
SteamAPI_ISteamUGC_AddExcludedTag_proxy ENDP
SteamAPI_ISteamUGC_AddItemKeyValueTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2880]
SteamAPI_ISteamUGC_AddItemKeyValueTag_proxy ENDP
SteamAPI_ISteamUGC_AddItemPreviewFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2884]
SteamAPI_ISteamUGC_AddItemPreviewFile_proxy ENDP
SteamAPI_ISteamUGC_AddItemPreviewVideo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2888]
SteamAPI_ISteamUGC_AddItemPreviewVideo_proxy ENDP
SteamAPI_ISteamUGC_AddItemToFavorites_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2892]
SteamAPI_ISteamUGC_AddItemToFavorites_proxy ENDP
SteamAPI_ISteamUGC_AddRequiredKeyValueTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2896]
SteamAPI_ISteamUGC_AddRequiredKeyValueTag_proxy ENDP
SteamAPI_ISteamUGC_AddRequiredTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2900]
SteamAPI_ISteamUGC_AddRequiredTag_proxy ENDP
SteamAPI_ISteamUGC_AddRequiredTagGroup_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2904]
SteamAPI_ISteamUGC_AddRequiredTagGroup_proxy ENDP
SteamAPI_ISteamUGC_BInitWorkshopForGameServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2908]
SteamAPI_ISteamUGC_BInitWorkshopForGameServer_proxy ENDP
SteamAPI_ISteamUGC_CreateItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2912]
SteamAPI_ISteamUGC_CreateItem_proxy ENDP
SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2916]
SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor_proxy ENDP
SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2920]
SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage_proxy ENDP
SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2924]
SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest_proxy ENDP
SteamAPI_ISteamUGC_CreateQueryUserUGCRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2928]
SteamAPI_ISteamUGC_CreateQueryUserUGCRequest_proxy ENDP
SteamAPI_ISteamUGC_DeleteItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2932]
SteamAPI_ISteamUGC_DeleteItem_proxy ENDP
SteamAPI_ISteamUGC_DownloadItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2936]
SteamAPI_ISteamUGC_DownloadItem_proxy ENDP
SteamAPI_ISteamUGC_GetAppDependencies_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2940]
SteamAPI_ISteamUGC_GetAppDependencies_proxy ENDP
SteamAPI_ISteamUGC_GetItemDownloadInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2944]
SteamAPI_ISteamUGC_GetItemDownloadInfo_proxy ENDP
SteamAPI_ISteamUGC_GetItemInstallInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2948]
SteamAPI_ISteamUGC_GetItemInstallInfo_proxy ENDP
SteamAPI_ISteamUGC_GetItemState_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2952]
SteamAPI_ISteamUGC_GetItemState_proxy ENDP
SteamAPI_ISteamUGC_GetItemUpdateProgress_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2956]
SteamAPI_ISteamUGC_GetItemUpdateProgress_proxy ENDP
SteamAPI_ISteamUGC_GetNumSubscribedItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2960]
SteamAPI_ISteamUGC_GetNumSubscribedItems_proxy ENDP
SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2964]
SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2968]
SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCChildren_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2972]
SteamAPI_ISteamUGC_GetQueryUGCChildren_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2976]
SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCMetadata_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2980]
SteamAPI_ISteamUGC_GetQueryUGCMetadata_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2984]
SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2988]
SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCNumTags_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2992]
SteamAPI_ISteamUGC_GetQueryUGCNumTags_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCPreviewURL_proxy PROC
    jmp DWORD PTR [g_steamProcs + 2996]
SteamAPI_ISteamUGC_GetQueryUGCPreviewURL_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3000]
SteamAPI_ISteamUGC_GetQueryUGCResult_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCStatistic_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3004]
SteamAPI_ISteamUGC_GetQueryUGCStatistic_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3008]
SteamAPI_ISteamUGC_GetQueryUGCTag_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3012]
SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName_proxy ENDP
SteamAPI_ISteamUGC_GetSubscribedItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3016]
SteamAPI_ISteamUGC_GetSubscribedItems_proxy ENDP
SteamAPI_ISteamUGC_GetUserItemVote_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3020]
SteamAPI_ISteamUGC_GetUserItemVote_proxy ENDP
SteamAPI_ISteamUGC_GetWorkshopEULAStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3024]
SteamAPI_ISteamUGC_GetWorkshopEULAStatus_proxy ENDP
SteamAPI_ISteamUGC_ReleaseQueryUGCRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3028]
SteamAPI_ISteamUGC_ReleaseQueryUGCRequest_proxy ENDP
SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3032]
SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags_proxy ENDP
SteamAPI_ISteamUGC_RemoveAppDependency_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3036]
SteamAPI_ISteamUGC_RemoveAppDependency_proxy ENDP
SteamAPI_ISteamUGC_RemoveDependency_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3040]
SteamAPI_ISteamUGC_RemoveDependency_proxy ENDP
SteamAPI_ISteamUGC_RemoveItemFromFavorites_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3044]
SteamAPI_ISteamUGC_RemoveItemFromFavorites_proxy ENDP
SteamAPI_ISteamUGC_RemoveItemKeyValueTags_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3048]
SteamAPI_ISteamUGC_RemoveItemKeyValueTags_proxy ENDP
SteamAPI_ISteamUGC_RemoveItemPreview_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3052]
SteamAPI_ISteamUGC_RemoveItemPreview_proxy ENDP
SteamAPI_ISteamUGC_RequestUGCDetails_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3056]
SteamAPI_ISteamUGC_RequestUGCDetails_proxy ENDP
SteamAPI_ISteamUGC_SendQueryUGCRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3060]
SteamAPI_ISteamUGC_SendQueryUGCRequest_proxy ENDP
SteamAPI_ISteamUGC_SetAllowCachedResponse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3064]
SteamAPI_ISteamUGC_SetAllowCachedResponse_proxy ENDP
SteamAPI_ISteamUGC_SetAllowLegacyUpload_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3068]
SteamAPI_ISteamUGC_SetAllowLegacyUpload_proxy ENDP
SteamAPI_ISteamUGC_SetCloudFileNameFilter_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3072]
SteamAPI_ISteamUGC_SetCloudFileNameFilter_proxy ENDP
SteamAPI_ISteamUGC_SetItemContent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3076]
SteamAPI_ISteamUGC_SetItemContent_proxy ENDP
SteamAPI_ISteamUGC_SetItemDescription_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3080]
SteamAPI_ISteamUGC_SetItemDescription_proxy ENDP
SteamAPI_ISteamUGC_SetItemMetadata_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3084]
SteamAPI_ISteamUGC_SetItemMetadata_proxy ENDP
SteamAPI_ISteamUGC_SetItemPreview_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3088]
SteamAPI_ISteamUGC_SetItemPreview_proxy ENDP
SteamAPI_ISteamUGC_SetItemTags_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3092]
SteamAPI_ISteamUGC_SetItemTags_proxy ENDP
SteamAPI_ISteamUGC_SetItemTitle_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3096]
SteamAPI_ISteamUGC_SetItemTitle_proxy ENDP
SteamAPI_ISteamUGC_SetItemUpdateLanguage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3100]
SteamAPI_ISteamUGC_SetItemUpdateLanguage_proxy ENDP
SteamAPI_ISteamUGC_SetItemVisibility_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3104]
SteamAPI_ISteamUGC_SetItemVisibility_proxy ENDP
SteamAPI_ISteamUGC_SetLanguage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3108]
SteamAPI_ISteamUGC_SetLanguage_proxy ENDP
SteamAPI_ISteamUGC_SetMatchAnyTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3112]
SteamAPI_ISteamUGC_SetMatchAnyTag_proxy ENDP
SteamAPI_ISteamUGC_SetRankedByTrendDays_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3116]
SteamAPI_ISteamUGC_SetRankedByTrendDays_proxy ENDP
SteamAPI_ISteamUGC_SetReturnAdditionalPreviews_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3120]
SteamAPI_ISteamUGC_SetReturnAdditionalPreviews_proxy ENDP
SteamAPI_ISteamUGC_SetReturnChildren_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3124]
SteamAPI_ISteamUGC_SetReturnChildren_proxy ENDP
SteamAPI_ISteamUGC_SetReturnKeyValueTags_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3128]
SteamAPI_ISteamUGC_SetReturnKeyValueTags_proxy ENDP
SteamAPI_ISteamUGC_SetReturnLongDescription_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3132]
SteamAPI_ISteamUGC_SetReturnLongDescription_proxy ENDP
SteamAPI_ISteamUGC_SetReturnMetadata_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3136]
SteamAPI_ISteamUGC_SetReturnMetadata_proxy ENDP
SteamAPI_ISteamUGC_SetReturnOnlyIDs_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3140]
SteamAPI_ISteamUGC_SetReturnOnlyIDs_proxy ENDP
SteamAPI_ISteamUGC_SetReturnPlaytimeStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3144]
SteamAPI_ISteamUGC_SetReturnPlaytimeStats_proxy ENDP
SteamAPI_ISteamUGC_SetReturnTotalOnly_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3148]
SteamAPI_ISteamUGC_SetReturnTotalOnly_proxy ENDP
SteamAPI_ISteamUGC_SetSearchText_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3152]
SteamAPI_ISteamUGC_SetSearchText_proxy ENDP
SteamAPI_ISteamUGC_SetTimeCreatedDateRange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3156]
SteamAPI_ISteamUGC_SetTimeCreatedDateRange_proxy ENDP
SteamAPI_ISteamUGC_SetTimeUpdatedDateRange_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3160]
SteamAPI_ISteamUGC_SetTimeUpdatedDateRange_proxy ENDP
SteamAPI_ISteamUGC_SetUserItemVote_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3164]
SteamAPI_ISteamUGC_SetUserItemVote_proxy ENDP
SteamAPI_ISteamUGC_ShowWorkshopEULA_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3168]
SteamAPI_ISteamUGC_ShowWorkshopEULA_proxy ENDP
SteamAPI_ISteamUGC_StartItemUpdate_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3172]
SteamAPI_ISteamUGC_StartItemUpdate_proxy ENDP
SteamAPI_ISteamUGC_StartPlaytimeTracking_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3176]
SteamAPI_ISteamUGC_StartPlaytimeTracking_proxy ENDP
SteamAPI_ISteamUGC_StopPlaytimeTracking_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3180]
SteamAPI_ISteamUGC_StopPlaytimeTracking_proxy ENDP
SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3184]
SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems_proxy ENDP
SteamAPI_ISteamUGC_SubmitItemUpdate_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3188]
SteamAPI_ISteamUGC_SubmitItemUpdate_proxy ENDP
SteamAPI_ISteamUGC_SubscribeItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3192]
SteamAPI_ISteamUGC_SubscribeItem_proxy ENDP
SteamAPI_ISteamUGC_SuspendDownloads_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3196]
SteamAPI_ISteamUGC_SuspendDownloads_proxy ENDP
SteamAPI_ISteamUGC_UnsubscribeItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3200]
SteamAPI_ISteamUGC_UnsubscribeItem_proxy ENDP
SteamAPI_ISteamUGC_UpdateItemPreviewFile_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3204]
SteamAPI_ISteamUGC_UpdateItemPreviewFile_proxy ENDP
SteamAPI_ISteamUGC_UpdateItemPreviewVideo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3208]
SteamAPI_ISteamUGC_UpdateItemPreviewVideo_proxy ENDP
SteamAPI_ISteamUserStats_AttachLeaderboardUGC_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3212]
SteamAPI_ISteamUserStats_AttachLeaderboardUGC_proxy ENDP
SteamAPI_ISteamUserStats_ClearAchievement_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3216]
SteamAPI_ISteamUserStats_ClearAchievement_proxy ENDP
SteamAPI_ISteamUserStats_DownloadLeaderboardEntries_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3220]
SteamAPI_ISteamUserStats_DownloadLeaderboardEntries_proxy ENDP
SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3224]
SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers_proxy ENDP
SteamAPI_ISteamUserStats_FindLeaderboard_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3228]
SteamAPI_ISteamUserStats_FindLeaderboard_proxy ENDP
SteamAPI_ISteamUserStats_FindOrCreateLeaderboard_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3232]
SteamAPI_ISteamUserStats_FindOrCreateLeaderboard_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievement_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3236]
SteamAPI_ISteamUserStats_GetAchievement_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementAchievedPercent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3240]
SteamAPI_ISteamUserStats_GetAchievementAchievedPercent_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3244]
SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3248]
SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementIcon_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3252]
SteamAPI_ISteamUserStats_GetAchievementIcon_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3256]
SteamAPI_ISteamUserStats_GetAchievementName_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3260]
SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3264]
SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32_proxy ENDP
SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3268]
SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry_proxy ENDP
SteamAPI_ISteamUserStats_GetGlobalStatDouble_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3272]
SteamAPI_ISteamUserStats_GetGlobalStatDouble_proxy ENDP
SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3276]
SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble_proxy ENDP
SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3280]
SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64_proxy ENDP
SteamAPI_ISteamUserStats_GetGlobalStatInt64_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3284]
SteamAPI_ISteamUserStats_GetGlobalStatInt64_proxy ENDP
SteamAPI_ISteamUserStats_GetLeaderboardDisplayType_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3288]
SteamAPI_ISteamUserStats_GetLeaderboardDisplayType_proxy ENDP
SteamAPI_ISteamUserStats_GetLeaderboardEntryCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3292]
SteamAPI_ISteamUserStats_GetLeaderboardEntryCount_proxy ENDP
SteamAPI_ISteamUserStats_GetLeaderboardName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3296]
SteamAPI_ISteamUserStats_GetLeaderboardName_proxy ENDP
SteamAPI_ISteamUserStats_GetLeaderboardSortMethod_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3300]
SteamAPI_ISteamUserStats_GetLeaderboardSortMethod_proxy ENDP
SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3304]
SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo_proxy ENDP
SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3308]
SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo_proxy ENDP
SteamAPI_ISteamUserStats_GetNumAchievements_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3312]
SteamAPI_ISteamUserStats_GetNumAchievements_proxy ENDP
SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3316]
SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers_proxy ENDP
SteamAPI_ISteamUserStats_GetStatFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3320]
SteamAPI_ISteamUserStats_GetStatFloat_proxy ENDP
SteamAPI_ISteamUserStats_GetStatInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3324]
SteamAPI_ISteamUserStats_GetStatInt32_proxy ENDP
SteamAPI_ISteamUserStats_GetUserAchievement_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3328]
SteamAPI_ISteamUserStats_GetUserAchievement_proxy ENDP
SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3332]
SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime_proxy ENDP
SteamAPI_ISteamUserStats_GetUserStatFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3336]
SteamAPI_ISteamUserStats_GetUserStatFloat_proxy ENDP
SteamAPI_ISteamUserStats_GetUserStatInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3340]
SteamAPI_ISteamUserStats_GetUserStatInt32_proxy ENDP
SteamAPI_ISteamUserStats_IndicateAchievementProgress_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3344]
SteamAPI_ISteamUserStats_IndicateAchievementProgress_proxy ENDP
SteamAPI_ISteamUserStats_RequestCurrentStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3348]
SteamAPI_ISteamUserStats_RequestCurrentStats_proxy ENDP
SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3352]
SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages_proxy ENDP
SteamAPI_ISteamUserStats_RequestGlobalStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3356]
SteamAPI_ISteamUserStats_RequestGlobalStats_proxy ENDP
SteamAPI_ISteamUserStats_RequestUserStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3360]
SteamAPI_ISteamUserStats_RequestUserStats_proxy ENDP
SteamAPI_ISteamUserStats_ResetAllStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3364]
SteamAPI_ISteamUserStats_ResetAllStats_proxy ENDP
SteamAPI_ISteamUserStats_SetAchievement_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3368]
SteamAPI_ISteamUserStats_SetAchievement_proxy ENDP
SteamAPI_ISteamUserStats_SetStatFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3372]
SteamAPI_ISteamUserStats_SetStatFloat_proxy ENDP
SteamAPI_ISteamUserStats_SetStatInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3376]
SteamAPI_ISteamUserStats_SetStatInt32_proxy ENDP
SteamAPI_ISteamUserStats_StoreStats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3380]
SteamAPI_ISteamUserStats_StoreStats_proxy ENDP
SteamAPI_ISteamUserStats_UpdateAvgRateStat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3384]
SteamAPI_ISteamUserStats_UpdateAvgRateStat_proxy ENDP
SteamAPI_ISteamUserStats_UploadLeaderboardScore_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3388]
SteamAPI_ISteamUserStats_UploadLeaderboardScore_proxy ENDP
SteamAPI_ISteamUser_AdvertiseGame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3392]
SteamAPI_ISteamUser_AdvertiseGame_proxy ENDP
SteamAPI_ISteamUser_BIsBehindNAT_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3396]
SteamAPI_ISteamUser_BIsBehindNAT_proxy ENDP
SteamAPI_ISteamUser_BIsPhoneIdentifying_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3400]
SteamAPI_ISteamUser_BIsPhoneIdentifying_proxy ENDP
SteamAPI_ISteamUser_BIsPhoneRequiringVerification_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3404]
SteamAPI_ISteamUser_BIsPhoneRequiringVerification_proxy ENDP
SteamAPI_ISteamUser_BIsPhoneVerified_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3408]
SteamAPI_ISteamUser_BIsPhoneVerified_proxy ENDP
SteamAPI_ISteamUser_BIsTwoFactorEnabled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3412]
SteamAPI_ISteamUser_BIsTwoFactorEnabled_proxy ENDP
SteamAPI_ISteamUser_BLoggedOn_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3416]
SteamAPI_ISteamUser_BLoggedOn_proxy ENDP
SteamAPI_ISteamUser_BSetDurationControlOnlineState_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3420]
SteamAPI_ISteamUser_BSetDurationControlOnlineState_proxy ENDP
SteamAPI_ISteamUser_BeginAuthSession_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3424]
SteamAPI_ISteamUser_BeginAuthSession_proxy ENDP
SteamAPI_ISteamUser_CancelAuthTicket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3428]
SteamAPI_ISteamUser_CancelAuthTicket_proxy ENDP
SteamAPI_ISteamUser_DecompressVoice_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3432]
SteamAPI_ISteamUser_DecompressVoice_proxy ENDP
SteamAPI_ISteamUser_EndAuthSession_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3436]
SteamAPI_ISteamUser_EndAuthSession_proxy ENDP
SteamAPI_ISteamUser_GetAuthSessionTicket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3440]
SteamAPI_ISteamUser_GetAuthSessionTicket_proxy ENDP
SteamAPI_ISteamUser_GetAvailableVoice_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3444]
SteamAPI_ISteamUser_GetAvailableVoice_proxy ENDP
SteamAPI_ISteamUser_GetDurationControl_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3448]
SteamAPI_ISteamUser_GetDurationControl_proxy ENDP
SteamAPI_ISteamUser_GetEncryptedAppTicket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3452]
SteamAPI_ISteamUser_GetEncryptedAppTicket_proxy ENDP
SteamAPI_ISteamUser_GetGameBadgeLevel_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3456]
SteamAPI_ISteamUser_GetGameBadgeLevel_proxy ENDP
SteamAPI_ISteamUser_GetHSteamUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3460]
SteamAPI_ISteamUser_GetHSteamUser_proxy ENDP
SteamAPI_ISteamUser_GetMarketEligibility_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3464]
SteamAPI_ISteamUser_GetMarketEligibility_proxy ENDP
SteamAPI_ISteamUser_GetPlayerSteamLevel_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3468]
SteamAPI_ISteamUser_GetPlayerSteamLevel_proxy ENDP
SteamAPI_ISteamUser_GetSteamID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3472]
SteamAPI_ISteamUser_GetSteamID_proxy ENDP
SteamAPI_ISteamUser_GetUserDataFolder_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3476]
SteamAPI_ISteamUser_GetUserDataFolder_proxy ENDP
SteamAPI_ISteamUser_GetVoice_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3480]
SteamAPI_ISteamUser_GetVoice_proxy ENDP
SteamAPI_ISteamUser_GetVoiceOptimalSampleRate_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3484]
SteamAPI_ISteamUser_GetVoiceOptimalSampleRate_proxy ENDP
SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3488]
SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED_proxy ENDP
SteamAPI_ISteamUser_RequestEncryptedAppTicket_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3492]
SteamAPI_ISteamUser_RequestEncryptedAppTicket_proxy ENDP
SteamAPI_ISteamUser_RequestStoreAuthURL_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3496]
SteamAPI_ISteamUser_RequestStoreAuthURL_proxy ENDP
SteamAPI_ISteamUser_StartVoiceRecording_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3500]
SteamAPI_ISteamUser_StartVoiceRecording_proxy ENDP
SteamAPI_ISteamUser_StopVoiceRecording_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3504]
SteamAPI_ISteamUser_StopVoiceRecording_proxy ENDP
SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3508]
SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED_proxy ENDP
SteamAPI_ISteamUser_TrackAppUsageEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3512]
SteamAPI_ISteamUser_TrackAppUsageEvent_proxy ENDP
SteamAPI_ISteamUser_UserHasLicenseForApp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3516]
SteamAPI_ISteamUser_UserHasLicenseForApp_proxy ENDP
SteamAPI_ISteamUtils_BOverlayNeedsPresent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3520]
SteamAPI_ISteamUtils_BOverlayNeedsPresent_proxy ENDP
SteamAPI_ISteamUtils_CheckFileSignature_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3524]
SteamAPI_ISteamUtils_CheckFileSignature_proxy ENDP
SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3528]
SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput_proxy ENDP
SteamAPI_ISteamUtils_FilterText_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3532]
SteamAPI_ISteamUtils_FilterText_proxy ENDP
SteamAPI_ISteamUtils_GetAPICallFailureReason_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3536]
SteamAPI_ISteamUtils_GetAPICallFailureReason_proxy ENDP
SteamAPI_ISteamUtils_GetAPICallResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3540]
SteamAPI_ISteamUtils_GetAPICallResult_proxy ENDP
SteamAPI_ISteamUtils_GetAppID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3544]
SteamAPI_ISteamUtils_GetAppID_proxy ENDP
SteamAPI_ISteamUtils_GetConnectedUniverse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3548]
SteamAPI_ISteamUtils_GetConnectedUniverse_proxy ENDP
SteamAPI_ISteamUtils_GetCurrentBatteryPower_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3552]
SteamAPI_ISteamUtils_GetCurrentBatteryPower_proxy ENDP
SteamAPI_ISteamUtils_GetEnteredGamepadTextInput_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3556]
SteamAPI_ISteamUtils_GetEnteredGamepadTextInput_proxy ENDP
SteamAPI_ISteamUtils_GetEnteredGamepadTextLength_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3560]
SteamAPI_ISteamUtils_GetEnteredGamepadTextLength_proxy ENDP
SteamAPI_ISteamUtils_GetIPCCallCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3564]
SteamAPI_ISteamUtils_GetIPCCallCount_proxy ENDP
SteamAPI_ISteamUtils_GetIPCountry_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3568]
SteamAPI_ISteamUtils_GetIPCountry_proxy ENDP
SteamAPI_ISteamUtils_GetIPv6ConnectivityState_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3572]
SteamAPI_ISteamUtils_GetIPv6ConnectivityState_proxy ENDP
SteamAPI_ISteamUtils_GetImageRGBA_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3576]
SteamAPI_ISteamUtils_GetImageRGBA_proxy ENDP
SteamAPI_ISteamUtils_GetImageSize_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3580]
SteamAPI_ISteamUtils_GetImageSize_proxy ENDP
SteamAPI_ISteamUtils_GetSecondsSinceAppActive_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3584]
SteamAPI_ISteamUtils_GetSecondsSinceAppActive_proxy ENDP
SteamAPI_ISteamUtils_GetSecondsSinceComputerActive_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3588]
SteamAPI_ISteamUtils_GetSecondsSinceComputerActive_proxy ENDP
SteamAPI_ISteamUtils_GetServerRealTime_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3592]
SteamAPI_ISteamUtils_GetServerRealTime_proxy ENDP
SteamAPI_ISteamUtils_GetSteamUILanguage_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3596]
SteamAPI_ISteamUtils_GetSteamUILanguage_proxy ENDP
SteamAPI_ISteamUtils_InitFilterText_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3600]
SteamAPI_ISteamUtils_InitFilterText_proxy ENDP
SteamAPI_ISteamUtils_IsAPICallCompleted_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3604]
SteamAPI_ISteamUtils_IsAPICallCompleted_proxy ENDP
SteamAPI_ISteamUtils_IsOverlayEnabled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3608]
SteamAPI_ISteamUtils_IsOverlayEnabled_proxy ENDP
SteamAPI_ISteamUtils_IsSteamChinaLauncher_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3612]
SteamAPI_ISteamUtils_IsSteamChinaLauncher_proxy ENDP
SteamAPI_ISteamUtils_IsSteamInBigPictureMode_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3616]
SteamAPI_ISteamUtils_IsSteamInBigPictureMode_proxy ENDP
SteamAPI_ISteamUtils_IsSteamRunningInVR_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3620]
SteamAPI_ISteamUtils_IsSteamRunningInVR_proxy ENDP
SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3624]
SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck_proxy ENDP
SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3628]
SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled_proxy ENDP
SteamAPI_ISteamUtils_SetGameLauncherMode_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3632]
SteamAPI_ISteamUtils_SetGameLauncherMode_proxy ENDP
SteamAPI_ISteamUtils_SetOverlayNotificationInset_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3636]
SteamAPI_ISteamUtils_SetOverlayNotificationInset_proxy ENDP
SteamAPI_ISteamUtils_SetOverlayNotificationPosition_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3640]
SteamAPI_ISteamUtils_SetOverlayNotificationPosition_proxy ENDP
SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3644]
SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled_proxy ENDP
SteamAPI_ISteamUtils_SetWarningMessageHook_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3648]
SteamAPI_ISteamUtils_SetWarningMessageHook_proxy ENDP
SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3652]
SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput_proxy ENDP
SteamAPI_ISteamUtils_ShowGamepadTextInput_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3656]
SteamAPI_ISteamUtils_ShowGamepadTextInput_proxy ENDP
SteamAPI_ISteamUtils_StartVRDashboard_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3660]
SteamAPI_ISteamUtils_StartVRDashboard_proxy ENDP
SteamAPI_ISteamVideo_GetOPFSettings_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3664]
SteamAPI_ISteamVideo_GetOPFSettings_proxy ENDP
SteamAPI_ISteamVideo_GetOPFStringForApp_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3668]
SteamAPI_ISteamVideo_GetOPFStringForApp_proxy ENDP
SteamAPI_ISteamVideo_GetVideoURL_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3672]
SteamAPI_ISteamVideo_GetVideoURL_proxy ENDP
SteamAPI_ISteamVideo_IsBroadcasting_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3676]
SteamAPI_ISteamVideo_IsBroadcasting_proxy ENDP
SteamAPI_IsSteamRunning_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3680]
SteamAPI_IsSteamRunning_proxy ENDP
SteamAPI_ManualDispatch_FreeLastCallback_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3684]
SteamAPI_ManualDispatch_FreeLastCallback_proxy ENDP
SteamAPI_ManualDispatch_GetAPICallResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3688]
SteamAPI_ManualDispatch_GetAPICallResult_proxy ENDP
SteamAPI_ManualDispatch_GetNextCallback_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3692]
SteamAPI_ManualDispatch_GetNextCallback_proxy ENDP
SteamAPI_ManualDispatch_Init_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3696]
SteamAPI_ManualDispatch_Init_proxy ENDP
SteamAPI_ManualDispatch_RunFrame_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3700]
SteamAPI_ManualDispatch_RunFrame_proxy ENDP
SteamAPI_MatchMakingKeyValuePair_t_Construct_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3704]
SteamAPI_MatchMakingKeyValuePair_t_Construct_proxy ENDP
SteamAPI_RegisterCallResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3708]
SteamAPI_RegisterCallResult_proxy ENDP
SteamAPI_RegisterCallback_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3712]
SteamAPI_RegisterCallback_proxy ENDP
SteamAPI_ReleaseCurrentThreadMemory_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3716]
SteamAPI_ReleaseCurrentThreadMemory_proxy ENDP
SteamAPI_RunCallbacks_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3720]
SteamAPI_RunCallbacks_proxy ENDP
SteamAPI_SetBreakpadAppID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3724]
SteamAPI_SetBreakpadAppID_proxy ENDP
SteamAPI_SetMiniDumpComment_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3728]
SteamAPI_SetMiniDumpComment_proxy ENDP
SteamAPI_SetTryCatchCallbacks_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3732]
SteamAPI_SetTryCatchCallbacks_proxy ENDP
SteamAPI_Shutdown_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3736]
SteamAPI_Shutdown_proxy ENDP
SteamAPI_SteamAppList_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3740]
SteamAPI_SteamAppList_v001_proxy ENDP
SteamAPI_SteamApps_v008_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3744]
SteamAPI_SteamApps_v008_proxy ENDP
SteamAPI_SteamController_v008_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3748]
SteamAPI_SteamController_v008_proxy ENDP
SteamAPI_SteamDatagramHostedAddress_Clear_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3752]
SteamAPI_SteamDatagramHostedAddress_Clear_proxy ENDP
SteamAPI_SteamDatagramHostedAddress_GetPopID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3756]
SteamAPI_SteamDatagramHostedAddress_GetPopID_proxy ENDP
SteamAPI_SteamDatagramHostedAddress_SetDevAddress_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3760]
SteamAPI_SteamDatagramHostedAddress_SetDevAddress_proxy ENDP
SteamAPI_SteamFriends_v017_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3764]
SteamAPI_SteamFriends_v017_proxy ENDP
SteamAPI_SteamGameSearch_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3768]
SteamAPI_SteamGameSearch_v001_proxy ENDP
SteamAPI_SteamGameServerHTTP_v003_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3772]
SteamAPI_SteamGameServerHTTP_v003_proxy ENDP
SteamAPI_SteamGameServerInventory_v003_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3776]
SteamAPI_SteamGameServerInventory_v003_proxy ENDP
SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3780]
SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002_proxy ENDP
SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3784]
SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012_proxy ENDP
SteamAPI_SteamGameServerNetworking_v006_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3788]
SteamAPI_SteamGameServerNetworking_v006_proxy ENDP
SteamAPI_SteamGameServerStats_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3792]
SteamAPI_SteamGameServerStats_v001_proxy ENDP
SteamAPI_SteamGameServerUGC_v016_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3796]
SteamAPI_SteamGameServerUGC_v016_proxy ENDP
SteamAPI_SteamGameServerUtils_v010_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3800]
SteamAPI_SteamGameServerUtils_v010_proxy ENDP
SteamAPI_SteamGameServer_v014_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3804]
SteamAPI_SteamGameServer_v014_proxy ENDP
SteamAPI_SteamHTMLSurface_v005_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3808]
SteamAPI_SteamHTMLSurface_v005_proxy ENDP
SteamAPI_SteamHTTP_v003_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3812]
SteamAPI_SteamHTTP_v003_proxy ENDP
SteamAPI_SteamIPAddress_t_IsSet_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3816]
SteamAPI_SteamIPAddress_t_IsSet_proxy ENDP
SteamAPI_SteamInput_v006_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3820]
SteamAPI_SteamInput_v006_proxy ENDP
SteamAPI_SteamInventory_v003_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3824]
SteamAPI_SteamInventory_v003_proxy ENDP
SteamAPI_SteamMatchmakingServers_v002_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3828]
SteamAPI_SteamMatchmakingServers_v002_proxy ENDP
SteamAPI_SteamMatchmaking_v009_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3832]
SteamAPI_SteamMatchmaking_v009_proxy ENDP
SteamAPI_SteamMusicRemote_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3836]
SteamAPI_SteamMusicRemote_v001_proxy ENDP
SteamAPI_SteamMusic_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3840]
SteamAPI_SteamMusic_v001_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetFloat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3844]
SteamAPI_SteamNetworkingConfigValue_t_SetFloat_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetInt32_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3848]
SteamAPI_SteamNetworkingConfigValue_t_SetInt32_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetInt64_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3852]
SteamAPI_SteamNetworkingConfigValue_t_SetInt64_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetPtr_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3856]
SteamAPI_SteamNetworkingConfigValue_t_SetPtr_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3860]
SteamAPI_SteamNetworkingConfigValue_t_SetString_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_Clear_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3864]
SteamAPI_SteamNetworkingIPAddr_Clear_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_GetFakeIPType_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3868]
SteamAPI_SteamNetworkingIPAddr_GetFakeIPType_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_GetIPv4_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3872]
SteamAPI_SteamNetworkingIPAddr_GetIPv4_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsEqualTo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3876]
SteamAPI_SteamNetworkingIPAddr_IsEqualTo_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsFakeIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3880]
SteamAPI_SteamNetworkingIPAddr_IsFakeIP_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsIPv4_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3884]
SteamAPI_SteamNetworkingIPAddr_IsIPv4_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3888]
SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsLocalHost_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3892]
SteamAPI_SteamNetworkingIPAddr_IsLocalHost_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_ParseString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3896]
SteamAPI_SteamNetworkingIPAddr_ParseString_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_SetIPv4_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3900]
SteamAPI_SteamNetworkingIPAddr_SetIPv4_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_SetIPv6_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3904]
SteamAPI_SteamNetworkingIPAddr_SetIPv6_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3908]
SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_ToString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3912]
SteamAPI_SteamNetworkingIPAddr_ToString_proxy ENDP
SteamAPI_SteamNetworkingIdentity_Clear_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3916]
SteamAPI_SteamNetworkingIdentity_Clear_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetFakeIPType_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3920]
SteamAPI_SteamNetworkingIdentity_GetFakeIPType_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetGenericBytes_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3924]
SteamAPI_SteamNetworkingIdentity_GetGenericBytes_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetGenericString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3928]
SteamAPI_SteamNetworkingIdentity_GetGenericString_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetIPAddr_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3932]
SteamAPI_SteamNetworkingIdentity_GetIPAddr_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetIPv4_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3936]
SteamAPI_SteamNetworkingIdentity_GetIPv4_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetPSNID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3940]
SteamAPI_SteamNetworkingIdentity_GetPSNID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetStadiaID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3944]
SteamAPI_SteamNetworkingIdentity_GetStadiaID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetSteamID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3948]
SteamAPI_SteamNetworkingIdentity_GetSteamID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetSteamID64_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3952]
SteamAPI_SteamNetworkingIdentity_GetSteamID64_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3956]
SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_IsEqualTo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3960]
SteamAPI_SteamNetworkingIdentity_IsEqualTo_proxy ENDP
SteamAPI_SteamNetworkingIdentity_IsFakeIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3964]
SteamAPI_SteamNetworkingIdentity_IsFakeIP_proxy ENDP
SteamAPI_SteamNetworkingIdentity_IsInvalid_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3968]
SteamAPI_SteamNetworkingIdentity_IsInvalid_proxy ENDP
SteamAPI_SteamNetworkingIdentity_IsLocalHost_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3972]
SteamAPI_SteamNetworkingIdentity_IsLocalHost_proxy ENDP
SteamAPI_SteamNetworkingIdentity_ParseString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3976]
SteamAPI_SteamNetworkingIdentity_ParseString_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetGenericBytes_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3980]
SteamAPI_SteamNetworkingIdentity_SetGenericBytes_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetGenericString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3984]
SteamAPI_SteamNetworkingIdentity_SetGenericString_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetIPAddr_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3988]
SteamAPI_SteamNetworkingIdentity_SetIPAddr_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetIPv4Addr_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3992]
SteamAPI_SteamNetworkingIdentity_SetIPv4Addr_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetLocalHost_proxy PROC
    jmp DWORD PTR [g_steamProcs + 3996]
SteamAPI_SteamNetworkingIdentity_SetLocalHost_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetPSNID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4000]
SteamAPI_SteamNetworkingIdentity_SetPSNID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetStadiaID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4004]
SteamAPI_SteamNetworkingIdentity_SetStadiaID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetSteamID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4008]
SteamAPI_SteamNetworkingIdentity_SetSteamID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetSteamID64_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4012]
SteamAPI_SteamNetworkingIdentity_SetSteamID64_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4016]
SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_ToString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4020]
SteamAPI_SteamNetworkingIdentity_ToString_proxy ENDP
SteamAPI_SteamNetworkingMessage_t_Release_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4024]
SteamAPI_SteamNetworkingMessage_t_Release_proxy ENDP
SteamAPI_SteamNetworkingMessages_SteamAPI_v002_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4028]
SteamAPI_SteamNetworkingMessages_SteamAPI_v002_proxy ENDP
SteamAPI_SteamNetworkingSockets_SteamAPI_v012_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4032]
SteamAPI_SteamNetworkingSockets_SteamAPI_v012_proxy ENDP
SteamAPI_SteamNetworkingUtils_SteamAPI_v004_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4036]
SteamAPI_SteamNetworkingUtils_SteamAPI_v004_proxy ENDP
SteamAPI_SteamNetworking_v006_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4040]
SteamAPI_SteamNetworking_v006_proxy ENDP
SteamAPI_SteamParentalSettings_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4044]
SteamAPI_SteamParentalSettings_v001_proxy ENDP
SteamAPI_SteamParties_v002_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4048]
SteamAPI_SteamParties_v002_proxy ENDP
SteamAPI_SteamRemotePlay_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4052]
SteamAPI_SteamRemotePlay_v001_proxy ENDP
SteamAPI_SteamRemoteStorage_v016_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4056]
SteamAPI_SteamRemoteStorage_v016_proxy ENDP
SteamAPI_SteamScreenshots_v003_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4060]
SteamAPI_SteamScreenshots_v003_proxy ENDP
SteamAPI_SteamUGC_v016_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4064]
SteamAPI_SteamUGC_v016_proxy ENDP
SteamAPI_SteamUserStats_v012_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4068]
SteamAPI_SteamUserStats_v012_proxy ENDP
SteamAPI_SteamUser_v021_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4072]
SteamAPI_SteamUser_v021_proxy ENDP
SteamAPI_SteamUtils_v010_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4076]
SteamAPI_SteamUtils_v010_proxy ENDP
SteamAPI_SteamVideo_v002_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4080]
SteamAPI_SteamVideo_v002_proxy ENDP
SteamAPI_UnregisterCallResult_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4084]
SteamAPI_UnregisterCallResult_proxy ENDP
SteamAPI_UnregisterCallback_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4088]
SteamAPI_UnregisterCallback_proxy ENDP
SteamAPI_UseBreakpadCrashHandler_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4092]
SteamAPI_UseBreakpadCrashHandler_proxy ENDP
SteamAPI_WriteMiniDump_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4096]
SteamAPI_WriteMiniDump_proxy ENDP
SteamAPI_gameserveritem_t_Construct_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4100]
SteamAPI_gameserveritem_t_Construct_proxy ENDP
SteamAPI_gameserveritem_t_GetName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4104]
SteamAPI_gameserveritem_t_GetName_proxy ENDP
SteamAPI_gameserveritem_t_SetName_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4108]
SteamAPI_gameserveritem_t_SetName_proxy ENDP
SteamAPI_servernetadr_t_Assign_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4112]
SteamAPI_servernetadr_t_Assign_proxy ENDP
SteamAPI_servernetadr_t_Construct_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4116]
SteamAPI_servernetadr_t_Construct_proxy ENDP
SteamAPI_servernetadr_t_GetConnectionAddressString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4120]
SteamAPI_servernetadr_t_GetConnectionAddressString_proxy ENDP
SteamAPI_servernetadr_t_GetConnectionPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4124]
SteamAPI_servernetadr_t_GetConnectionPort_proxy ENDP
SteamAPI_servernetadr_t_GetIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4128]
SteamAPI_servernetadr_t_GetIP_proxy ENDP
SteamAPI_servernetadr_t_GetQueryAddressString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4132]
SteamAPI_servernetadr_t_GetQueryAddressString_proxy ENDP
SteamAPI_servernetadr_t_GetQueryPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4136]
SteamAPI_servernetadr_t_GetQueryPort_proxy ENDP
SteamAPI_servernetadr_t_Init_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4140]
SteamAPI_servernetadr_t_Init_proxy ENDP
SteamAPI_servernetadr_t_IsLessThan_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4144]
SteamAPI_servernetadr_t_IsLessThan_proxy ENDP
SteamAPI_servernetadr_t_SetConnectionPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4148]
SteamAPI_servernetadr_t_SetConnectionPort_proxy ENDP
SteamAPI_servernetadr_t_SetIP_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4152]
SteamAPI_servernetadr_t_SetIP_proxy ENDP
SteamAPI_servernetadr_t_SetQueryPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4156]
SteamAPI_servernetadr_t_SetQueryPort_proxy ENDP
SteamClient_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4160]
SteamClient_proxy ENDP
SteamGameServer_BSecure_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4164]
SteamGameServer_BSecure_proxy ENDP
SteamGameServer_GetHSteamPipe_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4168]
SteamGameServer_GetHSteamPipe_proxy ENDP
SteamGameServer_GetHSteamUser_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4172]
SteamGameServer_GetHSteamUser_proxy ENDP
SteamGameServer_GetIPCCallCount_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4176]
SteamGameServer_GetIPCCallCount_proxy ENDP
SteamGameServer_GetSteamID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4180]
SteamGameServer_GetSteamID_proxy ENDP
SteamGameServer_InitSafe_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4184]
SteamGameServer_InitSafe_proxy ENDP
SteamGameServer_RunCallbacks_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4188]
SteamGameServer_RunCallbacks_proxy ENDP
SteamGameServer_Shutdown_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4192]
SteamGameServer_Shutdown_proxy ENDP
SteamInternal_ContextInit_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4196]
SteamInternal_ContextInit_proxy ENDP
SteamInternal_CreateInterface_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4200]
SteamInternal_CreateInterface_proxy ENDP
SteamInternal_FindOrCreateGameServerInterface_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4204]
SteamInternal_FindOrCreateGameServerInterface_proxy ENDP
SteamInternal_FindOrCreateUserInterface_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4208]
SteamInternal_FindOrCreateUserInterface_proxy ENDP
SteamInternal_GameServer_Init_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4212]
SteamInternal_GameServer_Init_proxy ENDP
g_pSteamClientGameServer_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4216]
g_pSteamClientGameServer_proxy ENDP
SteamInternal_SteamAPI_Init_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4220]
SteamInternal_SteamAPI_Init_proxy ENDP
SteamInternal_GameServer_Init_V2_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4224]
SteamInternal_GameServer_Init_V2_proxy ENDP
SteamAPI_ISteamGameServer_EnableHeartbeats_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4228]
SteamAPI_ISteamGameServer_EnableHeartbeats_proxy ENDP
SteamAPI_ISteamGameServer_ForceHeartbeat_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4232]
SteamAPI_ISteamGameServer_ForceHeartbeat_proxy ENDP
SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4236]
SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_proxy ENDP
SteamAPI_ISteamGameServer_SendUserDisconnect_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4240]
SteamAPI_ISteamGameServer_SendUserDisconnect_proxy ENDP
SteamAPI_ISteamGameServer_SetHeartbeatInterval_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4244]
SteamAPI_ISteamGameServer_SetHeartbeatInterval_proxy ENDP
SteamAPI_ISteamInput_GetGlyphForActionOrigin_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4248]
SteamAPI_ISteamInput_GetGlyphForActionOrigin_proxy ENDP
SteamAPI_ISteamInput_TriggerHapticPulse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4252]
SteamAPI_ISteamInput_TriggerHapticPulse_proxy ENDP
SteamAPI_ISteamInput_TriggerRepeatedHapticPulse_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4256]
SteamAPI_ISteamInput_TriggerRepeatedHapticPulse_proxy ENDP
SteamAPI_ISteamNetworkingConnectionCustomSignaling_Release_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4260]
SteamAPI_ISteamNetworkingConnectionCustomSignaling_Release_proxy ENDP
SteamAPI_ISteamNetworkingConnectionCustomSignaling_SendSignal_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4264]
SteamAPI_ISteamNetworkingConnectionCustomSignaling_SendSignal_proxy ENDP
SteamAPI_ISteamNetworkingCustomSignalingRecvContext_OnConnectRequest_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4268]
SteamAPI_ISteamNetworkingCustomSignalingRecvContext_OnConnectRequest_proxy ENDP
SteamAPI_ISteamNetworkingCustomSignalingRecvContext_SendRejectionSignal_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4272]
SteamAPI_ISteamNetworkingCustomSignalingRecvContext_SendRejectionSignal_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetQuickConnectionStatus_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4276]
SteamAPI_ISteamNetworkingSockets_GetQuickConnectionStatus_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetFirstConfigValue_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4280]
SteamAPI_ISteamNetworkingUtils_GetFirstConfigValue_proxy ENDP
SteamAPI_ISteamTV_AddBroadcastGameData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4284]
SteamAPI_ISteamTV_AddBroadcastGameData_proxy ENDP
SteamAPI_ISteamTV_AddRegion_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4288]
SteamAPI_ISteamTV_AddRegion_proxy ENDP
SteamAPI_ISteamTV_AddTimelineMarker_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4292]
SteamAPI_ISteamTV_AddTimelineMarker_proxy ENDP
SteamAPI_ISteamTV_IsBroadcasting_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4296]
SteamAPI_ISteamTV_IsBroadcasting_proxy ENDP
SteamAPI_ISteamTV_RemoveBroadcastGameData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4300]
SteamAPI_ISteamTV_RemoveBroadcastGameData_proxy ENDP
SteamAPI_ISteamTV_RemoveRegion_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4304]
SteamAPI_ISteamTV_RemoveRegion_proxy ENDP
SteamAPI_ISteamTV_RemoveTimelineMarker_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4308]
SteamAPI_ISteamTV_RemoveTimelineMarker_proxy ENDP
SteamAPI_ISteamUser_InitiateGameConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4312]
SteamAPI_ISteamUser_InitiateGameConnection_proxy ENDP
SteamAPI_ISteamUser_TerminateGameConnection_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4316]
SteamAPI_ISteamUser_TerminateGameConnection_proxy ENDP
SteamAPI_ISteamUtils_GetCSERIPPort_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4320]
SteamAPI_ISteamUtils_GetCSERIPPort_proxy ENDP
SteamAPI_SteamController_v007_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4324]
SteamAPI_SteamController_v007_proxy ENDP
SteamAPI_SteamGameServerApps_v008_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4328]
SteamAPI_SteamGameServerApps_v008_proxy ENDP
SteamAPI_SteamGameServerNetworkingSockets_v008_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4332]
SteamAPI_SteamGameServerNetworkingSockets_v008_proxy ENDP
SteamAPI_SteamGameServerUGC_v014_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4336]
SteamAPI_SteamGameServerUGC_v014_proxy ENDP
SteamAPI_SteamGameServerUtils_v009_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4340]
SteamAPI_SteamGameServerUtils_v009_proxy ENDP
SteamAPI_SteamGameServer_v013_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4344]
SteamAPI_SteamGameServer_v013_proxy ENDP
SteamAPI_SteamInput_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4348]
SteamAPI_SteamInput_v001_proxy ENDP
SteamAPI_SteamNetworkingSockets_v008_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4352]
SteamAPI_SteamNetworkingSockets_v008_proxy ENDP
SteamAPI_SteamNetworkingUtils_v003_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4356]
SteamAPI_SteamNetworkingUtils_v003_proxy ENDP
SteamAPI_SteamRemoteStorage_v014_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4360]
SteamAPI_SteamRemoteStorage_v014_proxy ENDP
SteamAPI_SteamTV_v001_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4364]
SteamAPI_SteamTV_v001_proxy ENDP
SteamAPI_SteamUGC_v014_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4368]
SteamAPI_SteamUGC_v014_proxy ENDP
SteamAPI_SteamUserStats_v011_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4372]
SteamAPI_SteamUserStats_v011_proxy ENDP
SteamAPI_SteamUser_v020_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4376]
SteamAPI_SteamUser_v020_proxy ENDP
SteamAPI_SteamUtils_v009_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4380]
SteamAPI_SteamUtils_v009_proxy ENDP
SteamAPI_ISteamApps_GetBetaInfo_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4384]
SteamAPI_ISteamApps_GetBetaInfo_proxy ENDP
SteamAPI_ISteamApps_GetNumBetas_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4388]
SteamAPI_ISteamApps_GetNumBetas_proxy ENDP
SteamAPI_ISteamApps_SetActiveBeta_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4392]
SteamAPI_ISteamApps_SetActiveBeta_proxy ENDP
SteamAPI_ISteamApps_SetDlcContext_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4396]
SteamAPI_ISteamApps_SetDlcContext_proxy ENDP
SteamAPI_ISteamFriends_BHasEquippedProfileItem_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4400]
SteamAPI_ISteamFriends_BHasEquippedProfileItem_proxy ENDP
SteamAPI_ISteamFriends_GetProfileItemPropertyString_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4404]
SteamAPI_ISteamFriends_GetProfileItemPropertyString_proxy ENDP
SteamAPI_ISteamFriends_GetProfileItemPropertyUint_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4408]
SteamAPI_ISteamFriends_GetProfileItemPropertyUint_proxy ENDP
SteamAPI_ISteamFriends_RequestEquippedProfileItems_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4412]
SteamAPI_ISteamFriends_RequestEquippedProfileItems_proxy ENDP
SteamAPI_ISteamInput_SetDualSenseTriggerEffect_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4416]
SteamAPI_ISteamInput_SetDualSenseTriggerEffect_proxy ENDP
SteamAPI_ISteamRemotePlay_BStartRemotePlayTogether_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4420]
SteamAPI_ISteamRemotePlay_BStartRemotePlayTogether_proxy ENDP
SteamAPI_ISteamTimeline_AddGamePhaseTag_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4424]
SteamAPI_ISteamTimeline_AddGamePhaseTag_proxy ENDP
SteamAPI_ISteamTimeline_AddInstantaneousTimelineEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4428]
SteamAPI_ISteamTimeline_AddInstantaneousTimelineEvent_proxy ENDP
SteamAPI_ISteamTimeline_AddRangeTimelineEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4432]
SteamAPI_ISteamTimeline_AddRangeTimelineEvent_proxy ENDP
SteamAPI_ISteamTimeline_ClearTimelineTooltip_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4436]
SteamAPI_ISteamTimeline_ClearTimelineTooltip_proxy ENDP
SteamAPI_ISteamTimeline_DoesEventRecordingExist_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4440]
SteamAPI_ISteamTimeline_DoesEventRecordingExist_proxy ENDP
SteamAPI_ISteamTimeline_DoesGamePhaseRecordingExist_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4444]
SteamAPI_ISteamTimeline_DoesGamePhaseRecordingExist_proxy ENDP
SteamAPI_ISteamTimeline_EndGamePhase_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4448]
SteamAPI_ISteamTimeline_EndGamePhase_proxy ENDP
SteamAPI_ISteamTimeline_EndRangeTimelineEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4452]
SteamAPI_ISteamTimeline_EndRangeTimelineEvent_proxy ENDP
SteamAPI_ISteamTimeline_OpenOverlayToGamePhase_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4456]
SteamAPI_ISteamTimeline_OpenOverlayToGamePhase_proxy ENDP
SteamAPI_ISteamTimeline_OpenOverlayToTimelineEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4460]
SteamAPI_ISteamTimeline_OpenOverlayToTimelineEvent_proxy ENDP
SteamAPI_ISteamTimeline_RemoveTimelineEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4464]
SteamAPI_ISteamTimeline_RemoveTimelineEvent_proxy ENDP
SteamAPI_ISteamTimeline_SetGamePhaseAttribute_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4468]
SteamAPI_ISteamTimeline_SetGamePhaseAttribute_proxy ENDP
SteamAPI_ISteamTimeline_SetGamePhaseID_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4472]
SteamAPI_ISteamTimeline_SetGamePhaseID_proxy ENDP
SteamAPI_ISteamTimeline_SetTimelineGameMode_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4476]
SteamAPI_ISteamTimeline_SetTimelineGameMode_proxy ENDP
SteamAPI_ISteamTimeline_SetTimelineTooltip_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4480]
SteamAPI_ISteamTimeline_SetTimelineTooltip_proxy ENDP
SteamAPI_ISteamTimeline_StartGamePhase_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4484]
SteamAPI_ISteamTimeline_StartGamePhase_proxy ENDP
SteamAPI_ISteamTimeline_StartRangeTimelineEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4488]
SteamAPI_ISteamTimeline_StartRangeTimelineEvent_proxy ENDP
SteamAPI_ISteamTimeline_UpdateRangeTimelineEvent_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4492]
SteamAPI_ISteamTimeline_UpdateRangeTimelineEvent_proxy ENDP
SteamAPI_ISteamUGC_AddContentDescriptor_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4496]
SteamAPI_ISteamUGC_AddContentDescriptor_proxy ENDP
SteamAPI_ISteamUGC_GetNumSupportedGameVersions_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4500]
SteamAPI_ISteamUGC_GetNumSupportedGameVersions_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4504]
SteamAPI_ISteamUGC_GetQueryUGCContentDescriptors_proxy ENDP
SteamAPI_ISteamUGC_GetSupportedGameVersionData_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4508]
SteamAPI_ISteamUGC_GetSupportedGameVersionData_proxy ENDP
SteamAPI_ISteamUGC_GetUserContentDescriptorPreferences_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4512]
SteamAPI_ISteamUGC_GetUserContentDescriptorPreferences_proxy ENDP
SteamAPI_ISteamUGC_RemoveContentDescriptor_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4516]
SteamAPI_ISteamUGC_RemoveContentDescriptor_proxy ENDP
SteamAPI_ISteamUGC_SetAdminQuery_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4520]
SteamAPI_ISteamUGC_SetAdminQuery_proxy ENDP
SteamAPI_ISteamUGC_SetRequiredGameVersions_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4524]
SteamAPI_ISteamUGC_SetRequiredGameVersions_proxy ENDP
SteamAPI_ISteamUtils_DismissGamepadTextInput_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4528]
SteamAPI_ISteamUtils_DismissGamepadTextInput_proxy ENDP
SteamAPI_SteamGameServerUGC_v020_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4532]
SteamAPI_SteamGameServerUGC_v020_proxy ENDP
SteamAPI_SteamGameServer_v015_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4536]
SteamAPI_SteamGameServer_v015_proxy ENDP
SteamAPI_SteamRemotePlay_v002_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4540]
SteamAPI_SteamRemotePlay_v002_proxy ENDP
SteamAPI_SteamTimeline_v004_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4544]
SteamAPI_SteamTimeline_v004_proxy ENDP
SteamAPI_SteamUGC_v020_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4548]
SteamAPI_SteamUGC_v020_proxy ENDP
SteamAPI_SteamUserStats_v013_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4552]
SteamAPI_SteamUserStats_v013_proxy ENDP
SteamAPI_SteamUser_v023_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4556]
SteamAPI_SteamUser_v023_proxy ENDP
SteamAPI_SteamVideo_v007_proxy PROC
    jmp DWORD PTR [g_steamProcs + 4560]
SteamAPI_SteamVideo_v007_proxy ENDP

PUBLIC SafeUnsupportedExportStub
SafeUnsupportedExportStub PROC
    xor eax, eax
    ret
SafeUnsupportedExportStub ENDP

END
