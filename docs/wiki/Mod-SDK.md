# ReFix Mod SDK

A ReFix mod is a signed package (`.rfmod`) that ReFix loads into one game. Use a mod for game-specific
work that does not belong in ReFix itself: emulating a game's own backend, adding an overlay panel,
patching a few bytes, changing how a game talks on the network.

This page covers the whole path, using the CS:GO 1.37 mod that ships with ReFix 2.2.0 as the running
example:

1. [How ReFix loads mods](#1-how-refix-loads-mods)
2. [The package and its manifest](#2-the-package-and-its-manifest)
3. [Capabilities and trust](#3-capabilities-and-trust)
4. [Native mods (C++)](#4-native-mods-c)
5. [Sandboxed mods (WebAssembly)](#5-sandboxed-mods-webassembly)
6. [Building, packing and signing](#6-building-packing-and-signing)
7. [Testing a mod locally](#7-testing-a-mod-locally)
8. [Publishing: the mod catalog and AutoDeploy](#8-publishing-the-mod-catalog-and-autodeploy)
9. [Walkthrough: the CS:GO mod](#9-walkthrough-the-csgo-mod)
10. [Reference](#10-reference)

---

## 1. How ReFix loads mods

When the game starts, ReFix (the `steam_api.dll` / `steam_api64.dll` it puts in the game) reads the
`[Mods]` section of `ReFix.ini`:

```ini
[Mods]
Enabled = true          ; load mods at all
DevMode = false         ; true: load unsigned or untrusted packages (development only)
; ModsDirectory =       ; default: refix_mods next to the game's .exe
```

It then opens every `*.rfmod` in `<game exe folder>\refix_mods\`, and for each package:

1. Reads `refixmod.json` and validates it (schema, id, capabilities, file paths).
2. Checks the Ed25519 signature and the SHA-256 of every file listed in the manifest.
3. Decides the trust level (see [section 3](#3-capabilities-and-trust)) and whether the mod may run
   without asking the player.
4. Extracts the package to `%TEMP%\refix\mods\<id>\` (hashes checked again on extraction).
5. Starts it: a native DLL is loaded and `refix_mod_init` is called; a WebAssembly module is loaded
   into the wasm3 sandbox.
6. Registers it with the scheduler, which orders mods by phase, `priority` and `dependencies`.

At startup ReFix runs the lifecycle phases `Boot`, `PreRefix`, `PostRefix` and `EngineReady`, in that
order. Each active mod gets every phase: a native mod through `refix_mod_on_event(phase, nullptr)`, a
sandboxed mod through its exported `refix_on_phase()`. The phase enum also names later phases
(`NetworkReady`, `PreFrame`, `LobbyJoined`, `MatchStart`, `Shutdown`...); ReFix 2.2.0 does not fire
those yet, so do not rely on them.

If a mod faults (a trap in the sandbox, a watchdog timeout), ReFix unloads it and rolls back the
patches and hooks it installed. The game keeps running.

## 2. The package and its manifest

An `.rfmod` is a ZIP archive with the manifest at its root:

```
refix.csgo.lan_overhaul.rfmod
├── refixmod.json      manifest (signed)
└── refix_csgo.dll     entrypoint
```

The manifest of the CS:GO mod:

```json
{
  "schema_version": 1,
  "id": "refix.csgo.lan_overhaul",
  "name": "CS:GO LAN & Economy Overhaul",
  "version": "1.0.0",
  "author": "ReFix Team",
  "author_pubkey": "00306210b98e23fd0699179bb38ab8d1ed58498e5569abc95652cd53caaec9c9",
  "entrypoint": "refix_csgo.dll",
  "execution_tier": "native",
  "phase": "EngineReady",
  "priority": 100,
  "target_games": [
    { "app_id": 730, "exe_name": "csgo.exe", "engine": "Source" }
  ],
  "capabilities": [
    "log", "config", "storage", "events", "overlay.panel", "memory.write", "hooks.game"
  ],
  "resource_limits": {
    "max_memory_mb": 64,
    "watchdog_timeout_ms": 50,
    "max_hooks": 64,
    "storage_quota_kb": 8192
  }
}
```

| Field | Required | Meaning |
|---|---|---|
| `schema_version` | yes | Always `1`. |
| `id` | yes | Unique id. Letters, digits, `.`, `_` and `-` only, no `..`. Reverse-domain style (`author.game.name`) keeps it unique. |
| `name`, `version`, `author` | no | Shown to the player in the overlay. |
| `author_pubkey` | for signed mods | The author's Ed25519 public key, 64 hex characters. `refixmod pack --key` fills it in. |
| `developer_cert` | no | ReFix's signature over `author_pubkey`: makes the mod *Verified Community*. |
| `entrypoint` | yes | File the mod starts from: a `.dll` (native) or a `.wasm` (sandboxed). |
| `execution_tier` | no | `native` or `sandboxed` (default). |
| `phase` | no | Phase the mod is scheduled in (`Boot`, `PreRefix`, `PostRefix`, `EngineReady`). |
| `priority` | no | Order inside a phase. |
| `target_games` | no | Games the mod is for (`app_id`, `exe_name`, `engine`). Informational in 2.2.0: the loader does not filter on it. The [catalog](#8-publishing-the-mod-catalog-and-autodeploy) decides which game gets the mod. |
| `dependencies` | no | `{ "before": [], "after": [], "requires": [], "conflicts": [] }`, each a list of mod ids. |
| `capabilities` | no | What the mod may do (next section). |
| `resource_limits` | no | Sandbox limits: `max_memory_mb` (16), `watchdog_timeout_ms` (15), `max_hooks` (16), `storage_quota_kb` (1024). Defaults in brackets. |
| `files` | written by `pack` | SHA-256 of every file in the package. Do not write it by hand. |

## 3. Capabilities and trust

### Capabilities

A mod declares what it needs; anything else is refused. Valid values:

| Capability | Allows | Risk |
|---|---|---|
| `log` | Writing to `ReFix.log`. | |
| `config` | Reading its own settings. | |
| `storage` | A private key/value store, limited by `storage_quota_kb`. | |
| `events` | Receiving lifecycle and game events. | |
| `overlay.panel` | Adding panels and cards to the ReFix overlay. | |
| `patch.declarative` | Byte patches checked against the expected original bytes. | |
| `hooks.game` | Function hooks in the game's own modules (never system DLLs). | |
| `memory.write` | Writing game memory directly. | high |
| `net.inspect_channel` | Reading the game's traffic on a channel. | |
| `net.override_channel` | Replacing that traffic. | |
| `players.display_name` | Reading other players' display names. | high |

An unknown capability makes the manifest invalid.

### Trust levels

ReFix works out a trust level from the signature and the author key:

| Level | When | What happens |
|---|---|---|
| Official | Signed with the ReFix release key. | Runs without asking. |
| Verified Community | Author key certified by ReFix (`developer_cert`). | Approved once by the player, like Unverified Community; ReFix can certify keys so they can be told apart. |
| Unverified Community | Self-signed author key. | The player approves it once in the overlay; the key is remembered (`%APPDATA%\ReFix\mods\trust.json`). A different key or new capabilities ask again. |
| Untrusted | Unsigned, or the signature does not match. | Never runs, unless `DevMode = true`. |
| Revoked | Key or package on the revocation list. | Never runs. |

**Native DLLs only run when the package is signed with the ReFix release key.** Community mods use the
sandboxed (WebAssembly) tier. The CS:GO mod is native because it is part of ReFix itself.

## 4. Native mods (C++)

A native mod is a DLL built for the game's architecture (CS:GO 1.37 is 32-bit, so `refix_csgo.dll` is
x86). It exports three functions:

```cpp
extern "C" {
__declspec(dllexport) int  refix_mod_init(const void* host);      // 0 = loaded, anything else = refused
__declspec(dllexport) void refix_mod_shutdown();
__declspec(dllexport) void refix_mod_on_event(int event_id, const void* event_data);
}
```

`host` points to ReFix's services table, `refix::IRefixServices`, declared in
[`src/include/refix_services.h`](../../src/include/refix_services.h). Check `version` before using it:

| Member | What it gives the mod |
|---|---|
| `Log(fmt, ...)` | Writes to `ReFix.log`. |
| `gc` (`IGcCore`) | Valve Game Coordinator: `RegisterClientHandler` / `RegisterServerHandler` take a GC message id, `SendClient` / `SendServer` answer, `SendSingleObject` / `SendSOCacheSubscribed` push SO cache objects, `GetActiveSteamID`, `GetActiveAppID`, LAN relay helpers. |
| `econ` (`IEconSchema`) | The game's item schema: item definitions, categories, loot rolls. |
| `inventory` (`IInventoryStore`) | The player's persisted inventory: items, grants, equipped slots, new item ids. |
| `engine` (`ISourceEngine`) | `EnqueueCommand` runs a console command on the game's main thread; `FindInterface` returns an engine interface. |
| `panorama` (`IPanorama`) | Inject script into Panorama files, run script. |
| `overlay` (`IOverlayActions`) | Register overlay cards and modals, open and close the overlay. |
| `entitlements` (`IEntitlements`) | Game entitlements (Prime, passes, DLC). |
| `imgui_context`, `imgui_alloc`, `imgui_free` | The overlay's ImGui context and allocator. A DLL has its own ImGui copy and CRT heap: call `ImGui::SetCurrentContext` and `ImGui::SetAllocatorFunctions` with these before drawing. |

> `sdk/include/refix_mod_api.h` describes a plain C table (`RefixHostApi`) meant for future
> third-party native mods. The loader of 2.2.0 passes `IRefixServices`, as above.

A GC handler registered through `gc` runs before ReFix's built-in GC emulator, so a mod can take over
any message it registers and leave the rest to ReFix. The CS:GO entrypoint, trimmed:

```cpp
// src/games/csgo/csgo_mod.cpp
static refix::IRefixServices* g_services = nullptr;

extern "C" __declspec(dllexport) int refix_mod_init(const void* host) {
    if (!host) return -1;
    g_services = const_cast<refix::IRefixServices*>(static_cast<const refix::IRefixServices*>(host));
    if (g_services->version < 1) return -2;
    refix::csgo::RegisterCSGOHandlers(g_services);
    return 0;
}
```

A handler looks like this (message 9165 is how CS:GO asks for a new operation mission):

```cpp
services->gc->RegisterClientHandler(9165, [](const uint8_t* body, size_t size, bool isProto) {
    // parse the protobuf body, update state, then answer:
    // services->gc->SendClient(9165, reply.data(), reply.size(), true);
});
```

## 5. Sandboxed mods (WebAssembly)

Community mods compile to WebAssembly (`wasm32-unknown-unknown`, no WASI) and run in ReFix's wasm3
interpreter. The module has no file system, no Windows API and no sockets. In 2.2.0 it can import
these functions from the `env` module:

| Import | Signature | Does |
|---|---|---|
| `refix_log` | `void (i32 level, i32 msg_ptr, i32 msg_len)` | Logs a UTF-8 string from linear memory. Levels: 0 debug, 1 info, 2 warn, 3 error. |
| `refix_get_time_ms` | `i64 ()` | Milliseconds since the PC started. |
| `refix_storage_read` | `i32 (i32 key_ptr, i32 key_len, i32 out_ptr, i32 max_len)` | Reads a value from the mod's private storage; returns its length, or a negative error. |
| `refix_storage_write` | `i32 (i32 key_ptr, i32 key_len, i32 val_ptr, i32 val_len)` | Writes a value; returns -2 past `storage_quota_kb`. |

Calling an import the manifest has no capability for (for example `refix_storage_write` without `storage`) traps, and ReFix unloads the mod.

It exports `refix_on_phase()`, called once per lifecycle phase. A minimal mod in C:

```c
// clang --target=wasm32 -O2 -nostdlib -Wl,--no-entry -Wl,--export=refix_on_phase -o main.wasm main.c
__attribute__((import_module("env"), import_name("refix_log")))
void refix_log(int level, const char* msg, int len);

static int phases = 0;

__attribute__((export_name("refix_on_phase")))
void refix_on_phase(void) {
    static const char msg[] = "hello from a sandboxed mod";
    if (phases++ == 0) refix_log(1, msg, sizeof(msg) - 1);
}
```

Its manifest needs `"execution_tier": "sandboxed"`, `"entrypoint": "main.wasm"` and
`"capabilities": ["log"]`. More examples are in [`sdk/examples`](../../sdk/examples). Some of those
use host functions that 2.2.0 does not provide yet (patches, hooks, overlay tabs, networking); the
table above is the current list.

## 6. Building, packing and signing

`refixmod.exe` (in `bin\`, source in [`tools/refixmod`](../../tools/refixmod)) does packaging and
signing:

| Command | Does |
|---|---|
| `refixmod keygen [--out <name>]` | Makes an Ed25519 key pair: `<name>.pub` (share it) and `<name>.priv` (keep it secret, never commit it). |
| `refixmod lint <refixmod.json>` | Validates a manifest. |
| `refixmod pack <mod_dir> <out.rfmod> [--key <priv>]` | Hashes every file of the folder into `files`, writes `author_pubkey`, signs, zips. |
| `refixmod sign <mod.rfmod> --key <priv>` | Signs an existing package again. |
| `refixmod verify <mod.rfmod> [--key <pub>]` | Checks signature, hashes and structure. |
| `refixmod inspect <mod.rfmod>` | Prints manifest, files, risk rating and signature state. |

The CS:GO mod is built and packed like this:

```bat
build_csgo_mod.bat
mkdir build\package_csgo_mod
copy build_csgo_mod\refix_csgo.dll build\package_csgo_mod\
copy <manifest> build\package_csgo_mod\refixmod.json
bin\refixmod.exe pack build\package_csgo_mod refix_mods\refix.csgo.lan_overhaul.rfmod --key csgo_author.priv
bin\refixmod.exe verify refix_mods\refix.csgo.lan_overhaul.rfmod
```

`build_csgo_mod.bat` compiles `src/games/csgo/*.cpp` into a 32-bit `refix_csgo.dll`
(`cl ... /LD /MACHINE:X86`). A native mod links nothing from ReFix: it only uses what `host` gives it.

## 7. Testing a mod locally

1. Install ReFix in the game with AutoDeploy.
2. Copy your `.rfmod` into `<game exe folder>\refix_mods\`.
3. For an unsigned or self-signed build, set `DevMode = true` under `[Mods]` in the game's `ReFix.ini`.
   Turn it off again when you are done: with DevMode on, any package in that folder runs.
4. Set `EnableLog = true` under `[Debug]` and start the game. `ReFix.log` (next to `ReFix.ini`) shows
   `[ModHost]` lines for every package: loaded, waiting for approval, or why it was refused.

The `test_mod_*` programs that `build.bat` produces check manifest parsing, packages, signatures, the
trust store, the scheduler and the sandbox; run them after changing the loader.

## 8. Publishing: the mod catalog and AutoDeploy

Players do not copy mods by hand. [`refix_mods/catalog.tsv`](../../refix_mods/catalog.tsv) in this
repository lists every mod and the game it is for; AutoDeploy (Windows and Linux) reads it after
installing ReFix:

```
# id	product	exe	match_file	match_text	file	sha256
refix.csgo.lan_overhaul	lan	csgo.exe	csgo/steam.inf	ProductName=csgo	refix.csgo.lan_overhaul.rfmod	9f29a187...
```

| Column | Meaning |
|---|---|
| `id` | The mod id. |
| `product` | `lan`, `online` or `any`: which ReFix product the mod works with. |
| `exe` | The game's executable. AutoDeploy looks for it in the game folder (three levels deep) and installs the mod next to it, in `refix_mods\`. |
| `match_file`, `match_text` | Optional version check: the row only matches when this file (relative to the exe) contains the text. Put one row per game version when versions need different builds of the mod, newest first. |
| `file` | The package, in `refix_mods/` of this repository. |
| `sha256` | SHA-256 of the package. A copy that does not match is not installed. |

AutoDeploy takes the first matching row. The package comes from the `refix_mods` folder of the release
zip when it is there, otherwise it is downloaded from
`https://raw.githubusercontent.com/Coronitaa/ReFix/main/refix_mods/` (set `REFIX_MODS_URL` to use
another place). What it installs is listed in `refix_mods\.refix_installed`; the uninstallers remove
exactly those files, and restore every game file a mod changed and backed up as `<file>.refix_orig`.

To publish a mod:

1. Add the package to `refix_mods/`.
2. Add its row to `catalog.tsv` with `sha256sum` (Linux) or `Get-FileHash` (Windows) of the package.
3. Open a pull request. Native mods are signed by the ReFix team; sandboxed mods can be signed with
   your own key.

If your mod changes game files, always keep the original first as `<file>.refix_orig`, and write
through a temporary file so a crash never leaves a half-written file. That is what lets the
uninstaller undo it.

## 9. Walkthrough: the CS:GO mod

The CS:GO 1.37 support is split in two:

- **The mod package** (`refix.csgo.lan_overhaul.rfmod`): manifest and `refix_csgo.dll`. It is the
  part AutoDeploy installs for CS:GO, and the place where CS:GO-only GC handlers go
  (`RegisterCSGOHandlers` in `src/games/csgo/csgo_handlers.cpp`).
- **CS:GO support inside ReFix**, which the mod builds on. ReFix 2.2.0 still carries most of it, because
  it predates the SDK:

| Area | Files | What it does |
|---|---|---|
| Game Coordinator | `src/steam_gc_emu.cpp` | Answers CS:GO's GC: welcome and SO cache, inventory, cases, stickers, music kits, service medals, matchmaking on the LAN, operation passes and missions. |
| Item schema | `src/csgo_schema.cpp` | Reads `items_game.txt` and the language files: names, rarities, loot lists, sticker/graffiti/music kits, mission cards and quest goals. |
| Operation missions | `src/csgo_operation_patch.cpp`, `tools/gen_csgo_operation_patch.py` | Adds the Shattered Web weeks that the 2019 build ships locked to `items_game.txt` and the language files, keeping `.refix_orig` backups. |
| Overlay | `src/overlay/csgo/` | The CS:GO LAN card, Market & Skins and the LAN server dialog. |
| Rooms and relay | `src/gc/lan_rooms.cpp`, `src/gc/gc_relay.cpp` | LAN rooms, finding open matches, carrying each player's inventory and quest points to the host. |

Moving these parts into the mod, one GC message at a time, is the way forward: register the message in
`RegisterCSGOHandlers` and the mod's handler wins over the built-in one.

Some details of the CS:GO GC worth knowing if you write a mod for another Valve GC game:

- **SO cache.** The client keeps typed objects (items = type 1, operation progress = 40, active mission
  card = 41, quest progress = 46). `CMsgSOSingleObject` (create 21, update 22, destroy 23) changes
  one; the welcome message carries them all. Send the same object types to the game server in its cache
  (`BuildEquippedCacheSubscribed`) when the server needs them; CS:GO's server reads type 41 to know each
  player's mission.
- **The client's clock comes from the GC.** CS:GO unlocks operation weeks from the time in the welcome
  message (`CMsgClientWelcome` field 7) and the matchmaking hello. Leave it out and every week stays
  hidden.
- **Look for answers in the game's own files.** The proto descriptors embedded in `client_panorama.dll`
  and `server.dll` give message ids and field numbers (for example, `MatchEndRunRewardDrops` is 9136 with
  the quest points in field 4).

## 10. Reference

- Services table: [`src/include/refix_services.h`](../../src/include/refix_services.h)
- Loader: [`src/mods/mod_host.cpp`](../../src/mods/mod_host.cpp), manifest:
  [`src/mods/mod_manifest.cpp`](../../src/mods/mod_manifest.cpp), capabilities and trust levels:
  [`src/mods/mod_types.h`](../../src/mods/mod_types.h)
- Sandbox: [`src/mods/sandbox_wasm.cpp`](../../src/mods/sandbox_wasm.cpp)
- CLI: [`tools/refixmod/main.cpp`](../../tools/refixmod/main.cpp)
- CS:GO mod: [`src/games/csgo`](../../src/games/csgo), [`build_csgo_mod.bat`](../../build_csgo_mod.bat)
- Catalog: [`refix_mods/catalog.tsv`](../../refix_mods/catalog.tsv)
