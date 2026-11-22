# Release v2.8.0

## Summary of Changes Since v2.7.0

ReFix v2.8.0 introduces an overhaul of the skin and glove preview rendering engine for the CS:GO mod, a unified reddish danger theme across all overlay caution modals, bounded word-level container text wrapping, and an OS-default reactive Game Language selector.

---

### 1. Unified Reddish Theme & Bounded Text in Caution Modals
- **Unified Reddish Visual Identity:** Both Step 1 (`##overlay_warn1`) and Step 2 (`##overlay_warn2`) caution modals now share the reddish warning theme across all interaction flows (replacing with Steam Overlay or disabling the overlay completely):
  - Warning icon: `ui::icon::Warning` in `ui::color::Danger` (`#FF5C5C` Coral Red).
  - Modal title: Formatted in `ui::color::Danger`.
  - Notice card border: Framed with a semi-transparent danger stroke (`ui::Fade(ui::color::Danger, 0.45f)`).
  - Hold-to-confirm button: Base, hover border, and progressive fill styled in danger red shades, ensuring an unambiguous visual signal for critical actions.
- **Word-Level Bounded Text Wrapping:**
  - Rewrote `DrawFormattedNotice` with word-level boundary wrapping against the exact inner width of the notice card (`availW - 24px`), strictly preventing any text overflow or container clipping.
  - Implemented tokenized word tracking (`Word::spaceAfter`) that preserves spaces when transitioning between plain text and bold markdown spans (`**...**`), eliminating concatenated words in English and Spanish.

---

### 2. CS:GO Skin & Glove Preview Overhaul (Photorealistic 3D Fidelity)
- **Glove Preview Rendering Overhaul:**
  - Resolved the flat 2D camouflage cutout defect (`media_1795308722029_e9056e30.jpg`) where glove finishes (such as *¡Bum!*, *Menta fresca*, *Polígono*, *Convoy*, *Tartán imperial*) were displayed as flat, unshaded 2D pattern textures stretched over the silhouette.
  - CS:GO glove finishes in the Source engine are `customcharacter` materials defined by multi-tone palette colors (`$palettecolor1`..`$palettecolor8`).
  - The renderer now parses multi-palette colors directly from `.vmt` definitions and applies 3D diffuse lighting modulation over high-resolution base glove meshes (`ct_gloves.png` / `t_gloves.png`):
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

### 4. Automated UX, Accessibility & Texture Audit
- **Subagent Audit:** Performed automated design and accessibility auditing across 13 test frames and preview renders:
  - Verified text containment within container cards (zero overflow).
  - Verified click-and-hold 5-second decremental charge animation visibility and cancel abort behavior.
  - Confirmed WCAG AA contrast compliance across all text and button elements.
  - Confirmed elimination of glove camouflage noise and verified texture fidelity against in-game models.

---

### 5. Multi-Game Deployment & Verification
- Synchronized and verified binaries across all installed games in `C:\Users\andre\Games`:
  - `Counter-Strike_Global Offensive`: `bin\steam_api.dll` (x86 LAN edition), `refix.csgo.lan_overhaul.rfmod`.
  - `Among Us 3D`: `steam_api64.dll`.
  - `Barotrauma`: `steam_api64.dll`.
  - `CRACK`: `steam_api64.dll`.
  - `Deep Rock Galactic: Rogue Core`: `steam_api64.dll` (Engine & Win64).
  - `Dressmaker`: `steam_api64.dll`.
  - `Machine Party`: `steam_api64.dll`.
