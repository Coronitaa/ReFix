# Overlay panels for mods

A mod adds UI to the ReFix overlay through a small C ABI:
[`sdk/include/refix_overlay_api.h`](../../sdk/include/refix_overlay_api.h). Plain C structs, so any
compiler works, and every struct starts with `struct_size` and `abi_version`.

## What a mod can add

| Kind | Where it shows | Use it for |
|---|---|---|
| Panel, `REFIX_PANEL_SLOT_GRID` | A cell of the mod grid on the overlay home (left column). Cells are at least 300 px wide; columns reflow with the window, rows take the height of their tallest cell, and the grid scrolls when there is no room. | Status, shortcuts, small lists. |
| Panel, `REFIX_PANEL_SLOT_HUD` | Over the game, while the mod keeps it open with `open_panel` / `close_panel`. The panel draws its own ImGui window. | Full dialogs (CS:GO's Market & Skins and LAN server use this). |
| Action | Not drawn. `on_request` sees the game's `ActivateGameOverlay(dialog)` and `ActivateGameOverlayToWebPage(url)` calls first and returns 1 when it handled one. | Opening a HUD from a button inside the game. |

## Only in your game

The mod's signed `refixmod.json` lists `target_games`. ReFix loads the package, and so registers its
panels and actions, only when the running game matches: each entry needs an `app_id` or an `exe_name`;
with both, both must match. A mod for one game never shows up in another, and neither do its request
handlers (that is how Barotrauma's "Discover mods" link once opened CS:GO's market: the handler was
global). The package also needs the `overlay.panel` capability.

## Steps

1. Write `refixmod.json` with your game in `target_games` and `overlay.panel` in `capabilities`.
2. Export `refix_mod_register_overlay(const RefixOverlayHost* host)` from your DLL. ReFix calls it once,
   after `refix_mod_init`. Copy `host->token` and the function pointers you need; the table itself is
   only valid during the call.
3. Register up to 8 panels and 8 actions. Ids: 1 to 63 characters of `a-z 0-9 . _ -`. Titles: up to 127
   bytes of UTF-8. ReFix copies the strings.
4. In `draw`, set ReFix's ImGui context and allocator from `RefixPanelDrawArgs` before drawing (your DLL
   has its own ImGui copy). Use Dear ImGui 1.91.x. `args` is valid only during the call.
5. Pack and sign with `refixmod` (see [Mod-SDK](Mod-SDK.md), sections 6 and 7). Native DLLs only load
   when the package is signed with the ReFix release key (`src/mods/native_host.cpp`), so a community
   overlay mod goes through the ReFix team for signing.

A complete example: [`sdk/examples/example_overlay_native`](../../sdk/examples/example_overlay_native).

## What ReFix guards

- A struct that is too small, has another major version, a bad id or an over-long string is rejected
  (`REFIX_E_INVALID`); too many panels or a taken id gives `REFIX_E_LIMIT`.
- A mod can only open, close or register with its own token, and only its own HUD panels.
- Every call into the mod runs behind a crash guard. A crash, a C++ exception, a call that takes more
  than 5 s, or a `draw` that leaves ImGui's stacks unbalanced turns the mod off: ReFix repairs the
  ImGui state, logs the reason in `ReFix.log` (`[OverlayMods] <id> turned off: ...`) and shows "This mod
  stopped working" in its cell. The game keeps running.
- Before the DLL is unloaded, ReFix removes everything it registered and waits for its running calls.
- Not guarded: a callback that never returns. Keep `draw` short and do slow work on your own thread.

## Builtin modules

UI that ships inside ReFix for one game uses the same ABI. `src/overlay/game_actions/builtin_modules.cpp`
lists each module with its target games; the CS:GO module lives in
[`src/overlay/csgo/csgo_overlay_module.cpp`](../../src/overlay/csgo/csgo_overlay_module.cpp).
