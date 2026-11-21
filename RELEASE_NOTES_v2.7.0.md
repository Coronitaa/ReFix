# Release v2.7.0

## Summary of Changes Since v2.6.0

ReFix v2.7.0 brings display and resolution mode customization, a complete aesthetic overhaul of the overlay safety caution modals with decremental click-and-hold confirmation, horizontal alignment of overlay action triggers, accessibility optimizations verified via technical UX audit, and synchronized binary deployment across all supported games.

---

### 1. Launch Screen Mode & Dynamic Resolution Selection
- **New General Settings Section:** Added a dedicated **"Launch Screen Mode"** ("Modo de pantalla al ejecutar") section in the General overlay settings with three distinct display modes:
  - **Fullscreen Windowed (Default) / Ventana sin bordes (Predet.):** Runs the game in a borderless window matching your display resolution.
  - **Fullscreen / Pantalla completa:** Runs the game in exclusive fullscreen mode.
  - **Windowed / Modo ventana:** Runs the game in a bordered window and dynamically exposes the **Window Resolution** ("Resolución de ventana") picker.
- **Resolution Selector:** Supports standard widescreen aspect ratios and resolutions:
  - `1280 x 720 (16:9 HD)`
  - `1366 x 768 (16:9)`
  - `1600 x 900 (16:9 HD+)`
  - `1920 x 1080 (16:9 Full HD)`
  - `2560 x 1440 (16:9 2K QHD)`
  - `3840 x 2160 (16:9 4K UHD)`
- **Multi-Engine Parameter Injection:**
  - Automatically translates selected display mode and dimensions into Source engine command-line launch parameters (`-windowed -noborder`, `-fullscreen`, `-windowed -w <W> -h <H>`) on instance initialization.
  - Injects target parameters upon in-game restart (`ApplyDisplayModeToCommandLine`) for all game engines.

### 2. Horizontal Action Button Layout
- **Side-by-Side Dual Action Controls:** The overlay mode triggers in General settings are now arranged horizontally on the same baseline:
  - **Replace with Steam Overlay ("Reemplazar por Overlay de Steam"):** Styled with ReFix's emerald accent theme (`ui::color::Accent`).
  - **Disable overlay completely ("Desactivar overlay completamente"):** Styled with danger coral red (`ui::color::Danger`) to visually highlight the risk before clicking.
- **Responsive Width:** Split evenly across the content region width (`availW`) with standardized 12px spacing, perfectly matching upper combo box margins.

### 3. Caution Modals Redesign & Decremental Hold-to-Confirm
- **Top-Level Modal Decoupling:** Separated caution modals from inner settings popup nesting (`DrawOverlayCautionModals`) to eliminate ImGui modal occlusion bugs, z-index fighting, and clipping issues.
- **Card Framing & Token Highlighting:**
  - Formatted warning descriptions inside an elevated card container (`dl->ChannelsSplit(2)`) with a rounded 10px frame and subtle color-coded border stroke.
  - Added inline markdown keyword highlighter (`DrawFormattedNotice`) emphasizing config keys (`Enabled=false`, `BlockSteamOverlay=false/true`, `ReFix.ini`), permanence warnings, and restart notices in high-contrast warm amber (`ui::color::Star`).
  - Eliminated excess bottom margins, producing a compact, well-proportioned dialog footprint.
- **5-Second Decremental Click-and-Hold Button:**
  - Restored and polished the confirmation button in Step 2 modal (`##overlay_warn2`).
  - Implemented an ease-out decremental charging curve ($p(u) = 1.0 - (1.0 - u)^{2.2}$) where $u = t / 5.0$, delivering instantaneous tactile feedback ($78\%$ charged at 2.5s) while decelerating towards 5s to ensure intentional commitment.
  - Implemented rapid asymmetric discharge ($8\times$ speed) when released prematurely to prevent progress pumping.
  - Real-time countdown label (`Holding to restart... (X.Xs)` / `Manteniendo para reiniciar... (X.Xs)`) with a text drop shadow for maximum contrast against animated fill bars.
  - Fully supports keyboard `Escape` to immediately abort and return to settings.

### 4. Technical UX & Accessibility Audit (Score: 9.8 / 10)
- Audited across 13 rendered UI frames in English and Spanish by an autonomous technical research subagent.
- Verified WCAG 2.1 contrast compliance:
  - Text-on-Panel: 13.66 : 1 (Passes AAA standard).
  - Star-on-Card: 9.66 : 1 (Passes AAA standard).
  - Button Text-on-Fill: 8.88 : 1 (Passes AAA standard).
- Refined Spanish string economics to ensure zero truncation across dropdown combos.

### 5. Full Bilingual Parity (EN / ES)
- Complete linguistic parity across all settings options, tooltips, dialogs, step indicators, and hold animations in English and Spanish.

---

### Included Binaries & Assets
- `bin/lan/steam_api64.dll` & `bin/lan/x86/steam_api.dll`
- `bin/lan/EOSSDK-Win64-Shipping.dll` & `bin/lan/RedboneEOS.dll`
- `bin/lan/winhttp.dll` & `bin/lan/winmm.dll`
- `bin/refixmod.exe` & `bin/lan/refix_steamhost64.exe`
- `refix_mods/refix.csgo.lan_overhaul.rfmod` & `refix_mods/catalog.tsv`
- Deployed directly to all target game installations in `C:\Users\andre\Games`.
