# ReFix Legacy EOS Implementation — Root Cause Analysis (RCA)

## 1. Problem Matrix & Summary

| Symptom | Severity | Primary Cause in Legacy `eos_proxy.cpp` |
| :--- | :--- | :--- |
| **Players isolated on LAN / cannot find each other** | Critical | No LAN broadcast for EOS; search relies on Steam Matchmaking which drops EOS attributes. |
| **Phantom / Ghost lobbies with 4 slots** | Critical | `EOS_LobbyDetails_CopyInfo` & `EOS_SessionDetails_CopyInfo` hardcode `MaxMembers = 4`, `AvailableSlots = 3`, `BucketId = "refix_bucket"`. |
| **"A session with this name already exists" (`AlreadyExists`)** | High | `EOS_SessionDetails_CopyInfo` hardcodes `SessionId = "RefixSession_001"`; destroy session does not clear backend state. |
| **Nameless / Invisible sessions** | High | Attributes are synthesized via "search parameter lying" (`g_capturedSearchParameters`), leaving real session names blank. |
| **Invites do not work / never received** | High | `EOS_Lobby_AddNotifyLobbyInviteReceived` and `EOS_Sessions_AddNotifySessionInviteReceived` return dummy notification IDs without registering callbacks. |
| **Steam Friends works but EOS matchmaking fails** | Critical | Steam Friends queries local Steamworks API, while EOS matchmaking queries EOS SDK; proxy fails to route session state between them. |
| **P2P packets dropped / connection timeouts** | Critical | `EOS_P2P_SendPacket` attempts to parse `RemoteUserId` as SteamID; fails when Redpoint passes native EOS PUIDs. |

---

## 2. Detailed Root Cause Analysis

### RCA 1: Why LAN Players Cannot Find Each Other
- **Observed Behavior**: Two PCs on the same LAN run the game. PC A creates a lobby. PC B searches for lobbies. Zero lobbies are found, or PC B sees only its own local placeholder.
- **Evidence in Code**:
  - In `eos_proxy.cpp`, `EOS_LobbySearch_Find` (line 2078) and `EOS_SessionSearch_Find` (line 2976) do **not** perform any local network UDP broadcast discovery.
  - Instead, they call `g_pfn_MM_RequestLobbyList` via the in-process Steamworks proxy.
  - If Re:Goldberg LAN emulation is not synchronized or if the game relies solely on EOS SDK APIs without Steamworks matchmaking active, `RequestLobbyList` returns 0 results.
  - Furthermore, `eos_SessionSearch_GetSearchResultCount` (line 2981) has a fallback:
    ```cpp
    if (count == 0 && g_hasActiveSession) count = 1;
    ```
    This causes PC B to return its **own** session back to itself if it ever created one, creating a ghost loop!

---

### RCA 2: Why Lobbies Always Have 4 Slots and Synthetic Data
- **Observed Behavior**: All lobbies appear with max 4 players regardless of whether the game configured a 2-player duel or a 16-player raid.
- **Evidence in Code**:
  - `src/eos_proxy.cpp` lines 2415–2418:
    ```cpp
    info->PermissionLevel = 0; // Public
    info->AvailableSlots = 3;
    info->MaxMembers = 4;
    info->BucketId = "refix_bucket";
    ```
  - `src/eos_proxy.cpp` lines 3416–3436:
    ```cpp
    settings->NumPublicConnections = 4;
    settings->BucketId = _strdup("refix_bucket");
    info->SessionId = _strdup("RefixSession_001");
    info->NumOpenPublicConnections = 4;
    ```
  - The proxy completely ignores `EOS_LobbyModification_SetMaxMembers` and `EOS_SessionModification_SetMaxPlayers`.

---

### RCA 3: Why `AlreadyExists` Occurs on Session Recreation
- **Observed Behavior**: Hosting a game, leaving to the main menu, and clicking "Host" again produces an Unreal error: `"A session with this name already exists"`.
- **Evidence in Code**:
  - In `eos_proxy.cpp` (lines 3434, 2909), `g_hasActiveSession` is toggled to `true` during `UpdateSession`.
  - When `EOS_Sessions_DestroySession` is called (line 2955), it sets `g_hasActiveSession = false`, but the Steam Matchmaking lobby created by `CreateAndTagRealSteamLobbyAsync()` in the detached thread is never synchronously torn down.
  - The static session handle `0x2000` and session name `RefixSession_001` remain in Unreal's internal session cache.

---

### RCA 4: Why Invites Fail Silently
- **Observed Behavior**: Player A sends an invite to Player B. Player A sees "Invite sent", but Player B never receives any popup or notification.
- **Evidence in Code**:
  - In `eos_proxy.cpp`:
    ```cpp
    // lines 2651-2654
    static EOS_NotificationId eos_Lobby_AddNotifyLobbyInviteReceived(void* H, void* O, void* C, void* Cb) {
        Log("EOS_Lobby_AddNotifyLobbyInviteReceived called");
        return (EOS_NotificationId)(++g_nextNotifId);
    }
    // lines 3734-3737
    static EOS_NotificationId eos_Sessions_AddNotifySessionInviteReceived(void* H, void* O, void* C, void* Cb) {
        Log("EOS_Sessions_AddNotifySessionInviteReceived called");
        return (EOS_NotificationId)(++g_nextNotifId);
    }
    ```
  - The registration functions log a message and return an incremented integer, but **discard** the callback function pointer `Cb` and context `C`.
  - When `EOS_Lobby_SendInvite` (line 2703) or `EOS_Sessions_SendInvite` (line 3758) is called, it queues a success callback to the sender, but **never broadcasts** any invite packet over the network to the recipient.
