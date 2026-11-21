# Release v2.6.0

## Summary of Changes Since v2.5.5

ReFix v2.6.0 introduces major enhancements across multiplayer networking, modular architecture, overlay customization, and cloud profile synchronization.

---

### 1. LAN Connection & Networking Overhaul
- **Fixed GC 30-Retries Connection Failure:** Resolved an issue where guest clients could not connect to the host's LAN match due to loopback IP address rewrites in `gc_relay.cpp`. Guests now correctly receive the host's actual LAN socket address and connect seamlessly.
- **Eliminated Network Jitter:** Optimized packet timing and polling to eliminate lag and jitter during LAN multiplayer sessions.

### 2. CS:GO Mod Architecture Isolation
- **Generic Engine Decoupling:** Isolated CS:GO-specific market hooks, schemas, inventory overrides, and overlay modules into the `refix.csgo.lan_overhaul.rfmod` package.
- **Engine Reusability:** The core ReFix runtime remains generic and clean, providing universal Source engine and Steamworks compatibility for all supported games.

### 3. Overlay Settings & UI Refinements
- **Compact Keyboard Shortcut Dropdown:** Compressed the overlay shortcut selection into a sleek combo box supporting `Shift + Tab` (default), `Ctrl + Tab`, `Shift + F1`, `Shift + F2`, `F11`, `F12`, `Home`, and `Insert`.
- **In-Game Overlay Behavior Dropdown:** Compressed the behavior when the overlay is open into a styled combo box:
  - *Keep game active:* Game simulation and audio continue running normally.
  - *Mute game audio:* Background audio is muted via WASAPI while the overlay is open.
  - *Pause game:* Pauses singleplayer games and mutes audio.
- **Overlay Management Action Buttons:** Left-aligned buttons with distinct visual accents:
  - *Replace with Steam Overlay:* Replaces ReFix overlay with official Steam overlay.
  - *Disable overlay completely:* Disables both ReFix and Steam overlays for this game.

### 4. Dual-Stage Safety Modals & Decremental Hold-to-Confirm
- **Aesthetic Warning Modals:** Redesigned cautionary modal dialogs with compact margins, subtle warning borders, and bold keywords for clear visual hierarchy.
- **Decremental Click-and-Hold Button:** Final confirmation requires clicking and holding the confirmation button for 5 seconds:
  - Rapidly discharges if the mouse or button is released early.
  - Features a non-linear, decremental fill progress animation ($p = 1 - (1 - u)^2$) that fills quickly at first and slows down towards 5 seconds.
  - Explicitly notifies the player that the game will restart upon confirmation, and restarts the process automatically once confirmed.

### 5. Cloud Sync Profile Detection Modal
- **Automatic Cloud Profile Discovery:** When connecting to Google Drive or other cloud destinations, ReFix detects profiles in the cloud that are not yet installed locally.
- **Interactive Grid Selector:** Displays cloud profiles in a 3-column card grid (mirroring the in-game profile selector), with all profiles selected by default.
- **Game Presence & Sorting:**
  - Profiles with data for the active game are prioritized at the top of the list.
  - Displays the creation game and multi-game presence (e.g., `Lethal Company +5 more` / `Lethal Company +5 más`).
- **Game Categorization Integrity:** After import, profiles belonging to the current game appear in the main profile menu, while profiles created for other games are preserved under "Profiles from other games".

### 6. 3D Skin Preview Texture Compositing
- **Shaded Weapon Silhouettes:** Implemented multi-pass texture compositing with metallic protection and diffuse luminance modulation, bringing in-game weapon previews closer to official Source engine rendering.

---

### Build Binaries Included
- `bin/lan/steam_api64.dll` (x64 LAN Overlay proxy)
- `bin/lan/x86/steam_api.dll` (x86 LAN Overlay proxy)
- `bin/lan/EOSSDK-Win64-Shipping.dll` / `RedboneEOS.dll` (EOS v3 emulator)
- `bin/lan/winhttp.dll` / `winmm.dll` (Proxy modules)
- `refix_mods/refix.csgo.lan_overhaul.rfmod` (CS:GO LAN Overhaul Mod package)
