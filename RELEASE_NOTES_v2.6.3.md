# ReFix v2.6.3

## What's New Since v2.6.2

ReFix v2.6.3 adds full Photon Quantum 2.x multiplayer emulation (including late-join snapshot sync), fixes game exit deadlocks, and improves profile switching, per-game display settings, and Steam profile importing.

---

### Multiplayer & Photon Quantum 2.x
- **Photon Quantum 2.x Server Plugin (#41):** Added deterministic server-plugin emulation for Photon Quantum 2.x games (`StartRequest`, `Joined`, `SessionConfig`, `RuntimeConfig`, `SimulationStart`, `SetPlayerData`, `Command`, and verified tick broadcasts).
- **Late-Join Snapshot Sync:** Added `FrameSnapshotRequest` / `FrameSnapshot` forwarding and verified tick catch-up so players joining an already-running Quantum room sync cleanly instead of getting stuck on a blank screen.
- **Clean Room & Actor Cleanup:** Properly cleans up Quantum actor slots, pending RPCs/inputs, and snapshot waiters when a player or snapshot provider leaves.

### Stability & Exit Handling
- **No More Exit Deadlocks:** Fixed hangs when closing games (especially Unity IL2CPP titles) by removing unsafe loader-lock teardown in `DllMain` / `SteamP2PHook`, bounding `profile_sync` shutdown waits, and cleanly stopping the overlay host and Photon P2P threads.

### Overlay, Profiles & Display
- **Profile Switching Fixes:** Improved `REFIX_PROFILE_ID` and identity handling across `winhttp.dll`, `winmm.dll`, and `UnrealSteamEmu` when switching profiles.
- **Per-Game Display Settings:** Display mode and resolution can now be saved per game (`[OverlayDisplay.<GameKey>]`) directly from the overlay settings.
- **Import from Steam Grid:** Unified Steam profile importing into a cleaner grid modal.
