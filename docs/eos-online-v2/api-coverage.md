# ReFix EOS API Coverage & Status Matrix

## 1. Classification Methodology

Every EOS SDK API is classified into one of the following exact categories:

| Status | Definition |
| :--- | :--- |
| **`IMPLEMENTED`** | Fully implemented with authentic data structures, authoritative state tracking, and spec-compliant callbacks. |
| **`PARTIAL`** | Implemented but missing edge cases, advanced options, or secondary attributes. |
| **`STUB`** | Returns a fixed result code (usually `EOS_Success` or `EOS_NotFound`) with valid layout to prevent crashes. |
| **`FAKE`** | Returns synthetic, hardcoded, or dynamically fabricated data (e.g. `refix_bucket`, `MaxMembers=4`). |
| **`STEAM-BACKED`** | Delegated through Steamworks API (`steam_api64.dll` / ISteamMatchmaking / ISteamNetworking). |
| **`UNIMPLEMENTED`** | Exported via `eos_fwd.asm` trampoline but lacks any implementation in `src/eos_proxy.cpp`. |
| **`UNKNOWN`** | Behavior or usage pattern by RedpointEOS has not been verified yet via runtime trace. |

---

## 2. Runtime Imported APIs (Audit of *MECCHA CHAMELEON* / RedpointEOS)

The following 347 APIs are directly imported from `EOSSDK-Win64-Shipping.dll` by Unreal Engine + RedpointEOS in *MECCHA CHAMELEON* (`PenguinHotel-Win64-Shipping.exe`):

### 2.1. Platform & Lifecycle (29 APIs)
| API Name | Legacy Status | V2 Target Status | Notes |
| :--- | :--- | :--- | :--- |
| `EOS_Initialize` | IMPLEMENTED | IMPLEMENTED | Initializes memory and global options |
| `EOS_Shutdown` | IMPLEMENTED | IMPLEMENTED | Cleans up global state |
| `EOS_Platform_Create` | IMPLEMENTED | IMPLEMENTED | Allocates platform handle |
| `EOS_Platform_Tick` | PARTIAL | IMPLEMENTED | Pushes queued callbacks and pumps network packets |
| `EOS_Platform_Release` | IMPLEMENTED | IMPLEMENTED | Frees platform handle |
| `EOS_Platform_GetApplicationStatus` | STUB | IMPLEMENTED | Returns `EOS_AS_Foreground` |
| `EOS_Platform_SetApplicationStatus` | STUB | IMPLEMENTED | No-op |
| `EOS_Platform_GetNetworkStatus` | STUB | IMPLEMENTED | Returns `EOS_NS_Online` |
| `EOS_Platform_SetNetworkStatus` | STUB | IMPLEMENTED | No-op |
| `EOS_Platform_CheckForLauncherAndRestart` | STUB | IMPLEMENTED | Returns `EOS_Success` (no restart) |
| `EOS_Platform_GetActiveCountryCode` | STUB | IMPLEMENTED | Returns `"US"` / configured locale |
| `EOS_Platform_GetActiveLocaleCode` | STUB | IMPLEMENTED | Returns `"en-US"` |
| `EOS_Platform_GetOverrideCountryCode` | STUB | IMPLEMENTED | Returns `"US"` |
| `EOS_Platform_GetOverrideLocaleCode` | STUB | IMPLEMENTED | Returns `"en-US"` |
| `EOS_Platform_GetAuthInterface` | IMPLEMENTED | IMPLEMENTED | Returns Auth interface handle |
| `EOS_Platform_GetConnectInterface` | IMPLEMENTED | IMPLEMENTED | Returns Connect interface handle |
| `EOS_Platform_GetLobbyInterface` | IMPLEMENTED | IMPLEMENTED | Returns Lobby interface handle |
| `EOS_Platform_GetSessionsInterface` | IMPLEMENTED | IMPLEMENTED | Returns Sessions interface handle |
| `EOS_Platform_GetP2PInterface` | IMPLEMENTED | IMPLEMENTED | Returns P2P interface handle |
| `EOS_Platform_GetFriendsInterface` | IMPLEMENTED | IMPLEMENTED | Returns Friends interface handle |
| `EOS_Platform_GetPresenceInterface` | IMPLEMENTED | IMPLEMENTED | Returns Presence interface handle |
| `EOS_Platform_GetUserInfoInterface` | IMPLEMENTED | IMPLEMENTED | Returns UserInfo interface handle |
| `EOS_Platform_GetUIInterface` | IMPLEMENTED | IMPLEMENTED | Returns UI interface handle |
| `EOS_Platform_GetPlayerDataStorageInterface` | IMPLEMENTED | IMPLEMENTED | Returns PlayerDataStorage handle |
| `EOS_Platform_GetTitleStorageInterface` | IMPLEMENTED | IMPLEMENTED | Returns TitleStorage handle |
| `EOS_Platform_GetRTCInterface` | STUB | STUB | Voice chat interface handle |
| `EOS_Platform_GetAchievementsInterface` | STUB | STUB | Achievements handle |
| `EOS_Platform_GetLeaderboardsInterface` | STUB | STUB | Leaderboards handle |
| `EOS_Platform_GetStatsInterface` | STUB | STUB | Stats handle |

