# ReFix Legacy EOS Implementation — Confirmed Root Causes

## 1. Confirmed Bugs with Exact Code Citations

### Bug 1: Hardcoded 4-Member Slots & Synthetic Bucket
- **Code Citation**: `src/eos_proxy.cpp` lines 2415–2418 & 3416–3436:
  ```cpp
  info->AvailableSlots = 3;
  info->MaxMembers = 4;
  info->BucketId = "refix_bucket";
  ```
- **Consequence**: Redpoint's `FLobbyRoomProvider` sets `MaxMembers = N` via `EOS_LobbyModification_SetMaxMembers`, but the legacy proxy ignores this and returns `MaxMembers = 4`, corrupting game room capacity.

### Bug 2: `AlreadyExists` Error on Session Re-creation
- **Code Citation**: `src/eos_proxy.cpp` line 3434:
  ```cpp
  info->SessionId = _strdup("RefixSession_001");
  ```
- **Consequence**: When a session is destroyed and a new one created, the proxy returns the identical static `SessionId = "RefixSession_001"`. Unreal's `FOnlineSession` cache rejects it with `AlreadyExists`.

### Bug 3: Attribute "Dynamic Lying" & Invisible Sessions
- **Code Citation**: `src/eos_proxy.cpp` lines 1803–1836, 2488–2495:
  When searching, the proxy captures search filter keys into `g_capturedSearchParameters` and returns those filter values as the lobby's attributes. Real host attributes set during `EOS_LobbyModification_AddAttribute` are lost.

### Bug 4: Broken In-Game Invitations
- **Code Citation**: `src/eos_proxy.cpp` lines 2651–2654, 3734–3737:
  `eos_Lobby_AddNotifyLobbyInviteReceived` discards the callback pointer `Cb`. When `EOS_Lobby_SendInvite` is called, no network invite packet is generated or delivered to the remote peer.

### Bug 5: LAN Peer Isolation
- **Code Citation**: `src/eos_proxy.cpp` lines 2085–2099:
  `EOS_LobbySearch_Find` only queries `g_pfn_MM_RequestLobbyList` in Steamworks. It does not perform any local UDP subnet broadcast for EOS rooms. If the game does not publish a Steam lobby, discovery returns 0 results.

### Bug 6: P2P Packet Drop on Native EOS PUIDs
- **Code Citation**: `src/eos_proxy.cpp` lines 3138–3152:
  `EOS_P2P_SendPacket` attempts to parse `RemoteUserId` as a SteamID string. When Redpoint passes an EOS `ProductUserId`, the lookup fails and the packet is dropped.
