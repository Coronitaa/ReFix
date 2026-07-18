; =============================================================================
; ReFix - steam_api64.dll x64 Forwarding Trampolines (AUTO-GENERATED)
; =============================================================================
; Forwards 1055 exports to steam_api64_valve.dll via jump table.
; 4 functions are intercepted in C++ code: SteamAPI_Init, SteamAPI_InitSafe, SteamAPI_InitAnonymousUser, SteamAPI_RestartAppIfNecessary
; =============================================================================

.data
EXTERN g_steamProcs:QWORD

.code
GetHSteamPipe_proxy PROC
    jmp QWORD PTR [g_steamProcs + 0]
GetHSteamPipe_proxy ENDP
GetHSteamUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8]
GetHSteamUser_proxy ENDP
SteamAPI_GetHSteamPipe_proxy PROC
    jmp QWORD PTR [g_steamProcs + 16]
SteamAPI_GetHSteamPipe_proxy ENDP
SteamAPI_GetHSteamUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 24]
SteamAPI_GetHSteamUser_proxy ENDP
SteamAPI_GetSteamInstallPath_proxy PROC
    jmp QWORD PTR [g_steamProcs + 32]
SteamAPI_GetSteamInstallPath_proxy ENDP
SteamAPI_ISteamAppList_GetAppBuildId_proxy PROC
    jmp QWORD PTR [g_steamProcs + 40]
SteamAPI_ISteamAppList_GetAppBuildId_proxy ENDP
SteamAPI_ISteamAppList_GetAppInstallDir_proxy PROC
    jmp QWORD PTR [g_steamProcs + 48]
SteamAPI_ISteamAppList_GetAppInstallDir_proxy ENDP
SteamAPI_ISteamAppList_GetAppName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 56]
SteamAPI_ISteamAppList_GetAppName_proxy ENDP
SteamAPI_ISteamAppList_GetInstalledApps_proxy PROC
    jmp QWORD PTR [g_steamProcs + 64]
SteamAPI_ISteamAppList_GetInstalledApps_proxy ENDP
SteamAPI_ISteamAppList_GetNumInstalledApps_proxy PROC
    jmp QWORD PTR [g_steamProcs + 72]
SteamAPI_ISteamAppList_GetNumInstalledApps_proxy ENDP
SteamAPI_ISteamApps_BGetDLCDataByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 80]
SteamAPI_ISteamApps_BGetDLCDataByIndex_proxy ENDP
SteamAPI_ISteamApps_BIsAppInstalled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 88]
SteamAPI_ISteamApps_BIsAppInstalled_proxy ENDP
SteamAPI_ISteamApps_BIsCybercafe_proxy PROC
    jmp QWORD PTR [g_steamProcs + 96]
SteamAPI_ISteamApps_BIsCybercafe_proxy ENDP
SteamAPI_ISteamApps_BIsDlcInstalled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 104]
SteamAPI_ISteamApps_BIsDlcInstalled_proxy ENDP
SteamAPI_ISteamApps_BIsLowViolence_proxy PROC
    jmp QWORD PTR [g_steamProcs + 112]
SteamAPI_ISteamApps_BIsLowViolence_proxy ENDP
SteamAPI_ISteamApps_BIsSubscribed_proxy PROC
    jmp QWORD PTR [g_steamProcs + 120]
SteamAPI_ISteamApps_BIsSubscribed_proxy ENDP
SteamAPI_ISteamApps_BIsSubscribedApp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 128]
SteamAPI_ISteamApps_BIsSubscribedApp_proxy ENDP
SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing_proxy PROC
    jmp QWORD PTR [g_steamProcs + 136]
SteamAPI_ISteamApps_BIsSubscribedFromFamilySharing_proxy ENDP
SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend_proxy PROC
    jmp QWORD PTR [g_steamProcs + 144]
SteamAPI_ISteamApps_BIsSubscribedFromFreeWeekend_proxy ENDP
SteamAPI_ISteamApps_BIsTimedTrial_proxy PROC
    jmp QWORD PTR [g_steamProcs + 152]
SteamAPI_ISteamApps_BIsTimedTrial_proxy ENDP
SteamAPI_ISteamApps_BIsVACBanned_proxy PROC
    jmp QWORD PTR [g_steamProcs + 160]
SteamAPI_ISteamApps_BIsVACBanned_proxy ENDP
SteamAPI_ISteamApps_GetAppBuildId_proxy PROC
    jmp QWORD PTR [g_steamProcs + 168]
SteamAPI_ISteamApps_GetAppBuildId_proxy ENDP
SteamAPI_ISteamApps_GetAppInstallDir_proxy PROC
    jmp QWORD PTR [g_steamProcs + 176]
SteamAPI_ISteamApps_GetAppInstallDir_proxy ENDP
SteamAPI_ISteamApps_GetAppOwner_proxy PROC
    jmp QWORD PTR [g_steamProcs + 184]
SteamAPI_ISteamApps_GetAppOwner_proxy ENDP
SteamAPI_ISteamApps_GetAvailableGameLanguages_proxy PROC
    jmp QWORD PTR [g_steamProcs + 192]
SteamAPI_ISteamApps_GetAvailableGameLanguages_proxy ENDP
SteamAPI_ISteamApps_GetCurrentBetaName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 200]
SteamAPI_ISteamApps_GetCurrentBetaName_proxy ENDP
SteamAPI_ISteamApps_GetCurrentGameLanguage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 208]
SteamAPI_ISteamApps_GetCurrentGameLanguage_proxy ENDP
SteamAPI_ISteamApps_GetDLCCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 216]
SteamAPI_ISteamApps_GetDLCCount_proxy ENDP
SteamAPI_ISteamApps_GetDlcDownloadProgress_proxy PROC
    jmp QWORD PTR [g_steamProcs + 224]
SteamAPI_ISteamApps_GetDlcDownloadProgress_proxy ENDP
SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime_proxy PROC
    jmp QWORD PTR [g_steamProcs + 232]
SteamAPI_ISteamApps_GetEarliestPurchaseUnixTime_proxy ENDP
SteamAPI_ISteamApps_GetFileDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 240]
SteamAPI_ISteamApps_GetFileDetails_proxy ENDP
SteamAPI_ISteamApps_GetInstalledDepots_proxy PROC
    jmp QWORD PTR [g_steamProcs + 248]
SteamAPI_ISteamApps_GetInstalledDepots_proxy ENDP
SteamAPI_ISteamApps_GetLaunchCommandLine_proxy PROC
    jmp QWORD PTR [g_steamProcs + 256]
SteamAPI_ISteamApps_GetLaunchCommandLine_proxy ENDP
SteamAPI_ISteamApps_GetLaunchQueryParam_proxy PROC
    jmp QWORD PTR [g_steamProcs + 264]
SteamAPI_ISteamApps_GetLaunchQueryParam_proxy ENDP
SteamAPI_ISteamApps_InstallDLC_proxy PROC
    jmp QWORD PTR [g_steamProcs + 272]
SteamAPI_ISteamApps_InstallDLC_proxy ENDP
SteamAPI_ISteamApps_MarkContentCorrupt_proxy PROC
    jmp QWORD PTR [g_steamProcs + 280]
SteamAPI_ISteamApps_MarkContentCorrupt_proxy ENDP
SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys_proxy PROC
    jmp QWORD PTR [g_steamProcs + 288]
SteamAPI_ISteamApps_RequestAllProofOfPurchaseKeys_proxy ENDP
SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey_proxy PROC
    jmp QWORD PTR [g_steamProcs + 296]
SteamAPI_ISteamApps_RequestAppProofOfPurchaseKey_proxy ENDP
SteamAPI_ISteamApps_UninstallDLC_proxy PROC
    jmp QWORD PTR [g_steamProcs + 304]
SteamAPI_ISteamApps_UninstallDLC_proxy ENDP
SteamAPI_ISteamClient_BReleaseSteamPipe_proxy PROC
    jmp QWORD PTR [g_steamProcs + 312]
SteamAPI_ISteamClient_BReleaseSteamPipe_proxy ENDP
SteamAPI_ISteamClient_BShutdownIfAllPipesClosed_proxy PROC
    jmp QWORD PTR [g_steamProcs + 320]
SteamAPI_ISteamClient_BShutdownIfAllPipesClosed_proxy ENDP
SteamAPI_ISteamClient_ConnectToGlobalUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 328]
SteamAPI_ISteamClient_ConnectToGlobalUser_proxy ENDP
SteamAPI_ISteamClient_CreateLocalUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 336]
SteamAPI_ISteamClient_CreateLocalUser_proxy ENDP
SteamAPI_ISteamClient_CreateSteamPipe_proxy PROC
    jmp QWORD PTR [g_steamProcs + 344]
SteamAPI_ISteamClient_CreateSteamPipe_proxy ENDP
SteamAPI_ISteamClient_GetIPCCallCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 352]
SteamAPI_ISteamClient_GetIPCCallCount_proxy ENDP
SteamAPI_ISteamClient_GetISteamAppList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 360]
SteamAPI_ISteamClient_GetISteamAppList_proxy ENDP
SteamAPI_ISteamClient_GetISteamApps_proxy PROC
    jmp QWORD PTR [g_steamProcs + 368]
SteamAPI_ISteamClient_GetISteamApps_proxy ENDP
SteamAPI_ISteamClient_GetISteamController_proxy PROC
    jmp QWORD PTR [g_steamProcs + 376]
SteamAPI_ISteamClient_GetISteamController_proxy ENDP
SteamAPI_ISteamClient_GetISteamFriends_proxy PROC
    jmp QWORD PTR [g_steamProcs + 384]
SteamAPI_ISteamClient_GetISteamFriends_proxy ENDP
SteamAPI_ISteamClient_GetISteamGameSearch_proxy PROC
    jmp QWORD PTR [g_steamProcs + 392]
SteamAPI_ISteamClient_GetISteamGameSearch_proxy ENDP
SteamAPI_ISteamClient_GetISteamGameServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 400]
SteamAPI_ISteamClient_GetISteamGameServer_proxy ENDP
SteamAPI_ISteamClient_GetISteamGameServerStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 408]
SteamAPI_ISteamClient_GetISteamGameServerStats_proxy ENDP
SteamAPI_ISteamClient_GetISteamGenericInterface_proxy PROC
    jmp QWORD PTR [g_steamProcs + 416]
SteamAPI_ISteamClient_GetISteamGenericInterface_proxy ENDP
SteamAPI_ISteamClient_GetISteamHTMLSurface_proxy PROC
    jmp QWORD PTR [g_steamProcs + 424]
SteamAPI_ISteamClient_GetISteamHTMLSurface_proxy ENDP
SteamAPI_ISteamClient_GetISteamHTTP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 432]
SteamAPI_ISteamClient_GetISteamHTTP_proxy ENDP
SteamAPI_ISteamClient_GetISteamInput_proxy PROC
    jmp QWORD PTR [g_steamProcs + 440]
SteamAPI_ISteamClient_GetISteamInput_proxy ENDP
SteamAPI_ISteamClient_GetISteamInventory_proxy PROC
    jmp QWORD PTR [g_steamProcs + 448]
SteamAPI_ISteamClient_GetISteamInventory_proxy ENDP
SteamAPI_ISteamClient_GetISteamMatchmaking_proxy PROC
    jmp QWORD PTR [g_steamProcs + 456]
SteamAPI_ISteamClient_GetISteamMatchmaking_proxy ENDP
SteamAPI_ISteamClient_GetISteamMatchmakingServers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 464]
SteamAPI_ISteamClient_GetISteamMatchmakingServers_proxy ENDP
SteamAPI_ISteamClient_GetISteamMusic_proxy PROC
    jmp QWORD PTR [g_steamProcs + 472]
SteamAPI_ISteamClient_GetISteamMusic_proxy ENDP
SteamAPI_ISteamClient_GetISteamMusicRemote_proxy PROC
    jmp QWORD PTR [g_steamProcs + 480]
SteamAPI_ISteamClient_GetISteamMusicRemote_proxy ENDP
SteamAPI_ISteamClient_GetISteamNetworking_proxy PROC
    jmp QWORD PTR [g_steamProcs + 488]
SteamAPI_ISteamClient_GetISteamNetworking_proxy ENDP
SteamAPI_ISteamClient_GetISteamParentalSettings_proxy PROC
    jmp QWORD PTR [g_steamProcs + 496]
SteamAPI_ISteamClient_GetISteamParentalSettings_proxy ENDP
SteamAPI_ISteamClient_GetISteamParties_proxy PROC
    jmp QWORD PTR [g_steamProcs + 504]
SteamAPI_ISteamClient_GetISteamParties_proxy ENDP
SteamAPI_ISteamClient_GetISteamRemotePlay_proxy PROC
    jmp QWORD PTR [g_steamProcs + 512]
SteamAPI_ISteamClient_GetISteamRemotePlay_proxy ENDP
SteamAPI_ISteamClient_GetISteamRemoteStorage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 520]
SteamAPI_ISteamClient_GetISteamRemoteStorage_proxy ENDP
SteamAPI_ISteamClient_GetISteamScreenshots_proxy PROC
    jmp QWORD PTR [g_steamProcs + 528]
SteamAPI_ISteamClient_GetISteamScreenshots_proxy ENDP
SteamAPI_ISteamClient_GetISteamUGC_proxy PROC
    jmp QWORD PTR [g_steamProcs + 536]
SteamAPI_ISteamClient_GetISteamUGC_proxy ENDP
SteamAPI_ISteamClient_GetISteamUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 544]
SteamAPI_ISteamClient_GetISteamUser_proxy ENDP
SteamAPI_ISteamClient_GetISteamUserStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 552]
SteamAPI_ISteamClient_GetISteamUserStats_proxy ENDP
SteamAPI_ISteamClient_GetISteamUtils_proxy PROC
    jmp QWORD PTR [g_steamProcs + 560]
SteamAPI_ISteamClient_GetISteamUtils_proxy ENDP
SteamAPI_ISteamClient_GetISteamVideo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 568]
SteamAPI_ISteamClient_GetISteamVideo_proxy ENDP
SteamAPI_ISteamClient_ReleaseUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 576]
SteamAPI_ISteamClient_ReleaseUser_proxy ENDP
SteamAPI_ISteamClient_SetLocalIPBinding_proxy PROC
    jmp QWORD PTR [g_steamProcs + 584]
SteamAPI_ISteamClient_SetLocalIPBinding_proxy ENDP
SteamAPI_ISteamClient_SetWarningMessageHook_proxy PROC
    jmp QWORD PTR [g_steamProcs + 592]
SteamAPI_ISteamClient_SetWarningMessageHook_proxy ENDP
SteamAPI_ISteamController_ActivateActionSet_proxy PROC
    jmp QWORD PTR [g_steamProcs + 600]
SteamAPI_ISteamController_ActivateActionSet_proxy ENDP
SteamAPI_ISteamController_ActivateActionSetLayer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 608]
SteamAPI_ISteamController_ActivateActionSetLayer_proxy ENDP
SteamAPI_ISteamController_DeactivateActionSetLayer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 616]
SteamAPI_ISteamController_DeactivateActionSetLayer_proxy ENDP
SteamAPI_ISteamController_DeactivateAllActionSetLayers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 624]
SteamAPI_ISteamController_DeactivateAllActionSetLayers_proxy ENDP
SteamAPI_ISteamController_GetActionOriginFromXboxOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 632]
SteamAPI_ISteamController_GetActionOriginFromXboxOrigin_proxy ENDP
SteamAPI_ISteamController_GetActionSetHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 640]
SteamAPI_ISteamController_GetActionSetHandle_proxy ENDP
SteamAPI_ISteamController_GetActiveActionSetLayers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 648]
SteamAPI_ISteamController_GetActiveActionSetLayers_proxy ENDP
SteamAPI_ISteamController_GetAnalogActionData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 656]
SteamAPI_ISteamController_GetAnalogActionData_proxy ENDP
SteamAPI_ISteamController_GetAnalogActionHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 664]
SteamAPI_ISteamController_GetAnalogActionHandle_proxy ENDP
SteamAPI_ISteamController_GetAnalogActionOrigins_proxy PROC
    jmp QWORD PTR [g_steamProcs + 672]
SteamAPI_ISteamController_GetAnalogActionOrigins_proxy ENDP
SteamAPI_ISteamController_GetConnectedControllers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 680]
SteamAPI_ISteamController_GetConnectedControllers_proxy ENDP
SteamAPI_ISteamController_GetControllerBindingRevision_proxy PROC
    jmp QWORD PTR [g_steamProcs + 688]
SteamAPI_ISteamController_GetControllerBindingRevision_proxy ENDP
SteamAPI_ISteamController_GetControllerForGamepadIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 696]
SteamAPI_ISteamController_GetControllerForGamepadIndex_proxy ENDP
SteamAPI_ISteamController_GetCurrentActionSet_proxy PROC
    jmp QWORD PTR [g_steamProcs + 704]
SteamAPI_ISteamController_GetCurrentActionSet_proxy ENDP
SteamAPI_ISteamController_GetDigitalActionData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 712]
SteamAPI_ISteamController_GetDigitalActionData_proxy ENDP
SteamAPI_ISteamController_GetDigitalActionHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 720]
SteamAPI_ISteamController_GetDigitalActionHandle_proxy ENDP
SteamAPI_ISteamController_GetDigitalActionOrigins_proxy PROC
    jmp QWORD PTR [g_steamProcs + 728]
SteamAPI_ISteamController_GetDigitalActionOrigins_proxy ENDP
SteamAPI_ISteamController_GetGamepadIndexForController_proxy PROC
    jmp QWORD PTR [g_steamProcs + 736]
