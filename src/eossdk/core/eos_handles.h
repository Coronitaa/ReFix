// ReFix EOS v3 - opaque handle definitions.
//
// The SDK forward-declares every interface handle as a pointer to an incomplete
// struct, which leaves the emulator free to define what those structs actually
// are. Two properties matter:
//
//   * every handle carries a magic word, so a stale or foreign pointer is
//     rejected instead of being dereferenced (the legacy proxy guessed at
//     pointers by scanning memory, which is what made it crash);
//   * interface handles are singletons owned by the platform, matching the real
//     SDK where EOS_Platform_Get*Interface returns the same pointer every time.
#pragma once
#include "refix_common.h"

namespace refix { class PlatformImpl; }

// Interface handles: stateless views onto the platform that produced them.
#define REFIX_DECLARE_INTERFACE_HANDLE(StructName)                    \
    struct StructName { uint32_t Magic; refix::PlatformImpl* Owner; };

REFIX_DECLARE_INTERFACE_HANDLE(EOS_PlatformHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_AuthHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_ConnectHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_LobbyHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_SessionsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_P2PHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_PresenceHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_FriendsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_UserInfoHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_UIHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_MetricsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_StatsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_AchievementsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_LeaderboardsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_EcomHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_ReportsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_SanctionsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_AntiCheatClientHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_AntiCheatServerHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_CustomInvitesHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_PlayerDataStorageHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_TitleStorageHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_RTCHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_RTCAdminHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_RTCAudioHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_RTCDataHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_KWSHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_ModsHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_ProgressionSnapshotHandle)
REFIX_DECLARE_INTERFACE_HANDLE(EOS_IntegratedPlatformHandle)

namespace refix {

// Magic words: distinct per handle family so a mix-up is caught immediately.
enum : uint32_t {
    kMagicInterface            = 0x52465849u, // 'RFXI'
    kMagicLobbyDetails         = 0x5246584Cu, // 'RFXL'
    kMagicLobbyModification    = 0x5246584Du, // 'RFXM'
    kMagicLobbySearch          = 0x52465853u, // 'RFXS'
    kMagicSessionDetails       = 0x52465844u, // 'RFXD'
    kMagicSessionModification  = 0x5246584Eu, // 'RFXN'
    kMagicSessionSearch        = 0x52465846u, // 'RFXF'
    kMagicActiveSession        = 0x52465841u, // 'RFXA'
    kMagicPresenceModification = 0x52465850u, // 'RFXP'
    kMagicOptionsContainer     = 0x5246584Fu, // 'RFXO'
};

template <typename T>
inline bool IsInterface(T* h) { return h && h->Magic == kMagicInterface; }

} // namespace refix
