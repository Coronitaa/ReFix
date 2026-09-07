// ReFix EOS v3 - EOS_Initialize / EOS_Platform_* / global helpers.
//
// The platform is the title's entry point into EOS. Beyond handing out
// interface handles it owns the tick: every completion callback and every
// notification the emulator raises is delivered from EOS_Platform_Tick, on the
// thread the game chose to call it from, exactly like the real SDK.
#include "../core/refix_common.h"
#include "../core/refix_config.h"
#include "../core/refix_log.h"
#include "../core/eos_handles.h"
#include "../core/eos_platform.h"
#include "../core/eos_ids.h"
#include "../core/eos_identity.h"
#include "../core/eos_dispatch.h"
#include "../eos_module.h"

namespace refix {

static PlatformImpl* g_platform = nullptr;
static bool          g_initialised = false;

PlatformImpl* CurrentPlatform() { return g_platform; }

PlatformImpl::PlatformImpl() {
    auto init = [this](auto& handle) { handle.Magic = kMagicInterface; handle.Owner = this; };
    init(Self); init(Auth); init(Connect); init(Lobby); init(Sessions); init(P2P);
    init(Presence); init(Friends); init(UserInfo); init(UI); init(Metrics); init(Stats);
    init(Achievements); init(Leaderboards); init(Ecom); init(Reports); init(Sanctions);
    init(AntiCheatClient); init(AntiCheatServer); init(CustomInvites);
    init(PlayerDataStorage); init(TitleStorage); init(RTC); init(RTCAdmin); init(RTCAudio);
    init(RTCData); init(KWS); init(Mods); init(ProgressionSnapshot); init(IntegratedPlatform);
}

} // namespace refix