SteamAPI_ISteamController_GetGamepadIndexForController_proxy ENDP
SteamAPI_ISteamController_GetGlyphForActionOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 744]
SteamAPI_ISteamController_GetGlyphForActionOrigin_proxy ENDP
SteamAPI_ISteamController_GetGlyphForXboxOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 752]
SteamAPI_ISteamController_GetGlyphForXboxOrigin_proxy ENDP
SteamAPI_ISteamController_GetInputTypeForHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 760]
SteamAPI_ISteamController_GetInputTypeForHandle_proxy ENDP
SteamAPI_ISteamController_GetMotionData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 768]
SteamAPI_ISteamController_GetMotionData_proxy ENDP
SteamAPI_ISteamController_GetStringForActionOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 776]
SteamAPI_ISteamController_GetStringForActionOrigin_proxy ENDP
SteamAPI_ISteamController_GetStringForXboxOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 784]
SteamAPI_ISteamController_GetStringForXboxOrigin_proxy ENDP
SteamAPI_ISteamController_Init_proxy PROC
    jmp QWORD PTR [g_steamProcs + 792]
SteamAPI_ISteamController_Init_proxy ENDP
SteamAPI_ISteamController_RunFrame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 800]
SteamAPI_ISteamController_RunFrame_proxy ENDP
SteamAPI_ISteamController_SetLEDColor_proxy PROC
    jmp QWORD PTR [g_steamProcs + 808]
SteamAPI_ISteamController_SetLEDColor_proxy ENDP
SteamAPI_ISteamController_ShowBindingPanel_proxy PROC
    jmp QWORD PTR [g_steamProcs + 816]
SteamAPI_ISteamController_ShowBindingPanel_proxy ENDP
SteamAPI_ISteamController_Shutdown_proxy PROC
    jmp QWORD PTR [g_steamProcs + 824]
SteamAPI_ISteamController_Shutdown_proxy ENDP
SteamAPI_ISteamController_StopAnalogActionMomentum_proxy PROC
    jmp QWORD PTR [g_steamProcs + 832]
SteamAPI_ISteamController_StopAnalogActionMomentum_proxy ENDP
SteamAPI_ISteamController_TranslateActionOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 840]
SteamAPI_ISteamController_TranslateActionOrigin_proxy ENDP
SteamAPI_ISteamController_TriggerHapticPulse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 848]
SteamAPI_ISteamController_TriggerHapticPulse_proxy ENDP
SteamAPI_ISteamController_TriggerRepeatedHapticPulse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 856]
SteamAPI_ISteamController_TriggerRepeatedHapticPulse_proxy ENDP
SteamAPI_ISteamController_TriggerVibration_proxy PROC
    jmp QWORD PTR [g_steamProcs + 864]
SteamAPI_ISteamController_TriggerVibration_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlay_proxy PROC
    jmp QWORD PTR [g_steamProcs + 872]
SteamAPI_ISteamFriends_ActivateGameOverlay_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog_proxy PROC
    jmp QWORD PTR [g_steamProcs + 880]
SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 888]
SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialogConnectString_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog_proxy PROC
    jmp QWORD PTR [g_steamProcs + 896]
SteamAPI_ISteamFriends_ActivateGameOverlayRemotePlayTogetherInviteDialog_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayToStore_proxy PROC
    jmp QWORD PTR [g_steamProcs + 904]
SteamAPI_ISteamFriends_ActivateGameOverlayToStore_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayToUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 912]
SteamAPI_ISteamFriends_ActivateGameOverlayToUser_proxy ENDP
SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 920]
SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage_proxy ENDP
SteamAPI_ISteamFriends_ClearRichPresence_proxy PROC
    jmp QWORD PTR [g_steamProcs + 928]
SteamAPI_ISteamFriends_ClearRichPresence_proxy ENDP
SteamAPI_ISteamFriends_CloseClanChatWindowInSteam_proxy PROC
    jmp QWORD PTR [g_steamProcs + 936]
SteamAPI_ISteamFriends_CloseClanChatWindowInSteam_proxy ENDP
SteamAPI_ISteamFriends_DownloadClanActivityCounts_proxy PROC
    jmp QWORD PTR [g_steamProcs + 944]
SteamAPI_ISteamFriends_DownloadClanActivityCounts_proxy ENDP
SteamAPI_ISteamFriends_EnumerateFollowingList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 952]
SteamAPI_ISteamFriends_EnumerateFollowingList_proxy ENDP
SteamAPI_ISteamFriends_GetChatMemberByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 960]
SteamAPI_ISteamFriends_GetChatMemberByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetClanActivityCounts_proxy PROC
    jmp QWORD PTR [g_steamProcs + 968]
SteamAPI_ISteamFriends_GetClanActivityCounts_proxy ENDP
SteamAPI_ISteamFriends_GetClanByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 976]
SteamAPI_ISteamFriends_GetClanByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetClanChatMemberCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 984]
SteamAPI_ISteamFriends_GetClanChatMemberCount_proxy ENDP
SteamAPI_ISteamFriends_GetClanChatMessage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 992]
SteamAPI_ISteamFriends_GetClanChatMessage_proxy ENDP
SteamAPI_ISteamFriends_GetClanCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1000]
SteamAPI_ISteamFriends_GetClanCount_proxy ENDP
SteamAPI_ISteamFriends_GetClanName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1008]
SteamAPI_ISteamFriends_GetClanName_proxy ENDP
SteamAPI_ISteamFriends_GetClanOfficerByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1016]
SteamAPI_ISteamFriends_GetClanOfficerByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetClanOfficerCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1024]
SteamAPI_ISteamFriends_GetClanOfficerCount_proxy ENDP
SteamAPI_ISteamFriends_GetClanOwner_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1032]
SteamAPI_ISteamFriends_GetClanOwner_proxy ENDP
SteamAPI_ISteamFriends_GetClanTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1040]
SteamAPI_ISteamFriends_GetClanTag_proxy ENDP
SteamAPI_ISteamFriends_GetCoplayFriend_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1048]
SteamAPI_ISteamFriends_GetCoplayFriend_proxy ENDP
SteamAPI_ISteamFriends_GetCoplayFriendCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1056]
SteamAPI_ISteamFriends_GetCoplayFriendCount_proxy ENDP
SteamAPI_ISteamFriends_GetFollowerCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1064]
SteamAPI_ISteamFriends_GetFollowerCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1072]
SteamAPI_ISteamFriends_GetFriendByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetFriendCoplayGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1080]
SteamAPI_ISteamFriends_GetFriendCoplayGame_proxy ENDP
SteamAPI_ISteamFriends_GetFriendCoplayTime_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1088]
SteamAPI_ISteamFriends_GetFriendCoplayTime_proxy ENDP
SteamAPI_ISteamFriends_GetFriendCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1096]
SteamAPI_ISteamFriends_GetFriendCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendCountFromSource_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1104]
SteamAPI_ISteamFriends_GetFriendCountFromSource_proxy ENDP
SteamAPI_ISteamFriends_GetFriendFromSourceByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1112]
SteamAPI_ISteamFriends_GetFriendFromSourceByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetFriendGamePlayed_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1120]
SteamAPI_ISteamFriends_GetFriendGamePlayed_proxy ENDP
SteamAPI_ISteamFriends_GetFriendMessage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1128]
SteamAPI_ISteamFriends_GetFriendMessage_proxy ENDP
SteamAPI_ISteamFriends_GetFriendPersonaName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1136]
SteamAPI_ISteamFriends_GetFriendPersonaName_proxy ENDP
SteamAPI_ISteamFriends_GetFriendPersonaNameHistory_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1144]
SteamAPI_ISteamFriends_GetFriendPersonaNameHistory_proxy ENDP
SteamAPI_ISteamFriends_GetFriendPersonaState_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1152]
SteamAPI_ISteamFriends_GetFriendPersonaState_proxy ENDP
SteamAPI_ISteamFriends_GetFriendRelationship_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1160]
SteamAPI_ISteamFriends_GetFriendRelationship_proxy ENDP
SteamAPI_ISteamFriends_GetFriendRichPresence_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1168]
SteamAPI_ISteamFriends_GetFriendRichPresence_proxy ENDP
SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1176]
SteamAPI_ISteamFriends_GetFriendRichPresenceKeyByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1184]
SteamAPI_ISteamFriends_GetFriendRichPresenceKeyCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendSteamLevel_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1192]
SteamAPI_ISteamFriends_GetFriendSteamLevel_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1200]
SteamAPI_ISteamFriends_GetFriendsGroupCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1208]
SteamAPI_ISteamFriends_GetFriendsGroupIDByIndex_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupMembersCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1216]
SteamAPI_ISteamFriends_GetFriendsGroupMembersCount_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupMembersList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1224]
SteamAPI_ISteamFriends_GetFriendsGroupMembersList_proxy ENDP
SteamAPI_ISteamFriends_GetFriendsGroupName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1232]
SteamAPI_ISteamFriends_GetFriendsGroupName_proxy ENDP
SteamAPI_ISteamFriends_GetLargeFriendAvatar_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1240]
SteamAPI_ISteamFriends_GetLargeFriendAvatar_proxy ENDP
SteamAPI_ISteamFriends_GetMediumFriendAvatar_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1248]
SteamAPI_ISteamFriends_GetMediumFriendAvatar_proxy ENDP
SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1256]
SteamAPI_ISteamFriends_GetNumChatsWithUnreadPriorityMessages_proxy ENDP
SteamAPI_ISteamFriends_GetPersonaName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1264]
SteamAPI_ISteamFriends_GetPersonaName_proxy ENDP
SteamAPI_ISteamFriends_GetPersonaState_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1272]
SteamAPI_ISteamFriends_GetPersonaState_proxy ENDP
SteamAPI_ISteamFriends_GetPlayerNickname_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1280]
SteamAPI_ISteamFriends_GetPlayerNickname_proxy ENDP
SteamAPI_ISteamFriends_GetSmallFriendAvatar_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1288]
SteamAPI_ISteamFriends_GetSmallFriendAvatar_proxy ENDP
SteamAPI_ISteamFriends_GetUserRestrictions_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1296]
SteamAPI_ISteamFriends_GetUserRestrictions_proxy ENDP
SteamAPI_ISteamFriends_HasFriend_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1304]
SteamAPI_ISteamFriends_HasFriend_proxy ENDP
SteamAPI_ISteamFriends_InviteUserToGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1312]
SteamAPI_ISteamFriends_InviteUserToGame_proxy ENDP
SteamAPI_ISteamFriends_IsClanChatAdmin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1320]
SteamAPI_ISteamFriends_IsClanChatAdmin_proxy ENDP
SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1328]
SteamAPI_ISteamFriends_IsClanChatWindowOpenInSteam_proxy ENDP
SteamAPI_ISteamFriends_IsClanOfficialGameGroup_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1336]
SteamAPI_ISteamFriends_IsClanOfficialGameGroup_proxy ENDP
SteamAPI_ISteamFriends_IsClanPublic_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1344]
SteamAPI_ISteamFriends_IsClanPublic_proxy ENDP
SteamAPI_ISteamFriends_IsFollowing_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1352]
SteamAPI_ISteamFriends_IsFollowing_proxy ENDP
SteamAPI_ISteamFriends_IsUserInSource_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1360]
SteamAPI_ISteamFriends_IsUserInSource_proxy ENDP
SteamAPI_ISteamFriends_JoinClanChatRoom_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1368]
SteamAPI_ISteamFriends_JoinClanChatRoom_proxy ENDP
SteamAPI_ISteamFriends_LeaveClanChatRoom_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1376]
SteamAPI_ISteamFriends_LeaveClanChatRoom_proxy ENDP
SteamAPI_ISteamFriends_OpenClanChatWindowInSteam_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1384]
SteamAPI_ISteamFriends_OpenClanChatWindowInSteam_proxy ENDP
SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1392]
SteamAPI_ISteamFriends_RegisterProtocolInOverlayBrowser_proxy ENDP
SteamAPI_ISteamFriends_ReplyToFriendMessage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1400]
SteamAPI_ISteamFriends_ReplyToFriendMessage_proxy ENDP
SteamAPI_ISteamFriends_RequestClanOfficerList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1408]
SteamAPI_ISteamFriends_RequestClanOfficerList_proxy ENDP
SteamAPI_ISteamFriends_RequestFriendRichPresence_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1416]
SteamAPI_ISteamFriends_RequestFriendRichPresence_proxy ENDP
SteamAPI_ISteamFriends_RequestUserInformation_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1424]
SteamAPI_ISteamFriends_RequestUserInformation_proxy ENDP
SteamAPI_ISteamFriends_SendClanChatMessage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1432]
SteamAPI_ISteamFriends_SendClanChatMessage_proxy ENDP
SteamAPI_ISteamFriends_SetInGameVoiceSpeaking_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1440]
SteamAPI_ISteamFriends_SetInGameVoiceSpeaking_proxy ENDP
SteamAPI_ISteamFriends_SetListenForFriendsMessages_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1448]
SteamAPI_ISteamFriends_SetListenForFriendsMessages_proxy ENDP
SteamAPI_ISteamFriends_SetPersonaName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1456]
SteamAPI_ISteamFriends_SetPersonaName_proxy ENDP
SteamAPI_ISteamFriends_SetPlayedWith_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1464]
SteamAPI_ISteamFriends_SetPlayedWith_proxy ENDP
SteamAPI_ISteamFriends_SetRichPresence_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1472]
SteamAPI_ISteamFriends_SetRichPresence_proxy ENDP
SteamAPI_ISteamGameSearch_AcceptGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1480]
SteamAPI_ISteamGameSearch_AcceptGame_proxy ENDP
SteamAPI_ISteamGameSearch_AddGameSearchParams_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1488]
SteamAPI_ISteamGameSearch_AddGameSearchParams_proxy ENDP
SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1496]
SteamAPI_ISteamGameSearch_CancelRequestPlayersForGame_proxy ENDP
SteamAPI_ISteamGameSearch_DeclineGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1504]
SteamAPI_ISteamGameSearch_DeclineGame_proxy ENDP
SteamAPI_ISteamGameSearch_EndGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1512]
SteamAPI_ISteamGameSearch_EndGame_proxy ENDP
SteamAPI_ISteamGameSearch_EndGameSearch_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1520]
SteamAPI_ISteamGameSearch_EndGameSearch_proxy ENDP
SteamAPI_ISteamGameSearch_HostConfirmGameStart_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1528]
SteamAPI_ISteamGameSearch_HostConfirmGameStart_proxy ENDP
SteamAPI_ISteamGameSearch_RequestPlayersForGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1536]
SteamAPI_ISteamGameSearch_RequestPlayersForGame_proxy ENDP
SteamAPI_ISteamGameSearch_RetrieveConnectionDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1544]
SteamAPI_ISteamGameSearch_RetrieveConnectionDetails_proxy ENDP
SteamAPI_ISteamGameSearch_SearchForGameSolo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1552]
SteamAPI_ISteamGameSearch_SearchForGameSolo_proxy ENDP
SteamAPI_ISteamGameSearch_SearchForGameWithLobby_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1560]
SteamAPI_ISteamGameSearch_SearchForGameWithLobby_proxy ENDP
SteamAPI_ISteamGameSearch_SetConnectionDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1568]
SteamAPI_ISteamGameSearch_SetConnectionDetails_proxy ENDP
SteamAPI_ISteamGameSearch_SetGameHostParams_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1576]
SteamAPI_ISteamGameSearch_SetGameHostParams_proxy ENDP
SteamAPI_ISteamGameSearch_SubmitPlayerResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1584]
SteamAPI_ISteamGameSearch_SubmitPlayerResult_proxy ENDP
SteamAPI_ISteamGameServerStats_ClearUserAchievement_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1592]
SteamAPI_ISteamGameServerStats_ClearUserAchievement_proxy ENDP
SteamAPI_ISteamGameServerStats_GetUserAchievement_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1600]
SteamAPI_ISteamGameServerStats_GetUserAchievement_proxy ENDP
SteamAPI_ISteamGameServerStats_GetUserStatFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1608]
SteamAPI_ISteamGameServerStats_GetUserStatFloat_proxy ENDP
SteamAPI_ISteamGameServerStats_GetUserStatInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1616]
SteamAPI_ISteamGameServerStats_GetUserStatInt32_proxy ENDP
SteamAPI_ISteamGameServerStats_RequestUserStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1624]
SteamAPI_ISteamGameServerStats_RequestUserStats_proxy ENDP
SteamAPI_ISteamGameServerStats_SetUserAchievement_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1632]
SteamAPI_ISteamGameServerStats_SetUserAchievement_proxy ENDP
SteamAPI_ISteamGameServerStats_SetUserStatFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1640]
SteamAPI_ISteamGameServerStats_SetUserStatFloat_proxy ENDP
SteamAPI_ISteamGameServerStats_SetUserStatInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1648]
SteamAPI_ISteamGameServerStats_SetUserStatInt32_proxy ENDP
SteamAPI_ISteamGameServerStats_StoreUserStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1656]
SteamAPI_ISteamGameServerStats_StoreUserStats_proxy ENDP
SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1664]
SteamAPI_ISteamGameServerStats_UpdateUserAvgRateStat_proxy ENDP
SteamAPI_ISteamGameServer_AssociateWithClan_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1672]
SteamAPI_ISteamGameServer_AssociateWithClan_proxy ENDP
SteamAPI_ISteamGameServer_BLoggedOn_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1680]
SteamAPI_ISteamGameServer_BLoggedOn_proxy ENDP
SteamAPI_ISteamGameServer_BSecure_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1688]
SteamAPI_ISteamGameServer_BSecure_proxy ENDP
SteamAPI_ISteamGameServer_BUpdateUserData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1696]
SteamAPI_ISteamGameServer_BUpdateUserData_proxy ENDP
SteamAPI_ISteamGameServer_BeginAuthSession_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1704]
SteamAPI_ISteamGameServer_BeginAuthSession_proxy ENDP
SteamAPI_ISteamGameServer_CancelAuthTicket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1712]
SteamAPI_ISteamGameServer_CancelAuthTicket_proxy ENDP
SteamAPI_ISteamGameServer_ClearAllKeyValues_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1720]
SteamAPI_ISteamGameServer_ClearAllKeyValues_proxy ENDP
SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1728]
SteamAPI_ISteamGameServer_ComputeNewPlayerCompatibility_proxy ENDP
SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1736]
SteamAPI_ISteamGameServer_CreateUnauthenticatedUserConnection_proxy ENDP
SteamAPI_ISteamGameServer_EndAuthSession_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1744]
SteamAPI_ISteamGameServer_EndAuthSession_proxy ENDP
SteamAPI_ISteamGameServer_GetAuthSessionTicket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1752]
SteamAPI_ISteamGameServer_GetAuthSessionTicket_proxy ENDP
SteamAPI_ISteamGameServer_GetGameplayStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1760]
SteamAPI_ISteamGameServer_GetGameplayStats_proxy ENDP
SteamAPI_ISteamGameServer_GetNextOutgoingPacket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1768]
SteamAPI_ISteamGameServer_GetNextOutgoingPacket_proxy ENDP
SteamAPI_ISteamGameServer_GetPublicIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1776]
SteamAPI_ISteamGameServer_GetPublicIP_proxy ENDP
SteamAPI_ISteamGameServer_GetServerReputation_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1784]
SteamAPI_ISteamGameServer_GetServerReputation_proxy ENDP
SteamAPI_ISteamGameServer_GetSteamID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1792]
SteamAPI_ISteamGameServer_GetSteamID_proxy ENDP
SteamAPI_ISteamGameServer_HandleIncomingPacket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1800]
SteamAPI_ISteamGameServer_HandleIncomingPacket_proxy ENDP
SteamAPI_ISteamGameServer_LogOff_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1808]
SteamAPI_ISteamGameServer_LogOff_proxy ENDP
SteamAPI_ISteamGameServer_LogOn_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1816]
SteamAPI_ISteamGameServer_LogOn_proxy ENDP
SteamAPI_ISteamGameServer_LogOnAnonymous_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1824]
SteamAPI_ISteamGameServer_LogOnAnonymous_proxy ENDP
SteamAPI_ISteamGameServer_RequestUserGroupStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1832]
SteamAPI_ISteamGameServer_RequestUserGroupStatus_proxy ENDP
SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1840]
SteamAPI_ISteamGameServer_SendUserConnectAndAuthenticate_DEPRECATED_proxy ENDP
SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1848]
SteamAPI_ISteamGameServer_SendUserDisconnect_DEPRECATED_proxy ENDP
SteamAPI_ISteamGameServer_SetAdvertiseServerActive_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1856]
SteamAPI_ISteamGameServer_SetAdvertiseServerActive_proxy ENDP
SteamAPI_ISteamGameServer_SetBotPlayerCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1864]
SteamAPI_ISteamGameServer_SetBotPlayerCount_proxy ENDP
SteamAPI_ISteamGameServer_SetDedicatedServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1872]
SteamAPI_ISteamGameServer_SetDedicatedServer_proxy ENDP
SteamAPI_ISteamGameServer_SetGameData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1880]
SteamAPI_ISteamGameServer_SetGameData_proxy ENDP
SteamAPI_ISteamGameServer_SetGameDescription_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1888]
SteamAPI_ISteamGameServer_SetGameDescription_proxy ENDP
SteamAPI_ISteamGameServer_SetGameTags_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1896]
SteamAPI_ISteamGameServer_SetGameTags_proxy ENDP
SteamAPI_ISteamGameServer_SetKeyValue_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1904]
SteamAPI_ISteamGameServer_SetKeyValue_proxy ENDP
SteamAPI_ISteamGameServer_SetMapName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1912]
SteamAPI_ISteamGameServer_SetMapName_proxy ENDP
SteamAPI_ISteamGameServer_SetMaxPlayerCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1920]
SteamAPI_ISteamGameServer_SetMaxPlayerCount_proxy ENDP
SteamAPI_ISteamGameServer_SetModDir_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1928]
SteamAPI_ISteamGameServer_SetModDir_proxy ENDP
SteamAPI_ISteamGameServer_SetPasswordProtected_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1936]
SteamAPI_ISteamGameServer_SetPasswordProtected_proxy ENDP
SteamAPI_ISteamGameServer_SetProduct_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1944]
SteamAPI_ISteamGameServer_SetProduct_proxy ENDP
SteamAPI_ISteamGameServer_SetRegion_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1952]
SteamAPI_ISteamGameServer_SetRegion_proxy ENDP
SteamAPI_ISteamGameServer_SetServerName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1960]
SteamAPI_ISteamGameServer_SetServerName_proxy ENDP
SteamAPI_ISteamGameServer_SetSpectatorPort_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1968]
SteamAPI_ISteamGameServer_SetSpectatorPort_proxy ENDP
SteamAPI_ISteamGameServer_SetSpectatorServerName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1976]
SteamAPI_ISteamGameServer_SetSpectatorServerName_proxy ENDP
SteamAPI_ISteamGameServer_UserHasLicenseForApp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1984]
SteamAPI_ISteamGameServer_UserHasLicenseForApp_proxy ENDP
SteamAPI_ISteamGameServer_WasRestartRequested_proxy PROC
    jmp QWORD PTR [g_steamProcs + 1992]
