#pragma once
#include <windows.h>
#include <cstdint>

namespace ReFixIL2CPP {
    // Initializes the generic IL2CPP bridge and game-specific hooks if GameAssembly.dll is present
    void Initialize();

    // Cleans up IL2CPP hooks and terminates the background watcher thread
    void Shutdown();

    // Per-frame main thread update hook (called from SteamAPI_ManualDispatch_RunFrame)
    void Update();

    // Returns true if GameAssembly.dll and IL2CPP runtime are loaded and active
    bool IsAvailable();

    // Activates P2P networking mode in supported game runtimes (e.g. Party Animals P2PRoomUtils)
    bool ActivateP2PMode();

    // Transitions the game UI to the main menu / NemoLand hall scene
    bool TransitionToMainMenu();
}
