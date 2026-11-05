# ReFix Mods SDK — Complete Developer Guide

Welcome to the **ReFix Mods SDK**. This SDK empowers developers and game preservationists to create high-performance, game-specific plugins—including declarative byte patches, engine-level mediated hooks, protocol overrides, network adapters, and custom overlay interfaces—that ReFix LAN loads, schedules, isolates, and cryptographically verifies.

---

## Table of Contents
1. [Core Architectural Principles](#core-architectural-principles)
2. [Prerequisites & Toolchain Setup](#prerequisites--toolchain-setup)
3. [SDK Directory Layout](#sdk-directory-layout)
4. [Manifest File (`refixmod.json`)](#manifest-file-refixmodjson)
5. [The Developer CLI (`refixmod.exe`)](#the-developer-cli-refixmodexe)
6. [Compilation & Build Workflows](#compilation--build-workflows)
   - [Compiling C / C++ to WebAssembly](#compiling-c--c-to-webassembly)
   - [Compiling Rust to WebAssembly](#compiling-rust-to-webassembly)
7. [Testing & Local Development (`DevMode`)](#testing--local-development-devmode)
8. [Publishing & Distribution](#publishing--distribution)
9. [Documentation Directory Index](#documentation-directory-index)

---

## Core Architectural Principles

ReFix Mods is built from the ground up on strict, non-negotiable security and privacy guarantees:

1. **Sandboxed Execution by Default (Zero-Trust Community Tier):**
   Community mods compile to **WebAssembly (WASM)** and run within an embedded, statically-linked `wasm3` interpreter. Mod modules have **no WASI filesystem**, **no Win32 API access**, and **no raw socket imports**. Host capabilities are mediated and enforced on every single host call.
2. **Strict Player Privacy (Requirement S1):**
   Mods never receive physical network addresses (IPv4, IPv6, ports, MAC addresses), hardware GUIDs, real SteamIDs, or Epic Account IDs. Remote peers appear solely as opaque, ephemeral, double-salted `PeerHandle` values:
   $$\text{PeerHandle} = \text{Truncate64}(\text{HMAC-SHA256}(\text{SessionSalt} \oplus \text{ModSalt}, \text{RealPlayerID}))$$
   This guarantees that two different mods cannot correlate players, nor can a single mod track a player across sessions.
3. **Declarative Code Patches (No Direct `VirtualProtect`):**
   Sandboxed code cannot allocate executable memory or change page protections. Memory code patches are declared statically in the manifest:
   `{ "module": "GameAssembly.dll", "rva": 0x12345, "expected": "85C07405", "replacement": "9090909090" }`.
   ReFix validates that the target module belongs to the game/engine (strictly forbidding system DLLs such as `kernel32.dll` or `ntdll.dll`), verifies that the existing bytes match the expectation, and executes the modification safely.
4. **Mediated MinHook Trampolines:**
   Function hooks are installed by ReFix host logic using an internal MinHook engine. Sandboxed mods provide the hook signature and WASM function index—they **never** supply a native executable pointer.
5. **Cryptographic Sealing & TOFU (Requirement S3):**
   Mod packages (`.rfmod`) are ZIP archives containing a canonical `refixmod.json` manifest signed via **Ed25519**. Every file in the package is SHA-256 hashed. The host maintains a DPAPI-encrypted (`CryptProtectData`) + HMAC-authenticated trust store (`%APPDATA%\ReFix\mods\trust.json`). Community keys are pinned on first approval (Trust-On-First-Use); any unexpected key rotation or capability escalation forces manual re-approval.
6. **Watchdog & Automatic Trap Rollback:**
   Execution time is monitored via high-resolution timers (`QueryPerformanceCounter`). If a mod triggers an instruction trap, divides by zero, exhausts linear memory, or exceeds its execution watchdog budget (default 15 ms), ReFix safely unloads the mod and automatically rolls back all declarative patches and MinHook detours installed by that mod. The host game process never crashes.

---

## Prerequisites & Toolchain Setup

### For C/C++ WebAssembly Development
To compile C/C++ mods to WebAssembly without system dependencies, you can use either LLVM Clang or the WASI-SDK:
- **LLVM Clang (v14+)**:
  ```bash
  clang --version
  # Ensure wasm-ld is in your PATH
  wasm-ld --version
  ```
- **Compilation target:** `wasm32-unknown-unknown` with `-nostdlib`, `-O3`, and `-Wl,--no-entry,--export-all`.

### For Rust WebAssembly Development
- **Rust Toolchain (v1.70+)**:
  ```bash
  rustup target add wasm32-unknown-unknown
  cargo --version
  ```

### For Official Native Plugins (Internal ReFix Team Only)
- **MSVC v143 (Visual Studio 2022 / Build Tools)** with C++17 support.
- Native plugins require signing with the ReFix Offline Master Key. Community native mods are strictly rejected by the loader.

---

## SDK Directory Layout

```
sdk/
├── include/
│   └── refix_mod_api.h        # Stable, versioned C ABI (header-only, zero dependencies)
├── bindings/
│   ├── cpp/
│   │   └── refix_mod.hpp      # Modern C++17 RAII wrapper and ergonomic helper classes
│   └── rust/
│       ├── Cargo.toml         # Rust crate configuration for wasm32-unknown-unknown
│       └── src/
│           └── lib.rs         # Safe Rust abstractions for ReFix Mods host functions
├── examples/
│   ├── example_byte_patch/    # Declarative memory patch on game logic
│   ├── example_engine_hook/   # Mediated MinHook callback on game engine function
│   ├── example_protocol_override/ # Network packet inspector & channel transformer
│   └── example_overlay_panel/ # Custom ImGui UI tab rendered in ReFix Overlay
└── docs/
    ├── README.md              # This guide
    ├── API_REFERENCE.md       # Complete C ABI and function export/import reference
    ├── SECURITY.md            # Threat model, mathematical proofs, and security rules
    ├── TUTORIAL.md            # Step-by-step developer tutorial
    ├── ENGINE_ADAPTERS.md     # Engine-specific hooks and structures (Mono, IL2CPP, Unreal, etc.)
    ├── PROTOCOL_ADAPTERS.md   # Network protocol overrides (Steam, EOS, Photon, LAN)
    └── CLI_REFERENCE.md       # Manpage and reference for tools/refixmod
```

---

## Manifest File (`refixmod.json`)

Every mod must contain a canonical `refixmod.json` file in its root:

```json
{
  "schema_version": 1,
  "id": "com.developer.gamefix",
  "name": "Game Latency & LAN Fix",
  "version": "1.0.0",
  "author": "Developer",
  "author_pubkey": "da910f5ef3b079e9388c7b65ed7ddab2e835ce4ca7545873009c7f38760c8e1b",
  "entrypoint": "plugin.wasm",
  "execution_tier": "sandboxed",
  "phase": "EngineReady",
  "priority": 10,
  "target_games": [
    {
      "app_id": 480,
      "exe_name": "Spacewar.exe",
      "engine": "unity_il2cpp"
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
    "config",
    "storage",
    "patch.declarative",
    "hooks.game",
    "net.inspect_channel"
  ],
  "resource_limits": {
    "max_memory_mb": 16,
    "watchdog_timeout_ms": 15,
    "max_hooks": 8,
    "storage_quota_kb": 1024
  }
}
```

---

## The Developer CLI (`refixmod.exe`)

The ReFix Mod Developer CLI tool is located in `bin/refixmod.exe` (source in `tools/refixmod/main.cpp`). It supports all mod lifecycle tasks:

| Command | Description | Example |
|---|---|---|
| `keygen` | Generates a secure Ed25519 keypair for signing | `refixmod keygen --out mykey` |
| `lint` | Validates manifest syntax, schema version, and capabilities | `refixmod lint path/to/refixmod.json` |
| `pack` | Archives files into `.rfmod` and signs with Ed25519 | `refixmod pack ./src mymod.rfmod --key mykey.priv` |
| `sign` | Signs an existing package or manifest | `refixmod sign mymod.rfmod --key mykey.priv` |
| `verify` | Verifies cryptographic signature and internal SHA-256 hashes | `refixmod verify mymod.rfmod --key mykey.pub` |
| `inspect` | Displays metadata, declared capabilities, targets, and files | `refixmod inspect mymod.rfmod` |

---

## Compilation & Build Workflows

### Compiling C / C++ to WebAssembly

Create your source file (e.g., `main.cpp`):
```cpp
#include "refix_mod.hpp"

REFIX_MOD_EXPORT int32_t refix_mod_init(const RefixHostApi* host, const char* mod_id) {
    refix::Host::Initialize(host, mod_id);
    refix::Host::Log(REFIX_LOG_INFO, "Mod initialized successfully!");
    return REFIX_SUCCESS;
}

REFIX_MOD_EXPORT void refix_mod_on_phase(int32_t phase) {
    if (phase == REFIX_PHASE_ENGINE_READY) {
        refix::Host::Log(REFIX_LOG_INFO, "Engine is ready! Applying patches...");
    }
}
```

Compile directly with Clang:
```bash
clang --target=wasm32-unknown-unknown -O3 -nostdlib \
      -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
      -I../../include -I../../bindings/cpp \
      -o plugin.wasm main.cpp
```

### Compiling Rust to WebAssembly

In `Cargo.toml`:
```toml
[package]
name = "my_rust_mod"
version = "1.0.0"
edition = "2021"

[lib]
crate-type = ["cdylib"]

[dependencies]
refix_mod = { path = "../../bindings/rust" }
```

In `src/lib.rs`:
```rust
use refix_mod::*;

#[no_mangle]
pub extern "C" fn refix_mod_init(host: *const RefixHostApi, mod_id: *const u8) -> i32 {
    unsafe { init_host(host, mod_id) };
    log(LogLevel::Info, "Rust mod online!");
    REFIX_SUCCESS
}
```

Build the release WASM artifact:
```bash
cargo build --target wasm32-unknown-unknown --release
cp target/wasm32-unknown-unknown/release/my_rust_mod.wasm ./plugin.wasm
```

---

## Testing & Local Development (`DevMode`)

During active development, generating and signing `.rfmod` packages on every iteration can slow you down. ReFix provides a designated **Developer Mode**:

1. Enable Developer Mode in `ReFix.ini`:
   ```ini
   [Mods]
   DevMode=1
   ModsDir=refix_mods
   ```
2. When `DevMode=1` is active:
   - ReFix scans subdirectories inside `bin/refix_mods/` directly (e.g., `bin/refix_mods/my_dev_mod/refixmod.json`).
   - Unsigned packages and local directory manifests are accepted for execution.
   - An amber/yellow **"MOD DEV MODE ACTIVE"** warning banner is rendered permanently in the ReFix Overlay to prevent security circumvention in production environments.
   - Capability checks and memory isolation remain strictly active.

---

## Publishing & Distribution

When you are ready to publish your mod to other players:

1. **Verify Manifest**:
   ```bash
   refixmod lint refixmod.json
   ```
2. **Package & Sign**:
   ```bash
   refixmod pack ./my_mod_folder release_v1.0.0.rfmod --key my_author_key.priv
   ```
3. **Inspect Output**:
   ```bash
   refixmod inspect release_v1.0.0.rfmod
   ```
4. **Distribute**: Distribute `release_v1.0.0.rfmod`. End users simply drop the file into their `refix_mods/` folder. When the game starts, the ReFix Overlay will display the mod's author, verified public key fingerprint, and requested capabilities for one-click approval.

---

## Documentation Directory Index

For in-depth documentation on specific topics, refer to:

- [**API Reference**](API_REFERENCE.md): Full C ABI specification, data structures, and function hooks.
- [**Security & Threat Model**](SECURITY.md): Mathematical definition of privacy double-salts, DPAPI storage, and ROP mitigations.
- [**Developer Tutorial**](TUTORIAL.md): Step-by-step walkthroughs for writing byte patches, hooks, protocol overrides, and UI panels.
- [**Engine Adapters**](ENGINE_ADAPTERS.md): Deep architectural details for Unity Mono/IL2CPP, Unreal Engine 4/5, Godot, Source, and Native engines.
- [**Protocol Adapters**](PROTOCOL_ADAPTERS.md): Network layer interception across Steam, EOS, Photon, and LanCore.
- [**CLI Reference**](CLI_REFERENCE.md): Complete command-line syntax and option reference for `refixmod.exe`.