SteamAPI_ISteamGameServer_WasRestartRequested_proxy ENDP
SteamAPI_ISteamHTMLSurface_AddHeader_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2000]
SteamAPI_ISteamHTMLSurface_AddHeader_proxy ENDP
SteamAPI_ISteamHTMLSurface_AllowStartRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2008]
SteamAPI_ISteamHTMLSurface_AllowStartRequest_proxy ENDP
SteamAPI_ISteamHTMLSurface_CopyToClipboard_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2016]
SteamAPI_ISteamHTMLSurface_CopyToClipboard_proxy ENDP
SteamAPI_ISteamHTMLSurface_CreateBrowser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2024]
SteamAPI_ISteamHTMLSurface_CreateBrowser_proxy ENDP
SteamAPI_ISteamHTMLSurface_ExecuteJavascript_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2032]
SteamAPI_ISteamHTMLSurface_ExecuteJavascript_proxy ENDP
SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2040]
SteamAPI_ISteamHTMLSurface_FileLoadDialogResponse_proxy ENDP
SteamAPI_ISteamHTMLSurface_Find_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2048]
SteamAPI_ISteamHTMLSurface_Find_proxy ENDP
SteamAPI_ISteamHTMLSurface_GetLinkAtPosition_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2056]
SteamAPI_ISteamHTMLSurface_GetLinkAtPosition_proxy ENDP
SteamAPI_ISteamHTMLSurface_GoBack_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2064]
SteamAPI_ISteamHTMLSurface_GoBack_proxy ENDP
SteamAPI_ISteamHTMLSurface_GoForward_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2072]
SteamAPI_ISteamHTMLSurface_GoForward_proxy ENDP
SteamAPI_ISteamHTMLSurface_Init_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2080]
SteamAPI_ISteamHTMLSurface_Init_proxy ENDP
SteamAPI_ISteamHTMLSurface_JSDialogResponse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2088]
SteamAPI_ISteamHTMLSurface_JSDialogResponse_proxy ENDP
SteamAPI_ISteamHTMLSurface_KeyChar_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2096]
SteamAPI_ISteamHTMLSurface_KeyChar_proxy ENDP
SteamAPI_ISteamHTMLSurface_KeyDown_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2104]
SteamAPI_ISteamHTMLSurface_KeyDown_proxy ENDP
SteamAPI_ISteamHTMLSurface_KeyUp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2112]
SteamAPI_ISteamHTMLSurface_KeyUp_proxy ENDP
SteamAPI_ISteamHTMLSurface_LoadURL_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2120]
SteamAPI_ISteamHTMLSurface_LoadURL_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseDoubleClick_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2128]
SteamAPI_ISteamHTMLSurface_MouseDoubleClick_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseDown_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2136]
SteamAPI_ISteamHTMLSurface_MouseDown_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseMove_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2144]
SteamAPI_ISteamHTMLSurface_MouseMove_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseUp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2152]
SteamAPI_ISteamHTMLSurface_MouseUp_proxy ENDP
SteamAPI_ISteamHTMLSurface_MouseWheel_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2160]
SteamAPI_ISteamHTMLSurface_MouseWheel_proxy ENDP
SteamAPI_ISteamHTMLSurface_OpenDeveloperTools_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2168]
SteamAPI_ISteamHTMLSurface_OpenDeveloperTools_proxy ENDP
SteamAPI_ISteamHTMLSurface_PasteFromClipboard_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2176]
SteamAPI_ISteamHTMLSurface_PasteFromClipboard_proxy ENDP
SteamAPI_ISteamHTMLSurface_Reload_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2184]
SteamAPI_ISteamHTMLSurface_Reload_proxy ENDP
SteamAPI_ISteamHTMLSurface_RemoveBrowser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2192]
SteamAPI_ISteamHTMLSurface_RemoveBrowser_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetBackgroundMode_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2200]
SteamAPI_ISteamHTMLSurface_SetBackgroundMode_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetCookie_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2208]
SteamAPI_ISteamHTMLSurface_SetCookie_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2216]
SteamAPI_ISteamHTMLSurface_SetDPIScalingFactor_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetHorizontalScroll_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2224]
SteamAPI_ISteamHTMLSurface_SetHorizontalScroll_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetKeyFocus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2232]
SteamAPI_ISteamHTMLSurface_SetKeyFocus_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetPageScaleFactor_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2240]
SteamAPI_ISteamHTMLSurface_SetPageScaleFactor_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetSize_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2248]
SteamAPI_ISteamHTMLSurface_SetSize_proxy ENDP
SteamAPI_ISteamHTMLSurface_SetVerticalScroll_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2256]
SteamAPI_ISteamHTMLSurface_SetVerticalScroll_proxy ENDP
SteamAPI_ISteamHTMLSurface_Shutdown_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2264]
SteamAPI_ISteamHTMLSurface_Shutdown_proxy ENDP
SteamAPI_ISteamHTMLSurface_StopFind_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2272]
SteamAPI_ISteamHTMLSurface_StopFind_proxy ENDP
SteamAPI_ISteamHTMLSurface_StopLoad_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2280]
SteamAPI_ISteamHTMLSurface_StopLoad_proxy ENDP
SteamAPI_ISteamHTMLSurface_ViewSource_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2288]
SteamAPI_ISteamHTMLSurface_ViewSource_proxy ENDP
SteamAPI_ISteamHTTP_CreateCookieContainer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2296]
SteamAPI_ISteamHTTP_CreateCookieContainer_proxy ENDP
SteamAPI_ISteamHTTP_CreateHTTPRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2304]
SteamAPI_ISteamHTTP_CreateHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_DeferHTTPRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2312]
SteamAPI_ISteamHTTP_DeferHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2320]
SteamAPI_ISteamHTTP_GetHTTPDownloadProgressPct_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2328]
SteamAPI_ISteamHTTP_GetHTTPRequestWasTimedOut_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPResponseBodyData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2336]
SteamAPI_ISteamHTTP_GetHTTPResponseBodyData_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPResponseBodySize_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2344]
SteamAPI_ISteamHTTP_GetHTTPResponseBodySize_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2352]
SteamAPI_ISteamHTTP_GetHTTPResponseHeaderSize_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2360]
SteamAPI_ISteamHTTP_GetHTTPResponseHeaderValue_proxy ENDP
SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2368]
SteamAPI_ISteamHTTP_GetHTTPStreamingResponseBodyData_proxy ENDP
SteamAPI_ISteamHTTP_PrioritizeHTTPRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2376]
SteamAPI_ISteamHTTP_PrioritizeHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_ReleaseCookieContainer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2384]
SteamAPI_ISteamHTTP_ReleaseCookieContainer_proxy ENDP
SteamAPI_ISteamHTTP_ReleaseHTTPRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2392]
SteamAPI_ISteamHTTP_ReleaseHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_SendHTTPRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2400]
SteamAPI_ISteamHTTP_SendHTTPRequest_proxy ENDP
SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2408]
SteamAPI_ISteamHTTP_SendHTTPRequestAndStreamResponse_proxy ENDP
SteamAPI_ISteamHTTP_SetCookie_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2416]
SteamAPI_ISteamHTTP_SetCookie_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2424]
SteamAPI_ISteamHTTP_SetHTTPRequestAbsoluteTimeoutMS_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestContextValue_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2432]
SteamAPI_ISteamHTTP_SetHTTPRequestContextValue_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2440]
SteamAPI_ISteamHTTP_SetHTTPRequestCookieContainer_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2448]
SteamAPI_ISteamHTTP_SetHTTPRequestGetOrPostParameter_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2456]
SteamAPI_ISteamHTTP_SetHTTPRequestHeaderValue_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2464]
SteamAPI_ISteamHTTP_SetHTTPRequestNetworkActivityTimeout_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2472]
SteamAPI_ISteamHTTP_SetHTTPRequestRawPostBody_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2480]
SteamAPI_ISteamHTTP_SetHTTPRequestRequiresVerifiedCertificate_proxy ENDP
SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2488]
SteamAPI_ISteamHTTP_SetHTTPRequestUserAgentInfo_proxy ENDP
SteamAPI_ISteamInput_ActivateActionSet_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2496]
SteamAPI_ISteamInput_ActivateActionSet_proxy ENDP
SteamAPI_ISteamInput_ActivateActionSetLayer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2504]
SteamAPI_ISteamInput_ActivateActionSetLayer_proxy ENDP
SteamAPI_ISteamInput_BNewDataAvailable_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2512]
SteamAPI_ISteamInput_BNewDataAvailable_proxy ENDP
SteamAPI_ISteamInput_BWaitForData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2520]
SteamAPI_ISteamInput_BWaitForData_proxy ENDP
SteamAPI_ISteamInput_DeactivateActionSetLayer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2528]
SteamAPI_ISteamInput_DeactivateActionSetLayer_proxy ENDP
SteamAPI_ISteamInput_DeactivateAllActionSetLayers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2536]
SteamAPI_ISteamInput_DeactivateAllActionSetLayers_proxy ENDP
SteamAPI_ISteamInput_EnableActionEventCallbacks_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2544]
SteamAPI_ISteamInput_EnableActionEventCallbacks_proxy ENDP
SteamAPI_ISteamInput_EnableDeviceCallbacks_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2552]
SteamAPI_ISteamInput_EnableDeviceCallbacks_proxy ENDP
SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2560]
SteamAPI_ISteamInput_GetActionOriginFromXboxOrigin_proxy ENDP
SteamAPI_ISteamInput_GetActionSetHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2568]
SteamAPI_ISteamInput_GetActionSetHandle_proxy ENDP
SteamAPI_ISteamInput_GetActiveActionSetLayers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2576]
SteamAPI_ISteamInput_GetActiveActionSetLayers_proxy ENDP
SteamAPI_ISteamInput_GetAnalogActionData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2584]
SteamAPI_ISteamInput_GetAnalogActionData_proxy ENDP
SteamAPI_ISteamInput_GetAnalogActionHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2592]
SteamAPI_ISteamInput_GetAnalogActionHandle_proxy ENDP
SteamAPI_ISteamInput_GetAnalogActionOrigins_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2600]
SteamAPI_ISteamInput_GetAnalogActionOrigins_proxy ENDP
SteamAPI_ISteamInput_GetConnectedControllers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2608]
SteamAPI_ISteamInput_GetConnectedControllers_proxy ENDP
SteamAPI_ISteamInput_GetControllerForGamepadIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2616]
SteamAPI_ISteamInput_GetControllerForGamepadIndex_proxy ENDP
SteamAPI_ISteamInput_GetCurrentActionSet_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2624]
SteamAPI_ISteamInput_GetCurrentActionSet_proxy ENDP
SteamAPI_ISteamInput_GetDeviceBindingRevision_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2632]
SteamAPI_ISteamInput_GetDeviceBindingRevision_proxy ENDP
SteamAPI_ISteamInput_GetDigitalActionData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2640]
SteamAPI_ISteamInput_GetDigitalActionData_proxy ENDP
SteamAPI_ISteamInput_GetDigitalActionHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2648]
SteamAPI_ISteamInput_GetDigitalActionHandle_proxy ENDP
SteamAPI_ISteamInput_GetDigitalActionOrigins_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2656]
SteamAPI_ISteamInput_GetDigitalActionOrigins_proxy ENDP
SteamAPI_ISteamInput_GetGamepadIndexForController_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2664]
SteamAPI_ISteamInput_GetGamepadIndexForController_proxy ENDP
SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2672]
SteamAPI_ISteamInput_GetGlyphForActionOrigin_Legacy_proxy ENDP
SteamAPI_ISteamInput_GetGlyphForXboxOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2680]
SteamAPI_ISteamInput_GetGlyphForXboxOrigin_proxy ENDP
SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2688]
SteamAPI_ISteamInput_GetGlyphPNGForActionOrigin_proxy ENDP
SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2696]
SteamAPI_ISteamInput_GetGlyphSVGForActionOrigin_proxy ENDP
SteamAPI_ISteamInput_GetInputTypeForHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2704]
SteamAPI_ISteamInput_GetInputTypeForHandle_proxy ENDP
SteamAPI_ISteamInput_GetMotionData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2712]
SteamAPI_ISteamInput_GetMotionData_proxy ENDP
SteamAPI_ISteamInput_GetRemotePlaySessionID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2720]
SteamAPI_ISteamInput_GetRemotePlaySessionID_proxy ENDP
SteamAPI_ISteamInput_GetSessionInputConfigurationSettings_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2728]
SteamAPI_ISteamInput_GetSessionInputConfigurationSettings_proxy ENDP
SteamAPI_ISteamInput_GetStringForActionOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2736]
SteamAPI_ISteamInput_GetStringForActionOrigin_proxy ENDP
SteamAPI_ISteamInput_GetStringForAnalogActionName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2744]
SteamAPI_ISteamInput_GetStringForAnalogActionName_proxy ENDP
SteamAPI_ISteamInput_GetStringForDigitalActionName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2752]
SteamAPI_ISteamInput_GetStringForDigitalActionName_proxy ENDP
SteamAPI_ISteamInput_GetStringForXboxOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2760]
SteamAPI_ISteamInput_GetStringForXboxOrigin_proxy ENDP
SteamAPI_ISteamInput_Init_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2768]
SteamAPI_ISteamInput_Init_proxy ENDP
SteamAPI_ISteamInput_Legacy_TriggerHapticPulse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2776]
SteamAPI_ISteamInput_Legacy_TriggerHapticPulse_proxy ENDP
SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2784]
SteamAPI_ISteamInput_Legacy_TriggerRepeatedHapticPulse_proxy ENDP
SteamAPI_ISteamInput_RunFrame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2792]
SteamAPI_ISteamInput_RunFrame_proxy ENDP
SteamAPI_ISteamInput_SetInputActionManifestFilePath_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2800]
SteamAPI_ISteamInput_SetInputActionManifestFilePath_proxy ENDP
SteamAPI_ISteamInput_SetLEDColor_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2808]
SteamAPI_ISteamInput_SetLEDColor_proxy ENDP
SteamAPI_ISteamInput_ShowBindingPanel_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2816]
SteamAPI_ISteamInput_ShowBindingPanel_proxy ENDP
SteamAPI_ISteamInput_Shutdown_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2824]
SteamAPI_ISteamInput_Shutdown_proxy ENDP
SteamAPI_ISteamInput_StopAnalogActionMomentum_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2832]
SteamAPI_ISteamInput_StopAnalogActionMomentum_proxy ENDP
SteamAPI_ISteamInput_TranslateActionOrigin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2840]
SteamAPI_ISteamInput_TranslateActionOrigin_proxy ENDP
SteamAPI_ISteamInput_TriggerSimpleHapticEvent_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2848]
SteamAPI_ISteamInput_TriggerSimpleHapticEvent_proxy ENDP
SteamAPI_ISteamInput_TriggerVibration_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2856]
SteamAPI_ISteamInput_TriggerVibration_proxy ENDP
SteamAPI_ISteamInput_TriggerVibrationExtended_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2864]
SteamAPI_ISteamInput_TriggerVibrationExtended_proxy ENDP
SteamAPI_ISteamInventory_AddPromoItem_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2872]
SteamAPI_ISteamInventory_AddPromoItem_proxy ENDP
SteamAPI_ISteamInventory_AddPromoItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2880]
SteamAPI_ISteamInventory_AddPromoItems_proxy ENDP
SteamAPI_ISteamInventory_CheckResultSteamID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2888]
SteamAPI_ISteamInventory_CheckResultSteamID_proxy ENDP
SteamAPI_ISteamInventory_ConsumeItem_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2896]
SteamAPI_ISteamInventory_ConsumeItem_proxy ENDP
SteamAPI_ISteamInventory_DeserializeResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2904]
SteamAPI_ISteamInventory_DeserializeResult_proxy ENDP
SteamAPI_ISteamInventory_DestroyResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2912]
SteamAPI_ISteamInventory_DestroyResult_proxy ENDP
SteamAPI_ISteamInventory_ExchangeItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2920]
SteamAPI_ISteamInventory_ExchangeItems_proxy ENDP
SteamAPI_ISteamInventory_GenerateItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2928]
SteamAPI_ISteamInventory_GenerateItems_proxy ENDP
SteamAPI_ISteamInventory_GetAllItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2936]
SteamAPI_ISteamInventory_GetAllItems_proxy ENDP
SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2944]
SteamAPI_ISteamInventory_GetEligiblePromoItemDefinitionIDs_proxy ENDP
SteamAPI_ISteamInventory_GetItemDefinitionIDs_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2952]
SteamAPI_ISteamInventory_GetItemDefinitionIDs_proxy ENDP
SteamAPI_ISteamInventory_GetItemDefinitionProperty_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2960]
SteamAPI_ISteamInventory_GetItemDefinitionProperty_proxy ENDP
SteamAPI_ISteamInventory_GetItemPrice_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2968]
SteamAPI_ISteamInventory_GetItemPrice_proxy ENDP
SteamAPI_ISteamInventory_GetItemsByID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2976]
SteamAPI_ISteamInventory_GetItemsByID_proxy ENDP
SteamAPI_ISteamInventory_GetItemsWithPrices_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2984]
SteamAPI_ISteamInventory_GetItemsWithPrices_proxy ENDP
SteamAPI_ISteamInventory_GetNumItemsWithPrices_proxy PROC
    jmp QWORD PTR [g_steamProcs + 2992]
