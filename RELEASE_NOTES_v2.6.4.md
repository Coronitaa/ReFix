# ReFix v2.6.4

## What's New Since v2.6.3

ReFix v2.6.4 improves Steam Online Services emulation (dual-host Steam mask + achievement deduplication), Photon & LAN multi-instance room stability, singleplayer game pausing, Cloud Sync reliability, and overlay UI polish.

---

### Steam Online Services & Emulation
- **Dual-Host Steam Mask & Achievement Deduplication:** Running with a custom/current-game bridge AppID (with Official Servers off) now keeps Spacewar (`480`) active for networking while running the selected game as a companion `--mask` helper for Steam presence, achievements, and stats without duplicate progress/unlock forwards.
- **Official Servers Warning & Account Detection:** Added experimental Official Servers confirmation modal (noting incompatibility with ReFix LAN / other local instances on the same PC), fixed license probe detection on 0-license Steam tunnel accounts and normalized `SteamPath` / `SteamClientDll64` + `kCheckApp` live queries.
- **Per-Account Rich Presence Refcount:** Exiting one local game instance no longer clears Steam Rich Presence for another instance sharing the same Steam tunnel account.

### Multiplayer & Photon Fixes
- **Shared Tunnel Room Stability:** Fixed an issue where a second instance joining and disconnecting on the same Steam tunnel account caused `PeerRegistryImpl` / `UnrealSteamEmu::MemberLeft` / `PnServer::EnterRoom` to evict the host or tear down the lobby.

### Overlay, Cloud Sync & Audio
- **Cloud Sync Reliability & Rich Import Cards:** Switched `StateHash` to deterministic content hashing (`BLAKE2b`), excluded volatile runtime/log files, added `saves::ScopedBypass` so sync scans never trigger game save redirection, fixed `Apply()` writing directly to `saves::StashOf()`, prevented `FlushUserStats()` from overwriting restored cloud achievements, and upgraded the Cloud Import cards with clean game names (`+X`), current-game priority + size ordering, profile playtime, save/achievement counts (with current-game breakdown in parentheses), and extra stats/inventory counts.
- **Singleplayer Overlay Pause:** Implemented frame-boundary game pausing (`PeekMessageW`/`PeekMessageA`) for **Pause game** mode when not in a multiplayer room.
- **Audio Unmute on Restart:** Cleared Windows per-process audio mute on startup and before `RestartWithProfile` / `RestartGame` so restarted games never start muted.
- **UI & Notification Polish:** Wrapped toast notification text dynamically, removed extra warning badges outside the Bridge dropdown, and centered icons/labels across Settings.
- **Release Config:** `DevMode` defaults to `false` in `ReFix.ini`.
