# ReFix Mods SDK

Welcome to the **ReFix Mods SDK**. ReFix Mods is an in-process plugin framework and runtime that allows game-specific plugins, byte patches, network protocol adapters, and UI widgets to be loaded, isolated, and cryptographically verified within ReFix LAN.

## Key Features

1. **Sandboxed Execution by Default:**
   All community mods run inside an embedded WebAssembly runtime (`wasm3`) with zero direct access to Win32 APIs, the filesystem, or raw sockets.
2. **Zero Network Identity Leak (Privacy Gate):**
   Mods never receive remote IP addresses, ports, MAC addresses, or real SteamID/EOS IDs. Remote players appear exclusively as opaque, double-salted ephemeral `PeerHandle` values.
3. **Declarative Code Patches:**
   Mod code never executes `VirtualProtect` directly. Code modifications are declared in the manifest as `{ module, rva, expected_original_bytes, new_bytes }`. ReFix verifies the module and original bytes before applying, and automatically rolls back changes if a mod faults.
4. **Cryptographic Signatures & TOFU:**
   Every `.rfmod` package is signed using Ed25519. Key pinning (Trust-On-First-Use) and DPAPI-protected trust stores protect users from silent key substitutions or unauthorized capability escalations.

## Quick Start

### 1. Generating Author Keypair
```bash
refixmod keygen --out my_author_key
```

### 2. Linting Manifest
```bash
refixmod lint refixmod.json
```

### 3. Packaging and Signing
```bash
refixmod pack path/to/mod_dir my_mod.rfmod --key my_author_key.priv
```

### 4. Verifying Package
```bash
refixmod verify my_mod.rfmod --key my_author_key.pub
```