SteamAPI_ISteamInventory_GetNumItemsWithPrices_proxy ENDP
SteamAPI_ISteamInventory_GetResultItemProperty_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3000]
SteamAPI_ISteamInventory_GetResultItemProperty_proxy ENDP
SteamAPI_ISteamInventory_GetResultItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3008]
SteamAPI_ISteamInventory_GetResultItems_proxy ENDP
SteamAPI_ISteamInventory_GetResultStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3016]
SteamAPI_ISteamInventory_GetResultStatus_proxy ENDP
SteamAPI_ISteamInventory_GetResultTimestamp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3024]
SteamAPI_ISteamInventory_GetResultTimestamp_proxy ENDP
SteamAPI_ISteamInventory_GrantPromoItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3032]
SteamAPI_ISteamInventory_GrantPromoItems_proxy ENDP
SteamAPI_ISteamInventory_InspectItem_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3040]
SteamAPI_ISteamInventory_InspectItem_proxy ENDP
SteamAPI_ISteamInventory_LoadItemDefinitions_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3048]
SteamAPI_ISteamInventory_LoadItemDefinitions_proxy ENDP
SteamAPI_ISteamInventory_RemoveProperty_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3056]
SteamAPI_ISteamInventory_RemoveProperty_proxy ENDP
SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3064]
SteamAPI_ISteamInventory_RequestEligiblePromoItemDefinitionsIDs_proxy ENDP
SteamAPI_ISteamInventory_RequestPrices_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3072]
SteamAPI_ISteamInventory_RequestPrices_proxy ENDP
SteamAPI_ISteamInventory_SendItemDropHeartbeat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3080]
SteamAPI_ISteamInventory_SendItemDropHeartbeat_proxy ENDP
SteamAPI_ISteamInventory_SerializeResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3088]
SteamAPI_ISteamInventory_SerializeResult_proxy ENDP
SteamAPI_ISteamInventory_SetPropertyBool_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3096]
SteamAPI_ISteamInventory_SetPropertyBool_proxy ENDP
SteamAPI_ISteamInventory_SetPropertyFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3104]
SteamAPI_ISteamInventory_SetPropertyFloat_proxy ENDP
SteamAPI_ISteamInventory_SetPropertyInt64_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3112]
SteamAPI_ISteamInventory_SetPropertyInt64_proxy ENDP
SteamAPI_ISteamInventory_SetPropertyString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3120]
SteamAPI_ISteamInventory_SetPropertyString_proxy ENDP
SteamAPI_ISteamInventory_StartPurchase_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3128]
SteamAPI_ISteamInventory_StartPurchase_proxy ENDP
SteamAPI_ISteamInventory_StartUpdateProperties_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3136]
SteamAPI_ISteamInventory_StartUpdateProperties_proxy ENDP
SteamAPI_ISteamInventory_SubmitUpdateProperties_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3144]
SteamAPI_ISteamInventory_SubmitUpdateProperties_proxy ENDP
SteamAPI_ISteamInventory_TradeItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3152]
SteamAPI_ISteamInventory_TradeItems_proxy ENDP
SteamAPI_ISteamInventory_TransferItemQuantity_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3160]
SteamAPI_ISteamInventory_TransferItemQuantity_proxy ENDP
SteamAPI_ISteamInventory_TriggerItemDrop_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3168]
SteamAPI_ISteamInventory_TriggerItemDrop_proxy ENDP
SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3176]
SteamAPI_ISteamMatchmakingPingResponse_ServerFailedToRespond_proxy ENDP
SteamAPI_ISteamMatchmakingPingResponse_ServerResponded_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3184]
SteamAPI_ISteamMatchmakingPingResponse_ServerResponded_proxy ENDP
SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3192]
SteamAPI_ISteamMatchmakingPlayersResponse_AddPlayerToList_proxy ENDP
SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3200]
SteamAPI_ISteamMatchmakingPlayersResponse_PlayersFailedToRespond_proxy ENDP
SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3208]
SteamAPI_ISteamMatchmakingPlayersResponse_PlayersRefreshComplete_proxy ENDP
SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3216]
SteamAPI_ISteamMatchmakingRulesResponse_RulesFailedToRespond_proxy ENDP
SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3224]
SteamAPI_ISteamMatchmakingRulesResponse_RulesRefreshComplete_proxy ENDP
SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3232]
SteamAPI_ISteamMatchmakingRulesResponse_RulesResponded_proxy ENDP
SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3240]
SteamAPI_ISteamMatchmakingServerListResponse_RefreshComplete_proxy ENDP
SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3248]
SteamAPI_ISteamMatchmakingServerListResponse_ServerFailedToRespond_proxy ENDP
SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3256]
SteamAPI_ISteamMatchmakingServerListResponse_ServerResponded_proxy ENDP
SteamAPI_ISteamMatchmakingServers_CancelQuery_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3264]
SteamAPI_ISteamMatchmakingServers_CancelQuery_proxy ENDP
SteamAPI_ISteamMatchmakingServers_CancelServerQuery_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3272]
SteamAPI_ISteamMatchmakingServers_CancelServerQuery_proxy ENDP
SteamAPI_ISteamMatchmakingServers_GetServerCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3280]
SteamAPI_ISteamMatchmakingServers_GetServerCount_proxy ENDP
SteamAPI_ISteamMatchmakingServers_GetServerDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3288]
SteamAPI_ISteamMatchmakingServers_GetServerDetails_proxy ENDP
SteamAPI_ISteamMatchmakingServers_IsRefreshing_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3296]
SteamAPI_ISteamMatchmakingServers_IsRefreshing_proxy ENDP
SteamAPI_ISteamMatchmakingServers_PingServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3304]
SteamAPI_ISteamMatchmakingServers_PingServer_proxy ENDP
SteamAPI_ISteamMatchmakingServers_PlayerDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3312]
SteamAPI_ISteamMatchmakingServers_PlayerDetails_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RefreshQuery_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3320]
SteamAPI_ISteamMatchmakingServers_RefreshQuery_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RefreshServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3328]
SteamAPI_ISteamMatchmakingServers_RefreshServer_proxy ENDP
SteamAPI_ISteamMatchmakingServers_ReleaseRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3336]
SteamAPI_ISteamMatchmakingServers_ReleaseRequest_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3344]
SteamAPI_ISteamMatchmakingServers_RequestFavoritesServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3352]
SteamAPI_ISteamMatchmakingServers_RequestFriendsServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3360]
SteamAPI_ISteamMatchmakingServers_RequestHistoryServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestInternetServerList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3368]
SteamAPI_ISteamMatchmakingServers_RequestInternetServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestLANServerList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3376]
SteamAPI_ISteamMatchmakingServers_RequestLANServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3384]
SteamAPI_ISteamMatchmakingServers_RequestSpectatorServerList_proxy ENDP
SteamAPI_ISteamMatchmakingServers_ServerRules_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3392]
SteamAPI_ISteamMatchmakingServers_ServerRules_proxy ENDP
SteamAPI_ISteamMatchmaking_AddFavoriteGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3400]
SteamAPI_ISteamMatchmaking_AddFavoriteGame_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3408]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3416]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3424]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3432]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3440]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3448]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3456]
SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter_proxy ENDP
SteamAPI_ISteamMatchmaking_CreateLobby_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3464]
SteamAPI_ISteamMatchmaking_CreateLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_DeleteLobbyData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3472]
SteamAPI_ISteamMatchmaking_DeleteLobbyData_proxy ENDP
SteamAPI_ISteamMatchmaking_GetFavoriteGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3480]
SteamAPI_ISteamMatchmaking_GetFavoriteGame_proxy ENDP
SteamAPI_ISteamMatchmaking_GetFavoriteGameCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3488]
SteamAPI_ISteamMatchmaking_GetFavoriteGameCount_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3496]
SteamAPI_ISteamMatchmaking_GetLobbyByIndex_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyChatEntry_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3504]
SteamAPI_ISteamMatchmaking_GetLobbyChatEntry_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3512]
SteamAPI_ISteamMatchmaking_GetLobbyData_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3520]
SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyDataCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3528]
SteamAPI_ISteamMatchmaking_GetLobbyDataCount_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyGameServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3536]
SteamAPI_ISteamMatchmaking_GetLobbyGameServer_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3544]
SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyMemberData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3552]
SteamAPI_ISteamMatchmaking_GetLobbyMemberData_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3560]
SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit_proxy ENDP
SteamAPI_ISteamMatchmaking_GetLobbyOwner_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3568]
SteamAPI_ISteamMatchmaking_GetLobbyOwner_proxy ENDP
SteamAPI_ISteamMatchmaking_GetNumLobbyMembers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3576]
SteamAPI_ISteamMatchmaking_GetNumLobbyMembers_proxy ENDP
SteamAPI_ISteamMatchmaking_InviteUserToLobby_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3584]
SteamAPI_ISteamMatchmaking_InviteUserToLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_JoinLobby_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3592]
SteamAPI_ISteamMatchmaking_JoinLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_LeaveLobby_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3600]
SteamAPI_ISteamMatchmaking_LeaveLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_RemoveFavoriteGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3608]
SteamAPI_ISteamMatchmaking_RemoveFavoriteGame_proxy ENDP
SteamAPI_ISteamMatchmaking_RequestLobbyData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3616]
SteamAPI_ISteamMatchmaking_RequestLobbyData_proxy ENDP
SteamAPI_ISteamMatchmaking_RequestLobbyList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3624]
SteamAPI_ISteamMatchmaking_RequestLobbyList_proxy ENDP
SteamAPI_ISteamMatchmaking_SendLobbyChatMsg_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3632]
SteamAPI_ISteamMatchmaking_SendLobbyChatMsg_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLinkedLobby_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3640]
SteamAPI_ISteamMatchmaking_SetLinkedLobby_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3648]
SteamAPI_ISteamMatchmaking_SetLobbyData_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyGameServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3656]
SteamAPI_ISteamMatchmaking_SetLobbyGameServer_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyJoinable_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3664]
SteamAPI_ISteamMatchmaking_SetLobbyJoinable_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyMemberData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3672]
SteamAPI_ISteamMatchmaking_SetLobbyMemberData_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3680]
SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyOwner_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3688]
SteamAPI_ISteamMatchmaking_SetLobbyOwner_proxy ENDP
SteamAPI_ISteamMatchmaking_SetLobbyType_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3696]
SteamAPI_ISteamMatchmaking_SetLobbyType_proxy ENDP
SteamAPI_ISteamMusicRemote_BActivationSuccess_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3704]
SteamAPI_ISteamMusicRemote_BActivationSuccess_proxy ENDP
SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3712]
SteamAPI_ISteamMusicRemote_BIsCurrentMusicRemote_proxy ENDP
SteamAPI_ISteamMusicRemote_CurrentEntryDidChange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3720]
SteamAPI_ISteamMusicRemote_CurrentEntryDidChange_proxy ENDP
SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3728]
SteamAPI_ISteamMusicRemote_CurrentEntryIsAvailable_proxy ENDP
SteamAPI_ISteamMusicRemote_CurrentEntryWillChange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3736]
SteamAPI_ISteamMusicRemote_CurrentEntryWillChange_proxy ENDP
SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3744]
SteamAPI_ISteamMusicRemote_DeregisterSteamMusicRemote_proxy ENDP
SteamAPI_ISteamMusicRemote_EnableLooped_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3752]
SteamAPI_ISteamMusicRemote_EnableLooped_proxy ENDP
SteamAPI_ISteamMusicRemote_EnablePlayNext_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3760]
SteamAPI_ISteamMusicRemote_EnablePlayNext_proxy ENDP
SteamAPI_ISteamMusicRemote_EnablePlayPrevious_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3768]
SteamAPI_ISteamMusicRemote_EnablePlayPrevious_proxy ENDP
SteamAPI_ISteamMusicRemote_EnablePlaylists_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3776]
SteamAPI_ISteamMusicRemote_EnablePlaylists_proxy ENDP
SteamAPI_ISteamMusicRemote_EnableQueue_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3784]
SteamAPI_ISteamMusicRemote_EnableQueue_proxy ENDP
SteamAPI_ISteamMusicRemote_EnableShuffled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3792]
SteamAPI_ISteamMusicRemote_EnableShuffled_proxy ENDP
SteamAPI_ISteamMusicRemote_PlaylistDidChange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3800]
SteamAPI_ISteamMusicRemote_PlaylistDidChange_proxy ENDP
SteamAPI_ISteamMusicRemote_PlaylistWillChange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3808]
SteamAPI_ISteamMusicRemote_PlaylistWillChange_proxy ENDP
SteamAPI_ISteamMusicRemote_QueueDidChange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3816]
SteamAPI_ISteamMusicRemote_QueueDidChange_proxy ENDP
SteamAPI_ISteamMusicRemote_QueueWillChange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3824]
SteamAPI_ISteamMusicRemote_QueueWillChange_proxy ENDP
SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3832]
SteamAPI_ISteamMusicRemote_RegisterSteamMusicRemote_proxy ENDP
SteamAPI_ISteamMusicRemote_ResetPlaylistEntries_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3840]
SteamAPI_ISteamMusicRemote_ResetPlaylistEntries_proxy ENDP
SteamAPI_ISteamMusicRemote_ResetQueueEntries_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3848]
SteamAPI_ISteamMusicRemote_ResetQueueEntries_proxy ENDP
SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3856]
SteamAPI_ISteamMusicRemote_SetCurrentPlaylistEntry_proxy ENDP
SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3864]
SteamAPI_ISteamMusicRemote_SetCurrentQueueEntry_proxy ENDP
SteamAPI_ISteamMusicRemote_SetDisplayName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3872]
SteamAPI_ISteamMusicRemote_SetDisplayName_proxy ENDP
SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3880]
SteamAPI_ISteamMusicRemote_SetPNGIcon_64x64_proxy ENDP
SteamAPI_ISteamMusicRemote_SetPlaylistEntry_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3888]
SteamAPI_ISteamMusicRemote_SetPlaylistEntry_proxy ENDP
SteamAPI_ISteamMusicRemote_SetQueueEntry_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3896]
SteamAPI_ISteamMusicRemote_SetQueueEntry_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3904]
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryCoverArt_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3912]
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryElapsedSeconds_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3920]
SteamAPI_ISteamMusicRemote_UpdateCurrentEntryText_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateLooped_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3928]
SteamAPI_ISteamMusicRemote_UpdateLooped_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3936]
SteamAPI_ISteamMusicRemote_UpdatePlaybackStatus_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateShuffled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3944]
SteamAPI_ISteamMusicRemote_UpdateShuffled_proxy ENDP
SteamAPI_ISteamMusicRemote_UpdateVolume_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3952]
SteamAPI_ISteamMusicRemote_UpdateVolume_proxy ENDP
SteamAPI_ISteamMusic_BIsEnabled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3960]
SteamAPI_ISteamMusic_BIsEnabled_proxy ENDP
SteamAPI_ISteamMusic_BIsPlaying_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3968]
SteamAPI_ISteamMusic_BIsPlaying_proxy ENDP
SteamAPI_ISteamMusic_GetPlaybackStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3976]
SteamAPI_ISteamMusic_GetPlaybackStatus_proxy ENDP
SteamAPI_ISteamMusic_GetVolume_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3984]
SteamAPI_ISteamMusic_GetVolume_proxy ENDP
SteamAPI_ISteamMusic_Pause_proxy PROC
    jmp QWORD PTR [g_steamProcs + 3992]
