# Re:Photon — Compatibility Matrix

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

## 2. Target Games Matrix

| Game | Engine | Photon Product | Realtime | PUN | Fusion | Voice | Chat | Current Status | Notes |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **R.E.P.O.** | Unity | PUN 2 / Photon Voice | ✅ | ✅ | ❌ (N/A) | 🔄 | ❌ | **RoomCompatible** | Primary MVP target; overrides `PhotonServerSettings`. |
| **Phasmophobia** | Unity | PUN / Photon Voice | ✅ | ✅ | ❌ (N/A) | 🔄 | ❌ | **Investigating** | Supports redirection via custom proxy / UltimatePhobia reference. |
| **Roadside Research** | Unity | Photon Realtime / PUN | ✅ | 🔄 | ❌ | ❌ | ❌ | **Investigating** | Black-box observation and baseline protocol matching. |
| **Tabletop Simulator** | Unity | Photon Realtime | ✅ | ✅ | ❌ (N/A) | ❌ | 🔄 | **Investigating** | Room synchronization and table state replication. |

*Legend: ✅ Fully Compatible / Implemented | 🔄 Under Verification / Stubbed | ❌ Not Applicable / Pending*

---

## 3. Product Support Roadmap

1. **Milestone 0 (Current):** Photon Realtime & PUN 2 Core Compatibility (NameServer, MasterServer, GameServer, Lobbies, Rooms, Properties, Event Relay).
2. **Milestone 1:** Photon Voice Audio Transport & Spatial SFU Architecture.
3. **Milestone 2:** Photon Fusion Shared Mode State Synchronization.
4. **Milestone 3:** Photon Chat Channel & Direct Messaging.
5. **Milestone 4:** Photon Fusion Server Mode & Quantum Deterministic Simulation.
