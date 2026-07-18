; ReFix - EOSSDK x64 Forwarding Trampolines (AUTO-GENERATED)
; Forwards 679 EOS exports to EOSSDK_original.dll

.data
EXTERN g_eosProcs:QWORD

.code

EOS_Achievements_AddNotifyAchievementsUnlocked_proxy PROC
    jmp QWORD PTR [g_eosProcs + 0]
EOS_Achievements_AddNotifyAchievementsUnlocked_proxy ENDP

EOS_Achievements_AddNotifyAchievementsUnlockedV2_proxy PROC
    jmp QWORD PTR [g_eosProcs + 8]
EOS_Achievements_AddNotifyAchievementsUnlockedV2_proxy ENDP

EOS_Achievements_CopyAchievementDefinitionByAchievementId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 16]
EOS_Achievements_CopyAchievementDefinitionByAchievementId_proxy ENDP

EOS_Achievements_CopyAchievementDefinitionByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 24]
EOS_Achievements_CopyAchievementDefinitionByIndex_proxy ENDP

EOS_Achievements_CopyAchievementDefinitionV2ByAchievementId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 32]
EOS_Achievements_CopyAchievementDefinitionV2ByAchievementId_proxy ENDP

EOS_Achievements_CopyAchievementDefinitionV2ByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 40]
EOS_Achievements_CopyAchievementDefinitionV2ByIndex_proxy ENDP

EOS_Achievements_CopyPlayerAchievementByAchievementId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 48]
EOS_Achievements_CopyPlayerAchievementByAchievementId_proxy ENDP

EOS_Achievements_CopyPlayerAchievementByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 56]
EOS_Achievements_CopyPlayerAchievementByIndex_proxy ENDP

EOS_Achievements_CopyUnlockedAchievementByAchievementId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 64]
EOS_Achievements_CopyUnlockedAchievementByAchievementId_proxy ENDP

EOS_Achievements_CopyUnlockedAchievementByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 72]
EOS_Achievements_CopyUnlockedAchievementByIndex_proxy ENDP

EOS_Achievements_DefinitionV2_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 80]
EOS_Achievements_DefinitionV2_Release_proxy ENDP

EOS_Achievements_Definition_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 88]
EOS_Achievements_Definition_Release_proxy ENDP

EOS_Achievements_GetAchievementDefinitionCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 96]
EOS_Achievements_GetAchievementDefinitionCount_proxy ENDP

EOS_Achievements_GetPlayerAchievementCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 104]
EOS_Achievements_GetPlayerAchievementCount_proxy ENDP

EOS_Achievements_GetUnlockedAchievementCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 112]
EOS_Achievements_GetUnlockedAchievementCount_proxy ENDP

EOS_Achievements_PlayerAchievement_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 120]
EOS_Achievements_PlayerAchievement_Release_proxy ENDP

EOS_Achievements_QueryDefinitions_proxy PROC
    jmp QWORD PTR [g_eosProcs + 128]
EOS_Achievements_QueryDefinitions_proxy ENDP

EOS_Achievements_QueryPlayerAchievements_proxy PROC
    jmp QWORD PTR [g_eosProcs + 136]
EOS_Achievements_QueryPlayerAchievements_proxy ENDP

EOS_Achievements_RemoveNotifyAchievementsUnlocked_proxy PROC
    jmp QWORD PTR [g_eosProcs + 144]
EOS_Achievements_RemoveNotifyAchievementsUnlocked_proxy ENDP

EOS_Achievements_UnlockAchievements_proxy PROC
    jmp QWORD PTR [g_eosProcs + 152]
EOS_Achievements_UnlockAchievements_proxy ENDP

EOS_Achievements_UnlockedAchievement_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 160]
EOS_Achievements_UnlockedAchievement_Release_proxy ENDP

EOS_ActiveSession_CopyInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 168]
EOS_ActiveSession_CopyInfo_proxy ENDP

EOS_ActiveSession_GetRegisteredPlayerByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 176]
EOS_ActiveSession_GetRegisteredPlayerByIndex_proxy ENDP

EOS_ActiveSession_GetRegisteredPlayerCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 184]
EOS_ActiveSession_GetRegisteredPlayerCount_proxy ENDP

EOS_ActiveSession_Info_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 192]
EOS_ActiveSession_Info_Release_proxy ENDP

EOS_ActiveSession_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 200]
EOS_ActiveSession_Release_proxy ENDP

EOS_AntiCheatClient_AddExternalIntegrityCatalog_proxy PROC
    jmp QWORD PTR [g_eosProcs + 208]
EOS_AntiCheatClient_AddExternalIntegrityCatalog_proxy ENDP

EOS_AntiCheatClient_AddNotifyClientIntegrityViolated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 216]
EOS_AntiCheatClient_AddNotifyClientIntegrityViolated_proxy ENDP

EOS_AntiCheatClient_AddNotifyMessageToPeer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 224]
EOS_AntiCheatClient_AddNotifyMessageToPeer_proxy ENDP

EOS_AntiCheatClient_AddNotifyMessageToServer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 232]
EOS_AntiCheatClient_AddNotifyMessageToServer_proxy ENDP

EOS_AntiCheatClient_AddNotifyPeerActionRequired_proxy PROC
    jmp QWORD PTR [g_eosProcs + 240]
EOS_AntiCheatClient_AddNotifyPeerActionRequired_proxy ENDP

EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 248]
EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged_proxy ENDP

EOS_AntiCheatClient_BeginSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 256]
EOS_AntiCheatClient_BeginSession_proxy ENDP

EOS_AntiCheatClient_EndSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 264]
EOS_AntiCheatClient_EndSession_proxy ENDP

EOS_AntiCheatClient_GetModuleBuildId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 272]
EOS_AntiCheatClient_GetModuleBuildId_proxy ENDP

EOS_AntiCheatClient_GetProtectMessageOutputLength_proxy PROC
    jmp QWORD PTR [g_eosProcs + 280]
EOS_AntiCheatClient_GetProtectMessageOutputLength_proxy ENDP

EOS_AntiCheatClient_PollStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 288]
EOS_AntiCheatClient_PollStatus_proxy ENDP

EOS_AntiCheatClient_ProtectMessage_proxy PROC
    jmp QWORD PTR [g_eosProcs + 296]
EOS_AntiCheatClient_ProtectMessage_proxy ENDP

EOS_AntiCheatClient_ReceiveMessageFromPeer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 304]
EOS_AntiCheatClient_ReceiveMessageFromPeer_proxy ENDP

EOS_AntiCheatClient_ReceiveMessageFromServer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 312]
EOS_AntiCheatClient_ReceiveMessageFromServer_proxy ENDP

EOS_AntiCheatClient_RegisterPeer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 320]
EOS_AntiCheatClient_RegisterPeer_proxy ENDP

EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 328]
EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated_proxy ENDP

EOS_AntiCheatClient_RemoveNotifyMessageToPeer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 336]
EOS_AntiCheatClient_RemoveNotifyMessageToPeer_proxy ENDP

EOS_AntiCheatClient_RemoveNotifyMessageToServer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 344]
EOS_AntiCheatClient_RemoveNotifyMessageToServer_proxy ENDP

EOS_AntiCheatClient_RemoveNotifyPeerActionRequired_proxy PROC
    jmp QWORD PTR [g_eosProcs + 352]
EOS_AntiCheatClient_RemoveNotifyPeerActionRequired_proxy ENDP

EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 360]
EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged_proxy ENDP

EOS_AntiCheatClient_Reserved01_proxy PROC
    jmp QWORD PTR [g_eosProcs + 368]
EOS_AntiCheatClient_Reserved01_proxy ENDP

EOS_AntiCheatClient_Reserved02_proxy PROC
    jmp QWORD PTR [g_eosProcs + 376]
EOS_AntiCheatClient_Reserved02_proxy ENDP

EOS_AntiCheatClient_UnprotectMessage_proxy PROC
    jmp QWORD PTR [g_eosProcs + 384]
EOS_AntiCheatClient_UnprotectMessage_proxy ENDP

EOS_AntiCheatClient_UnregisterPeer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 392]
EOS_AntiCheatClient_UnregisterPeer_proxy ENDP

EOS_AntiCheatServer_AddNotifyClientActionRequired_proxy PROC
    jmp QWORD PTR [g_eosProcs + 400]
EOS_AntiCheatServer_AddNotifyClientActionRequired_proxy ENDP

EOS_AntiCheatServer_AddNotifyClientAuthStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 408]
EOS_AntiCheatServer_AddNotifyClientAuthStatusChanged_proxy ENDP

EOS_AntiCheatServer_AddNotifyMessageToClient_proxy PROC
    jmp QWORD PTR [g_eosProcs + 416]
EOS_AntiCheatServer_AddNotifyMessageToClient_proxy ENDP

EOS_AntiCheatServer_BeginSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 424]
EOS_AntiCheatServer_BeginSession_proxy ENDP

EOS_AntiCheatServer_EndSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 432]
EOS_AntiCheatServer_EndSession_proxy ENDP

EOS_AntiCheatServer_GetProtectMessageOutputLength_proxy PROC
    jmp QWORD PTR [g_eosProcs + 440]
EOS_AntiCheatServer_GetProtectMessageOutputLength_proxy ENDP

EOS_AntiCheatServer_LogEvent_proxy PROC
    jmp QWORD PTR [g_eosProcs + 448]
EOS_AntiCheatServer_LogEvent_proxy ENDP

EOS_AntiCheatServer_LogGameRoundEnd_proxy PROC
    jmp QWORD PTR [g_eosProcs + 456]
EOS_AntiCheatServer_LogGameRoundEnd_proxy ENDP

EOS_AntiCheatServer_LogGameRoundStart_proxy PROC
    jmp QWORD PTR [g_eosProcs + 464]
EOS_AntiCheatServer_LogGameRoundStart_proxy ENDP

EOS_AntiCheatServer_LogPlayerDespawn_proxy PROC
    jmp QWORD PTR [g_eosProcs + 472]
EOS_AntiCheatServer_LogPlayerDespawn_proxy ENDP

EOS_AntiCheatServer_LogPlayerRevive_proxy PROC
    jmp QWORD PTR [g_eosProcs + 480]
EOS_AntiCheatServer_LogPlayerRevive_proxy ENDP

EOS_AntiCheatServer_LogPlayerSpawn_proxy PROC
    jmp QWORD PTR [g_eosProcs + 488]
EOS_AntiCheatServer_LogPlayerSpawn_proxy ENDP

EOS_AntiCheatServer_LogPlayerTakeDamage_proxy PROC
    jmp QWORD PTR [g_eosProcs + 496]
EOS_AntiCheatServer_LogPlayerTakeDamage_proxy ENDP

EOS_AntiCheatServer_LogPlayerTick_proxy PROC
    jmp QWORD PTR [g_eosProcs + 504]
EOS_AntiCheatServer_LogPlayerTick_proxy ENDP

EOS_AntiCheatServer_LogPlayerUseAbility_proxy PROC
    jmp QWORD PTR [g_eosProcs + 512]
EOS_AntiCheatServer_LogPlayerUseAbility_proxy ENDP

EOS_AntiCheatServer_LogPlayerUseWeapon_proxy PROC
    jmp QWORD PTR [g_eosProcs + 520]
EOS_AntiCheatServer_LogPlayerUseWeapon_proxy ENDP

EOS_AntiCheatServer_ProtectMessage_proxy PROC
    jmp QWORD PTR [g_eosProcs + 528]
EOS_AntiCheatServer_ProtectMessage_proxy ENDP

EOS_AntiCheatServer_ReceiveMessageFromClient_proxy PROC
    jmp QWORD PTR [g_eosProcs + 536]
EOS_AntiCheatServer_ReceiveMessageFromClient_proxy ENDP