SteamAPI_ISteamMusic_Pause_proxy ENDP
SteamAPI_ISteamMusic_Play_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4000]
SteamAPI_ISteamMusic_Play_proxy ENDP
SteamAPI_ISteamMusic_PlayNext_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4008]
SteamAPI_ISteamMusic_PlayNext_proxy ENDP
SteamAPI_ISteamMusic_PlayPrevious_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4016]
SteamAPI_ISteamMusic_PlayPrevious_proxy ENDP
SteamAPI_ISteamMusic_SetVolume_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4024]
SteamAPI_ISteamMusic_SetVolume_proxy ENDP
SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4032]
SteamAPI_ISteamNetworkingFakeUDPPort_DestroyFakeUDPPort_proxy ENDP
SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4040]
SteamAPI_ISteamNetworkingFakeUDPPort_ReceiveMessages_proxy ENDP
SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4048]
SteamAPI_ISteamNetworkingFakeUDPPort_ScheduleCleanup_proxy ENDP
SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4056]
SteamAPI_ISteamNetworkingFakeUDPPort_SendMessageToFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4064]
SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser_proxy ENDP
SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4072]
SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser_proxy ENDP
SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4080]
SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser_proxy ENDP
SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4088]
SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo_proxy ENDP
SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4096]
SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel_proxy ENDP
SteamAPI_ISteamNetworkingMessages_SendMessageToUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4104]
SteamAPI_ISteamNetworkingMessages_SendMessageToUser_proxy ENDP
SteamAPI_ISteamNetworkingSockets_AcceptConnection_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4112]
SteamAPI_ISteamNetworkingSockets_AcceptConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4120]
SteamAPI_ISteamNetworkingSockets_BeginAsyncRequestFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CloseConnection_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4128]
SteamAPI_ISteamNetworkingSockets_CloseConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CloseListenSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4136]
SteamAPI_ISteamNetworkingSockets_CloseListenSocket_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4144]
SteamAPI_ISteamNetworkingSockets_ConfigureConnectionLanes_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4152]
SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConnectP2P_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4160]
SteamAPI_ISteamNetworkingSockets_ConnectP2P_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4168]
SteamAPI_ISteamNetworkingSockets_ConnectP2PCustomSignaling_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4176]
SteamAPI_ISteamNetworkingSockets_ConnectToHostedDedicatedServer_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4184]
SteamAPI_ISteamNetworkingSockets_CreateFakeUDPPort_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4192]
SteamAPI_ISteamNetworkingSockets_CreateHostedDedicatedServerListenSocket_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4200]
SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4208]
SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4216]
SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2PFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreatePollGroup_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4224]
SteamAPI_ISteamNetworkingSockets_CreatePollGroup_proxy ENDP
SteamAPI_ISteamNetworkingSockets_CreateSocketPair_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4232]
SteamAPI_ISteamNetworkingSockets_CreateSocketPair_proxy ENDP
SteamAPI_ISteamNetworkingSockets_DestroyPollGroup_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4240]
SteamAPI_ISteamNetworkingSockets_DestroyPollGroup_proxy ENDP
SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4248]
SteamAPI_ISteamNetworkingSockets_FindRelayAuthTicketForServer_proxy ENDP
SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4256]
SteamAPI_ISteamNetworkingSockets_FlushMessagesOnConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4264]
SteamAPI_ISteamNetworkingSockets_GetAuthenticationStatus_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetCertificateRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4272]
SteamAPI_ISteamNetworkingSockets_GetCertificateRequest_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetConnectionInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4280]
SteamAPI_ISteamNetworkingSockets_GetConnectionInfo_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetConnectionName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4288]
SteamAPI_ISteamNetworkingSockets_GetConnectionName_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4296]
SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetConnectionUserData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4304]
SteamAPI_ISteamNetworkingSockets_GetConnectionUserData_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4312]
SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetFakeIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4320]
SteamAPI_ISteamNetworkingSockets_GetFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4328]
SteamAPI_ISteamNetworkingSockets_GetGameCoordinatorServerLogin_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4336]
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerAddress_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4344]
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPOPID_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4352]
SteamAPI_ISteamNetworkingSockets_GetHostedDedicatedServerPort_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetIdentity_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4360]
SteamAPI_ISteamNetworkingSockets_GetIdentity_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4368]
SteamAPI_ISteamNetworkingSockets_GetListenSocketAddress_proxy ENDP
SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4376]
SteamAPI_ISteamNetworkingSockets_GetRemoteFakeIPForConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_InitAuthentication_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4384]
SteamAPI_ISteamNetworkingSockets_InitAuthentication_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4392]
SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4400]
SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4408]
SteamAPI_ISteamNetworkingSockets_ReceivedP2PCustomSignal_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4416]
SteamAPI_ISteamNetworkingSockets_ReceivedRelayAuthTicket_proxy ENDP
SteamAPI_ISteamNetworkingSockets_ResetIdentity_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4424]
SteamAPI_ISteamNetworkingSockets_ResetIdentity_proxy ENDP
SteamAPI_ISteamNetworkingSockets_RunCallbacks_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4432]
SteamAPI_ISteamNetworkingSockets_RunCallbacks_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SendMessageToConnection_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4440]
SteamAPI_ISteamNetworkingSockets_SendMessageToConnection_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SendMessages_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4448]
SteamAPI_ISteamNetworkingSockets_SendMessages_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SetCertificate_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4456]
SteamAPI_ISteamNetworkingSockets_SetCertificate_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SetConnectionName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4464]
SteamAPI_ISteamNetworkingSockets_SetConnectionName_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4472]
SteamAPI_ISteamNetworkingSockets_SetConnectionPollGroup_proxy ENDP
SteamAPI_ISteamNetworkingSockets_SetConnectionUserData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4480]
SteamAPI_ISteamNetworkingSockets_SetConnectionUserData_proxy ENDP
SteamAPI_ISteamNetworkingUtils_AllocateMessage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4488]
SteamAPI_ISteamNetworkingUtils_AllocateMessage_proxy ENDP
SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4496]
SteamAPI_ISteamNetworkingUtils_CheckPingDataUpToDate_proxy ENDP
SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4504]
SteamAPI_ISteamNetworkingUtils_ConvertPingLocationToString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4512]
SteamAPI_ISteamNetworkingUtils_EstimatePingTimeBetweenTwoLocations_proxy ENDP
SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4520]
SteamAPI_ISteamNetworkingUtils_EstimatePingTimeFromLocalHost_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetConfigValue_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4528]
SteamAPI_ISteamNetworkingUtils_GetConfigValue_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4536]
SteamAPI_ISteamNetworkingUtils_GetConfigValueInfo_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4544]
SteamAPI_ISteamNetworkingUtils_GetDirectPingToPOP_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4552]
SteamAPI_ISteamNetworkingUtils_GetIPv4FakeIPType_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4560]
SteamAPI_ISteamNetworkingUtils_GetLocalPingLocation_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4568]
SteamAPI_ISteamNetworkingUtils_GetLocalTimestamp_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetPOPCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4576]
SteamAPI_ISteamNetworkingUtils_GetPOPCount_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetPOPList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4584]
SteamAPI_ISteamNetworkingUtils_GetPOPList_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4592]
SteamAPI_ISteamNetworkingUtils_GetPingToDataCenter_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4600]
SteamAPI_ISteamNetworkingUtils_GetRealIdentityForFakeIP_proxy ENDP
SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4608]
SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus_proxy ENDP
SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4616]
SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess_proxy ENDP
SteamAPI_ISteamNetworkingUtils_IsFakeIPv4_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4624]
SteamAPI_ISteamNetworkingUtils_IsFakeIPv4_proxy ENDP
SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4632]
SteamAPI_ISteamNetworkingUtils_IterateGenericEditableConfigValues_proxy ENDP
SteamAPI_ISteamNetworkingUtils_ParsePingLocationString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4640]
SteamAPI_ISteamNetworkingUtils_ParsePingLocationString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConfigValue_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4648]
SteamAPI_ISteamNetworkingUtils_SetConfigValue_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4656]
SteamAPI_ISteamNetworkingUtils_SetConfigValueStruct_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4664]
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueFloat_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4672]
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueInt32_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4680]
SteamAPI_ISteamNetworkingUtils_SetConnectionConfigValueString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4688]
SteamAPI_ISteamNetworkingUtils_SetDebugOutputFunction_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4696]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_FakeIPResult_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4704]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4712]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4720]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4728]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4736]
SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4744]
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueFloat_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4752]
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4760]
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4768]
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4776]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_GetFakeIPType_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4784]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ParseString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4792]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIPAddr_ToString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4800]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ParseString_proxy ENDP
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4808]
SteamAPI_ISteamNetworkingUtils_SteamNetworkingIdentity_ToString_proxy ENDP
SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4816]
SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser_proxy ENDP
SteamAPI_ISteamNetworking_AllowP2PPacketRelay_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4824]
SteamAPI_ISteamNetworking_AllowP2PPacketRelay_proxy ENDP
SteamAPI_ISteamNetworking_CloseP2PChannelWithUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4832]
SteamAPI_ISteamNetworking_CloseP2PChannelWithUser_proxy ENDP
SteamAPI_ISteamNetworking_CloseP2PSessionWithUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4840]
SteamAPI_ISteamNetworking_CloseP2PSessionWithUser_proxy ENDP
SteamAPI_ISteamNetworking_CreateConnectionSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4848]
SteamAPI_ISteamNetworking_CreateConnectionSocket_proxy ENDP
SteamAPI_ISteamNetworking_CreateListenSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4856]
SteamAPI_ISteamNetworking_CreateListenSocket_proxy ENDP
SteamAPI_ISteamNetworking_CreateP2PConnectionSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4864]
SteamAPI_ISteamNetworking_CreateP2PConnectionSocket_proxy ENDP
SteamAPI_ISteamNetworking_DestroyListenSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4872]
SteamAPI_ISteamNetworking_DestroyListenSocket_proxy ENDP
SteamAPI_ISteamNetworking_DestroySocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4880]
SteamAPI_ISteamNetworking_DestroySocket_proxy ENDP
SteamAPI_ISteamNetworking_GetListenSocketInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4888]
SteamAPI_ISteamNetworking_GetListenSocketInfo_proxy ENDP
SteamAPI_ISteamNetworking_GetMaxPacketSize_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4896]
SteamAPI_ISteamNetworking_GetMaxPacketSize_proxy ENDP
SteamAPI_ISteamNetworking_GetP2PSessionState_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4904]
SteamAPI_ISteamNetworking_GetP2PSessionState_proxy ENDP
SteamAPI_ISteamNetworking_GetSocketConnectionType_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4912]
SteamAPI_ISteamNetworking_GetSocketConnectionType_proxy ENDP
SteamAPI_ISteamNetworking_GetSocketInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4920]
SteamAPI_ISteamNetworking_GetSocketInfo_proxy ENDP
SteamAPI_ISteamNetworking_IsDataAvailable_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4928]
SteamAPI_ISteamNetworking_IsDataAvailable_proxy ENDP
SteamAPI_ISteamNetworking_IsDataAvailableOnSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4936]
SteamAPI_ISteamNetworking_IsDataAvailableOnSocket_proxy ENDP
SteamAPI_ISteamNetworking_IsP2PPacketAvailable_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4944]
SteamAPI_ISteamNetworking_IsP2PPacketAvailable_proxy ENDP
SteamAPI_ISteamNetworking_ReadP2PPacket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4952]
SteamAPI_ISteamNetworking_ReadP2PPacket_proxy ENDP
SteamAPI_ISteamNetworking_RetrieveData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4960]
SteamAPI_ISteamNetworking_RetrieveData_proxy ENDP
SteamAPI_ISteamNetworking_RetrieveDataFromSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4968]
SteamAPI_ISteamNetworking_RetrieveDataFromSocket_proxy ENDP
SteamAPI_ISteamNetworking_SendDataOnSocket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4976]
SteamAPI_ISteamNetworking_SendDataOnSocket_proxy ENDP
SteamAPI_ISteamNetworking_SendP2PPacket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4984]
SteamAPI_ISteamNetworking_SendP2PPacket_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsAppBlocked_proxy PROC
    jmp QWORD PTR [g_steamProcs + 4992]
