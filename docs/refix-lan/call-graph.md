# ReFix LAN / Re:Goldberg — Architecture & Call Graph

## Current Implementation Map (Phase 1 Baseline)

```mermaid
flowchart TD
    subgraph GAME
        G1[Game / Engine]
        UE[Unreal Engine specific]
        Unity[Unity / Photon specific]
    end

    subgraph REFIX_ABI [ReFix Compatibility Layer]
        SAPI[steam_api64.dll / steam_api.dll]
        VTable[VTable Hooks]
        WSHook[Winsock Hooks]
        BIE[BepInEx Harmony Plugin]
        EOS[eos_proxy / EOS SDK v3]
    end

    subgraph PROVIDER [Current ReFix / Goldberg Providers]
        SEmu[unreal_steam_emu.cpp]
        Gold[Goldberg Steam Emulator]
        LAN_P2P[steam_p2p_hook.cpp / Multicast]
    end
    
    subgraph EXTERNAL [External Dependencies]
        Ipify[api.ipify.org - WinINet]
    end

    %% Steamworks Flow
    G1 -->|Steamworks API| SAPI
    SAPI -->|Forward Exports| Gold
    SAPI -->|VTable Intercept| VTable
    VTable -->|ISteamMatchmaking / ISteamFriends| SEmu
    
    %% Unreal Flow
    UE -->|Winsock sendto/recvfrom| WSHook
    WSHook -->|UDP Routing| LAN_P2P

    %% Photon Flow
    Unity -->|ConnectUsingSettings| BIE
    BIE -->|Local Config Injection| Unity
    Unity -->|UDP 5055, etc| WSHook
    
    %% EOS Flow
    G1 -->|EOS SDK calls| EOS
    EOS -->|HTTP GET Public IP| Ipify
    EOS -->|Bridge EOS Lobby/P2P| SEmu

    %% LAN Discovery
    SEmu -->|Lobby & Peer Discovery| LAN_P2P
```

## Summary of Findings (Phase 1 Audit)
- **Implemented:**
  - Steamworks ABI (x86/x64) via flat exports forwarding to Goldberg.
  - VTable hooking for crucial matchmaking/friends interfaces.
  - Winsock proxy for routing Unreal/Photon UDP traffic to Steam P2P.
  - EOS Lobby to Steam Matchmaking bridge.
- **Forwarded:**
  - Most Steam API flat exports are forwarded natively to the underlying Goldberg installation (`steam_api64_valve.dll`).
- **Hooked:**
  - `ISteamMatchmaking`, `ISteamMatchmakingServers`, `ISteamFriends`, `ISteamUGC`.
  - Winsock socket creation, `sendto`, `recvfrom`.
  - Unity Photon Realtime/Fusion endpoints via BepInEx.
- **Depends on Goldberg:**
  - Deep Steam profile management, stats, achievements, initial DLL loading.
- **Depends on Internet:**
  - `api.ipify.org` for Public IP (via EOS Proxy). 
  - Some SDR (Steam Datagram Relay) stubs exist but traffic is dropped or routed locally.
- **Depends on Steam Client:**
  - The offline mode generates an identity from `MachineGuid` and doesn't require a Steam installation.
