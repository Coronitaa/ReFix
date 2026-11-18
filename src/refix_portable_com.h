// Portable XAudio2 2.7: games built on the DirectX June 2010 SDK create XAudio2 through COM, which needs the runtime's
// registry entries. On a PC without that runtime (a portable copy), CoCreateInstance fails and the game has no sound.
// This hook creates those classes from the game's own XAudio2_7.dll instead, only when COM reports them unregistered.
#pragma once

void ReFix_InstallPortableXAudio2();
