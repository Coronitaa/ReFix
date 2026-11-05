// Suppresses NVIDIA Reflex in-game overlay, Reflex latency flash indicator,
// and GeForce Experience performance overlays for games starting with the ReFix overlay.
// Safe for DllMain and multi-engine environments (Unreal, Unity, Source, Native).
#pragma once

namespace refix::overlay {

// Sets environment switches, disables Reflex flash indicators, and arms in-process hooks if NVAPI is loaded.
// Idempotent and DllMain-safe.
void ArmNvidiaReflexBlock();

} // namespace refix::overlay
