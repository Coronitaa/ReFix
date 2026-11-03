// Per-profile save isolation.
// Engines fix their save folders before steam_api is even loaded (Unreal caches %LOCALAPPDATA%\<Game>\Saved,
// Unity its LocalLow\<Company>\<Product> path), so redirecting SHGetKnownFolderPath would come too late.
// Instead, file opens under the user's data folders (Local, LocalLow, Roaming, Documents, Saved Games) are
// rewritten at the ntdll level to <profile>\Data\<folder>\..., whatever path string the game cached.
// System and vendor sub-folders (Temp, Microsoft, GPU shader caches, ...) are never redirected.
// Unity PlayerPrefs (HKCU\Software\<Company>\<Product>) are redirected to a per-profile registry key.
#pragma once
#include <string>

namespace refix::overlay {

// Installs the redirection for the active profile. Safe to call once; later calls are ignored.
bool InstallSaveRedirect(const std::wstring& profileId, const std::wstring& dataRoot);

} // namespace refix::overlay