EOS_AntiCheatServer_RegisterClient_proxy PROC
    jmp QWORD PTR [g_eosProcs + 544]
EOS_AntiCheatServer_RegisterClient_proxy ENDP

EOS_AntiCheatServer_RegisterEvent_proxy PROC
    jmp QWORD PTR [g_eosProcs + 552]
EOS_AntiCheatServer_RegisterEvent_proxy ENDP

EOS_AntiCheatServer_RemoveNotifyClientActionRequired_proxy PROC
    jmp QWORD PTR [g_eosProcs + 560]
EOS_AntiCheatServer_RemoveNotifyClientActionRequired_proxy ENDP

EOS_AntiCheatServer_RemoveNotifyClientAuthStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 568]
EOS_AntiCheatServer_RemoveNotifyClientAuthStatusChanged_proxy ENDP

EOS_AntiCheatServer_RemoveNotifyMessageToClient_proxy PROC
    jmp QWORD PTR [g_eosProcs + 576]
EOS_AntiCheatServer_RemoveNotifyMessageToClient_proxy ENDP

EOS_AntiCheatServer_SetClientDetails_proxy PROC
    jmp QWORD PTR [g_eosProcs + 584]
EOS_AntiCheatServer_SetClientDetails_proxy ENDP

EOS_AntiCheatServer_SetClientNetworkState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 592]
EOS_AntiCheatServer_SetClientNetworkState_proxy ENDP

EOS_AntiCheatServer_SetGameSessionId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 600]
EOS_AntiCheatServer_SetGameSessionId_proxy ENDP

EOS_AntiCheatServer_UnprotectMessage_proxy PROC
    jmp QWORD PTR [g_eosProcs + 608]
EOS_AntiCheatServer_UnprotectMessage_proxy ENDP

EOS_AntiCheatServer_UnregisterClient_proxy PROC
    jmp QWORD PTR [g_eosProcs + 616]
EOS_AntiCheatServer_UnregisterClient_proxy ENDP

EOS_Audio_CreateNewInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 624]
EOS_Audio_CreateNewInputStream_proxy ENDP

EOS_Audio_CreateNewOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 632]
EOS_Audio_CreateNewOutputStream_proxy ENDP

EOS_Audio_DestroyInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 640]
EOS_Audio_DestroyInputStream_proxy ENDP

EOS_Audio_DestroyOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 648]
EOS_Audio_DestroyOutputStream_proxy ENDP

EOS_Audio_EnableCommunicationsModeOutputDevices_proxy PROC
    jmp QWORD PTR [g_eosProcs + 656]
EOS_Audio_EnableCommunicationsModeOutputDevices_proxy ENDP

EOS_Audio_GetInputDeviceInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 664]
EOS_Audio_GetInputDeviceInfo_proxy ENDP

EOS_Audio_GetInputStreamInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 672]
EOS_Audio_GetInputStreamInfo_proxy ENDP

EOS_Audio_GetOutputDeviceInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 680]
EOS_Audio_GetOutputDeviceInfo_proxy ENDP

EOS_Audio_GetOutputStreamInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 688]
EOS_Audio_GetOutputStreamInfo_proxy ENDP

EOS_Audio_IsInputStreamDeviceDisconnected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 696]
EOS_Audio_IsInputStreamDeviceDisconnected_proxy ENDP

EOS_Audio_IsInputStreamSilent_proxy PROC
    jmp QWORD PTR [g_eosProcs + 704]
EOS_Audio_IsInputStreamSilent_proxy ENDP

EOS_Audio_QueryInputDevices_proxy PROC
    jmp QWORD PTR [g_eosProcs + 712]
EOS_Audio_QueryInputDevices_proxy ENDP

EOS_Audio_QueryOutputDevices_proxy PROC
    jmp QWORD PTR [g_eosProcs + 720]
EOS_Audio_QueryOutputDevices_proxy ENDP

EOS_Audio_RegisterUser_proxy PROC
    jmp QWORD PTR [g_eosProcs + 728]
EOS_Audio_RegisterUser_proxy ENDP

EOS_Audio_RemoveNotifyDevicesChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 736]
EOS_Audio_RemoveNotifyDevicesChanged_proxy ENDP

EOS_Audio_SetFeatureEnabledForInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 744]
EOS_Audio_SetFeatureEnabledForInputStream_proxy ENDP

EOS_Audio_SetNotifyDevicesChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 752]
EOS_Audio_SetNotifyDevicesChanged_proxy ENDP

EOS_Audio_StartInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 760]
EOS_Audio_StartInputStream_proxy ENDP

EOS_Audio_StartOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 768]
EOS_Audio_StartOutputStream_proxy ENDP

EOS_Audio_StopInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 776]
EOS_Audio_StopInputStream_proxy ENDP

EOS_Audio_StopOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 784]
EOS_Audio_StopOutputStream_proxy ENDP

EOS_Audio_UnregisterUser_proxy PROC
    jmp QWORD PTR [g_eosProcs + 792]
EOS_Audio_UnregisterUser_proxy ENDP

EOS_Auth_AddNotifyLoginStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 800]
EOS_Auth_AddNotifyLoginStatusChanged_proxy ENDP

EOS_Auth_CopyIdToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 808]
EOS_Auth_CopyIdToken_proxy ENDP

EOS_Auth_CopyUserAuthToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 816]
EOS_Auth_CopyUserAuthToken_proxy ENDP

EOS_Auth_DeletePersistentAuth_proxy PROC
    jmp QWORD PTR [g_eosProcs + 824]
EOS_Auth_DeletePersistentAuth_proxy ENDP

EOS_Auth_GetLoggedInAccountByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 832]
EOS_Auth_GetLoggedInAccountByIndex_proxy ENDP

EOS_Auth_GetLoggedInAccountsCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 840]
EOS_Auth_GetLoggedInAccountsCount_proxy ENDP

EOS_Auth_GetLoginStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 848]
EOS_Auth_GetLoginStatus_proxy ENDP

EOS_Auth_GetMergedAccountByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 856]
EOS_Auth_GetMergedAccountByIndex_proxy ENDP

EOS_Auth_GetMergedAccountsCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 864]
EOS_Auth_GetMergedAccountsCount_proxy ENDP

EOS_Auth_GetSelectedAccountId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 872]
EOS_Auth_GetSelectedAccountId_proxy ENDP

EOS_Auth_IdToken_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 880]
EOS_Auth_IdToken_Release_proxy ENDP

EOS_Auth_LinkAccount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 888]
EOS_Auth_LinkAccount_proxy ENDP

EOS_Auth_Login_proxy PROC
    jmp QWORD PTR [g_eosProcs + 896]
EOS_Auth_Login_proxy ENDP

EOS_Auth_Logout_proxy PROC
    jmp QWORD PTR [g_eosProcs + 904]
EOS_Auth_Logout_proxy ENDP

EOS_Auth_QueryIdToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 912]
EOS_Auth_QueryIdToken_proxy ENDP

EOS_Auth_RemoveNotifyLoginStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 920]
EOS_Auth_RemoveNotifyLoginStatusChanged_proxy ENDP

EOS_Auth_Token_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 928]
EOS_Auth_Token_Release_proxy ENDP

EOS_Auth_VerifyIdToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 936]
EOS_Auth_VerifyIdToken_proxy ENDP

EOS_Auth_VerifyUserAuth_proxy PROC
    jmp QWORD PTR [g_eosProcs + 944]
EOS_Auth_VerifyUserAuth_proxy ENDP

EOS_BeginScopeEvent_proxy PROC
    jmp QWORD PTR [g_eosProcs + 952]
EOS_BeginScopeEvent_proxy ENDP

EOS_BroadcastAudio_CreateNewInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 960]
EOS_BroadcastAudio_CreateNewInputStream_proxy ENDP

EOS_BroadcastAudio_CreateNewOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 968]
EOS_BroadcastAudio_CreateNewOutputStream_proxy ENDP

EOS_BroadcastAudio_DestroyInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 976]
EOS_BroadcastAudio_DestroyInputStream_proxy ENDP

EOS_BroadcastAudio_DestroyOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 984]
EOS_BroadcastAudio_DestroyOutputStream_proxy ENDP

EOS_BroadcastAudio_GetCurrentGainLevel_proxy PROC
    jmp QWORD PTR [g_eosProcs + 992]
EOS_BroadcastAudio_GetCurrentGainLevel_proxy ENDP

EOS_BroadcastAudio_GetCurrentMicAmplitude_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1000]
EOS_BroadcastAudio_GetCurrentMicAmplitude_proxy ENDP

EOS_BroadcastAudio_GetInputStreamInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1008]
EOS_BroadcastAudio_GetInputStreamInfo_proxy ENDP

EOS_BroadcastAudio_GetOutputStreamInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1016]
EOS_BroadcastAudio_GetOutputStreamInfo_proxy ENDP

EOS_BroadcastAudio_PushPacketToOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1024]
EOS_BroadcastAudio_PushPacketToOutputStream_proxy ENDP

EOS_BroadcastAudio_SetEncoderSettings_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1032]
EOS_BroadcastAudio_SetEncoderSettings_proxy ENDP

EOS_BroadcastAudio_SetMicProcessingSettings_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1040]
EOS_BroadcastAudio_SetMicProcessingSettings_proxy ENDP

EOS_BroadcastAudio_StartInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1048]
EOS_BroadcastAudio_StartInputStream_proxy ENDP

EOS_BroadcastAudio_StartOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1056]
EOS_BroadcastAudio_StartOutputStream_proxy ENDP

EOS_BroadcastAudio_StopInputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1064]
EOS_BroadcastAudio_StopInputStream_proxy ENDP

EOS_BroadcastAudio_StopOutputStream_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1072]
EOS_BroadcastAudio_StopOutputStream_proxy ENDP

EOS_ByteArray_ToString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1080]
EOS_ByteArray_ToString_proxy ENDP

EOS_Connect_AddNotifyAuthExpiration_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1088]
EOS_Connect_AddNotifyAuthExpiration_proxy ENDP

EOS_Connect_AddNotifyLoginStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1096]
EOS_Connect_AddNotifyLoginStatusChanged_proxy ENDP

EOS_Connect_CopyIdToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1104]
EOS_Connect_CopyIdToken_proxy ENDP

EOS_Connect_CopyProductUserExternalAccountByAccountId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1112]
EOS_Connect_CopyProductUserExternalAccountByAccountId_proxy ENDP

EOS_Connect_CopyProductUserExternalAccountByAccountType_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1120]
EOS_Connect_CopyProductUserExternalAccountByAccountType_proxy ENDP

EOS_Connect_CopyProductUserExternalAccountByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1128]
EOS_Connect_CopyProductUserExternalAccountByIndex_proxy ENDP

EOS_Connect_CopyProductUserInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1136]
EOS_Connect_CopyProductUserInfo_proxy ENDP

EOS_Connect_CreateDeviceId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1144]
EOS_Connect_CreateDeviceId_proxy ENDP

EOS_Connect_CreateUser_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1152]
EOS_Connect_CreateUser_proxy ENDP

EOS_Connect_DeleteDeviceId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1160]
EOS_Connect_DeleteDeviceId_proxy ENDP

EOS_Connect_ExternalAccountInfo_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1168]
EOS_Connect_ExternalAccountInfo_Release_proxy ENDP

EOS_Connect_GetExternalAccountMapping_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1176]
EOS_Connect_GetExternalAccountMapping_proxy ENDP

EOS_Connect_GetLoggedInUserByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1184]
EOS_Connect_GetLoggedInUserByIndex_proxy ENDP

EOS_Connect_GetLoggedInUsersCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1192]
EOS_Connect_GetLoggedInUsersCount_proxy ENDP

