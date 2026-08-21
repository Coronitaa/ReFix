# ReFix EOS Online v2 — Verification & Test Plan

## 1. Test Matrix Overview

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

---

## 2. Detailed Test Specifications

### Test A — LAN Discovery & Matchmaking
1. **Setup**:
   - PC 1 (IP: `192.168.1.10`) running target game (*MECCHA CHAMELEON*).
   - PC 2 (IP: `192.168.1.20`) running target game.
2. **Execution**:
   - PC 1 navigates to Multiplayer -> Host Game -> Sets Room Name `"Chameleon_Battle"`, Max Players = `6`.
   - PC 2 navigates to Multiplayer -> Find Game.
3. **Verification Criteria**:
   - PC 2 search results show exactly 1 session:
     - Name: `"Chameleon_Battle"`
     - Max Players: `6`, Open Slots: `5`
     - Host Address: `192.168.1.10:7777`
   - PC 2 clicks Join -> Both PCs successfully load into game lobby.
   - Logs show `EOS_Sessions_JoinSession` callback returning `EOS_Success`.

---

### Test C — Invitation Flow
1. **Setup**: PC 1 (Host) and PC 2 (Client) both logged in.
2. **Execution**:
   - PC 1 creates private lobby.
   - PC 1 opens Friends menu -> Clicks "Invite" on PC 2.
3. **Verification Criteria**:
   - PC 2 immediately receives `EOS_Sessions_AddNotifySessionInviteReceived` event callback.
   - In-game notification banner / prompt appears on PC 2.
   - PC 2 clicks "Accept" -> `EOS_Sessions_JoinSession` executes -> PC 2 enters PC 1's lobby.

---

### Test D — Duplicate & Session Recreation Lifecycle
1. **Execution**:
   - Host creates Session `"GameSession"`.
   - Host leaves match back to Main Menu (triggers `EOS_Sessions_DestroySession`).
   - Host immediately creates Session `"GameSession"` again.
2. **Verification Criteria**:
   - Callback returns `EOS_Success`.
   - No `"A session with this name already exists"` error.
   - Session search returns only the new active instance.
