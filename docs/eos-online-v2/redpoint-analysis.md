# RedpointEOS & Unreal Engine Integration Analysis

## 1. Unreal Engine OnlineSubsystem Architecture

Unreal Engine defines an abstract networking interface (`IOnlineSubsystem`, `IOnlineSession`, `IOnlineIdentity`, `IOnlineFriends`, `IOnlinePresence`).

In Unreal Engine titles:
- Standard Epic integration uses `OnlineSubsystemEOS` (bundled in UE).
- Enhanced commercial integration uses **RedpointEOS** (`OnlineSubsystemRedpointEOS` / Redpoint Games EOS Online Subsystem), which provides deeper cross-platform support, automatic credential resolution, and direct P2P NetDriver integration.

```mermaid
graph TD
    Game[Game Gameplay Code] --> UOSS[IOnlineSubsystem Interface]
    UOSS --> RP_OSS[RedpointEOS Plugin (OnlineSubsystemRedpoint)]
    RP_OSS --> RP_AUTH[FSteamCredentialObtainer / FAuthHandler]
    RP_OSS --> RP_SESS[FOnlineSessionRedpointEOS]
    RP_OSS --> RP_NET[FEOSNetDriver / FEOSSocket]
    
    RP_AUTH -->|EOS_Connect_Login (CredType=1 SteamTicket)| EOS_SDK[EOS SDK Proxy]
    RP_SESS -->|EOS_Sessions_* / EOS_Lobby_*| EOS_SDK
    RP_NET -->|EOS_P2P_SendPacket / ReceivePacket| EOS_SDK
```

---

## 2. Redpoint Authentication Flow

RedpointEOS implements automatic identity acquisition:
1. **Credential Detection**:
   - In Steam environments, Redpoint initializes Steamworks and calls `SteamAPI_ISteamUser_GetAuthSessionTicket` to retrieve a Steam Session Ticket.
   - If Steam is unavailable, it calls `EOS_Connect_CreateDeviceId` to obtain a persistent hardware Device ID.
2. **EOS Connect Login**:
   - Calls `EOS_Connect_Login` with `EOS_Connect_Credentials`:
     - `Type = EOS_ECT_STEAM_SESSION_TICKET` (value `1`), `Token = <HexEncodedSteamTicket>`.
     - Or `Type = EOS_ECT_DEVICEID_ACCESS_TOKEN` (value `11`).
3. **ProductUserId Acquisition**:
   - Expects `CB_Connect_Login` callback with `ResultCode = EOS_Success` and a valid `LocalUserId` (`EOS_ProductUserId`).
   - If the user has not linked before, it calls `EOS_Connect_CreateUser`.
4. **External Account Mapping**:
   - Calls `EOS_Connect_QueryProductUserIdMappings` and `EOS_Connect_CopyProductUserInfo` to resolve display names and cross-platform IDs.

---

## 3. Sessions vs Lobbies in RedpointEOS

Redpoint supports two distinct multiplayer session models:

### Model A: Session Matchmaking (`EOS_Sessions_*`)
- Used for Dedicated Servers and traditional Listen Servers.
- Redpoint creates a session using:
  ```
  EOS_Sessions_CreateSessionModification ->
  EOS_SessionModification_SetHostAddress("IP:Port") ->
  EOS_SessionModification_SetMaxPlayers(N) ->
  EOS_SessionModification_AddAttribute(...) ->
  EOS_Sessions_UpdateSession
  ```
- Joining clients call:
  ```
  EOS_Sessions_CreateSessionSearch ->
  EOS_SessionSearch_SetParameter(...) ->
  EOS_SessionSearch_Find ->
  EOS_SessionSearch_CopySearchResultByIndex ->
  EOS_SessionDetails_CopyInfo ->
  EOS_Sessions_JoinSession
  ```
- **Crucial Redpoint Expectation**: Redpoint reads `EOS_SessionDetails_Info::HostAddress` (or attributes `HOST_IP`/`SERVER_IP` and `HOST_PORT`/`SERVER_PORT`) and passes it directly to Unreal's `ClientTravel("IP:Port")` to initiate the standard Unreal NetDriver handshake.

### Model B: Lobby Matchmaking (`EOS_Lobby_*`)
- Used for Peer-to-Peer / Party lobbies with rich presence.
- Redpoint creates a lobby using `EOS_Lobby_CreateLobby` with `EOS_LobbyModification_*`.
- Members communicate via `EOS_P2P_*` or migrate to a listen server session once matchmaking completes.

---

## 4. Redpoint NetDriver & EOS P2P Socket Routing

When configured for EOS P2P (`EOSNetDriver`):
- Packets are framed using `EOS_P2P_SocketId` (struct with `ApiVersion` and a 33-byte `SocketName`).
- Redpoint calls:
  - `EOS_P2P_AddNotifyPeerConnectionRequest` to listen for incoming connections.
  - `EOS_P2P_AcceptConnection` when a remote player connects.
  - `EOS_P2P_SendPacket` with `Channel = 0` and `Reliability = EOS_PR_ReliableOrdered` or `UnreliableUnordered`.
  - `EOS_P2P_ReceivePacket` in its network tick loop.

**Why Legacy ReFix Failed Here**:
- Legacy ReFix forwarded `EOS_P2P_SendPacket` to `ISteamNetworking::SendP2PPacket` using `remoteSteamID`.
- But in RedpointEOS, the peer's `ProductUserId` was passed as `RemoteUserId`. The legacy code attempted to parse this pointer into a Steam ID string using `FindExternalId`. When that failed (or returned a dummy ID), packets were silently dropped!