EOS_Connect_GetLoginStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1200]
EOS_Connect_GetLoginStatus_proxy ENDP

EOS_Connect_GetProductUserExternalAccountCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1208]
EOS_Connect_GetProductUserExternalAccountCount_proxy ENDP

EOS_Connect_GetProductUserIdMapping_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1216]
EOS_Connect_GetProductUserIdMapping_proxy ENDP

EOS_Connect_IdToken_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1224]
EOS_Connect_IdToken_Release_proxy ENDP

EOS_Connect_LinkAccount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1232]
EOS_Connect_LinkAccount_proxy ENDP

EOS_Connect_Login_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1240]
EOS_Connect_Login_proxy ENDP

EOS_Connect_Logout_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1248]
EOS_Connect_Logout_proxy ENDP

EOS_Connect_QueryExternalAccountMappings_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1256]
EOS_Connect_QueryExternalAccountMappings_proxy ENDP

EOS_Connect_QueryProductUserIdMappings_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1264]
EOS_Connect_QueryProductUserIdMappings_proxy ENDP

EOS_Connect_RemoveNotifyAuthExpiration_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1272]
EOS_Connect_RemoveNotifyAuthExpiration_proxy ENDP

EOS_Connect_RemoveNotifyLoginStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1280]
EOS_Connect_RemoveNotifyLoginStatusChanged_proxy ENDP

EOS_Connect_TransferDeviceIdAccount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1288]
EOS_Connect_TransferDeviceIdAccount_proxy ENDP

EOS_Connect_UnlinkAccount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1296]
EOS_Connect_UnlinkAccount_proxy ENDP

EOS_Connect_VerifyIdToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1304]
EOS_Connect_VerifyIdToken_proxy ENDP

EOS_ContinuanceToken_ToString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1312]
EOS_ContinuanceToken_ToString_proxy ENDP

EOS_CustomInvites_AcceptRequestToJoin_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1320]
EOS_CustomInvites_AcceptRequestToJoin_proxy ENDP

EOS_CustomInvites_AddNotifyCustomInviteAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1328]
EOS_CustomInvites_AddNotifyCustomInviteAccepted_proxy ENDP

EOS_CustomInvites_AddNotifyCustomInviteReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1336]
EOS_CustomInvites_AddNotifyCustomInviteReceived_proxy ENDP

EOS_CustomInvites_AddNotifyCustomInviteRejected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1344]
EOS_CustomInvites_AddNotifyCustomInviteRejected_proxy ENDP

EOS_CustomInvites_AddNotifyRequestToJoinAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1352]
EOS_CustomInvites_AddNotifyRequestToJoinAccepted_proxy ENDP

EOS_CustomInvites_AddNotifyRequestToJoinReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1360]
EOS_CustomInvites_AddNotifyRequestToJoinReceived_proxy ENDP

EOS_CustomInvites_AddNotifyRequestToJoinRejected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1368]
EOS_CustomInvites_AddNotifyRequestToJoinRejected_proxy ENDP

EOS_CustomInvites_AddNotifyRequestToJoinResponseReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1376]
EOS_CustomInvites_AddNotifyRequestToJoinResponseReceived_proxy ENDP

EOS_CustomInvites_AddNotifySendCustomNativeInviteRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1384]
EOS_CustomInvites_AddNotifySendCustomNativeInviteRequested_proxy ENDP

EOS_CustomInvites_FinalizeInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1392]
EOS_CustomInvites_FinalizeInvite_proxy ENDP

EOS_CustomInvites_RejectRequestToJoin_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1400]
EOS_CustomInvites_RejectRequestToJoin_proxy ENDP

EOS_CustomInvites_RemoveNotifyCustomInviteAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1408]
EOS_CustomInvites_RemoveNotifyCustomInviteAccepted_proxy ENDP

EOS_CustomInvites_RemoveNotifyCustomInviteReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1416]
EOS_CustomInvites_RemoveNotifyCustomInviteReceived_proxy ENDP

EOS_CustomInvites_RemoveNotifyCustomInviteRejected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1424]
EOS_CustomInvites_RemoveNotifyCustomInviteRejected_proxy ENDP

EOS_CustomInvites_RemoveNotifyRequestToJoinAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1432]
EOS_CustomInvites_RemoveNotifyRequestToJoinAccepted_proxy ENDP

EOS_CustomInvites_RemoveNotifyRequestToJoinReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1440]
EOS_CustomInvites_RemoveNotifyRequestToJoinReceived_proxy ENDP

EOS_CustomInvites_RemoveNotifyRequestToJoinRejected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1448]
EOS_CustomInvites_RemoveNotifyRequestToJoinRejected_proxy ENDP

EOS_CustomInvites_RemoveNotifyRequestToJoinResponseReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1456]
EOS_CustomInvites_RemoveNotifyRequestToJoinResponseReceived_proxy ENDP

EOS_CustomInvites_RemoveNotifySendCustomNativeInviteRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1464]
EOS_CustomInvites_RemoveNotifySendCustomNativeInviteRequested_proxy ENDP

EOS_CustomInvites_SendCustomInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1472]
EOS_CustomInvites_SendCustomInvite_proxy ENDP

EOS_CustomInvites_SendRequestToJoin_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1480]
EOS_CustomInvites_SendRequestToJoin_proxy ENDP

EOS_CustomInvites_SetCustomInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1488]
EOS_CustomInvites_SetCustomInvite_proxy ENDP

EOS_EApplicationStatus_ToString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1496]
EOS_EApplicationStatus_ToString_proxy ENDP

EOS_ENetworkStatus_ToString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1504]
EOS_ENetworkStatus_ToString_proxy ENDP

EOS_EResult_IsOperationComplete_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1512]
EOS_EResult_IsOperationComplete_proxy ENDP

EOS_EResult_ToString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1520]
EOS_EResult_ToString_proxy ENDP

EOS_Ecom_CatalogItem_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1528]
EOS_Ecom_CatalogItem_Release_proxy ENDP

EOS_Ecom_CatalogOffer_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1536]
EOS_Ecom_CatalogOffer_Release_proxy ENDP

EOS_Ecom_CatalogRelease_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1544]
EOS_Ecom_CatalogRelease_Release_proxy ENDP

EOS_Ecom_Checkout_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1552]
EOS_Ecom_Checkout_proxy ENDP

EOS_Ecom_CopyEntitlementById_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1560]
EOS_Ecom_CopyEntitlementById_proxy ENDP

EOS_Ecom_CopyEntitlementByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1568]
EOS_Ecom_CopyEntitlementByIndex_proxy ENDP

EOS_Ecom_CopyEntitlementByNameAndIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1576]
EOS_Ecom_CopyEntitlementByNameAndIndex_proxy ENDP

EOS_Ecom_CopyItemById_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1584]
EOS_Ecom_CopyItemById_proxy ENDP

EOS_Ecom_CopyItemImageInfoByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1592]
EOS_Ecom_CopyItemImageInfoByIndex_proxy ENDP

EOS_Ecom_CopyItemReleaseByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1600]
EOS_Ecom_CopyItemReleaseByIndex_proxy ENDP

EOS_Ecom_CopyLastRedeemEntitlementsResultByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1608]
EOS_Ecom_CopyLastRedeemEntitlementsResultByIndex_proxy ENDP

EOS_Ecom_CopyLastRedeemedEntitlementByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1616]
EOS_Ecom_CopyLastRedeemedEntitlementByIndex_proxy ENDP

EOS_Ecom_CopyOfferById_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1624]
EOS_Ecom_CopyOfferById_proxy ENDP

EOS_Ecom_CopyOfferByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1632]
EOS_Ecom_CopyOfferByIndex_proxy ENDP

EOS_Ecom_CopyOfferImageInfoByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1640]
EOS_Ecom_CopyOfferImageInfoByIndex_proxy ENDP

EOS_Ecom_CopyOfferItemByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1648]
EOS_Ecom_CopyOfferItemByIndex_proxy ENDP

EOS_Ecom_CopyTransactionById_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1656]
EOS_Ecom_CopyTransactionById_proxy ENDP

EOS_Ecom_CopyTransactionByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1664]
EOS_Ecom_CopyTransactionByIndex_proxy ENDP

EOS_Ecom_Entitlement_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1672]
EOS_Ecom_Entitlement_Release_proxy ENDP

EOS_Ecom_GetEntitlementsByNameCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1680]
EOS_Ecom_GetEntitlementsByNameCount_proxy ENDP

EOS_Ecom_GetEntitlementsCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1688]
EOS_Ecom_GetEntitlementsCount_proxy ENDP

EOS_Ecom_GetItemImageInfoCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1696]
EOS_Ecom_GetItemImageInfoCount_proxy ENDP

EOS_Ecom_GetItemReleaseCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1704]
EOS_Ecom_GetItemReleaseCount_proxy ENDP

EOS_Ecom_GetLastRedeemEntitlementsResultCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1712]
EOS_Ecom_GetLastRedeemEntitlementsResultCount_proxy ENDP

EOS_Ecom_GetLastRedeemedEntitlementsCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1720]
EOS_Ecom_GetLastRedeemedEntitlementsCount_proxy ENDP

EOS_Ecom_GetOfferCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1728]
EOS_Ecom_GetOfferCount_proxy ENDP

EOS_Ecom_GetOfferImageInfoCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1736]
EOS_Ecom_GetOfferImageInfoCount_proxy ENDP

EOS_Ecom_GetOfferItemCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1744]
EOS_Ecom_GetOfferItemCount_proxy ENDP

EOS_Ecom_GetTransactionCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1752]
EOS_Ecom_GetTransactionCount_proxy ENDP

EOS_Ecom_KeyImageInfo_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1760]
EOS_Ecom_KeyImageInfo_Release_proxy ENDP

EOS_Ecom_QueryEntitlementToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1768]
EOS_Ecom_QueryEntitlementToken_proxy ENDP

EOS_Ecom_QueryEntitlements_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1776]
EOS_Ecom_QueryEntitlements_proxy ENDP

EOS_Ecom_QueryOffers_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1784]
EOS_Ecom_QueryOffers_proxy ENDP

EOS_Ecom_QueryOwnership_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1792]
EOS_Ecom_QueryOwnership_proxy ENDP

EOS_Ecom_QueryOwnershipBySandboxIds_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1800]
EOS_Ecom_QueryOwnershipBySandboxIds_proxy ENDP

EOS_Ecom_QueryOwnershipToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1808]
EOS_Ecom_QueryOwnershipToken_proxy ENDP

EOS_Ecom_RedeemEntitlements_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1816]
EOS_Ecom_RedeemEntitlements_proxy ENDP

EOS_Ecom_Transaction_CopyEntitlementByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1824]
EOS_Ecom_Transaction_CopyEntitlementByIndex_proxy ENDP

EOS_Ecom_Transaction_GetEntitlementsCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1832]
EOS_Ecom_Transaction_GetEntitlementsCount_proxy ENDP

EOS_Ecom_Transaction_GetTransactionId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1840]
EOS_Ecom_Transaction_GetTransactionId_proxy ENDP

EOS_Ecom_Transaction_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1848]
EOS_Ecom_Transaction_Release_proxy ENDP

EOS_EndScopeEvent_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1856]
EOS_EndScopeEvent_proxy ENDP

EOS_EpicAccountId_FromString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1864]
EOS_EpicAccountId_FromString_proxy ENDP

EOS_EpicAccountId_IsValid_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1872]
EOS_EpicAccountId_IsValid_proxy ENDP

EOS_EpicAccountId_ToString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1880]
EOS_EpicAccountId_ToString_proxy ENDP

EOS_Friends_AcceptInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1888]
EOS_Friends_AcceptInvite_proxy ENDP

