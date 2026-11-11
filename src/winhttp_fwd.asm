; =============================================================================
; ReFix - winhttp.dll x64 Forwarding Trampolines
; =============================================================================

.data
EXTERN g_winhttpProcs:QWORD

.code
; [0] DllCanUnloadNow
DllCanUnloadNow_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 0]
DllCanUnloadNow_proxy ENDP
; [1] DllGetClassObject
DllGetClassObject_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 8]
DllGetClassObject_proxy ENDP
; [2] Private1
Private1_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 16]
Private1_proxy ENDP
; [3] SvchostPushServiceGlobals
SvchostPushServiceGlobals_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 24]
SvchostPushServiceGlobals_proxy ENDP
; [4] WinHttpAddRequestHeaders
WinHttpAddRequestHeaders_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 32]
WinHttpAddRequestHeaders_proxy ENDP
; [5] WinHttpAddRequestHeadersEx
WinHttpAddRequestHeadersEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 40]
WinHttpAddRequestHeadersEx_proxy ENDP
; [6] WinHttpAutoProxySvcMain
WinHttpAutoProxySvcMain_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 48]
WinHttpAutoProxySvcMain_proxy ENDP
; [7] WinHttpCheckPlatform
WinHttpCheckPlatform_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 56]
WinHttpCheckPlatform_proxy ENDP
; [8] WinHttpCloseHandle
WinHttpCloseHandle_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 64]
WinHttpCloseHandle_proxy ENDP
; [9] WinHttpConnect
WinHttpConnect_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 72]
WinHttpConnect_proxy ENDP
; [10] WinHttpConnectionDeletePolicyEntries
WinHttpConnectionDeletePolicyEntries_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 80]
WinHttpConnectionDeletePolicyEntries_proxy ENDP
; [11] WinHttpConnectionDeletePolicyEntriesByAppSid
WinHttpConnectionDeletePolicyEntriesByAppSid_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 88]
WinHttpConnectionDeletePolicyEntriesByAppSid_proxy ENDP
; [12] WinHttpConnectionDeleteProxyInfo
WinHttpConnectionDeleteProxyInfo_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 96]
WinHttpConnectionDeleteProxyInfo_proxy ENDP
; [13] WinHttpConnectionFreeNameList
WinHttpConnectionFreeNameList_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 104]
WinHttpConnectionFreeNameList_proxy ENDP
; [14] WinHttpConnectionFreeProxyInfo
WinHttpConnectionFreeProxyInfo_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 112]
WinHttpConnectionFreeProxyInfo_proxy ENDP
; [15] WinHttpConnectionFreeProxyList
WinHttpConnectionFreeProxyList_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 120]
WinHttpConnectionFreeProxyList_proxy ENDP
; [16] WinHttpConnectionGetNameList
WinHttpConnectionGetNameList_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 128]
WinHttpConnectionGetNameList_proxy ENDP
; [17] WinHttpConnectionGetProxyInfo
WinHttpConnectionGetProxyInfo_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 136]
WinHttpConnectionGetProxyInfo_proxy ENDP
; [18] WinHttpConnectionGetProxyList
WinHttpConnectionGetProxyList_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 144]
WinHttpConnectionGetProxyList_proxy ENDP
; [19] WinHttpConnectionOnlyConvert
WinHttpConnectionOnlyConvert_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 152]
WinHttpConnectionOnlyConvert_proxy ENDP
; [20] WinHttpConnectionOnlyReceive
WinHttpConnectionOnlyReceive_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 160]
WinHttpConnectionOnlyReceive_proxy ENDP
; [21] WinHttpConnectionOnlySend
WinHttpConnectionOnlySend_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 168]
WinHttpConnectionOnlySend_proxy ENDP
; [22] WinHttpConnectionSetPolicyEntries
WinHttpConnectionSetPolicyEntries_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 176]
WinHttpConnectionSetPolicyEntries_proxy ENDP
; [23] WinHttpConnectionSetProxyInfo
WinHttpConnectionSetProxyInfo_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 184]
WinHttpConnectionSetProxyInfo_proxy ENDP
; [24] WinHttpConnectionUpdateIfIndexTable
WinHttpConnectionUpdateIfIndexTable_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 192]
WinHttpConnectionUpdateIfIndexTable_proxy ENDP
; [25] WinHttpCrackUrl
WinHttpCrackUrl_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 200]
WinHttpCrackUrl_proxy ENDP
; [26] WinHttpCreateProxyList
WinHttpCreateProxyList_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 208]
WinHttpCreateProxyList_proxy ENDP
; [27] WinHttpCreateProxyManager
WinHttpCreateProxyManager_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 216]
WinHttpCreateProxyManager_proxy ENDP
; [28] WinHttpCreateProxyResolver
WinHttpCreateProxyResolver_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 224]
WinHttpCreateProxyResolver_proxy ENDP
; [29] WinHttpCreateProxyResult
WinHttpCreateProxyResult_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 232]
WinHttpCreateProxyResult_proxy ENDP
; [30] WinHttpCreateUiCompatibleProxyString
WinHttpCreateUiCompatibleProxyString_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 240]
WinHttpCreateUiCompatibleProxyString_proxy ENDP
; [31] WinHttpCreateUrl
WinHttpCreateUrl_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 248]
WinHttpCreateUrl_proxy ENDP
; [32] WinHttpDetectAutoProxyConfigUrl
WinHttpDetectAutoProxyConfigUrl_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 256]
WinHttpDetectAutoProxyConfigUrl_proxy ENDP
; [33] WinHttpFreeProxyResult
WinHttpFreeProxyResult_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 264]
WinHttpFreeProxyResult_proxy ENDP
; [34] WinHttpFreeProxyResultEx
WinHttpFreeProxyResultEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 272]
WinHttpFreeProxyResultEx_proxy ENDP
; [35] WinHttpFreeProxySettings
WinHttpFreeProxySettings_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 280]
WinHttpFreeProxySettings_proxy ENDP
; [36] WinHttpFreeProxySettingsEx
WinHttpFreeProxySettingsEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 288]
WinHttpFreeProxySettingsEx_proxy ENDP
; [37] WinHttpFreeQueryConnectionGroupResult
WinHttpFreeQueryConnectionGroupResult_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 296]
WinHttpFreeQueryConnectionGroupResult_proxy ENDP
; [38] WinHttpGetDefaultProxyConfiguration
WinHttpGetDefaultProxyConfiguration_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 304]
WinHttpGetDefaultProxyConfiguration_proxy ENDP
; [39] WinHttpGetIEProxyConfigForCurrentUser
WinHttpGetIEProxyConfigForCurrentUser_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 312]
WinHttpGetIEProxyConfigForCurrentUser_proxy ENDP
; [40] WinHttpGetProxyForUrl
WinHttpGetProxyForUrl_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 320]
WinHttpGetProxyForUrl_proxy ENDP
; [41] WinHttpGetProxyForUrlEx
WinHttpGetProxyForUrlEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 328]
WinHttpGetProxyForUrlEx_proxy ENDP
; [42] WinHttpGetProxyForUrlEx2
WinHttpGetProxyForUrlEx2_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 336]
WinHttpGetProxyForUrlEx2_proxy ENDP
; [43] WinHttpGetProxyForUrlHvsi
WinHttpGetProxyForUrlHvsi_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 344]
WinHttpGetProxyForUrlHvsi_proxy ENDP
; [44] WinHttpGetProxyResult
WinHttpGetProxyResult_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 352]
WinHttpGetProxyResult_proxy ENDP
; [45] WinHttpGetProxyResultEx
WinHttpGetProxyResultEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 360]
WinHttpGetProxyResultEx_proxy ENDP
; [46] WinHttpGetProxySettingsEx
WinHttpGetProxySettingsEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 368]
WinHttpGetProxySettingsEx_proxy ENDP
; [47] WinHttpGetProxySettingsResultEx
WinHttpGetProxySettingsResultEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 376]
WinHttpGetProxySettingsResultEx_proxy ENDP
; [48] WinHttpGetProxySettingsVersion
WinHttpGetProxySettingsVersion_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 384]
WinHttpGetProxySettingsVersion_proxy ENDP
; [49] WinHttpGetTunnelSocket
WinHttpGetTunnelSocket_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 392]
WinHttpGetTunnelSocket_proxy ENDP
; [50] WinHttpOpen
WinHttpOpen_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 400]
WinHttpOpen_proxy ENDP
; [51] WinHttpOpenRequest
WinHttpOpenRequest_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 408]
WinHttpOpenRequest_proxy ENDP
; [52] WinHttpPacJsWorkerMain
WinHttpPacJsWorkerMain_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 416]
WinHttpPacJsWorkerMain_proxy ENDP
; [53] WinHttpProbeConnectivity
WinHttpProbeConnectivity_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 424]
WinHttpProbeConnectivity_proxy ENDP
; [54] WinHttpProtocolCompleteUpgrade
WinHttpProtocolCompleteUpgrade_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 432]
WinHttpProtocolCompleteUpgrade_proxy ENDP
; [55] WinHttpProtocolReceive
WinHttpProtocolReceive_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 440]
WinHttpProtocolReceive_proxy ENDP
; [56] WinHttpProtocolSend
WinHttpProtocolSend_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 448]
WinHttpProtocolSend_proxy ENDP
; [57] WinHttpQueryAuthSchemes
WinHttpQueryAuthSchemes_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 456]
WinHttpQueryAuthSchemes_proxy ENDP
; [58] WinHttpQueryConnectionGroup
WinHttpQueryConnectionGroup_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 464]
WinHttpQueryConnectionGroup_proxy ENDP
; [59] WinHttpQueryDataAvailable
WinHttpQueryDataAvailable_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 472]
WinHttpQueryDataAvailable_proxy ENDP
; [60] WinHttpQueryHeaders
WinHttpQueryHeaders_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 480]
WinHttpQueryHeaders_proxy ENDP
; [61] WinHttpQueryHeadersEx
WinHttpQueryHeadersEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 488]
WinHttpQueryHeadersEx_proxy ENDP
; [62] WinHttpQueryOption
WinHttpQueryOption_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 496]
WinHttpQueryOption_proxy ENDP
; [63] WinHttpReadData
WinHttpReadData_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 504]
WinHttpReadData_proxy ENDP
; [64] WinHttpReadDataEx
WinHttpReadDataEx_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 512]
WinHttpReadDataEx_proxy ENDP
; [65] WinHttpReadProxySettings
WinHttpReadProxySettings_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 520]
WinHttpReadProxySettings_proxy ENDP
; [66] WinHttpReadProxySettingsHvsi
WinHttpReadProxySettingsHvsi_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 528]
WinHttpReadProxySettingsHvsi_proxy ENDP
; [67] WinHttpReceiveResponse
WinHttpReceiveResponse_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 536]
WinHttpReceiveResponse_proxy ENDP
; [68] WinHttpRefreshProxySettings
WinHttpRefreshProxySettings_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 544]
WinHttpRefreshProxySettings_proxy ENDP
; [69] WinHttpRegisterProxyChangeNotification
WinHttpRegisterProxyChangeNotification_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 552]
WinHttpRegisterProxyChangeNotification_proxy ENDP
; [70] WinHttpResetAutoProxy
WinHttpResetAutoProxy_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 560]
WinHttpResetAutoProxy_proxy ENDP
; [71] WinHttpResolverGetProxyForUrl
WinHttpResolverGetProxyForUrl_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 568]
WinHttpResolverGetProxyForUrl_proxy ENDP
; [72] WinHttpSaveProxyCredentials
WinHttpSaveProxyCredentials_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 576]
WinHttpSaveProxyCredentials_proxy ENDP
; [73] WinHttpSendRequest
WinHttpSendRequest_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 584]
WinHttpSendRequest_proxy ENDP
; [74] WinHttpSetCredentials
WinHttpSetCredentials_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 592]
WinHttpSetCredentials_proxy ENDP
; [75] WinHttpSetDefaultProxyConfiguration
WinHttpSetDefaultProxyConfiguration_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 600]
WinHttpSetDefaultProxyConfiguration_proxy ENDP
; [76] WinHttpSetOption
WinHttpSetOption_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 608]
WinHttpSetOption_proxy ENDP
; [77] WinHttpSetProxySettingsPerUser
WinHttpSetProxySettingsPerUser_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 616]
WinHttpSetProxySettingsPerUser_proxy ENDP
; [78] WinHttpSetSecureLegacyServersAppCompat
WinHttpSetSecureLegacyServersAppCompat_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 624]
WinHttpSetSecureLegacyServersAppCompat_proxy ENDP
; [79] WinHttpSetStatusCallback
WinHttpSetStatusCallback_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 632]
WinHttpSetStatusCallback_proxy ENDP
; [80] WinHttpSetTimeouts
WinHttpSetTimeouts_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 640]
WinHttpSetTimeouts_proxy ENDP
; [81] WinHttpTimeFromSystemTime
WinHttpTimeFromSystemTime_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 648]
WinHttpTimeFromSystemTime_proxy ENDP
; [82] WinHttpTimeToSystemTime
WinHttpTimeToSystemTime_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 656]
WinHttpTimeToSystemTime_proxy ENDP
; [83] WinHttpUnregisterProxyChangeNotification
WinHttpUnregisterProxyChangeNotification_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 664]
WinHttpUnregisterProxyChangeNotification_proxy ENDP
; [84] WinHttpWebSocketClose
WinHttpWebSocketClose_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 672]
WinHttpWebSocketClose_proxy ENDP
; [85] WinHttpWebSocketCompleteUpgrade
WinHttpWebSocketCompleteUpgrade_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 680]
WinHttpWebSocketCompleteUpgrade_proxy ENDP
; [86] WinHttpWebSocketQueryCloseStatus
WinHttpWebSocketQueryCloseStatus_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 688]
WinHttpWebSocketQueryCloseStatus_proxy ENDP
; [87] WinHttpWebSocketReceive
WinHttpWebSocketReceive_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 696]
WinHttpWebSocketReceive_proxy ENDP
; [88] WinHttpWebSocketSend
WinHttpWebSocketSend_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 704]
WinHttpWebSocketSend_proxy ENDP
; [89] WinHttpWebSocketShutdown
WinHttpWebSocketShutdown_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 712]
WinHttpWebSocketShutdown_proxy ENDP
; [90] WinHttpWriteData
WinHttpWriteData_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 720]
WinHttpWriteData_proxy ENDP
; [91] WinHttpWriteProxySettings
WinHttpWriteProxySettings_proxy PROC
    jmp QWORD PTR [g_winhttpProcs + 728]
WinHttpWriteProxySettings_proxy ENDP

END

