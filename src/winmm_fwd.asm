; =============================================================================
; ReFix - winmm.dll x64 Forwarding Trampolines (AUTO-GENERATED)
; =============================================================================
; Each function is a jump through a function pointer table defined in the C++ code.
; The table is populated at DLL load time with addresses from the real winmm.dll.
;
; This is the standard approach for x64 DLL proxying since __declspec(naked)
; is not supported on x64.
; =============================================================================

.data
EXTERN g_winmmProcs:QWORD

.code
; [0] CloseDriver
CloseDriver_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 0]
CloseDriver_proxy ENDP
; [1] DefDriverProc
DefDriverProc_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 8]
DefDriverProc_proxy ENDP
; [2] DriverCallback
DriverCallback_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 16]
DriverCallback_proxy ENDP
; [3] DrvGetModuleHandle
DrvGetModuleHandle_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 24]
DrvGetModuleHandle_proxy ENDP
; [4] GetDriverModuleHandle
GetDriverModuleHandle_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 32]
GetDriverModuleHandle_proxy ENDP
; [5] OpenDriver
OpenDriver_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 40]
OpenDriver_proxy ENDP
; [6] PlaySound
PlaySound_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 48]
PlaySound_proxy ENDP
; [7] PlaySoundA
PlaySoundA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 56]
PlaySoundA_proxy ENDP
; [8] PlaySoundW
PlaySoundW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 64]
PlaySoundW_proxy ENDP
; [9] SendDriverMessage
SendDriverMessage_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 72]
SendDriverMessage_proxy ENDP
; [10] WOWAppExit
WOWAppExit_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 80]
WOWAppExit_proxy ENDP
; [11] auxGetDevCapsA
auxGetDevCapsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 88]
auxGetDevCapsA_proxy ENDP
; [12] auxGetDevCapsW
auxGetDevCapsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 96]
auxGetDevCapsW_proxy ENDP
; [13] auxGetNumDevs
auxGetNumDevs_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 104]
auxGetNumDevs_proxy ENDP
; [14] auxGetVolume
auxGetVolume_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 112]
auxGetVolume_proxy ENDP
; [15] auxOutMessage
auxOutMessage_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 120]
auxOutMessage_proxy ENDP
; [16] auxSetVolume
auxSetVolume_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 128]
auxSetVolume_proxy ENDP
; [17] joyConfigChanged
joyConfigChanged_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 136]
joyConfigChanged_proxy ENDP
; [18] joyGetDevCapsA
joyGetDevCapsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 144]
joyGetDevCapsA_proxy ENDP
; [19] joyGetDevCapsW
joyGetDevCapsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 152]
joyGetDevCapsW_proxy ENDP
; [20] joyGetNumDevs
joyGetNumDevs_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 160]
joyGetNumDevs_proxy ENDP
; [21] joyGetPos
joyGetPos_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 168]
joyGetPos_proxy ENDP
; [22] joyGetPosEx
joyGetPosEx_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 176]
joyGetPosEx_proxy ENDP
; [23] joyGetThreshold
joyGetThreshold_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 184]
joyGetThreshold_proxy ENDP
; [24] joyReleaseCapture
joyReleaseCapture_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 192]
joyReleaseCapture_proxy ENDP
; [25] joySetCapture
joySetCapture_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 200]
joySetCapture_proxy ENDP
; [26] joySetThreshold
joySetThreshold_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 208]
joySetThreshold_proxy ENDP
; [27] mciDriverNotify
mciDriverNotify_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 216]
mciDriverNotify_proxy ENDP
; [28] mciDriverYield
mciDriverYield_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 224]
mciDriverYield_proxy ENDP
; [29] mciExecute
mciExecute_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 232]
mciExecute_proxy ENDP
; [30] mciFreeCommandResource
mciFreeCommandResource_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 240]
mciFreeCommandResource_proxy ENDP
; [31] mciGetCreatorTask
mciGetCreatorTask_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 248]
mciGetCreatorTask_proxy ENDP
; [32] mciGetDeviceIDA
mciGetDeviceIDA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 256]
mciGetDeviceIDA_proxy ENDP
; [33] mciGetDeviceIDFromElementIDA
mciGetDeviceIDFromElementIDA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 264]
mciGetDeviceIDFromElementIDA_proxy ENDP
; [34] mciGetDeviceIDFromElementIDW
mciGetDeviceIDFromElementIDW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 272]
mciGetDeviceIDFromElementIDW_proxy ENDP
; [35] mciGetDeviceIDW
mciGetDeviceIDW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 280]
mciGetDeviceIDW_proxy ENDP
; [36] mciGetDriverData
mciGetDriverData_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 288]
mciGetDriverData_proxy ENDP
; [37] mciGetErrorStringA
mciGetErrorStringA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 296]
mciGetErrorStringA_proxy ENDP
; [38] mciGetErrorStringW
mciGetErrorStringW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 304]
mciGetErrorStringW_proxy ENDP
; [39] mciGetYieldProc
mciGetYieldProc_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 312]
mciGetYieldProc_proxy ENDP
; [40] mciLoadCommandResource
mciLoadCommandResource_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 320]
mciLoadCommandResource_proxy ENDP
; [41] mciSendCommandA
mciSendCommandA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 328]
mciSendCommandA_proxy ENDP
; [42] mciSendCommandW
mciSendCommandW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 336]
mciSendCommandW_proxy ENDP
; [43] mciSendStringA
mciSendStringA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 344]
mciSendStringA_proxy ENDP
; [44] mciSendStringW
mciSendStringW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 352]
mciSendStringW_proxy ENDP
; [45] mciSetDriverData
mciSetDriverData_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 360]
mciSetDriverData_proxy ENDP
; [46] mciSetYieldProc
mciSetYieldProc_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 368]
mciSetYieldProc_proxy ENDP
; [47] midiConnect
midiConnect_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 376]
midiConnect_proxy ENDP
; [48] midiDisconnect
midiDisconnect_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 384]
midiDisconnect_proxy ENDP
; [49] midiInAddBuffer
midiInAddBuffer_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 392]
midiInAddBuffer_proxy ENDP
; [50] midiInClose
midiInClose_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 400]
midiInClose_proxy ENDP
; [51] midiInGetDevCapsA
midiInGetDevCapsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 408]
midiInGetDevCapsA_proxy ENDP
; [52] midiInGetDevCapsW
midiInGetDevCapsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 416]
midiInGetDevCapsW_proxy ENDP
; [53] midiInGetErrorTextA
midiInGetErrorTextA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 424]
midiInGetErrorTextA_proxy ENDP
; [54] midiInGetErrorTextW
midiInGetErrorTextW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 432]
midiInGetErrorTextW_proxy ENDP
; [55] midiInGetID
midiInGetID_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 440]
midiInGetID_proxy ENDP
; [56] midiInGetNumDevs
midiInGetNumDevs_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 448]
midiInGetNumDevs_proxy ENDP
; [57] midiInMessage
midiInMessage_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 456]
midiInMessage_proxy ENDP
; [58] midiInOpen
midiInOpen_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 464]
midiInOpen_proxy ENDP
; [59] midiInPrepareHeader
midiInPrepareHeader_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 472]
midiInPrepareHeader_proxy ENDP
; [60] midiInReset
midiInReset_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 480]
midiInReset_proxy ENDP
; [61] midiInStart
midiInStart_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 488]
midiInStart_proxy ENDP
; [62] midiInStop
midiInStop_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 496]
midiInStop_proxy ENDP
; [63] midiInUnprepareHeader
midiInUnprepareHeader_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 504]
midiInUnprepareHeader_proxy ENDP
; [64] midiOutCacheDrumPatches
midiOutCacheDrumPatches_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 512]
midiOutCacheDrumPatches_proxy ENDP
; [65] midiOutCachePatches
midiOutCachePatches_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 520]
midiOutCachePatches_proxy ENDP
; [66] midiOutClose
midiOutClose_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 528]
midiOutClose_proxy ENDP
; [67] midiOutGetDevCapsA
midiOutGetDevCapsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 536]
midiOutGetDevCapsA_proxy ENDP
; [68] midiOutGetDevCapsW
midiOutGetDevCapsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 544]
midiOutGetDevCapsW_proxy ENDP
; [69] midiOutGetErrorTextA
midiOutGetErrorTextA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 552]
midiOutGetErrorTextA_proxy ENDP
; [70] midiOutGetErrorTextW
midiOutGetErrorTextW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 560]
midiOutGetErrorTextW_proxy ENDP
; [71] midiOutGetID
midiOutGetID_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 568]
midiOutGetID_proxy ENDP
; [72] midiOutGetNumDevs
midiOutGetNumDevs_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 576]
midiOutGetNumDevs_proxy ENDP
; [73] midiOutGetVolume
midiOutGetVolume_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 584]
midiOutGetVolume_proxy ENDP
; [74] midiOutLongMsg
midiOutLongMsg_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 592]
midiOutLongMsg_proxy ENDP
; [75] midiOutMessage
midiOutMessage_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 600]
midiOutMessage_proxy ENDP
; [76] midiOutOpen
midiOutOpen_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 608]
midiOutOpen_proxy ENDP
; [77] midiOutPrepareHeader
midiOutPrepareHeader_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 616]
midiOutPrepareHeader_proxy ENDP
; [78] midiOutReset
midiOutReset_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 624]
midiOutReset_proxy ENDP
; [79] midiOutSetVolume
midiOutSetVolume_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 632]
midiOutSetVolume_proxy ENDP
; [80] midiOutShortMsg
midiOutShortMsg_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 640]
midiOutShortMsg_proxy ENDP
; [81] midiOutUnprepareHeader
midiOutUnprepareHeader_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 648]
midiOutUnprepareHeader_proxy ENDP
; [82] midiStreamClose
midiStreamClose_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 656]
midiStreamClose_proxy ENDP
; [83] midiStreamOpen
midiStreamOpen_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 664]
midiStreamOpen_proxy ENDP
; [84] midiStreamOut
midiStreamOut_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 672]
midiStreamOut_proxy ENDP
; [85] midiStreamPause
midiStreamPause_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 680]
midiStreamPause_proxy ENDP
; [86] midiStreamPosition
midiStreamPosition_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 688]
midiStreamPosition_proxy ENDP
; [87] midiStreamProperty
midiStreamProperty_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 696]
midiStreamProperty_proxy ENDP
; [88] midiStreamRestart
midiStreamRestart_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 704]
midiStreamRestart_proxy ENDP
; [89] midiStreamStop
midiStreamStop_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 712]
midiStreamStop_proxy ENDP
; [90] mixerClose
mixerClose_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 720]
mixerClose_proxy ENDP
; [91] mixerGetControlDetailsA
mixerGetControlDetailsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 728]
mixerGetControlDetailsA_proxy ENDP
; [92] mixerGetControlDetailsW
mixerGetControlDetailsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 736]
mixerGetControlDetailsW_proxy ENDP
; [93] mixerGetDevCapsA
mixerGetDevCapsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 744]
mixerGetDevCapsA_proxy ENDP
; [94] mixerGetDevCapsW
mixerGetDevCapsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 752]
mixerGetDevCapsW_proxy ENDP
; [95] mixerGetID
mixerGetID_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 760]
mixerGetID_proxy ENDP
; [96] mixerGetLineControlsA
mixerGetLineControlsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 768]
mixerGetLineControlsA_proxy ENDP
; [97] mixerGetLineControlsW
mixerGetLineControlsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 776]
mixerGetLineControlsW_proxy ENDP
; [98] mixerGetLineInfoA
mixerGetLineInfoA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 784]
mixerGetLineInfoA_proxy ENDP
; [99] mixerGetLineInfoW
mixerGetLineInfoW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 792]
mixerGetLineInfoW_proxy ENDP
; [100] mixerGetNumDevs
mixerGetNumDevs_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 800]
mixerGetNumDevs_proxy ENDP
; [101] mixerMessage
mixerMessage_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 808]
mixerMessage_proxy ENDP
; [102] mixerOpen
mixerOpen_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 816]
mixerOpen_proxy ENDP
; [103] mixerSetControlDetails
mixerSetControlDetails_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 824]
mixerSetControlDetails_proxy ENDP
; [104] mmDrvInstall
mmDrvInstall_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 832]
mmDrvInstall_proxy ENDP
; [105] mmGetCurrentTask
mmGetCurrentTask_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 840]
mmGetCurrentTask_proxy ENDP
; [106] mmTaskBlock
mmTaskBlock_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 848]
mmTaskBlock_proxy ENDP
; [107] mmTaskCreate
mmTaskCreate_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 856]
mmTaskCreate_proxy ENDP
; [108] mmTaskSignal
mmTaskSignal_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 864]
mmTaskSignal_proxy ENDP
; [109] mmTaskYield
mmTaskYield_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 872]
mmTaskYield_proxy ENDP
; [110] mmioAdvance
mmioAdvance_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 880]
mmioAdvance_proxy ENDP
; [111] mmioAscend
mmioAscend_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 888]
mmioAscend_proxy ENDP
; [112] mmioClose
mmioClose_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 896]
mmioClose_proxy ENDP
; [113] mmioCreateChunk
mmioCreateChunk_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 904]
mmioCreateChunk_proxy ENDP
; [114] mmioDescend
mmioDescend_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 912]
mmioDescend_proxy ENDP
; [115] mmioFlush
mmioFlush_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 920]
mmioFlush_proxy ENDP
; [116] mmioGetInfo
mmioGetInfo_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 928]
mmioGetInfo_proxy ENDP
; [117] mmioInstallIOProcA
mmioInstallIOProcA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 936]
mmioInstallIOProcA_proxy ENDP
; [118] mmioInstallIOProcW
mmioInstallIOProcW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 944]
mmioInstallIOProcW_proxy ENDP
; [119] mmioOpenA
mmioOpenA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 952]
mmioOpenA_proxy ENDP
; [120] mmioOpenW
mmioOpenW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 960]
mmioOpenW_proxy ENDP
; [121] mmioRead
mmioRead_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 968]
mmioRead_proxy ENDP
; [122] mmioRenameA
mmioRenameA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 976]
mmioRenameA_proxy ENDP
; [123] mmioRenameW
mmioRenameW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 984]
mmioRenameW_proxy ENDP
; [124] mmioSeek
mmioSeek_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 992]
mmioSeek_proxy ENDP
; [125] mmioSendMessage
mmioSendMessage_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1000]
mmioSendMessage_proxy ENDP
; [126] mmioSetBuffer
mmioSetBuffer_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1008]
mmioSetBuffer_proxy ENDP
; [127] mmioSetInfo
mmioSetInfo_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1016]
mmioSetInfo_proxy ENDP
; [128] mmioStringToFOURCCA
mmioStringToFOURCCA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1024]
mmioStringToFOURCCA_proxy ENDP
; [129] mmioStringToFOURCCW
mmioStringToFOURCCW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1032]
mmioStringToFOURCCW_proxy ENDP
; [130] mmioWrite
mmioWrite_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1040]
mmioWrite_proxy ENDP
; [131] mmsystemGetVersion
mmsystemGetVersion_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1048]
mmsystemGetVersion_proxy ENDP
; [132] sndPlaySoundA
sndPlaySoundA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1056]
sndPlaySoundA_proxy ENDP
; [133] sndPlaySoundW
sndPlaySoundW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1064]
sndPlaySoundW_proxy ENDP
; [134] timeBeginPeriod
timeBeginPeriod_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1072]
timeBeginPeriod_proxy ENDP
; [135] timeEndPeriod
timeEndPeriod_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1080]
timeEndPeriod_proxy ENDP
; [136] timeGetDevCaps
timeGetDevCaps_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1088]
timeGetDevCaps_proxy ENDP
; [137] timeGetSystemTime
timeGetSystemTime_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1096]
timeGetSystemTime_proxy ENDP
; [138] timeGetTime
timeGetTime_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1104]
timeGetTime_proxy ENDP
; [139] timeKillEvent
timeKillEvent_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1112]
timeKillEvent_proxy ENDP
; [140] timeSetEvent
timeSetEvent_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1120]
timeSetEvent_proxy ENDP
; [141] waveInAddBuffer
waveInAddBuffer_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1128]
waveInAddBuffer_proxy ENDP
; [142] waveInClose
waveInClose_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1136]
waveInClose_proxy ENDP
; [143] waveInGetDevCapsA
waveInGetDevCapsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1144]
waveInGetDevCapsA_proxy ENDP
; [144] waveInGetDevCapsW
waveInGetDevCapsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1152]
waveInGetDevCapsW_proxy ENDP
; [145] waveInGetErrorTextA
waveInGetErrorTextA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1160]
waveInGetErrorTextA_proxy ENDP
; [146] waveInGetErrorTextW
waveInGetErrorTextW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1168]
waveInGetErrorTextW_proxy ENDP
; [147] waveInGetID
waveInGetID_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1176]
waveInGetID_proxy ENDP
; [148] waveInGetNumDevs
waveInGetNumDevs_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1184]
waveInGetNumDevs_proxy ENDP
; [149] waveInGetPosition
waveInGetPosition_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1192]
waveInGetPosition_proxy ENDP
; [150] waveInMessage
waveInMessage_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1200]
waveInMessage_proxy ENDP
; [151] waveInOpen
waveInOpen_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1208]
waveInOpen_proxy ENDP
; [152] waveInPrepareHeader
waveInPrepareHeader_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1216]
waveInPrepareHeader_proxy ENDP
; [153] waveInReset
waveInReset_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1224]
waveInReset_proxy ENDP
; [154] waveInStart
waveInStart_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1232]
waveInStart_proxy ENDP
; [155] waveInStop
waveInStop_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1240]
waveInStop_proxy ENDP
; [156] waveInUnprepareHeader
waveInUnprepareHeader_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1248]
waveInUnprepareHeader_proxy ENDP
; [157] waveOutBreakLoop
waveOutBreakLoop_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1256]
waveOutBreakLoop_proxy ENDP
; [158] waveOutClose
waveOutClose_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1264]
waveOutClose_proxy ENDP
; [159] waveOutGetDevCapsA
waveOutGetDevCapsA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1272]
waveOutGetDevCapsA_proxy ENDP
; [160] waveOutGetDevCapsW
waveOutGetDevCapsW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1280]
waveOutGetDevCapsW_proxy ENDP
; [161] waveOutGetErrorTextA
waveOutGetErrorTextA_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1288]
waveOutGetErrorTextA_proxy ENDP
; [162] waveOutGetErrorTextW
waveOutGetErrorTextW_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1296]
waveOutGetErrorTextW_proxy ENDP
; [163] waveOutGetID
waveOutGetID_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1304]
waveOutGetID_proxy ENDP
; [164] waveOutGetNumDevs
waveOutGetNumDevs_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1312]
waveOutGetNumDevs_proxy ENDP
; [165] waveOutGetPitch
waveOutGetPitch_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1320]
waveOutGetPitch_proxy ENDP
; [166] waveOutGetPlaybackRate
waveOutGetPlaybackRate_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1328]
waveOutGetPlaybackRate_proxy ENDP
; [167] waveOutGetPosition
waveOutGetPosition_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1336]
waveOutGetPosition_proxy ENDP
; [168] waveOutGetVolume
waveOutGetVolume_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1344]
waveOutGetVolume_proxy ENDP
; [169] waveOutMessage
waveOutMessage_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1352]
waveOutMessage_proxy ENDP
; [170] waveOutOpen
waveOutOpen_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1360]
waveOutOpen_proxy ENDP
; [171] waveOutPause
waveOutPause_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1368]
waveOutPause_proxy ENDP
; [172] waveOutPrepareHeader
waveOutPrepareHeader_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1376]
waveOutPrepareHeader_proxy ENDP
; [173] waveOutReset
waveOutReset_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1384]
waveOutReset_proxy ENDP
; [174] waveOutRestart
waveOutRestart_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1392]
waveOutRestart_proxy ENDP
; [175] waveOutSetPitch
waveOutSetPitch_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1400]
waveOutSetPitch_proxy ENDP
; [176] waveOutSetPlaybackRate
waveOutSetPlaybackRate_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1408]
waveOutSetPlaybackRate_proxy ENDP
; [177] waveOutSetVolume
waveOutSetVolume_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1416]
waveOutSetVolume_proxy ENDP
; [178] waveOutUnprepareHeader
waveOutUnprepareHeader_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1424]
waveOutUnprepareHeader_proxy ENDP
; [179] waveOutWrite
waveOutWrite_proxy PROC
    jmp QWORD PTR [g_winmmProcs + 1432]
waveOutWrite_proxy ENDP
END

