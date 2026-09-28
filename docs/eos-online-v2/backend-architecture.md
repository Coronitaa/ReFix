# ReFix Online v2 — Authoritative Backend Architecture Specification

## 1. Architectural Overview & Separation of Concerns

ReFix EOS Online v2 enforces strict architectural decoupling between the public Epic Online Services (EOS) C ABI, the intermediate translation layer (`eos_core`), and the authoritative backend subsystem (`refix_online`):

```
┌────────────────────────────────────────────────────────┐
│               RedpointEOS (Unreal Engine 5)           │
└──────────────────────────┬─────────────────────────────┘
                           │ Public EOS C ABI Calls
┌──────────────────────────▼─────────────────────────────┐
│             src/eos/ (ABI Entry Points)               │
│   (EOS_Connect_*, EOS_Lobby_*, EOS_ProductUserId_*)   │
└──────────────────────────┬─────────────────────────────┘
                           │ Translates EOS Handles & Pointers
┌──────────────────────────▼─────────────────────────────┐
│          src/eos_core/ (Room Manager Bridge)           │
│           (eos_room_manager.h / cpp)                   │
└──────────────────────────┬─────────────────────────────┘
                           │ Requests via IRefixTransport (Binary Protocol)
┌──────────────────────────▼─────────────────────────────┐
│         src/refix_online/ (Client Subsystem)          │
│       (refix_wire, protocol, backend_client)          │
└──────────────────────────┬─────────────────────────────┘
                           │ Network / Direct In-Process Protocol
┌──────────────────────────▼─────────────────────────────┐
│    Authoritative ReFix Online Backend Server State     │
│       (refix_backend_state.h / cpp)                    │
└────────────────────────────────────────────────────────┘
```

### Decoupling Rules:
1. **Server Ignorance of EOS**: The backend server and binary protocol **NEVER** handle EOS pointers, EOS handles, `EOS_EResult` enum values, or Unreal Engine data structures.
2. **Canonical Logical Identity**: All identities across the wire are represented as 32-character hexadecimal strings derived deterministically from the persistent `AccountUUID` (`ProductUserId`).
3. **Absolute Server Authority**: The backend is the sole authority for:
   - Lobby creation and global ID allocation
   - Active lobby membership and slot capacity
   - Host ownership assignment and ownership migration upon disconnect
   - Attribute validation and synchronization
   - Member heartbeat tracking and stale session expiration

---

## 2. Component Structure

### 2.1. `src/refix_online/` (Network & Protocol Engine)
- `refix_wire.h` / `refix_wire.cpp`: Low-level binary stream reader (`ByteReader`) and writer (`ByteWriter`) with strict bounds checking and memory safety guarantees.
- `refix_backend_protocol.h` / `refix_backend_protocol.cpp`: Packet header definitions, message enums, security limits, and packet serialization functions.
- `refix_backend_client.h` / `refix_backend_client.cpp`: Client networking abstraction (`IRefixTransport`, `InProcessDirectTransport`), client state machine, and asynchronous `RequestId` response correlation.
- `refix_backend_state.h` / `refix_backend_state.cpp`: Authoritative in-memory state engine (`BackendServerState`) managing sessions, active lobbies, membership rosters, and heartbeat timeouts.

### 2.2. `src/eos_core/` (Room Manager Bridge)
- `eos_room_manager.h` / `eos_room_manager.cpp`: Bridge that routes incoming room requests (`CreateLobby`, `FindLobbies`, `JoinLobby`, `LeaveLobby`, `ResyncLobby`) from the EOS ABI layer to the backend client.
