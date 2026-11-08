// NVIDIA Reflex vs. the ReFix overlay.
// With Reflex on, the game paces its frames through NvAPI_D3D_Sleep / low-latency mode, which left the overlay
// drawn over it flickering or see-through. While the overlay is open, Reflex is paused for the game (low
// latency off, Sleep returns at once) and restored with the game's own settings when it closes. The Reflex
// "latency flash" square is always dropped. Setting: Overlay settings > "Pause NVIDIA Reflex"
// ([Overlay] BlockNvidiaReflex in profiles.ini / ReFix.ini), applied from the next game start.
#pragma once

namespace refix::overlay {

// Environment switches + hooks on NVAPI's Reflex functions once nvapi64.dll is loaded. Idempotent; the
// hooks are installed outside DllMain only (PollNvidiaReflexHooks retries until the game loads NVAPI).
void ArmNvidiaReflexBlock();
void PollNvidiaReflexHooks();

// The overlay opened (true) or closed (false). No-op unless the block is armed.
void SetReflexPausedForOverlay(bool paused);
bool NvidiaReflexBlockArmed();   // the block is active in this game session

} // namespace refix::overlay
