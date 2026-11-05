# ReFix Mods SDK — Developer Tutorial

This tutorial provides step-by-step instructions for authoring, building, packaging, and debugging game-specific plugins using the ReFix Mods SDK.

---

## Tutorial 1: Building a Declarative Byte Patch Mod

### Objective
Disable a hardcoded 30 FPS cap in a native game executable by replacing a conditional jump opcode with NOP instructions (`0x90 0x90`).

### Step 1: Create Project Structure
```
my_fps_patch/
├── refixmod.json
└── main.cpp
```

### Step 2: Define `refixmod.json`
```json
{
  "schema_version": 1,
  "id": "com.community.fpsunlock",
  "name": "60 FPS Uncap Patch",
  "version": "1.0.0",
  "author": "ModderOne",
  "author_pubkey": "REPLACE_WITH_YOUR_HEX_PUBKEY",
  "entrypoint": "patch.wasm",
  "execution_tier": "sandboxed",
  "phase": "GameInit",
  "priority": 0,
  "target_games": [
    {
      "app_id": 480,
      "exe_name": "Game.exe",
      "engine": "native"
    }
  ],
  "dependencies": {
    "before": [],
    "after": [],
    "requires": [],
    "conflicts": []
  },
  "capabilities": [
    "log",
    "patch.declarative"
  ],
  "resource_limits": {
    "max_memory_mb": 4,
    "watchdog_timeout_ms": 10,
    "max_hooks": 0,
    "storage_quota_kb": 64
  }
}
```

### Step 3: Write Mod Logic (`main.cpp`)
```cpp
#include "refix_mod.hpp"

REFIX_MOD_EXPORT int32_t refix_mod_init(const RefixHostApi* host, const char* mod_id) {
    refix::Host::Initialize(host, mod_id);
    refix::Host::Log(REFIX_LOG_INFO, "[FPS Unlock] Mod loaded. Registering patch...");

    // Target RVA 0x4A120: original instructions are 74 08 (JE +8)
    const uint8_t expected[2] = { 0x74, 0x08 };
    const uint8_t replacement[2] = { 0x90, 0x90 }; // NOP, NOP

    RefixDeclarativePatch patch = {};
    patch.struct_size = sizeof(RefixDeclarativePatch);
    patch.api_version = REFIX_MOD_API_VERSION;
    patch.module_name = nullptr; // Target primary game executable
    patch.rva = 0x4A120;
    patch.expected_bytes = expected;
    patch.expected_size = sizeof(expected);
    patch.new_bytes = replacement;
    patch.new_size = sizeof(replacement);

    int32_t res = refix::Host::RegisterPatch(&patch);
    if (res == REFIX_SUCCESS) {
        refix::Host::Log(REFIX_LOG_INFO, "[FPS Unlock] Patch successfully applied!");
    } else {
        refix::Host::Log(REFIX_LOG_ERROR, "[FPS Unlock] Failed to apply patch!");
    }

    return res;
}

REFIX_MOD_EXPORT void refix_mod_shutdown(void) {
    refix::Host::Log(REFIX_LOG_INFO, "[FPS Unlock] Shutdown completed.");
}
```

### Step 4: Compile to WebAssembly
```bash
clang --target=wasm32-unknown-unknown -O3 -nostdlib \
      -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
      -I../../include -I../../bindings/cpp \
      -o patch.wasm main.cpp
```

---

## Tutorial 2: Building an Engine Hook Mod (Mediated MinHook)

### Objective
Intercept a game's damage calculation function to log combat events without using native pointers or modifying code pages directly.

### Step 1: Define `refixmod.json`
```json
{
  "schema_version": 1,
  "id": "com.community.damagelogger",
  "name": "Combat Event Logger",
  "version": "1.0.0",
  "author": "ModderOne",
  "author_pubkey": "REPLACE_WITH_YOUR_HEX_PUBKEY",
  "entrypoint": "hook.wasm",
  "execution_tier": "sandboxed",
  "phase": "EngineReady",
  "priority": 10,
  "target_games": [
    {
      "app_id": 480,
      "exe_name": "GameAssembly.dll",
      "engine": "unity_il2cpp"
    }
  ],
  "dependencies": { "before": [], "after": [], "requires": [], "conflicts": [] },
  "capabilities": [
    "log",
    "hooks.game"
  ],
  "resource_limits": {
    "max_memory_mb": 8,
    "watchdog_timeout_ms": 15,
    "max_hooks": 4,
    "storage_quota_kb": 128
  }
}
```

