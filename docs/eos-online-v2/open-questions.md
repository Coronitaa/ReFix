# ReFix EOS Online v2 — Open Questions & Technical Uncertainties

## 1. Register of Uncertainties

### Question 1: Redpoint Synthetic Sessions vs Direct Lobbies
- **Question**: When a game calls `IOnlineSession::CreateSession` in Unreal Engine with RedpointEOS, does Redpoint create an `EOS_Lobby` via `FLobbyRoomProvider`, an `EOS_Session` via `FSyntheticSessionManager`, or both in tandem?
- **Current Status**: **`HYPOTHESIS`** (Binary disassembly indicates `FSyntheticSessionManager` delegates room operations to `FLobbyRoomProvider`, but listen servers may also register host addresses via `EOS_Sessions_*`).
- **Evidence Required**: Runtime trace capturing function calls during `CreateSession` in *MECCHA CHAMELEON*.

---

### Question 2: NetDriver Socket Protocol in RedpointEOS
- **Question**: Does Redpoint's `FEOSNetDriver` use `EOS_P2P_SendPacket` with `Channel = 0` and custom `SocketName`, or does it fallback to standard Unreal UDP sockets (`IpNetDriver`) once the host IP/Port is resolved via `EOS_LobbyDetails_CopyAttributeByKey("HOST_IP")`?
- **Current Status**: **`HYPOTHESIS`** (Redpoint supports both: P2P sockets for cross-play and direct IP sockets for local/direct servers).
- **Evidence Required**: Runtime trace of `EOS_P2P_SendPacket` vs socket `sendto` calls during game connection.

---

### Question 3: Redpoint Credentials Priority
- **Question**: In an environment where Steamworks is active, does Redpoint always prefer `EOS_ECT_STEAM_SESSION_TICKET`, or does it attempt `EOS_ECT_DEVICEID_ACCESS_TOKEN` if the Steam ticket fails?
- **Current Status**: **`HYPOTHESIS`** (`FSteamCredentialObtainer` tries Steam ticket first; falls back to DeviceId).
- **Evidence Required**: Runtime trace of `EOS_Connect_Login` options struct.
