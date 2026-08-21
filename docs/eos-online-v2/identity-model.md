# ReFix EOS Online v2 — Identity Model Specification

## 1. Identity Architecture Classification

ReFix EOS Online v2 formally implements:
**Model B: EOS-Compatible Local & Authoritative Identity Abstraction** `[DISASSEMBLY]` `[CONTROLLED TEST]`.

### Why Model B?
1. **Not a Proxy (Not Model D)**: ReFix does not connect to Epic Games cloud servers (`*.epicgames.com`). Games running under ReFix operate in decentralized LAN, private WAN, or community-hosted environments without Epic Games Launcher dependencies.
2. **Not Full Cloud Identity Emulation (Not Model A)**: ReFix does not emulate Epic Account Services (EAS) web OAuth2 login portals, parental controls, or Epic 2FA servers, as these are unnecessary for multiplayer matchmaking.
3. **An EOS-Compatible Authoritative Abstraction (Model B)**: ReFix implements 100% ABI-compliant opaque memory handles (`EOS_ProductUserId` / `EOS_EpicAccountId`) and state machines so that Unreal Engine, OnlineSubsystemEOS, and RedpointEOS interact with authentic EOS interfaces without realizing the cloud backend has been replaced by the lightweight ReFix Online Backend.

---

## 2. Identity Transformation Pipeline

```mermaid
graph TD
    subgraph "External Platform Identity"
        STEAM[SteamID64 / PersonaName]
    end

    subgraph "ReFix Persistent Layer"
        UUID[Persistent ReFix AccountUUID (Stored in user_profile.json)]
    end

    subgraph "EOS SDK ABI Layer"
        PUID[Opaque EOS_ProductUserId Handle]
        EAID[Opaque EOS_EpicAccountId Handle]
    end

    subgraph "Consumer (Game Engine)"
        RP[RedpointEOS / Unreal Engine IOnlineIdentity]
    end

    STEAM -->|Linked Account Mapping| UUID
    UUID -->|Deterministic Derivation SHA256| PUID
    UUID -->|Deterministic Derivation SHA256| EAID
    PUID -->|Returned in EOS_Connect_Login Callback| RP
    EAID -->|Auxiliary Account Resolution| RP
```

### 2.1. AccountUUID Specification `[CONTROLLED TEST]`
- Generated on first launch using cryptographically secure random bytes (`CoCreateGuid` / CryptoAPI random generator).
- Stored permanently in `%APPDATA%\ReFix\user_profile.json` (or local `saves\refix_user.json`).
- Format: Standard UUIDv4 string (e.g. `83953d40-9649-43ad-8089-01a284e71356`).
- **Invariance**: Survives reboots, hardware swaps, network adapters, and IP changes.

### 2.2. ProductUserId Derivation & Opaque Handle `[CONTROLLED TEST]`
- Canonical String Formula:
  $$\text{PUID\_String} = \text{Hex}(\text{SHA256}("EOS\_PUID:" \parallel \text{AccountUUID}))[0\dots31]$$
- Memory Handle:
  Allocated once in heap memory as an `OpaqueProductUserIdHandle` with magic signature `0x50554944` ('PUID').
- Thread-safe registry lookup guarantees handle address permanence for the process lifetime.

---

## 3. EpicAccountId: Role, Usage & Redpoint Analysis `[DISASSEMBLY]` `[EPIC DOCS]`

### 3.1. Why `EOS_EpicAccountId` Exists in the EOS SDK
In Epic's cloud architecture, there is a strict separation:
- **`EpicAccountId` (EAID)**: Represents an Epic Games Store / Epic Account Services identity (linked to an Epic email, friends list, and purchases).
- **`ProductUserId` (PUID)**: Represents a Game Services identity (specific to the game title/sandbox), to which multiple platform accounts (Steam, PSN, Xbox, Epic) can be linked.

### 3.2. Binary Audit of `EOS_EpicAccountId` in *MECCHA CHAMELEON* `[DISASSEMBLY]`
Inspection of `PenguinHotel-Win64-Shipping.exe` symbols reveals:
1. **Lobby Creation**: Uses `EOS_ProductUserId` exclusively (`FLobbyRoomProvider::ExecuteCreateRoomOperation`). `EOS_EpicAccountId` is **NOT** passed or checked.
2. **Session Matchmaking**: Uses `EOS_ProductUserId` exclusively (`FSyntheticSessionManager` and `EOS_Sessions_RegisterPlayers`).
3. **P2P Networking**: Uses `EOS_ProductUserId` exclusively (`EOS_P2P_SendPacket`).
4. **Presence**: Uses `EOS_ProductUserId` for local presence updates.
5. **Redpoint UserCache**: `EpicAccountIdsByDisplayNameQueue_Process` exists only as an optional lookup queue when querying Epic Games Store friends.
6. **Verdict**: **`EOS_EpicAccountId` is UNUSED in the core multiplayer matchmaking and gameplay loop.** ReFix provides safe opaque handles and conversion APIs (`ToString` / `FromString` / `IsValid`) for ABI compatibility, but it does not participate in multiplayer signaling.