SteamAPI_ISteamParentalSettings_BIsAppBlocked_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsAppInBlockList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5000]
SteamAPI_ISteamParentalSettings_BIsAppInBlockList_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsFeatureBlocked_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5008]
SteamAPI_ISteamParentalSettings_BIsFeatureBlocked_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5016]
SteamAPI_ISteamParentalSettings_BIsFeatureInBlockList_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5024]
SteamAPI_ISteamParentalSettings_BIsParentalLockEnabled_proxy ENDP
SteamAPI_ISteamParentalSettings_BIsParentalLockLocked_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5032]
SteamAPI_ISteamParentalSettings_BIsParentalLockLocked_proxy ENDP
SteamAPI_ISteamParties_CancelReservation_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5040]
SteamAPI_ISteamParties_CancelReservation_proxy ENDP
SteamAPI_ISteamParties_ChangeNumOpenSlots_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5048]
SteamAPI_ISteamParties_ChangeNumOpenSlots_proxy ENDP
SteamAPI_ISteamParties_CreateBeacon_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5056]
SteamAPI_ISteamParties_CreateBeacon_proxy ENDP
SteamAPI_ISteamParties_DestroyBeacon_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5064]
SteamAPI_ISteamParties_DestroyBeacon_proxy ENDP
SteamAPI_ISteamParties_GetAvailableBeaconLocations_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5072]
SteamAPI_ISteamParties_GetAvailableBeaconLocations_proxy ENDP
SteamAPI_ISteamParties_GetBeaconByIndex_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5080]
SteamAPI_ISteamParties_GetBeaconByIndex_proxy ENDP
SteamAPI_ISteamParties_GetBeaconDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5088]
SteamAPI_ISteamParties_GetBeaconDetails_proxy ENDP
SteamAPI_ISteamParties_GetBeaconLocationData_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5096]
SteamAPI_ISteamParties_GetBeaconLocationData_proxy ENDP
SteamAPI_ISteamParties_GetNumActiveBeacons_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5104]
SteamAPI_ISteamParties_GetNumActiveBeacons_proxy ENDP
SteamAPI_ISteamParties_GetNumAvailableBeaconLocations_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5112]
SteamAPI_ISteamParties_GetNumAvailableBeaconLocations_proxy ENDP
SteamAPI_ISteamParties_JoinParty_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5120]
SteamAPI_ISteamParties_JoinParty_proxy ENDP
SteamAPI_ISteamParties_OnReservationCompleted_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5128]
SteamAPI_ISteamParties_OnReservationCompleted_proxy ENDP
SteamAPI_ISteamRemotePlay_BGetSessionClientResolution_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5136]
SteamAPI_ISteamRemotePlay_BGetSessionClientResolution_proxy ENDP
SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5144]
SteamAPI_ISteamRemotePlay_BSendRemotePlayTogetherInvite_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5152]
SteamAPI_ISteamRemotePlay_GetSessionClientFormFactor_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionClientName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5160]
SteamAPI_ISteamRemotePlay_GetSessionClientName_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5168]
SteamAPI_ISteamRemotePlay_GetSessionCount_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5176]
SteamAPI_ISteamRemotePlay_GetSessionID_proxy ENDP
SteamAPI_ISteamRemotePlay_GetSessionSteamID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5184]
SteamAPI_ISteamRemotePlay_GetSessionSteamID_proxy ENDP
SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5192]
SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch_proxy ENDP
SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5200]
SteamAPI_ISteamRemoteStorage_CommitPublishedFileUpdate_proxy ENDP
SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5208]
SteamAPI_ISteamRemoteStorage_CreatePublishedFileUpdateRequest_proxy ENDP
SteamAPI_ISteamRemoteStorage_DeletePublishedFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5216]
SteamAPI_ISteamRemoteStorage_DeletePublishedFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_EndFileWriteBatch_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5224]
SteamAPI_ISteamRemoteStorage_EndFileWriteBatch_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5232]
SteamAPI_ISteamRemoteStorage_EnumeratePublishedFilesByUserAction_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5240]
SteamAPI_ISteamRemoteStorage_EnumeratePublishedWorkshopFiles_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5248]
SteamAPI_ISteamRemoteStorage_EnumerateUserPublishedFiles_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5256]
SteamAPI_ISteamRemoteStorage_EnumerateUserSharedWorkshopFiles_proxy ENDP
SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5264]
SteamAPI_ISteamRemoteStorage_EnumerateUserSubscribedFiles_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileDelete_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5272]
SteamAPI_ISteamRemoteStorage_FileDelete_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileExists_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5280]
SteamAPI_ISteamRemoteStorage_FileExists_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileForget_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5288]
SteamAPI_ISteamRemoteStorage_FileForget_proxy ENDP
SteamAPI_ISteamRemoteStorage_FilePersisted_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5296]
SteamAPI_ISteamRemoteStorage_FilePersisted_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileRead_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5304]
SteamAPI_ISteamRemoteStorage_FileRead_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileReadAsync_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5312]
SteamAPI_ISteamRemoteStorage_FileReadAsync_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5320]
SteamAPI_ISteamRemoteStorage_FileReadAsyncComplete_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileShare_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5328]
SteamAPI_ISteamRemoteStorage_FileShare_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWrite_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5336]
SteamAPI_ISteamRemoteStorage_FileWrite_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteAsync_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5344]
SteamAPI_ISteamRemoteStorage_FileWriteAsync_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5352]
SteamAPI_ISteamRemoteStorage_FileWriteStreamCancel_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteStreamClose_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5360]
SteamAPI_ISteamRemoteStorage_FileWriteStreamClose_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5368]
SteamAPI_ISteamRemoteStorage_FileWriteStreamOpen_proxy ENDP
SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5376]
SteamAPI_ISteamRemoteStorage_FileWriteStreamWriteChunk_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetCachedUGCCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5384]
SteamAPI_ISteamRemoteStorage_GetCachedUGCCount_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5392]
SteamAPI_ISteamRemoteStorage_GetCachedUGCHandle_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetFileCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5400]
SteamAPI_ISteamRemoteStorage_GetFileCount_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetFileNameAndSize_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5408]
SteamAPI_ISteamRemoteStorage_GetFileNameAndSize_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetFileSize_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5416]
SteamAPI_ISteamRemoteStorage_GetFileSize_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetFileTimestamp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5424]
SteamAPI_ISteamRemoteStorage_GetFileTimestamp_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetLocalFileChange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5432]
SteamAPI_ISteamRemoteStorage_GetLocalFileChange_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5440]
SteamAPI_ISteamRemoteStorage_GetLocalFileChangeCount_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5448]
SteamAPI_ISteamRemoteStorage_GetPublishedFileDetails_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5456]
SteamAPI_ISteamRemoteStorage_GetPublishedItemVoteDetails_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetQuota_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5464]
SteamAPI_ISteamRemoteStorage_GetQuota_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetSyncPlatforms_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5472]
SteamAPI_ISteamRemoteStorage_GetSyncPlatforms_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetUGCDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5480]
SteamAPI_ISteamRemoteStorage_GetUGCDetails_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5488]
SteamAPI_ISteamRemoteStorage_GetUGCDownloadProgress_proxy ENDP
SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5496]
SteamAPI_ISteamRemoteStorage_GetUserPublishedItemVoteDetails_proxy ENDP
SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5504]
SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount_proxy ENDP
SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5512]
SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp_proxy ENDP
SteamAPI_ISteamRemoteStorage_PublishVideo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5520]
SteamAPI_ISteamRemoteStorage_PublishVideo_proxy ENDP
SteamAPI_ISteamRemoteStorage_PublishWorkshopFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5528]
SteamAPI_ISteamRemoteStorage_PublishWorkshopFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5536]
SteamAPI_ISteamRemoteStorage_SetCloudEnabledForApp_proxy ENDP
SteamAPI_ISteamRemoteStorage_SetSyncPlatforms_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5544]
SteamAPI_ISteamRemoteStorage_SetSyncPlatforms_proxy ENDP
SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5552]
SteamAPI_ISteamRemoteStorage_SetUserPublishedFileAction_proxy ENDP
SteamAPI_ISteamRemoteStorage_SubscribePublishedFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5560]
SteamAPI_ISteamRemoteStorage_SubscribePublishedFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_UGCDownload_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5568]
SteamAPI_ISteamRemoteStorage_UGCDownload_proxy ENDP
SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5576]
SteamAPI_ISteamRemoteStorage_UGCDownloadToLocation_proxy ENDP
SteamAPI_ISteamRemoteStorage_UGCRead_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5584]
SteamAPI_ISteamRemoteStorage_UGCRead_proxy ENDP
SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5592]
SteamAPI_ISteamRemoteStorage_UnsubscribePublishedFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5600]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileDescription_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5608]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5616]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFilePreviewFile_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5624]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileSetChangeDescription_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5632]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTags_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5640]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileTitle_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5648]
SteamAPI_ISteamRemoteStorage_UpdatePublishedFileVisibility_proxy ENDP
SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5656]
SteamAPI_ISteamRemoteStorage_UpdateUserPublishedItemVote_proxy ENDP
SteamAPI_ISteamScreenshots_AddScreenshotToLibrary_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5664]
SteamAPI_ISteamScreenshots_AddScreenshotToLibrary_proxy ENDP
SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5672]
SteamAPI_ISteamScreenshots_AddVRScreenshotToLibrary_proxy ENDP
SteamAPI_ISteamScreenshots_HookScreenshots_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5680]
SteamAPI_ISteamScreenshots_HookScreenshots_proxy ENDP
SteamAPI_ISteamScreenshots_IsScreenshotsHooked_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5688]
SteamAPI_ISteamScreenshots_IsScreenshotsHooked_proxy ENDP
SteamAPI_ISteamScreenshots_SetLocation_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5696]
SteamAPI_ISteamScreenshots_SetLocation_proxy ENDP
SteamAPI_ISteamScreenshots_TagPublishedFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5704]
SteamAPI_ISteamScreenshots_TagPublishedFile_proxy ENDP
SteamAPI_ISteamScreenshots_TagUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5712]
SteamAPI_ISteamScreenshots_TagUser_proxy ENDP
SteamAPI_ISteamScreenshots_TriggerScreenshot_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5720]
SteamAPI_ISteamScreenshots_TriggerScreenshot_proxy ENDP
SteamAPI_ISteamScreenshots_WriteScreenshot_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5728]
SteamAPI_ISteamScreenshots_WriteScreenshot_proxy ENDP
SteamAPI_ISteamUGC_AddAppDependency_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5736]
SteamAPI_ISteamUGC_AddAppDependency_proxy ENDP
SteamAPI_ISteamUGC_AddDependency_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5744]
SteamAPI_ISteamUGC_AddDependency_proxy ENDP
SteamAPI_ISteamUGC_AddExcludedTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5752]
SteamAPI_ISteamUGC_AddExcludedTag_proxy ENDP
SteamAPI_ISteamUGC_AddItemKeyValueTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5760]
SteamAPI_ISteamUGC_AddItemKeyValueTag_proxy ENDP
SteamAPI_ISteamUGC_AddItemPreviewFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5768]
SteamAPI_ISteamUGC_AddItemPreviewFile_proxy ENDP
SteamAPI_ISteamUGC_AddItemPreviewVideo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5776]
SteamAPI_ISteamUGC_AddItemPreviewVideo_proxy ENDP
SteamAPI_ISteamUGC_AddItemToFavorites_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5784]
SteamAPI_ISteamUGC_AddItemToFavorites_proxy ENDP
SteamAPI_ISteamUGC_AddRequiredKeyValueTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5792]
SteamAPI_ISteamUGC_AddRequiredKeyValueTag_proxy ENDP
SteamAPI_ISteamUGC_AddRequiredTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5800]
SteamAPI_ISteamUGC_AddRequiredTag_proxy ENDP
SteamAPI_ISteamUGC_AddRequiredTagGroup_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5808]
SteamAPI_ISteamUGC_AddRequiredTagGroup_proxy ENDP
SteamAPI_ISteamUGC_BInitWorkshopForGameServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5816]
SteamAPI_ISteamUGC_BInitWorkshopForGameServer_proxy ENDP
SteamAPI_ISteamUGC_CreateItem_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5824]
SteamAPI_ISteamUGC_CreateItem_proxy ENDP
SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5832]
SteamAPI_ISteamUGC_CreateQueryAllUGCRequestCursor_proxy ENDP
SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5840]
SteamAPI_ISteamUGC_CreateQueryAllUGCRequestPage_proxy ENDP
SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5848]
SteamAPI_ISteamUGC_CreateQueryUGCDetailsRequest_proxy ENDP
SteamAPI_ISteamUGC_CreateQueryUserUGCRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5856]
SteamAPI_ISteamUGC_CreateQueryUserUGCRequest_proxy ENDP
SteamAPI_ISteamUGC_DeleteItem_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5864]
SteamAPI_ISteamUGC_DeleteItem_proxy ENDP
SteamAPI_ISteamUGC_DownloadItem_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5872]
SteamAPI_ISteamUGC_DownloadItem_proxy ENDP
SteamAPI_ISteamUGC_GetAppDependencies_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5880]
SteamAPI_ISteamUGC_GetAppDependencies_proxy ENDP
SteamAPI_ISteamUGC_GetItemDownloadInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5888]
SteamAPI_ISteamUGC_GetItemDownloadInfo_proxy ENDP
SteamAPI_ISteamUGC_GetItemInstallInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5896]
SteamAPI_ISteamUGC_GetItemInstallInfo_proxy ENDP
SteamAPI_ISteamUGC_GetItemState_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5904]
SteamAPI_ISteamUGC_GetItemState_proxy ENDP
SteamAPI_ISteamUGC_GetItemUpdateProgress_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5912]
SteamAPI_ISteamUGC_GetItemUpdateProgress_proxy ENDP
SteamAPI_ISteamUGC_GetNumSubscribedItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5920]
SteamAPI_ISteamUGC_GetNumSubscribedItems_proxy ENDP
SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5928]
SteamAPI_ISteamUGC_GetQueryFirstUGCKeyValueTag_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5936]
SteamAPI_ISteamUGC_GetQueryUGCAdditionalPreview_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCChildren_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5944]
SteamAPI_ISteamUGC_GetQueryUGCChildren_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5952]
SteamAPI_ISteamUGC_GetQueryUGCKeyValueTag_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCMetadata_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5960]
SteamAPI_ISteamUGC_GetQueryUGCMetadata_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5968]
SteamAPI_ISteamUGC_GetQueryUGCNumAdditionalPreviews_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5976]
SteamAPI_ISteamUGC_GetQueryUGCNumKeyValueTags_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCNumTags_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5984]
SteamAPI_ISteamUGC_GetQueryUGCNumTags_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCPreviewURL_proxy PROC
    jmp QWORD PTR [g_steamProcs + 5992]