EOS_Friends_AddNotifyBlockedUsersUpdate_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1896]
EOS_Friends_AddNotifyBlockedUsersUpdate_proxy ENDP

EOS_Friends_AddNotifyFriendsUpdate_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1904]
EOS_Friends_AddNotifyFriendsUpdate_proxy ENDP

EOS_Friends_GetBlockedUserAtIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1912]
EOS_Friends_GetBlockedUserAtIndex_proxy ENDP

EOS_Friends_GetBlockedUsersCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1920]
EOS_Friends_GetBlockedUsersCount_proxy ENDP

EOS_Friends_GetFriendAtIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1928]
EOS_Friends_GetFriendAtIndex_proxy ENDP

EOS_Friends_GetFriendsCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1936]
EOS_Friends_GetFriendsCount_proxy ENDP

EOS_Friends_GetStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1944]
EOS_Friends_GetStatus_proxy ENDP

EOS_Friends_QueryFriends_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1952]
EOS_Friends_QueryFriends_proxy ENDP

EOS_Friends_RejectInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1960]
EOS_Friends_RejectInvite_proxy ENDP

EOS_Friends_RemoveNotifyBlockedUsersUpdate_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1968]
EOS_Friends_RemoveNotifyBlockedUsersUpdate_proxy ENDP

EOS_Friends_RemoveNotifyFriendsUpdate_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1976]
EOS_Friends_RemoveNotifyFriendsUpdate_proxy ENDP

EOS_Friends_SendInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1984]
EOS_Friends_SendInvite_proxy ENDP

EOS_GetVersion_proxy PROC
    jmp QWORD PTR [g_eosProcs + 1992]
EOS_GetVersion_proxy ENDP

EOS_Initialize_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2000]
EOS_Initialize_proxy ENDP

EOS_IntegratedPlatformOptionsContainer_Add_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2008]
EOS_IntegratedPlatformOptionsContainer_Add_proxy ENDP

EOS_IntegratedPlatformOptionsContainer_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2016]
EOS_IntegratedPlatformOptionsContainer_Release_proxy ENDP

EOS_IntegratedPlatform_AddNotifyUserLoginStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2024]
EOS_IntegratedPlatform_AddNotifyUserLoginStatusChanged_proxy ENDP

EOS_IntegratedPlatform_ClearUserPreLogoutCallback_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2032]
EOS_IntegratedPlatform_ClearUserPreLogoutCallback_proxy ENDP

EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2040]
EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer_proxy ENDP

EOS_IntegratedPlatform_FinalizeDeferredUserLogout_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2048]
EOS_IntegratedPlatform_FinalizeDeferredUserLogout_proxy ENDP

EOS_IntegratedPlatform_RemoveNotifyUserLoginStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2056]
EOS_IntegratedPlatform_RemoveNotifyUserLoginStatusChanged_proxy ENDP

EOS_IntegratedPlatform_SetUserLoginStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2064]
EOS_IntegratedPlatform_SetUserLoginStatus_proxy ENDP

EOS_IntegratedPlatform_SetUserPreLogoutCallback_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2072]
EOS_IntegratedPlatform_SetUserPreLogoutCallback_proxy ENDP

EOS_KWS_AddNotifyPermissionsUpdateReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2080]
EOS_KWS_AddNotifyPermissionsUpdateReceived_proxy ENDP

EOS_KWS_CopyPermissionByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2088]
EOS_KWS_CopyPermissionByIndex_proxy ENDP

EOS_KWS_CreateUser_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2096]
EOS_KWS_CreateUser_proxy ENDP

EOS_KWS_GetPermissionByKey_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2104]
EOS_KWS_GetPermissionByKey_proxy ENDP

EOS_KWS_GetPermissionsCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2112]
EOS_KWS_GetPermissionsCount_proxy ENDP

EOS_KWS_PermissionStatus_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2120]
EOS_KWS_PermissionStatus_Release_proxy ENDP

EOS_KWS_QueryAgeGate_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2128]
EOS_KWS_QueryAgeGate_proxy ENDP

EOS_KWS_QueryPermissions_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2136]
EOS_KWS_QueryPermissions_proxy ENDP

EOS_KWS_RemoveNotifyPermissionsUpdateReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2144]
EOS_KWS_RemoveNotifyPermissionsUpdateReceived_proxy ENDP

EOS_KWS_RequestPermissions_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2152]
EOS_KWS_RequestPermissions_proxy ENDP

EOS_KWS_UpdateParentEmail_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2160]
EOS_KWS_UpdateParentEmail_proxy ENDP

EOS_Leaderboards_CopyLeaderboardDefinitionByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2168]
EOS_Leaderboards_CopyLeaderboardDefinitionByIndex_proxy ENDP

EOS_Leaderboards_CopyLeaderboardDefinitionByLeaderboardId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2176]
EOS_Leaderboards_CopyLeaderboardDefinitionByLeaderboardId_proxy ENDP

EOS_Leaderboards_CopyLeaderboardRecordByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2184]
EOS_Leaderboards_CopyLeaderboardRecordByIndex_proxy ENDP

EOS_Leaderboards_CopyLeaderboardRecordByUserId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2192]
EOS_Leaderboards_CopyLeaderboardRecordByUserId_proxy ENDP

EOS_Leaderboards_CopyLeaderboardUserScoreByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2200]
EOS_Leaderboards_CopyLeaderboardUserScoreByIndex_proxy ENDP

EOS_Leaderboards_CopyLeaderboardUserScoreByUserId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2208]
EOS_Leaderboards_CopyLeaderboardUserScoreByUserId_proxy ENDP

EOS_Leaderboards_Definition_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2216]
EOS_Leaderboards_Definition_Release_proxy ENDP

EOS_Leaderboards_GetLeaderboardDefinitionCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2224]
EOS_Leaderboards_GetLeaderboardDefinitionCount_proxy ENDP

EOS_Leaderboards_GetLeaderboardRecordCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2232]
EOS_Leaderboards_GetLeaderboardRecordCount_proxy ENDP

EOS_Leaderboards_GetLeaderboardUserScoreCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2240]
EOS_Leaderboards_GetLeaderboardUserScoreCount_proxy ENDP

EOS_Leaderboards_LeaderboardDefinition_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2248]
EOS_Leaderboards_LeaderboardDefinition_Release_proxy ENDP

EOS_Leaderboards_LeaderboardRecord_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2256]
EOS_Leaderboards_LeaderboardRecord_Release_proxy ENDP

EOS_Leaderboards_LeaderboardUserScore_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2264]
EOS_Leaderboards_LeaderboardUserScore_Release_proxy ENDP

EOS_Leaderboards_QueryLeaderboardDefinitions_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2272]
EOS_Leaderboards_QueryLeaderboardDefinitions_proxy ENDP

EOS_Leaderboards_QueryLeaderboardRanks_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2280]
EOS_Leaderboards_QueryLeaderboardRanks_proxy ENDP

EOS_Leaderboards_QueryLeaderboardUserScores_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2288]
EOS_Leaderboards_QueryLeaderboardUserScores_proxy ENDP

EOS_LobbyDetails_CopyAttributeByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2296]
EOS_LobbyDetails_CopyAttributeByIndex_proxy ENDP

EOS_LobbyDetails_CopyAttributeByKey_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2304]
EOS_LobbyDetails_CopyAttributeByKey_proxy ENDP

EOS_LobbyDetails_CopyInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2312]
EOS_LobbyDetails_CopyInfo_proxy ENDP

EOS_LobbyDetails_CopyMemberAttributeByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2320]
EOS_LobbyDetails_CopyMemberAttributeByIndex_proxy ENDP

EOS_LobbyDetails_CopyMemberAttributeByKey_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2328]
EOS_LobbyDetails_CopyMemberAttributeByKey_proxy ENDP

EOS_LobbyDetails_CopyMemberInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2336]
EOS_LobbyDetails_CopyMemberInfo_proxy ENDP

EOS_LobbyDetails_GetAttributeCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2344]
EOS_LobbyDetails_GetAttributeCount_proxy ENDP

EOS_LobbyDetails_GetLobbyOwner_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2352]
EOS_LobbyDetails_GetLobbyOwner_proxy ENDP

EOS_LobbyDetails_GetMemberAttributeCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2360]
EOS_LobbyDetails_GetMemberAttributeCount_proxy ENDP

EOS_LobbyDetails_GetMemberByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2368]
EOS_LobbyDetails_GetMemberByIndex_proxy ENDP

EOS_LobbyDetails_GetMemberCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2376]
EOS_LobbyDetails_GetMemberCount_proxy ENDP

EOS_LobbyDetails_Info_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2384]
EOS_LobbyDetails_Info_Release_proxy ENDP

EOS_LobbyDetails_MemberInfo_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2392]
EOS_LobbyDetails_MemberInfo_Release_proxy ENDP

EOS_LobbyDetails_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2400]
EOS_LobbyDetails_Release_proxy ENDP

EOS_LobbyModification_AddAttribute_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2408]
EOS_LobbyModification_AddAttribute_proxy ENDP

EOS_LobbyModification_AddMemberAttribute_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2416]
EOS_LobbyModification_AddMemberAttribute_proxy ENDP

EOS_LobbyModification_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2424]
EOS_LobbyModification_Release_proxy ENDP

EOS_LobbyModification_RemoveAttribute_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2432]
EOS_LobbyModification_RemoveAttribute_proxy ENDP

EOS_LobbyModification_RemoveMemberAttribute_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2440]
EOS_LobbyModification_RemoveMemberAttribute_proxy ENDP

EOS_LobbyModification_SetAllowedPlatformIds_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2448]
EOS_LobbyModification_SetAllowedPlatformIds_proxy ENDP

EOS_LobbyModification_SetBucketId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2456]
EOS_LobbyModification_SetBucketId_proxy ENDP

EOS_LobbyModification_SetInvitesAllowed_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2464]
EOS_LobbyModification_SetInvitesAllowed_proxy ENDP

EOS_LobbyModification_SetMaxMembers_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2472]
EOS_LobbyModification_SetMaxMembers_proxy ENDP

EOS_LobbyModification_SetPermissionLevel_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2480]
EOS_LobbyModification_SetPermissionLevel_proxy ENDP

EOS_LobbySearch_CopySearchResultByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2488]
EOS_LobbySearch_CopySearchResultByIndex_proxy ENDP

EOS_LobbySearch_Find_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2496]
EOS_LobbySearch_Find_proxy ENDP

EOS_LobbySearch_GetSearchResultCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2504]
EOS_LobbySearch_GetSearchResultCount_proxy ENDP

EOS_LobbySearch_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2512]
EOS_LobbySearch_Release_proxy ENDP

EOS_LobbySearch_RemoveParameter_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2520]
EOS_LobbySearch_RemoveParameter_proxy ENDP

EOS_LobbySearch_SetLobbyId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2528]
EOS_LobbySearch_SetLobbyId_proxy ENDP

EOS_LobbySearch_SetMaxResults_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2536]
EOS_LobbySearch_SetMaxResults_proxy ENDP

EOS_LobbySearch_SetParameter_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2544]
EOS_LobbySearch_SetParameter_proxy ENDP

EOS_LobbySearch_SetTargetUserId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2552]
EOS_LobbySearch_SetTargetUserId_proxy ENDP

EOS_Lobby_AddNotifyJoinLobbyAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2560]
EOS_Lobby_AddNotifyJoinLobbyAccepted_proxy ENDP

EOS_Lobby_AddNotifyLeaveLobbyRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2568]
EOS_Lobby_AddNotifyLeaveLobbyRequested_proxy ENDP

EOS_Lobby_AddNotifyLobbyInviteAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2576]
EOS_Lobby_AddNotifyLobbyInviteAccepted_proxy ENDP