### Step 2: Implement Mediated Hook (`main.cpp`)
```cpp
#include "refix_mod.hpp"

// WASM callback executed whenever the hooked engine function is called
REFIX_MOD_EXPORT void on_damage_calculated(int32_t attacker_id, int32_t target_id, int32_t amount) {
    char buf[128];
    // Log combat info through safe host abstraction
    refix::Host::Log(REFIX_LOG_INFO, "[Combat] Damage event intercepted via MinHook trampoline.");
}

REFIX_MOD_EXPORT int32_t refix_mod_init(const RefixHostApi* host, const char* mod_id) {
    refix::Host::Initialize(host, mod_id);

    RefixHookDescriptor hook = {};
    hook.struct_size = sizeof(RefixHookDescriptor);
    hook.api_version = REFIX_MOD_API_VERSION;
    hook.module_name = "GameAssembly.dll";
    hook.function_name = nullptr;
    hook.rVA = 0x82C340; // RVA of TakeDamage function
    hook.wasm_callback_index = 1; // Index or ID of on_damage_calculated

    uint32_t hook_id = 0;
    int32_t res = refix::Host::RegisterHook(&hook, &hook_id);
    if (res == REFIX_SUCCESS) {
        refix::Host::Log(REFIX_LOG_INFO, "[Combat] Hook registered safely!");
    }
    return res;
}
```

---

## Tutorial 3: Building a Network Protocol Override Mod

### Objective
Intercept game packets on network channel 0 to filter malformed data or adjust network sync rates without exposing IP addresses.

### Step 1: Manifest Capabilities
Ensure your `refixmod.json` requests network capabilities:
```json
"capabilities": [
  "log",
  "net.inspect_channel",
  "net.override_channel"
]
```

### Step 2: Implement Packet Interceptor
```cpp
#include "refix_mod.hpp"

REFIX_MOD_EXPORT int32_t refix_mod_init(const RefixHostApi* host, const char* mod_id) {
    refix::Host::Initialize(host, mod_id);

    // Register filter on channel 0 for Steam Networking / LanCore
    RefixPacketFilter filter = {};
    filter.struct_size = sizeof(RefixPacketFilter);
    filter.api_version = REFIX_MOD_API_VERSION;
    filter.protocol_type = REFIX_PROTO_STEAM_NETWORKING;
    filter.channel_id = 0;
    filter.flags = 2; // Allow inspection and override

    refix::Host::RegisterPacketFilter(&filter);
    return REFIX_SUCCESS;
}

// Dispatched on packet arrival
REFIX_MOD_EXPORT int32_t refix_mod_on_packet(
    uint32_t protocol_type,
    uint64_t peer_handle,
    uint32_t channel,
    const uint8_t* data,
    size_t len,
    uint8_t* out_modified_data,
    size_t* inout_modified_len
) {
    // Drop malformed packets shorter than 4 bytes
    if (len < 4) {
        return 2; // Drop packet
    }

    // Inspect packet opcode
    uint8_t opcode = data[0];
    if (opcode == 0xFF) {
        // Example: Transform or rewrite payload
        for (size_t i = 0; i < len; ++i) {
            out_modified_data[i] = data[i];
        }
        out_modified_data[1] = 0x01; // Override flag
        *inout_modified_len = len;
        return 1; // Modified
    }

    return 0; // Forward unmodified
}
```

---

## Tutorial 4: Building a Custom Overlay Panel

### Objective
Render a custom in-game telemetry widget using Dear ImGui inside the ReFix in-game overlay.

### Step 1: Manifest Declaration
```json
"capabilities": [
  "log",
  "overlay.panel"
]
```

### Step 2: Implement UI Hook
```cpp
#include "refix_mod.hpp"

REFIX_MOD_EXPORT void refix_mod_render_ui(void) {
    // ReFix renders this inside a dedicated tab in the in-game overlay
    // using sandboxed ImGui immediate-mode primitives
    refix::UI::Text("=== Custom Telemetry Panel ===");
    refix::UI::Separator();
    refix::UI::Text("Packets Processed: %u", 42);
    if (refix::UI::Button("Reset Counter")) {
        refix::Host::Log(REFIX_LOG_INFO, "Counter reset requested from UI.");
    }
}
```

---

## Packaging, Signing & Testing

### Step 1: Generate Keypair
```bash
refixmod keygen --out my_developer_key
```

### Step 2: Validate Manifest
```bash
refixmod lint refixmod.json
```

### Step 3: Pack and Sign
```bash
refixmod pack . dist/my_mod.rfmod --key my_developer_key.priv
```

### Step 4: Verify
```bash
refixmod verify dist/my_mod.rfmod --key my_developer_key.pub
```

### Step 5: Test in Game
1. Copy `my_mod.rfmod` to the `refix_mods/` folder in your game directory.
2. Launch the game through ReFix LAN.
3. Open the ReFix in-game overlay (`Shift + F2` or `Guide` on controller).
4. Navigate to the **Mods** tab to approve and verify your new mod.
