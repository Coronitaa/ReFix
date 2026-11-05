# ReFix Mod API Reference

## Manifest Schema (`refixmod.json`)

```json
{
  "schema_version": 1,
  "id": "com.author.modname",
  "name": "Human Readable Name",
  "version": "1.0.0",
  "author": "Author Name",
  "author_pubkey": "64_char_hex_ed25519_pubkey",
  "entrypoint": "main.wasm",
  "execution_tier": "sandboxed",
  "phase": "EngineReady",
  "priority": 0,
  "target_games": [
    { "app_id": 480, "exe_name": "Game.exe", "engine": "unity_il2cpp" }
  ],
  "dependencies": {
    "before": [],
    "after": [],
    "requires": [],
    "conflicts": []
  },
  "capabilities": [
    "log",
    "config",
    "storage",
    "patch.declarative",
    "hooks.game",
    "net.inspect_channel",
    "net.override_channel",
    "memory.write",
    "players.display_name"
  ],
  "resource_limits": {
    "max_memory_mb": 16,
    "watchdog_timeout_ms": 15,
    "max_hooks": 16,
    "storage_quota_kb": 1024
  }
}
```

## Lifecycle Phases

1. `Boot` - Initial proxy injection before any ReFix patch
2. `PreRefix` - Before ReFix installs its core API proxies
3. `PostRefix` - ReFix core hooks active, overlay initialized
4. `GameInit` - Game process main entrypoint reachable
5. `EngineReady` - Game engine detected (Mono, IL2CPP, Unreal, etc.)
6. `NetworkReady` - Network subsystems and sockets initialized
7. `PreFrame` - Beginning of render/update frame
8. `PostFrame` - End of render frame, before SwapBuffers / Present
9. `LobbyJoined` - Multiplayer session joined
10. `MatchStart` - Game match starts
11. `Shutdown` - Orderly shutdown and cleanup

## Capabilities Reference

| Capability | Risk Level | Description |
|---|---|---|
| `log` | Low | Write messages to the ReFix log console |
| `config` | Low | Read and set mod-specific configuration options |
| `storage` | Low | Read and write isolated persistent key-value data |
| `events` | Low | Subscribe to game lifecycle events |
| `overlay.panel` | Low | Render custom tabs or widgets in the ReFix overlay |
| `patch.declarative` | Medium | Register declarative byte patches on game modules |
| `hooks.game` | Medium | Register mediated MinHook trampolines on game functions |
| `net.inspect_channel` | Medium | Inspect game payload on declared network channels |
| `net.override_channel` | Medium | Modify or drop game payload on declared channels |
| `memory.write` | **High** | Direct memory writes into game data sections / heap |
| `players.display_name` | **High** | Access real visible names of other players |
