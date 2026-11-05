# ReFix Mods Security Architecture & Threat Model

## 1. Non-Negotiable Privacy Requirements (S1)

- **Double-Salted Ephemeral Peer Handles:**
  $$\text{PeerHandle} = \text{Truncate64}(\text{HMAC-SHA256}(\text{SessionSalt} \oplus \text{ModSalt}, \text{RealID}))$$
  - Every game session generates a new 32-byte cryptographic salt.
  - Every mod ID generates a unique salt.
  - Result: Two different mods running in the same match cannot correlate a remote player. The same mod running in two different sessions cannot track a remote player.
- **Physical Address Isolation:**
  Physical `sockaddr`, IPv4/IPv6 addresses, ports, and MAC addresses remain strictly encapsulated within the low-level transport (`lan_core`). Mods only interact with `(PeerHandle, Channel)` tuples.
- **Display Name Redaction:**
  Unless the mod explicitly declares `players.display_name` and the user explicitly approves it, display names are masked as anonymous aliases (`Player_xxxx`).

## 2. System Isolation & 2-Tier Execution (S2)

| Execution Tier | Technology | Permitted Authors | Security Isolation |
|---|---|---|---|
| **Sandboxed (Default)** | WebAssembly (wasm3) | Entire Community | Real isolation: No WASI, no Win32 APIs, no raw filesystem or socket imports. Host functions filtered strictly by declared/approved capabilities. |
| **Native (Restricted)** | Win32 PE DLL | ReFix Official Only | Verified cryptographically against ReFix Root Key. Community mods are NEVER allowed to run native DLLs. |

### Memory & Patching Defenses
1. **Forbidden System Modules:** Declarative patches and hooks targeting system libraries (`kernel32`, `ntdll`, `user32`, `ws2_32`, etc.) or ReFix internal modules (`winmm`, `refix_lan_core`, `steam_api`, etc.) are strictly rejected.
2. **PE Structure Protection:** Writes targeting IAT, EAT, or PE headers are blocked.
3. **ROP Pointer Mitigation:** Memory write payloads are inspected to reject any values containing pointers into executable sections of system DLLs.
4. **Watchdog & Resource Quotas:** Linear memory is capped (default 16MB), call execution time is monitored (default 15ms watchdog), and storage is quota-bounded.
5. **Trap Rollback:** Any WASM trap or watchdog timeout immediately rolls back all declarative patches and MinHook hooks installed by that mod, disabling it without crashing the game.

## 3. Cryptographic Verification & Approval (S3)

- **Ed25519 Signatures:** Canonical manifest signed with Ed25519; file hashes computed using SHA-256.
- **Trust-On-First-Use (TOFU):** Self-signed community author keys are pinned on first approval. If an update is signed by a different key or requests new capabilities, manual re-approval is mandatory.
- **DPAPI + HMAC Trust Store:** `%APPDATA%\ReFix\mods\trust.json` is protected with Windows DPAPI (`CryptProtectData`) and SHA-256 integrity checksums to prevent offline tampering.