EOS_Lobby_AddNotifyLobbyInviteReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2584]
EOS_Lobby_AddNotifyLobbyInviteReceived_proxy ENDP

EOS_Lobby_AddNotifyLobbyInviteRejected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2592]
EOS_Lobby_AddNotifyLobbyInviteRejected_proxy ENDP

EOS_Lobby_AddNotifyLobbyMemberStatusReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2600]
EOS_Lobby_AddNotifyLobbyMemberStatusReceived_proxy ENDP

EOS_Lobby_AddNotifyLobbyMemberUpdateReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2608]
EOS_Lobby_AddNotifyLobbyMemberUpdateReceived_proxy ENDP

EOS_Lobby_AddNotifyLobbyUpdateReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2616]
EOS_Lobby_AddNotifyLobbyUpdateReceived_proxy ENDP

EOS_Lobby_AddNotifyRTCRoomConnectionChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2624]
EOS_Lobby_AddNotifyRTCRoomConnectionChanged_proxy ENDP

EOS_Lobby_AddNotifySendLobbyNativeInviteRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2632]
EOS_Lobby_AddNotifySendLobbyNativeInviteRequested_proxy ENDP

EOS_Lobby_Attribute_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2640]
EOS_Lobby_Attribute_Release_proxy ENDP

EOS_Lobby_CopyLobbyDetailsHandle_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2648]
EOS_Lobby_CopyLobbyDetailsHandle_proxy ENDP

EOS_Lobby_CopyLobbyDetailsHandleByInviteId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2656]
EOS_Lobby_CopyLobbyDetailsHandleByInviteId_proxy ENDP

EOS_Lobby_CopyLobbyDetailsHandleByUiEventId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2664]
EOS_Lobby_CopyLobbyDetailsHandleByUiEventId_proxy ENDP

EOS_Lobby_CreateLobby_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2672]
EOS_Lobby_CreateLobby_proxy ENDP

EOS_Lobby_CreateLobbySearch_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2680]
EOS_Lobby_CreateLobbySearch_proxy ENDP

EOS_Lobby_DestroyLobby_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2688]
EOS_Lobby_DestroyLobby_proxy ENDP

EOS_Lobby_GetConnectString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2696]
EOS_Lobby_GetConnectString_proxy ENDP

EOS_Lobby_GetInviteCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2704]
EOS_Lobby_GetInviteCount_proxy ENDP

EOS_Lobby_GetInviteIdByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2712]
EOS_Lobby_GetInviteIdByIndex_proxy ENDP

EOS_Lobby_GetRTCRoomName_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2720]
EOS_Lobby_GetRTCRoomName_proxy ENDP

EOS_Lobby_HardMuteMember_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2728]
EOS_Lobby_HardMuteMember_proxy ENDP

EOS_Lobby_IsRTCRoomConnected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2736]
EOS_Lobby_IsRTCRoomConnected_proxy ENDP

EOS_Lobby_JoinLobby_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2744]
EOS_Lobby_JoinLobby_proxy ENDP

EOS_Lobby_JoinLobbyById_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2752]
EOS_Lobby_JoinLobbyById_proxy ENDP

EOS_Lobby_JoinRTCRoom_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2760]
EOS_Lobby_JoinRTCRoom_proxy ENDP

EOS_Lobby_KickMember_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2768]
EOS_Lobby_KickMember_proxy ENDP

EOS_Lobby_LeaveLobby_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2776]
EOS_Lobby_LeaveLobby_proxy ENDP

EOS_Lobby_LeaveRTCRoom_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2784]
EOS_Lobby_LeaveRTCRoom_proxy ENDP

EOS_Lobby_ParseConnectString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2792]
EOS_Lobby_ParseConnectString_proxy ENDP

EOS_Lobby_PromoteMember_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2800]
EOS_Lobby_PromoteMember_proxy ENDP

EOS_Lobby_QueryInvites_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2808]
EOS_Lobby_QueryInvites_proxy ENDP

EOS_Lobby_RejectInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2816]
EOS_Lobby_RejectInvite_proxy ENDP

EOS_Lobby_RemoveNotifyJoinLobbyAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2824]
EOS_Lobby_RemoveNotifyJoinLobbyAccepted_proxy ENDP

EOS_Lobby_RemoveNotifyLeaveLobbyRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2832]
EOS_Lobby_RemoveNotifyLeaveLobbyRequested_proxy ENDP

EOS_Lobby_RemoveNotifyLobbyInviteAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2840]
EOS_Lobby_RemoveNotifyLobbyInviteAccepted_proxy ENDP

EOS_Lobby_RemoveNotifyLobbyInviteReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2848]
EOS_Lobby_RemoveNotifyLobbyInviteReceived_proxy ENDP

EOS_Lobby_RemoveNotifyLobbyInviteRejected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2856]
EOS_Lobby_RemoveNotifyLobbyInviteRejected_proxy ENDP

EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2864]
EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived_proxy ENDP

EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2872]
EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived_proxy ENDP

EOS_Lobby_RemoveNotifyLobbyUpdateReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2880]
EOS_Lobby_RemoveNotifyLobbyUpdateReceived_proxy ENDP

EOS_Lobby_RemoveNotifyRTCRoomConnectionChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2888]
EOS_Lobby_RemoveNotifyRTCRoomConnectionChanged_proxy ENDP

EOS_Lobby_RemoveNotifySendLobbyNativeInviteRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2896]
EOS_Lobby_RemoveNotifySendLobbyNativeInviteRequested_proxy ENDP

EOS_Lobby_SendInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2904]
EOS_Lobby_SendInvite_proxy ENDP

EOS_Lobby_UpdateLobby_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2912]
EOS_Lobby_UpdateLobby_proxy ENDP

EOS_Lobby_UpdateLobbyModification_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2920]
EOS_Lobby_UpdateLobbyModification_proxy ENDP

EOS_Logging_SetCallback_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2928]
EOS_Logging_SetCallback_proxy ENDP

EOS_Logging_SetLogLevel_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2936]
EOS_Logging_SetLogLevel_proxy ENDP

EOS_Mercury_Initialize_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2944]
EOS_Mercury_Initialize_proxy ENDP

EOS_Mercury_Shutdown_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2952]
EOS_Mercury_Shutdown_proxy ENDP

EOS_Mercury_Tick_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2960]
EOS_Mercury_Tick_proxy ENDP

EOS_Metrics_BeginPlayerSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2968]
EOS_Metrics_BeginPlayerSession_proxy ENDP

EOS_Metrics_EndPlayerSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2976]
EOS_Metrics_EndPlayerSession_proxy ENDP

EOS_Mods_CopyModInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2984]
EOS_Mods_CopyModInfo_proxy ENDP

EOS_Mods_EnumerateMods_proxy PROC
    jmp QWORD PTR [g_eosProcs + 2992]
EOS_Mods_EnumerateMods_proxy ENDP

EOS_Mods_InstallMod_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3000]
EOS_Mods_InstallMod_proxy ENDP

EOS_Mods_ModInfo_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3008]
EOS_Mods_ModInfo_Release_proxy ENDP

EOS_Mods_UninstallMod_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3016]
EOS_Mods_UninstallMod_proxy ENDP

EOS_Mods_UpdateMod_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3024]
EOS_Mods_UpdateMod_proxy ENDP

EOS_P2P_AcceptConnection_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3032]
EOS_P2P_AcceptConnection_proxy ENDP

EOS_P2P_AddNotifyIncomingPacketQueueFull_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3040]
EOS_P2P_AddNotifyIncomingPacketQueueFull_proxy ENDP

EOS_P2P_AddNotifyPeerConnectionClosed_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3048]
EOS_P2P_AddNotifyPeerConnectionClosed_proxy ENDP

EOS_P2P_AddNotifyPeerConnectionEstablished_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3056]
EOS_P2P_AddNotifyPeerConnectionEstablished_proxy ENDP

EOS_P2P_AddNotifyPeerConnectionInterrupted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3064]
EOS_P2P_AddNotifyPeerConnectionInterrupted_proxy ENDP

EOS_P2P_AddNotifyPeerConnectionRequest_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3072]
EOS_P2P_AddNotifyPeerConnectionRequest_proxy ENDP

EOS_P2P_ClearPacketQueue_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3080]
EOS_P2P_ClearPacketQueue_proxy ENDP

EOS_P2P_CloseConnection_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3088]
EOS_P2P_CloseConnection_proxy ENDP

EOS_P2P_CloseConnections_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3096]
EOS_P2P_CloseConnections_proxy ENDP

EOS_P2P_GetNATType_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3104]
EOS_P2P_GetNATType_proxy ENDP

EOS_P2P_GetNextReceivedPacketSize_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3112]
EOS_P2P_GetNextReceivedPacketSize_proxy ENDP

EOS_P2P_GetPacketQueueInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3120]
EOS_P2P_GetPacketQueueInfo_proxy ENDP

EOS_P2P_GetPortRange_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3128]
EOS_P2P_GetPortRange_proxy ENDP

EOS_P2P_GetRelayControl_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3136]
EOS_P2P_GetRelayControl_proxy ENDP

EOS_P2P_QueryNATType_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3144]
EOS_P2P_QueryNATType_proxy ENDP

EOS_P2P_ReceivePacket_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3152]
EOS_P2P_ReceivePacket_proxy ENDP

EOS_P2P_RemoveNotifyIncomingPacketQueueFull_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3160]
EOS_P2P_RemoveNotifyIncomingPacketQueueFull_proxy ENDP

EOS_P2P_RemoveNotifyPeerConnectionClosed_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3168]
EOS_P2P_RemoveNotifyPeerConnectionClosed_proxy ENDP

EOS_P2P_RemoveNotifyPeerConnectionEstablished_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3176]
EOS_P2P_RemoveNotifyPeerConnectionEstablished_proxy ENDP

EOS_P2P_RemoveNotifyPeerConnectionInterrupted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3184]
EOS_P2P_RemoveNotifyPeerConnectionInterrupted_proxy ENDP

EOS_P2P_RemoveNotifyPeerConnectionRequest_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3192]
EOS_P2P_RemoveNotifyPeerConnectionRequest_proxy ENDP

EOS_P2P_SendPacket_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3200]
EOS_P2P_SendPacket_proxy ENDP

EOS_P2P_SetPacketQueueSize_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3208]
EOS_P2P_SetPacketQueueSize_proxy ENDP

EOS_P2P_SetPortRange_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3216]
EOS_P2P_SetPortRange_proxy ENDP

EOS_P2P_SetRelayControl_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3224]
EOS_P2P_SetRelayControl_proxy ENDP

EOS_Platform_CheckForLauncherAndRestart_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3232]
EOS_Platform_CheckForLauncherAndRestart_proxy ENDP

EOS_Platform_Create_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3240]
EOS_Platform_Create_proxy ENDP

EOS_Platform_GetAchievementsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3248]
EOS_Platform_GetAchievementsInterface_proxy ENDP

EOS_Platform_GetActiveCountryCode_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3256]
EOS_Platform_GetActiveCountryCode_proxy ENDP

EOS_Platform_GetActiveLocaleCode_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3264]
EOS_Platform_GetActiveLocaleCode_proxy ENDP

EOS_Platform_GetAntiCheatClientInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3272]
EOS_Platform_GetAntiCheatClientInterface_proxy ENDP

EOS_Platform_GetAntiCheatServerInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3280]
EOS_Platform_GetAntiCheatServerInterface_proxy ENDP

EOS_Platform_GetApplicationStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3288]
EOS_Platform_GetApplicationStatus_proxy ENDP

EOS_Platform_GetAuthInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3296]
EOS_Platform_GetAuthInterface_proxy ENDP

EOS_Platform_GetConnectInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3304]
EOS_Platform_GetConnectInterface_proxy ENDP