---

### 2.2. User Identity & Connect Interface (21 APIs)
| API Name | Legacy Status | V2 Target Status | Notes |
| :--- | :--- | :--- | :--- |
| `EOS_Connect_Login` | PARTIAL | IMPLEMENTED | Authenticates external token (Steam/DeviceId) to PUID |
| `EOS_Connect_Logout` | STUB | IMPLEMENTED | Cleans local login session |
| `EOS_Connect_CreateUser` | STUB | IMPLEMENTED | Creates local PUID |
| `EOS_Connect_CreateDeviceId` | STUB | IMPLEMENTED | Generates hardware DeviceId |
| `EOS_Connect_DeleteDeviceId` | STUB | IMPLEMENTED | No-op success |
| `EOS_Connect_QueryExternalAccountMappings` | PARTIAL | IMPLEMENTED | Resolves SteamID <-> PUID mappings |
| `EOS_Connect_GetExternalAccountMapping` | PARTIAL | IMPLEMENTED | Returns mapped PUID |
| `EOS_Connect_QueryProductUserIdMappings` | PARTIAL | IMPLEMENTED | Resolves PUID -> Account info |
| `EOS_Connect_GetProductUserIdMapping` | PARTIAL | IMPLEMENTED | Returns string PUID |
| `EOS_Connect_CopyProductUserInfo` | FAKE | IMPLEMENTED | Returns real display name & linked accounts |
| `EOS_Connect_CopyProductUserExternalAccountByIndex` | FAKE | IMPLEMENTED | Returns Epic & Steam linked account info |
| `EOS_Connect_CopyProductUserExternalAccountByAccountType` | FAKE | IMPLEMENTED | Returns specific external account info |
| `EOS_Connect_ExternalAccountInfo_Release` | IMPLEMENTED | IMPLEMENTED | Frees allocated struct |
| `EOS_Connect_GetProductUserExternalAccountCount` | FAKE | IMPLEMENTED | Returns count of linked accounts (2) |
| `EOS_Connect_GetLoggedInUserByIndex` | FAKE | IMPLEMENTED | Returns local player PUID |
| `EOS_Connect_GetLoggedInUsersCount` | STUB | IMPLEMENTED | Returns 1 (or multi-user count) |
| `EOS_Connect_GetLoginStatus` | STUB | IMPLEMENTED | Returns `EOS_LS_LoggedIn` |
| `EOS_Connect_AddNotifyLoginStatusChanged` | STUB | IMPLEMENTED | Tracks notify callback |
| `EOS_Connect_RemoveNotifyLoginStatusChanged` | STUB | IMPLEMENTED | Unregisters notify callback |
| `EOS_Connect_AddNotifyAuthExpiration` | STUB | STUB | No-op (never expires) |
| `EOS_Connect_RemoveNotifyAuthExpiration` | STUB | STUB | No-op |

---

### 2.3. Identity Validation & Formatting (6 APIs)
| API Name | Legacy Status | V2 Target Status | Notes |
| :--- | :--- | :--- | :--- |
| `EOS_ProductUserId_IsValid` | IMPLEMENTED | IMPLEMENTED | Validates 32-char hex string pointer |
| `EOS_EpicAccountId_IsValid` | IMPLEMENTED | IMPLEMENTED | Validates 32-char hex string pointer |
| `EOS_ProductUserId_ToString` | IMPLEMENTED | IMPLEMENTED | Converts PUID pointer to hex string |
| `EOS_EpicAccountId_ToString` | IMPLEMENTED | IMPLEMENTED | Converts EAID pointer to hex string |
| `EOS_ProductUserId_FromString` | PARTIAL | IMPLEMENTED | Parses hex string to stable PUID pointer |
| `EOS_EpicAccountId_FromString` | PARTIAL | IMPLEMENTED | Parses hex string to stable EAID pointer |

---

