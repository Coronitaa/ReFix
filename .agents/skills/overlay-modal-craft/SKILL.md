---
name: overlay-modal-craft
description: "Specialized skill for in-game overlay dialogs, confirmation modals, and safety confirmation flows. Focuses on dark gaming aesthetic alignment, warning visual hierarchy, click-and-hold tactile feedback, and seamless integration with the host game and overlay."
allowed-tools: Read, Write, Edit, Glob, Grep
---

# In-Game Overlay Modal Craft

> Guidelines for high-impact, ergonomically sound confirmation and caution modals in gaming overlays.

## 1. Aesthetic Integration with Overlay Theme
- In-game overlays exist within an active gaming session. Modals must not feel like disconnected OS alert boxes.
- Colors must inherit from the overlay's established theme tokens:
  - `PanelRaised` (`#181B26`): Popup background.
  - `Row` (`#222736`): Internal notice card background.
  - `Border` (`#2D3446`): Dividers and card frames.
  - `Danger` (`#FF5C5C`): Used specifically for risk warnings, header icons, and hold progress.
  - `Muted` (`#8A94A6`): Step indicators, non-critical sub-text.

## 2. Notice Card Framing
- Do not let warning text float in an unbounded container.
- Use a distinct framed card with rounded corners (`8px - 10px`) and subtle inner padding (`12px - 16px`).
- Frame border should use a low-opacity accent or danger stroke (`ui::Fade(ui::color::Danger, 0.45f)`).
- Text formatting: Highlight key configuration changes, file names (`ReFix.ini`), and irreversible consequences using a warm, high-contrast star color (`#F5A623`) or bold emphasis.

## 3. Two-Step Safety Pattern
- **Step 1 (Clarification & Intent)**: Explain the change in plain, concise terms. Present clear "Continue" and "Cancel" buttons.
- **Step 2 (Critical Commitment & Hold)**: Specify the exact ini changes, notify about automatic restart, and require a deliberate Click-and-Hold action (3s to 5s) to guarantee no accidental triggers.

## 4. Click-and-Hold Button Mechanics
- **Asymmetric Charging**:
  - Smooth decremental curve ($1 - (1 - u)^{2.2}$) for satisfying initial response while requiring full duration commitment.
  - Fast discharge (6x - 8x speed) upon early release.
- **Visual Feedback**:
  - Clear progress fill bar clipped to rounded button corners.
  - High-contrast text label showing remaining seconds: `Hold to restart (5s)` -> `Holding to restart... (2.4s)`.
  - Secondary button: A dedicated "Cancel" button always present on the right, also bound to the Escape key.
