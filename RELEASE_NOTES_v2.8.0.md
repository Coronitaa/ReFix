# Release v2.8.0

## Summary of Changes Since v2.7.0

ReFix v2.8.0 introduces an overhaul of the skin and glove preview rendering engine for the CS:GO mod, anti-slop interface design certification across all overlay caution dialogs, atomic cluster-based text wrapping with zero orphaned punctuation, unified 540px geometric rhythm, tactile decremental hold-to-confirm mechanics, and an OS-default reactive Game Language selector.

---

### 1. Anti-Slop Interface Craft & Caution Dialog Architecture
- **Anti-Slop Integration & Skills:**
  - Integrated `anti-slop` design standards (`antislop`, `antislop-code`, `antislop-human`, `antislop-ui`, `antislop-copywriting`, `antislop-layoutmobile`) and authored specialized design system skills (`interface-design-system`, `overlay-modal-craft`).
  - Formed a dedicated multi-agent design audit team (Interaction Reviewer, Anti-Slop Visual Auditor, Design System Architect) that iteratively reviewed rendered UI frames to achieve **100% Unanimous Approval & Sign-Off**.
- **Atomic `WordCluster` Typographic Layout:**
  - Re-architected `DrawFormattedNotice` using space-delimited `Cluster` tokenization where trailing punctuation (such as `?`, `.`, `:`) is clustered directly with preceding bold markdown spans (`**...**`).
  - Completely eliminated orphaned punctuation on isolated wrapped lines across all resolutions and languages (English and Spanish).
  - Eliminated trailing uncompensated newline cursor advances, achieving mathematical 14px vertical card symmetry (`Dummy(14)` top/bottom) and 16px horizontal breathing room (`cardInnerW = availW - 32px`).
- **Unified Geometric Rhythm & Modal Stability:**
  - Unified both Step 1 (`##overlay_warn1`) and Step 2 (`##overlay_warn2`) modals to an exact `540px` width (`ui::S(540)`) and `16px` popup rounding (`ui::S(16)`), matching the main Settings modal and completely eliminating horizontal jumps when stepping between dialogs.
  - Symmetrical `Dummy(8)` divider line margins and standard `16px` structural gaps before action buttons.
  - Standardized all button heights across both modals to `40px` (`ui::S(40)` on the 8px grid baseline).
- **Tactile Decremental Charging Beam & Accessibility:**
  - Step 2 confirmation requires a 5-second decremental hold (`p = 1.0 - (1.0 - (t / 5.0))^2.2`) with rapid discharge on release and automatic game restart upon confirmation.
  - Added a 2px specular charging beam line (`IM_COL32(255, 220, 220, 220)`) rendered along the advancing leading edge of the hold progress bar, providing distinct mechanical feedback without blurry glow slop.
  - Full keyboard and gamepad focus ring (`ui::Fade(ui::color::Danger, 0.7f)`) complying with WCAG 2.4.7 focus visibility.

---

### 2. CS:GO Skin & Glove Preview Overhaul (Photorealistic 3D Fidelity)
- **Glove Preview Rendering Overhaul:**
  - Resolved the flat 2D camouflage cutout defect where glove finishes (such as *¡Bum!*, *Menta fresca*, *Polígono*, *Convoy*, *Tartán imperial*) were displayed as flat, unshaded 2D pattern textures stretched over the silhouette.
  - The renderer now parses multi-palette colors (`$palettecolor1`..`$palettecolor8`) directly from `.vmt` definitions and applies 3D diffuse lighting modulation over high-resolution base glove meshes (`ct_gloves.png` / `t_gloves.png`):
    - **$palettecolor1:** Primary body leather with realistic creases and shadows.
    - **$palettecolor2 / $palettecolor3:** Curved hard-knuckle highlights, finger guards, and trim contours (`lum > 0.40f` luminance gating).
    - Preserves genuine 3D lighting, creases, dorsal mesh textures, and specular rolloff matching in-game models.
- **Weapon Composite Lighting & Hardware Preservation:**
  - Tuned the luminance cutoff threshold to isolate true black recesses (trigger shoe, inner scope aperture, deep screws) without turning the surrounding hardware dark.
  - Dark finishes (such as *AUG Hot Rod*, *AUG Azul marino anodizado*) now render with metallic reflections instead of flat black silhouettes.
  - Weapon skins (such as *AWP Dragon Lore*, *Hiperbestia*, *Medusa*) composite smoothly with 3D weapon form and volume lighting.
- **Dynamic `.iic` Cache Invalidation:**
  - Added periodic negative-cache eviction in `Exists()` every 2 seconds, allowing on-the-fly engine-rendered Scaleform icons (`resource/flash/econ/weapons/cached/10-ffffffff-<val>.iic`) to be discovered and loaded instantly while browsing the market.
  - Doubled asynchronous load throughput (`kLoadsPerFrame = 8`) for smoother grid scrolling.

---

### 3. Game Language Dropdown (OS Default & Runtime Reactive)
- **OS-Default Language Detection:**
  - Added system language auto-detection mapping Windows `GetUserDefaultUILanguage()` to official Steam language identifiers (`english`, `spanish`, `latam`, `french`, `german`, `italian`, `russian`, `japanese`, `koreana`, `schinese`, `tchinese`, `polish`, `turkish`, etc.).
- **Reactive In-Game Synchronization:**
  - Exposed `UnrealSteamEmu::GetGameLanguage()` and `UnrealSteamEmu::SetGameLanguage()`.
  - Added a reactive **Game Language** dropdown in the overlay settings under the **Language / Idioma** tab.
  - Automatically detects and updates if the game changes its language at runtime, and persists changes to both `[Unreal.Steam]` and `[Steam]` in `ReFix.ini`.

---

### 4. Multi-Game Deployment & Verification
- Synchronized and verified rebuilt binaries across all installed games in `C:\Users\andre\Games`:
  - `Counter-Strike_Global Offensive`: `bin\steam_api.dll` (x86 LAN edition), `refix.csgo.lan_overhaul.rfmod`.
  - `Among Us 3D`: `steam_api64.dll`, `EOSSDK-Win64-Shipping.dll`.
  - `Barotrauma`: `steam_api64.dll`, `EOSSDK-Win64-Shipping.dll`.
  - `CRACK`: `steam_api64.dll`.
  - `Deep Rock Galactic: Rogue Core`: `steam_api64.dll` (Engine & Win64), `EOSSDK-Win64-Shipping.dll`, `winmm.dll`.
  - `Dressmaker`: `steam_api64.dll`.
  - `Machine Party`: `steam_api64.dll`, `winmm.dll`.
