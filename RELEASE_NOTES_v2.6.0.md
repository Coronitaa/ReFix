# ReFix v2.6.0

## Summary of Changes Since v2.5.5

ReFix v2.6.0 brings substantial improvements across LAN networking, overlay configuration and design, CS:GO mod visuals, and deployment tools.

---

### 1. Networking & LAN Invites
- **LAN Connection Reliability:** Fixed guest connection failures and eliminated network latency/jitter across local matches.
- **Cross-Session LAN Invites (PR #37 by @Syntist):** Seeded process invite IDs to prevent duplicate ID collisions on sender restarts, added 30-second TTL invite deduplication with flood protection, and matched acknowledgments directly to the recipient.

### 2. Overlay Settings & Display Modes
- **Display Mode Controls:** Added Fullscreen, Windowed (with selectable resolution), and Fullscreen Windowed (default) options.
- **In-Game Overlay Behavior:** Added customizable behavior when opening the overlay (keep game active, mute audio, or pause game).
- **Shortcut Configuration:** Added compact dropdown to customize the overlay shortcut (Shift+Tab, Ctrl+Tab, Shift+F1, F11, etc.).
- **Game Language Selector:** Added reactive Game Language dropdown with automatic OS system language detection and runtime synchronization.

### 3. Caution Modals & Anti-Slop Interface Craft
- **Redesigned Caution Dialogs:** Cleaned up Steam overlay toggle and overlay disable modals with a cohesive reddish aesthetic.
- **Atomic Text Layout:** Formatted text wraps cleanly within container bounds without orphaned punctuation.
- **5-Second Decremental Hold-to-Confirm:** Added tactile decremental hold button ($p = 1 - (1 - u)^{2.2}$) with a 2px specular charging beam, rapid release decay, and automatic restart notification upon confirmation.
- **Accessibility:** Aligned action buttons horizontally with full keyboard and gamepad navigation support.

### 4. CS:GO Mod Enhancements
- **Isolated Mod Architecture:** Decoupled game-specific schemas, market UI, and hooks into `refix.csgo.lan_overhaul.rfmod`.
- **Photorealistic Skin & Glove Previews:** Replaced flat 2D cutouts with multi-palette 3D diffuse lighting, metallic specular rolloff, and accurate knuckle/trim contours.
- **Dynamic Cache Discovery:** Added automatic `.iic` icon cache discovery and doubled async loading throughput.

### 5. Cloud Sync & Deployment Tools
- **Cloud Profile Import:** Added cloud profile discovery modal with multi-game presence indicators and batch selection grid.
- **AutoDeploy & Uninstall Tools:** Updated Windows and Linux deployment scripts (`AutoDeploy.bat`, `AutoDeploy.sh`, `Uninstall_ReFix.bat`) with full overlay configuration persistence and clean uninstallation.

---

### Contributor Credit
Special thanks to **@Syntist** for diagnosing and contributing the cross-session LAN invite lifecycle fix ([PR #37](https://github.com/Coronitaa/ReFix/pull/37)).
