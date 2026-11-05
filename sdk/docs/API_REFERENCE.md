# ReFix Mod API Reference

This document provides the definitive specification for the **ReFix Mod C ABI** (`sdk/include/refix_mod_api.h`), host imports, mod exports, lifecycle phases, capabilities, and data types.

---

## 1. Versioning & ABI Stability

All structures passed across the ReFix host-mod boundary are strictly versioned using a leading `uint32_t struct_size` and `uint32_t api_version`. This guarantees forward and backward binary compatibility across updates.

### Versioning Constants & Macros
```c
#define REFIX_MOD_API_VERSION_MAJOR 1
#define REFIX_MOD_API_VERSION_MINOR 0
#define REFIX_MOD_API_VERSION_PATCH 0

#define REFIX_MAKE_VERSION(major, minor, patch) \
    (((uint32_t)(major) << 16) | ((uint32_t)(minor) << 8) | ((uint32_t)(patch)))

#define REFIX_MOD_API_VERSION \
    REFIX_MAKE_VERSION(REFIX_MOD_API_VERSION_MAJOR, REFIX_MOD_API_VERSION_MINOR, REFIX_MOD_API_VERSION_PATCH)
```

### Version Structure
```c
typedef struct RefixVersion {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint16_t reserved;
} RefixVersion;
```

---

## 2. Status & Error Codes (`RefixResult`)

Every API function returning a status code conforms to `RefixResult`:

```c
typedef enum RefixResult {
    REFIX_SUCCESS                 = 0,   /* Operation completed successfully */
    REFIX_ERROR_GENERIC           = -1,  /* Unspecified internal error */
    REFIX_ERROR_INVALID_ARGUMENT  = -2,  /* Null pointer or invalid parameter passed */
    REFIX_ERROR_PERMISSION_DENIED = -3,  /* Mod does not possess the required capability */
    REFIX_ERROR_NOT_FOUND         = -4,  /* Target module, function, or key does not exist */
    REFIX_ERROR_ALREADY_EXISTS    = -5,  /* Item or hook already registered */
    REFIX_ERROR_RESOURCE_EXHAUSTED= -6,  /* Linear memory, hook limit, or storage quota exceeded */
    REFIX_ERROR_TIMEOUT           = -7,  /* Watchdog execution timer expired */
    REFIX_ERROR_NOT_SUPPORTED     = -8,  /* Feature not supported on this platform/engine */
    REFIX_ERROR_VERSION_MISMATCH  = -9,  /* Incompatible ABI version */
    REFIX_ERROR_INTEGRITY_FAILED  = -10  /* Verification or hash checksum failure */
} RefixResult;
```

---

## 3. Lifecycle Phases (`RefixLifecyclePhase`)

Execution is organized into 11 strictly ordered phases. Dependencies between mods and internal ReFix hooks are scheduled via topological sort.

```c
typedef enum RefixLifecyclePhase {
    REFIX_PHASE_BOOT          = 0,  /* Proxy loaded; before any ReFix internal hooking */
    REFIX_PHASE_PRE_REFIX     = 1,  /* Before ReFix core hooks install */
    REFIX_PHASE_POST_REFIX    = 2,  /* ReFix core hooks installed; overlay initialized */
    REFIX_PHASE_GAME_INIT     = 3,  /* Game entry point (WinMain/main) reached */
    REFIX_PHASE_ENGINE_READY  = 4,  /* Engine runtime identified (Mono, IL2CPP, UE, Godot, Source) */
    REFIX_PHASE_NETWORK_READY = 5,  /* Network sockets and transport interfaces online */
    REFIX_PHASE_PRE_FRAME     = 6,  /* Beginning of render/update loop */
    REFIX_PHASE_POST_FRAME    = 7,  /* End of render loop, immediately before Present / SwapBuffers */
    REFIX_PHASE_LOBBY_JOINED  = 8,  /* Multiplayer lobby or session joined */
    REFIX_PHASE_MATCH_START   = 9,  /* Game match/round started */
    REFIX_PHASE_SHUTDOWN      = 10  /* Process shutdown; unhook and cleanup */
} RefixLifecyclePhase;
```

---

## 4. Execution Tiers (`RefixExecutionTier`)