EOS_Platform_GetCustomInvitesInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3312]
EOS_Platform_GetCustomInvitesInterface_proxy ENDP

EOS_Platform_GetDesktopCrossplayStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3320]
EOS_Platform_GetDesktopCrossplayStatus_proxy ENDP

EOS_Platform_GetEcomInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3328]
EOS_Platform_GetEcomInterface_proxy ENDP

EOS_Platform_GetFriendsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3336]
EOS_Platform_GetFriendsInterface_proxy ENDP

EOS_Platform_GetIntegratedPlatformInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3344]
EOS_Platform_GetIntegratedPlatformInterface_proxy ENDP

EOS_Platform_GetKWSInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3352]
EOS_Platform_GetKWSInterface_proxy ENDP

EOS_Platform_GetLeaderboardsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3360]
EOS_Platform_GetLeaderboardsInterface_proxy ENDP

EOS_Platform_GetLobbyInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3368]
EOS_Platform_GetLobbyInterface_proxy ENDP

EOS_Platform_GetMetricsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3376]
EOS_Platform_GetMetricsInterface_proxy ENDP

EOS_Platform_GetModsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3384]
EOS_Platform_GetModsInterface_proxy ENDP

EOS_Platform_GetNetworkStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3392]
EOS_Platform_GetNetworkStatus_proxy ENDP

EOS_Platform_GetOverrideCountryCode_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3400]
EOS_Platform_GetOverrideCountryCode_proxy ENDP

EOS_Platform_GetOverrideLocaleCode_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3408]
EOS_Platform_GetOverrideLocaleCode_proxy ENDP

EOS_Platform_GetP2PInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3416]
EOS_Platform_GetP2PInterface_proxy ENDP

EOS_Platform_GetPlayerDataStorageInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3424]
EOS_Platform_GetPlayerDataStorageInterface_proxy ENDP

EOS_Platform_GetPresenceInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3432]
EOS_Platform_GetPresenceInterface_proxy ENDP

EOS_Platform_GetProgressionSnapshotInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3440]
EOS_Platform_GetProgressionSnapshotInterface_proxy ENDP

EOS_Platform_GetRTCAdminInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3448]
EOS_Platform_GetRTCAdminInterface_proxy ENDP

EOS_Platform_GetRTCInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3456]
EOS_Platform_GetRTCInterface_proxy ENDP

EOS_Platform_GetReportsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3464]
EOS_Platform_GetReportsInterface_proxy ENDP

EOS_Platform_GetSanctionsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3472]
EOS_Platform_GetSanctionsInterface_proxy ENDP

EOS_Platform_GetSessionsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3480]
EOS_Platform_GetSessionsInterface_proxy ENDP

EOS_Platform_GetStatsInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3488]
EOS_Platform_GetStatsInterface_proxy ENDP

EOS_Platform_GetTitleStorageInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3496]
EOS_Platform_GetTitleStorageInterface_proxy ENDP

EOS_Platform_GetUIInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3504]
EOS_Platform_GetUIInterface_proxy ENDP

EOS_Platform_GetUserInfoInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3512]
EOS_Platform_GetUserInfoInterface_proxy ENDP

EOS_Platform_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3520]
EOS_Platform_Release_proxy ENDP

EOS_Platform_SetApplicationStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3528]
EOS_Platform_SetApplicationStatus_proxy ENDP

EOS_Platform_SetNetworkStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3536]
EOS_Platform_SetNetworkStatus_proxy ENDP

EOS_Platform_SetOverrideCountryCode_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3544]
EOS_Platform_SetOverrideCountryCode_proxy ENDP

EOS_Platform_SetOverrideLocaleCode_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3552]
EOS_Platform_SetOverrideLocaleCode_proxy ENDP

EOS_Platform_Tick_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3560]
EOS_Platform_Tick_proxy ENDP

EOS_PlayerDataStorageFileTransferRequest_CancelRequest_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3568]
EOS_PlayerDataStorageFileTransferRequest_CancelRequest_proxy ENDP

EOS_PlayerDataStorageFileTransferRequest_GetFileRequestState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3576]
EOS_PlayerDataStorageFileTransferRequest_GetFileRequestState_proxy ENDP

EOS_PlayerDataStorageFileTransferRequest_GetFilename_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3584]
EOS_PlayerDataStorageFileTransferRequest_GetFilename_proxy ENDP

EOS_PlayerDataStorageFileTransferRequest_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3592]
EOS_PlayerDataStorageFileTransferRequest_Release_proxy ENDP

EOS_PlayerDataStorage_CopyFileMetadataAtIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3600]
EOS_PlayerDataStorage_CopyFileMetadataAtIndex_proxy ENDP

EOS_PlayerDataStorage_CopyFileMetadataByFilename_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3608]
EOS_PlayerDataStorage_CopyFileMetadataByFilename_proxy ENDP

EOS_PlayerDataStorage_DeleteCache_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3616]
EOS_PlayerDataStorage_DeleteCache_proxy ENDP

EOS_PlayerDataStorage_DeleteFile_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3624]
EOS_PlayerDataStorage_DeleteFile_proxy ENDP

EOS_PlayerDataStorage_DuplicateFile_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3632]
EOS_PlayerDataStorage_DuplicateFile_proxy ENDP

EOS_PlayerDataStorage_FileMetadata_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3640]
EOS_PlayerDataStorage_FileMetadata_Release_proxy ENDP

EOS_PlayerDataStorage_GetFileMetadataCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3648]
EOS_PlayerDataStorage_GetFileMetadataCount_proxy ENDP

EOS_PlayerDataStorage_QueryFile_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3656]
EOS_PlayerDataStorage_QueryFile_proxy ENDP

EOS_PlayerDataStorage_QueryFileList_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3664]
EOS_PlayerDataStorage_QueryFileList_proxy ENDP

EOS_PlayerDataStorage_ReadFile_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3672]
EOS_PlayerDataStorage_ReadFile_proxy ENDP

EOS_PlayerDataStorage_WriteFile_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3680]
EOS_PlayerDataStorage_WriteFile_proxy ENDP

EOS_PresenceModification_DeleteData_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3688]
EOS_PresenceModification_DeleteData_proxy ENDP

EOS_PresenceModification_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3696]
EOS_PresenceModification_Release_proxy ENDP

EOS_PresenceModification_SetData_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3704]
EOS_PresenceModification_SetData_proxy ENDP

EOS_PresenceModification_SetJoinInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3712]
EOS_PresenceModification_SetJoinInfo_proxy ENDP

EOS_PresenceModification_SetRawRichText_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3720]
EOS_PresenceModification_SetRawRichText_proxy ENDP

EOS_PresenceModification_SetStatus_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3728]
EOS_PresenceModification_SetStatus_proxy ENDP

EOS_PresenceModification_SetTemplateData_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3736]
EOS_PresenceModification_SetTemplateData_proxy ENDP

EOS_PresenceModification_SetTemplateId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3744]
EOS_PresenceModification_SetTemplateId_proxy ENDP

EOS_Presence_AddNotifyJoinGameAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3752]
EOS_Presence_AddNotifyJoinGameAccepted_proxy ENDP

EOS_Presence_AddNotifyOnPresenceChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3760]
EOS_Presence_AddNotifyOnPresenceChanged_proxy ENDP

EOS_Presence_CopyPresence_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3768]
EOS_Presence_CopyPresence_proxy ENDP

EOS_Presence_CreatePresenceModification_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3776]
EOS_Presence_CreatePresenceModification_proxy ENDP

EOS_Presence_GetJoinInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3784]
EOS_Presence_GetJoinInfo_proxy ENDP

EOS_Presence_HasPresence_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3792]
EOS_Presence_HasPresence_proxy ENDP

EOS_Presence_Info_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3800]
EOS_Presence_Info_Release_proxy ENDP

EOS_Presence_QueryPresence_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3808]
EOS_Presence_QueryPresence_proxy ENDP

EOS_Presence_RemoveNotifyJoinGameAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3816]
EOS_Presence_RemoveNotifyJoinGameAccepted_proxy ENDP

EOS_Presence_RemoveNotifyOnPresenceChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3824]
EOS_Presence_RemoveNotifyOnPresenceChanged_proxy ENDP

EOS_Presence_SetPresence_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3832]
EOS_Presence_SetPresence_proxy ENDP

EOS_ProductUserId_FromString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3840]
EOS_ProductUserId_FromString_proxy ENDP

EOS_ProductUserId_IsValid_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3848]
EOS_ProductUserId_IsValid_proxy ENDP

EOS_ProductUserId_ToString_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3856]
EOS_ProductUserId_ToString_proxy ENDP

EOS_ProgressionSnapshot_AddProgression_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3864]
EOS_ProgressionSnapshot_AddProgression_proxy ENDP

EOS_ProgressionSnapshot_BeginSnapshot_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3872]
EOS_ProgressionSnapshot_BeginSnapshot_proxy ENDP

EOS_ProgressionSnapshot_DeleteSnapshot_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3880]
EOS_ProgressionSnapshot_DeleteSnapshot_proxy ENDP

EOS_ProgressionSnapshot_EndSnapshot_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3888]
EOS_ProgressionSnapshot_EndSnapshot_proxy ENDP

EOS_ProgressionSnapshot_SubmitSnapshot_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3896]
EOS_ProgressionSnapshot_SubmitSnapshot_proxy ENDP

EOS_RTCAdmin_CopyUserTokenByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3904]
EOS_RTCAdmin_CopyUserTokenByIndex_proxy ENDP

EOS_RTCAdmin_CopyUserTokenByUserId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3912]
EOS_RTCAdmin_CopyUserTokenByUserId_proxy ENDP

EOS_RTCAdmin_Kick_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3920]
EOS_RTCAdmin_Kick_proxy ENDP

EOS_RTCAdmin_QueryJoinRoomToken_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3928]
EOS_RTCAdmin_QueryJoinRoomToken_proxy ENDP

EOS_RTCAdmin_SetParticipantHardMute_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3936]
EOS_RTCAdmin_SetParticipantHardMute_proxy ENDP

EOS_RTCAdmin_UserToken_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3944]
EOS_RTCAdmin_UserToken_Release_proxy ENDP

EOS_RTCAudio_AddNotifyAudioBeforeRender_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3952]
EOS_RTCAudio_AddNotifyAudioBeforeRender_proxy ENDP

EOS_RTCAudio_AddNotifyAudioBeforeSend_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3960]
EOS_RTCAudio_AddNotifyAudioBeforeSend_proxy ENDP

EOS_RTCAudio_AddNotifyAudioDevicesChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3968]
EOS_RTCAudio_AddNotifyAudioDevicesChanged_proxy ENDP

EOS_RTCAudio_AddNotifyAudioInputState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3976]
EOS_RTCAudio_AddNotifyAudioInputState_proxy ENDP

EOS_RTCAudio_AddNotifyAudioOutputState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3984]
EOS_RTCAudio_AddNotifyAudioOutputState_proxy ENDP

EOS_RTCAudio_AddNotifyParticipantUpdated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 3992]
EOS_RTCAudio_AddNotifyParticipantUpdated_proxy ENDP

EOS_RTCAudio_CopyInputDeviceInformationByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4000]
EOS_RTCAudio_CopyInputDeviceInformationByIndex_proxy ENDP

EOS_RTCAudio_CopyOutputDeviceInformationByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4008]
EOS_RTCAudio_CopyOutputDeviceInformationByIndex_proxy ENDP

EOS_RTCAudio_GetAudioInputDeviceByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4016]
EOS_RTCAudio_GetAudioInputDeviceByIndex_proxy ENDP

EOS_RTCAudio_GetAudioInputDevicesCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4024]
EOS_RTCAudio_GetAudioInputDevicesCount_proxy ENDP

