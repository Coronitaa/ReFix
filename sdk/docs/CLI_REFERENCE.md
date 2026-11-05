# ReFix Mod CLI (`refixmod`) Reference Manual

`refixmod.exe` is the official developer command-line tool for authoring, linting, packaging, signing, and inspecting ReFix mod packages.

---

## 1. Installation & Location

`refixmod.exe` is distributed in the `bin/` directory of ReFix LAN:
```bash
bin/refixmod.exe --help
```
Source code is maintained in `tools/refixmod/main.cpp`.

---

## 2. Command Overview

```
refixmod <command> [options]

Commands:
  keygen    Generate a new Ed25519 signing keypair
  lint      Validate syntax and security constraints of a refixmod.json manifest
  pack      Archive directory files into an .rfmod package and sign it
  sign      Digitally sign an existing package or manifest
  verify    Verify the cryptographic signature and file hashes of a package
  inspect   Display detailed metadata, targets, capabilities, and file contents
```

---

## 3. Command Reference

### 3.1 `keygen`
Generates a cryptographically secure 256-bit Ed25519 keypair for signing mods.

```bash
refixmod keygen [--out <path_prefix>]
```

#### Options:
- `--out <path_prefix>` *(Optional)*: Base filename for output keys. Defaults to `author_key`. Produces `<path_prefix>.pub` (hex public key) and `<path_prefix>.priv` (hex secret key).

#### Example:
```bash
refixmod keygen --out my_developer_key
```
Output:
```
[SUCCESS] Generated Ed25519 keypair:
  Public key (my_developer_key.pub): da910f5ef3b079e9388c7b65ed7ddab2e835ce4ca7545873009c7f38760c8e1b
  Private key (my_developer_key.priv): [SAVED TO FILE]
```

---

### 3.2 `lint`
Analyzes a `refixmod.json` manifest against the schema specification and security policies.

```bash
refixmod lint <path/to/refixmod.json>
```

#### Checks Performed:
- Schema version compatibility (`schema_version == 1`).
- Valid mod ID format (`com.author.modname`).
- Capability names against recognized whitelist.
- High-risk capabilities flagged (`memory.write`, `players.display_name`).
- Resource limit sanity bounds.
- Path traversal / Zip-Slip patterns in file paths.

#### Example:
```bash
refixmod lint ./my_mod/refixmod.json
```
Output:
```
[SUCCESS] Manifest is valid.
  Mod ID: com.community.fpsunlock (v1.0.0)
  Tier:   Sandboxed (WASM)
  Author: ModderOne
  Target games: 1
  Capabilities: 2
    - log
    - patch.declarative
```

---

### 3.3 `pack`
Gathers files from a source directory, generates a manifest hash ledger, packs into an `.rfmod` archive, and signs with an author private key.

```bash
refixmod pack <source_dir> <output.rfmod> [--key <private_key_file>]
```

#### Options:
- `<source_dir>`: Directory containing `refixmod.json` and all mod assets/binaries.
- `<output.rfmod>`: Path to the target archive file.
- `--key <path>` *(Recommended)*: Path to the 64-character hex private key file. If omitted, package is left unsigned (can only run in `DevMode=1`).

#### Example:
```bash
refixmod pack ./src_dir dist/game_fix.rfmod --key my_key.priv
```
Output:
```
[SUCCESS] Packed 2 file(s) into dist/game_fix.rfmod
  Package digitally signed with author key.
```

---

### 3.4 `verify`
Cryptographically verifies an `.rfmod` package against an author public key and checks every contained file against its canonical SHA-256 hash.

```bash
refixmod verify <package.rfmod> [--key <public_key_file>]
```

#### Options:
- `<package.rfmod>`: Path to the `.rfmod` archive.
- `--key <path>` *(Optional)*: Specific public key to verify against. If omitted, uses the `author_pubkey` declared inside the manifest.

#### Exit Codes:
- `0`: Signature authentic; all file hashes match.
- `2`: Verification failed (tampered file, corrupt signature, or revoked key).

---

### 3.5 `inspect`
Dumps all human-readable metadata, target games, declared capabilities, risk assessments, and file manifests.

```bash
refixmod inspect <package.rfmod>
```

#### Example:
```bash
refixmod inspect dist/game_fix.rfmod
```
Output:
```
=== ReFix Mod Inspection ===
Package:      dist/game_fix.rfmod
Mod ID:       com.community.fpsunlock
Name:         60 FPS Uncap Patch
Version:      1.0.0
Author:       ModderOne
Pubkey:       da910f5ef3b079e9388c7b65ed7ddab2e835ce4ca7545873009c7f38760c8e1b
Execution:    Sandboxed (WASM)
Phase:        GameInit
Priority:     0
Signature:    VALID
Overall Valid:YES

Target Games:
  - AppID=480, Exe=Game.exe, Engine=native

Declared Capabilities (2):
  [*] log
  [*] patch.declarative

Files in Package (1):
  patch.wasm                     SHA256: 4e91b3fa...
```

---

## 4. Exit Code Reference

| Exit Code | Meaning |
|---|---|
| `0` | Success |
| `1` | Invalid command line arguments or file not found |
| `2` | Cryptographic signature or hash verification failure |
| `3` | Manifest syntax or schema validation error |
| `4` | I/O or archive packing error |
