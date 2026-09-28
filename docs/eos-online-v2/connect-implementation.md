# ReFix EOS Online v2 — Step 2 Connect Implementation Report

## 1. Executive Summary & Status

- **Status**: **IMPLEMENTED & VERIFIED**
- **Layer**: Connect & Authentication (`src/eos/eos_connect.h`, `src/eos/eos_connect.cpp`, `src/eos/eos_identity.h`, `src/eos/eos_identity.cpp`, `src/eos/eos_callbacks.h`, `src/eos/eos_callbacks.cpp`).
- **Target Subsystem**: `EOS_Connect_*`, User Identity, External Account Mappings, and Asynchronous Notification Registry.

---

## 2. Implemented Functions & APIs

The following public EOS SDK exports have been implemented with strict ABI version checks, thread safety, and opaque handle memory validation:

| Function Export | ABI Contract | Description |
| :--- | :---: | :--- |
| `EOS_Connect_Login` | Async (Tick) | Validates credentials (`STEAM_SESSION_TICKET` or `DEVICEID_ACCESS_TOKEN`), maps identity, and queues `EOS_Connect_LoginCallbackInfo`. |
| `EOS_Connect_CreateUser` | Async (Tick) | Consumes `ContinuanceToken` and returns created persistent `LocalUserId`. |
| `EOS_Connect_LinkAccount` | Async (Tick) | Links external account tokens to local `ProductUserId`. |
| `EOS_Connect_CreateDeviceId` | Async (Tick) | Registers virtual hardware DeviceId for standalone fallback authentication. |
| `EOS_Connect_DeleteDeviceId` | Async (Tick) | Deletes local DeviceId registration. |
| `EOS_Connect_Logout` | Async (Tick) | Cleans local login session and triggers status callback. |
| `EOS_Connect_QueryExternalAccountMappings` | Async (Tick) | Resolves SteamID / external account arrays to PUIDs. |
| `EOS_Connect_GetExternalAccountMapping` | Synchronous | Returns mapped `EOS_ProductUserId` for a target external account ID. |
| `EOS_Connect_QueryProductUserIdMappings` | Async (Tick) | Queries display names and metadata for PUID arrays. |
| `EOS_Connect_GetProductUserIdMapping` | Synchronous | Converts target `ProductUserId` to string buffer. |
| `EOS_Connect_GetProductUserExternalAccountCount` | Synchronous | Returns count of linked accounts (2: Epic + Steam). |
| `EOS_Connect_CopyProductUserInfo` | Synchronous | Returns `EOS_Connect_ExternalAccountInfo` with SteamID64 and display name. |
| `EOS_Connect_CopyProductUserExternalAccountByIndex` | Synchronous | Returns external account data by index. |
| `EOS_Connect_CopyProductUserExternalAccountByAccountType` | Synchronous | Returns external account data by type (`EOS_EAT_STEAM` / `EOS_EAT_EPIC`). |
| `EOS_Connect_CopyProductUserExternalAccountByAccountId` | Synchronous | Returns external account data by account ID string. |
| `EOS_Connect_ExternalAccountInfo_Release` | Memory Free | Safely frees heap allocations from `CopyProductUserInfo`. |
| `EOS_Connect_GetLoggedInUserByIndex` | Synchronous | Returns local `EOS_ProductUserId` (index 0). |
| `EOS_Connect_GetLoggedInUsersCount` | Synchronous | Returns 1 (single-user local context). |
| `EOS_Connect_GetLoginStatus` | Synchronous | Returns `EOS_LS_LoggedIn` for valid local PUID; `EOS_LS_NotLoggedIn` otherwise. |
| `EOS_Connect_AddNotifyLoginStatusChanged` | Notification | Subscribes callback to login status change events. |
| `EOS_Connect_RemoveNotifyLoginStatusChanged` | Notification | Unregisters login status notification. |
| `EOS_Connect_AddNotifyAuthExpiration` | Notification | Subscribes callback to auth expiration events. |
| `EOS_Connect_RemoveNotifyAuthExpiration` | Notification | Unregisters auth expiration notification. |