### 2.4. Sessions & ActiveSession Interface (43 APIs)
| API Name | Legacy Status | V2 Target Status | Notes |
| :--- | :--- | :--- | :--- |
| `EOS_Sessions_CreateSessionModification` | FAKE | IMPLEMENTED | Creates authoritative SessionModification handle |
| `EOS_Sessions_UpdateSessionModification` | FAKE | IMPLEMENTED | Updates modification handle |
| `EOS_SessionModification_SetBucketId` | STUB | IMPLEMENTED | Sets custom game bucket ID |
| `EOS_SessionModification_SetHostAddress` | STUB | IMPLEMENTED | Sets server IP:Port address |
| `EOS_SessionModification_SetPermissionLevel` | STUB | IMPLEMENTED | Sets Public / InviteOnly permissions |
| `EOS_SessionModification_SetJoinInProgressAllowed`| STUB | IMPLEMENTED | Sets JIP flag |
| `EOS_SessionModification_SetMaxPlayers` | STUB | IMPLEMENTED | Sets real player capacity |
| `EOS_SessionModification_SetInvitesAllowed` | STUB | IMPLEMENTED | Sets invites allowed flag |
| `EOS_SessionModification_AddAttribute` | STUB | IMPLEMENTED | Adds key-value attribute to session |
| `EOS_SessionModification_RemoveAttribute` | STUB | IMPLEMENTED | Removes attribute |
| `EOS_SessionModification_Release` | IMPLEMENTED | IMPLEMENTED | Frees modification handle |
| `EOS_Sessions_UpdateSession` | STEAM-BACKED | IMPLEMENTED | Commits session to local/network state |
| `EOS_Sessions_DestroySession` | PARTIAL | IMPLEMENTED | Removes session and notifies peers |
| `EOS_Sessions_StartSession` | STUB | IMPLEMENTED | Transitions session to ACTIVE |
| `EOS_Sessions_EndSession` | STUB | IMPLEMENTED | Transitions session to ENDED |
| `EOS_Sessions_RegisterPlayers` | FAKE | IMPLEMENTED | Registers remote PUIDs into session |
| `EOS_Sessions_UnregisterPlayers` | FAKE | IMPLEMENTED | Unregisters PUIDs from session |
| `EOS_Sessions_CreateSessionSearch` | FAKE | IMPLEMENTED | Creates SessionSearch handle |
| `EOS_SessionSearch_SetParameter` | STUB | IMPLEMENTED | Adds search filter criteria |
| `EOS_SessionSearch_SetSessionId` | STUB | IMPLEMENTED | Searches by specific SessionId |
| `EOS_SessionSearch_SetTargetUserId` | STUB | IMPLEMENTED | Searches by specific user |
| `EOS_SessionSearch_Find` | STEAM-BACKED | IMPLEMENTED | Performs network query for sessions |
| `EOS_SessionSearch_GetSearchResultCount` | FAKE | IMPLEMENTED | Returns true number of found sessions |
| `EOS_SessionSearch_CopySearchResultByIndex` | FAKE | IMPLEMENTED | Allocates SessionDetails handle |
| `EOS_SessionSearch_Release` | IMPLEMENTED | IMPLEMENTED | Frees search handle |
| `EOS_SessionDetails_CopyInfo` | FAKE | IMPLEMENTED | Returns real SessionDetails_Info struct |
| `EOS_SessionDetails_Info_Release` | IMPLEMENTED | IMPLEMENTED | Frees details info struct |
| `EOS_SessionDetails_GetSessionAttributeCount` | FAKE | IMPLEMENTED | Returns real session attribute count |
| `EOS_SessionDetails_CopySessionAttributeByIndex`| FAKE | IMPLEMENTED | Returns attribute data struct |
| `EOS_SessionDetails_CopySessionAttributeByKey` | FAKE | IMPLEMENTED | Returns attribute by key |
| `EOS_SessionDetails_Attribute_Release` | IMPLEMENTED | IMPLEMENTED | Frees attribute struct |
| `EOS_SessionDetails_Release` | IMPLEMENTED | IMPLEMENTED | Frees session details handle |
| `EOS_Sessions_CopyActiveSessionHandle` | FAKE | IMPLEMENTED | Returns ActiveSession handle |
| `EOS_ActiveSession_CopyInfo` | UNIMPLEMENTED | IMPLEMENTED | Returns ActiveSession info |
| `EOS_ActiveSession_GetRegisteredPlayerCount` | UNIMPLEMENTED | IMPLEMENTED | Returns registered player count |
| `EOS_ActiveSession_GetRegisteredPlayerByIndex` | UNIMPLEMENTED | IMPLEMENTED | Returns registered PUID |
| `EOS_ActiveSession_Release` | UNIMPLEMENTED | IMPLEMENTED | Frees ActiveSession handle |
| `EOS_Sessions_JoinSession` | STEAM-BACKED | IMPLEMENTED | Joins session, resolves address, fires callback |
| `EOS_Sessions_SendInvite` | STUB | IMPLEMENTED | Sends session invite packet to peer |
| `EOS_Sessions_AddNotifySessionInviteReceived` | STUB | IMPLEMENTED | Subscribes to invite notifications |
| `EOS_Sessions_RemoveNotifySessionInviteReceived`| STUB | IMPLEMENTED | Unsubscribes from invites |
| `EOS_Sessions_AddNotifySessionInviteAccepted` | STUB | IMPLEMENTED | Subscribes to accepted invites |
| `EOS_Sessions_AddNotifyJoinSessionAccepted` | STUB | IMPLEMENTED | Subscribes to join accepted |