SteamAPI_ISteamUGC_GetQueryUGCPreviewURL_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6000]
SteamAPI_ISteamUGC_GetQueryUGCResult_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCStatistic_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6008]
SteamAPI_ISteamUGC_GetQueryUGCStatistic_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6016]
SteamAPI_ISteamUGC_GetQueryUGCTag_proxy ENDP
SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6024]
SteamAPI_ISteamUGC_GetQueryUGCTagDisplayName_proxy ENDP
SteamAPI_ISteamUGC_GetSubscribedItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6032]
SteamAPI_ISteamUGC_GetSubscribedItems_proxy ENDP
SteamAPI_ISteamUGC_GetUserItemVote_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6040]
SteamAPI_ISteamUGC_GetUserItemVote_proxy ENDP
SteamAPI_ISteamUGC_GetWorkshopEULAStatus_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6048]
SteamAPI_ISteamUGC_GetWorkshopEULAStatus_proxy ENDP
SteamAPI_ISteamUGC_ReleaseQueryUGCRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6056]
SteamAPI_ISteamUGC_ReleaseQueryUGCRequest_proxy ENDP
SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6064]
SteamAPI_ISteamUGC_RemoveAllItemKeyValueTags_proxy ENDP
SteamAPI_ISteamUGC_RemoveAppDependency_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6072]
SteamAPI_ISteamUGC_RemoveAppDependency_proxy ENDP
SteamAPI_ISteamUGC_RemoveDependency_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6080]
SteamAPI_ISteamUGC_RemoveDependency_proxy ENDP
SteamAPI_ISteamUGC_RemoveItemFromFavorites_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6088]
SteamAPI_ISteamUGC_RemoveItemFromFavorites_proxy ENDP
SteamAPI_ISteamUGC_RemoveItemKeyValueTags_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6096]
SteamAPI_ISteamUGC_RemoveItemKeyValueTags_proxy ENDP
SteamAPI_ISteamUGC_RemoveItemPreview_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6104]
SteamAPI_ISteamUGC_RemoveItemPreview_proxy ENDP
SteamAPI_ISteamUGC_RequestUGCDetails_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6112]
SteamAPI_ISteamUGC_RequestUGCDetails_proxy ENDP
SteamAPI_ISteamUGC_SendQueryUGCRequest_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6120]
SteamAPI_ISteamUGC_SendQueryUGCRequest_proxy ENDP
SteamAPI_ISteamUGC_SetAllowCachedResponse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6128]
SteamAPI_ISteamUGC_SetAllowCachedResponse_proxy ENDP
SteamAPI_ISteamUGC_SetAllowLegacyUpload_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6136]
SteamAPI_ISteamUGC_SetAllowLegacyUpload_proxy ENDP
SteamAPI_ISteamUGC_SetCloudFileNameFilter_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6144]
SteamAPI_ISteamUGC_SetCloudFileNameFilter_proxy ENDP
SteamAPI_ISteamUGC_SetItemContent_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6152]
SteamAPI_ISteamUGC_SetItemContent_proxy ENDP
SteamAPI_ISteamUGC_SetItemDescription_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6160]
SteamAPI_ISteamUGC_SetItemDescription_proxy ENDP
SteamAPI_ISteamUGC_SetItemMetadata_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6168]
SteamAPI_ISteamUGC_SetItemMetadata_proxy ENDP
SteamAPI_ISteamUGC_SetItemPreview_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6176]
SteamAPI_ISteamUGC_SetItemPreview_proxy ENDP
SteamAPI_ISteamUGC_SetItemTags_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6184]
SteamAPI_ISteamUGC_SetItemTags_proxy ENDP
SteamAPI_ISteamUGC_SetItemTitle_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6192]
SteamAPI_ISteamUGC_SetItemTitle_proxy ENDP
SteamAPI_ISteamUGC_SetItemUpdateLanguage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6200]
SteamAPI_ISteamUGC_SetItemUpdateLanguage_proxy ENDP
SteamAPI_ISteamUGC_SetItemVisibility_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6208]
SteamAPI_ISteamUGC_SetItemVisibility_proxy ENDP
SteamAPI_ISteamUGC_SetLanguage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6216]
SteamAPI_ISteamUGC_SetLanguage_proxy ENDP
SteamAPI_ISteamUGC_SetMatchAnyTag_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6224]
SteamAPI_ISteamUGC_SetMatchAnyTag_proxy ENDP
SteamAPI_ISteamUGC_SetRankedByTrendDays_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6232]
SteamAPI_ISteamUGC_SetRankedByTrendDays_proxy ENDP
SteamAPI_ISteamUGC_SetReturnAdditionalPreviews_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6240]
SteamAPI_ISteamUGC_SetReturnAdditionalPreviews_proxy ENDP
SteamAPI_ISteamUGC_SetReturnChildren_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6248]
SteamAPI_ISteamUGC_SetReturnChildren_proxy ENDP
SteamAPI_ISteamUGC_SetReturnKeyValueTags_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6256]
SteamAPI_ISteamUGC_SetReturnKeyValueTags_proxy ENDP
SteamAPI_ISteamUGC_SetReturnLongDescription_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6264]
SteamAPI_ISteamUGC_SetReturnLongDescription_proxy ENDP
SteamAPI_ISteamUGC_SetReturnMetadata_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6272]
SteamAPI_ISteamUGC_SetReturnMetadata_proxy ENDP
SteamAPI_ISteamUGC_SetReturnOnlyIDs_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6280]
SteamAPI_ISteamUGC_SetReturnOnlyIDs_proxy ENDP
SteamAPI_ISteamUGC_SetReturnPlaytimeStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6288]
SteamAPI_ISteamUGC_SetReturnPlaytimeStats_proxy ENDP
SteamAPI_ISteamUGC_SetReturnTotalOnly_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6296]
SteamAPI_ISteamUGC_SetReturnTotalOnly_proxy ENDP
SteamAPI_ISteamUGC_SetSearchText_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6304]
SteamAPI_ISteamUGC_SetSearchText_proxy ENDP
SteamAPI_ISteamUGC_SetTimeCreatedDateRange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6312]
SteamAPI_ISteamUGC_SetTimeCreatedDateRange_proxy ENDP
SteamAPI_ISteamUGC_SetTimeUpdatedDateRange_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6320]
SteamAPI_ISteamUGC_SetTimeUpdatedDateRange_proxy ENDP
SteamAPI_ISteamUGC_SetUserItemVote_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6328]
SteamAPI_ISteamUGC_SetUserItemVote_proxy ENDP
SteamAPI_ISteamUGC_ShowWorkshopEULA_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6336]
SteamAPI_ISteamUGC_ShowWorkshopEULA_proxy ENDP
SteamAPI_ISteamUGC_StartItemUpdate_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6344]
SteamAPI_ISteamUGC_StartItemUpdate_proxy ENDP
SteamAPI_ISteamUGC_StartPlaytimeTracking_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6352]
SteamAPI_ISteamUGC_StartPlaytimeTracking_proxy ENDP
SteamAPI_ISteamUGC_StopPlaytimeTracking_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6360]
SteamAPI_ISteamUGC_StopPlaytimeTracking_proxy ENDP
SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6368]
SteamAPI_ISteamUGC_StopPlaytimeTrackingForAllItems_proxy ENDP
SteamAPI_ISteamUGC_SubmitItemUpdate_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6376]
SteamAPI_ISteamUGC_SubmitItemUpdate_proxy ENDP
SteamAPI_ISteamUGC_SubscribeItem_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6384]
SteamAPI_ISteamUGC_SubscribeItem_proxy ENDP
SteamAPI_ISteamUGC_SuspendDownloads_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6392]
SteamAPI_ISteamUGC_SuspendDownloads_proxy ENDP
SteamAPI_ISteamUGC_UnsubscribeItem_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6400]
SteamAPI_ISteamUGC_UnsubscribeItem_proxy ENDP
SteamAPI_ISteamUGC_UpdateItemPreviewFile_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6408]
SteamAPI_ISteamUGC_UpdateItemPreviewFile_proxy ENDP
SteamAPI_ISteamUGC_UpdateItemPreviewVideo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6416]
SteamAPI_ISteamUGC_UpdateItemPreviewVideo_proxy ENDP
SteamAPI_ISteamUserStats_AttachLeaderboardUGC_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6424]
SteamAPI_ISteamUserStats_AttachLeaderboardUGC_proxy ENDP
SteamAPI_ISteamUserStats_ClearAchievement_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6432]
SteamAPI_ISteamUserStats_ClearAchievement_proxy ENDP
SteamAPI_ISteamUserStats_DownloadLeaderboardEntries_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6440]
SteamAPI_ISteamUserStats_DownloadLeaderboardEntries_proxy ENDP
SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6448]
SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers_proxy ENDP
SteamAPI_ISteamUserStats_FindLeaderboard_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6456]
SteamAPI_ISteamUserStats_FindLeaderboard_proxy ENDP
SteamAPI_ISteamUserStats_FindOrCreateLeaderboard_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6464]
SteamAPI_ISteamUserStats_FindOrCreateLeaderboard_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievement_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6472]
SteamAPI_ISteamUserStats_GetAchievement_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementAchievedPercent_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6480]
SteamAPI_ISteamUserStats_GetAchievementAchievedPercent_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6488]
SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6496]
SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementIcon_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6504]
SteamAPI_ISteamUserStats_GetAchievementIcon_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6512]
SteamAPI_ISteamUserStats_GetAchievementName_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6520]
SteamAPI_ISteamUserStats_GetAchievementProgressLimitsFloat_proxy ENDP
SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6528]
SteamAPI_ISteamUserStats_GetAchievementProgressLimitsInt32_proxy ENDP
SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6536]
SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry_proxy ENDP
SteamAPI_ISteamUserStats_GetGlobalStatDouble_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6544]
SteamAPI_ISteamUserStats_GetGlobalStatDouble_proxy ENDP
SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6552]
SteamAPI_ISteamUserStats_GetGlobalStatHistoryDouble_proxy ENDP
SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6560]
SteamAPI_ISteamUserStats_GetGlobalStatHistoryInt64_proxy ENDP
SteamAPI_ISteamUserStats_GetGlobalStatInt64_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6568]
SteamAPI_ISteamUserStats_GetGlobalStatInt64_proxy ENDP
SteamAPI_ISteamUserStats_GetLeaderboardDisplayType_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6576]
SteamAPI_ISteamUserStats_GetLeaderboardDisplayType_proxy ENDP
SteamAPI_ISteamUserStats_GetLeaderboardEntryCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6584]
SteamAPI_ISteamUserStats_GetLeaderboardEntryCount_proxy ENDP
SteamAPI_ISteamUserStats_GetLeaderboardName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6592]
SteamAPI_ISteamUserStats_GetLeaderboardName_proxy ENDP
SteamAPI_ISteamUserStats_GetLeaderboardSortMethod_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6600]
SteamAPI_ISteamUserStats_GetLeaderboardSortMethod_proxy ENDP
SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6608]
SteamAPI_ISteamUserStats_GetMostAchievedAchievementInfo_proxy ENDP
SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6616]
SteamAPI_ISteamUserStats_GetNextMostAchievedAchievementInfo_proxy ENDP
SteamAPI_ISteamUserStats_GetNumAchievements_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6624]
SteamAPI_ISteamUserStats_GetNumAchievements_proxy ENDP
SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6632]
SteamAPI_ISteamUserStats_GetNumberOfCurrentPlayers_proxy ENDP
SteamAPI_ISteamUserStats_GetStatFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6640]
SteamAPI_ISteamUserStats_GetStatFloat_proxy ENDP
SteamAPI_ISteamUserStats_GetStatInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6648]
SteamAPI_ISteamUserStats_GetStatInt32_proxy ENDP
SteamAPI_ISteamUserStats_GetUserAchievement_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6656]
SteamAPI_ISteamUserStats_GetUserAchievement_proxy ENDP
SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6664]
SteamAPI_ISteamUserStats_GetUserAchievementAndUnlockTime_proxy ENDP
SteamAPI_ISteamUserStats_GetUserStatFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6672]
SteamAPI_ISteamUserStats_GetUserStatFloat_proxy ENDP
SteamAPI_ISteamUserStats_GetUserStatInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6680]
SteamAPI_ISteamUserStats_GetUserStatInt32_proxy ENDP
SteamAPI_ISteamUserStats_IndicateAchievementProgress_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6688]
SteamAPI_ISteamUserStats_IndicateAchievementProgress_proxy ENDP
SteamAPI_ISteamUserStats_RequestCurrentStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6696]
SteamAPI_ISteamUserStats_RequestCurrentStats_proxy ENDP
SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6704]
SteamAPI_ISteamUserStats_RequestGlobalAchievementPercentages_proxy ENDP
SteamAPI_ISteamUserStats_RequestGlobalStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6712]
SteamAPI_ISteamUserStats_RequestGlobalStats_proxy ENDP
SteamAPI_ISteamUserStats_RequestUserStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6720]
SteamAPI_ISteamUserStats_RequestUserStats_proxy ENDP
SteamAPI_ISteamUserStats_ResetAllStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6728]
SteamAPI_ISteamUserStats_ResetAllStats_proxy ENDP
SteamAPI_ISteamUserStats_SetAchievement_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6736]
SteamAPI_ISteamUserStats_SetAchievement_proxy ENDP
SteamAPI_ISteamUserStats_SetStatFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6744]
SteamAPI_ISteamUserStats_SetStatFloat_proxy ENDP
SteamAPI_ISteamUserStats_SetStatInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6752]
SteamAPI_ISteamUserStats_SetStatInt32_proxy ENDP
SteamAPI_ISteamUserStats_StoreStats_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6760]
SteamAPI_ISteamUserStats_StoreStats_proxy ENDP
SteamAPI_ISteamUserStats_UpdateAvgRateStat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6768]
SteamAPI_ISteamUserStats_UpdateAvgRateStat_proxy ENDP
SteamAPI_ISteamUserStats_UploadLeaderboardScore_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6776]
SteamAPI_ISteamUserStats_UploadLeaderboardScore_proxy ENDP
SteamAPI_ISteamUser_AdvertiseGame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6784]
SteamAPI_ISteamUser_AdvertiseGame_proxy ENDP
SteamAPI_ISteamUser_BIsBehindNAT_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6792]
SteamAPI_ISteamUser_BIsBehindNAT_proxy ENDP
SteamAPI_ISteamUser_BIsPhoneIdentifying_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6800]
SteamAPI_ISteamUser_BIsPhoneIdentifying_proxy ENDP
SteamAPI_ISteamUser_BIsPhoneRequiringVerification_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6808]
SteamAPI_ISteamUser_BIsPhoneRequiringVerification_proxy ENDP
SteamAPI_ISteamUser_BIsPhoneVerified_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6816]
SteamAPI_ISteamUser_BIsPhoneVerified_proxy ENDP
SteamAPI_ISteamUser_BIsTwoFactorEnabled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6824]
SteamAPI_ISteamUser_BIsTwoFactorEnabled_proxy ENDP
SteamAPI_ISteamUser_BLoggedOn_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6832]
SteamAPI_ISteamUser_BLoggedOn_proxy ENDP
SteamAPI_ISteamUser_BSetDurationControlOnlineState_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6840]
SteamAPI_ISteamUser_BSetDurationControlOnlineState_proxy ENDP
SteamAPI_ISteamUser_BeginAuthSession_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6848]
SteamAPI_ISteamUser_BeginAuthSession_proxy ENDP
SteamAPI_ISteamUser_CancelAuthTicket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6856]
SteamAPI_ISteamUser_CancelAuthTicket_proxy ENDP
SteamAPI_ISteamUser_DecompressVoice_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6864]
SteamAPI_ISteamUser_DecompressVoice_proxy ENDP
SteamAPI_ISteamUser_EndAuthSession_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6872]
SteamAPI_ISteamUser_EndAuthSession_proxy ENDP
SteamAPI_ISteamUser_GetAuthSessionTicket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6880]
SteamAPI_ISteamUser_GetAuthSessionTicket_proxy ENDP
SteamAPI_ISteamUser_GetAvailableVoice_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6888]
SteamAPI_ISteamUser_GetAvailableVoice_proxy ENDP
SteamAPI_ISteamUser_GetDurationControl_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6896]
SteamAPI_ISteamUser_GetDurationControl_proxy ENDP
SteamAPI_ISteamUser_GetEncryptedAppTicket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6904]
SteamAPI_ISteamUser_GetEncryptedAppTicket_proxy ENDP
SteamAPI_ISteamUser_GetGameBadgeLevel_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6912]
SteamAPI_ISteamUser_GetGameBadgeLevel_proxy ENDP
SteamAPI_ISteamUser_GetHSteamUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6920]
SteamAPI_ISteamUser_GetHSteamUser_proxy ENDP
SteamAPI_ISteamUser_GetMarketEligibility_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6928]
SteamAPI_ISteamUser_GetMarketEligibility_proxy ENDP
SteamAPI_ISteamUser_GetPlayerSteamLevel_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6936]
SteamAPI_ISteamUser_GetPlayerSteamLevel_proxy ENDP
SteamAPI_ISteamUser_GetSteamID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6944]
SteamAPI_ISteamUser_GetSteamID_proxy ENDP
SteamAPI_ISteamUser_GetUserDataFolder_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6952]
SteamAPI_ISteamUser_GetUserDataFolder_proxy ENDP
SteamAPI_ISteamUser_GetVoice_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6960]
SteamAPI_ISteamUser_GetVoice_proxy ENDP
SteamAPI_ISteamUser_GetVoiceOptimalSampleRate_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6968]
SteamAPI_ISteamUser_GetVoiceOptimalSampleRate_proxy ENDP
SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6976]
SteamAPI_ISteamUser_InitiateGameConnection_DEPRECATED_proxy ENDP
SteamAPI_ISteamUser_RequestEncryptedAppTicket_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6984]
SteamAPI_ISteamUser_RequestEncryptedAppTicket_proxy ENDP
SteamAPI_ISteamUser_RequestStoreAuthURL_proxy PROC
    jmp QWORD PTR [g_steamProcs + 6992]
