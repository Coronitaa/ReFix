---
name: interface-design-system
description: "Foundational interface design skill: spatial rhythm, typographic hierarchy, contrast geometry, component semantics, and purposeful aesthetic integration. Use when designing, evaluating, or refining any user interface to ensure it meets professional software design standards and avoids generic AI aesthetics."
allowed-tools: Read, Write, Edit, Glob, Grep
---

# Interface Design System & Craft

> Essential principles for crafting intentional, coherent, and visually mature digital interfaces.

## 1. Spatial Rhythm & Spacing Scales
- **Base Grid**: Anchor layouts to an explicit 4px / 8px scale. Never use arbitrary padding like 13px, 17px, or 23px.
- **Hierarchical Density**: Spacing communicates relationship:
  - Micro (4px - 8px): Related elements within a component (icon to label, label to badge).
  - Component (12px - 16px): Padding inside cards, inputs, and list rows.
  - Structural (20px - 28px): Separation between distinct functional groups or sections.
- **Symmetry & Breathing Room**: Content cards should balance internal padding with external margins. Top and bottom internal padding must counterbalance button action heights.

## 2. Palette & Contrast Architecture
- **Restraint Over Saturation**:
  - Establish a quiet neutral base (e.g. Slate/Obsidian `#0F1117`, `#161922`, `#1F2430`).
  - Reserve accent color for primary functional affordances.
  - Danger/Destructive color must signify genuine permanence or irreversible consequence—never apply it as decorative background wallpaper.
- **Surface Elevation Layers**:
  - `Base`: Background viewport (`#0B0D13`).
  - `Panel`: Overlay windows, sidebars (`#141721`).
  - `Card / Row`: Elevated interactive content containers (`#1C202C`).
  - `Stroke`: Subtle boundary separation with 15% - 25% opacity, avoiding harsh 100% solid borders.

## 3. Typographic Hierarchy
- **Single Scale, Clear Roles**:
  - Heading (18px - 22px, Bold/SemiBold): Section or dialog anchor.
  - Subtitle / Section Tag (13px - 14px, Medium/Muted): Structural breadcrumbs.
  - Body (13px - 14px, Regular): Readable descriptive copy with comfortable line-height (1.4 - 1.5).
  - Caption / Microcopy (11px - 12px, Regular): Secondary indicators, timestamps, hotkeys.
- **Zero Orphaned Elements**: Ensure trailing punctuation and short articles wrap gracefully with their preceding words.

## 4. Affordance & Micro-Interactions
- **State Clarity**: Every interactive element must exhibit distinct default, hovered, active, focused, and disabled states.
- **Progressive Commitment**: Critical actions (game restarts, config wipes, provider replacements) benefit from intent-proving interactions (like decremental click-and-hold) rather than instant clicks.