---

### 2.5. Lobbies & LobbyDetails Interface (57 APIs)
| Subsystem Group | Legacy Status | V2 Target Status | Notes |
| :--- | :--- | :--- | :--- |
| `EOS_Lobby_CreateLobby` | STEAM-BACKED | IMPLEMENTED | Creates authoritative lobby object |
| `EOS_Lobby_DestroyLobby` | STEAM-BACKED | IMPLEMENTED | Destroys lobby object |
| `EOS_Lobby_UpdateLobby` | STEAM-BACKED | IMPLEMENTED | Commits lobby modifications |
| `EOS_Lobby_JoinLobby` | STEAM-BACKED | IMPLEMENTED | Joins lobby object |
| `EOS_Lobby_LeaveLobby` | STEAM-BACKED | IMPLEMENTED | Leaves lobby object |
| `EOS_LobbyModification_*` (9 APIs) | STUB | IMPLEMENTED | MaxMembers, BucketId, Attributes, Permissions |
| `EOS_LobbySearch_*` (7 APIs) | FAKE/STEAM | IMPLEMENTED | Filter-based lobby search & enumeration |
| `EOS_LobbyDetails_*` (9 APIs) | FAKE | IMPLEMENTED | Real attributes, member counts, member PUIDs |
| `EOS_Lobby_AddNotify*` (8 APIs) | STUB | IMPLEMENTED | Member status, update, invite notifications |

---

### 2.6. P2P Networking (13 APIs)
| API Name | Legacy Status | V2 Target Status | Notes |
| :--- | :--- | :--- | :--- |
| `EOS_P2P_SendPacket` | STEAM-BACKED | IMPLEMENTED | Transmits packet via Direct UDP / Relay |
| `EOS_P2P_GetNextReceivedPacketSize` | STEAM-BACKED | IMPLEMENTED | Checks incoming packet buffer |
| `EOS_P2P_ReceivePacket` | STEAM-BACKED | IMPLEMENTED | Reads next packet and peer PUID |
| `EOS_P2P_AcceptConnection` | STEAM-BACKED | IMPLEMENTED | Handshakes P2P connection with peer |
| `EOS_P2P_CloseConnection` | STUB | IMPLEMENTED | Closes peer connection |
| `EOS_P2P_CloseConnections` | STUB | IMPLEMENTED | Closes all peer connections |
| `EOS_P2P_AddNotifyPeerConnectionRequest` | STUB | IMPLEMENTED | Triggers when peer connects |
| `EOS_P2P_RemoveNotifyPeerConnectionRequest` | STUB | IMPLEMENTED | Unsubscribes connection request |
| `EOS_P2P_AddNotifyPeerConnectionClosed` | STUB | IMPLEMENTED | Triggers when peer disconnects |
| `EOS_P2P_RemoveNotifyPeerConnectionClosed` | STUB | IMPLEMENTED | Unsubscribes connection closed |
| `EOS_P2P_SetPacketQueueSize` | STUB | IMPLEMENTED | Configures queue limits |
| `EOS_P2P_GetPacketQueueInfo` | UNIMPLEMENTED | IMPLEMENTED | Returns queue stats |

---

### 2.7. Friends, Presence, UserInfo, PlayerDataStorage
| Subsystem | Legacy Status | V2 Target Status | Notes |
| :--- | :--- | :--- | :--- |
| `EOS_Friends_*` (10 APIs) | STEAM-BACKED | IMPLEMENTED | Queries online friends and status |
| `EOS_Presence_*` (13 APIs) | STUB | IMPLEMENTED | Rich presence, join info, status string |
| `EOS_UserInfo_*` (9 APIs) | FAKE | IMPLEMENTED | Real display names and PUID resolution |
| `EOS_PlayerDataStorage_*` (8 APIs) | IMPLEMENTED | IMPLEMENTED | Local file-based save game virtualization |
| `EOS_AntiCheatClient_*` / `Server_*` (27 APIs)| STUB | STUB | Safety stubs returning `EOS_Success` |
| `EOS_RTC_*` / `RTCAudio_*` (21 APIs) | STUB | STUB | Voice chat stubs |