using namespace refix;

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(EOS_EResult) EOS_Initialize(const EOS_InitializeOptions* Options) {
    const char* product = (Options && Options->ProductName) ? Options->ProductName : "<none>";
    const char* version = (Options && Options->ProductVersion) ? Options->ProductVersion : "<none>";
    RFLOG(Platform, "EOS_Initialize: product='%s' version='%s' apiVersion=%d",
          product, version, Options ? Options->ApiVersion : -1);
    if (g_initialised) return ER::EOS_AlreadyConfigured;
    g_initialised = true;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Shutdown() {
    RFLOG(Platform, "EOS_Shutdown");
    g_initialised = false;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_HPlatform) EOS_Platform_Create(const EOS_Platform_Options* Options) {
    if (g_platform) {
        RFLOG(Platform, "EOS_Platform_Create: returning the existing platform");
        return (EOS_HPlatform)&g_platform->Self;
    }
    g_platform = new PlatformImpl();
    if (Options) {
        if (Options->ProductId)    g_platform->ProductId    = Options->ProductId;
        if (Options->SandboxId)    g_platform->SandboxId    = Options->SandboxId;
        if (Options->DeploymentId) g_platform->DeploymentId = Options->DeploymentId;
    }
    RFLOG(Platform, "EOS_Platform_Create: product='%s' sandbox='%s' deployment='%s'",
          g_platform->ProductId.c_str(), g_platform->SandboxId.c_str(),
          g_platform->DeploymentId.c_str());
    return (EOS_HPlatform)&g_platform->Self;
}

EOS_DECLARE_FUNC(void) EOS_Platform_Release(EOS_HPlatform Handle) {
    RFLOG(Platform, "EOS_Platform_Release(%p)", Handle);
    // The platform outlives Release deliberately: titles frequently release and
    // re-create it while background threads still hold interface handles, and
    // real EOS keeps those pointers valid for the process lifetime.
}

EOS_DECLARE_FUNC(void) EOS_Platform_Tick(EOS_HPlatform Handle) {
    size_t n = Dispatcher::Get().Drain();
    if (n) RFLOG(Platform, "EOS_Platform_Tick: dispatched %zu callback(s)", n);
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_CheckForLauncherAndRestart(EOS_HPlatform Handle) {
    // There is no Epic launcher in the loop; report that no restart is needed.
    return ER::EOS_NoChange;
}

// ---------------------------------------------------------------------------
// Interface accessors
// ---------------------------------------------------------------------------
#define REFIX_INTERFACE_GETTER(HandleType, Fn, Member)                              \
    EOS_DECLARE_FUNC(HandleType) Fn(EOS_HPlatform Handle) {                         \
        PlatformImpl* p = CurrentPlatform();                                        \
        return p ? (HandleType)&p->Member : nullptr;                                \
    }

REFIX_INTERFACE_GETTER(EOS_HAuth,                EOS_Platform_GetAuthInterface,                Auth)
REFIX_INTERFACE_GETTER(EOS_HConnect,             EOS_Platform_GetConnectInterface,             Connect)
REFIX_INTERFACE_GETTER(EOS_HLobby,               EOS_Platform_GetLobbyInterface,               Lobby)
REFIX_INTERFACE_GETTER(EOS_HSessions,            EOS_Platform_GetSessionsInterface,            Sessions)
REFIX_INTERFACE_GETTER(EOS_HP2P,                 EOS_Platform_GetP2PInterface,                 P2P)
REFIX_INTERFACE_GETTER(EOS_HPresence,            EOS_Platform_GetPresenceInterface,            Presence)
REFIX_INTERFACE_GETTER(EOS_HFriends,             EOS_Platform_GetFriendsInterface,             Friends)
REFIX_INTERFACE_GETTER(EOS_HUserInfo,            EOS_Platform_GetUserInfoInterface,            UserInfo)
REFIX_INTERFACE_GETTER(EOS_HUI,                  EOS_Platform_GetUIInterface,                  UI)
REFIX_INTERFACE_GETTER(EOS_HMetrics,             EOS_Platform_GetMetricsInterface,             Metrics)
REFIX_INTERFACE_GETTER(EOS_HStats,               EOS_Platform_GetStatsInterface,               Stats)
REFIX_INTERFACE_GETTER(EOS_HAchievements,        EOS_Platform_GetAchievementsInterface,        Achievements)
REFIX_INTERFACE_GETTER(EOS_HLeaderboards,        EOS_Platform_GetLeaderboardsInterface,        Leaderboards)
REFIX_INTERFACE_GETTER(EOS_HEcom,                EOS_Platform_GetEcomInterface,                Ecom)
REFIX_INTERFACE_GETTER(EOS_HReports,             EOS_Platform_GetReportsInterface,             Reports)
REFIX_INTERFACE_GETTER(EOS_HSanctions,           EOS_Platform_GetSanctionsInterface,           Sanctions)
REFIX_INTERFACE_GETTER(EOS_HAntiCheatClient,     EOS_Platform_GetAntiCheatClientInterface,     AntiCheatClient)
REFIX_INTERFACE_GETTER(EOS_HAntiCheatServer,     EOS_Platform_GetAntiCheatServerInterface,     AntiCheatServer)
REFIX_INTERFACE_GETTER(EOS_HCustomInvites,       EOS_Platform_GetCustomInvitesInterface,       CustomInvites)
REFIX_INTERFACE_GETTER(EOS_HPlayerDataStorage,   EOS_Platform_GetPlayerDataStorageInterface,   PlayerDataStorage)
REFIX_INTERFACE_GETTER(EOS_HTitleStorage,        EOS_Platform_GetTitleStorageInterface,        TitleStorage)
REFIX_INTERFACE_GETTER(EOS_HRTC,                 EOS_Platform_GetRTCInterface,                 RTC)
REFIX_INTERFACE_GETTER(EOS_HRTCAdmin,            EOS_Platform_GetRTCAdminInterface,            RTCAdmin)
REFIX_INTERFACE_GETTER(EOS_HKWS,                 EOS_Platform_GetKWSInterface,                 KWS)
REFIX_INTERFACE_GETTER(EOS_HMods,                EOS_Platform_GetModsInterface,                Mods)
REFIX_INTERFACE_GETTER(EOS_HProgressionSnapshot, EOS_Platform_GetProgressionSnapshotInterface, ProgressionSnapshot)
REFIX_INTERFACE_GETTER(EOS_HIntegratedPlatform,  EOS_Platform_GetIntegratedPlatformInterface,  IntegratedPlatform)

EOS_DECLARE_FUNC(EOS_HRTCAudio) EOS_RTC_GetAudioInterface(EOS_HRTC Handle) {
    PlatformImpl* p = CurrentPlatform();
    return p ? (EOS_HRTCAudio)&p->RTCAudio : nullptr;
}

// ---------------------------------------------------------------------------
// Platform state
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(EOS_EApplicationStatus) EOS_Platform_GetApplicationStatus(EOS_HPlatform Handle) {
    PlatformImpl* p = CurrentPlatform();
    return p ? p->ApplicationStatus : EOS_EApplicationStatus::EOS_AS_Foreground;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_SetApplicationStatus(EOS_HPlatform Handle, const EOS_EApplicationStatus NewStatus) {
    if (PlatformImpl* p = CurrentPlatform()) p->ApplicationStatus = NewStatus;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_ENetworkStatus) EOS_Platform_GetNetworkStatus(EOS_HPlatform Handle) {
    PlatformImpl* p = CurrentPlatform();
    return p ? p->NetworkStatus : EOS_ENetworkStatus::EOS_NS_Online;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_SetNetworkStatus(EOS_HPlatform Handle, const EOS_ENetworkStatus NewStatus) {
    if (PlatformImpl* p = CurrentPlatform()) p->NetworkStatus = NewStatus;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_GetDesktopCrossplayStatus(EOS_HPlatform Handle, const EOS_Platform_GetDesktopCrossplayStatusOptions* Options, EOS_Platform_DesktopCrossplayStatusInfo* OutDesktopCrossplayStatusInfo) {
    if (!OutDesktopCrossplayStatusInfo) return ER::EOS_InvalidParameters;
    OutDesktopCrossplayStatusInfo->Status = EOS_EDesktopCrossplayStatus::EOS_DCS_OK;
    OutDesktopCrossplayStatusInfo->ServiceInitResult = 0;
    return ER::EOS_Success;
}

// The emulator is region-agnostic; report whatever the title asked us to
// override, and otherwise leave the fields empty rather than inventing a locale.
static EOS_EResult CopyCode(const std::string& value, char* out, int32_t* inOutLen) {
    if (!out || !inOutLen) return ER::EOS_InvalidParameters;
    int32_t needed = (int32_t)value.size() + 1;
    if (*inOutLen < needed) { *inOutLen = needed; return ER::EOS_LimitExceeded; }
    std::memcpy(out, value.c_str(), needed);
    *inOutLen = needed;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_GetActiveCountryCode(EOS_HPlatform Handle, EOS_EpicAccountId LocalUserId, char* OutBuffer, int32_t* InOutBufferLength) {
    PlatformImpl* p = CurrentPlatform();
    return CopyCode(p ? p->OverrideCountryCode : std::string(), OutBuffer, InOutBufferLength);
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_GetActiveLocaleCode(EOS_HPlatform Handle, EOS_EpicAccountId LocalUserId, char* OutBuffer, int32_t* InOutBufferLength) {
    PlatformImpl* p = CurrentPlatform();
    return CopyCode(p ? p->OverrideLocaleCode : std::string(), OutBuffer, InOutBufferLength);
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_GetOverrideCountryCode(EOS_HPlatform Handle, char* OutBuffer, int32_t* InOutBufferLength) {
    PlatformImpl* p = CurrentPlatform();
    return CopyCode(p ? p->OverrideCountryCode : std::string(), OutBuffer, InOutBufferLength);
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_GetOverrideLocaleCode(EOS_HPlatform Handle, char* OutBuffer, int32_t* InOutBufferLength) {
    PlatformImpl* p = CurrentPlatform();
    return CopyCode(p ? p->OverrideLocaleCode : std::string(), OutBuffer, InOutBufferLength);
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_SetOverrideCountryCode(EOS_HPlatform Handle, const char* NewCountryCode) {
    if (PlatformImpl* p = CurrentPlatform()) p->OverrideCountryCode = NewCountryCode ? NewCountryCode : "";
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_Platform_SetOverrideLocaleCode(EOS_HPlatform Handle, const char* NewLocaleCode) {
    if (PlatformImpl* p = CurrentPlatform()) p->OverrideLocaleCode = NewLocaleCode ? NewLocaleCode : "";
    return ER::EOS_Success;
}

// ---------------------------------------------------------------------------
// Logging, ids and results
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(EOS_EResult) EOS_Logging_SetCallback(EOS_LogMessageFunc Callback) { return ER::EOS_Success; }
EOS_DECLARE_FUNC(EOS_EResult) EOS_Logging_SetLogLevel(EOS_ELogCategory LogCategory, EOS_ELogLevel LogLevel) { return ER::EOS_Success; }

EOS_DECLARE_FUNC(EOS_Bool) EOS_ProductUserId_IsValid(EOS_ProductUserId AccountId) {
    return IdRegistry::IsValidPuid(AccountId) ? EOS_TRUE : EOS_FALSE;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_ProductUserId_ToString(EOS_ProductUserId AccountId, char* OutBuffer, int32_t* InOutBufferLength) {
    const char* s = IdRegistry::ToString(AccountId);
    if (!s) return ER::EOS_InvalidUser;
    return CopyCode(s, OutBuffer, InOutBufferLength);
}

EOS_DECLARE_FUNC(EOS_ProductUserId) EOS_ProductUserId_FromString(const char* ProductUserIdString) {
    return ProductUserIdString ? IdRegistry::Get().Puid(ProductUserIdString) : nullptr;
}

EOS_DECLARE_FUNC(EOS_Bool) EOS_EpicAccountId_IsValid(EOS_EpicAccountId AccountId) {
    return IdRegistry::IsValidEaid(AccountId) ? EOS_TRUE : EOS_FALSE;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_EpicAccountId_ToString(EOS_EpicAccountId AccountId, char* OutBuffer, int32_t* InOutBufferLength) {
    const char* s = IdRegistry::ToString(AccountId);
    if (!s) return ER::EOS_InvalidUser;
    return CopyCode(s, OutBuffer, InOutBufferLength);
}

EOS_DECLARE_FUNC(EOS_EpicAccountId) EOS_EpicAccountId_FromString(const char* AccountIdString) {
    return AccountIdString ? IdRegistry::Get().Eaid(AccountIdString) : nullptr;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_ByteArray_ToString(const uint8_t* ByteArray, const uint32_t Length, char* OutBuffer, uint32_t* InOutBufferLength) {
    if (!InOutBufferLength) return ER::EOS_InvalidParameters;
    uint32_t needed = Length * 2 + 1;
    if (!OutBuffer || *InOutBufferLength < needed) { *InOutBufferLength = needed; return ER::EOS_LimitExceeded; }
    static const char* hx = "0123456789abcdef";
    for (uint32_t i = 0; i < Length; i++) {
        OutBuffer[i * 2 + 0] = hx[ByteArray[i] >> 4];
        OutBuffer[i * 2 + 1] = hx[ByteArray[i] & 0x0F];
    }
    OutBuffer[Length * 2] = '\0';
    *InOutBufferLength = needed;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_Bool) EOS_EResult_IsOperationComplete(EOS_EResult Result) {
    // Only the "still running" codes mean the operation has not finished.
    switch (Result) {
        case ER::EOS_OperationWillRetry:
        case ER::EOS_AlreadyPending:
            return EOS_FALSE;
        default:
            return EOS_TRUE;
    }
}

EOS_DECLARE_FUNC(const char*) EOS_EResult_ToString(EOS_EResult Result) {
    switch (Result) {
        case ER::EOS_Success:                return "EOS_Success";
        case ER::EOS_NoConnection:           return "EOS_NoConnection";
        case ER::EOS_InvalidParameters:      return "EOS_InvalidParameters";
        case ER::EOS_InvalidRequest:         return "EOS_InvalidRequest";
        case ER::EOS_InvalidUser:            return "EOS_InvalidUser";
        case ER::EOS_InvalidAuth:            return "EOS_InvalidAuth";
        case ER::EOS_AccessDenied:           return "EOS_AccessDenied";
        case ER::EOS_NotFound:               return "EOS_NotFound";
        case ER::EOS_TimedOut:               return "EOS_TimedOut";
        case ER::EOS_AlreadyPending:         return "EOS_AlreadyPending";
        case ER::EOS_LimitExceeded:          return "EOS_LimitExceeded";
        case ER::EOS_NotImplemented:         return "EOS_NotImplemented";
        case ER::EOS_OperationWillRetry:     return "EOS_OperationWillRetry";
        case ER::EOS_AlreadyConfigured:      return "EOS_AlreadyConfigured";
        case ER::EOS_NoChange:               return "EOS_NoChange";
        case ER::EOS_Lobby_NotOwner:         return "EOS_Lobby_NotOwner";
        case ER::EOS_Lobby_TooManyPlayers:   return "EOS_Lobby_TooManyPlayers";
        case ER::EOS_Lobby_NotAllowed:       return "EOS_Lobby_NotAllowed";
        case ER::EOS_Sessions_SessionAlreadyExists: return "EOS_Sessions_SessionAlreadyExists";
        default:                             return "EOS_UnexpectedError";
    }
}

namespace refix {

void RegisterPlatformApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_EResult_IsOperationComplete);
    REFIX_BIND(reg, EOS_EResult_ToString);
    REFIX_BIND(reg, EOS_Initialize);
    REFIX_BIND(reg, EOS_Shutdown);
    REFIX_BIND(reg, EOS_Platform_Create);
    REFIX_BIND(reg, EOS_Platform_Release);
    REFIX_BIND(reg, EOS_Platform_Tick);
    REFIX_BIND(reg, EOS_Platform_CheckForLauncherAndRestart);

    REFIX_BIND(reg, EOS_Platform_GetAuthInterface);
    REFIX_BIND(reg, EOS_Platform_GetConnectInterface);
    REFIX_BIND(reg, EOS_Platform_GetLobbyInterface);
    REFIX_BIND(reg, EOS_Platform_GetSessionsInterface);
    REFIX_BIND(reg, EOS_Platform_GetP2PInterface);
    REFIX_BIND(reg, EOS_Platform_GetPresenceInterface);
    REFIX_BIND(reg, EOS_Platform_GetFriendsInterface);
    REFIX_BIND(reg, EOS_Platform_GetUserInfoInterface);
    REFIX_BIND(reg, EOS_Platform_GetUIInterface);
    REFIX_BIND(reg, EOS_Platform_GetMetricsInterface);
    REFIX_BIND(reg, EOS_Platform_GetStatsInterface);
    REFIX_BIND(reg, EOS_Platform_GetAchievementsInterface);
    REFIX_BIND(reg, EOS_Platform_GetLeaderboardsInterface);
    REFIX_BIND(reg, EOS_Platform_GetEcomInterface);
    REFIX_BIND(reg, EOS_Platform_GetReportsInterface);
    REFIX_BIND(reg, EOS_Platform_GetSanctionsInterface);
    REFIX_BIND(reg, EOS_Platform_GetAntiCheatClientInterface);
    REFIX_BIND(reg, EOS_Platform_GetAntiCheatServerInterface);
    REFIX_BIND(reg, EOS_Platform_GetCustomInvitesInterface);
    REFIX_BIND(reg, EOS_Platform_GetPlayerDataStorageInterface);
    REFIX_BIND(reg, EOS_Platform_GetTitleStorageInterface);
    REFIX_BIND(reg, EOS_Platform_GetRTCInterface);
    REFIX_BIND(reg, EOS_Platform_GetRTCAdminInterface);
    REFIX_BIND(reg, EOS_Platform_GetKWSInterface);
    REFIX_BIND(reg, EOS_Platform_GetModsInterface);
    REFIX_BIND(reg, EOS_Platform_GetProgressionSnapshotInterface);
    REFIX_BIND(reg, EOS_Platform_GetIntegratedPlatformInterface);
    REFIX_BIND(reg, EOS_RTC_GetAudioInterface);

    REFIX_BIND(reg, EOS_Platform_GetApplicationStatus);
    REFIX_BIND(reg, EOS_Platform_SetApplicationStatus);
    REFIX_BIND(reg, EOS_Platform_GetNetworkStatus);
    REFIX_BIND(reg, EOS_Platform_SetNetworkStatus);
    REFIX_BIND(reg, EOS_Platform_GetDesktopCrossplayStatus);
    REFIX_BIND(reg, EOS_Platform_GetActiveCountryCode);
    REFIX_BIND(reg, EOS_Platform_GetActiveLocaleCode);
    REFIX_BIND(reg, EOS_Platform_GetOverrideCountryCode);
    REFIX_BIND(reg, EOS_Platform_GetOverrideLocaleCode);
    REFIX_BIND(reg, EOS_Platform_SetOverrideCountryCode);
    REFIX_BIND(reg, EOS_Platform_SetOverrideLocaleCode);

    REFIX_BIND(reg, EOS_Logging_SetCallback);
    REFIX_BIND(reg, EOS_Logging_SetLogLevel);

    REFIX_BIND(reg, EOS_ProductUserId_IsValid);
    REFIX_BIND(reg, EOS_ProductUserId_ToString);
    REFIX_BIND(reg, EOS_ProductUserId_FromString);
    REFIX_BIND(reg, EOS_EpicAccountId_IsValid);
    REFIX_BIND(reg, EOS_EpicAccountId_ToString);
    REFIX_BIND(reg, EOS_EpicAccountId_FromString);
    REFIX_BIND(reg, EOS_ByteArray_ToString);
}

} // namespace refix
