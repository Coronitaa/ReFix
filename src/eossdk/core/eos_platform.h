// ReFix EOS v3 - the platform object.
#pragma once
#include "refix_common.h"
#include "eos_handles.h"

namespace refix {

class PlatformImpl {
public:
    PlatformImpl();

    EOS_PlatformHandle           Self;
    EOS_AuthHandle               Auth;
    EOS_ConnectHandle            Connect;
    EOS_LobbyHandle              Lobby;
    EOS_SessionsHandle           Sessions;
    EOS_P2PHandle                P2P;
    EOS_PresenceHandle           Presence;
    EOS_FriendsHandle            Friends;
    EOS_UserInfoHandle           UserInfo;
    EOS_UIHandle                 UI;
    EOS_MetricsHandle            Metrics;
    EOS_StatsHandle              Stats;
    EOS_AchievementsHandle       Achievements;
    EOS_LeaderboardsHandle       Leaderboards;
    EOS_EcomHandle               Ecom;
    EOS_ReportsHandle            Reports;
    EOS_SanctionsHandle          Sanctions;
    EOS_AntiCheatClientHandle    AntiCheatClient;
    EOS_AntiCheatServerHandle    AntiCheatServer;
    EOS_CustomInvitesHandle      CustomInvites;
    EOS_PlayerDataStorageHandle  PlayerDataStorage;
    EOS_TitleStorageHandle       TitleStorage;
    EOS_RTCHandle                RTC;
    EOS_RTCAdminHandle           RTCAdmin;
    EOS_RTCAudioHandle           RTCAudio;
    EOS_RTCDataHandle            RTCData;
    EOS_KWSHandle                KWS;
    EOS_ModsHandle               Mods;
    EOS_ProgressionSnapshotHandle ProgressionSnapshot;
    EOS_IntegratedPlatformHandle IntegratedPlatform;

    std::string ProductId;
    std::string SandboxId;
    std::string DeploymentId;

    EOS_EApplicationStatus ApplicationStatus = EOS_EApplicationStatus::EOS_AS_Foreground;
    EOS_ENetworkStatus     NetworkStatus     = EOS_ENetworkStatus::EOS_NS_Online;
    std::string            OverrideCountryCode;
    std::string            OverrideLocaleCode;
};

// The active platform, or null before EOS_Platform_Create.
PlatformImpl* CurrentPlatform();

} // namespace refix
