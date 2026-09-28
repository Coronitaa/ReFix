# Reference Projects & Architectural Comparison

## 1. Comparative Analysis

| Feature | Nemirtingas `eos_sdk_emu` | EOSLANKit | ReFix Legacy (`eos_proxy.cpp`) | ReFix EOS Online v2 (Proposed) |
| :--- | :--- | :--- | :--- | :--- |
| **Primary Focus** | Standalone Epic Games emulator | LAN UDP broadcast proxy | Steamworks wrapper | Online-grade hybrid EOS architecture |
| **Identity Model** | Deterministic / Config-based PUID | Local IP-based identity | Machine-derived SteamID in fake PUID | Logical, persistent ReFix Account UUID -> PUID |
| **Matchmaking Authority** | Local config / LAN sockets | Local UDP broadcast | Steam Matchmaking | Authoritative ReFix Online Backend |
| **Lobby State Machine** | Full in-memory state | Basic JSON UDP packet | Dynamic query lying | Authoritative state machine + dynamic attributes |
| **Sessions vs Lobbies** | Full support for both | Sessions-focused | Mixed / broken | Explicit Redpoint `FLobbyRoomProvider` support |
| **P2P Transport** | Direct UDP sockets | Direct UDP | Wrapped `ISteamNetworking` | Direct UDP (UPnP/STUN) + Encrypted Relay fallback |
| **Callbacks** | Worker thread queue | Synchronous | Deferred queue on Tick | Thread-safe queue dispatched on `EOS_Platform_Tick` |