SteamAPI_ISteamUser_RequestStoreAuthURL_proxy ENDP
SteamAPI_ISteamUser_StartVoiceRecording_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7000]
SteamAPI_ISteamUser_StartVoiceRecording_proxy ENDP
SteamAPI_ISteamUser_StopVoiceRecording_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7008]
SteamAPI_ISteamUser_StopVoiceRecording_proxy ENDP
SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7016]
SteamAPI_ISteamUser_TerminateGameConnection_DEPRECATED_proxy ENDP
SteamAPI_ISteamUser_TrackAppUsageEvent_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7024]
SteamAPI_ISteamUser_TrackAppUsageEvent_proxy ENDP
SteamAPI_ISteamUser_UserHasLicenseForApp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7032]
SteamAPI_ISteamUser_UserHasLicenseForApp_proxy ENDP
SteamAPI_ISteamUtils_BOverlayNeedsPresent_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7040]
SteamAPI_ISteamUtils_BOverlayNeedsPresent_proxy ENDP
SteamAPI_ISteamUtils_CheckFileSignature_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7048]
SteamAPI_ISteamUtils_CheckFileSignature_proxy ENDP
SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7056]
SteamAPI_ISteamUtils_DismissFloatingGamepadTextInput_proxy ENDP
SteamAPI_ISteamUtils_FilterText_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7064]
SteamAPI_ISteamUtils_FilterText_proxy ENDP
SteamAPI_ISteamUtils_GetAPICallFailureReason_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7072]
SteamAPI_ISteamUtils_GetAPICallFailureReason_proxy ENDP
SteamAPI_ISteamUtils_GetAPICallResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7080]
SteamAPI_ISteamUtils_GetAPICallResult_proxy ENDP
SteamAPI_ISteamUtils_GetAppID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7088]
SteamAPI_ISteamUtils_GetAppID_proxy ENDP
SteamAPI_ISteamUtils_GetConnectedUniverse_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7096]
SteamAPI_ISteamUtils_GetConnectedUniverse_proxy ENDP
SteamAPI_ISteamUtils_GetCurrentBatteryPower_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7104]
SteamAPI_ISteamUtils_GetCurrentBatteryPower_proxy ENDP
SteamAPI_ISteamUtils_GetEnteredGamepadTextInput_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7112]
SteamAPI_ISteamUtils_GetEnteredGamepadTextInput_proxy ENDP
SteamAPI_ISteamUtils_GetEnteredGamepadTextLength_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7120]
SteamAPI_ISteamUtils_GetEnteredGamepadTextLength_proxy ENDP
SteamAPI_ISteamUtils_GetIPCCallCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7128]
SteamAPI_ISteamUtils_GetIPCCallCount_proxy ENDP
SteamAPI_ISteamUtils_GetIPCountry_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7136]
SteamAPI_ISteamUtils_GetIPCountry_proxy ENDP
SteamAPI_ISteamUtils_GetIPv6ConnectivityState_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7144]
SteamAPI_ISteamUtils_GetIPv6ConnectivityState_proxy ENDP
SteamAPI_ISteamUtils_GetImageRGBA_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7152]
SteamAPI_ISteamUtils_GetImageRGBA_proxy ENDP
SteamAPI_ISteamUtils_GetImageSize_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7160]
SteamAPI_ISteamUtils_GetImageSize_proxy ENDP
SteamAPI_ISteamUtils_GetSecondsSinceAppActive_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7168]
SteamAPI_ISteamUtils_GetSecondsSinceAppActive_proxy ENDP
SteamAPI_ISteamUtils_GetSecondsSinceComputerActive_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7176]
SteamAPI_ISteamUtils_GetSecondsSinceComputerActive_proxy ENDP
SteamAPI_ISteamUtils_GetServerRealTime_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7184]
SteamAPI_ISteamUtils_GetServerRealTime_proxy ENDP
SteamAPI_ISteamUtils_GetSteamUILanguage_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7192]
SteamAPI_ISteamUtils_GetSteamUILanguage_proxy ENDP
SteamAPI_ISteamUtils_InitFilterText_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7200]
SteamAPI_ISteamUtils_InitFilterText_proxy ENDP
SteamAPI_ISteamUtils_IsAPICallCompleted_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7208]
SteamAPI_ISteamUtils_IsAPICallCompleted_proxy ENDP
SteamAPI_ISteamUtils_IsOverlayEnabled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7216]
SteamAPI_ISteamUtils_IsOverlayEnabled_proxy ENDP
SteamAPI_ISteamUtils_IsSteamChinaLauncher_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7224]
SteamAPI_ISteamUtils_IsSteamChinaLauncher_proxy ENDP
SteamAPI_ISteamUtils_IsSteamInBigPictureMode_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7232]
SteamAPI_ISteamUtils_IsSteamInBigPictureMode_proxy ENDP
SteamAPI_ISteamUtils_IsSteamRunningInVR_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7240]
SteamAPI_ISteamUtils_IsSteamRunningInVR_proxy ENDP
SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7248]
SteamAPI_ISteamUtils_IsSteamRunningOnSteamDeck_proxy ENDP
SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7256]
SteamAPI_ISteamUtils_IsVRHeadsetStreamingEnabled_proxy ENDP
SteamAPI_ISteamUtils_SetGameLauncherMode_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7264]
SteamAPI_ISteamUtils_SetGameLauncherMode_proxy ENDP
SteamAPI_ISteamUtils_SetOverlayNotificationInset_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7272]
SteamAPI_ISteamUtils_SetOverlayNotificationInset_proxy ENDP
SteamAPI_ISteamUtils_SetOverlayNotificationPosition_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7280]
SteamAPI_ISteamUtils_SetOverlayNotificationPosition_proxy ENDP
SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7288]
SteamAPI_ISteamUtils_SetVRHeadsetStreamingEnabled_proxy ENDP
SteamAPI_ISteamUtils_SetWarningMessageHook_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7296]
SteamAPI_ISteamUtils_SetWarningMessageHook_proxy ENDP
SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7304]
SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput_proxy ENDP
SteamAPI_ISteamUtils_ShowGamepadTextInput_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7312]
SteamAPI_ISteamUtils_ShowGamepadTextInput_proxy ENDP
SteamAPI_ISteamUtils_StartVRDashboard_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7320]
SteamAPI_ISteamUtils_StartVRDashboard_proxy ENDP
SteamAPI_ISteamVideo_GetOPFSettings_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7328]
SteamAPI_ISteamVideo_GetOPFSettings_proxy ENDP
SteamAPI_ISteamVideo_GetOPFStringForApp_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7336]
SteamAPI_ISteamVideo_GetOPFStringForApp_proxy ENDP
SteamAPI_ISteamVideo_GetVideoURL_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7344]
SteamAPI_ISteamVideo_GetVideoURL_proxy ENDP
SteamAPI_ISteamVideo_IsBroadcasting_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7352]
SteamAPI_ISteamVideo_IsBroadcasting_proxy ENDP
SteamAPI_IsSteamRunning_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7360]
SteamAPI_IsSteamRunning_proxy ENDP
SteamAPI_ManualDispatch_FreeLastCallback_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7368]
SteamAPI_ManualDispatch_FreeLastCallback_proxy ENDP
SteamAPI_ManualDispatch_GetAPICallResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7376]
SteamAPI_ManualDispatch_GetAPICallResult_proxy ENDP
SteamAPI_ManualDispatch_GetNextCallback_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7384]
SteamAPI_ManualDispatch_GetNextCallback_proxy ENDP
SteamAPI_ManualDispatch_Init_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7392]
SteamAPI_ManualDispatch_Init_proxy ENDP
SteamAPI_ManualDispatch_RunFrame_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7400]
SteamAPI_ManualDispatch_RunFrame_proxy ENDP
SteamAPI_MatchMakingKeyValuePair_t_Construct_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7408]
SteamAPI_MatchMakingKeyValuePair_t_Construct_proxy ENDP
SteamAPI_RegisterCallResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7416]
SteamAPI_RegisterCallResult_proxy ENDP
SteamAPI_RegisterCallback_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7424]
SteamAPI_RegisterCallback_proxy ENDP
SteamAPI_ReleaseCurrentThreadMemory_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7432]
SteamAPI_ReleaseCurrentThreadMemory_proxy ENDP
SteamAPI_RunCallbacks_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7440]
SteamAPI_RunCallbacks_proxy ENDP
SteamAPI_SetBreakpadAppID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7448]
SteamAPI_SetBreakpadAppID_proxy ENDP
SteamAPI_SetMiniDumpComment_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7456]
SteamAPI_SetMiniDumpComment_proxy ENDP
SteamAPI_SetTryCatchCallbacks_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7464]
SteamAPI_SetTryCatchCallbacks_proxy ENDP
SteamAPI_Shutdown_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7472]
SteamAPI_Shutdown_proxy ENDP
SteamAPI_SteamAppList_v001_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7480]
SteamAPI_SteamAppList_v001_proxy ENDP
SteamAPI_SteamApps_v008_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7488]
SteamAPI_SteamApps_v008_proxy ENDP
SteamAPI_SteamController_v008_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7496]
SteamAPI_SteamController_v008_proxy ENDP
SteamAPI_SteamDatagramHostedAddress_Clear_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7504]
SteamAPI_SteamDatagramHostedAddress_Clear_proxy ENDP
SteamAPI_SteamDatagramHostedAddress_GetPopID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7512]
SteamAPI_SteamDatagramHostedAddress_GetPopID_proxy ENDP
SteamAPI_SteamDatagramHostedAddress_SetDevAddress_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7520]
SteamAPI_SteamDatagramHostedAddress_SetDevAddress_proxy ENDP
SteamAPI_SteamFriends_v017_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7528]
SteamAPI_SteamFriends_v017_proxy ENDP
SteamAPI_SteamGameSearch_v001_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7536]
SteamAPI_SteamGameSearch_v001_proxy ENDP
SteamAPI_SteamGameServerHTTP_v003_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7544]
SteamAPI_SteamGameServerHTTP_v003_proxy ENDP
SteamAPI_SteamGameServerInventory_v003_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7552]
SteamAPI_SteamGameServerInventory_v003_proxy ENDP
SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7560]
SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002_proxy ENDP
SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7568]
SteamAPI_SteamGameServerNetworkingSockets_SteamAPI_v012_proxy ENDP
SteamAPI_SteamGameServerNetworking_v006_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7576]
SteamAPI_SteamGameServerNetworking_v006_proxy ENDP
SteamAPI_SteamGameServerStats_v001_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7584]
SteamAPI_SteamGameServerStats_v001_proxy ENDP
SteamAPI_SteamGameServerUGC_v016_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7592]
SteamAPI_SteamGameServerUGC_v016_proxy ENDP
SteamAPI_SteamGameServerUtils_v010_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7600]
SteamAPI_SteamGameServerUtils_v010_proxy ENDP
SteamAPI_SteamGameServer_v014_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7608]
SteamAPI_SteamGameServer_v014_proxy ENDP
SteamAPI_SteamHTMLSurface_v005_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7616]
SteamAPI_SteamHTMLSurface_v005_proxy ENDP
SteamAPI_SteamHTTP_v003_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7624]
SteamAPI_SteamHTTP_v003_proxy ENDP
SteamAPI_SteamIPAddress_t_IsSet_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7632]
SteamAPI_SteamIPAddress_t_IsSet_proxy ENDP
SteamAPI_SteamInput_v006_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7640]
SteamAPI_SteamInput_v006_proxy ENDP
SteamAPI_SteamInventory_v003_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7648]
SteamAPI_SteamInventory_v003_proxy ENDP
SteamAPI_SteamMatchmakingServers_v002_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7656]
SteamAPI_SteamMatchmakingServers_v002_proxy ENDP
SteamAPI_SteamMatchmaking_v009_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7664]
SteamAPI_SteamMatchmaking_v009_proxy ENDP
SteamAPI_SteamMusicRemote_v001_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7672]
SteamAPI_SteamMusicRemote_v001_proxy ENDP
SteamAPI_SteamMusic_v001_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7680]
SteamAPI_SteamMusic_v001_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetFloat_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7688]
SteamAPI_SteamNetworkingConfigValue_t_SetFloat_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetInt32_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7696]
SteamAPI_SteamNetworkingConfigValue_t_SetInt32_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetInt64_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7704]
SteamAPI_SteamNetworkingConfigValue_t_SetInt64_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetPtr_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7712]
SteamAPI_SteamNetworkingConfigValue_t_SetPtr_proxy ENDP
SteamAPI_SteamNetworkingConfigValue_t_SetString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7720]
SteamAPI_SteamNetworkingConfigValue_t_SetString_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_Clear_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7728]
SteamAPI_SteamNetworkingIPAddr_Clear_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_GetFakeIPType_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7736]
SteamAPI_SteamNetworkingIPAddr_GetFakeIPType_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_GetIPv4_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7744]
SteamAPI_SteamNetworkingIPAddr_GetIPv4_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsEqualTo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7752]
SteamAPI_SteamNetworkingIPAddr_IsEqualTo_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsFakeIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7760]
SteamAPI_SteamNetworkingIPAddr_IsFakeIP_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsIPv4_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7768]
SteamAPI_SteamNetworkingIPAddr_IsIPv4_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7776]
SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_IsLocalHost_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7784]
SteamAPI_SteamNetworkingIPAddr_IsLocalHost_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_ParseString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7792]
SteamAPI_SteamNetworkingIPAddr_ParseString_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_SetIPv4_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7800]
SteamAPI_SteamNetworkingIPAddr_SetIPv4_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_SetIPv6_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7808]
SteamAPI_SteamNetworkingIPAddr_SetIPv6_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7816]
SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost_proxy ENDP
SteamAPI_SteamNetworkingIPAddr_ToString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7824]
SteamAPI_SteamNetworkingIPAddr_ToString_proxy ENDP
SteamAPI_SteamNetworkingIdentity_Clear_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7832]
SteamAPI_SteamNetworkingIdentity_Clear_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetFakeIPType_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7840]
SteamAPI_SteamNetworkingIdentity_GetFakeIPType_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetGenericBytes_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7848]
SteamAPI_SteamNetworkingIdentity_GetGenericBytes_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetGenericString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7856]
SteamAPI_SteamNetworkingIdentity_GetGenericString_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetIPAddr_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7864]
SteamAPI_SteamNetworkingIdentity_GetIPAddr_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetIPv4_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7872]
SteamAPI_SteamNetworkingIdentity_GetIPv4_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetPSNID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7880]
SteamAPI_SteamNetworkingIdentity_GetPSNID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetStadiaID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7888]
SteamAPI_SteamNetworkingIdentity_GetStadiaID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetSteamID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7896]
SteamAPI_SteamNetworkingIdentity_GetSteamID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetSteamID64_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7904]
SteamAPI_SteamNetworkingIdentity_GetSteamID64_proxy ENDP
SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7912]
SteamAPI_SteamNetworkingIdentity_GetXboxPairwiseID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_IsEqualTo_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7920]
SteamAPI_SteamNetworkingIdentity_IsEqualTo_proxy ENDP
SteamAPI_SteamNetworkingIdentity_IsFakeIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7928]
SteamAPI_SteamNetworkingIdentity_IsFakeIP_proxy ENDP
SteamAPI_SteamNetworkingIdentity_IsInvalid_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7936]
SteamAPI_SteamNetworkingIdentity_IsInvalid_proxy ENDP
SteamAPI_SteamNetworkingIdentity_IsLocalHost_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7944]
SteamAPI_SteamNetworkingIdentity_IsLocalHost_proxy ENDP
SteamAPI_SteamNetworkingIdentity_ParseString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7952]
SteamAPI_SteamNetworkingIdentity_ParseString_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetGenericBytes_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7960]
SteamAPI_SteamNetworkingIdentity_SetGenericBytes_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetGenericString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7968]
SteamAPI_SteamNetworkingIdentity_SetGenericString_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetIPAddr_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7976]
SteamAPI_SteamNetworkingIdentity_SetIPAddr_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetIPv4Addr_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7984]
SteamAPI_SteamNetworkingIdentity_SetIPv4Addr_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetLocalHost_proxy PROC
    jmp QWORD PTR [g_steamProcs + 7992]
SteamAPI_SteamNetworkingIdentity_SetLocalHost_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetPSNID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8000]
SteamAPI_SteamNetworkingIdentity_SetPSNID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetStadiaID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8008]
SteamAPI_SteamNetworkingIdentity_SetStadiaID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetSteamID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8016]
SteamAPI_SteamNetworkingIdentity_SetSteamID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetSteamID64_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8024]
SteamAPI_SteamNetworkingIdentity_SetSteamID64_proxy ENDP
SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8032]
SteamAPI_SteamNetworkingIdentity_SetXboxPairwiseID_proxy ENDP
SteamAPI_SteamNetworkingIdentity_ToString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8040]
SteamAPI_SteamNetworkingIdentity_ToString_proxy ENDP
SteamAPI_SteamNetworkingMessage_t_Release_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8048]
SteamAPI_SteamNetworkingMessage_t_Release_proxy ENDP
SteamAPI_SteamNetworkingMessages_SteamAPI_v002_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8056]
SteamAPI_SteamNetworkingMessages_SteamAPI_v002_proxy ENDP
SteamAPI_SteamNetworkingSockets_SteamAPI_v012_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8064]
SteamAPI_SteamNetworkingSockets_SteamAPI_v012_proxy ENDP
SteamAPI_SteamNetworkingUtils_SteamAPI_v004_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8072]
SteamAPI_SteamNetworkingUtils_SteamAPI_v004_proxy ENDP
SteamAPI_SteamNetworking_v006_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8080]
SteamAPI_SteamNetworking_v006_proxy ENDP
SteamAPI_SteamParentalSettings_v001_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8088]
SteamAPI_SteamParentalSettings_v001_proxy ENDP
SteamAPI_SteamParties_v002_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8096]
SteamAPI_SteamParties_v002_proxy ENDP
SteamAPI_SteamRemotePlay_v001_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8104]
SteamAPI_SteamRemotePlay_v001_proxy ENDP
SteamAPI_SteamRemoteStorage_v016_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8112]
SteamAPI_SteamRemoteStorage_v016_proxy ENDP
SteamAPI_SteamScreenshots_v003_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8120]
SteamAPI_SteamScreenshots_v003_proxy ENDP
SteamAPI_SteamUGC_v016_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8128]
SteamAPI_SteamUGC_v016_proxy ENDP
SteamAPI_SteamUserStats_v012_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8136]
SteamAPI_SteamUserStats_v012_proxy ENDP
SteamAPI_SteamUser_v021_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8144]
SteamAPI_SteamUser_v021_proxy ENDP
SteamAPI_SteamUtils_v010_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8152]
SteamAPI_SteamUtils_v010_proxy ENDP
SteamAPI_SteamVideo_v002_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8160]
SteamAPI_SteamVideo_v002_proxy ENDP
SteamAPI_UnregisterCallResult_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8168]
SteamAPI_UnregisterCallResult_proxy ENDP
SteamAPI_UnregisterCallback_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8176]
SteamAPI_UnregisterCallback_proxy ENDP
SteamAPI_UseBreakpadCrashHandler_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8184]
SteamAPI_UseBreakpadCrashHandler_proxy ENDP
SteamAPI_WriteMiniDump_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8192]
SteamAPI_WriteMiniDump_proxy ENDP
SteamAPI_gameserveritem_t_Construct_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8200]
SteamAPI_gameserveritem_t_Construct_proxy ENDP
SteamAPI_gameserveritem_t_GetName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8208]
SteamAPI_gameserveritem_t_GetName_proxy ENDP
SteamAPI_gameserveritem_t_SetName_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8216]
SteamAPI_gameserveritem_t_SetName_proxy ENDP
SteamAPI_servernetadr_t_Assign_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8224]
SteamAPI_servernetadr_t_Assign_proxy ENDP
SteamAPI_servernetadr_t_Construct_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8232]
SteamAPI_servernetadr_t_Construct_proxy ENDP
SteamAPI_servernetadr_t_GetConnectionAddressString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8240]
SteamAPI_servernetadr_t_GetConnectionAddressString_proxy ENDP
SteamAPI_servernetadr_t_GetConnectionPort_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8248]
SteamAPI_servernetadr_t_GetConnectionPort_proxy ENDP
SteamAPI_servernetadr_t_GetIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8256]
SteamAPI_servernetadr_t_GetIP_proxy ENDP
SteamAPI_servernetadr_t_GetQueryAddressString_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8264]
SteamAPI_servernetadr_t_GetQueryAddressString_proxy ENDP
SteamAPI_servernetadr_t_GetQueryPort_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8272]
SteamAPI_servernetadr_t_GetQueryPort_proxy ENDP
SteamAPI_servernetadr_t_Init_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8280]
SteamAPI_servernetadr_t_Init_proxy ENDP
SteamAPI_servernetadr_t_IsLessThan_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8288]
SteamAPI_servernetadr_t_IsLessThan_proxy ENDP
SteamAPI_servernetadr_t_SetConnectionPort_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8296]
SteamAPI_servernetadr_t_SetConnectionPort_proxy ENDP
SteamAPI_servernetadr_t_SetIP_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8304]
SteamAPI_servernetadr_t_SetIP_proxy ENDP
SteamAPI_servernetadr_t_SetQueryPort_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8312]
SteamAPI_servernetadr_t_SetQueryPort_proxy ENDP
SteamClient_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8320]
SteamClient_proxy ENDP
SteamGameServer_BSecure_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8328]
SteamGameServer_BSecure_proxy ENDP
SteamGameServer_GetHSteamPipe_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8336]
SteamGameServer_GetHSteamPipe_proxy ENDP
SteamGameServer_GetHSteamUser_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8344]
SteamGameServer_GetHSteamUser_proxy ENDP
SteamGameServer_GetIPCCallCount_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8352]
SteamGameServer_GetIPCCallCount_proxy ENDP
SteamGameServer_GetSteamID_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8360]
SteamGameServer_GetSteamID_proxy ENDP
SteamGameServer_InitSafe_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8368]
SteamGameServer_InitSafe_proxy ENDP
SteamGameServer_RunCallbacks_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8376]
SteamGameServer_RunCallbacks_proxy ENDP
SteamGameServer_Shutdown_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8384]
SteamGameServer_Shutdown_proxy ENDP
SteamInternal_ContextInit_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8392]
SteamInternal_ContextInit_proxy ENDP
SteamInternal_CreateInterface_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8400]
SteamInternal_CreateInterface_proxy ENDP
SteamInternal_FindOrCreateGameServerInterface_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8408]
SteamInternal_FindOrCreateGameServerInterface_proxy ENDP
SteamInternal_FindOrCreateUserInterface_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8416]
SteamInternal_FindOrCreateUserInterface_proxy ENDP
SteamInternal_GameServer_Init_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8424]
SteamInternal_GameServer_Init_proxy ENDP
g_pSteamClientGameServer_proxy PROC
    jmp QWORD PTR [g_steamProcs + 8432]
g_pSteamClientGameServer_proxy ENDP
END

