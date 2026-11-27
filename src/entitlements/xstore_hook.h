#pragma once

namespace refix::entitlements {

// Hooks Microsoft GDK XStore* APIs (xgameruntime.dll / XGamingRuntimeThunks.dll)
// to report add-ons as owned and active when ReFix DLC unlocking is enabled.
void InstallXStoreHooks(bool enabled);

} // namespace refix::entitlements
