# ReFix EOS Online v2 — Technical References & Citations

## 1. Primary Source Repositories & Binaries

### Reference 1: ReFix Active Codebase
- **Source**: `https://github.com/Coronitaa/ReFix`
- **Active Branch**: `feature/eos-online-v2` (Base commit: `0ee88cb`)
- **Key Files Audited**:
  - `src/eos_proxy.cpp` (Lines 1–4958): Legacy EOS SDK proxy implementation.
  - `src/unreal_detect.cpp` (Lines 1–254): Unreal Engine & RedpointEOS auto-detection heuristics.
  - `src/unreal_steam_emu.cpp` (Lines 1–2714): Standalone Steamworks emulator.
  - `src/steam_proxy.cpp` (Lines 1–2760): Steam proxy hooks and P2P tunneling.
  - `src/eos_fwd.asm` (Lines 1–679): MASM x64 export forwarding table.
  - `ReFix.ini`: Configuration schema and subsystem toggles.

### Reference 2: Reference Target Game (*MECCHA CHAMELEON*)
- **Executable**: `d:\EOS_REFIX\MECCHA CHAMELEON\Chameleon\Binaries\Win64\PenguinHotel-Win64-Shipping.exe`
- **Engine**: Unreal Engine 5.x
- **Online Subsystem**: RedpointEOS (`Chameleon/Binaries/Win64/RedpointEOS/EOSSDK-Win64-Shipping.dll`)
- **Binary Dump Analysis**: Exactly 347 EOS SDK functions imported (documented in `docs/eos-online-v2/api-coverage.md`).

---

## 2. External Technical References

### Reference 3: Nemirtingas Epic Emulator Redux (`eos_sdk_emu`)
- **Repository**: `https://github.com/psfree/eos_sdk_emu`
- **Architectural Takeaways**:
  - Implementation of `ProductUserId` as a reference-counted handle wrapping a deterministic 32-character hex identity.
  - Clean separation of memory allocation for `EOS_SessionDetails_Info`, `EOS_LobbyDetails_Info`, and attribute arrays.
  - Asynchronous callback worker thread pattern with deferred dispatch queue.
  - Thread-safe handle tables mapping 64-bit opaque integer IDs to state structures.

### Reference 4: EOSLANKit
- **Repository**: `https://github.com/newtonjin/EOSLANKit`
- **Architectural Takeaways**:
  - UDP broadcast discovery protocol for local subnet game announcement.
  - Interception of `EOS_Sessions_CreateSessionModification` and `EOS_SessionSearch_Find` for LAN packet serialization.
  - Local host IP/Port advertisement pattern for Unreal NetDriver direct connection.

### Reference 5: Official Epic Online Services (EOS) Documentation
- **Source**: Epic Games Developer Portal (`https://dev.epicgames.com/docs/epic-online-services`)
- **Referenced Subsystems**:
  - *EOS Connect*: User authentication, external account mapping, DeviceId management.
  - *EOS Sessions*: Session creation, lifecycle states (`Creating` -> `Active` -> `Destroyed`), search parameters, attribute schemas.
  - *EOS Lobbies*: Member management, permission levels, dynamic attributes, lobby invites, RTC rooms.
  - *EOS P2P*: Peer-to-peer connections, NAT traversal, socket IDs, packet reliability modes.
  - *EOS Presence & Friends*: Rich presence strings, join status, friend subscriptions.

### Reference 6: Redpoint Games Online Subsystem EOS Documentation
- **Source**: `https://docs.redpoint.games/eos-online-subsystem/`
- **Referenced Mechanisms**:
  - Steam ticket acquisition via `FSteamCredentialObtainer`.
  - Session attribute naming conventions (`__EOS_bUsesPresence`, `__EOS_numPublicConnections`, `HOST_IP`, `HOST_PORT`).
  - Listen server travel handshakes via `ClientTravel`.