```c
typedef enum RefixExecutionTier {
    REFIX_TIER_SANDBOXED = 0, /* WebAssembly module in wasm3 sandbox (Default for all community mods) */
    REFIX_TIER_NATIVE    = 1  /* Win32 C ABI PE DLL (Strictly restricted to official ReFix key signatures) */
} RefixExecutionTier;
```

---

## 5. Engines & Protocols Supported

### Engine Types
```c
typedef enum RefixEngineType {
    REFIX_ENGINE_UNKNOWN      = 0,
    REFIX_ENGINE_UNITY_MONO   = 1,
    REFIX_ENGINE_UNITY_IL2CPP = 2,
    REFIX_ENGINE_UNREAL_4     = 3,
    REFIX_ENGINE_UNREAL_5     = 4,
    REFIX_ENGINE_GODOT        = 5,
    REFIX_ENGINE_SOURCE       = 6,
    REFIX_ENGINE_SOURCE_2     = 7,
    REFIX_ENGINE_GAMEMAKER    = 8,
    REFIX_ENGINE_NATIVE       = 9
} RefixEngineType;
```

### Protocol Types
```c
typedef enum RefixProtocolType {
    REFIX_PROTO_UNKNOWN          = 0,
    REFIX_PROTO_STEAM_NETWORKING = 1,
    REFIX_PROTO_STEAM_SOCKETS    = 2,
    REFIX_PROTO_STEAM_LOBBIES    = 3,
    REFIX_PROTO_EOS_P2P          = 4,
    REFIX_PROTO_EOS_SESSIONS     = 5,
    REFIX_PROTO_PHOTON_PUN       = 6,
    REFIX_PROTO_PHOTON_FUSION    = 7,
    REFIX_PROTO_LAN_CORE         = 8,
    REFIX_PROTO_RAW_SOCKETS      = 9,
    REFIX_PROTO_DISCOVERY        = 10
} RefixProtocolType;
```

---

## 6. Core Data Structures

### 6.1 Declarative Byte Patch Descriptor
Used to register byte patches without requiring `VirtualProtect` or executable memory:

```c
typedef struct RefixDeclarativePatch {
    uint32_t struct_size;          /* sizeof(RefixDeclarativePatch) */
    uint32_t api_version;          /* REFIX_MOD_API_VERSION */
    const char* module_name;       /* e.g., "GameAssembly.dll" or NULL for game exe */
    uint64_t rva;                  /* Relative Virtual Address from module base */
    const uint8_t* expected_bytes; /* Byte array that MUST match memory before patch */
    size_t expected_size;          /* Length of expected_bytes */
    const uint8_t* new_bytes;      /* Byte array to write */
    size_t new_size;               /* Length of new_bytes */
} RefixDeclarativePatch;
```

### 6.2 Mediated Hook Descriptor
Used to register mediated MinHook trampolines:

```c
typedef struct RefixHookDescriptor {
    uint32_t struct_size;          /* sizeof(RefixHookDescriptor) */
    uint32_t api_version;          /* REFIX_MOD_API_VERSION */
    const char* module_name;       /* Target module containing function */
    const char* function_name;     /* Exported function name, or NULL if using RVA/pattern */
    uint64_t rva;                  /* RVA if function_name is NULL */
    const char* pattern;           /* Signature pattern (e.g. "48 89 5C 24 ? 57") */
    const char* pattern_mask;      /* Mask if not using inline ? */
    uint32_t wasm_callback_index;  /* Index of WASM export callback */
    void** out_original_fn;        /* Receives pointer to trampoline (native mods only) */
} RefixHookDescriptor;
```

### 6.3 Network Packet Filter
Used to inspect or alter packets on specific channels:

```c
typedef struct RefixPacketFilter {
    uint32_t struct_size;          /* sizeof(RefixPacketFilter) */
    uint32_t api_version;          /* REFIX_MOD_API_VERSION */
    uint32_t protocol_type;        /* RefixProtocolType */
    uint32_t channel_id;           /* Target channel number */
    uint32_t flags;                /* 1 = Inspect only, 2 = Allow Override/Drop */
} RefixPacketFilter;
```

---

## 7. Host API Exports (`RefixHostApi`)

