// =============================================================================
// ReFix EOS Online v2 - EOS Common Types & ABI Definitions
// =============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

#pragma pack(push, 8)

// Return Results
typedef int32_t EOS_Bool;
typedef int32_t EOS_Boolean;
constexpr EOS_Bool EOS_TRUE = 1;
constexpr EOS_Bool EOS_FALSE = 0;
typedef int32_t EOS_EResult;
#define EOS_Success                               0
#define EOS_NoConnection                          1
#define EOS_InvalidParameters                     2
#define EOS_InvalidUser                           3
#define EOS_InvalidAuth                           4
#define EOS_AccessDenied                          5
#define EOS_MissingPermissions                    6
#define EOS_Token_Not_Permitted                   7
#define EOS_NotFound                              14
#define EOS_LimitExceeded                         31
#define EOS_AlreadyConfigured                     30
#define EOS_Sessions_AlreadyInSession             2000
#define EOS_Sessions_SessionAlreadyExists         2001
#define EOS_Lobby_TooManyPlayers                  3000
#define EOS_Lobby_HostAtCapacity                  3001
#define EOS_Lobby_LobbyAlreadyExists              3002

// Common Opaque Handles
typedef void* EOS_HPlatform;
typedef void* EOS_ProductUserId;
typedef void* EOS_EpicAccountId;
typedef void* EOS_ContinuanceToken;
typedef uint64_t EOS_NotificationId;
#define EOS_INVALID_NOTIFICATIONID ((EOS_NotificationId)0)

// Subsystem Handles
typedef void* EOS_HAuth;
typedef void* EOS_HConnect;
typedef void* EOS_HLobby;
typedef void* EOS_HLobbySearch;
typedef void* EOS_HLobbyModification;
typedef void* EOS_HLobbyDetails;
typedef void* EOS_HSessions;
typedef void* EOS_HSessionSearch;
typedef void* EOS_HSessionModification;
typedef void* EOS_HSessionDetails;
typedef void* EOS_HActiveSession;
typedef void* EOS_HP2P;
typedef void* EOS_HUserInfo;
typedef void* EOS_HFriends;
typedef void* EOS_HPresence;
typedef void* EOS_HPresenceModification;
typedef void* EOS_HPlayerDataStorage;
typedef void* EOS_HTitleStorage;
typedef void* EOS_HUI;

// Login Status
typedef int32_t EOS_ELoginStatus;
#define EOS_LS_NotLoggedIn       0
#define EOS_LS_UsingLocalProfile 1
#define EOS_LS_LoggedIn          2

// External Account Types
typedef int32_t EOS_EExternalAccountType;
#define EOS_EAT_EPIC             0
#define EOS_EAT_STEAM            1
#define EOS_EAT_PLAYSTATION      2
#define EOS_EAT_XBOX             3
#define EOS_EAT_DISCORD          4
#define EOS_EAT_GOG              5
#define EOS_EAT_NINTENDO         6
#define EOS_EAT_UPLAY            7
#define EOS_EAT_OPENID           8
#define EOS_EAT_APPLE            9
#define EOS_EAT_GOOGLE           10
#define EOS_EAT_OCULUS           11
#define EOS_EAT_ITCHIO           12
#define EOS_EAT_AMAZON           13

// External Credential Types
typedef int32_t EOS_EExternalCredentialType;
#define EOS_ECT_EPIC                     0
#define EOS_ECT_STEAM_APP_TICKET         1
#define EOS_ECT_STEAM_SESSION_TICKET     1
#define EOS_ECT_DEVICEID_ACCESS_TOKEN    11
#define EOS_ECT_EPIC_ID_TOKEN            18

// Lobby Permission Levels
typedef int32_t EOS_ELobbyPermissionLevel;
#define EOS_LPL_PUBLICADVERTISED 0
#define EOS_LPL_JOINVIAPRESENCE  1
#define EOS_LPL_INVITEONLY       2

// Attribute Types
typedef int32_t EOS_EAttributeType;
#define EOS_AT_BOOLEAN 0
#define EOS_AT_INT64   1
#define EOS_AT_DOUBLE  2
#define EOS_AT_STRING  3

// P2P Packet Reliability
typedef int32_t EOS_EPacketReliability;
#define EOS_PR_UnreliableUnordered 0
#define EOS_PR_ReliableUnordered   1
#define EOS_PR_ReliableOrdered     2

// P2P Socket ID
#define EOS_P2P_SOCKETID_API_LATEST 1
struct EOS_P2P_SocketId {
    int32_t ApiVersion;
    char SocketName[33];
};

// Generic Callback Function Pointer
typedef void (*EOS_CallbackFn)(const void* Data);

#pragma pack(pop)