---

## 3. Observed Behavior & Architectural Guarantees

1. **Strict Asynchronous Execution**:
   - `EOS_Connect_Login` never executes the callback synchronously within the API entry point.
   - All async completion delegates are queued into `CallbackManager` and flushed strictly during `EOS_Platform_Tick()`.
2. **Explicit Credential State Machine**:
   - `EOS_ECT_STEAM_SESSION_TICKET` (Type `1`): Validates token length, resolves Steam persona and SteamID64 into `IdentityManager`, maps to local persistent `AccountUUID`, and returns `LocalUserId` with `EOS_Success`.
   - `EOS_ECT_DEVICEID_ACCESS_TOKEN` (Type `11`): Resolves persistent local PUID.
   - Invalid or null credentials safely return `EOS_InvalidParameters` or `EOS_InvalidAuth`.
3. **ContinuanceToken Handling**:
   - When user creation is simulated or required, a heap-allocated opaque handle (`magic = 0x43544F4B`) is returned with `EOS_InvalidUser`, safely consumed by `EOS_Connect_CreateUser`.
4. **Diagnostic Logging with Redaction**:
   - Sensitive credential tokens (Steam session tickets) are never printed in cleartext and are safely redacted as `[REDACTED:len=N]`.

---

## 4. Deviations from Official Cloud EOS SDK

| Feature | Official Cloud EOS SDK | ReFix EOS Online v2 | Rationale |
| :--- | :--- | :--- | :--- |
| **Backend Dependency** | Requires HTTPS connection to Epic auth servers (`api.epicgames.dev`) | Local / Private ReFix Online Backend | Eliminates external cloud dependencies, offline/LAN playable. |
| **Token Verification** | Decrypted on Epic Game Services backend with Steam Web API key | Validated locally and mapped to persistent `AccountUUID` | Works seamlessly with Goldberg Steam emulator or Spacewar (AppID 480). |
| **Token Expiration** | Tokens expire after 1 hour (requires refresh callback) | Persistent across process lifetime | Prevents sudden session disconnects during offline/LAN play. |

---

## 5. Verification & Test Results

### 5.1. Unit Test Suite Results (100% Pass Rate)
- **`test_identity.exe`**:
  - Validates local opaque handle persistence, PUID/EAID derivations, cross-process simulation, foreign PUID independence, buffer boundary checks, and pointer validation without crashes.
- **`test_callbacks.exe`**:
  - Validates asynchronous queueing, deferred tick dispatch, 4-thread concurrent queueing (1,000 items), and notification subscription lifecycles.
- **`test_connect_contract.exe`**:
  - Validates complete mock contract: `EOS_Connect_Login -> Tick -> PUID -> QueryProductUserIdMappings -> CopyProductUserInfo -> Release`.
- **`test_connect_login.exe`**:
  - Validates Steam ticket login, repeated login idempotency, bad credential rejection (`EOS_InvalidParameters` / `EOS_InvalidAuth`), and concurrent login queueing.
- **`test_connect_deviceid.exe`**:
  - Validates `CreateDeviceId`, `Login` with `DEVICEID_ACCESS_TOKEN`, and `DeleteDeviceId`.
- **`test_connect_external_account.exe`**:
  - Validates `QueryProductUserIdMappings`, `CopyProductUserInfo` (SteamID64 + DisplayName), index/type queries, `GetLoggedInUserByIndex`, and `GetLoginStatus`.

### 5.2. Real Game Runtime Integration
- Target game: *MECCHA CHAMELEON* (`PenguinHotel-Win64-Shipping.exe`, UE5 + RedpointEOS).
- Binaries deployed via `deploy.bat`: `winmm.dll`, `steam_api64.dll`, `RedpointEOS\EOSSDK-Win64-Shipping.dll`, `RedpointEOS\RedboneEOS.dll`.
- Game launched cleanly (PID 31612), loaded all proxy DLLs, initialized subsystems without access violations, and ran steadily.
