# Re:Photon — Architecture Overview

## 1. Executive Summary

**Re:Photon** is a clean-room, modular C++ subsystem built within the **ReFix** framework. Its primary goal is to provide a unified, low-overhead compatibility layer for games built on the Photon networking ecosystem:

* **Photon Realtime** (Core matchmaking, room management, event relay, state synchronization)
* **PUN & PUN 2** (Photon Unity Networking high-level abstractions, PhotonViews, RPCs)
* **Photon Fusion** (State synchronization, tick-based simulation, shared and server topologies)
* **Photon Voice** (Positional/spatial voice audio routing and codec transport)
* **Photon Chat** (Channel and direct messaging)

---

## 2. Layered Architectural Design

Re:Photon is architected around strict separation of concerns, ensuring that game client interception, protocol serialization, transport mechanisms, and backend routing remain decoupled:

```
+-------------------------------------------------------------------------+
|                              Game Engine                                |
|          (Unity / Unreal / Native Game using Photon SDK)                |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
|                      ReFix Interception Layer                           |
|       (Proxy DLLs: steam_api64.dll / winmm.dll / EOSSDK / In-Game)       |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
|                         Re:Photon Core Engine                           |
|                                                                         |
|  +---------------------+  +---------------------+  +-----------------+  |
|  | Realtime Adapter    |  | PUN Adapter         |  | Fusion Adapter  |  |
|  +---------------------+  +---------------------+  +-----------------+  |
|  | Voice Adapter       |  | Chat Adapter        |  | Diagnostics     |  |
|  +---------------------+  +---------------------+  +-----------------+  |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
|                     Photon Abstraction Interfaces                       |
|   IPhotonBackend | IPhotonTransport | IPhotonRoomService | IPhotonAuth  |
+-------------------------------------------------------------------------+
                                    |
         +--------------------------+--------------------------+
         |                                                     |
         v                                                     v
+------------------------------------+  +------------------------------------+
|          ReFix Cloud               |  |           Custom Photon            |
|   - Local Dev Server (127.0.0.1)   |  |   - Official Photon Cloud          |
|   - Regional Game Server Pool      |  |   - Self-Hosted Private Server     |
|   - Cloudflare Workers Control     |  |   - User AppId & Credentials       |
+------------------------------------+  +------------------------------------+
```

---

## 3. Core Principles

1. **Clean-Room & Legal Compliance:** Zero proprietary Photon code or reverse-engineered binaries. All protocol implementations are constructed from public documentation, open standards, and observable network wire specifications.
2. **Cost-Aware Scalability:** Starts at $0 infrastructure cost for local development / LAN play and scales through low-cost regional relay servers and Cloudflare Workers for global presence.
3. **Multi-Target Agnostic:** No hardcoded game logic in the core engine. All game-specific characteristics are abstracted into extensible `GameProfile` definitions.
4. **Dual Backend Modality:** Direct toggle between `ReFixCloud` (managed infrastructure) and `CustomPhoton` (user-provided Photon account/self-hosted instances).
