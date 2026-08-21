# ReFix EOS Online v2 — Test Plan & Golden Test Specification

## 1. The Golden Test (End-to-End Success Benchmark)

The ultimate verification standard for ReFix EOS Online v2:

```mermaid
sequenceDiagram
    participant PC_A as Host (PC A)
    participant Backend as ReFix Online Backend
    participant PC_B as Client (PC B)

    PC_A->>Backend: Login (PUID_A, DisplayName="HostPlayer")
    PC_B->>Backend: Login (PUID_B, DisplayName="ClientPlayer")
    
    Note over PC_A: PC A creates Room / Session
    PC_A->>Backend: CreateRoom (Name="Chameleon_Match", MaxMembers=4)
    Backend-->>PC_A: Room Created (RoomId="room_9876")
    
    Note over PC_B: PC B searches for rooms
    PC_B->>Backend: SearchRooms (Filter: Game=MECCHA_CHAMELEON)
    Backend-->>PC_B: Return Rooms (room_9876, Host=PC_A, Slots=3/4)
    
    Note over PC_B: PC B joins room
    PC_B->>Backend: JoinRoom (room_9876, PUID_B)
    Backend-->>PC_A: NotifyMemberJoined (PUID_B)
    Backend-->>PC_B: JoinAccepted (RoomState, HostConnectionInfo)
    
    Note over PC_A,PC_B: Membership Synchronized on Both Ends
    
    Note over PC_A: PC A sends In-Game Invite to PC B
    PC_A->>Backend: SendInvite (From=PUID_A, To=PUID_B, Room=room_9876)
    Backend->>PC_B: NotifyInviteReceived (From=PUID_A, Room=room_9876)
    
    Note over PC_A,PC_B: P2P Connection Established (Direct UDP / Relay)
    PC_A->>PC_B: EOS_P2P_SendPacket (Channel 0)
    PC_B->>PC_A: EOS_P2P_SendPacket (Channel 0)
    
    Note over PC_A,PC_B: Unreal NetDriver connects & gameplay starts!
```

---

## 2. Test Matrix Overview

| Test ID | Test Title | Scope | Expected Outcome |
| :--- | :--- | :--- | :--- |
| **Test A** | LAN Discovery & Matchmaking | 2 PCs on same LAN subnet | PC B discovers PC A's lobby/session with exact name, attributes, and slot count. |
| **Test B** | WAN Internet Matchmaking | 2 PCs on different networks | PC B discovers PC A over WAN signaling; establishes P2P connection via hole punch / relay. |
| **Test C** | End-to-End Invite Flow | 2 PCs (Host & Client) | Host sends invite -> Client receives `NotifyInviteReceived` -> Client accepts -> Auto-joins session. |
| **Test D** | Session Creation & Destroy Lifecycle | 1 PC (Repeated Create/Destroy) | Create Session A -> Destroy -> Create Session A again -> `EOS_Success` (No `AlreadyExists`). |
| **Test E** | Concurrent Multi-Session Hosting | 2 Host PCs | Host A creates Session A; Host B creates Session B -> Both coexist and are searchable independently. |
| **Test F** | Member Capacity & Boundary Rejection | 1 Host, N Clients | Create 4-player lobby -> Connect Clients 1, 2, 3 -> Client 4 receives `EOS_Lobby_TooManyPlayers` / rejected. |
| **Test G** | Metadata & Attribute Fidelity | Host & Client | Custom keys, map name, game mode, and server ports match exactly on search results without fake defaults. |
| **Test H** | Direct P2P & Relay Fallback | 2 PCs under Strict NAT | Direct UDP fails -> Seamlessly transitions to ReFix Relay -> Packets delivered with <100ms latency. |
| **Test I** | Non-EOS Subsystems Non-Regression | All game engine modes | Verify Re:Goldberg, Steamworks proxy, Godot proxy, and Re:Photon remain 100% operational. |