EOS_RTCAudio_GetAudioOutputDeviceByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4032]
EOS_RTCAudio_GetAudioOutputDeviceByIndex_proxy ENDP

EOS_RTCAudio_GetAudioOutputDevicesCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4040]
EOS_RTCAudio_GetAudioOutputDevicesCount_proxy ENDP

EOS_RTCAudio_GetInputDevicesCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4048]
EOS_RTCAudio_GetInputDevicesCount_proxy ENDP

EOS_RTCAudio_GetOutputDevicesCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4056]
EOS_RTCAudio_GetOutputDevicesCount_proxy ENDP

EOS_RTCAudio_InputDeviceInformation_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4064]
EOS_RTCAudio_InputDeviceInformation_Release_proxy ENDP

EOS_RTCAudio_OutputDeviceInformation_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4072]
EOS_RTCAudio_OutputDeviceInformation_Release_proxy ENDP

EOS_RTCAudio_QueryInputDevicesInformation_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4080]
EOS_RTCAudio_QueryInputDevicesInformation_proxy ENDP

EOS_RTCAudio_QueryOutputDevicesInformation_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4088]
EOS_RTCAudio_QueryOutputDevicesInformation_proxy ENDP

EOS_RTCAudio_RegisterPlatformAudioUser_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4096]
EOS_RTCAudio_RegisterPlatformAudioUser_proxy ENDP

EOS_RTCAudio_RegisterPlatformUser_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4104]
EOS_RTCAudio_RegisterPlatformUser_proxy ENDP

EOS_RTCAudio_RemoveNotifyAudioBeforeRender_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4112]
EOS_RTCAudio_RemoveNotifyAudioBeforeRender_proxy ENDP

EOS_RTCAudio_RemoveNotifyAudioBeforeSend_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4120]
EOS_RTCAudio_RemoveNotifyAudioBeforeSend_proxy ENDP

EOS_RTCAudio_RemoveNotifyAudioDevicesChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4128]
EOS_RTCAudio_RemoveNotifyAudioDevicesChanged_proxy ENDP

EOS_RTCAudio_RemoveNotifyAudioInputState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4136]
EOS_RTCAudio_RemoveNotifyAudioInputState_proxy ENDP

EOS_RTCAudio_RemoveNotifyAudioOutputState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4144]
EOS_RTCAudio_RemoveNotifyAudioOutputState_proxy ENDP

EOS_RTCAudio_RemoveNotifyParticipantUpdated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4152]
EOS_RTCAudio_RemoveNotifyParticipantUpdated_proxy ENDP

EOS_RTCAudio_SendAudio_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4160]
EOS_RTCAudio_SendAudio_proxy ENDP

EOS_RTCAudio_SetAudioInputSettings_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4168]
EOS_RTCAudio_SetAudioInputSettings_proxy ENDP

EOS_RTCAudio_SetAudioOutputSettings_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4176]
EOS_RTCAudio_SetAudioOutputSettings_proxy ENDP

EOS_RTCAudio_SetInputDeviceSettings_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4184]
EOS_RTCAudio_SetInputDeviceSettings_proxy ENDP

EOS_RTCAudio_SetOutputDeviceSettings_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4192]
EOS_RTCAudio_SetOutputDeviceSettings_proxy ENDP

EOS_RTCAudio_UnregisterPlatformAudioUser_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4200]
EOS_RTCAudio_UnregisterPlatformAudioUser_proxy ENDP

EOS_RTCAudio_UnregisterPlatformUser_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4208]
EOS_RTCAudio_UnregisterPlatformUser_proxy ENDP

EOS_RTCAudio_UpdateParticipantVolume_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4216]
EOS_RTCAudio_UpdateParticipantVolume_proxy ENDP

EOS_RTCAudio_UpdateReceiving_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4224]
EOS_RTCAudio_UpdateReceiving_proxy ENDP

EOS_RTCAudio_UpdateReceivingVolume_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4232]
EOS_RTCAudio_UpdateReceivingVolume_proxy ENDP

EOS_RTCAudio_UpdateSending_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4240]
EOS_RTCAudio_UpdateSending_proxy ENDP

EOS_RTCAudio_UpdateSendingVolume_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4248]
EOS_RTCAudio_UpdateSendingVolume_proxy ENDP

EOS_RTCData_AddNotifyDataReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4256]
EOS_RTCData_AddNotifyDataReceived_proxy ENDP

EOS_RTCData_AddNotifyParticipantUpdated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4264]
EOS_RTCData_AddNotifyParticipantUpdated_proxy ENDP

EOS_RTCData_RemoveNotifyDataReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4272]
EOS_RTCData_RemoveNotifyDataReceived_proxy ENDP

EOS_RTCData_RemoveNotifyParticipantUpdated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4280]
EOS_RTCData_RemoveNotifyParticipantUpdated_proxy ENDP

EOS_RTCData_SendData_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4288]
EOS_RTCData_SendData_proxy ENDP

EOS_RTCData_UpdateReceiving_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4296]
EOS_RTCData_UpdateReceiving_proxy ENDP

EOS_RTCData_UpdateSending_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4304]
EOS_RTCData_UpdateSending_proxy ENDP

EOS_RTC_AddNotifyDisconnected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4312]
EOS_RTC_AddNotifyDisconnected_proxy ENDP

EOS_RTC_AddNotifyParticipantStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4320]
EOS_RTC_AddNotifyParticipantStatusChanged_proxy ENDP

EOS_RTC_AddNotifyRoomBeforeJoin_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4328]
EOS_RTC_AddNotifyRoomBeforeJoin_proxy ENDP

EOS_RTC_AddNotifyRoomStatisticsUpdated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4336]
EOS_RTC_AddNotifyRoomStatisticsUpdated_proxy ENDP

EOS_RTC_BlockParticipant_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4344]
EOS_RTC_BlockParticipant_proxy ENDP

EOS_RTC_GetAudioInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4352]
EOS_RTC_GetAudioInterface_proxy ENDP

EOS_RTC_GetDataInterface_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4360]
EOS_RTC_GetDataInterface_proxy ENDP

EOS_RTC_JoinRoom_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4368]
EOS_RTC_JoinRoom_proxy ENDP

EOS_RTC_LeaveRoom_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4376]
EOS_RTC_LeaveRoom_proxy ENDP

EOS_RTC_RemoveNotifyDisconnected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4384]
EOS_RTC_RemoveNotifyDisconnected_proxy ENDP

EOS_RTC_RemoveNotifyParticipantStatusChanged_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4392]
EOS_RTC_RemoveNotifyParticipantStatusChanged_proxy ENDP

EOS_RTC_RemoveNotifyRoomBeforeJoin_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4400]
EOS_RTC_RemoveNotifyRoomBeforeJoin_proxy ENDP

EOS_RTC_RemoveNotifyRoomStatisticsUpdated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4408]
EOS_RTC_RemoveNotifyRoomStatisticsUpdated_proxy ENDP

EOS_RTC_SetRoomSetting_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4416]
EOS_RTC_SetRoomSetting_proxy ENDP

EOS_RTC_SetSetting_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4424]
EOS_RTC_SetSetting_proxy ENDP

EOS_Reports_SendPlayerBehaviorReport_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4432]
EOS_Reports_SendPlayerBehaviorReport_proxy ENDP

EOS_Sanctions_CopyPlayerSanctionByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4440]
EOS_Sanctions_CopyPlayerSanctionByIndex_proxy ENDP

EOS_Sanctions_CreatePlayerSanctionAppeal_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4448]
EOS_Sanctions_CreatePlayerSanctionAppeal_proxy ENDP

EOS_Sanctions_GetPlayerSanctionCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4456]
EOS_Sanctions_GetPlayerSanctionCount_proxy ENDP

EOS_Sanctions_PlayerSanction_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4464]
EOS_Sanctions_PlayerSanction_Release_proxy ENDP

EOS_Sanctions_QueryActivePlayerSanctions_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4472]
EOS_Sanctions_QueryActivePlayerSanctions_proxy ENDP

EOS_SessionDetails_Attribute_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4480]
EOS_SessionDetails_Attribute_Release_proxy ENDP

EOS_SessionDetails_CopyInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4488]
EOS_SessionDetails_CopyInfo_proxy ENDP

EOS_SessionDetails_CopySessionAttributeByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4496]
EOS_SessionDetails_CopySessionAttributeByIndex_proxy ENDP

EOS_SessionDetails_CopySessionAttributeByKey_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4504]
EOS_SessionDetails_CopySessionAttributeByKey_proxy ENDP

EOS_SessionDetails_GetSessionAttributeCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4512]
EOS_SessionDetails_GetSessionAttributeCount_proxy ENDP

EOS_SessionDetails_Info_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4520]
EOS_SessionDetails_Info_Release_proxy ENDP

EOS_SessionDetails_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4528]
EOS_SessionDetails_Release_proxy ENDP

EOS_SessionModification_AddAttribute_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4536]
EOS_SessionModification_AddAttribute_proxy ENDP

EOS_SessionModification_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4544]
EOS_SessionModification_Release_proxy ENDP

EOS_SessionModification_RemoveAttribute_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4552]
EOS_SessionModification_RemoveAttribute_proxy ENDP

EOS_SessionModification_SetAllowedPlatformIds_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4560]
EOS_SessionModification_SetAllowedPlatformIds_proxy ENDP

EOS_SessionModification_SetBucketId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4568]
EOS_SessionModification_SetBucketId_proxy ENDP

EOS_SessionModification_SetHostAddress_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4576]
EOS_SessionModification_SetHostAddress_proxy ENDP

EOS_SessionModification_SetInvitesAllowed_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4584]
EOS_SessionModification_SetInvitesAllowed_proxy ENDP

EOS_SessionModification_SetJoinInProgressAllowed_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4592]
EOS_SessionModification_SetJoinInProgressAllowed_proxy ENDP

EOS_SessionModification_SetMaxPlayers_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4600]
EOS_SessionModification_SetMaxPlayers_proxy ENDP

EOS_SessionModification_SetPermissionLevel_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4608]
EOS_SessionModification_SetPermissionLevel_proxy ENDP

EOS_SessionSearch_CopySearchResultByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4616]
EOS_SessionSearch_CopySearchResultByIndex_proxy ENDP

EOS_SessionSearch_Find_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4624]
EOS_SessionSearch_Find_proxy ENDP

EOS_SessionSearch_GetSearchResultCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4632]
EOS_SessionSearch_GetSearchResultCount_proxy ENDP

EOS_SessionSearch_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4640]
EOS_SessionSearch_Release_proxy ENDP

EOS_SessionSearch_RemoveParameter_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4648]
EOS_SessionSearch_RemoveParameter_proxy ENDP

EOS_SessionSearch_SetMaxResults_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4656]
EOS_SessionSearch_SetMaxResults_proxy ENDP

EOS_SessionSearch_SetParameter_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4664]
EOS_SessionSearch_SetParameter_proxy ENDP

EOS_SessionSearch_SetSessionId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4672]
EOS_SessionSearch_SetSessionId_proxy ENDP

EOS_SessionSearch_SetTargetUserId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4680]
EOS_SessionSearch_SetTargetUserId_proxy ENDP

EOS_Sessions_AddNotifyJoinSessionAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4688]
EOS_Sessions_AddNotifyJoinSessionAccepted_proxy ENDP

EOS_Sessions_AddNotifyLeaveSessionRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4696]
EOS_Sessions_AddNotifyLeaveSessionRequested_proxy ENDP

EOS_Sessions_AddNotifySendSessionNativeInviteRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4704]
EOS_Sessions_AddNotifySendSessionNativeInviteRequested_proxy ENDP

EOS_Sessions_AddNotifySessionInviteAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4712]
EOS_Sessions_AddNotifySessionInviteAccepted_proxy ENDP

