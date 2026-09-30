// ReFix EOS v3 - Dynamic Passthrough Architecture to genuine EOSSDK_original.dll
#pragma once
#include "../core/refix_common.h"
#include "../eos_module.h"

namespace refix {

// Checks whether genuine EOSSDK_original.dll (or equivalent) is present on disk.
bool HasOriginalSdk();

// Initializes passthrough mode: loads genuine SDK, populates g_eosProcs for all exports,
// and registers the DeviceIdAuth and External Account synthesis hooks.
bool InitialisePassthrough(Registrar& reg);

} // namespace refix
