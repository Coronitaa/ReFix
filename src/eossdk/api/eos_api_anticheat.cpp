// ReFix EOS v3 - EOS_AntiCheatClient_* and EOS_AntiCheatServer_* universal emulation.
#include "../core/refix_common.h"
#include "../sdk/eos_anticheatclient.h"
#include "../sdk/eos_anticheatserver.h"
#include "../core/refix_log.h"
#include "../core/eos_handles.h"
#include "../core/eos_platform.h"
#include "../core/eos_dispatch.h"
#include "../eos_module.h"

#include <atomic>
#include <cstring>

using namespace refix;

namespace {

static std::atomic<EOS_NotificationId> s_nextNotifId{ 1 };

} // namespace

// -----------------------------------------------------------------------------
// AntiCheat Client
// -----------------------------------------------------------------------------

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_AntiCheatClient_AddNotifyMessageToServer(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_AddNotifyMessageToServerOptions* Options,
    void* ClientData,
    EOS_AntiCheatClient_OnMessageToServerCallback NotificationFn) {
    return s_nextNotifId++;
}

EOS_DECLARE_FUNC(void) EOS_AntiCheatClient_RemoveNotifyMessageToServer(
    EOS_HAntiCheatClient Handle,
    EOS_NotificationId NotificationId) {
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_AntiCheatClient_AddNotifyMessageToPeer(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_AddNotifyMessageToPeerOptions* Options,
    void* ClientData,
    EOS_AntiCheatClient_OnMessageToPeerCallback NotificationFn) {
    return s_nextNotifId++;
}

EOS_DECLARE_FUNC(void) EOS_AntiCheatClient_RemoveNotifyMessageToPeer(
    EOS_HAntiCheatClient Handle,
    EOS_NotificationId NotificationId) {
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_AntiCheatClient_AddNotifyPeerActionRequired(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_AddNotifyPeerActionRequiredOptions* Options,
    void* ClientData,
    EOS_AntiCheatClient_OnPeerActionRequiredCallback NotificationFn) {
    return s_nextNotifId++;
}

EOS_DECLARE_FUNC(void) EOS_AntiCheatClient_RemoveNotifyPeerActionRequired(
    EOS_HAntiCheatClient Handle,
    EOS_NotificationId NotificationId) {
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_AddNotifyPeerAuthStatusChangedOptions* Options,
    void* ClientData,
    EOS_AntiCheatClient_OnPeerAuthStatusChangedCallback NotificationFn) {
    return s_nextNotifId++;
}

EOS_DECLARE_FUNC(void) EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged(
    EOS_HAntiCheatClient Handle,
    EOS_NotificationId NotificationId) {
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_AntiCheatClient_AddNotifyClientIntegrityViolated(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_AddNotifyClientIntegrityViolatedOptions* Options,
    void* ClientData,
    EOS_AntiCheatClient_OnClientIntegrityViolatedCallback NotificationFn) {
    return s_nextNotifId++;
}

EOS_DECLARE_FUNC(void) EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated(
    EOS_HAntiCheatClient Handle,
    EOS_NotificationId NotificationId) {
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_BeginSession(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_BeginSessionOptions* Options) {
    RFLOG(Abi, "EOS_AntiCheatClient_BeginSession called -> EOS_Success");
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_EndSession(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_EndSessionOptions* Options) {
    RFLOG(Abi, "EOS_AntiCheatClient_EndSession called -> EOS_Success");
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_PollStatus(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_PollStatusOptions* Options,
    EOS_EAntiCheatClientViolationType* OutViolationType,
    char* OutMessage) {
    // Official documentation: EOS_NotFound indicates no violation has occurred.
    return ER::EOS_NotFound;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_Reserved01(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_Reserved01Options* Options,
    int32_t* OutValue) {
    if (OutValue) *OutValue = 0;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_Reserved02(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_Reserved02Options* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_GetModuleBuildId(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_GetModuleBuildIdOptions* Options,
    uint32_t* OutModuleBuildId) {
    if (OutModuleBuildId) *OutModuleBuildId = 1;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_AddExternalIntegrityCatalog(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_AddExternalIntegrityCatalogOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_ReceiveMessageFromServer(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_ReceiveMessageFromServerOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_GetProtectMessageOutputLength(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_GetProtectMessageOutputLengthOptions* Options,
    uint32_t* OutBufferSizeBytes) {
    if (OutBufferSizeBytes) {
        *OutBufferSizeBytes = Options ? Options->DataLengthBytes : 0;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_ProtectMessage(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_ProtectMessageOptions* Options,
    void* OutBuffer,
    uint32_t* OutBytesWritten) {
    if (Options && OutBuffer && Options->Data) {
        if (OutBuffer != Options->Data) {
            std::memcpy(OutBuffer, Options->Data, Options->DataLengthBytes);
        }
        if (OutBytesWritten) *OutBytesWritten = Options->DataLengthBytes;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_UnprotectMessage(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_UnprotectMessageOptions* Options,
    void* OutBuffer,
    uint32_t* OutBytesWritten) {
    if (Options && OutBuffer && Options->Data) {
        if (OutBuffer != Options->Data) {
            std::memcpy(OutBuffer, Options->Data, Options->DataLengthBytes);
        }
        if (OutBytesWritten) *OutBytesWritten = Options->DataLengthBytes;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_RegisterPeer(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_RegisterPeerOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_UnregisterPeer(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_UnregisterPeerOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatClient_ReceiveMessageFromPeer(
    EOS_HAntiCheatClient Handle,
    const EOS_AntiCheatClient_ReceiveMessageFromPeerOptions* Options) {
    return ER::EOS_Success;
}

// -----------------------------------------------------------------------------
// AntiCheat Server
// -----------------------------------------------------------------------------

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_AntiCheatServer_AddNotifyMessageToClient(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_AddNotifyMessageToClientOptions* Options,
    void* ClientData,
    EOS_AntiCheatServer_OnMessageToClientCallback NotificationFn) {
    return s_nextNotifId++;
}

EOS_DECLARE_FUNC(void) EOS_AntiCheatServer_RemoveNotifyMessageToClient(
    EOS_HAntiCheatServer Handle,
    EOS_NotificationId NotificationId) {
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_AntiCheatServer_AddNotifyClientActionRequired(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions* Options,
    void* ClientData,
    EOS_AntiCheatServer_OnClientActionRequiredCallback NotificationFn) {
    return s_nextNotifId++;
}

EOS_DECLARE_FUNC(void) EOS_AntiCheatServer_RemoveNotifyClientActionRequired(
    EOS_HAntiCheatServer Handle,
    EOS_NotificationId NotificationId) {
}

EOS_DECLARE_FUNC(EOS_NotificationId) EOS_AntiCheatServer_AddNotifyClientAuthStatusChanged(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_AddNotifyClientAuthStatusChangedOptions* Options,
    void* ClientData,
    EOS_AntiCheatServer_OnClientAuthStatusChangedCallback NotificationFn) {
    return s_nextNotifId++;
}

EOS_DECLARE_FUNC(void) EOS_AntiCheatServer_RemoveNotifyClientAuthStatusChanged(
    EOS_HAntiCheatServer Handle,
    EOS_NotificationId NotificationId) {
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_BeginSession(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_BeginSessionOptions* Options) {
    RFLOG(Abi, "EOS_AntiCheatServer_BeginSession called -> EOS_Success");
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_EndSession(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_EndSessionOptions* Options) {
    RFLOG(Abi, "EOS_AntiCheatServer_EndSession called -> EOS_Success");
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_RegisterClient(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_RegisterClientOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_UnregisterClient(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_UnregisterClientOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_ReceiveMessageFromClient(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_ReceiveMessageFromClientOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_SetClientDetails(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_SetClientDetailsOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_SetGameSessionId(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_SetGameSessionIdOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_SetClientNetworkState(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_SetClientNetworkStateOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_GetProtectMessageOutputLength(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_GetProtectMessageOutputLengthOptions* Options,
    uint32_t* OutBufferSizeBytes) {
    if (OutBufferSizeBytes) {
        *OutBufferSizeBytes = Options ? Options->DataLengthBytes : 0;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_ProtectMessage(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_ProtectMessageOptions* Options,
    void* OutBuffer,
    uint32_t* OutBytesWritten) {
    if (Options && OutBuffer && Options->Data) {
        if (OutBuffer != Options->Data) {
            std::memcpy(OutBuffer, Options->Data, Options->DataLengthBytes);
        }
        if (OutBytesWritten) *OutBytesWritten = Options->DataLengthBytes;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_UnprotectMessage(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatServer_UnprotectMessageOptions* Options,
    void* OutBuffer,
    uint32_t* OutBytesWritten) {
    if (Options && OutBuffer && Options->Data) {
        if (OutBuffer != Options->Data) {
            std::memcpy(OutBuffer, Options->Data, Options->DataLengthBytes);
        }
        if (OutBytesWritten) *OutBytesWritten = Options->DataLengthBytes;
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_RegisterEvent(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_RegisterEventOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogEvent(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogEventOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogGameRoundStart(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogGameRoundStartOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogGameRoundEnd(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogGameRoundEndOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogPlayerSpawn(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogPlayerSpawnOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogPlayerDespawn(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogPlayerDespawnOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogPlayerRevive(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogPlayerReviveOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogPlayerTick(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogPlayerTickOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogPlayerUseWeapon(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogPlayerUseWeaponOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogPlayerUseAbility(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogPlayerUseAbilityOptions* Options) {
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_AntiCheatServer_LogPlayerTakeDamage(
    EOS_HAntiCheatServer Handle,
    const EOS_AntiCheatCommon_LogPlayerTakeDamageOptions* Options) {
    return ER::EOS_Success;
}

namespace refix {

void RegisterAntiCheatApi(Registrar& reg) {
    // Client
    REFIX_BIND(reg, EOS_AntiCheatClient_AddExternalIntegrityCatalog);
    REFIX_BIND(reg, EOS_AntiCheatClient_AddNotifyClientIntegrityViolated);
    REFIX_BIND(reg, EOS_AntiCheatClient_AddNotifyMessageToPeer);
    REFIX_BIND(reg, EOS_AntiCheatClient_AddNotifyMessageToServer);
    REFIX_BIND(reg, EOS_AntiCheatClient_AddNotifyPeerActionRequired);
    REFIX_BIND(reg, EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged);
    REFIX_BIND(reg, EOS_AntiCheatClient_BeginSession);
    REFIX_BIND(reg, EOS_AntiCheatClient_EndSession);
    REFIX_BIND(reg, EOS_AntiCheatClient_GetModuleBuildId);
    REFIX_BIND(reg, EOS_AntiCheatClient_GetProtectMessageOutputLength);
    REFIX_BIND(reg, EOS_AntiCheatClient_PollStatus);
    REFIX_BIND(reg, EOS_AntiCheatClient_ProtectMessage);
    REFIX_BIND(reg, EOS_AntiCheatClient_ReceiveMessageFromPeer);
    REFIX_BIND(reg, EOS_AntiCheatClient_ReceiveMessageFromServer);
    REFIX_BIND(reg, EOS_AntiCheatClient_RegisterPeer);
    REFIX_BIND(reg, EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated);
    REFIX_BIND(reg, EOS_AntiCheatClient_RemoveNotifyMessageToPeer);
    REFIX_BIND(reg, EOS_AntiCheatClient_RemoveNotifyMessageToServer);
    REFIX_BIND(reg, EOS_AntiCheatClient_RemoveNotifyPeerActionRequired);
    REFIX_BIND(reg, EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged);
    REFIX_BIND(reg, EOS_AntiCheatClient_Reserved01);
    REFIX_BIND(reg, EOS_AntiCheatClient_Reserved02);
    REFIX_BIND(reg, EOS_AntiCheatClient_UnprotectMessage);
    REFIX_BIND(reg, EOS_AntiCheatClient_UnregisterPeer);

    // Server
    REFIX_BIND(reg, EOS_AntiCheatServer_AddNotifyClientActionRequired);
    REFIX_BIND(reg, EOS_AntiCheatServer_AddNotifyClientAuthStatusChanged);
    REFIX_BIND(reg, EOS_AntiCheatServer_AddNotifyMessageToClient);
    REFIX_BIND(reg, EOS_AntiCheatServer_BeginSession);
    REFIX_BIND(reg, EOS_AntiCheatServer_EndSession);
    REFIX_BIND(reg, EOS_AntiCheatServer_GetProtectMessageOutputLength);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogEvent);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogGameRoundEnd);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogGameRoundStart);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogPlayerDespawn);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogPlayerRevive);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogPlayerSpawn);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogPlayerTakeDamage);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogPlayerTick);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogPlayerUseAbility);
    REFIX_BIND(reg, EOS_AntiCheatServer_LogPlayerUseWeapon);
    REFIX_BIND(reg, EOS_AntiCheatServer_ProtectMessage);
    REFIX_BIND(reg, EOS_AntiCheatServer_ReceiveMessageFromClient);
    REFIX_BIND(reg, EOS_AntiCheatServer_RegisterClient);
    REFIX_BIND(reg, EOS_AntiCheatServer_RegisterEvent);
    REFIX_BIND(reg, EOS_AntiCheatServer_RemoveNotifyClientActionRequired);
    REFIX_BIND(reg, EOS_AntiCheatServer_RemoveNotifyClientAuthStatusChanged);
    REFIX_BIND(reg, EOS_AntiCheatServer_RemoveNotifyMessageToClient);
    REFIX_BIND(reg, EOS_AntiCheatServer_SetClientDetails);
    REFIX_BIND(reg, EOS_AntiCheatServer_SetClientNetworkState);
    REFIX_BIND(reg, EOS_AntiCheatServer_SetGameSessionId);
    REFIX_BIND(reg, EOS_AntiCheatServer_UnprotectMessage);
    REFIX_BIND(reg, EOS_AntiCheatServer_UnregisterClient);
}

} // namespace refix
