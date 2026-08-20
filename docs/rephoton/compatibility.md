# Re:Photon — Compatibility Matrix & Implementation Audit

## 1. Status Definitions

To maintain absolute technical integrity, games and features are evaluated against strictly defined compatibility stages:

* **`Unknown`**: Untested and unverified.
* **`Investigating`**: Under active protocol / network flow investigation.
* **`ProtocolCompatible`**: Wire protocol, serialization, and handshake verified.
* **`Connects`**: Successfully authenticates with NameServer and MasterServer.
* **`RoomCompatible`**: Creates and joins rooms, assigns actors, and synchronizes custom room/player properties.
* **`GameplayCompatible`**: Realtime game state replication, RPCs, and event relay operational between 2+ players.
* **`VoiceCompatible`**: Spatial/positional voice streaming active and synchronized.
* **`ChatCompatible`**: In-game text channels functional.
* **`Supported`**: End-to-end multiplayer fully functional and regression-tested.
* **`Broken`**: Protocol incompatibility or proprietary server plugin requirement identified.

---

## 2. Implementation & Audit Classification

| Component / Subsystem | Classification | Evidence & Real-World Validation |
| :--- | :---: | :--- |
| **ENet UDP Transport Engine** | `REAL_IMPLEMENTATION` | Fully implemented in C++ WinSock2 (`ENetDatagramHeader`, `ENetCommandHeader`, Ack handling, sequenced reliable & unreliable packet delivery). |
| **Photon Binary Protocol 1.6** | `REAL_IMPLEMENTATION` | Full type encoding & decoding (`GpType::Hashtable`, `Array`, `StringArray`, `IntegerArray`, `Dictionary`, `Custom`). Verified byte-for-byte against PUN 2 client. |
| **Diffie-Hellman Key Exchange** | `REAL_IMPLEMENTATION` | Oakley 768 / 96-byte key derivation via `InternalOperationRequest` (0x06) and `InternalOperationResponse` (0x07) for OpCode 0. |
| **MasterServer Flow** | `REAL_IMPLEMENTATION` | `OpAuthenticate (230)`, `OpCreateGame (227)`, `OpJoinGame (226)` redirection to GameServer with token exchange. |
| **GameServer Flow** | `REAL_IMPLEMENTATION` | `OpAuthenticate (230)`, `OpCreateGame (227)`, `OpJoinGame (226)`, `OpSetProperties (252)`, `OpGetProperties (251)`, `OpRaiseEvent (253)`. |
| **PUN 2 Live Game Interop (R.E.P.O.)** | `REAL_IMPLEMENTATION` | Verified with two concurrent real game instances (`REPO.exe`). Room creation, room joining, actor assignment, player callbacks, custom property sync, and RPC/Event relay verified live. |
| **Traffic Redaction & Diagnostics** | `REAL_IMPLEMENTATION` | Structured `UNSUPPORTED_OPERATION` diagnostics and automatic redaction of AppIDs, tokens, and credentials in logs. |
| **Photon Voice Engine** | `MOCK/STUB` | Basic voice server packet routing architecture defined; Opus payload relay unverified in live match. |
| **Photon Chat Engine** | `MOCK/STUB` | Channel subscribe/publish structures defined; not yet wired to live game UI. |
| **Photon Fusion Engine** | `MOCK/STUB` | High-level abstraction layer; Fusion Shared/Server network tick synchronization pending. |

---

## 3. Target Games Matrix (Live Verified)

| Game | Engine | Photon Product | Realtime | PUN 2 | Fusion | Voice | Chat | Current Status | Notes |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **R.E.P.O.** | Unity | PUN 2 / Photon Voice | ✅ | ✅ | ❌ (N/A) | 🔄 | ❌ | **GameplayCompatible** | Live verified with 2 real `REPO.exe` clients: Handshake, Auth, Room Create/Join, ActorNr, Callbacks, Properties, RaiseEvent, and RPC relay. |
| **Phasmophobia** | Unity | PUN / Photon Voice | ✅ | 🔄 | ❌ (N/A) | 🔄 | ❌ | **Investigating** | Supports redirection via custom proxy / UltimatePhobia reference. |
| **Roadside Research** | Unity | Photon Realtime / PUN | ✅ | 🔄 | ❌ | ❌ | ❌ | **Investigating** | Black-box observation and baseline protocol matching. |
| **Tabletop Simulator** | Unity | Photon Realtime | ✅ | 🔄 | ❌ (N/A) | ❌ | 🔄 | **Investigating** | Room synchronization and table state replication. |

*Legend: ✅ Fully Compatible & Live-Tested | 🔄 Under Verification / Stubbed | ❌ Not Applicable / Pending*

---

## 4. Live Verification Evidence (R.E.P.O. Host ↔ Re:Photon ↔ R.E.P.O. Joiner)

### Test Run Metrics & Verified Events:
1. **OpAuthenticate (230)**:
   - Host (`Player_1` / `Host_Client`) authenticated on MasterServer & GameServer.
   - Joiner (`Player_3` / `Joiner_Client`) authenticated on MasterServer & GameServer.
2. **OpCreateGame (227) & OpJoinGame (226)**:
   - Host created room `REPO_Interoperability_Room` (Assigned ActorNr: `1`, MasterClient: `1`).
   - Joiner joined room `REPO_Interoperability_Room` (Assigned ActorNr: `3`, MasterClient: `1`).
3. **Actor Numbers & Room State**:
   - Host `ClientState.Joined` reached (`InRoom = True`, `IsMaster = True`).
   - Joiner `ClientState.Joined` reached (`InRoom = True`, `IsMaster = False`).
4. **Player Lifecycle Callbacks**:
   - Host received `OnPlayerEnteredRoom: Actor 3 ('Joiner_Client')`.
   - Both clients received `EventCode::Join (255)` with complete ActorList and player property dictionaries.
5. **Room Properties Synchronization**:
   - `GameProperties` (`Map = Level_Underground`, `GameMode = Survival`) propagated to both peers.
   - `OpSetPropertiesOfActor (252)` and `OnPlayerPropertiesUpdate` synchronized.
6. **OpRaiseEvent (253) & Event Relay**:
   - Host broadcasted Event 100 (`Live_PUN_RPC_Data_From_Host_Client_Actor_1`).
   - Joiner broadcasted Event 100 (`Live_PUN_RPC_Data_From_Joiner_Client_Actor_3`).
   - Both clients received all event payloads with sender actor attribution.

---

## 5. Product Support Roadmap

1. **Milestone 0 (COMPLETED):** Photon Realtime & PUN 2 Core Compatibility (ENet UDP, DH Oakley-768 Handshake, MasterServer redirect, GameServer room management, ActorNr lifecycle, Property sync, RaiseEvent & RPC broadcast).
2. **Milestone 1 (NEXT):** Photon Voice Audio Transport & Spatial SFU relay.
3. **Milestone 2:** Photon Fusion Shared Mode State Synchronization.
4. **Milestone 3:** Photon Chat Channel & Direct Messaging.
5. **Milestone 4:** Photon Fusion Server Mode & Quantum Deterministic Simulation.
