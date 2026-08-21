# ReFix Legacy EOS Implementation Audit (`src/eos_proxy.cpp`)

## 1. Overview & File Structure

The current EOS implementation in ReFix is contained entirely in:
- `src/eos_proxy.cpp` (4,958 lines of C++)
- `src/eos_fwd.asm` (Assembly forwarding stubs for 679 DLL exports)
- `src/eos_proxy.def` (DLL export definition file)

The legacy code was designed as a lightweight translation proxy that forwarded EOS calls into the Steamworks API (`steam_api64.dll` / `unreal_steam_emu.cpp`). While this allowed games to boot and show the Steam overlay, it introduced fundamental architectural flaws that broke multiplayer functionality in Unreal Engine titles using RedpointEOS.

---

## 2. Critical Flaws in Legacy Implementation

### Flaw 1: Pointer Heuristics and Memory Scanning (`FindStringPointer`, `FindPuidPointer`)
In `src/eos_proxy.cpp` (lines 415–450), instead of maintaining properly typed struct definitions for each EOS SDK `ApiVersion`, the code attempts to parse structs by scanning raw memory for 64-bit pointer values:

```cpp
// src/eos_proxy.cpp lines 415-435
static const char* FindStringPointer(const void* optionsStruct, size_t sizeBytes) {
    if (!optionsStruct) return nullptr;
    const uint64_t* ptrs = (const uint64_t*)optionsStruct;
    size_t count = sizeBytes / 8;
    for (size_t i = 0; i < count; i++) {
        uint64_t val = ptrs[i];
        if (val > 0x10000 && !IsBadReadPtr((const void*)val, 1)) {
            const char* str = (const char*)val;
            // Checks if printable ASCII...
        }
    }
    return nullptr;
}
```
**Consequence**:
- Severe memory access violations / crashes if the struct contains integer fields (timestamps, bitmasks, IDs) that happen to fall within valid virtual address ranges.
- Incorrect field extraction when multiple string pointers exist in a struct (e.g., `BucketId` vs `SessionName` vs `HostAddress`).

---

### Flaw 2: Fake ProductUserId Collisions and Static Identity
In `src/eos_proxy.cpp` (lines 359–363, 838–844), identity is assigned statically or semi-statically:

```cpp
static char s_fakeProductUserId[64] = "refix_product_user_00001";
static char s_fakeEpicAccountId[64] = "refix_epic_account_00001";
#define FAKE_PRODUCT_USER_ID ((EOS_ProductUserId)s_fakeProductUserId)
#define FAKE_EPIC_ACCOUNT_ID ((EOS_EpicAccountId)s_fakeEpicAccountId)
```
- In `RefreshUserName()` (lines 838–842), `s_fakeProductUserId` is overwritten with the local player's machine-derived SteamID.
- In `eos_LobbyDetails_GetMemberByIndex` (line 2712) and `eos_LobbyDetails_GetLobbyOwner` (line 2732), it **always** returns `FAKE_PRODUCT_USER_ID`.
- In `eos_Sessions_RegisterPlayers` (line 3804), it returns a static array `s_dummyPlayerIds` containing only `FAKE_PRODUCT_USER_ID`.

**Consequence**:
- All remote players in a session appear to have the exact same `ProductUserId` as the local host.
- RedpointEOS cannot differentiate between the local player and remote peers, causing player registration failures, duplicate session member errors, and NetDriver connection aborts.

---

### Flaw 3: Search Parameter "Dynamic Lying"
In `src/eos_proxy.cpp` (lines 1803–1836, 2444–2522):
When the client searches for lobbies using `EOS_LobbySearch_SetParameter`, the proxy stores the search query parameters in a global vector `g_capturedSearchParameters`.

When the game later calls `EOS_LobbyDetails_CopyAttributeByIndex` (line 2461) or `CopyAttributeByKey` (line 2530), the proxy reflects whatever parameters the client searched for back as the lobby's own attributes:

```cpp
// src/eos_proxy.cpp lines 2488-2495
if (index < g_capturedSearchParameters.size()) {
    keyName = g_capturedSearchParameters[index].Key;
    valStr = g_capturedSearchParameters[index].Value.AsUtf8;
}
```
**Consequence**:
- Lobbies do not store real host-configured attributes.
- If two games search with different filters, lobby metadata is corrupted and desynchronized.

---

### Flaw 4: Hardcoded 4-Member Slots & Synthetic Session Attributes
In `src/eos_proxy.cpp`:
- `eos_LobbyDetails_CopyInfo` (lines 2415–2418):
  ```cpp
  info->AvailableSlots = 3;
  info->MaxMembers = 4;
  info->BucketId = "refix_bucket";
  ```
- `eos_SessionDetails_CopyInfo` (lines 3416–3436):
  ```cpp
  settings->NumPublicConnections = 4;
  settings->BucketId = _strdup("refix_bucket");
  info->SessionId = _strdup("RefixSession_001");
  info->NumOpenPublicConnections = 4;
  ```
- `eos_SessionDetails_GetAttributeCount` (line 3530):
  Always returns 10 static attributes (`__EOS_bUsesPresence`, `SERVER_IP`, `HOST_PORT`, etc.).

**Consequence**:
- Every session in every game is locked to 4 slots with bucket `refix_bucket`. Games expecting 2, 8, 16, or 32 player lobbies or custom game buckets fail matchmaking validation.
- Every session is named `RefixSession_001`, leading directly to `AlreadyExists` errors when attempting to create a second session or recreate an old one.

---

### Flaw 5: Asynchronous Steam Lobby Tunneling Mismatch
In `src/eos_proxy.cpp` (lines 1863–2015, 2918–2928), when `EOS_Lobby_CreateLobby` or `EOS_Sessions_UpdateSession` is called:
1. It returns immediate success to Unreal via `QueueCallback`.
2. In the background, `CreateAndTagRealSteamLobbyAsync()` spawns a detached thread to call Steamworks `CreateLobby`.
3. If Steam is not running or Goldberg hasn't created the lobby yet, it assigns a synthetic Steam Lobby ID `0x0110000100000000 | (steamId & 0xFFFFFFFF)`.

**Consequence**:
- Unreal believes the EOS session is created immediately, but the underlying Steam lobby might take seconds or fail entirely.
- Joining a session tries to resolve a Steam lobby that may not exist, resulting in join timeouts.

---

### Flaw 6: Non-functional Notification Callbacks
In `src/eos_proxy.cpp`:
- `eos_Lobby_AddNotifyLobbyInviteReceived` (line 2651): Returns incremented ID, never stores callback pointer or fires.
- `eos_Sessions_AddNotifySessionInviteReceived` (line 3734): Returns incremented ID, never fires.
- `eos_P2P_AddNotifyPeerConnectionRequest` (line 3241): Returns static ID 1, never fires.

**Consequence**:
- In-game invites never trigger the receiver's UI.
- P2P connection requests from remote peers are never signaled to the game engine.