The host passes a pointer to this table during `refix_mod_init`:

```c
typedef struct RefixHostApi {
    uint32_t struct_size;
    uint32_t api_version;

    /* Logging */
    void (*log)(int32_t level, const char* message);

    /* Time */
    uint64_t (*get_time_ms)(void);

    /* Persistent Storage (Sandboxed key-value per mod) */
    int32_t (*storage_read)(const char* key, uint8_t* out_data, size_t max_len, size_t* out_actual_len);
    int32_t (*storage_write)(const char* key, const uint8_t* data, size_t len);

    /* Declarative Patching */
    int32_t (*register_patch)(const RefixDeclarativePatch* patch);

    /* Mediated Hooking */
    int32_t (*register_hook)(const RefixHookDescriptor* hook, uint32_t* out_hook_id);
    int32_t (*unregister_hook)(uint32_t hook_id);

    /* Network Overrides */
    int32_t (*register_packet_filter)(const RefixPacketFilter* filter);
    int32_t (*send_packet)(uint64_t peer_handle, uint32_t channel, const uint8_t* data, size_t len);

    /* Memory Writing (Requires memory.write capability) */
    int32_t (*memory_write)(const char* module_name, uint64_t rva, const uint8_t* data, size_t len);

    /* Engine State Query */
    int32_t (*get_engine_info)(uint32_t* out_engine_type, void** out_engine_context);

} RefixHostApi;
```

---

## 8. Mod Export Functions

Every mod compiled to WASM or Native PE exports standard entrypoints:

### `refix_mod_init`
```c
REFIX_MOD_EXPORT int32_t refix_mod_init(const RefixHostApi* host, const char* mod_id);
```
- **When Called**: At module load time during the phase specified in the manifest.
- **Return**: `REFIX_SUCCESS` (0) on success, or an error code.

### `refix_mod_on_phase`
```c
REFIX_MOD_EXPORT void refix_mod_on_phase(int32_t phase);
```
- **When Called**: Dispatched whenever a lifecycle phase transition occurs.

### `refix_mod_on_packet`
```c
REFIX_MOD_EXPORT int32_t refix_mod_on_packet(
    uint32_t protocol_type,
    uint64_t peer_handle,
    uint32_t channel,
    const uint8_t* data,
    size_t len,
    uint8_t* out_modified_data,
    size_t* inout_modified_len
);
```
- **When Called**: Whenever a packet arrives on a registered filter channel.
- **Return Values**:
  - `0`: Pass packet unchanged.
  - `1`: Packet modified (using `out_modified_data`).
  - `2`: Drop packet completely.

### `refix_mod_render_ui`
```c
REFIX_MOD_EXPORT void refix_mod_render_ui(void);
```
- **When Called**: Called during the ReFix Overlay render pass if the mod possesses the `overlay.panel` capability.

### `refix_mod_shutdown`
```c
REFIX_MOD_EXPORT void refix_mod_shutdown(void);
```
- **When Called**: During process termination or when the mod is unloaded.

---

## 9. Capabilities Reference

| Capability | Risk Level | Description | Host Calls Guarded |
|---|---|---|---|
| `log` | Low | Write to ReFix log console | `host->log` |
| `config` | Low | Read ReFix and mod-specific INI settings | `host->config_*` |
| `storage` | Low | Isolated persistent key-value store | `host->storage_read`, `host->storage_write` |
| `events` | Low | Receive lifecycle phase notifications | `refix_mod_on_phase` |
| `overlay.panel` | Low | Render tab in ReFix overlay UI | `refix_mod_render_ui` |
| `patch.declarative`| Medium | Register declarative byte patches | `host->register_patch` |
| `hooks.game` | Medium | Register mediated MinHook trampolines | `host->register_hook`, `host->unregister_hook` |
| `net.inspect_channel`| Medium | Read game packet payloads on declared channels | `refix_mod_on_packet` |
| `net.override_channel`| Medium | Modify or drop game packets on declared channels | `refix_mod_on_packet` |
| `memory.write` | **High** | Direct write into game data/heap memory | `host->memory_write` |
| `players.display_name`| **High** | Reveal real persona name of remote peers | Returns real string instead of `Player_xxxx` |
