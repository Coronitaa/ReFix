// NVIDIA Reflex vs. the ReFix overlay.
// With Reflex on, the game paces its frames through NvAPI_D3D_Sleep / low-latency mode, which left the overlay
// drawn over it flickering or see-through. With the setting on, Reflex never runs in the game: NVAPI stops
// offering its functions (the game sees a driver without Reflex), and a game that looked them up earlier gets
// low latency forced off, no frame pacing and no latency markers. Setting: Overlay settings > Performance >
// "Turn off NVIDIA Reflex" ([Overlay] BlockNvidiaReflex in profiles.ini / ReFix.ini), from the next game start.
#pragma once

namespace refix::overlay {

// Environment switches + hooks on NVAPI's Reflex functions once nvapi64.dll is loaded. Idempotent; the
// hooks are installed outside DllMain only (PollNvidiaReflexHooks retries until the game loads NVAPI).
void ArmNvidiaReflexBlock();
void PollNvidiaReflexHooks();

bool NvidiaReflexBlockArmed();

// NVIDIA App's in-game overlay is a transparent always-on-top window over every game; it fights the ReFix
// overlay for the top and makes it flicker. Hidden while the ReFix overlay is open, shown again after.
void SetNvidiaOverlayHidden(bool hidden);   // the block is active in this game session

} // namespace refix::overlay
