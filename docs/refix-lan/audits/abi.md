# Steamworks ABI & Interface Audit Report

## 1. Overview
This repository contains two primary components for manipulating Steamworks API calls:
*   A **32-bit Proxy** (`src/proxy32/`) compiling to `steam_api.dll` which loads `steam_api_o.dll`.
*   A **64-bit Proxy** (`src/steam_proxy.cpp`) compiling to `steam_api64.dll` which hooks and forwards calls to `steam_api64_valve.dll`.

## 2. Export Forwarding
*   **x64:** Employs an ASM jump table (`steam_fwd.asm`) to forward 1057 flat API exports seamlessly without stack pollution.
*   **x86:** Utilizes a standard Module Definition (`.def`) file (`steam_api.def`) to alias flat exports.

## 3. VTable Hooks & Hooked Interfaces (x64)
The 64-bit proxy uses **MinHook** (`MH_Initialize()`) to dynamically hook VTable methods of interfaces returned by `SteamInternal_CreateInterface` and `SteamInternal_FindOrCreateUserInterface`.
*   **`ISteamMatchmaking`**: Slots 4-11 (Filters), 13-16 (Create, Join, Leave, Invite), 19-22, 24 (Lobby Data & Member Data).
*   **`ISteamMatchmakingServers`**: Slots 0 (`RequestInternetServerList`) and 1 (`RequestLANServerList`).
*   **`ISteamUtils`**: Slot 9 (`GetAppID`).
*   **`ISteamApps`**: Slots 6 (`BIsSubscribedApp`) and 7 (`BIsDlcInstalled`).
*   **`ISteamFriends`**: Slots 8 (`GetFriendGamePlayed`), 15 (`ActivateGameOverlayInviteDialog`), 16 (`InviteUserToGame`), 43 (`SetRichPresence`).
*   **`ISteamUGC`**: Slots 27 (`GetNumSubscribedItems`), 28 (`GetSubscribedItems`). Extracts a function pointer from slot 30.
*   **`ISteamNetworkingUtils`**: Slot 1 (`GetRelayNetworkStatus` - fixes hang).

## 4. Winsock P2P Hooks
To redirect Unreal Engine / Godot direct IP connections through Steamworks or vice versa, standard Winsock APIs are intercepted:
*   `sendto`, `recvfrom`, `connect`, `select`, `bind`, `closesocket`

## 5. Standalone Steam Emulator (unreal_steam_emu)
The repository contains a fully-fledged, ABI-compatible Steam emulator designed specifically for Unreal Engine (`src/unreal_steam_emu.cpp`), implementing a vast array of interfaces:
*   `ISteamClient`, `ISteamUser`, `ISteamFriends017`, `ISteamUtils`
*   `ISteamMatchmaking`, `ISteamMatchmakingServers`
*   `ISteamNetworking`, `ISteamNetworkingSockets`, `ISteamNetworkingUtils`, `ISteamNetworkingMessages`
*   `ISteamUserStats`, `ISteamApps`, `ISteamRemoteStorage`, `ISteamUGC`
*   `ISteamGameServer`, `ISteamGameServerStats`
*   `ISteamHTTP`, `ISteamInput`, `ISteamInventory`, `ISteamScreenshots`, `ISteamTimeline`

## 6. Calling Conventions & ABI Nuances
*   **x64:** Uses the standard Microsoft x64 `__fastcall` ABI. Functions simulating `__thiscall` simply accept `void* self` in the `RCX` register, mapping perfectly to the MSVC x64 ABI.
*   **x86:** Requires strict `__thiscall` specification for C++ methods (e.g., explicitly seen in `fn_VTable_GetRelayNetworkStatus_t`), though most of the heavy VTable hooking logic is centralized in the x64 proxy.

*(Note: The audit of the 32-bit emulator logic was abbreviated to prioritize the core 64-bit engine integrations per time constraints.)*
