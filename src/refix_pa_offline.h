#pragma once

// Party Animals offline login: switches StaticGameInfoAot.modeType to a mode for which
// StaticGameInfo.isLoginRequired is false (Lan / P2P / Unknown). In those modes the game's own
// TrailBlazer flow reports every login step as successful without contacting the backend, so the
// HybridCLR LoadingNode completes and the game loads the NemoLand hall through its normal path.
//
// The switch happens after the hot-update assemblies are loaded (InjectionNode.flowNodeGetter set),
// i.e. after PlatformInitNode has already initialized the Steam platform, and before TrailBlazer.Start.
namespace ReFixPAOffline {
    // Reads [P2P] OfflineModeType from ReFix.ini: "lan" (default), "p2p", "unknown" or "off".
    void Initialize();

    // Polls the IL2CPP runtime; call frequently (any thread) until IsFinished() returns true.
    void Tick();

    // Main-thread follow-up once engaged (Lan mode: StaticGameInfo.set_isLanMode(true)). Call every frame.
    void MainThreadTick();

    // True once the offline mode type is applied: the game now drives login and NemoLand on its own,
    // so ReFix must not force its synthetic transition.
    bool IsEngaged();

    // Logs every condition LoadingNode waits on before loading NemoLand (main thread only).
    void LogLoadingGate(void* menuSceneLoader);

    // True when there is nothing left to do (disabled, or engaged and the monitoring window elapsed).
    bool IsFinished();
}
