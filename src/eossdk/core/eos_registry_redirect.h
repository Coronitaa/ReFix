#pragma once
#include <windows.h>

namespace refix {

// Installs Win32 Registry redirection hooks for EOS DeviceId isolation.
// Redirects "Software\Epic Games\EOS" to "Software\ReFix\Profiles\<ProfileID>\Epic Games\EOS".
void InstallRegistryRedirectHooks();

} // namespace refix