EOS_Sessions_AddNotifySessionInviteReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4720]
EOS_Sessions_AddNotifySessionInviteReceived_proxy ENDP

EOS_Sessions_AddNotifySessionInviteRejected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4728]
EOS_Sessions_AddNotifySessionInviteRejected_proxy ENDP

EOS_Sessions_CopyActiveSessionHandle_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4736]
EOS_Sessions_CopyActiveSessionHandle_proxy ENDP

EOS_Sessions_CopySessionHandleByInviteId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4744]
EOS_Sessions_CopySessionHandleByInviteId_proxy ENDP

EOS_Sessions_CopySessionHandleByUiEventId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4752]
EOS_Sessions_CopySessionHandleByUiEventId_proxy ENDP

EOS_Sessions_CopySessionHandleForPresence_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4760]
EOS_Sessions_CopySessionHandleForPresence_proxy ENDP

EOS_Sessions_CreateSessionModification_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4768]
EOS_Sessions_CreateSessionModification_proxy ENDP

EOS_Sessions_CreateSessionSearch_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4776]
EOS_Sessions_CreateSessionSearch_proxy ENDP

EOS_Sessions_DestroySession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4784]
EOS_Sessions_DestroySession_proxy ENDP

EOS_Sessions_DumpSessionState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4792]
EOS_Sessions_DumpSessionState_proxy ENDP

EOS_Sessions_EndSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4800]
EOS_Sessions_EndSession_proxy ENDP

EOS_Sessions_GetInviteCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4808]
EOS_Sessions_GetInviteCount_proxy ENDP

EOS_Sessions_GetInviteIdByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4816]
EOS_Sessions_GetInviteIdByIndex_proxy ENDP

EOS_Sessions_IsUserInSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4824]
EOS_Sessions_IsUserInSession_proxy ENDP

EOS_Sessions_JoinSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4832]
EOS_Sessions_JoinSession_proxy ENDP

EOS_Sessions_QueryInvites_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4840]
EOS_Sessions_QueryInvites_proxy ENDP

EOS_Sessions_RegisterPlayers_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4848]
EOS_Sessions_RegisterPlayers_proxy ENDP

EOS_Sessions_RejectInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4856]
EOS_Sessions_RejectInvite_proxy ENDP

EOS_Sessions_RemoveNotifyJoinSessionAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4864]
EOS_Sessions_RemoveNotifyJoinSessionAccepted_proxy ENDP

EOS_Sessions_RemoveNotifyLeaveSessionRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4872]
EOS_Sessions_RemoveNotifyLeaveSessionRequested_proxy ENDP

EOS_Sessions_RemoveNotifySendSessionNativeInviteRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4880]
EOS_Sessions_RemoveNotifySendSessionNativeInviteRequested_proxy ENDP

EOS_Sessions_RemoveNotifySessionInviteAccepted_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4888]
EOS_Sessions_RemoveNotifySessionInviteAccepted_proxy ENDP

EOS_Sessions_RemoveNotifySessionInviteReceived_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4896]
EOS_Sessions_RemoveNotifySessionInviteReceived_proxy ENDP

EOS_Sessions_RemoveNotifySessionInviteRejected_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4904]
EOS_Sessions_RemoveNotifySessionInviteRejected_proxy ENDP

EOS_Sessions_SendInvite_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4912]
EOS_Sessions_SendInvite_proxy ENDP

EOS_Sessions_StartSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4920]
EOS_Sessions_StartSession_proxy ENDP

EOS_Sessions_UnregisterPlayers_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4928]
EOS_Sessions_UnregisterPlayers_proxy ENDP

EOS_Sessions_UpdateSession_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4936]
EOS_Sessions_UpdateSession_proxy ENDP

EOS_Sessions_UpdateSessionModification_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4944]
EOS_Sessions_UpdateSessionModification_proxy ENDP

EOS_Shutdown_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4952]
EOS_Shutdown_proxy ENDP

EOS_Stats_CopyStatByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4960]
EOS_Stats_CopyStatByIndex_proxy ENDP

EOS_Stats_CopyStatByName_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4968]
EOS_Stats_CopyStatByName_proxy ENDP

EOS_Stats_GetStatsCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4976]
EOS_Stats_GetStatsCount_proxy ENDP

EOS_Stats_IngestStat_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4984]
EOS_Stats_IngestStat_proxy ENDP

EOS_Stats_QueryStats_proxy PROC
    jmp QWORD PTR [g_eosProcs + 4992]
EOS_Stats_QueryStats_proxy ENDP

EOS_Stats_Stat_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5000]
EOS_Stats_Stat_Release_proxy ENDP

EOS_TitleStorageFileTransferRequest_CancelRequest_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5008]
EOS_TitleStorageFileTransferRequest_CancelRequest_proxy ENDP

EOS_TitleStorageFileTransferRequest_GetFileRequestState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5016]
EOS_TitleStorageFileTransferRequest_GetFileRequestState_proxy ENDP

EOS_TitleStorageFileTransferRequest_GetFilename_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5024]
EOS_TitleStorageFileTransferRequest_GetFilename_proxy ENDP

EOS_TitleStorageFileTransferRequest_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5032]
EOS_TitleStorageFileTransferRequest_Release_proxy ENDP

EOS_TitleStorage_CopyFileMetadataAtIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5040]
EOS_TitleStorage_CopyFileMetadataAtIndex_proxy ENDP

EOS_TitleStorage_CopyFileMetadataByFilename_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5048]
EOS_TitleStorage_CopyFileMetadataByFilename_proxy ENDP

EOS_TitleStorage_DeleteCache_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5056]
EOS_TitleStorage_DeleteCache_proxy ENDP

EOS_TitleStorage_FileMetadata_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5064]
EOS_TitleStorage_FileMetadata_Release_proxy ENDP

EOS_TitleStorage_GetFileMetadataCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5072]
EOS_TitleStorage_GetFileMetadataCount_proxy ENDP

EOS_TitleStorage_QueryFile_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5080]
EOS_TitleStorage_QueryFile_proxy ENDP

EOS_TitleStorage_QueryFileList_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5088]
EOS_TitleStorage_QueryFileList_proxy ENDP

EOS_TitleStorage_ReadFile_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5096]
EOS_TitleStorage_ReadFile_proxy ENDP

EOS_UI_AcknowledgeEventId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5104]
EOS_UI_AcknowledgeEventId_proxy ENDP

EOS_UI_AddNotifyDisplaySettingsUpdated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5112]
EOS_UI_AddNotifyDisplaySettingsUpdated_proxy ENDP

EOS_UI_AddNotifyMemoryMonitor_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5120]
EOS_UI_AddNotifyMemoryMonitor_proxy ENDP

EOS_UI_AddNotifyOnScreenKeyboardRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5128]
EOS_UI_AddNotifyOnScreenKeyboardRequested_proxy ENDP

EOS_UI_ConfigureOnScreenKeyboard_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5136]
EOS_UI_ConfigureOnScreenKeyboard_proxy ENDP

EOS_UI_GetFriendsExclusiveInput_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5144]
EOS_UI_GetFriendsExclusiveInput_proxy ENDP

EOS_UI_GetFriendsVisible_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5152]
EOS_UI_GetFriendsVisible_proxy ENDP

EOS_UI_GetNotificationLocationPreference_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5160]
EOS_UI_GetNotificationLocationPreference_proxy ENDP

EOS_UI_GetToggleFriendsButton_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5168]
EOS_UI_GetToggleFriendsButton_proxy ENDP

EOS_UI_GetToggleFriendsKey_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5176]
EOS_UI_GetToggleFriendsKey_proxy ENDP

EOS_UI_HideFriends_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5184]
EOS_UI_HideFriends_proxy ENDP

EOS_UI_IsSocialOverlayPaused_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5192]
EOS_UI_IsSocialOverlayPaused_proxy ENDP

EOS_UI_IsValidButtonCombination_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5200]
EOS_UI_IsValidButtonCombination_proxy ENDP

EOS_UI_IsValidKeyCombination_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5208]
EOS_UI_IsValidKeyCombination_proxy ENDP

EOS_UI_PauseSocialOverlay_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5216]
EOS_UI_PauseSocialOverlay_proxy ENDP

EOS_UI_PrePresent_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5224]
EOS_UI_PrePresent_proxy ENDP

EOS_UI_RemoveNotifyDisplaySettingsUpdated_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5232]
EOS_UI_RemoveNotifyDisplaySettingsUpdated_proxy ENDP

EOS_UI_RemoveNotifyMemoryMonitor_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5240]
EOS_UI_RemoveNotifyMemoryMonitor_proxy ENDP

EOS_UI_RemoveNotifyOnScreenKeyboardRequested_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5248]
EOS_UI_RemoveNotifyOnScreenKeyboardRequested_proxy ENDP

EOS_UI_ReportInputState_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5256]
EOS_UI_ReportInputState_proxy ENDP

EOS_UI_SetDisplayPreference_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5264]
EOS_UI_SetDisplayPreference_proxy ENDP

EOS_UI_SetToggleFriendsButton_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5272]
EOS_UI_SetToggleFriendsButton_proxy ENDP

EOS_UI_SetToggleFriendsKey_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5280]
EOS_UI_SetToggleFriendsKey_proxy ENDP

EOS_UI_ShowBlockPlayer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5288]
EOS_UI_ShowBlockPlayer_proxy ENDP

EOS_UI_ShowFriends_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5296]
EOS_UI_ShowFriends_proxy ENDP

EOS_UI_ShowNativeProfile_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5304]
EOS_UI_ShowNativeProfile_proxy ENDP

EOS_UI_ShowReportPlayer_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5312]
EOS_UI_ShowReportPlayer_proxy ENDP

EOS_UserInfo_BestDisplayName_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5320]
EOS_UserInfo_BestDisplayName_Release_proxy ENDP

EOS_UserInfo_CopyBestDisplayName_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5328]
EOS_UserInfo_CopyBestDisplayName_proxy ENDP

EOS_UserInfo_CopyBestDisplayNameWithPlatform_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5336]
EOS_UserInfo_CopyBestDisplayNameWithPlatform_proxy ENDP

EOS_UserInfo_CopyExternalUserInfoByAccountId_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5344]
EOS_UserInfo_CopyExternalUserInfoByAccountId_proxy ENDP

EOS_UserInfo_CopyExternalUserInfoByAccountType_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5352]
EOS_UserInfo_CopyExternalUserInfoByAccountType_proxy ENDP

EOS_UserInfo_CopyExternalUserInfoByIndex_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5360]
EOS_UserInfo_CopyExternalUserInfoByIndex_proxy ENDP

EOS_UserInfo_CopyUserInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5368]
EOS_UserInfo_CopyUserInfo_proxy ENDP

EOS_UserInfo_ExternalUserInfo_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5376]
EOS_UserInfo_ExternalUserInfo_Release_proxy ENDP

EOS_UserInfo_GetExternalUserInfoCount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5384]
EOS_UserInfo_GetExternalUserInfoCount_proxy ENDP

EOS_UserInfo_GetLocalPlatformType_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5392]
EOS_UserInfo_GetLocalPlatformType_proxy ENDP

EOS_UserInfo_QueryUserInfo_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5400]
EOS_UserInfo_QueryUserInfo_proxy ENDP

EOS_UserInfo_QueryUserInfoByDisplayName_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5408]
EOS_UserInfo_QueryUserInfoByDisplayName_proxy ENDP

EOS_UserInfo_QueryUserInfoByExternalAccount_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5416]
EOS_UserInfo_QueryUserInfoByExternalAccount_proxy ENDP

EOS_UserInfo_Release_proxy PROC
    jmp QWORD PTR [g_eosProcs + 5424]
EOS_UserInfo_Release_proxy ENDP

END

